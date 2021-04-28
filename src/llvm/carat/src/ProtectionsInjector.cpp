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
 * Copyright (c) 2021, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2021, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2021, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2021, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2021, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Souradip Ghosh, 
 *          Brian Suchy, Simone Campanoni, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "autoconf.h"

#if NAUT_CONFIG_USE_NOELLE

#include "../include/ProtectionsInjector.hpp"

/*
 * ---------- Constructors ----------
 */
ProtectionsInjector::ProtectionsInjector(
    Function *F, 
    DataFlowResult *DFR, 
    Value *NonCanonical,
    Noelle *noelle,
    Function *ProtectionsMethod
) : F(F), DFR(DFR), NonCanonical(NonCanonical), ProtectionsMethod(ProtectionsMethod), noelle(noelle) 
{
    /*
     * Set new state from NOELLE
     */ 
    auto ProgramLoops = noelle->getLoops();
    this->BasicBlockToLoopMap = noelle->getInnermostLoopsThatContains(*ProgramLoops);
    this->FDG = noelle->getFunctionDependenceGraph(F);


    /*
     * Perform initial analysis
     */ 
    _allocaOutsideFirstBBChecker();


    /*
     * Fetch the first instruction of the entry basic block
     */ 
    First = F->getEntryBlock().getFirstNonPHI();
}


/*
 * ---------- Drivers ----------
 */
void ProtectionsInjector::Inject(void)
{
    /*
     * Find all locations that guards need to be injected
     */
    _findInjectionLocations();


    /*
     * Now do the inject
     */ 
    _doTheInject();


    return;
}



/*
 * ---------- Visitor methods ----------
 */
void ProtectionsInjector::visitInvokeInst(InvokeInst &I)
{
    /*
     * Assumption for Nautilus --- no handling of invokes
     */  
    errs() << "Found a invoke instruction in function " << F->getName() << "\n"
           << I << "\n";

    abort();
}


void ProtectionsInjector::visitCallInst(CallInst &I)
{
    /*
     * Fetch the callee of @I
     */ 
    Function *Callee = I.getCalledFunction();


    /*
     * Debugging --- FIX 
     * 
     * NOTE --- We are instrumenting all indirect calls
     * because we have no idea what to do about this
     */ 
    if (!Callee) 
    {
        errs() << "Found an indirect call! Instrumenting for now ... \n" 
               << I << "\n";
    }


    /*
     * Debugging --- FIX
     * 
     * NOTE --- Ideally, only some intrinsics should be 
     * instrumented (i.e. llvm.memcpy, etc.), and markers
     * (i.e. llvm.lifetime, etc.) should be ignored. For
     * now, we are instrumenting ALL intrinsics as a 
     * conservative approach
     */  
    if (Callee->isIntrinsic())
    {
        errs() << "Found an intrinsic! Instrumenting for now ... \n" 
               << I << "\n";


        /*
         * APRIL, 2021 --- NOT HANDLING INTRINSICS --- REVISIT
         */ 
        return;
    }


    /*
     * If the callee of @I has already been instrumented and 
     * all stack locations are at the top of the entry basic
     * block (@this->AllocaOutsideEntry), then nothing else 
     * needs to be done --- return
     */ 
    if (true
        && !AllocaOutsideEntry
        && (Callee)
        && (InstrumentedFunctions[Callee])) { 
        return;
    }


    /*
     * If not all stack locations are grouped at the 
     * top of the entry basic block, we cannot hoist
     * any guards of the call instruction --- instrument
     * @I at @I
     * 
     * Otherwise, hoist the guard for @I to the first 
     * instruction in the entry basic block
     * 
     * Finally, update statistics
     */ 
    if (AllocaOutsideEntry)
    {
        InjectionLocations[&I] = 
            new GuardInfo(
                &I,
                NonCanonical, // TODO: change to the stack pointer location during the function call
                true, /* IsWrite */
                CARATNamesToMethods[CARAT_STACK_GUARD],
                "protect", /* Metadata type */
                "non.opt.call.guard" /* Metadata attached to injection */
            );

        nonOptimizedGuard++;
    }
    else
    {
        InjectionLocations[&I] = 
            new GuardInfo(
                First,
                NonCanonical, // TODO: change to the stack pointer location during the function call
                true, /* IsWrite */ 
                CARATNamesToMethods[CARAT_STACK_GUARD],
                "protect", /* Metadata type */
                "opt.call.guard" /* Metadata attached to injection */
            );

        callGuardOpt++;
    }


    /*
     * Mark the callee as handled
     */ 
    InstrumentedFunctions[Callee] = true;


    return;
}


