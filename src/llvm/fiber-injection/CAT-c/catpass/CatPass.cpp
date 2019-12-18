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
 * Transformation that injects calls to a wrapper for fiber yield function ("wrapper_nk_fiber_yield")
 * based on a data flow analysis that calcualtes expected and/or maximum latencies of bitcode 
 * instructions in a module
 */

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SparseBitVector.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include <vector>
#include <set>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cstdlib>

using namespace llvm;
using namespace std;

// Pass settings
#define DEBUG 0
#define LOOP_DEBUG 0
#define INLINE 0
#define INJECT 1
#define FALSE 0

// Fiber functions
#define WRAPPER_YIELD 0
#define INNER_YIELD 1
#define FIBER_START 2
#define FIBER_CREATE 3
#define IDLE_FIBER_ROUTINE 4

// Guards for yield call injections
#define GRAN 200
#define CALL_GUARDS 0
#define LOOP_GUARDS 1
#define LOOP_OPT 1

// Conservativeness, Latency path configurations
#define MAXIMUM 0
#define EXPECTED 1

#define HIGHCON 0
#define MEDCON 1
#define LOWCON 2

#define CONSERV HIGHCON
#define LATCONFIG EXPECTED

// Fiber function declarations
const vector<uint32_t> NK_ids = {WRAPPER_YIELD, INNER_YIELD, FIBER_START, FIBER_CREATE, IDLE_FIBER_ROUTINE};
const vector<string> NK_names = {"wrapper_nk_fiber_yield", "nk_fiber_yield", "nk_fiber_start", "nk_fiber_create", "__nk_fiber_idle"};
unordered_map<uint32_t, Function *> FIBERS;

namespace
{
struct debugInfo
{
    // Code injection info:
    int64_t totalLines;
    int64_t totalInjections;
    vector<Instruction *> InjectionLocations;

    // Inline information
    int64_t totalInlines;
    unordered_map<CallInst *, string> failedInlines;
    int64_t numFailedInlines;

    // Info on INITIAL yield calls in module --- print if necessary
    vector<CallInst *> InitCallsToYield;
    int64_t initNumCallsToYield;

    // Info on fiber routines found
    vector<string> RoutineNames;
};

struct CAT : public ModulePass
{
    static char ID;
    debugInfo *DI;

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
#if FALSE
        return false;
#endif
        /*
        - Function is not ready to be transformed at this point, metadata is not set
        - Other transformations from the clang command (most likely -fgnu89-inline or -O2) modifies
          the bitcode in a way that keeping a pointer to wrapper_nk_fiber_yield may not contain
          the same info seen in doInitialization to the info seen in runOnModule
        */

        // wrapper function, create function needs to be preserved --- set to NoInline
        for (auto CALL : NK_names)
        {
            auto func = M.getFunction(CALL);
            if (func != nullptr)
                func->addFnAttr(Attribute::NoInline);
        }

        return false;
    }

