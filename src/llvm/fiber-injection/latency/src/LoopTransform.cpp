#include "../include/LoopTransform.hpp"

LoopTransform::LoopTransform(Loop *L, Function *F, LoopInfo *LI, DominatorTree *DT,
                             ScalarEvolution *SE, AssumptionCache *AC, 
                             OptimizationRemarkEmitter *ORE,  uint64_t Gran)
{
    // Will assume wrapper pass info is valid

    if (L == nullptr || F == nullptr)
        abort(); // Serious

    if (L->getHeader() == nullptr)
        abort(); // Serious

    // Set state
    this->L = L;
    this->F = F;
    this->Granularity = Gran;

    // Calculate loop level DFA
    this->LoopLDFA = new LatencyDFA(L, EXPECTED, MEDCON);
    this->LoopLDFA->ComputeDFA();
    this->LoopLDFA->PrintBBLatencies();
    this->LoopLDFA->PrintInstLatencies();

    // Set wrapper pass state
    this->LI = LI;
    this->DT = DT;
    this->SE = SE;
    this->AC = AC;
    this->ORE = ORE;

    this->CallbackLocations = set<Instruction *>();

    // Try and transform loops into lcssa and loop-simplify forms
    this->CorrectForm = ((formLCSSARecursively(*L, *DT, LI, SE)) && 
                         (simplifyLoop(L, DT, LI, SE, AC, nullptr, true)));

    // Handle and transform subloops first before transforming
    // the current loop
    _transformSubLoops();
}


void LoopTransform::_transformSubLoops()
{
    vector<Loop *> SubLoops = L->getSubLoopsVector();
    for (auto SL : SubLoops)
    {
        auto SLT = new LoopTransform(SL, F, LI, DT, SE, AC, ORE, GRAN);
        SLT->Transform();

        for (auto I : *(SLT->GetCallbackLocations()))
            CallbackLocations.insert(I);
    }

    return;
}

void LoopTransform::Transform()
{
    // NOTE --- Transform invalidates LoopLDFA --- it does NOT
    // currently get updated 

    uint64_t LLS = (LoopLDFA->GetLoopLatencySize()), 
             MarginOffset = 0,
             ExtensionCount = _calculateLoopExtensionStats(LLS, &MarginOffset);

    switch(Extend)
    {
    case TransformOption::EXTEND:
    {
        ExtendLoop(ExtensionCount);
        _collectUnrolledCallbackLocations();

#if LOOP_GUARD
        _designateTopGuardViaPredecessors();
        _designateBottomGuardViaExits();
#endif

        return;
    }
    case TransformOption::BRANCH:
    {
        Instruction *OffsetInst = L->getHeader()->getFirstNonPHI();

#if MARGIN_OFFSET
        // Determine instruction with accumulated latency 
        // greater than MarginOffset

        if (MarginOffset)
            OffsetInst = _findOffsetInst(MarginOffset);
#endif

        BuildBiasedBranch(OffsetInst, ExtensionCount);

#if LOOP_GUARD
        _designateBottomGuardViaExits();
#endif
        return;
    }
    case TransformOption::MANUAL:
    {
        auto ManualCallbackLocations = LoopLDFA->BuildIntervalsFromZero();
        for (auto CL : *ManualCallbackLocations)
        {
            Utils::SetCallbackMetadata(CL, MANUAL_MD);
            CallbackLocations.insert(CL);
        }

#if LOOP_GUARD
        _designateTopGuardViaPredecessors();
        _designateBottomGuardViaExits();
#endif

        return;
    }
    default:
        abort();
    }
}

// HACK
void LoopTransform::_collectUnrolledCallbackLocations()
{
    set<Instruction *> CBMDInstructions;

    // NOTE --- set union probably more proper, but also probably
    // much more expensive --- just insert anyway
    if (Utils::HasCallbackMetadata(L, CBMDInstructions))
    {
        for (auto I : CBMDInstructions)
            CallbackLocations.insert(I);
    }

    return;
}