void ProtectionsInjector::visitStoreInst(StoreInst &I)
{
    bool IsWrite = true;
    _invokeLambda(&I, IsWrite);
    return;
}


void ProtectionsInjector::visitLoadInst(LoadInst &I)
{
    bool IsWrite = false;
    _invokeLambda(&I, IsWrite);
    return;
}


/*
 * ---------- Private methods ----------
 */
void ProtectionsInjector::_findInjectionLocations(void)
{
    /*
     * Invoke the visitors to fill out the InjectionLocations map
     */ 
    this->visit(F);


    /*
     * Debugging
     */ 
    _printGuards();


    return;
}


std::vector<Value *> ProtectionsInjector::_buildStackGuardArgs(GuardInfo *GI)
{
    /*
     * TOP --- Build the function arguments for the call injection
     * for the method "nk_carat_guard_callee_stack"
     */

    /*
     * Calculate the stack frame size to check --- set as a "uint64_t"
     *
     * TODO --- Actually calculate this, set to 512 for now
     */
    const uint64_t StackFrameSize = 512;
    llvm::IRBuilder<> Builder{F->getContext()};
    std::vector<Value *> CallArgs = {
        Builder.getInt64(StackFrameSize)
    };


    return CallArgs;
}


std::vector<Value *> ProtectionsInjector::_buildGenericProtectionArgs(GuardInfo *GI)
{
    /*
     * TOP --- Build the function arguments for the call injection
     * for the method "nk_carat_guard_address"
     */

    /*
     * Set up builder
     */

    llvm::IRBuilder<> Builder = 
        Utils::GetBuilder(
            GI->InjectionLocation->getFunction(),
            GI->InjectionLocation
        );


    /*
     * Cast @GI->PointerToGuard as a void pointer for instrumentation
     */
    Value *VoidPointerToGuard = 
        Builder.CreatePointerCast(
            GI->PointerToGuard,
            Builder.getInt8PtrTy()
        );


    /*
     * Build the call args
     */
    std::vector<Value *> CallArgs = {
        VoidPointerToGuard,
        Builder.getInt32(GI->IsWrite)
    };


    return CallArgs;
}


void ProtectionsInjector::_doTheInject(void)
{
    /*
     * Do the inject
     */ 
    for (auto const &[InstToGuard, GI] : InjectionLocations) 
    {
        /*
         * Set up builder
         */
        llvm::IRBuilder<> Builder = 
            Utils::GetBuilder(
                GI->InjectionLocation->getFunction(),
                GI->InjectionLocation
            );


        /*
         * Set up arguments based on the "GI->FunctionToInject" field
         */ 
        Function *FTI = GI->FunctionToInject;
        std::vector<Value *> CallArgs = 
            (FTI == CARATNamesToMethods[CARAT_STACK_GUARD]) ?
            (_buildStackGuardArgs(GI)) :
            (_buildGenericProtectionArgs(GI));


        /*
         * Inject the call instruction(s) based on the selected
         * args and set specialized metadata for each call
         */
        for (auto N = 0 ; N < GI->NumInjections ; N++)
        {
            CallInst *Instrumentation = 
                Builder.CreateCall(
                    FTI, 
                    ArrayRef<Value *>(CallArgs)
                );  

            Utils::SetInstrumentationMetadata(
                Instrumentation,
                GI->MDTypeString,
                GI->MDLiteral
            );
        }

    }


    return;
}