    bool runOnModule(Module &M) override
    {
#if FALSE
        return false;
#endif
        // Find fiber_functions again --- safe to transform here, granted functions not discarded
        for (auto i : NK_ids)
        {
            auto func = M.getFunction(NK_names[i]);
            if (func != NULL)
                FIBERS[i] = func;
            else
                return false;
        }

        // Get rid of LLVM debug intrinsics
        StripDebugInfo(M);
#if INJECT
        // Force inlining of nk_fiber_yield (should only occur in wrapper_nk_fiber_yield)
        inlineF(DI, *(FIBERS[INNER_YIELD]));

        // Force inlining of nk_fiber_start to generate direct calls to nk_fiber_create
        inlineF(DI, *(FIBERS[FIBER_START]));

#endif

#if DEBUG
        for (auto const &[id, func] : FIBERS)
        {
            if (!func)
                func->print(errs());
        }

        // To print later
        DI = new debugInfo();

        // Debugging info
        int64_t lines = 0;
        for (auto &F : M)
        {
            for (auto &B : F)
            {
                lines += B.size();

                for (auto &I : B)
                {
                    if (auto *call = dyn_cast<CallInst>(&I))
                    {
                        Function *callee = call->getCalledFunction();
                        if (callee == FIBERS[WRAPPER_YIELD])
                            DI->InitCallsToYield.push_back(call);
                    }
                }
            }
        }
        DI->totalLines = lines;
#endif

#if INJECT
        // IDENTIFY ROUTINES
        set<Function *> FiberRoutines = identifyRoutines(M, FIBERS[FIBER_CREATE]);
#endif

#if DEBUG
        for (auto routine : FiberRoutines)
            DI->RoutineNames.push_back(routine->getName());
#endif
        // INJECTION --- Determine where to inject calls, inject properly for each routine
        for (auto routine : FiberRoutines)
        {
#if DEBUG
            errs() << "\n\n\n\n\n\n\nCURR FUNCTION:\n " << routine->getName() << "\n";
#endif
            /*
             * SETUP --- Optimize loops for the injection pass. Unroll loops such that total loop
             * latency size matches a multiple of the granularity --- now injection in loop blocks 
             * are possible in intervals determined by the granularity
             */
            set<Instruction *> LoopGuards;

#if LOOP_OPT
            findAndSetLoops(*routine, LoopGuards);
#endif
            /*
            * GENERATE LATENCY CALCULATIONS --- a data flow analysis to compute expected 
            * and/or maximum latency for each bitcode instruction of the function (based on 
            * bitcode latency data). An adjusted control flow graph is needed to compute these
            * values --- back edges in functions are excluded (currently using FindFunctionBackedges
            * API instead of dominator tree)
            */

            // Isolate function back edges
            unordered_map<BasicBlock *, set<BasicBlock *>> BackEdges;
            organizeFunctionBackedges(*routine, BackEdges);

            // Latency calculations
            unordered_map<Instruction *, double> LatencyMeasurements;
            calculateLatencies(*routine, BackEdges, LatencyMeasurements);

            /*
            * FIND INJECTION LOCATIONS --- determine where to inject yield calls based on expected and/or 
            * maximum latency, given a granularity value X (macro for now). Specifics are found
            * in the description for findInjectionLocations
            */
            set<Instruction *> InjectionLocations;
            markInjectionLocationsFromLatencies(*routine, LatencyMeasurements, BackEdges, InjectionLocations);
            markGuardLocations(*routine, LoopGuards, InjectionLocations);

            /*
             * INJECT CALLS TO YIELD
             */
            injectYield(*routine, FIBERS[WRAPPER_YIELD], InjectionLocations);

#if DEBUG
            SmallVector<pair<const BasicBlock *, const BasicBlock *>, 32> Edges;
            FindFunctionBackedges(*routine, Edges);

            for (uint64_t i = 0, e = Edges.size(); i != e; ++i)
            {
                errs() << "Basic block that has a back-edge coming out of it\n";
                Edges[i].first->print(errs());

                errs() << "Basic Block that contains the back-edge\n";
                Edges[i].second->print(errs());
            }
            errs() << "GOT HERE6\n";

            errs() << "\n\n\n\n\nBackEdges\n";
            for (auto const &[BB, s] : BackEdges)
            {
                errs() << "\n\nCurr frontInst: ";
                BB->front().print(errs());
                errs() << "\n";

                errs() << "Now Back Edges: \n";
                for (auto b : s)
                {
                    b->front().print(errs());
                    errs() << "\n";
                }
            }

            errs() << "\n\n\nOnly Predecessors";
            for (auto &B : *routine)
            {
                errs() << "\n\nNew:\n";
                B.front().print(errs());
                errs() << "\nPredecessors:\n";
                for (auto predBB : predecessors(&B))
                {
                    predBB->front().print(errs());
                    errs() << "\n";
                }
            }

            errs() << "\n\n\nOnly Successors";
            for (auto &B : *routine)
            {
                errs() << "\n\nNew:\n";
                B.front().print(errs());
                errs() << "\nSuccessors:\n";
                for (auto succBB : successors(&B))
                {
                    succBB->front().print(errs());
                    errs() << "\n";
                }
            }

            errs() << "\n\n\ LATENCIES\n";
            for (auto &B : *routine)
            {
                for (auto &I : B)
                {
                    errs() << LatencyMeasurements[&I] << "   :    ";
                    I.print(errs());
                    errs() << "\n";
                }
            }

            errs() << "\n\n\n\n\n\n\nINJECTION LOCATIONS\n";
            for (auto inst : InjectionLocations)
            {
                errs() << "Curr Injection Location: ";
                inst->print(errs());
                errs() << "\n";
            }
#endif
        }

#if INLINE
        // INLINING --- inline the wrapper_nk_fiber_yield, inline all routines
        inlineF(DI, *(FIBERS[WRAPPER_YIELD]));
        for (auto routine : FiberRoutines)
            inlineF(DI, *routine);
#endif

            // Cleanup
#if DEBUG
        printDebugInfo(DI);
        free(DI);
        // M.print(errs(), nullptr);
#endif

        return false;
    }