Instruction *LoopTransform::_buildCallbackBlock(CmpInst *CI, Instruction *InsertionPoint, const string MD)
{
    Instruction *NewBlockTerminator = SplitBlockAndInsertIfThen(CI, InsertionPoint, false, 
                                                                nullptr, DT, LI, nullptr);

    BasicBlock *NewBlock = NewBlockTerminator->getParent();

    if (NewBlockTerminator == nullptr)
        return nullptr;

    // Get unique predecessor and successor for new block
    BasicBlock *NewSingleSucc = NewBlock->getUniqueSuccessor();
    BasicBlock *NewSinglePred = NewBlock->getUniquePredecessor();
    if (NewSinglePred == nullptr || NewSingleSucc == nullptr)
        return nullptr;

    Utils::SetCallbackMetadata(NewBlockTerminator, MD);
    CallbackLocations.insert(NewBlockTerminator);

    return NewBlockTerminator;
}

/*
 * _designateTopGuardViaPredecessors
 * 
 * A guard is injected at the start of every outermost loop (depth=1) 
 * in order to handle a potentially large miss in executions of 
 * the callback function --- it's possible that we're just a tiny bit
 * early right before entering the loop --- however, since loop analysis
 * resets at accumulated latency=0, we could generate a large miss (up 
 * to 2x). We want to prevent this by injecting a callback at the start 
 * of the loop 
 * 
 * We do NOT inject in nested loops --- the tradeoff in preventing a 
 * large miss is offset by the fact that each nested loop will be 
 * executing the callback if there is a top guard in place in every
 * iteration --- which does not scale as the granularity increases.
 * In other words, the overhead of entering early increases with the
 * granularity because each nested loop has an invocation to the 
 * callback every iteration no matter what
 * 
 * Additionally, nested loops tend to be smaller, and an expansion
 * factor is applied when transforming nested loops
 * 
 * Heuristic:
 * 
 * Insert top guard --- we want to execute the callback right before the
 * terminator of each predecessor ONLY if its terminator is an unconditional
 * branch into the loop body. 
 * 
 * HIGH LEVEL:
 * 
 * %1:                              %1:
 * ...                              ...
 * br (cond) %.loop                 tail call void callback() // Callback location
 *                                  br (cond) %.loop 
 * %.loop                               
 * ...                              %.loop
 * ...                              ...
 * br ... %.loop ...                ...
 *                                  br ... %.loop ...  
 * ...
 *                                  ...
 * %.other
 * ...                              %.other
 * ...                              ...
 *                                  ...
 * 
 * 
 * If it's a conditional branch --- we must inject a new basic block containing 
 * the invocation to the callback because injecting the callback before the 
 * conditional branch in the predecessor block itself may execute prematurely 
 * if the execution jumps to a non-loop block
 * 
 * HIGH LEVEL:
 * 
 * %1:                              %1:
 * ...                              ...
 * br (cond) %.loop %.other         br (cond) %.topguard %.other
 * 
 * %.loop                           %.topguard:
 * ...                              tail call void callback() // Callback location
 * ...                              br %.loop
 * br ... %.loop ...
 *                                  %.loop
 * ...                              ...
 *                                  ...
 * %.other                          br ... %.loop ...
 * ...                              
 * ...                              ...
 * 
 *                                  %.other
 *                                  ...
 *                                  ...
 * 
 * TODO --- develop a ratio between LLS for nested loops vs LLS for
 * outermost loop --- if the ratio is high, inject the top guard
 * in the innermost loop, else inject in the outermost loops
 * 
 * TODO --- scale for depth > 2
 */ 

