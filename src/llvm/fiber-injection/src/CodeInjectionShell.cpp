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
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Souradip Ghosh <sgh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
 * Transformation that injects calls to a wrapper to a pseudo interrupt handler ("nk_time_hook_fire")
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
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Attributes.h"
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
#define WHOLE 1 // Whole kernel injection
#define INJECT 1
#define FALSE 0

// Special Routines --- necessary for functionality
#define HOOK_FIRE 0
#define FIBER_START 1
#define FIBER_CREATE 2
#define IDLE_FIBER_ROUTINE 3

// Guards for yield call injections
#define GRAN 2000
#define CALL_GUARDS 0
#define LOOP_GUARDS 1
#define LOOP_OPT 1
#define LOOP_BRANCH 1

// Conservativeness, Latency path configurations
#define MAXIMUM 0
#define EXPECTED 1

#define HIGHCON 0
#define MEDCON 1
#define LOWCON 2

#define CONSERV HIGHCON
#define LATCONFIG EXPECTED

// Functions necessary to find in order to inject (hook_fire, fiber_start/create for fiber explicit injections)
const vector<uint32_t> NK_ids = {HOOK_FIRE, FIBER_START, FIBER_CREATE, IDLE_FIBER_ROUTINE};
const vector<string> NK_names = {"nk_time_hook_fire", "nk_fiber_start", "nk_fiber_create", "__nk_fiber_idle"};
const string ANNOTATION = "llvm.global.annotations";
const string NOHOOK = "nohook";
unordered_map<uint32_t, Function *> SpecialRoutines;
vector<string> NoHookFunctionNames;
vector<Function *> NoHookFunctions;

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
    GlobalVariable *GV = nullptr; // custom attribute

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
#if FALSE
        return false;
#endif
        /*
        - Function is not ready to be transformed at this point, metadata is not set
        - Other transformations may inline special functions or "nohook" attributed
          functions --- force noinline to preserve function bodies
        */

        // necessary functions (NK_names) will be set to noinline automatically
        for (auto CALL : NK_names)
        {
            auto func = M.getFunction(CALL);
            if (func != nullptr)
                func->addFnAttr(Attribute::NoInline);
        }

        // gather all functions with "nohook" attributes --- add noinline attributes
        // to those functions as well
        GV = M.getGlobalVariable(ANNOTATION);
        if (GV != nullptr)
        {
            vector<Function *> AnnotatedFunctions;
			gatherAnnotatedFunctions(AnnotatedFunctions);
			for (auto AF : AnnotatedFunctions)
            {
				if (AF != nullptr)
                    AF->addFnAttr(Attribute::NoInline);

                NoHookFunctionNames.push_back(AF->getName());
            }
        }

        return false;
    }

    bool runOnModule(Module &M) override
    {
#if FALSE
        return false;
#endif
        // Find special functions again --- safe to transform here, granted functions not discarded
        // --- if any functions are discarded, get out
        for (auto i : NK_ids)
        {
            auto func = M.getFunction(NK_names[i]);
            if (func != nullptr)
                SpecialRoutines[i] = func;
            else
                abort();
        }

        // Populate NoHookFunctions --- prevent injection in these functions later; gathering
        // these functions here because doInitialization may have altered/inlined some of the
        // functions in optimization process
        for (auto NHFN : NoHookFunctionNames)
        {
            auto func = M.getFunction(NHFN);
            if (func != nullptr)
                NoHookFunctions.push_back(func);
        }

#if INJECT
        // Force inlining of nk_fiber_start to generate direct calls to nk_fiber_create
        inlineF(DI, *(SpecialRoutines[FIBER_START]));
#endif

#if DEBUG
        for (auto const &[id, func] : SpecialRoutines)
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
                        if (callee == SpecialRoutines[HOOK_FIRE])
                            DI->InitCallsToYield.push_back(call);
                    }
                }
            }
        }
        DI->totalLines = lines;
#endif

#if INJECT
        // Identify routines --- specific to fibers
        set<Function *> Routines = identifyRoutines(M, SpecialRoutines[FIBER_CREATE]);

#if WHOLE
        // Identify and add to routines --- for whole kernel injection
        for (auto &F : M)
        {
            // Current hack below to prevent injection in certain functions
            // This should be replaced with a custom pragma or function attribute
            // in the future
            if (&F == nullptr)
                continue;
            else if (F.isIntrinsic())
                continue;
            else if (&F == SpecialRoutines[HOOK_FIRE] || (find(NoHookFunctions.begin(), NoHookFunctions.end(), &F) != NoHookFunctions.end()))
                continue;
#if DEBUG
            errs() << F.getName() << ": inst count --- " << F.getInstructionCount() << " and return: ";
            F.getReturnType()->print(errs());
            errs() << "\n";
#endif
            // Don't handle empty functions
            if (F.getInstructionCount() > 0)
                Routines.insert(&F);
        }
