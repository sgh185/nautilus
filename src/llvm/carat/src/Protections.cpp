/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, Gaurav Chaudhary <gauravchaudhary2021@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 *          Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "../include/Protections.hpp"

ProtectionsHandler::ProtectionsHandler(Module *M,
                                       std::unordered_map<std::string, int> *FunctionMap)
{
    DEBUG_INFO("--- Protections Constructor ---\n");

    // Set state
    this->M = M;
    this->FunctionMap = FunctionMap;
    this->Panic = M->getFunction("panic");
    if (this->Panic == nullptr) { abort(); }
    
    // Get globals
    this->LowerBound = M->getGlobalVariable(LOWER_BOUND);
    if (this->LowerBound == nullptr) { abort(); }

    this->UpperBound = M->getGlobalVariable(UPPER_BOUND);
    if (this->UpperBound == nullptr) { abort(); }

    this->PanicString = M->getGlobalVariable(PANIC_STRING);
    if (this->PanicString == nullptr) { abort(); }

    // Set up data structures
    this->MemoryInstructions = std::unordered_map<Instruction *, pair<Instruction *, Value *>>();
    this->EscapeBlocks = std::unordered_map<Function *, BasicBlock *>();
    this->BoundsLoadInsts = std::unordered_map<Function *, pair<LoadInst *, LoadInst *> *>();

    // Do analysis
    this->_getAllNecessaryInstructions();
}



/* 
 *
 * --- Loop analysis, transformation, guard selection --- 
 * 
 */

void ProtectionsHandler::_findGuardInsertionPoints(Function *F, 
                                                   DataFlowResult *DFAResult, 
                                                   Instruction *I,
                                                   Instruction *PointerOfMemoryInstruction)
{
    DEBUG_INFO("\n\n\n");

    /*
     * Check if the pointer has already been guarded.
     */
    auto &INSetOfI = DFAResult->IN(I);
    if (INSetOfI.find(PointerOfMemoryInstruction) != INSetOfI.end())
    {
        DEBUG_INFO("YAY: skip a guard for the instruction: ");
        OBJ_INFO(I);

        NumRedundantGuards++;
        
        return;
    }

    /*
     * We have to guard the pointer.
     *
     * Check if we can hoist the guard outside the loop.
     */
    DEBUG_INFO("XAN: we have to guard the memory instruction: ");
    OBJ_INFO(I);

    auto added = false;
    auto nestedLoop = loopInfo.getLoopFor(I->getParent());
    if (nestedLoop == nullptr)
    {
        /*
         * We have to guard just before the memory instruction.
         */
        MemoryInstructions[I] = {I, PointerOfMemoryInstruction};
        NumNonOptimizedGuards++;

        DEBUG_INFO("XAN:   The instruction doesn't belong to a loop so no further optimizations are available\n");
        
        return;
    }

    DEBUG_INFO("XAN:   It belongs to the loop ");
    OBJ_INFO(nestedLoop);
    Instruction *preheaderBBTerminator;

    while (nestedLoop != NULL)
    {
        if (false 
            || nestedLoop->isLoopInvariant(PointerOfMemoryInstruction) 
            || isa<Argument>(PointerOfMemoryInstruction) 
            || !isa<Instruction>(PointerOfMemoryInstruction) 
            || !nestedLoop->contains(cast<Instruction>(PointerOfMemoryInstruction)))
        {
            DEBUG_INFO("YAY:   we found an invariant address to check:")
            OBJ_INFO(I);

            auto preheaderBB = nestedLoop->getLoopPreheader();
            preheaderBBTerminator = preheaderBB->getTerminator();
            nestedLoop = loopInfo.getLoopFor(preheaderBB);
            added = true;
        }
        else { break; }
    }

    if (added)
    {
        /*
         * We can hoist the guard outside the loop.
         */
        NumLoopInvariantGuards++;
        MemoryInstructions[I] = {preheaderBBTerminator, PointerOfMemoryInstruction};
        
        return;
    }

    /*
     * The memory instruction isn't loop invariant.
     *
     * Check if it is based on a bounded scalar evolution.
     */
    DEBUG_INFO("XAN:   It cannot be hoisted. Check if it can be merged\n");

    auto scevPtrComputation = scalarEvo.getSCEV(PointerOfMemoryInstruction);
    DEBUG_INFO("XAN:     SCEV = ");
    OBJ_INFO(scevPtrComputation);

    if (auto AR = dyn_cast<SCEVAddRecExpr>(scevPtrComputation))
    {
        DEBUG_INFO("XAN:       It is SCEVAddRecExpr\n");

        /*
         * The computation of the pointer is based on a bounded scalar evolution.
         * This means, the guard can be hoisted outside the loop where the boundaries used in the check go from the lowest to the highest address.
         */
        SCEV_CARAT_Visitor visitor{};
        auto startAddress = numNowPtr; // FIX ---- Iterate over SCEV to fetch 
                                       // the actual start and end addresses

        // numNowPtr = rsp-8 ideally
        
        /*

        // BRIAN --- CHECK

        auto startAddress = visitor.visit((const SCEV *)AR);
        auto startAddressSCEV = AR->getStart();

        DEBUG_INFO("XAN:         Start address = ");
        OBJ_INFO(startAddressSCEV);

        Value *startAddress = nullptr;
        if (auto startGeneric = dyn_cast<SCEVUnknown>(startAddressSCEV)) {
            startAddress = startGeneric->getValue();
        } else if (auto startConst = dyn_cast<SCEVConstant>(startAddressSCEV)) {
            startAddress = startConst->getValue();
        }
        
        */

        if (startAddress)
        {
            DEBUG_INFO("YAY: we found a scalar evolution-based memory instruction: ");
            OBJ_INFO(I);

            auto nestedLoop = AR->getLoop();
            auto preheaderBB = nestedLoop->getLoopPreheader();
            preheaderBBTerminator = preheaderBB->getTerminator();

            NumScalarEvolutionGuards++;

            MemoryInstructions[I] = {preheaderBBTerminator, startAddress};

            return;
        }
    }

    /*
     * We have to guard just before the memory instruction.
     */

    DEBUG_INFO("NOOO: the guard cannot be hoisted or merged: \n");
    OBJ_INFO(I);

    MemoryInstructions[I] = {I, PointerOfMemoryInstruction};
    NumNonOptimizedGuards++;

    return;
}