bool ProtectionsInjector::_optimizeForLoopInvariance(
    LoopDependenceInfo *NestedLoop,
    Instruction *I, 
    Value *PointerOfMemoryInstruction, 
    bool IsWrite
)
{
    /*
     * If @NestedLoop is not valid, we cannot optimize for loop invariance
     */
    if (!NestedLoop) { 
        return false;
    }


    /*
     * Setup --- fetch the loop structure for @NestedLoop
     */
    bool Hoistable = false;
    LoopDependenceInfo *NextLoop = NestedLoop;
    LoopStructure *NextLoopStructure = NextLoop->getLoopStructure();
    Instruction *InjectionLocation = nullptr;


    /*
     * Fetch @PointerOfMemoryInstruction as an instruction
     */
    Instruction *PointerAsInst = dyn_cast<Instruction>(PointerOfMemoryInstruction);


    /*
     * Walk up the loop nest until @PointerOfMemoryInstruction to determine 
     * the outermost loop of which PointerOfMemoryInstruction is a loop 
     * invariant. 
     */
    while (NextLoop) 
    {
        /*
         * If @PointerOfMemoryInstruction is defined within the 
         * next loop, it can't be hoisted out of the loop without
         * a proper LICM --- FIX. This is the farthest we can hoist.
         * 
         * HACK, TODO
         */
        bool IsInLoop = false;
        if (PointerAsInst) {
            if (NextLoopStructure->isIncluded(PointerAsInst)) {
                IsInLoop = true;
            }
        }


        /*
         * Fetch the invariant manager of the next loop 
         */
        InvariantManager *Manager = NextLoop->getInvariantManager();


        /*
         * If @PointerOfMemoryInstruction is not a loop invariant
         * of this loop, we cannot iterate anymore --- break and 
         * determine if the guard can actually be hoisted
         * 
         * Essentially, we continue if we can hoist
         */ 
        if (false
            || !(Manager->isLoopInvariant(PointerOfMemoryInstruction))
            || (IsInLoop)) {
            break;
        }


        /*
         * We know we're dealining with a loop invariant --- set the 
         * injection location to this loop's preheader terminator and 
         * continue to iterate.
         */
        BasicBlock *PreHeader = NextLoopStructure->getPreHeader();
        InjectionLocation = PreHeader->getTerminator();


        /*
         * Set state for the next iteration up the loop nest
         */
        LoopDependenceInfo *ParentLoop = BasicBlockToLoopMap[PreHeader];
        assert (ParentLoop != NextLoop);

        NextLoop = ParentLoop;
        NextLoopStructure = 
            (NextLoop) ?
            (NextLoop->getLoopStructure()) :
            (nullptr) ;

        Hoistable |= true;
    }


    /*
     * If we can truly hoist the guard --- mark the 
     * injection location, update statistics, and return
     */
    if (Hoistable)
    {
        InjectionLocations[I] = 
            new GuardInfo(
                InjectionLocation,
                PointerOfMemoryInstruction, 
                IsWrite,
                CARATNamesToMethods[CARAT_PROTECT],
                "protect", /* Metadata type */
                "loop.ivt.guard" /* Metadata attached to injection */
            );

        loopInvariantGuard++;
    }


    return Hoistable;
}