void LoopTransform::_designateTopGuardViaPredecessors()
{
    // Handle at outermost loop (for now)
    if (L->getLoopDepth() != 1)
        return;

    // Acquire the header to the loop
    BasicBlock *Header = L->getHeader();

    // Find all predecessors of the header, determine if their terminators
    // match our criteria
    for (auto PredBB : predecessors(Header))
    {
        // Factor out backedges, etc.
        if ((LoopLDFA->IsBackEdge(PredBB, Header))
            || L->contains(PredBB))
            continue;

        // Get terminator, sanity check
        Instruction *PredTerminator = PredBB->getTerminator();
        if (PredTerminator == nullptr)
            continue;

        // Determine if the terminator is an unconditional branch
        BranchInst *BrTerminator = dyn_cast<BranchInst>(PredTerminator);
        if (BrTerminator == nullptr)
            continue;
        
        if (BrTerminator->isConditional())
        {
            CmpInst *Cond = dyn_cast<CmpInst>(BrTerminator->getCondition());
            if (Cond == nullptr)
                continue;
            
            Instruction *InsertionPoint = &(Header->front());
            Instruction *NewBlockTerminator = _buildCallbackBlock(Cond, InsertionPoint, TOP_MD);
            CallbackLocations.insert(NewBlockTerminator);
        }
        else
        {
            // Add this point to the CallbackLocations list
            Utils::SetCallbackMetadata(BrTerminator, TOP_MD);
            CallbackLocations.insert(BrTerminator);
        }
    }

    return;
}

/*
 * _designateBottomGuardViaExits
 * 
 * A guard is injected at every exit block for a loop --- at any depth
 * to handle the case when we exit a loop, but we have not iterated
 * to the point where we're ready to execute the callback yet. However,
 * since interval calculations outside the loop start at accumulated 
 * latency=0, there may be a large miss (up to 2x). We want to prevent
 * this from occurring via this guard
 * 
 * HIGH LEVEL:
 * Build an intermediate block for callback injections on each block
 * exit
 * 
 * %2:                      %2:
 * ...                      ...
 * br %3                    br %3
 * 
 * %3:                      %3:
 * ...                      ...
 * br (cond) %4 %6          br (cond) %4 %.ivcheck2
 * 
 * %4:                      %4:
 * ...                      ...
 * br (IV) %2 %5            br (IV) %2 %.ivcheck1
 * 
 * %5:                      %.ivcheck1: // requires a iteration couter
 * %a = ...                 br (iter geq threshold) %.guard1 %5
 * return %a                
 *                          %.guard1:
 * %6:                      tail call void callback() // Callback location
 * %b = ...                 br %5
 * return %b
 *                          %5:
 *                          %a = ... 
 *                          return %a
 * 
 *                          %.ivcheck2:
 *                          br (iter geq threshold) %.guard2 %6
 *                 
 *                          %.guard2:
 *                          tail call void callback() // Callback location
 *                          br %6
 * 
 *                          %6:
 *                          %b = ... 
 *                          return %b
 * 
 */ 

