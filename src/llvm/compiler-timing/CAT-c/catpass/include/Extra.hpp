void LatencyDFA::_buildINAndOUT()
{
    _workListDriver(_buildPredLatencyGraph, _modifyINAndOut);
    return;
}  

template<SetupFunc, WorkerFunc>
void LatencyDFA::_workListDriver(SetupFunc SF, WorkerFunc WF)
{
    queue<BasicBlock *> WorkList;
    unordered_map<BasicBlock *, bool> Visited;

    for (auto BB : Blocks)
    {
        Visited[BB] = false;
        WorkList.push(BB);
    }

    while (!(WorkList.empty()))
    {
        BasicBlock *CurrBlock = WorkList.front();
        WorkList.pop();

        bool ReadyToCompute = true;
        vector<BasicBlock *> ValidPreds; // Utilized by almost all users of this
                                         // driver method --- if not, SetupFunc will
                                         // just be void

        for (auto PredBB : predecessors(CurrBlock))
        {
            if (_isBackEdge(CurrBlock, PredBB))
                continue;

            // Loop specific
            if ((L != nullptr)
                && (!(L->contains(PredBB))))
                continue;

            if (!(Visited[PredBB]))
            {
                ReadyToCompute &= false;
                continue;
            }

            ValidPreds.push_back(PredBB);
        }

        // If not ready to compute, then continue --- the
        // successors of the block will be added to queue
        // when we come back to the current block later
        if (!ReadyToCompute)
        {
            WorkList.push(CurrBlock);
            continue;
        }

        Visited[CurrBlock] = true;

        SetupFunc(CurrBlock, ValidPreds);
        WorkerFunc(CurrBlock);
    }
}

void _buildLastCallbacksGraph(BasicBlock *BB, vector<BasicBlock *> &Preds)
{
    auto LCLMap = LastCallbackLatencyGraph[BB];
    for (auto PredBB : Preds)
        (*LCLLMap)[PredBB] = LastCallbackLatencies[PredBB];

    return;
}   

void _buildPredLatencyGraph(BasicBlock *BB, vector<BasicBlock *> &Preds)
{
    auto PLGMap = PredLatencyGraph[BB];
    for (auto PredBB : Preds)
        (*PLGMap)[PredBB] = AccumulatedLatencies[PredBB];

    return;
}     

void _markCallbackLocations(BasicBlock *BB)
{
    Instruction *EntryPoint = &(BB->front());
    double IncomingLastCallback = _calculateEntryPointLastCallback(BB);

    for (auto &I : *BB)
    {
        if ((OUT[&I] - IncomingLastCallback) > GRAN)
        {
            CallbackLocations.insert(&I);
            IncomingLastCallback = OUT[&I];
        }
    }

    LastCallbackLatencies[BB] = IncomingLastCallback;

    return;
}

void _modifyINAndOUT(BasicBlock *BB)
{
    // Ready to compute --- find the REAL IN set of the 
    // first instruction of the block
    double EntryPointLatency = _calculateEntryPointLatency(BB);

    for (auto &I : *BB)
    {
        // Compute modified DFA equations for IN and OUT
        IN[&I] += EntryPointLatency;
        OUT[&I] += EntryPointLatency;
    }

    // Set the accumulated latency for CurrBlock ---  will be 
    // the OUT set of the terminator of the block
    AccumulatedLatencies[BB] = OUT[BB->getTerminator()];

    return;
}