bool ProtectionsInjector::_optimizeForInductionVariableAnalysis(
    LoopDependenceInfo *NestedLoop,
    Instruction *I, 
    Value *PointerOfMemoryInstruction, 
    bool IsWrite
)
{
    /*
     * If @NestedLoop is not valid, we cannot optimize for loop invariance
     */
    if (!NestedLoop) { 
        return false; 
    }


    /*
     * Setup --- fetch the loop structure and IV manager for @NestedLoop
     */
    LoopStructure *NestedLoopStructure = NestedLoop->getLoopStructure();
    InductionVariableManager *IVManager = NestedLoop->getInductionVariableManager();


    /*
     * Fetch @PointerOfMemoryInstruction as an instruction, sanity check
     */
    Instruction *PointerOfMemoryInstructionAsInst = dyn_cast<Instruction>(PointerOfMemoryInstruction);
    if (!PointerOfMemoryInstructionAsInst) { 
        return false; 
    }


    /*
     * Check if @PointerOfMemoryInstruction contributes to an induction 
     * variable --- if not, there's no optimization we can do
     */
    if (!(IVManager->doesContributeToComputeAnInductionVariable(PointerOfMemoryInstructionAsInst))) {
        return false;
    }


    /*
     * At this point, we know that the computation of @PointerOfMemoryInstruction
     * depends on a bounded scalar evolution --- which means that the guard can be
     * hoisted outside the loop where the boundaries used in the check can range from
     * start to end (low to high??) address of the scalar evolution
     * 
     * FIX --- Currently using a non-canonical address for the start address and not 
     *         checking the end addresss
     */
    bool Hoisted = false;
    Value *StartAddress = NonCanonical; /* FIX */
    if (StartAddress)
    {
        BasicBlock *PreHeader = NestedLoopStructure->getPreHeader();
        Instruction *InjectionLocation = PreHeader->getTerminator();

        InjectionLocations[I] = 
            new GuardInfo(
                InjectionLocation,
                StartAddress,
                IsWrite,
                CARATNamesToMethods[CARAT_PROTECT],
                "protect", /* Metadata type */
                "iv.scev.guard", /* Metadata attached to injection */
                2 /* Need 2 injections for IV/SCEV guards --- FIX */
            );

        scalarEvolutionGuard++;
        Hoisted |= true;
    }


    return Hoisted;
}


bool ProtectionsInjector::_isAPointerReturnedByAllocator(Value *V)
{
    /*
     * Fetch @V as a call instruction
     */
    CallInst *Call = dyn_cast<CallInst>(V);
    if (!Call) return false;


    /*
     * Vet the callee
     */ 
    Function *Callee = Call->getCalledFunction();
    if (!Callee) return false;


    /*
     * Fetch the right memory allocator map
     */ 
    std::unordered_map<Function *, AllocID> MapToUse = 
        (InstrumentingUserCode) ?
        (UserAllocMethodsToIDs) :
        (KernelAllocMethodsToIDs) ;


    /*
     * Check if the callee is one of the recognized
     * library allocator functions
     */
    if (MapToUse.find(Callee) == MapToUse.end()) return false;


    /*
     * We can prove that @V is from a memory allocator!
     */
    return true;
}


Value *ProtectionsInjector::_fetchGEPBasePointer(Value *Pointer)
{
    /*
     * TOP --- If @Pointer is a GEP, fetch the pointer operand,
     * which represents the base pointer of the aggregate type
     * that said GEP would be indexing into
     * 
     * NOTE --- This GEP must be safe! We must understand that 
     * the indexing operation is inbounds, otherwise send a 
     * nullptr back.
     */
    GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Pointer);

    Value *BasePointer =
        (GEP && (GEP->isInBounds())) ?
        (GEP->getPointerOperand()) :
        (nullptr);


    return BasePointer;
}


bool ProtectionsInjector::_isASafeMemoryConstruct(Value *Pointer)
{
    /*
     * TOP --- See steps 1b-d in this->_findPointToInsertGuard
     * 
     * There is an added tidbit here --- it's possible that the 
     * pointer we're analyzing is really an offset into an aggregate
     * data type --- typically represented in the IR as a GEP. Check
     * this level of indirection.
     */
    if (!Pointer) return false;

    if (false
        || isa<AllocaInst>(Pointer)
        || isa<GlobalVariable>(Pointer)
        || _isAPointerReturnedByAllocator(Pointer)
        || _isASafeMemoryConstruct(_fetchGEPBasePointer(Pointer))) {
        return true;
    } 


    return false;
}