/* 
 * 
 * --- Find instructions to instrument --- 
 * 
 */

void ProtectionsHandler::_getAllNecessaryInstructions()
{
    /*
     * Check if there is no stack allocations other than those 
     * in the first basic block of the function.
     */
    auto allocaOutsideFirstBB = false;
    for (auto &B : F)
    {
        for (auto &I : B)
        {
            if (true && isa<AllocaInst>(&I) && (&B != firstBB))
            {
                /*
                 * We found a stack allocation not in the first basic block.
                 */
                DEBUG_INFO("NOOOO: Found an alloca outside the first BB: ");
                OBJ_INFO(I);

                allocaOutsideFirstBB = true;

                break;
            }
        }
    }

    /*
     * Identify where to place the guards.
     */
    std::unordered_map<Function *, bool> functionAlreadyChecked;
    for (auto &B : F)
    {
        for (auto &I : B)
        {
            if (isa<CallInst>(I) || isa<InvokeInst>(I))
            {
                Function *calleeFunction;

                /*
                 * Check if we are invoking an LLVM metadata callee. We don't need to guard these calls.
                 */
                if (auto tmpCallInst = dyn_cast<CallInst>(&I))
                {
                    calleeFunction = tmpCallInst->getCalledFunction();
                }
                else
                {
                    auto tmpInvokeInst = cast<InvokeInst>(&I);
                    calleeFunction = tmpInvokeInst->getCalledFunction();
                }

                if (true 
                    && (calleeFunction != nullptr) 
                    && calleeFunction->isIntrinsic())
                {
                    //errs() << "YAY: no need to guard calls to intrinsic LLVM functions: " << I << "\n" ;
                    continue;
                }

                /*
                 * Check if we have already checked the callee and there is no alloca outside the first basic block. We don't need to guard more than once a function in this case.
                 */
                if (true 
                    && !allocaOutsideFirstBB 
                    && (calleeFunction != nullptr) 
                    && (functionAlreadyChecked[calleeFunction] == true))
                {
                    //errs() << "YAY: no need to guard twice the callee of the instruction " << I << "\n" ;

                    continue;
                }

                /*
                 * Check if we can hoist the guard.
                 */
                if (allocaOutsideFirstBB)
                {
                    /*
                     * We cannot hoist the guard.
                     */
                    MemoryInstructions[&I] = {&I, numNowPtr};
                    //errs() << "NOOO: missed optimization because alloca invocation outside the first BB for : " << I << "\n" ;
                    NumNonOptimizedGuards++;
                    continue;
                }

                /*
                        * We can hoist the guard because the size of the allocation frame is constant.
                        *
                        * We decided to place the guard at the beginning of the function. 
                        * This could be good if we have many calls to this callee.
                        * This could be bad if we have a few calls to this callee and they could not be executed.
                        */
                //errs() << "YAY: we found a call check that can be hoisted: " << I << "\n" ;
                MemoryInstructions[&I] = {firstInst, numNowPtr};
                functionAlreadyChecked[calleeFunction] = true;
                NumCallGuardOptimized++;
                continue;
            }
    #if STORE_GUARD
            if (isa<StoreInst>(I))
            {
                auto storeInst = cast<StoreInst>(&I);
                auto pointerOfStore = storeInst->getPointerOperand();
                findPointToInsertGuard(&I, pointerOfStore);
                continue;
            }
    #endif
    #if LOAD_GUARD
            if (isa<LoadInst>(I))
            {
                auto loadInst = cast<LoadInst>(&I);
                auto pointerOfStore = loadInst->getPointerOperand();
                findPointToInsertGuard(&I, pointerOfStore);
                continue;
            }
    #endif
        }
    }
}