void LoopTransform::_buildBottomGuard(BasicBlock *Source, BasicBlock *Exit, PHINode *IteratorPHI)
{
    // Get terminator of the source block --- inside loop
    BranchInst *SourceTerminator = dyn_cast<BranchInst>(Source->getTerminator());
    if (SourceTerminator == nullptr)
        return;

    // If the source terminator is unconditional --- it's an isolated chain
    // in the CFG --- mark a callback location before the terminator and return
    if (SourceTerminator->isUnconditional())
    {
        Utils::SetCallbackMetadata(SourceTerminator, BOTTOM_MD);
        CallbackLocations.insert(SourceTerminator);
        return;
    }

    // Now working with a conditional branch terminator --- get the compare
    // instruction from the source block --- for building the callback block
    CmpInst *SourceCond = dyn_cast<CmpInst>(SourceTerminator->getCondition());
    if (SourceCond == nullptr)
        return;
    
    // Build the callback block --- NOTE --- a byproduct of the BasicBlockUtils
    // API is that the unique successor to the callback block will be used to 
    // inject the threshold check for the bottom guard
    Instruction *InsertionPoint = Exit->getFirstNonPHI(),
                *CallbackBlockTerminator = _buildCallbackBlock(SourceCond, InsertionPoint, BOTTOM_MD);
    if (CallbackBlockTerminator == nullptr)
        return;

    BasicBlock *CallbackBlock = CallbackBlockTerminator->getParent(),
               *ExitBlock = CallbackBlock->getUniqueSuccessor(), // Out of the loop, the original
                                                                 // exit block considered 
               *ChecksBlock = CallbackBlock->getUniquePredecessor(); // Use this block to perform
                                                                     // the threshold checks


    // Build the threshold checks --- replace the branch condition of the
    // ChecksBlock with one that calculates if the iteration count is past
    // the threshold for this particular loop

    // Set up IRBuilder
    Instruction *ChecksTerminator = ChecksBlock->getTerminator();
    BranchInst *ChecksBranch = dyn_cast<BranchInst>(ChecksTerminator);
    if (ChecksBranch == nullptr)
        return;

    IRBuilder<> ChecksBuilder{ChecksBranch};
    Type *IntTy = Type::getInt32Ty(F->getContext());

    // Build the compare instruction
    Value *DummyThreshold = ConstantInt::get(IntTy, 1234); 
    Value *ThresholdCheck = ChecksBuilder.CreateICmp(ICmpInst::ICMP_EQ, IteratorPHI, DummyThreshold);
    
    // Change condition of checks branch instruction
    ChecksBranch->setCondition(ThresholdCheck);

    // Set metadata
    Utils::SetCallbackMetadata(ChecksBranch, BOTTOM_MD);

    return;
}

void LoopTransform::_buildIterator(PHINode *&NewPHI, Instruction *&Iterator)
{
    BasicBlock *Header = L->getHeader();
    Instruction *InsertionPoint = Header->getFirstNonPHI();
    if (InsertionPoint == nullptr)
        return;

    // Set up builder 
    IRBuilder<> IteratorBuilder{InsertionPoint};
    Type *IntTy = Type::getInt32Ty(F->getContext());

    // Build the PHINode that handles the iterator value on each
    // iteration of the loop, create the instruction to iterate
    Value *Increment = ConstantInt::get(IntTy, 1);
    PHINode *TopPHI = IteratorBuilder.CreatePHI(IntTy, 0);
    Value *IteratorVal = IteratorBuilder.CreateAdd(TopPHI, Increment);
    
    // Set values
    NewPHI = TopPHI;
    Iterator = dyn_cast<Instruction>(IteratorVal);

    return;
}

void LoopTransform::_setIteratorPHI(PHINode *ThePHI, Value *Init, Value *Iterator)
{
    for (auto PredBB : predecessors(L->getHeader()))
    {
        // Handle backedges
        if (L->contains(PredBB)) // Shortcut --- for header
            ThePHI->addIncoming(Iterator, PredBB);
        else
            ThePHI->addIncoming(Init, PredBB);
    }

    return;
}

void LoopTransform::_designateBottomGuardViaExits()
{    
    if (L->getLoopDepth() != 1)
        return;

    // Get exit blocks
    SmallVector<std::pair<BasicBlock *, BasicBlock *>, 8> ExitEdges;
    L->getExitEdges(ExitEdges);

    for (auto EE : ExitEdges)
    {
        // Edge --- source, exit block
        BasicBlock *Source = EE.first, *Exit = EE.second;

        Instruction *SourceTerminator = Source->getTerminator();
        if ((SourceTerminator == nullptr)
            || (!(isa<BranchInst>(SourceTerminator))))
            continue; // Sanity check

        PHINode *IteratorPHI = nullptr;
        Instruction *Iterator = nullptr;

        _buildIterator(IteratorPHI, Iterator);
        if ((IteratorPHI == nullptr)
            || (Iterator == nullptr))
            continue;

        Type *IntTy = Type::getInt32Ty(F->getContext());
        _setIteratorPHI(IteratorPHI, ConstantInt::get(IntTy, 0), Iterator);
        _buildBottomGuard(Source, Exit, IteratorPHI);
    }

    return; 
}