    /*
     * identifyRoutines (REFACTORED)
     * 
     * Iterates over all call instructions. For every CallInst that calls nk_fiber_create, 
     * fetch the first argument of that call (which will be a fcn ptr). Return a set of those
     * pointers. 
     * 
     */

    set<Function *> identifyRoutines(Module const &M,
                                     Function const *parentFuncToFind)
    {
        set<Function *> Routines;

        // Iterate over uses of nk_fiber_create
        for (auto &use : parentFuncToFind->uses())
        {
            User *user = use.getUser();
            if (auto *call = dyn_cast<CallInst>(user))
            {
                // First arg of nk_fiber_create is a fcn ptr to the routine
                auto firstArg = call->getArgOperand(0);
                if (auto *routine = dyn_cast<Function>(firstArg))
                {
                    if (routine != FIBERS[IDLE_FIBER_ROUTINE])
                        Routines.insert(routine); // save the pointer
                }
            }
        }

        return Routines;
    }

    // ========================= LOOP SETTING =========================

    void findAndSetLoops(Function &F,
                         set<Instruction *> &LG)
    {
        auto &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
        auto &DT = getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
        auto &SE = getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();
        auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
        OptimizationRemarkEmitter ORE(&F);

        // Adjust loops for UnrollLoop API
        for (auto l : LI)
        {
            auto L = &*l;
            formLCSSARecursively(*L, DT, &LI, &SE);
            simplifyLoop(L, &DT, &LI, &SE, &AC, nullptr, true);
        }

#if LOOP_DEBUG
        errs() << "\n\n========================BEFORE TRANFORM - LOOPS FROM LI========================\n\n";
        errs() << F.getName() << "\n";
        for (auto L : LI)
        {
            errs() << "LI LOOP:\n";
            L->print(errs());
            for (auto B = L->block_begin(); B != L->block_end(); ++B)
                (*B)->print(errs());
            errs() << "\nNOW SUBLOOPS\n";
            vector<Loop *> SLs = L->getSubLoops();
            if (SLs.size() > 0)
            {
                for (auto SL : SLs)
                {
                    errs() << "SUBLOOP:\n";
                    SL->print(errs());
                    for (auto B = SL->block_begin(); B != SL->block_end(); ++B)
                        (*B)->print(errs());
                }
            }
            errs() << "\n\n\n";
        }
        //return;
#endif
        vector<Loop *> Loops;
        for (auto L : LI)
            Loops.push_back(L);

            /*
         * Depth first --- transform loops (and subloops) to a total latency size
         * that is a multiple of the granularity
         */
            // errs() << "\n\n=====NOW ENTERING TRANFORM =====\n\n";
#if LOOP_DEBUG
        errs() << "\n\n=====NOW ENTERING TRANFORM =====\n\n";
#endif
        for (auto L : Loops)
        {
#if LOOP_DEBUG
            errs() << "CURR LOOP TO PASS TO TRANSFORM LOOP\n";
            L->print(errs());
            for (auto B = L->block_begin(); B != L->block_end(); ++B)
                (*B)->print(errs());
            errs() << "HELLO THERE\n";
#endif
            transformLoop(F, L, LI, DT, SE, AC, ORE, LG);
        }

        return;
    }