/* 
 *
 * --- Data-flow analysis --- 
 * 
 */

std::function<void (Instruction *, DataFlowResult *)> ProtectionsHandler::_buildComputeGEN()
{ 
    auto dfaGEN = [this->StoreGuard, 
                   this->LoadGuard](Instruction *inst, 
                                    DataFlowResult *res) -> void 
    {
        /*
            * Handle store instructions.
            */
        if (true 
            && !isa<StoreInst>(inst) 
            && !isa<LoadInst>(inst))
            { return; }
            
        Value *ptrGuarded = nullptr;

        if (true 
            && isa<StoreInst>(inst)
            && this->StoreGuard)
        {
            auto storeInst = cast<StoreInst>(inst);
            ptrGuarded = storeInst->getPointerOperand();
        }
        else if (true
                 && this->LoadGuard)
        {
            auto loadInst = cast<LoadInst>(inst);
            ptrGuarded = loadInst->getPointerOperand();
        }

        if (ptrGuarded == nullptr) { return; }

        /*
            * Fetch the GEN[inst] set.
            */
        auto &instGEN = res->GEN(inst);

        /*
            * Add the pointer guarded in the GEN[inst] set.
            */
        instGEN.insert(ptrGuarded);

        return;
    };

    return dfaGEN;
}

std::function<void (Instruction *, DataFlowResult *)> ProtectionsHandler::_buildComputeKILL()
{ 
    auto dfaKILL = [](Instruction *inst, DataFlowResult *res) -> void { return; };
    return dfaKILL;
}

std::set<Value *> *ProtectionsHandler::_buildUniverse(Function *F)
{
    std::set<Value *> *Universe = new std::set<Value *>();

    for (auto &B : F)
    {
        for (auto &I : B)
        {
            Universe->insert(&I);
            for (auto i = 0; i < I.getNumOperands(); i++)
            {
                auto op = I.getOperand(i);
                if (false
                    || (isa<Function>(op))
                    || (isa<BasicBlock>(op)))
                    { continue; }

                Universe->insert(op);
            }
        }
    }

    for (auto &arg : F.args()) { Universe->insert(&arg); }

    return Universe;
}

std::function<void (Instruction *, DataFlowResult *)> ProtectionsHandler::_buildInitIN(std::set<Value *> &Universe, Instruction *First)
{ 
    auto initIN = [Universe, First](Instruction *inst, std::set<Value *> &IN) -> void 
    {
        if (inst == First) { return; }
        IN = Universe;

        return;
    };

    return initIN;
}

std::function<void (Instruction *, DataFlowResult *)> ProtectionsHandler::_buildInitOUT(std::set<Value *> &Universe)
{ 
    auto initOUT = [Universe, First](Instruction *inst, std::set<Value *> &OUT) -> void 
    {
        OUT = Universe;
        return;
    };

    return initOUT;
}