void LatencyDFA::_buildINAndOUT()
{
    // Setup ---  add AllBlocks to the queue, guarantees all blocks
    // will be examine and handled (we push back to the queue)
    // when we're not ready to compute yet
    queue<BasicBlock *> WorkList;
    unordered_map<BasicBlock *, bool> Visited;
    for (auto BB : Blocks)
    {
        Visited[BB] = false;
        WorkList.push(BB);
    }

    while (!(WorkList.empty()))
    {
        BasicBlock *CurrBlock = WorkList.front();
        WorkList.pop();

        /*
         *  Determine if latencies are ready to compute
         *  --- this is only possible if all predecessor
         *  blocks are visited (and therefore latencies
         *  are already calculated), excluding non-loop
         *  blocks and backedges
         * 
         *  If a predecessor block checks out, start 
         *  building the pred latency graph "node"
         *  for the current block
         */ 
        bool ReadyToCompute = true;
        auto PredBlocksToConsider = new unordered_map<BasicBlock *, double>();
        for (auto PredBB : predecessors(CurrBlock))
        {
            if (_isBackEdge(CurrBlock, PredBB))
                continue;

            // Loop specific
            if ((L != nullptr)
                && (!(L->contains(PredBB))))
                continue;

            if (!(Visited[PredBB]))
                ReadyToCompute &= false;
            
            (*PredBlocksToConsider)[PredBB] = AccumulatedLatencies[PredBB];
        }

        // If not ready to compute, then continue --- the
        // successors of the block will be added to queue
        // when we come back to the current block later
        if (!ReadyToCompute)
        {
            WorkList.push(CurrBlock);
            delete PredBlocksToConsider;
            continue;
        }

        // Set state for CurrBlock
        PredLatencyGraph[CurrBlock] = PredBlocksToConsider;
        Visited[CurrBlock] = true;

        // Ready to compute --- find the REAL IN set of the 
        // first instruction of the block
        Instruction *EntryPoint = &(CurrBlock->front());
        double EntryPointLatency = _calculateEntryPointLatency(CurrBlock);

        for (auto &I : *CurrBlock)
        {
            // Compute modified DFA equations for IN and OUT
            IN[&I] += EntryPointLatency;
            OUT[&I] += EntryPointLatency;
        }

        // Set the accumulated latency for CurrBlock ---  will be 
        // the OUT set of the terminator of the block
        AccumulatedLatencies[CurrBlock] = OUT[CurrBlock->getTerminator()];
    }

    return;
}  

           // Transform loops --- each outermost loop in the function will
            // each handle its own subloops

            // for (auto L : Loops)
            // {
            //     LoopTransform *LT = new LoopTransform(L, &F, FunctionLDFA, &LI, 
            //                                           &DT, &SE, &AC, &ORE, GRAN);
            //     LT->Transform();
            // }

            // // Recompute the function level DFA --- pass this to the function
            // // level transform object --- apply injections to callbacks and 
            // // final DFA/loop based adjustments

            // // DFA
            // FunctionLDFA = new LatencyDFA(&F, EXPECTED);
            // FunctionLDFA->ComputeDFA();

            // // Transfrom
            // FunctionTransform *FT = new FunctionTransform(&F);




























#include "include/LoopTransform.hpp"

namespace
{
struct CAT : public ModulePass
{
    static char ID;
    Function *CB = nullptr;

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
        auto Callback = M.getFunction("callback");
        if (Callback == nullptr)
            abort();

        CB = Callback;

        return false;
    }