#endif

#endif

#if DEBUG
        for (auto routine : Routines)
            DI->RoutineNames.push_back(routine->getName());
#endif
        // INJECTION --- Determine where to inject calls, inject properly for each routine
        // NOTE --- these are still called Routines
        for (auto routine : Routines)
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
            injectYield(*routine, SpecialRoutines[HOOK_FIRE], InjectionLocations);

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
        inlineF(DI, *(SpecialRoutines[HOOK_FIRE]));
        for (auto routine : Routines)
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

    void gatherAnnotatedFunctions(vector<Function *> &AnnotatedFunctions)
    {
        // First operand is the global annotations array --- get and parse
        // NOTE --- the fields have to be accessed through VALUE->getOperand(0),
        // which appears to be a layer of indirection for these values
        auto *AnnotatedArr = cast<ConstantArray>(GV->getOperand(0));
        for (auto OP = AnnotatedArr->operands().begin(); OP != AnnotatedArr->operands().end(); OP++)
        {
            // Each element in the annotations array is a ConstantStruct --- its
            // fields can be accessed through the first operand (indirection). There are two
            // fields --- Function *, GlobalVariable * (function ptr, annotation)

            auto *AnnotatedStruct = cast<ConstantStruct>(OP);
            auto *FunctionAsStructOp = AnnotatedStruct->getOperand(0)->getOperand(0);         // first field
            auto *GlobalAnnotationAsStructOp = AnnotatedStruct->getOperand(1)->getOperand(0); // second field

            // Set the function and global, respectively. Both have to exist to
            // be considered.
            Function *AnnotatedF = dyn_cast<Function>(FunctionAsStructOp);
            GlobalVariable *AnnotatedGV = dyn_cast<GlobalVariable>(GlobalAnnotationAsStructOp);

            if (AnnotatedF == nullptr || AnnotatedGV == nullptr)
                continue;

            // Check the annotation --- if it matches the NOHOOK global in the
            // pass --- push back to apply (X) transform as necessary later
            ConstantDataArray *ConstStrArr = dyn_cast<ConstantDataArray>(AnnotatedGV->getOperand(0));
            if (ConstStrArr == nullptr)
                continue;

            if (ConstStrArr->getAsCString() != NOHOOK)
                continue;

            AnnotatedFunctions.push_back(AnnotatedF);
        }

#if DEBUG
        for (auto AF : AnnotatedFunctions)
        {
            errs() << AF->getName() << "\n";
        }
#endif

        return;
    }

    /*
     * identifyRoutines (REFACTORED) --- specific to fibers implementation
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
                    if (routine != SpecialRoutines[IDLE_FIBER_ROUTINE])
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
            errs() << "NULLPTR IV ---\n\n";
        }
        //return;
#endif
        vector<Loop *> SLs = L->getSubLoops();
        if (SLs.size() > 0)
        {
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

        if (L->isLoopSimplifyForm() && L->isLCSSAForm(DT))
        {
            auto unrollCount = 0;
            auto peelCount = 0;

            unsigned minTripMultiple = 1;
            auto tripCount = SE.getSmallConstantTripCount(L);
            auto tripMultiple = max(minTripMultiple, SE.getSmallConstantTripMultiple(L));
            auto totalInstructionCount = getTotalLoopInstructionCount(L); // Total number of instructions in loop

#if LOOP_DEBUG
            errs() << "\n\nNOW NUMBERS:\n";
            errs() << "     TripCount: " << tripCount << "\n";
            errs() << "     TripMultiple: " << tripMultiple << "\n";
            errs() << "     TotalInstructionCount: " << totalInstructionCount << "\n";
#endif
            // For loops with an known tripCount --- unroll fully
            // Otherwise --- unroll based on loop latency size estimation if within
            // "reasonable" range --- else, generate branching. Note that the
            // magic numbers (32, 256) are currently arbitrary

            if (tripCount > 32 || !tripCount)
            {
                auto LLS = calculatePrelimLoopLatencySize(F, LI, L);
                unrollCount = getUnrollCount(calculatePrelimLoopLatencySize(F, LI, L));

#if LOOP_DEBUG
                errs() << "IN BRANCH --- tripCount > 32 || !tripCount\n";
                errs() << "     LoopLatencySize: " << LLS << "\n";
                errs() << "     unrollCount via GetUnrollCount: " << unrollCount << "\n";
#endif

#if LOOP_BRANCH
                auto totalUnroll = unrollCount * totalInstructionCount;
#if LOOP_DEBUG
                errs() << "             totalUnroll (unrollCount * totalInstructionCount): " << totalUnroll << "\n";
#endif
                if (totalUnroll > 256)
                {
                    Instruction *BranchTerminator = generateLoopIterationBranch(L, unrollCount, &LI, &DT);
                    if (BranchTerminator == nullptr)
                        return;

                    LG.insert(BranchTerminator);
#if LOOP_DEBUG
                    BranchTerminator->print(errs());
                    errs() << "\nGENERATED BRANCH in " << F.getName() << "\n";
#endif
                    return;
                }
#endif
            }
            else
                unrollCount = tripCount;

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

            if (!(unrolled == LoopUnrollResult::FullyUnrolled || unrolled == LoopUnrollResult::PartiallyUnrolled))
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

#if LOOP_DEBUG
            errs() << "Current Block: \n";
            currBlock->print(errs());
#endif

            vector<double> predBBLatencies;
            bool readyToCompute = true;

#if LOOP_DEBUG
            errs() << "All terminator latencies of predecessors of currBlock in loop:\n";
#endif

            for (auto predBB : predecessors(currBlock))
            {
                if (isBackEdge(predBB, currBlock, BackEdges) || !(L->contains(predBB)))
                    continue;

                if (!visited[predBB])
                {
                    readyToCompute = false;
                    break;
                }

#if LOOP_DEBUG
                errs() << "Pred:\n";
                predBB->print(errs());
                errs() << "Terminator latency: " << latencyMeasurements[predBB].second << "\n\n";
#endif
                predBBLatencies.push_back(latencyMeasurements[predBB].second);
            }

            if (!readyToCompute)
            {
                workList.push(currBlock);
                continue;
            }

#if LOOP_DEBUG
            errs() << "Back to currBlock:\n";
            if (predBBLatencies.size() > 0)
                errs() << "configLatencyCalculation result of predBBLatencies: " << configLatencyCalculation(predBBLatencies) << "\n";
#endif
            double frontLatency = getLatency(&(currBlock->front()));

#if LOOP_DEBUG
            errs() << "I: ";
            (&(currBlock->front()))->print(errs());
            errs() << "\nSingular Latency: " << frontLatency << "\n";
#endif
            if (predBBLatencies.size() > 0)
                frontLatency += configLatencyCalculation(predBBLatencies);

#if LOOP_DEBUG
            errs() << "Accumulated Latency: " << frontLatency << "\n\n";
#endif
            double propagatingLatency = frontLatency;
            for (auto iter = ++currBlock->begin(); iter != currBlock->end(); ++iter)
            {
#if LOOP_DEBUG
                errs() << "I: ";
                (&*iter)->print(errs());
                errs() << "\nSingular Latency: " << getLatency(&*iter) << "\n";
#endif
                propagatingLatency += getLatency(&*iter);
#if LOOP_DEBUG
                errs() << "Accumulated Latency: " << propagatingLatency << "\n\n";
#endif
            }

            errs() << "\n\n\n";

            pair<double, double> frontAndTerminator;
            frontAndTerminator.first = frontLatency;
            frontAndTerminator.second = propagatingLatency;

            latencyMeasurements[currBlock] = frontAndTerminator;
            visited[currBlock] = true;
        }

#if LOOP_DEBUG
        errs() << "BB latency sizes (format: front, last):\n";
        for (auto const &[BB, lat] : latencyMeasurements)
        {
            errs() << "BB: \n";
            BB->print(errs());
            errs() << "LATENCY MEASUREMENTS: " << lat.first << ", " << lat.second << "\n";
        }
        errs() << "\n\n";
#endif

        BasicBlock *loopFront = *(L->block_begin());
        BasicBlock *loopEnd = getLastLoopBlock(L);

        return (latencyMeasurements[loopEnd].second - latencyMeasurements[loopFront].first);
    }

    // Loop transformation policy --- insert "biased" branch
    Instruction *generateLoopIterationBranch(Loop *L, uint64_t UnrollCount, LoopInfo *LI, DominatorTree *DT)
    {
        BasicBlock *EntryBlock = L->getHeader();
        Instruction *LastLoopInst = getLastLoopBlock(L)->getTerminator();
        if (EntryBlock == nullptr || LastLoopInst == nullptr)
            return nullptr;

        Instruction *InsertionPoint = EntryBlock->getFirstNonPHI();
        Function *Parent = EntryBlock->getParent();
        if (Parent == nullptr || InsertionPoint == nullptr)
            return nullptr;

        // Set up primary builder for and constants
        IRBuilder<> topBuilder{InsertionPoint};
        Type *int_ty = Type::getInt32Ty(Parent->getContext());

        Value *PHIInitValue = ConstantInt::get(int_ty, 0);
        Value *ResetValue = ConstantInt::get(int_ty, UnrollCount);
        Value *Increment = ConstantInt::get(int_ty, 1);

        // Generate new instructions for new phi node and new block functionality inside entry block
        PHINode *TopPHI = topBuilder.CreatePHI(int_ty, 0);
        Value *Iterator = topBuilder.CreateAdd(TopPHI, Increment);
        Value *CmpInst = topBuilder.CreateICmp(ICmpInst::ICMP_EQ, Iterator, ResetValue);

        // Generate new block (for loop for injection of nk_time_hook_fire)
        Instruction *NewBlockTerminator = SplitBlockAndInsertIfThen(CmpInst, InsertionPoint, false, nullptr, DT, LI, nullptr);
        BasicBlock *NewBlock = NewBlockTerminator->getParent();
        if (NewBlockTerminator == nullptr)
            return nullptr;

        // Get unique predecessor and successor for new block
        BasicBlock *NewSingleSucc = NewBlock->getUniqueSuccessor();
        BasicBlock *NewSinglePred = NewBlock->getUniquePredecessor();
        if (NewSinglePred == nullptr || NewSingleSucc == nullptr)
            return nullptr;

        // Generate new builder on insertion point in the successor of the new block and
        // generate second level phi node
        Instruction *SecondaryInsertionPoint = NewSingleSucc->getFirstNonPHI();
        if (SecondaryInsertionPoint == nullptr)
            return nullptr;

        IRBuilder<> secondaryBuilder{NewBlockTerminator};
        PHINode *SecondaryPHI = topBuilder.CreatePHI(int_ty, 0);

        // Populate selection points for second level phi node
        SecondaryPHI->addIncoming(Iterator, NewSinglePred);
        SecondaryPHI->addIncoming(PHIInitValue, NewBlock);

        // Populate selection points for top level phi node
        for (auto predBB : predecessors(EntryBlock))
        {
            // Handle potential backedges
            if (L->contains(predBB))
                TopPHI->addIncoming(SecondaryPHI, predBB);
            else
                TopPHI->addIncoming(PHIInitValue, predBB);
        }

        return NewBlockTerminator;
    }

    // Helper for branch insertion, other methods
    uint64_t getTotalLoopInstructionCount(Loop *L)
    {
        uint64_t size = 0;
        for (auto B = L->block_begin(); B != L->block_end(); ++B)
        {
            BasicBlock *BB = *B;
            size += distance(BB->instructionsWithoutDebug().begin(),
                             BB->instructionsWithoutDebug().end());
        }

        return size;
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

        vector<Loop *> Loops;
        for (auto L : LI)
            Loops.push_back(L);

        for (auto L : Loops)
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
        // Call instructions need to be injected with relatively
        // "correct" debug locations --- Clang 9 will complain otherwise
        Instruction *FirstInstWithDBG = nullptr;
        for (auto &I : instructions(F))
        {
            if (I.getDebugLoc())
            {
                FirstInstWithDBG = &I;
                break;
            }
        }

        // Build CallInst to yield, insert into routine
        for (auto i : IL)
        {
#if DEBUG
            errs() << "\n\nCurrent instruction from IL: ";
            i->print(errs());
            errs() << "\n";
#endif
            // Inject yield call with correct debug locations
            IRBuilder<> builder{i};
            if (FirstInstWithDBG != nullptr)
                builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());

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
	
	double getInlineAsmLatency(string IAString)
	{
		double cost = 10; // default latency
		if (IAString.substr(0, 3) == "mov") // Terrible
			cost = 1; // Fog
		else if (IAString == "pause")
			cost = 25; // Fog

		return cost;
	}

	double getLatency(Instruction *I) 
	{
		int opcode = I->getOpcode();
		double cost = 0;

		switch (opcode)
		{
		// Terminator instructions
		case Instruction::Call:
		{
			if (auto *Call = dyn_cast<CallInst>(I)) // don't want to involve LLVM internals
			{
				// Handle InlineAsm (in the worst way possible)
				if (auto *IA = dyn_cast<InlineAsm>(Call->getCalledValue()))
					cost = getInlineAsmLatency(IA->getAsmString());

				// Handle real callees
				Function *Callee = Call->getCalledFunction();

				// Possible intrinsic, assembly stub, etc.
				if ((Callee == nullptr)
					|| (Callee->isIntrinsic()) // llvm.stacksave() ??? 
					|| (Callee->getName().startswith("llvm.lifetime"))) // Unclear
					break;
				else cost = 3; // Fog --- Knights Landing, generic Intel
			}

			break;
		}
		case Instruction::Ret:
			cost = 2; // Fog
			break;
		case Instruction::Br:
		case Instruction::IndirectBr: // NEW
		case Instruction::Unreachable: // NEW
		case Instruction::Switch: // FIX
			break;

		// Standard binary operators --- NOTE --- Fog numbers 
		// for Knights landing much higher
		case Instruction::Add:
		case Instruction::Sub:
			cost = 1;
			break;
		case Instruction::Mul:
			cost = 3;
			break;
		case Instruction::FAdd:
		case Instruction::FSub:
		case Instruction::FMul:
			cost = 4;
			break;
		case Instruction::UDiv:
		case Instruction::SDiv:
		case Instruction::URem:
		case Instruction::SRem:
			cost = 17;
			break;
		case Instruction::FDiv:
		case Instruction::FRem:
			cost = 24;
			break;
			
		// Logical operators (integer operands)
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			cost = 7;
			break;
		case Instruction::And:
		case Instruction::Or:
		case Instruction::Xor:
			cost = 1;
			break;

		// Vector ops
		case Instruction::ExtractElement:
		case Instruction::InsertElement:
		case Instruction::ShuffleVector:
		case Instruction::ExtractValue:
		case Instruction::InsertValue:
			break; 

		// Memory ops
		case Instruction::Alloca: // Prev = 2
			break;
		case Instruction::Load:
		case Instruction::Store:
			cost = 4; // L1
			break;
		
		// Atomics related
		// case Instruction::Fence:
		case Instruction::AtomicCmpXchg:
			cost = 24; // Fog
			break;
		case Instruction::AtomicRMW:
			cost = 11; // Fog
			break;

		// Control flow 
		case Instruction::ICmp:
			cost = 1; // Fog
			break;
		case Instruction::FCmp:
			cost = 7; // Fog
			break; 

		// Cast operators --- NEW
		case Instruction::IntToPtr:
		case Instruction::PtrToInt:
		case Instruction::BitCast: // Prev = 1
			break;
		case Instruction::ZExt:
		case Instruction::SExt:
		case Instruction::Trunc:
		case Instruction::FPExt:
		case Instruction::FPTrunc:
		case Instruction::AddrSpaceCast:
			cost = 1;
			break; 
		case Instruction::FPToUI:
		case Instruction::FPToSI:
		case Instruction::SIToFP:
		case Instruction::UIToFP:
			cost = 6; // Fog
			break;

		// LLVM IR specifics
		case Instruction::PHI:
		case Instruction::VAArg:
		case Instruction::GetElementPtr: // Unclear
			break;
		case Instruction::Select: // NEW
			cost = 10; // Unclear
			break;
		
		default:
			cost = 4; // Unclear
			break;
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

    // This is insanely bad --- still has undefined behavior probably
    uint64_t getUnrollCount(double num)
    {
        if (num == 0.0)
            return 0;

        uint64_t roundedIntNum = max((int)roundToNearest((uint64_t)num), 1);
        uint64_t divisor = (GRAN > roundedIntNum) ? roundedIntNum : GRAN;
        uint64_t gcd = __gcd(roundedIntNum, (uint64_t)GRAN);
        return (((roundedIntNum * GRAN) / __gcd(roundedIntNum, (uint64_t)GRAN)) / divisor);
    }

    // Absolutely the worst --- will kick soon
    uint64_t roundToNearest(uint64_t num)
    {
		static int nearest = 10;

        int moduloHundred = num % 100;
        unordered_map<int, int> nearestMap; // (difference from num, multiplier)
        vector<int> differences;

        for (int i = 1; i < (100 / nearest); i++)
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
static RegisterPass<CAT> X("CAT", "Shell for code injection --- nk_time_hook_fire");

static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT());} });
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT()); } });