/*
 * BuildBiasedBranch
 * 
 * In order to transform a loop properly to inject a callback AND account for
 * a large extension count, we cannot unroll properly --- compilation time
 * skyrockets (especially for nested loops), there's a massive code bloat,
 * and likely instruction cache thrashing
 * 
 * Instead, code is injected at the top of the loop body that handles this 
 * with a more nuanced approach --- we build a iteration counter and inject
 * a branch to a new basic block that contains an invocation to the callback
 * function based on the iteration count
 * 
 * The tradeoff is justified --- a counter is a simple add instruction, and 
 * the branch itself should not add too much overhead because it will be
 * taken once in a large number of iterations (by design) --- therefore 
 * more likely to be predicted by the branch predictor 
 * 
 * Build the branch at the top of the loop body
 * 
 * HIGH LEVEL: 
 * 
 * %2:                      %2:
 * ...                      ... // Keep an iteration count via PHINode
 * br %3                    br (iter eq ExtensionCount) %.biased %3
 * 
 * %3:                      %.biased:
 * ...                      tail call void callback() // Callback location
 * br (cond) %4 %6          ... br %3 // The rest of %2
 * 
 * %4:                      %3:
 * ...                      ...
 * br (IV) %2 %5            br (cond) %4 %6
 * 
 * %5:                      %4:
 * %a = ...                 ...
 * return %a                br (IV) %2 %5 
 *                          
 * %6:                      %5:
 * %b = ...                 %a = ...
 * return %b                return %a
 *                          
 *                          %6: 
 *                          %b = ...  
 *                          return %b
 * 
 */ 

void LoopTransform::BuildBiasedBranch(Instruction *InsertionPoint, uint64_t ExtensionCount)
{
    // Nothing to do if ExtensionCount is 0
    if (!ExtensionCount)
        return;

    if (InsertionPoint == nullptr)
        abort(); // Serious

    BasicBlock *EntryBlock = L->getHeader();
    BasicBlock *InsertionPointParent = InsertionPoint->getParent();
    Instruction *PHIInsertionPoint = InsertionPointParent->getFirstNonPHI();

    // Set up primary builder for and constants
    IRBuilder<> TopBuilder{InsertionPoint};
    IRBuilder<> TopPHIBuilder{PHIInsertionPoint};
    Type *IntTy = Type::getInt32Ty(F->getContext());

    Value *ZeroValue = ConstantInt::get(IntTy, 0); 
    Value *PHIInitValue = ZeroValue;

#if LOOP_GUARD
    // If we want to insert a loop guard, then we should set the init value
    // of the top level Phi node to ExtensionCount - 1, so the code subsequently
    // increments and executes the callback before any loop operations execute
    // NOTE --- we want to guard only outermost loops

    // This is much simpler than _designateTopGuardViaPredecessors, given that
    // this method can forge the guard via the PHINodes for the branch

    if (L->getLoopDepth() == 1)
        PHIInitValue = ConstantInt::get(IntTy, ExtensionCount - 1);
#endif
                          
    Value *ResetValue = ConstantInt::get(IntTy, ExtensionCount);
    Value *Increment = ConstantInt::get(IntTy, 1);

    // Generate new instructions for new phi node and new block functionality inside entry block
    PHINode *TopPHI = TopPHIBuilder.CreatePHI(IntTy, 0);
    Value *Iterator = TopBuilder.CreateAdd(TopPHI, Increment);
    Value *CmpInst = TopBuilder.CreateICmp(ICmpInst::ICMP_EQ, Iterator, ResetValue);

    // Generate new block (for loop for injection of nk_time_hook_fire)
    Instruction *NewBlockTerminator = SplitBlockAndInsertIfThen(CmpInst, InsertionPoint, false, nullptr, DT, LI, nullptr);
    BasicBlock *NewBlock = NewBlockTerminator->getParent();
    if (NewBlockTerminator == nullptr)
        return;

    // Get unique predecessor and successor for new block
    BasicBlock *NewSingleSucc = NewBlock->getUniqueSuccessor();
    BasicBlock *NewSinglePred = NewBlock->getUniquePredecessor();
    if (NewSinglePred == nullptr || NewSingleSucc == nullptr)
        return;

    // Generate new builder on insertion point in the successor of the new block and
    // generate second level phi node
    Instruction *SecondaryInsertionPoint = NewSingleSucc->getFirstNonPHI();
    if (SecondaryInsertionPoint == nullptr)
        return;

    IRBuilder<> SecondaryPHIBuilder{SecondaryInsertionPoint};
    PHINode *SecondaryPHI = SecondaryPHIBuilder.CreatePHI(IntTy, 0);

    // Populate selection points for second level phi node
    SecondaryPHI->addIncoming(Iterator, NewSinglePred);
    SecondaryPHI->addIncoming(ZeroValue, NewBlock);

    // Populate selection points for top level phi node
    for (auto PredBB : predecessors(EntryBlock))
    {
        // Handle potential backedges
        if (L->contains(PredBB)) // Shortcut
            TopPHI->addIncoming(SecondaryPHI, PredBB);
        else
            TopPHI->addIncoming(PHIInitValue, PredBB);
    }

    Utils::SetCallbackMetadata(NewBlockTerminator, BIASED_MD);
    CallbackLocations.insert(NewBlockTerminator);

    return;
}