    void transformLoop(Function &F, Loop *L,
                       LoopInfo &LI, DominatorTree &DT, ScalarEvolution &SE,
                       AssumptionCache &AC, OptimizationRemarkEmitter &ORE,
                       set<Instruction *> &LG)
    {
#if LOOP_DEBUG
        errs() << "\n\n\nIN TRANSFORM LOOP\n";
        errs() << "CURR LOOP TO TRANSFORM:\n";
        L->print(errs());
        for (auto B = L->block_begin(); B != L->block_end(); ++B)
            (*B)->print(errs());

        errs() << "CURR LOOP INDUCTION VARIABLE:\n";
        auto IV = L->getCanonicalInductionVariable();
        if (IV != nullptr)
        {
            IV->print(errs());
            errs() << "\n";
        }
        else
        {
            errs() << "NULLPTR IV ---\n";
        }
        //return;
#endif
        vector<Loop *> SLs = L->getSubLoops();
        if (SLs.size() > 0)
        {
            // errs() << "\nNOW SUBLOOPS TO TRANSFORM\n";
            for (auto SL : SLs)
            {
#if LOOP_DEBUG
                errs() << "SUBLOOP TO TRANSFORM:\n";
                SL->print(errs());
                for (auto B = SL->block_begin(); B != SL->block_end(); ++B)
                    (*B)->print(errs());
#endif
                transformLoop(F, SL, LI, DT, SE, AC, ORE, LG);
            }
        }
        //return;

        // errs() << "COUNT: " << count << "\n";
        // return;

        if (L->isLoopSimplifyForm() && L->isLCSSAForm(DT))
        {
            unsigned minTripMultiple = 1;
            auto tripCount = SE.getSmallConstantTripCount(L);
            auto tripMultiple = max(minTripMultiple, SE.getSmallConstantTripMultiple(L));

            auto unrollCount = tripCount;
            auto peelCount = 0;
            //auto unrollCount = getUnrollCount(calculatePrelimLoopLatencySize(F, LI, L));

            // For loops with an unknown trip count, set peelCount instead
            if (unrollCount > 100 || !unrollCount)
                unrollCount = getUnrollCount(calculatePrelimLoopLatencySize(F, LI, L));

            // Set options
            UnrollLoopOptions *ULO = new UnrollLoopOptions();
            ULO->Count = unrollCount;
            ULO->TripCount = tripCount;
            ULO->Force = true;
            ULO->AllowRuntime = true;
            ULO->AllowExpensiveTripCount = true;
            ULO->PreserveCondBr = true;
            ULO->PreserveOnlyFirst = false;
            ULO->TripMultiple = tripMultiple;
            ULO->PeelCount = peelCount;
            ULO->UnrollRemainder = false;
            ULO->ForgetAllSCEV = false;


            LoopUnrollResult unrolled = UnrollLoop(L, *ULO, &LI, &SE, &DT, &AC, &ORE, true);

            if (unrolled == LoopUnrollResult::FullyUnrolled || unrolled == LoopUnrollResult::PartiallyUnrolled)
                errs() << "\nUNROLLED SUCCESSFULLY\n";
            else
            {
#if LOOP_DEBUG

                errs() << "NOT UNROLLED\n";
                L->print(errs());
                errs() << "\n";
#endif
                LG.insert(getLastLoopBlock(L)->getTerminator());
            }

            delete ULO;
        }
        else
            LG.insert(getLastLoopBlock(L)->getTerminator());

        return;
    }

    double calculatePrelimLoopLatencySize(Function &F, LoopInfo &LI, Loop *L)
    {
        queue<BasicBlock *> workList;
        unordered_map<BasicBlock *, bool> visited;
        unordered_map<BasicBlock *, pair<double, double>> latencyMeasurements;

        unordered_map<BasicBlock *, set<BasicBlock *>> BackEdges;
        organizeFunctionBackedges(F, BackEdges);

        for (auto B = L->block_begin(); B != L->block_end(); ++B)
        {
            visited[*B] = false;
            workList.push(*B);
        }

        while (!workList.empty())
        {
            BasicBlock *currBlock = workList.front();
            workList.pop();

            vector<double> predBBLatencies;
            bool readyToCompute = true;

            for (auto predBB : predecessors(currBlock))
            {
                if (isBackEdge(predBB, currBlock, BackEdges) || !(L->contains(predBB)))
                    continue;

                if (!visited[predBB])
                {
                    readyToCompute = false;
                    break;
                }

                predBBLatencies.push_back(latencyMeasurements[predBB].second);
            }

            if (!readyToCompute)
            {
                workList.push(currBlock);
                continue;
            }

            double frontLatency = getLatency(&(currBlock->front()));
            if (predBBLatencies.size() > 0)
                frontLatency += configLatencyCalculation(predBBLatencies);

            double propagatingLatency = frontLatency;
            for (auto iter = ++currBlock->begin(); iter != currBlock->end(); ++iter)
                propagatingLatency += getLatency(&*iter);

            pair<double, double> frontAndTerminator;
            frontAndTerminator.first = frontLatency;
            frontAndTerminator.second = propagatingLatency;

            latencyMeasurements[currBlock] = frontAndTerminator;
            visited[currBlock] = true;
        }

        BasicBlock *loopFront = *(L->block_begin());
        BasicBlock *loopEnd = getLastLoopBlock(L);

        return (latencyMeasurements[loopEnd].second - latencyMeasurements[loopFront].first);
    }

    // ========================= PREPARE INJECTION LOCATIONS =========================

    /*
     * organizeFunctionBackedges --- (REFACTORED)
     * Tracks all back edges that exist in a routine in a bit vector 
     * structure. Applies the FindFunctionBackedges API --- and converts the structure of the  result
     * so that each source BasicBlock maps to a set of destination basic blocks based on FindFunctionBackedges. 
     * Function also fills out the LoopTerminators. Set to be used by findInjectionLocations.
     */