std::function<void (Instruction *, DataFlowResult *)> ProtectionsHandler::_buildComputeIN()
{ 
    auto computeIN = [](Instruction *inst, std::set<Value *> &IN, Instruction *predecessor, DataFlowResult *df) -> void 
    {
        auto &OUTPred = df->OUT(predecessor);

        std::set<Value *> tmpIN{};
        std::set_intersection(
            IN.begin(), 
            IN.end(), 
            OUTPred.begin(), 
            OUTPred.end(), 
            std::inserter(tmpIN, tmpIN.begin())
        );
        IN = tmpIN;

        return;
    };

    return computeIN;
}

std::function<void (Instruction *, DataFlowResult *)> ProtectionsHandler::_buildComputeOUT()
{ 
    auto computeOUT = [](Instruction *inst, std::set<Value *> &OUT, DataFlowResult *df) -> void 
    {
        /*
         * Fetch the IN[inst] set.
         */
        auto &IN = df->IN(inst);

        /*
         * Fetch the GEN[inst] set.
         */
        auto &GEN = df->GEN(inst);

        /*
         * Set the OUT[inst] set.
         */
        OUT = IN;
        OUT.insert(GEN.begin(), GEN.end());

        return;
    };

    return computeOUT;
}


DataFlowResult *ProtectionsHandler::_exeucteDFA(Function *F)
{
    // Set up DataFlowAnalysis
    DataFlowAnalysis DFA{};

    // Get necessary DFA parameters --- build universe
    std::set<Value *> TheUniverse = *(_buildUniverse(F));

    // Get necessary DFA parameters --- get first instruction
    auto FirstBB = &*F.begin();
    auto FirstInst = &*FirstBB->begin();

    // Compute DFA
    auto DFAResult = DFA.applyForward(F, 
                                      _buildComputeGEN(),
                                      _buildComputeKILL(),
                                      _buildInitIN(TheUniverse, FirstInst),
                                      _buildInitOUT(TheUniverse), 
                                      _buildComputeIN(),
                                      _buildComputeOUT());

    return DFAResult;
}





/* --- Function call injection scheme --- */

void ProtectionsHandler::InjectCheckCalls()
{

}



/* --- Complete injection scheme --- DEPRECATED --- */

BasicBlock *ProtectionsHandler::_buildEscapeBlock(Function *F)
{
    /* 

       Escape block structure:

       ...
       ...

       escape: 
        %l = load PanicString
        %p = tail call panic(PanicString)
        %u = unreachable

    */

    // Set up builder
    IRBuilder<> EscapeBuilder = Utils::GetBuilder(F, EscapeBlock);

    // Create escape basic block, save the state
    EscapeBlock = BasicBlock::Create(F->getContext(), "escape", F);
    EscapeBlocks[F] = EscapeBlock;

    // Load the panic string global
    LoadInst *PanicStringLoad = EscapeBuilder.CreateLoad(PanicString);

    // Set up arguments for call to panic
    std::vector<Value *> CallArgs;
    CallArgs.push_back(PanicStringLoad);
    ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

    // Build call to panic
    CallInst *PanicCall = EscapeBuilder.CreateCall(Panic, LLVMCallArgs)
    
    // Add LLVM unreachable
    UnreachableInst *Unreachable = EscapeBuilder.CreateUnreachable();

    return EscapeBlock;
}

pair<LoadInst *, LoadInst *> *ProtectionsHandler::_buildBoundsLoadInsts(Function *F)
{
    /* 

       Bounds loads structure:

       entry: 
        %l = load lower_bound
        %u = load upper_bound

        ...

        %0

    */

    // Get the insertion point
    BasicBlock *EntryBlock = &(F->getEntryBlock());
    Instruction *InsertionPoint = EntryBlock->getFirstNonPHI();
    if (InsertionPoint == nullptr)
        abort(); // Serious

    // Set up the builder
    IRBuilder<> BoundsBuilder = Utils::GetBuilder(F, InsertionPoint);

    // Insert lower bound, then upper bound
    LoadInst *LowerBoundLoad = BoundsBuilder.CreateLoad(LowerBound);
    LoadInst *UpperBoundLoad = BoundsBuilder.CreateLoad(UpperBound);

    // Build pair object, save the state
    pair<LoadInst *, LoadInst *> *NewBoundsPair = new pair<LoadInst *, LoadInst *>();
    NewBoundsPair->first = LowerBoundLoad;
    NewBoundsPair->second = UpperBoundLoad;
    BoundsLoadInsts[F] = NewBoundsPair;
    
    return NewBoundsPair;
}

