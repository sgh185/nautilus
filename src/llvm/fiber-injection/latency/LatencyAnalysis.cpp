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

            // Wrapper pass information, loop transformation setup
            auto *SE = &getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();
            auto *LI = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
            auto *DT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
            auto *AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
            OptimizationRemarkEmitter ORE(&F);

            DEBUG_INFO("\n\n\n\n----------- NOW LOOP -----------\n\n\n");

            vector<Loop *> Loops;
            for (auto L : *LI)
                Loops.push_back(L);

            for (auto L : Loops)
            {
                auto LT = new LoopTransform(L, &F, LI, DT, SE, AC, &ORE, GRAN);
                LT->Transform();

                for (auto I : *(LT->GetCallbackLocations()))
                    InjectionLocations.insert(I);
            }

            DEBUG_INFO("\n\n\n\n----------- NOW FUNCTION -----------\n\n\n");

            // Function level DFA --- acquire the accumulated latency
            // calculations and pass them on to the loop transformer
            // LatencyDFA *FunctionLDFA = new LatencyDFA(&F, LI, EXPECTED, MEDCON);
            // FunctionLDFA->ComputeDFA();
            // auto FCallbackLocations = FunctionLDFA->BuildIntervalsFromZero();

            // for (auto I : *FCallbackLocations)
            //     InjectionLocations.insert(I);

            for (auto I : InjectionLocations)
            {
                IRBuilder<> builder{I};
                CallInst *Injection = builder.CreateCall(CB, None);
            }
        }

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