    void organizeFunctionBackedges(Function &F,
                                   unordered_map<BasicBlock *, set<BasicBlock *>> &BackEdges)
    {
        SmallVector<pair<const BasicBlock *, const BasicBlock *>, 32> Edges;
        FindFunctionBackedges(F, Edges);

        for (uint64_t iter = 0, e = Edges.size(); iter != e; ++iter)
        {
            BasicBlock *from = const_cast<BasicBlock *>(Edges[iter].first);
            BasicBlock *to = const_cast<BasicBlock *>(Edges[iter].second);
            BackEdges[from].insert(to);
        }

        return;
    }

    /*
     * calculateLatencies --- (REFACTORED --- NEEDS CODE CLEANUP (repitition with other workList funcs)) ---
     * Calculates expected and maximum latencies for all instructions in a routine. The mechanics
     * of the calculations are described below. calculateLatencies applies an altered worklist 
     * algorithm. All BasicBlocks are considered at initial execution of the algorithm. However, 
     * successors are not considered as there is no fixed point when calculating maximum and 
     * expected latencies (possible). Instead, the current block is considered IFF expected and/or 
     * maximum latencies exist for the previous block. If not, the block is pushed back to the worklist
     */

    void calculateLatencies(Function &F,
                            unordered_map<BasicBlock *, set<BasicBlock *>> BackEdges,
                            unordered_map<Instruction *, double> &LM)
    {
        queue<BasicBlock *> workList;
        unordered_map<BasicBlock *, bool> visited;

        for (auto &B : F)
        {
            workList.push(&B);
            visited[&B] = false;
        }

        while (!workList.empty())
        {
            BasicBlock *currBlock = workList.front();
            Instruction *frontInst = &(currBlock->front());
            workList.pop();

#if DEBUG
            errs() << "CurrBLock:\n";
            frontInst->print(errs());
            errs() << "\n";
#endif
            vector<double> predBBLatencies;
            bool readyToCompute = true;

            for (auto predBB : predecessors(currBlock))
            {
                if (isBackEdge(predBB, currBlock, BackEdges))
                    continue;

                if (!visited[predBB])
                {
                    readyToCompute = false;
                    break;
                }

                predBBLatencies.push_back(LM[predBB->getTerminator()]);
            }

            if (!readyToCompute)
            {
                workList.push(currBlock);
                continue;
            }

            /* 
            * 
            * **** READY TO COMPUTE ****
            * Calculate expected and max latencies
            * 
            * NOTE: Pseudo data flow analysis is as follows --- EXPECTED:
            * - GEN[I] = getLatency(I)
            * - IN[I] = expectedVal(OUT[all predInst to I])
            * - OUT[I] = IN[I] + GEN[I] --- OUT[I] is the EXPECTED LATENCY of I
            * 
            * NOTE: Pseudo data flow analysis is as follows --- MAXIMUM:
            * - GEN[I] = getLatency(I)
            * - IN[I] = max(OUT[all predInst to I])
            * - OUT[I] = IN[I] + GEN[I] --- OUT[I] is the MAXIMUM LATENCY of I
            * 
            * NOTE: When computing the data flow analysis --- IGNORE BACK EDGES FOR LOOPS
            * 
            */

            for (auto &I : *currBlock)
                LM[&I] = 0.0;

            /*
             * If there are predecessors to this basic block --- maximum and/or expected 
             * latencies of the first instruction of the current will depend on the maximum and 
             * expected latencies of the terminators of the predecessor basic blocks. If not, 
             * then maximum and expected latencies of the first instruction to the current 
             * block will be found using GEN[frontInst] 
             */

            // Calculate latency of first instruction to a basic block
            double frontLatency = getLatency(frontInst); // GEN[frontInst]

            if (predBBLatencies.size() > 0)
                frontLatency += configLatencyCalculation(predBBLatencies);

            // Propagate the maximum and expected latency of the first instruction through the current block
            LM[frontInst] = frontLatency;
            auto propagatingLatency = LM[frontInst];

            for (auto iter = (++currBlock->begin()); iter != currBlock->end(); ++iter)
            {
                auto I = &*iter;
                double currLatency = getLatency(I); // GEN[I]

                // OUT[I] = IN[I] + GEN[I]
                // set propagating vars to new OUT set
                LM[I] = propagatingLatency + currLatency;
                propagatingLatency = LM[I];
            }

            visited[currBlock] = true;
        }

        return;
    }