void ProtectionsHandler::InjectManualChecks()
{
    for (auto const &[MI, InjPair] : MemoryInstructions)
    {
        // Break down the pair object
        Instruction *InjectionLocation = InjPair.first;
        Value *AddressToCheck = InjPair.second;

        // Get function to inject in
        Function *ParentFunction = I->getFunction();

        // Make sure the escape block exists in the parent
        // function --- or else, build it
        BasicBlock *EscapeBlock = (EscapeBlocks.find(ParentFunction) == EscapeBlocks.end()) ?
                                   _buildEscapeBlock(ParentFunction) :
                                   EscapeBlocks[ParentFunction];

        // Make sure the lower/upper bounds load instructions
        // are injected in the entry block --- if not, build them
        pair<LoadInst *, 
             LoadInst *> *BoundsLoads = (BoundsLoadInsts.find(ParentFunction) == BoundsLoadInsts.end()) ?
                                         _buildBoundsLoadInsts(ParentFunction) :
                                         BoundsLoadInsts[ParentFunction];

        // Break down the bounds load instructions
        LoadInst *LowerBoundLoad = BoundsLoads->first;
        LoadInst *UpperBoundLoad = BoundsLoads->second;

        // Optimized injections (implemented) vs. original 
        // CARAT injections
        /*
            %i = InjectionLocation
            
            --- OPTIMIZED ---
            ---
            
            entry-block:
              %l = load lower_bound
              %u = load upper_bound

            ...
            ...

            optimized-injection:
              %0 = icmp addressToCheck %l
              %1 = icmp addressToCheck %u
              %2 = and %0, %1
              %3 = br %2 %original %escape

            original:
              %i = InjectionLocation

            ...
            ...

            escape:
              %lp = load PanicString
              %escape1 = panic ...
              unreachable
            
            ---
            --- OLD ---
            ---

            carat-original-injection:
              %0 = load lower_bound
              %1 = icmp addressToCheck %0
              %2 = br %0 %continue-block %escape

            carat-continue-block:
              %3 = load upper_bound
              %4 = icmp addressToCheck %3
              %5 = br %3 %original %escape

            original:
              %i = InjectionLocation

            ...
            ...
            
            escape:
              %lp = load PanicString
              %escape1 = panic ...
              unreachable

            ---
        */

        /*
            Split the block --- 

            1) Inject icmp + and instructions at insertion point

            2) Split the block --- SplitBlock will patch an unconditional
               into OldBlock --- from OldBlock to NewBlock

            3) Modify the unconditional branch instruction to reflect
               a conditional jump to escape if necessary
        */
    
        // Set up builder
        IRBuilder<> ChecksBuilder = Utils::GetBuilder(F, InjectionLocation);

        // Inject compare for lower bound, then compare for upper bound,
        // then and instruction with both compares as operands
        Value *LowerBoundCompare = ChecksBuilder.CreateICmpULT(AddressToCheck, LowerBoundLoad),
              *UpperBoundCompare = ChecksBuilder.CreateICmpUGT(AddressToCheck, UpperBoundLoad),
              *AndInst = ChecksBuilder.CreateAnd(LowerBoundCompare, UpperBoundCompare);

        // Now split the block
        BasicBlock *OldBlock = InjectionLocation->getParent();
        BasicBlock *NewBlock = SplitBlock(OldBlock, InjectionLocation);
        
        // Reconfigure the OldBlock's unconditional branch to handle
        // conditional jump to escape block --- use the AndInst as 
        // the condition
        Instruction *OldBlockTerminator = OldBlock->getTerminator();
        BranchInst *BranchToModify = dyn_cast<BranchInst>(OldBlockTerminator);
        if (BranchToModify == nullptr)
            abort();

        BranchToModify->setCondition(AndInst);
        BranchToModify->setSuccessor(1, EscapeBlock); // 0th successor is NewBlock
    }

    return;
}