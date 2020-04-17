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
 * Copyright (c) 2019, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2019, Simone Campanoni <simonec@eecs.northwestern.edu>
 * Copyright (c) 2019, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Souradip Ghosh <sgh@u.northwestern.edu>
 *          Simone Campanoni <simonec@eecs.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
 * CompilerTiming.cpp
 * ----------------------------------------
 * 
 * Transformation that injects calls to a wrapper to a pseudo interrupt handler ("nk_time_hook_fire")
 * based on analyses that calculate latency measurements for bitcode level instructions in the kernel
 * module, and a series of transformations at the loop and function level.
 */

#include "include/LoopTransform.hpp"

namespace
{
struct CAT : public ModulePass
{
    static char ID;

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
        Utils::ExitOnInit();


        /*
         * Find all functions with special names (NKNames) and apply
         * NoInline attributes --- preserve for analysis and transformation
         */ 
        for (auto Name : NKNames)
        {
            auto F = M.getFunction(Name);
            if (F == nullptr)
                continue;
            
            F->addFnAttr(Attribute::NoInline);
        }


        /*
         * Find all functions with special "nohook" attributes and add
         * NoInline attributes to those functions as well --- prevent indirect
         * injections that may occur via inlining
         */
        GlobalVariable *GV = M.getGlobalVariable(ANNOTATION);
        if (GV == nullptr)
            return false;
        
        vector<Function *> AnnotatedFunctions;
        Utils::GatherAnnotatedFunctions(GV, AnnotatedFunctions);
        for (auto AF : AnnotatedFunctions)
        {
            if (AF == nullptr)
                continue;
            
            AF->addFnAttr(Attribute::NoInline);
            NoHookFunctionSignatures->push_back(AF->getName());
        }

        return false;
    }

    bool runOnModule(Module &M) override
    {
        /*
         * Find all functions necessary for analysis and transformation. If 
         * at least one function is missing, abort.
         */ 
        for (auto ID : NKIDs)
        {
            auto F = M.getFunction(NKNames[ID]);
            if (F == nullptr)
            {
                DEBUG_INFO("Nullptr special routine: "
                           + NKNames[ID] + "\n");
                abort(); // Serious
            }
            
            (*SpecialRoutines)[ID] = F;
        }


        /*
         * Populate NoHookFunctions --- these functions are specified so no 
         * calls to nk_time_hook_fire are injected into these.
         */ 
        for (auto Name : *NoHookFunctionSignatures)
        {
            auto F = M.getFunction(Name);
            if (F == nullptr)
                continue;

            NoHookFunctions->push_back(F);
        }


#if INJECT

        /*
         * TOP LEVEL --- 
         * Perform analysis and transformation on fiber routines. These routines
         * are the most specific interface for compiler-based timing. Fiber routines
         * are always analyzed and transformed.
         */ 

        /*
         * Force inlining of nk_fiber_start to generate direct calls to nk_fiber_create
         * (nk_fiber_start calls nk_fiber_create internally) --- we can extract all
         * fiber routines in the kernel by following the arguments of nk_fiber_create.  
         */ 
        Utils::InlineNKFunction((*SpecialRoutines)[FIBER_START]);
        set<Function *> Routines = *(Utils::IdentifyFiberRoutines());

#if WHOLE

        /*
         * If whole kernel injection/optimization is turned on, we want to inject 
         * callbacks to nk_time_hook_fire across the entire kernel module. In order
         * to do so, we want to identify all valid functions in the bitcode and mark
         * them (add them in the Routines set) 
         */ 

        Utils::IdentifyAllNKFunctions(M, Routines);

#endif

#endif
        /*
         * Iterate through all the routines collected in the kernel, and perform all 
         * DFA, interval analysis, loop and function transformations, etc. --- calls
         * will be injected per function
         */ 

#if 0 
        for (auto Routine : Routines)
        {
            // Gather wrapper analysis state
            auto *DT = &getAnalysis<DominatorTreeWrapperPass>(*Routine).getDomTree();
            auto *AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*Routine);
            auto *LI = &getAnalysis<LoopInfoWrapperPass>(*Routine).getLoopInfo();
            auto *SE = &getAnalysis<ScalarEvolutionWrapperPass>(*Routine).getSE();
            OptimizationRemarkEmitter ORE(Routine);

            vector<Loop *> Loops;
            for (auto L : *LI)
                Loops.push_back(L);

			VERIFY_DEBUG_INFO("F: " + Routine->getName() + "\n");
			
            for (auto L : Loops)
            {
                VERIFY_OBJ_INFO(L);
				VERIFY_DEBUG_INFO(to_string(SE->getSmallConstantTripCount(L)));
                VERIFY_DEBUG_INFO(to_string(SE->getSmallConstantTripMultiple(L)));
            }
        }

		return false;
#endif

        for (auto Routine : Routines)
        {
			// Set injection locations data structure
            set<Instruction *> InjectionLocations;

            
            // Gather wrapper analysis state
            auto *DT = &getAnalysis<DominatorTreeWrapperPass>(*Routine).getDomTree();
            auto *AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*Routine);
            auto *LI = &getAnalysis<LoopInfoWrapperPass>(*Routine).getLoopInfo();
            auto *SE = &getAnalysis<ScalarEvolutionWrapperPass>(*Routine).getSE();
            OptimizationRemarkEmitter ORE(Routine);


            // Gather loops
            vector<Loop *> Loops;
            for (auto L : *LI)
                Loops.push_back(L);


            // Execute loop analysis and transformations, collect any callback
            // locations generated
            for (auto L : Loops)
            {
                auto LT = new LoopTransform(L, Routine, LI, DT, SE, AC, &ORE, GRAN, L);
                LT->Transform();

                for (auto I : *(LT->GetCallbackLocations()))
                    InjectionLocations.insert(I);
            }


            // Execute function level analysis and transformation, collect any 
            // callback locations generated
            LatencyDFA *FunctionIDFA = new LatencyDFA(Routine, LI, EXPECTED, MEDCON, true);
            FunctionIDFA->ComputeDFA();
            auto FCallbackLocations = FunctionIDFA->BuildIntervalsFromZero();

            for (auto I : *FCallbackLocations)
                InjectionLocations.insert(I);


            // Inject callbacks at all specified injection locations
            Utils::InjectCallback(InjectionLocations, Routine);

            if (verifyFunction(*Routine, &(errs())))
            {
                VERIFY_DEBUG_INFO("\n");
                VERIFY_DEBUG_INFO(Routine->getName() + "\n");
                VERIFY_DEBUG_INFO("\n\n\n");
            }
        }


#if INLINE

        /*
         * Inline the callback function --- nk_time_hook_fire, option for
         * a potential optimization. 
         * 
         * (NOTE --- this is potentially very dangerous for at low granularities 
         * --- code bloat and explosion of instrisic debug info causes a 
         * shift of the .text section at link time and potential linker 
         * failure, also may cause performance bottleneck at low granularities 
         * (instruction cache thrashing))
         */ 
        Utils::InlineNKFunction((*SpecialRoutines)[HOOK_FIRE]);

#endif


        return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override
    {
        AU.addRequired<LoopInfoWrapperPass>();
        AU.addRequired<AssumptionCacheTracker>();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<ScalarEvolutionWrapperPass>();
        return;
    }
};
} 

char CAT::ID = 0;
static RegisterPass<CAT> X("ct", "Compiler-timing");

static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT());} });
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT()); } });