    /*
     * Finding Injection Locations ---
     * Computes the locations to inject a yield call based on the specific constraints:
     * - Uses a pseudo worklist algorithm to calculate based on data flow analysis similar to
     *   the analysis to calculate maximum and expected latencies (breadth first, mergers)
     * - Each call instruction is assumed to have high latency --- inject yield before and after
     *   call instruction (markGuardLocations)
     * - Identify first instruction in func that has an EXPECTED latency that exceeds X. Inject
     *   yield call before that instruction. "Reset" and continue. (markInjectionLocationsFromLatencies)
     * - OR, Identify first instruction in func that has a MAX latency that exceeds X. Inject
     *   yield call before that instruction. "Reset" and continue. (markInjectionLocationsFromLatencies)
     * - If a loop is identified, inject yield call at the end of the loop block (i.e. for each 
     *   iteration). No effort is done to idenfiy how many iterations will exceed X in expected or 
     *   maximum latency (future optimiztion) (markGuardLocations)
     */

    // ************ NEEDS MAJOR REFACTORING (WORKLIST + LARGE STACK) ************

    void markInjectionLocationsFromLatencies(Function &F,
                                             const unordered_map<Instruction *, double> &LM,
                                             const unordered_map<BasicBlock *, set<BasicBlock *>> BackEdges,
                                             set<Instruction *> &IL)
    {
        unordered_map<BasicBlock *, double> LastHits;
        unordered_map<BasicBlock *, bool> visited;

        queue<BasicBlock *> workList;

        for (auto &B : F)
        {
            workList.push(&B);
            visited[&B] = false;
        }

        while (!workList.empty())
        {
            BasicBlock *currBlock = workList.front();
            workList.pop();

            vector<double> predHits;
            bool readyToCompute = true;

            for (auto predBB : predecessors(currBlock))
            {
                if (isBackEdge(predBB, currBlock, BackEdges))
                    continue;

                if (!visited[predBB])
                {
                    readyToCompute = false;
                    break;
                }

                predHits.push_back(LastHits[predBB]);
            }

            if (!readyToCompute)
            {
                workList.push(currBlock);
                continue;
            }

            double updatedLastHit = 0.0;
            if (predHits.size() > 0)
                updatedLastHit = configConservativeness(predHits);

            for (auto &I : *currBlock)
            {
                if (isa<PHINode>(&I)) // Cannot inject in PHI Node block --- breaks LLVM invariant
                    continue;

                // Find injection locations based on expected latencies
                if ((LM.at(&I) - updatedLastHit) > GRAN)
                {
                    IL.insert(&I);
                    updatedLastHit = LM.at(&I);
                }
            }

            LastHits[currBlock] = updatedLastHit;
            visited[currBlock] = true;
        }

        return;
    }

    void markGuardLocations(Function &F,
                            const set<Instruction *> &LG,
                            set<Instruction *> &IL)
    {

#if CALL_GUARDS
        for (auto &B : F)
        {
            for (auto &I : B)
            {
                // Find injection locations based on call sites
                if (auto *call = dyn_cast<CallInst>(&I))
                {
                    /*
                     * Insert a "guard" before and after a call --- store the call 
                     * instruction (covers the 'before'), and the instruction after
                     * the call instruction (covers the 'after')
                     */

                    // Ignore LLVM internals/intrinsic calls
                    Function *callee = call->getCalledFunction();
                    if (callee->isIntrinsic())
                        continue;

                    IL.insert(&I);
                    IL.insert(I.getNextNode());
                }
            }
        }
#endif

#if LOOP_GUARDS
        for (auto loopGuard : LG)
            IL.insert(loopGuard);

        auto &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
        for (auto L : LI)
        {
            vector<Loop *> SLs = L->getSubLoops();
            for (auto SL : SLs)
                locationInLoop(SL, IL);

            locationInLoop(L, IL);
        }
#endif
        return;
    }

    void locationInLoop(Loop *L,
                        set<Instruction *> &IL)
    {
        bool loopYielded = false;
        for (auto I : IL)
        {
            if (L->contains(I))
            {
                loopYielded = true;
                break;
            }
        }

        if (!loopYielded)
            IL.insert(getLastLoopBlock(L)->getTerminator());

        return;
    }

    /*
     * injectYield
     * 
     * Iterates over all fiber routines (from identyRoutines) and marks every 10th bitcode 
     * instruction as an injection location. Iterate over all those locations to inject a 
     * call to wrapper_nk_fiber_yield
     * 
     */