std::function<void (Instruction *inst, Value *pointerOfMemoryInstruction, bool isWrite)> ProtectionsInjector::_findPointToInsertGuard(void) 
{
    /*
     * Define the lamda that will be executed to identify where to place the guards.
     */
    auto FindPointToInsertGuardFunc = 
    [this](Instruction *inst, Value *PointerOfMemoryInstruction, bool isWrite) -> void {

        /*
         * The scoop:
         * 
         * - @inst will be some kind of memory instruction (load/store, potentially
         *   call instruction)
         * - @PointerOfMemoryInstruction will be the pointer operand of said
         *   memory instruction (@inst)
         * - @isWrite denotes the characteristic of @inst (i.e. load=FALSE, 
         *   store=TRUE, etc.)
         * 
         * Several steps to check/perform:
         * 
         * 1) If @PointerOfMemoryInstruction has already been guarded:
         *    a) @PointerOfMemoryInstruction is in the IN set of @inst, indicating
         *       that the DFA has determined that the pointer need not be checked
         *       when considering guarding at @inst
         *    b) @PointerOfMemoryInstruction is an alloca i.e. we know all origins
         *       of allocas since they're on the stack, and there's a region for it.
         *    c) @PointerOfMemoryInstruction originates from a library allocator
         *       call instruction --- these pointers are the ones being tracked and
         *       also assumed to be safe b/c we trust the allocator --- there's a 
         *       region dedicated for this, it's the (at least initial) heap
         *    d) @PointerOfMemoryInstruction originates from a global variable, 
         *       we can assume a safe memory reference because globals lie in 
         *       known and designated region --- the blob.
         *    e) @PointerOfMemoryInstruction must aliases any of the variable or 
         *       data types described in 1b-d.
         *
         *    ... then we're done --- don't have to do anything!
         * 
         * 2) Otherwise, we can check if @inst is part of a loop nest. If that's the
         *    case, we can try to perform one of two optimizations:
         *    a) If @inst is a loop invariant, we can use NOELLE to understand how 
         *       far up the loop nest we can hoist the guard for @inst. Said guard will be 
         *       injected in the determined loop's preheader (b/c that's how hoisting works) 
         *       and guard the pointer directly.
         *    b) If this isn't possible (i.e. @inst isn't a loop invariant), then we can
         *       also determine if @inst contributes to an induction variable, which is
         *       going to be based on a scalar evolution expression. If NOELLE determines
         *       that @inst fits this characterization, then we guard the start address
         *       through the end address (**do we need to guard the end addr, or can we 
         *       guard start + an offset?**), and hoist this guard @inst's parent loop 
         *       (i.e to the preheader).
         *   
         * 3) If @inst wasn't a part of a loop nest or if the optimizations attempted
         *    did not work, then the guard must be placed right before @inst.
         */


        /*
         * <Step 1a.>
         */
        auto &INSetOfI = DFR->IN(inst);
        if (INSetOfI.find(PointerOfMemoryInstruction) != INSetOfI.end()) 
        {
            redundantGuard++;                  
            return;
        }


        /*
         * <Step 1b.-d.>
         */
        errs() << "_findPointToInsertGuard:\n";
        errs() << "\tinst: " << *inst  << "\n";
        errs() << "\tptr: " << *PointerOfMemoryInstruction << "\n";
        if (_isASafeMemoryConstruct(PointerOfMemoryInstruction))
        {
            errs() << "\t\tisASafeMemoryConstruct!\n";
            redundantGuard++;                  
            return;
        }
        

        /*
         * <Step 1e.>
         */
        bool MustAliasesSafeConstructs = false; 
        auto Iterator = 
            [this, inst, &MustAliasesSafeConstructs]
            (Value *depValue, DGEdge<Value> *dep) -> bool {

            errs() << "\t\tdep: " << *depValue << "\n";
        
            /*
             * If @dep is not a must dependence, return
             */
            if (!(dep->isMustDependence())) {
                return false;
            }
            
            errs() << "\t\t\tisMustDependence!\n";

            /*
             * <Conditions 1b-1d>.
             */
            if (_isASafeMemoryConstruct(depValue)) {
                errs() << "\t\t\tMustAliasesSafeConstructs!\n";
                MustAliasesSafeConstructs |= true;
            }


            return false;            

        };


        /*
         * Iterate over the dependences
         */
        auto Iterated = 
            FDG->iterateOverDependencesTo(
                PointerOfMemoryInstruction, 
                false, /* Control dependences */
                true, /* Memory dependences */
                true, /* Register dependences */
                Iterator
            );


        /*
         * If the iterator has identified a redudant guard, return
         */
        if (MustAliasesSafeConstructs) 
        {
            redundantGuard++;
            return;
        }
         

        /*
         * We have to guard the pointer --- fetch the 
         * potential loop nest that @inst belongs to
         */
        bool Guarded = false;
        LoopDependenceInfo *NestedLoop = BasicBlockToLoopMap[inst->getParent()];

        /*
         * <Step 2a.>
         */
        Guarded |= 
            _optimizeForLoopInvariance(
                NestedLoop,
                inst,
                PointerOfMemoryInstruction,
                isWrite
            );


        /*
         * <Step 2b.>
         */
        if (!Guarded)
        {
            Guarded |= 
                _optimizeForInductionVariableAnalysis(                
                    NestedLoop,
                    inst,
                    PointerOfMemoryInstruction,
                    isWrite
                );
        }


        /*
         * <Step 3>
         */
        if (!Guarded) 
        {
            InjectionLocations[inst] = 
                new GuardInfo(
                    inst,
                    PointerOfMemoryInstruction, 
                    isWrite,
                    CARATNamesToMethods[CARAT_PROTECT],
                    "protect", /* Metadata type */
                    "non.opt.mem.guard" /* Metadata attached to injection */
                );

            nonOptimizedGuard++;
        }


        return;
    };


    return FindPointToInsertGuardFunc;
}


