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

    uint64_t LLC = (LoopLDFA->GetLoopLatencySize()), 
             MarginOffset = 0,
             ExtensionCount = _calculateLoopExtensionStats(LLC, &MarginOffset);

    switch(Extend)
    {
    case TransformOption::EXTEND:
        ExtendLoop(ExtensionCount);
        return;
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
        return;
    }
    case TransformOption::MANUAL:
    {
        auto ManualCallbackLocations = LoopLDFA->BuildIntervalsFromZero();
        for (auto CL : *ManualCallbackLocations)
            CallbackLocations.insert(CL);

        return;
    }
    default:
        abort();
    }
}

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
    // incremenets and executes the callback before any loop operations execute
    // NOTE --- we want to guard only outermost loops

    // This is much simpler than _designateGuardViaPredecessors, given that
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

    CallbackLocations.insert(NewBlockTerminator);
}

void LoopTransform::ExtendLoop(uint64_t ExtensionCount)
{
    // Nothing to unroll if ExtensionCount is 0
    if (!ExtensionCount) 
        return;

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

    DEBUG_INFO("\n\n\nLOOP\n\n\n");
    DEBUG_INFO(F->getName());
    L->print(errs());

    LoopUnrollResult Unrolled = UnrollLoop(L, *ULO, LI, SE, DT, AC, ORE, true);

    if (!((Unrolled == LoopUnrollResult::FullyUnrolled)
        || (Unrolled == LoopUnrollResult::PartiallyUnrolled)))
    {
        LOOP_DEBUG_INFO("NOT UNROLLED, L: ");
        LOOP_OBJ_INFO(L);
        BuildBiasedBranch(L->getHeader()->getFirstNonPHI(), ExtensionCount);
    }

    F->print(errs());

    // Collect the callback locations and add them to the set --- should be
    // the terminator of the last 
    CallbackLocations.insert((L->getBlocksVector()).back()->getTerminator());

#if LOOP_GUARD
    _designateGuardViaPredecessors();
#endif

    return;
}

void LoopTransform::_designateGuardViaPredecessors()
{
    // Insert loop guard --- we want to execute a callback at the terminator
    // instruction of each predecessor --- ONLY if it's an unconditional branch
    // into the loop body --- we will forgo other predecessors that contain
    // conditional branches or other terminators --- NOTE --- we only want to
    // insert a guard for outermost loops

    if (L->getLoopDepth() != 1)
        return;
    
    // Acquire the header to the loop
    BasicBlock *Header = L->getHeader();

    // Find all predecessors of the header, determine if their terminators
    // match our criteria
    for (auto PredBB : predecessors(Header))
    {
        // Get terminator, sanity check
        Instruction *PredTerminator = PredBB->getTerminator();
        if (PredTerminator == nullptr)
            continue;

        // Determine if the terminator is an unconditional branch
        BranchInst *BrTerminator = dyn_cast<BranchInst>(PredTerminator);
        if ((BrTerminator == nullptr)
            || (BrTerminator->isConditional()))
            continue;

        // Add this point to the CallbackLocations list
        CallbackLocations.insert(BrTerminator);
    }

    return;
}

uint64_t LoopTransform::_calculateLoopExtensionStats(uint64_t LLC, uint64_t *MarginOffset)
{
    if (!LLC)
        return 0;

    // Calculate the extension count --- i.e. how much we should
    // unroll the loop or how many iterations should pass before
    // taking the biased branch

    // If LLC is larger than the granularity, we must designate 
    // callback locations manually but NOT actually perform
    // any transformations to the loop --- for injection purposes,
    // a callback MUST be injected no matter the accumulated
    // latency entering the loop body (mathematically speaking) 
    if (LLC > Granularity)
    {
        Extend = TransformOption::MANUAL;
        return 0;
    }

    double GranDouble = (double) Granularity;

    // Find margin of error
    double Exact = GranDouble / LLC;
    uint64_t Rounded = Granularity / LLC;
    double MarginOfError = (Exact - Rounded) * LLC; // Number of cycles

    // If we exceed the maximum margin of errorm, then we must extend
    // the loop in some way until LLC > Granularity
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
     * to mitigate for that by moving from overestimating the LLC of a nested
     * loop to something closer to the ground truth
     */ 
    if (_canVectorizeLoop())
        ExtensionCount *= (L->getLoopDepth() * ExpansionFactor);

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
    DEBUG_INFO("LLC: " + to_string(LoopLDFA->GetLoopLatencySize()) + "\n");

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