void LoopTransform::ExtendLoop(uint64_t ExtensionCount)
{
    // Nothing to unroll if ExtensionCount is 0
    if (!ExtensionCount) 
        return;

    PrintCurrentLoop();

    // Unroll iterations of the loop based on ExtensionCount
    uint32_t MinTripMultiple = 1;
    UnrollLoopOptions *ULO = new UnrollLoopOptions();
    ULO->Count = ExtensionCount;
    ULO->TripCount = SE->getSmallConstantTripCount(L);
    ULO->Force = true;
    ULO->AllowRuntime = true;
    ULO->AllowExpensiveTripCount = true;
    ULO->PreserveCondBr = true;
    ULO->PreserveOnlyFirst = false;
    ULO->TripMultiple = max(MinTripMultiple, SE->getSmallConstantTripMultiple(L));
    ULO->PeelCount = 0;
    ULO->UnrollRemainder = false;
    ULO->ForgetAllSCEV = false;

    LoopUnrollResult Unrolled = UnrollLoop(L, *ULO, LI, SE, DT, AC, ORE, true);

    if (!((Unrolled == LoopUnrollResult::FullyUnrolled)
        || (Unrolled == LoopUnrollResult::PartiallyUnrolled)))
    {
        LOOP_DEBUG_INFO("NOT UNROLLED, L: ");
        LOOP_OBJ_INFO(L);
        BuildBiasedBranch(L->getHeader()->getFirstNonPHI(), ExtensionCount);
    }

    PrintCurrentLoop();

    // Collect the callback locations and add them to the set --- should be
    // the terminator of the last 
    Instruction *LastInst = (L->getBlocksVector()).back()->getTerminator();
    Utils::SetCallbackMetadata(LastInst, (UNROLL_MD + to_string(ExtensionCount)));
    CallbackLocations.insert(LastInst);

    return;
}

