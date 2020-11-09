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
    DFA *TheDFA, 
    Constant* numNowPtr,
    std::unordered_map<Instruction*, pair<Instruction*, Value*>> storeInsts
    ) {
    /*
     * Set passed state
     */ 
    this->F = F;
    this->TheDFA = TheDFA;
    this->numNowPtr = numNowPtr,
    this->storeInsts = storeInsts;
}


/*
 * ---------- Drivers ----------
 */
void Injector::Inject(void)
{

    bool allocaOutsideFirstBB = allocaOutsideFirstBB();

    /*
     * Identify where to place the guards. TODO: this needs to be split up
     */
    std::unordered_map<Function *, bool> functionAlreadyChecked;
    for(auto& B : F){
        for(auto& I : B){
#if CALL_GUARD
        if(isa<CallInst>(I) || isa<InvokeInst>(I)){
#if !OPTIMIZED
            allocaOutsideFirstBB = true;
#endif

            Function *calleeFunction;
#if OPTIMIZED
            /*
            * Check if we are invoking an LLVM metadata callee. We don't need to guard these calls.
            */
            if (auto tmpCallInst = dyn_cast<CallInst>(&I)){
                calleeFunction = tmpCallInst->getCalledFunction();
            } else {
                auto tmpInvokeInst = cast<InvokeInst>(&I);
                calleeFunction = tmpInvokeInst->getCalledFunction();
            }
            if ( true
                && (calleeFunction != nullptr)
                && calleeFunction->isIntrinsic()
                ){
                //errs() << "YAY: no need to guard calls to intrinsic LLVM functions: " << I << "\n" ;
                continue;
            }

            /*
            * Check if we have already checked the callee and there is no alloca outside the first basic block. We don't need to guard more than once a function in this case.
            */
            if (  true
                && !allocaOutsideFirstBB
                && (calleeFunction != nullptr)
                && (functionAlreadyChecked[calleeFunction] == true)
                ){
            //errs() << "YAY: no need to guard twice the callee of the instruction " << I << "\n" ;
                continue ;
            }
#endif

            /*
             * Check if we can hoist the guard.
             */
            if (allocaOutsideFirstBB){

                /*
                 * We cannot hoist the guard.
                 */
                storeInsts[&I] = {&I, numNowPtr};
                //errs() << "NOOO: missed optimization because alloca invocation outside the first BB for : " << I << "\n" ;
                nonOptimizedGuard++;
                continue ;
            }

            /*
            * We can hoist the guard because the size of the allocation frame is constant.
            *
            * We decided to place the guard at the beginning of the function. 
            * This could be good if we have many calls to this callee.
            * This could be bad if we have a few calls to this callee and they could not be executed.
            */
            //errs() << "YAY: we found a call check that can be hoisted: " << I << "\n" ;
            storeInsts[&I] = {firstInst, numNowPtr};
            functionAlreadyChecked[calleeFunction] = true;
            callGuardOpt++;
            continue ;
        }
#endif
#if STORE_GUARD
        if(isa<StoreInst>(I)){
            auto storeInst = cast<StoreInst>(&I);
            auto pointerOfStore = storeInst->getPointerOperand();
            findPointToInsertGuard(&I, pointerOfStore);
            continue ;
        }
#endif
#if LOAD_GUARD
        if(isa<LoadInst>(I)){
            auto loadInst = cast<LoadInst>(&I);
            auto pointerOfStore = loadInst->getPointerOperand();
            findPointToInsertGuard(&I, pointerOfStore);
            continue ;
        }
#endif
        }
    }

    printGuards(storeInsts);


}



/*
 * ---------- Private methods ----------
 */
std::function<void (Instruction *inst, Value *pointerOfMemoryInstruction)> _findPointToInsertGuard(void) {

    /*
        * Define the code that will be executed to identify where to place the guards.
        */
    //errs() << "COMPILER OPTIMIZATION\n";
    //F.print(errs());
    auto findPointToInsertGuard = 
    [&redundantGuard, numNowPtr, TheDFA, &noelle, &programLoops, &storeInsts, &nonOptimizedGuard, &scalarEvolutionGuard, &loopInvariantGuard](Instruction *inst, Value *pointerOfMemoryInstruction) -> void {
#if OPTIMIZED
        /*
        * Check if the pointer has already been guarded.
        */
        auto& INSetOfI = TheDFA->IN(inst);
        if (INSetOfI.find(pointerOfMemoryInstruction) != INSetOfI.end()){
            //errs() << "YAY: skip a guard for the instruction: " << *inst << "\n";
            redundantGuard++;                  
            return ;
        }

        /*
        * We have to guard the pointer.
        *
        * Check if we can hoist the guard outside the loop.
        */
        //errs() << "XAN: we have to guard the memory instruction: " << *inst << "\n" ;
        auto added = false;
        auto nestedLoop = noelle.getInnermostLoopThatContains(*programLoops, inst);
        if (nestedLoop == nullptr){
            /*
             * We have to guard just before the memory instruction.
             */
            storeInsts[inst] = {inst, pointerOfMemoryInstruction};
            nonOptimizedGuard++;
            //errs() << "XAN:   The instruction doesn't belong to a loop so no further optimizations are available\n";
            return ;
        }
        //errs() << "XAN:   It belongs to the loop " << *nestedLoop << "\n";
        auto nestedLoopLS = nestedLoop->getLoopStructure();
        Instruction* preheaderBBTerminator = nullptr;
        while (nestedLoop != NULL){
        auto invariantManager = nestedLoop->getInvariantManager();
        if (  false
            || invariantManager->isLoopInvariant(pointerOfMemoryInstruction)
            ){
            //errs() << "YAY:   we found an invariant address to check:" << *inst << "\n";
            auto preheaderBB = nestedLoopLS->getPreHeader();
            preheaderBBTerminator = preheaderBB->getTerminator();
            nestedLoop = noelle.getInnermostLoopThatContains(*programLoops, preheaderBB);
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
            storeInsts[inst] = {preheaderBBTerminator, pointerOfMemoryInstruction};
            return ;
        }
        //errs() << "XAN:   It cannot be hoisted. Check if it can be merged\n";

        /*
        * The memory instruction isn't loop invariant.
        *
        * Check if it is based on an induction variable.
        */
        nestedLoop = noelle.getInnermostLoopThatContains(*programLoops, inst);
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

                storeInsts[inst] = {preheaderBBTerminator, startAddress};
                return;
            }
        }
#endif //OPTIMIZED
        //errs() << "NOOO: the guard cannot be hoisted or merged: " << *inst << "\n" ;

        /*
        * We have to guard just before the memory instruction.
        */
        storeInsts[inst] = {inst, pointerOfMemoryInstruction};
        nonOptimizedGuard++;
        return ;
    };

    return findPointToInsertGuard;
}

bool Injector::allocaOutsideFirstBB() {
    /*
     * Check if there is no stack allocations other than 
     * those in the first basic block of the function.
     */
    for(auto& B : F){
        for(auto& I : B){
            if (  true
                && isa<AllocaInst>(&I)
                && (&B != firstBB)
                ){

                /*
                * We found a stack allocation not in the first basic block.
                */
                //errs() << "NOOOO: Found an alloca outside the first BB = " << I << "\n";
                return true;
            }
        }
    }
    return false;
}

void printGuards() {
    /*
     * Print where to put the guards
     */
    //errs() << "GUARDS\n";
    for (auto& guard : storeInsts){
        auto inst = guard.first;
        //errs() << " " << *inst << "\n";
    }
}