    void injectYield(Function &F,
                     Function *funcToInsert,
                     set<Instruction *> const &IL)
    {
        // Build CallInst to yield, insert into routine

        for (auto i : IL)
        {
#if DEBUG
            errs() << "\n\nCurrent instruction from IL: ";
            i->print(errs());
            errs() << "\n";
#endif
            // Inject yield call
            IRBuilder<> builder{i};
            CallInst *yieldCall = builder.CreateCall(funcToInsert, None);

#if DEBUG
            errs() << "yieldCall CallInst: ";
            yieldCall->print(errs());
            errs() << "\n";

            DI->totalInjections++; // debugging info
#endif
        }

        return;
    }

    /*
     * inlineF
     * 
     * Iterate over all the uses of a function (passed as arg), and inline wherever possible
     * 
     */

    void inlineF(debugInfo *DI,
                 Function &F)
    {
        vector<CallInst *> CIToIterate;
        InlineFunctionInfo IFI;

#if DEBUG
        errs() << "Function: " << F.getName() << "\n";
        F.print(errs());
        errs() << "\n";
#endif

        for (auto &use : F.uses())
        {
            User *user = use.getUser();

#if DEBUG
            errs() << "Current User: ";
            user->print(errs());
            errs() << "\n";
#endif

            if (auto *call = dyn_cast<CallInst>(user))
            {
#if DEBUG
                errs() << "Call to push back: \n";
                call->print(errs());
#endif
                CIToIterate.push_back(call);
            }
        }

        for (auto CI : CIToIterate)
        {
#if DEBUG
            errs() << "Current CI: ";
            CI->print(errs());
            errs() << "\n";
#endif

            auto inlined = InlineFunction(CI, IFI);
            if (!inlined)
            {

#if DEBUG
                errs() << "Didn't inline --- failed at the following CI: \n";
                CI->print(errs());
                errs() << "\n";
                errs() << "INLINED failure message is: " << inlined.message << "\n";
                DI->failedInlines[CI] = inlined.message;
                // DI->numFailedInlines++;
#endif

                abort();
            }
#if DEBUG
            else
            {
                // DI->totalInlines++;
                errs() << "**** Successful Inline ****\n";
            }
#endif
        }
        return;
    }

    void printDebugInfo(debugInfo *DI)
    {
        errs() << "\n\n\nDEBUGGING INFO\n";
        errs() << "Total Lines: " << DI->totalLines << "\n";
        errs() << "Total Injections: " << DI->totalInjections << "\n";
        // for (auto IL : DI->InjectionLocations)
        // {
        //     errs() << "Injection Location: ";
        //     IL->print(errs());
        //     errs() << "\n";
        // }

        // errs() << "Total Inlines: " << DI->totalInlines << "\n";
        // errs() << "Total Failed Inlines: " << DI->numFailedInlines << "\n";

        // DI->initNumCallsToYield = DI->InitCallsToYield.size();
        // errs() << "Initial Number of Calls To Yield: " << DI->initNumCallsToYield << "\n";

        // if (DI->totalInlines == 0)
        // {
        //     errs() << "All initial calls to yield:\n";
        //     for (auto call : DI->InitCallsToYield)
        //     {
        //         errs() << "Call: ";
        //         call->print(errs());
        //         errs() << "\n";
        //     }
        // }

        errs() << "Routines found: \n";
        for (auto name : DI->RoutineNames)
            errs() << name << "\n";

        return;
    }

    // ===================================== HELPER FUNCTIONS =====================================
    double configConservativeness(vector<double> &PL)
    {
        switch (CONSERV)
        {
        case HIGHCON:
            return *min_element(PL.begin(), PL.end());
        case MEDCON:
            return (accumulate(PL.begin(), PL.end(), 0.0) / PL.size());
        case LOWCON:
            return *max_element(PL.begin(), PL.end());
        default:
            abort();
        }
    }

    double configLatencyCalculation(vector<double> &PL)
    {
        switch (LATCONFIG)
        {
        case EXPECTED:
            return (accumulate(PL.begin(), PL.end(), 0.0) / PL.size());
        case MAXIMUM:
            return *max_element(PL.begin(), PL.end());
        default:
            abort();
        }
    }