uint64_t LoopTransform::_calculateLoopExtensionStats(uint64_t LLS, uint64_t *MarginOffset)
{
    if (!LLS)
        return 0;

    // Calculate the extension count --- i.e. how much we should
    // unroll the loop or how many iterations should pass before
    // taking the biased branch

    // If LLS is larger than the granularity, we must designate 
    // callback locations manually but NOT actually perform
    // any transformations to the loop --- for injection purposes,
    // a callback MUST be injected no matter the accumulated
    // latency entering the loop body (mathematically speaking) 
    if (LLS > Granularity)
    {
        Extend = TransformOption::MANUAL;
        return 0;
    }

    double GranDouble = (double) Granularity;

    // Find margin of error
    double Exact = GranDouble / LLS;
    uint64_t Rounded = Granularity / LLS;
    double MarginOfError = (Exact - Rounded) * LLS; // Number of cycles

    // If we exceed the maximum margin of errorm, then we must extend
    // the loop in some way until LLS > Granularity
    uint64_t ExtensionCount = 0;
    if (MarginOfError > MaxMargin)
    {
        ExtensionCount = Rounded; // + 1; // Rounded; --- FIX
        *MarginOffset = (uint64_t) (MarginOfError); // Only matters for biased branching
    }
    else 
    {
        Rounded++;
        ExtensionCount = Rounded;
    }

    /*
     * Below is an attempt at weighting nested loops --- nested loops tend to
     * be smaller or a quicker set of operations, possibly more prone to 
     * vectorization, so miscalculating on a nested loop will generate 
     * consistently late or early calls to the callback function (i.e. the 
     * effect will compound through the loop depths at runtime) --- we want
     * to mitigate for that by moving from overestimating the LLS of a nested
     * loop to something closer to the ground truth
     */ 
    if (_canVectorizeLoop())
        ExtensionCount *= (L->getLoopDepth() * ExpansionFactor); // INACCURACY --- FIX

    // Check if we should extend or inject a biased branch --- we 
    // need to set up a biased branch if we are past the max extension
    // count --- Otherwise, if the extension count is low enough 
    // (after incrementing to account for the margin --- i.e. rounding 
    // up), return that extension count (Rounded++) 
    Extend = (ExtensionCount > MaxExtensionCount) ? 
              TransformOption::BRANCH : 
              TransformOption::EXTEND;

    return ExtensionCount;
}

// Deprecated
Instruction *LoopTransform::_findOffsetInst(uint64_t MarginOffset)
{
    Instruction *OffsetInst = nullptr;
    bool FoundOffsetInst = false;

    DEBUG_INFO("\n\n\n\n\nMARGINOFFSET: " + to_string(MarginOffset) + "\n");
    DEBUG_INFO("LLS: " + to_string(LoopLDFA->GetLoopLatencySize()) + "\n");

    // Very crude --- needs optimization
    int32_t ClosestAL = INT_MAX;
    for (auto BB : LoopLDFA->Blocks)
    {
        for (auto &I : *BB)
        {
            OBJ_INFO((&I));

            // Find the instruction with the "closest" accumulated 
            // latency to the specified offset --- HACK --- FIX
            uint64_t CurrAL = LoopLDFA->GetAccumulatedLatency(&I); 
            int32_t Diff = CurrAL - MarginOffset;
            if ((Diff >= 0) && (Diff < ClosestAL))
            {
                // We can skip these instructions --- would cause IR 
                // inconsistencies, and the latencies for these 
                // instructions are 0 --- by default, there should be
                // another instruction with accumulated latency the same
                // as a particular PHINode or BranchInst
                if (isa<PHINode>(&I) || isa<BranchInst>(&I))
                    continue;

                OffsetInst = &I;
                ClosestAL = CurrAL;
                FoundOffsetInst |= true;
            }
        }
    }

    if (!FoundOffsetInst)
        abort(); // Serious


    DEBUG_INFO("OFFSETINST\n");
    OBJ_INFO(OffsetInst);

    return OffsetInst;
}   

void LoopTransform::PrintCurrentLoop()
{
    DEBUG_INFO("Current Loop:\n");
    OBJ_INFO(L);
    for (auto B = L->block_begin(); B != L->block_end(); ++B)
    {
        BasicBlock *CurrBB = *B;
        OBJ_INFO(CurrBB);
    }

    DEBUG_INFO("\n\n\n---\n\n\n");

    return;
}