void ProtectionsInjector::_allocaOutsideFirstBBChecker(void) 
{
    /*
     * Check if there is no stack allocations other than 
     * those in the first basic block of the function.
     */
    BasicBlock *FirstBB = &*(F->begin());
    for (auto &B : *F)
    {
        for (auto &I : B)
        {
            if (true
                && isa<AllocaInst>(&I)
                && (&B != FirstBB))
            {
                /*
                 * We found a stack allocation not in the entry basic block.
                 */
                AllocaOutsideEntry |= true;
                break;
            }
        }
    }


    return;
}


template<typename MemInstTy>
void ProtectionsInjector::_invokeLambda(
    MemInstTy *I,
    bool IsWrite
)
{
    /*
     * Fetch the lambda to invoke --- FIX
     */ 
    auto TheLambda = _findPointToInsertGuard();
    

    /*
     * Fetch the pointer to handle from @I
     */
    Value *PointerToHandle = I->getPointerOperand();


    /*
     * Invoke the lambda
     */
    TheLambda(I, PointerToHandle, IsWrite);


    return;
}


void ProtectionsInjector::_printGuards(void) 
{
    /*
     * Print where to put the guards
     */
    errs() << "GUARDS\n";
    for (auto &Guard : InjectionLocations)
        errs() << " " << *(Guard.first) << "\n";


    /*
     * Print guard statistics
     */
    errs() << "GUARDS: Guard Information\n";
    errs() << "GUARDS: Unoptimized Guards:\t" << nonOptimizedGuard << "\n"; 
    errs() << "GUARDS: Redundant Optimized Guards:\t" << redundantGuard << "\n"; 
    errs() << "GUARDS: Loop Invariant Hoisted Guards:\t" << loopInvariantGuard << "\n"; 
    errs() << "GUARDS: Scalar Evolution Combined Guards:\t" << scalarEvolutionGuard << "\n"; 
    errs() << "GUARDS: Hoisted Call Guards\t" << callGuardOpt << "\n"; 
    errs() << "GUARDS: Total Guards:\t" << nonOptimizedGuard + loopInvariantGuard + scalarEvolutionGuard << "\n"; 


    return;
}

#endif
