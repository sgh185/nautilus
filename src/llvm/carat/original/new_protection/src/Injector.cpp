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
 * Authors: Drew Kersnar, Souradip Ghosh, 
 *          Brian Suchy, Simone Campanoni, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "../include/Injector.hpp"

/*
 * ---------- Constructors ----------
 */
Injector::Injector(
    Function *F, 
    DataFlowResult *DFR, 
    Constant *numNowPtr,
    Noelle *noelle,
    Function *ProtectionsMethod
) : F(F), DFR(DFR), numNowPtr(numNowPtr), ProtectionsMethod(ProtectionsMethod), noelle(noelle) 
{
   
    /*
     * Set new state
     */ 
    this->programLoops = noelle.getProgramLoops();


    /*
     * Perform initial analysis
     */ 
    __allocaOutsideFirstBBChecker();


    /*
     * Fetch the first instruction of the entry basic block
     */ 
    First = F->getEntryBasicBlock().getFirstNonPHI();

}


/*
 * ---------- Drivers ----------
 */
void Injector::Inject(void)
{
    /*
     * Do the dirty work
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


void Injector::visitInvokeInst(InvokeInst &I)
{
    /*
     * Assumption for Nautilus
     */  

    errs() << "Found a invoke instruction in function " << F->getName() << "\n"
           << I << "\n";

    abort();
}

void Injector::visitCallInst(CallInst &I)
{
    /*
     * Debugging --- FIX
     * 
     * NOTE --- Ideally, only some intrinsics should be 
     * instrumented (i.e. llvm.memcpy, etc.), and markers
     * (i.e. llvm.lifetime, etc.) should be ignored. For
     * now, we are instrumenting ALL intrinsics as a 
     * conservative approach
     */  
    if (I.isIntrinsic())
    {
        errs() << "Found an intrinsic! Instrumenting for now ... \n" 
               << I << "\n";
    }


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
     * If the callee of @I has already been instrumented and 
     * all stack locations are at the top of the entry basic
     * block (@this->AllocaOutsideEntry), then nothing else 
     * needs to be done --- return
     */ 
    if (true
        && !AllocaOutsideEntry
        && (Callee)
        && (InstrumentedFunctions[Callee])) { return; }


    /*
     * If not all stack locations are grouped at the 
     * top of the entry basic block, we cannot hoist
     * any guards of the call instruction --- instrument
     * @I at @I
     * 
     * Otherwise, hoist the guard for @I to the first 
     * instruction in the entry basic block
     * 
     * Update statistics
     */ 
    if (AllocaOutsideEntry)
    {
        InjectionLocations[&I] = 
            GuardInfo(
                &I,
                numNowPtr, // TODO: change to the stack pointer location during the function call
                true /* IsWrite */ 
            );

        nonOptimizedGuard++;
    }
    else
    {
        InjectionLocations[&I] = 
            GuardInfo(
                First,
                numNowPtr, // TODO: change to the stack pointer location during the function call
                true /* IsWrite */ 
            );

        callGuardOpt++;
    }


    /*
     * Mark the callee as handled
     */ 
    InstrumentedFunctions[Callee] = true;


    return;
}

/*
 * Simplification --- reduce code repitition
 */ 
#define LAMBDA_INVOCATION \
    auto TheLambda = _findPointToInsertGuard(); \
    \
    Value *PointerToHandle = I.getPointerOperand(); \
    TheLambda(&I, PointerToHandle, isWrite); \


void Injector::visitStoreInst(StoreInst &I)
{
    bool isWrite = true;
    LAMBDA_INVOCATION
    return;
}


void Injector::visitLoadInst(LoadInst &I)
{
    bool isWrite = false;
    LAMBDA_INVOCATION
    return;
}


/*
 * ---------- Private methods ----------
 */
void Injector::_findInjectionLocations(void)
{
    /*
     * Invoke the visitors to fill out the InjectionLocations map
     */ 
    this->visit(F);


    /*
     * Debugging
     */ 
    printGuards();


    return;
}


void Inject::_doTheInject(void)
{
    /*
     * Do the inject
     */ 
    for (auto const &[guardedInst, guardInfo] : InjectionLocations) {

        // TODO: insert call to ProtectionsMethod at guardInfo.InjectionLocation, 
        // with arguments guardInfo.PointerToGuard and guardInfo.IsWrite
    }

}