    bool runOnModule(Module &M) override
    {
#if FALSE
        return false;
#endif

        // Per function analysis and transformation
        for (auto &F : M)
        {
            set<Instruction *> InjectionLocations;

            if ((&F == nullptr) 
                || (F.isIntrinsic())
                || (!(F.getInstructionCount() > 0))
                || (&F == CB))
                continue;

            // // Wrapper pass information, loop transformation setup
            // auto &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
            // auto &DT = getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
            // auto &SE = getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();
            // auto &AA = getAnalysis<AAResultsWrapperPass>(F).getAAResults();
            // auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
            // OptimizationRemarkEmitter ORE(&F);
            // DemandedBits DB(F, AC, DT);
            
            // // Per loop
            // PredicatedScalarEvolution PSE(SE, *L);
            // LoopVectorizationRequirements LVR(ORE);
            // LoopVectorizeHints LVH(L, /* InterleaveOnlyWhenForced */ false, ORE);
            // std::function<const LoopAccessInfo &(Loop &)> GetLAA =
            //     [&](Loop &L) -> const LoopAccessInfo & { return LAA->getInfo(&L); };


            auto *SE = &getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();
            auto *LI = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
            auto *TTI = &getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
            auto *DT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
            auto *TLI = &getAnalysis<TargetLibraryInfoWrapperPass>(F).getTLI();
            auto *AA = &getAnalysis<AAResultsWrapperPass>(F).getAAResults();
            auto *AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
            // auto *LAA = &getAnalysis<LoopAccessLegacyAnalysis>(F);
            // auto *DB = &getAnalysis<DemandedBitsWrapperPass>(F).getDemandedBits();
            DemandedBits DB(F, *AC, *DT);
            OptimizationRemarkEmitter ORE(&F);
        
            std::function<const LoopAccessInfo &(Loop &)> GetLAA =
                [&](Loop &L) -> const LoopAccessInfo & { 
                    return LoopAccessInfo(&L, SE, TLI, AA, DT, LI);
                };


            vector<Loop *> Loops;
            for (auto L : *LI)
                Loops.push_back(L);

            for (auto L : Loops)
            {
                PredicatedScalarEvolution PSE(*SE, *L);
                LoopVectorizationRequirements LVR(*ORE);
                LoopVectorizeHints LVH(L, /* InterleaveOnlyWhenForced */ false, *ORE);
                LoopVectorizationLegality LVL(L, PSE, DT, TTI, TLI, AA, &F, &GetLAA, 
                                              LI, ORE, &LVR, &LVH, &DB, AC);

                errs() << LVL.canVectorize(false) << "\n";
            }

            // DEBUG_INFO("\n\n\n\n----------- NOW LOOP -----------\n\n\n");

            // for (auto L : Loops)
            // {
            //     auto LT = new LoopTransform(L, &F, &LI, &DT, &SE, &AC, &ORE, GRAN);
            //     LT->Transform();

            //     for (auto I : *(LT->GetCallbackLocations()))
            //         InjectionLocations.insert(I);
            // }

            // DEBUG_INFO("\n\n\n\n----------- NOW FUNCTION -----------\n\n\n");

            // // Function level DFA --- acquire the accumulated latency
            // // calculations and pass them on to the loop transformer
            // LatencyDFA *FunctionLDFA = new LatencyDFA(&F, &LI, EXPECTED, MEDCON);
            // FunctionLDFA->ComputeDFA();
            // auto FCallbackLocations = FunctionLDFA->BuildIntervalsFromZero();

            // for (auto I : *FCallbackLocations)
            //     InjectionLocations.insert(I);

            // for (auto I : InjectionLocations)
            // {
            //     IRBuilder<> builder{I};
            //     CallInst *Injection = builder.CreateCall(CB, None);
            // }
        }

        return false;
    }

// INITIALIZE_PASS_BEGIN(YourPass, "CAT", "Latency analysis pass", /*cfgonly=*/false, /*analysis=*/false)
// INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
// INITIALIZE_PASS_END(YourPass, "CAT", "Latency analysis pass", /*cfgonly=*/false, /*analysis=*/false)