    double getLatency(Instruction *I) // CMU Data (~10 years old)
    {                                 // Instruction Latency for Core 2, 65nm
        int opcode = I->getOpcode();
        double cost;
        switch (opcode)
        {
        // Terminator instructions
        case Instruction::Ret:
            cost = 1;
            break;
        case Instruction::Br:
            cost = 0;
            break;
        case Instruction::Switch:
            cost = 0;
            break;
        // Standard binary operators
        case Instruction::Add:
            cost = 1;
            break;
        case Instruction::FAdd:
            cost = 4;
            break;
        case Instruction::Sub:
            cost = 1;
            break;
        case Instruction::FSub:
            cost = 4;
            break;
        case Instruction::Mul:
            cost = 3;
            break;
        case Instruction::FMul:
            cost = 4;
            break;
        case Instruction::UDiv:
            cost = 17;
            break;
        case Instruction::SDiv:
            cost = 17;
            break;
        case Instruction::FDiv:
            cost = 24;
            break;
        case Instruction::URem:
            cost = 17;
            break;
        case Instruction::SRem:
            cost = 17;
            break;
        case Instruction::FRem:
            cost = 24;
            break;
        // logical operators (integer operands)
        case Instruction::Shl:
            cost = 7;
            break;
        case Instruction::LShr:
            cost = 7;
            break;
        case Instruction::AShr:
            cost = 7;
            break;
        case Instruction::And:
            cost = 1;
            break;
        case Instruction::Or:
            cost = 1;
            break;
        case Instruction::Xor:
            cost = 1;
            break;
        // Vector ops
        case Instruction::ExtractElement:
            cost = 0;
            break; // TODO
        case Instruction::InsertElement:
            cost = 0;
            break; // TODO
        case Instruction::ShuffleVector:
            cost = 0;
            break; // TODO
        // Aggregate ops
        case Instruction::ExtractValue:
            cost = 0;
            break; // TODO
        case Instruction::InsertValue:
            cost = 0;
            break; // TODO
        // Memory ops
        case Instruction::Alloca:
            cost = 2;
            break;
        case Instruction::Load:
            cost = 2;
            break;
        case Instruction::Store:
            cost = 2;
            break;
        case Instruction::Fence:
            cost = 0;
            break; // TODO
        case Instruction::AtomicCmpXchg:
            cost = 0;
            break; // TODO
        case Instruction::AtomicRMW:
            cost = 0;
            break; // TODO
        case Instruction::GetElementPtr:
            cost = 1;
            break;
            // Cast operators
        case Instruction::PHI:
            cost = 0; // this is on purpose --- gets around the PHIGroup issue
            break;
        case Instruction::ICmp:
            cost = 6; // this is random
            break;
        case Instruction::Call:
        {
            if (auto *call = dyn_cast<CallInst>(I)) // don't want to involve LLVM internals
            {
                Function *callee = call->getCalledFunction();
                if (callee != nullptr)
                    cost = ((callee->isIntrinsic()) || (callee->getName().startswith("llvm.lifetime"))) ? 0 : 200;
                else
                    cost = 200; // Arbitrary
            }

            break;
        }
        default:
        {
#if DEBUG
            errs() << "DEFAULT\n";
            I->print(errs());
            errs() << "\n";
#endif
            cost = 8; // this is random
            break;
        }
        }

        return cost;
    }

    BasicBlock *getLastLoopBlock(Loop *L)
    {
        auto B = L->block_begin();
        BasicBlock *increment = *B;
        for (; B != L->block_end(); ++B)
            increment = *B;

        return increment;
    }

    bool isBackEdge(BasicBlock *from,
                    BasicBlock *to,
                    const unordered_map<BasicBlock *, set<BasicBlock *>> &BackEdges)
    {
        if (BackEdges.find(from) != BackEdges.end())
            if (BackEdges.at(from).find(to) != BackEdges.at(from).end())
                return true;

        return false;
    }

    uint64_t getUnrollCount(double num)
    {
        uint64_t roundedIntNum = roundToNearest((uint64_t)num);
        uint64_t divisor = (GRAN > roundedIntNum) ? roundedIntNum : GRAN;
        return (((roundedIntNum * GRAN) / __gcd(roundedIntNum, (uint64_t)GRAN)) / divisor);
    }

    uint64_t roundToNearest(uint64_t num)
    {
        static int nearest = 25;

        int moduloHundred = num % 100;
        unordered_map<int, int> nearestMap; // (difference from num, multiplier)
        vector<int> differences;

        for (int i = 0; i < (100 / nearest); i++)
        {
            int difference = abs((moduloHundred - (i * nearest)));
            nearestMap[difference] = i;
            differences.push_back(difference);
        }

        return ((num - moduloHundred) + (nearestMap[*min_element(differences.begin(), differences.end())] * nearest));
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override
    {
        AU.addRequired<LoopInfoWrapperPass>();
        AU.addRequired<AssumptionCacheTracker>();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<ScalarEvolutionWrapperPass>();
        return;
    }
}; // namespace
} // namespace

char CAT::ID = 0;
static RegisterPass<CAT> X("CAT", "Shell for code injection --- yields");

static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT());} });
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT()); } });
        