std::function<void (Instruction *inst, Value *pointerOfMemoryInstruction)> Injector::_findPointToInsertGuard(void) {

    /*
     * Define the lamda that will be executed to identify where to place the guards.
     */

    auto findPointToInsertGuard = 
    [this](Instruction *inst, Value *pointerOfMemoryInstruction, bool isWrite) -> void {
#if OPTIMIZED
        /*
         * Check if the pointer has already been guarded.
         */
        auto& INSetOfI = DFR->IN(inst);
        if (INSetOfI.find(pointerOfMemoryInstruction) != INSetOfI.end()) {
            //errs() << "YAY: skip a guard for the instruction: " << *inst << "\n";
            redundantGuard++;                  
            return;
        }

        /*
         * We have to guard the pointer.
         *
         * Check if we can hoist the guard outside the loop.
         */
        //errs() << "XAN: we have to guard the memory instruction: " << *inst << "\n" ;
        auto added = false;
        auto nestedLoop = noelle->getInnermostLoopThatContains(*programLoops, inst);
        if (nestedLoop == nullptr) {
            /*
             * We have to guard just before the memory instruction.
             * The instruction doesn't belong to a loop so no further optimizations are available.
             */
            InjectionLocations[inst] = 
                GuardInfo(
                    inst,
                    pointerOfMemoryInstruction, 
                    isWrite
                );
            nonOptimizedGuard++;
            //errs() << "XAN:   The instruction doesn't belong to a loop so no further optimizations are available\n";
            return;
        }
        /*
         * @inst belongs to a loop, so we can try to hoist the guard.
         */
        //errs() << "XAN:   It belongs to the loop " << *nestedLoop << "\n";
        auto nestedLoopLS = nestedLoop->getLoopStructure();
        Instruction* preheaderBBTerminator = nullptr;
        while (nestedLoop != NULL) {
            auto invariantManager = nestedLoop->getInvariantManager();
            if (  false
                || invariantManager->isLoopInvariant(pointerOfMemoryInstruction)
                ) {
                //errs() << "YAY:   we found an invariant address to check:" << *inst << "\n";
                auto preheaderBB = nestedLoopLS->getPreHeader();
                preheaderBBTerminator = preheaderBB->getTerminator();
                nestedLoop = noelle->getInnermostLoopThatContains(*programLoops, preheaderBB);
                added = true;

            } else {
                break;
            }
        }
        if(added){
            /*
             * We can hoist the guard outside the loop.
             */
            loopInvariantGuard++;
            InjectionLocations[inst] = 
                GuardInfo(
                    preheaderBBTerminator,
                    pointerOfMemoryInstruction, 
                    isWrite
                );
            return;
        }
        //errs() << "XAN:   It cannot be hoisted. Check if it can be merged\n";

        /*
        * The memory instruction isn't loop invariant.
        *
        * Check if it is based on an induction variable.
        */
        nestedLoop = noelle->getInnermostLoopThatContains(*programLoops, inst);
        assert(nestedLoop != nullptr);
        nestedLoopLS = nestedLoop->getLoopStructure();
        auto ivManager = nestedLoop->getInductionVariableManager();
        auto pointerOfMemoryInstructionInst = dyn_cast<Instruction>(pointerOfMemoryInstruction);
        if (ivManager->doesContributeToComputeAnInductionVariable(pointerOfMemoryInstructionInst)){
            /*
                * The computation of the pointer is based on a bounded scalar evolution.
                * This means, the guard can be hoisted outside the loop where the boundaries used in the check go from the lowest to the highest address.
                */
            //SCEV_CARAT_Visitor visitor{};
            auto startAddress = numNowPtr; // FIXME Iterate over SCEV to fetch the actual start and end addresses
            //auto startAddress = visitor.visit((const SCEV *)AR);
            /*auto startAddressSCEV = AR->getStart();
                errs() << "XAN:         Start address = " << *startAddressSCEV << "\n";
                Value *startAddress = nullptr;
                if (auto startGeneric = dyn_cast<SCEVUnknown>(startAddressSCEV)){
                startAddress = startGeneric->getValue();
                } else if (auto startConst = dyn_cast<SCEVConstant>(startAddressSCEV)){
                startAddress = startConst->getValue();
                }*/
            if (startAddress){
                //errs() << "YAY: we found a scalar evolution-based memory instruction: " ;
                //inst->print(errs());
                //errs() << "\n";
                auto preheaderBB = nestedLoopLS->getPreHeader();
                preheaderBBTerminator = preheaderBB->getTerminator();

                scalarEvolutionGuard++;

                InjectionLocations[inst] = 
                    GuardInfo(
                        preheaderBBTerminator,
                        startAddress,
                        isWrite
                    );
                return;
            }
        }
#endif //OPTIMIZED
        //errs() << "NOOO: the guard cannot be hoisted or merged: " << *inst << "\n" ;

        /*
         * The guard cannot be hoisted or merged.
         * We have to guard just before the memory instruction.
         */
        InjectionLocations[inst] = 
            GuardInfo(
                inst,
                pointerOfMemoryInstruction, 
                isWrite
            );
        nonOptimizedGuard++;
        return;
    };

    return findPointToInsertGuard;
}

bool Injector::_allocaOutsideFirstBBChecker() {
    /*
     * Check if there is no stack allocations other than 
     * those in the first basic block of the function.
     */
    auto firstBB = &*(F->begin());
    for(auto& B : *F){
        for(auto& I : B){
            if (  true
                && isa<AllocaInst>(&I)
                && (&B != firstBB)
                ){

                /*
                 * We found a stack allocation not in the first basic block.
                 */
                //errs() << "NOOOO: Found an alloca outside the first BB = " << I << "\n";
                AllocaOutsideEntry = true;
            }
        }
    }
    AllocaOutsideEntry = false;

    return;
}

void Injector::printGuards() {
    /*
     * Print where to put the guards
     */
    errs() << "GUARDS\n";
    for (auto& guard : InjectionLocations){
        auto inst = guard.first;
        errs() << " " << *inst << "\n";
    }


    //Print results
    errs() << "Guard Information\n";
    errs() << "Unoptimized Guards:\t" << nonOptimizedGuard << "\n"; 
    errs() << "Redundant Optimized Guards:\t" << redundantGuard << "\n"; 
    errs() << "Loop Invariant Hoisted Guards:\t" << loopInvariantGuard << "\n"; 
    errs() << "Scalar Evolution Combined Guards:\t" << scalarEvolutionGuard << "\n"; 
    errs() << "Hoisted Call Guards\t" << callGuardOpt << "\n"; 
    errs() << "Total Guards:\t" << nonOptimizedGuard + loopInvariantGuard + scalarEvolutionGuard << "\n"; 


    return;
}