    void getAnalysisUsage(AnalysisUsage &AU) const override
    {
        // AU.addRequired<OptimizationRemarkEmitterWrapperPass>();
        AU.addRequired<LoopInfoWrapperPass>();
        AU.addRequired<AssumptionCacheTracker>();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<ScalarEvolutionWrapperPass>();
        // AU.addRequired<TargetTransformInfoWrapperPass>();




        // AU.addRequired<AAResultsWrapperPass>();
        // AU.addRequired<AssumptionCacheTracker>();
        // AU.addRequired<DominatorTreeWrapperPass>();
        // AU.addRequired<LoopInfoWrapperPass>();
        // AU.addRequired<ScalarEvolutionWrapperPass>();
        AU.addRequired<TargetTransformInfoWrapperPass>();
        AU.addRequired<TargetLibraryInfoWrapperPass>();
        AU.addRequired<AAResultsWrapperPass>();
        // AU.addRequired<LoopAccessLegacyAnalysis>();
        // AU.addRequired<DemandedBitsWrapperPass>();
        AU.addPreserved<BasicAAWrapperPass>();
        AU.addPreserved<GlobalsAAWrapperPass>();

        return;
    }
};

} // namespace

char CAT::ID = 0;

static RegisterPass<CAT> X("CAT", "Latency analysis pass");

static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT());} });
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT()); } });


static const char la_name[] = "Latency Analysis";

INITIALIZE_PASS_BEGIN(_RegPass1, "CAT", la_name, false, false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(BasicAAWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(GlobalsAAWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopAccessLegacyAnalysis)
INITIALIZE_PASS_DEPENDENCY(DemandedBitsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(OptimizationRemarkEmitterWrapperPass)
INITIALIZE_PASS_END(_RegPass1, "CAT", la_name, false, false)


























// void LatencyDFA::_buildINAndOUT()
// {
//     // Setup ---  add AllBlocks to the queue, guarantees all blocks
//     // will be examine and handled (we push back to the queue)
//     // when we're not ready to compute yet
//     queue<BasicBlock *> WorkList;
//     unordered_map<BasicBlock *, bool> Visited;
//     for (auto BB : Blocks)
//     {
//         Visited[BB] = false;
//         WorkList.push(BB);
//     }

//     while (!(WorkList.empty()))
//     {
//         BasicBlock *CurrBlock = WorkList.front();
//         WorkList.pop();

//         /*
//          *  Determine if latencies are ready to compute
//          *  --- this is only possible if all predecessor
//          *  blocks are visited (and therefore latencies
//          *  are already calculated), excluding non-loop
//          *  blocks and backedges
//          * 
//          *  If a predecessor block checks out, start 
//          *  building the pred latency graph "node"
//          *  for the current block
//          */ 
//         bool ReadyToCompute = true;
//         auto PredBlocksToConsider = new unordered_map<BasicBlock *, double>();
//         for (auto PredBB : predecessors(CurrBlock))
//         {
//             if (_isBackEdge(CurrBlock, PredBB))
//                 continue;

//             // Loop specific
//             if ((L != nullptr)
//                 && (!(L->contains(PredBB))))
//                 continue;

//             if (!(Visited[PredBB]))
//                 ReadyToCompute &= false;
            
//             (*PredBlocksToConsider)[PredBB] = AccumulatedLatencies[PredBB];
//         }

//         // If not ready to compute, then continue --- the
//         // successors of the block will be added to queue
//         // when we come back to the current block later
//         if (!ReadyToCompute)
//         {
//             WorkList.push(CurrBlock);
//             delete PredBlocksToConsider;
//             continue;
//         }

//         // Set state for CurrBlock
//         PredLatencyGraph[CurrBlock] = PredBlocksToConsider;
//         Visited[CurrBlock] = true;

//         // Ready to compute --- find the REAL IN set of the 
//         // first instruction of the block
//         Instruction *EntryPoint = &(CurrBlock->front());
//         double EntryPointLatency = _calculateEntryPointLatency(CurrBlock);

//         for (auto &I : *CurrBlock)
//         {
//             // Compute modified DFA equations for IN and OUT
//             IN[&I] += EntryPointLatency;
//             OUT[&I] += EntryPointLatency;
//         }

//         // Set the accumulated latency for CurrBlock ---  will be 
//         // the OUT set of the terminator of the block
//         AccumulatedLatencies[CurrBlock] = OUT[CurrBlock->getTerminator()];
//     }

//     return;
// }  