#include "LatencyTable.hpp"

using namespace std;
using namespace llvm;

class LatencyDFA
{
public:
    LatencyDFA(Function *F, LoopInfo *LI, int32_t PropPolicy, int32_t ConservPolicy);
    LatencyDFA(Loop *L, int32_t PropPolicy, int32_t ConservPolicy);

    // ------- Analysis ------- 
    void ComputeDFA();
    double GetLoopLatencySize() { return this->LoopLatencySize; }
    double GetLoopInstructionCount() { return this->LoopInstructionCount; }
    double GetAccumulatedLatency(Instruction *I);
    double GetAccumulatedLatency(BasicBlock *BB);
    double GetIndividualLatency(Instruction *I);
    double GetIndividualLatency(BasicBlock *BB);
    set<Instruction *> *BuildIntervalsFromZero();

    template<typename ChecksFunc, typename SetupFunc, typename WorkerFunc>
    void WorkListDriver(ChecksFunc &CF, SetupFunc &SF, WorkerFunc &WF);

    // ------- Debugging -------
    void PrintBBLatencies();
    void PrintInstLatencies();

    // Public for simplicity
    vector<BasicBlock *> Blocks;

private:
    Function *F;
    Loop *L;
    LoopInfo *LI;
    
    // ------- Analysis settings, statistics -------
    int32_t PropagationPolicy;
    int32_t ConservativenessPolicy;
    uint64_t LoopInstructionCount;
    uint64_t LoopLatencySize;
    BasicBlock *EntryBlock;

    // ------- Analysis data structures ------- 
    // NOTE --- BitSets not necessary here, as:
    // a) these are simple DFA calculations with no complex set operations,
    // b) IDs is too complicated for an indirection here, not necessary where
    //    a direct instruction to latency mapping is necessary
    unordered_map<BasicBlock *, set<BasicBlock *>> BackEdges; // SOURCE to set of DESTs
    vector<Loop *> Loops; // Function analysis
    unordered_map<Instruction *, double> GEN;
    unordered_map<Instruction *, double> IN;
    unordered_map<Instruction *, double> OUT; // NOTE --- these are the actual per-inst accumulated latencies
    unordered_map<BasicBlock *, double> IndividualLatencies;
    unordered_map<BasicBlock *, double> AccumulatedLatencies;
    unordered_map<BasicBlock *, double> LastCallbackLatencies;
    unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *> PredLatencyGraph;
    unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *> LastCallbackLatencyGraph;
    set<Instruction *> CallbackLocations;

    // ------- DFA -------

    // GEN set
    void _buildGEN();

    // IN and OUT set
    void _buildIndividualLatencies();
    void _buildINAndOUT(); // Wrapper for IN/OUT set computation
    bool _DFAChecks(BasicBlock *CurrBlock, BasicBlock *PredBB); // Checks for WorkListDriver
    void _buildPredLatencyGraph(BasicBlock *BB, vector<BasicBlock *> &Preds); // Setup for WorkListDriver
    void _modifyINAndOUT(BasicBlock *BB); // Computation for IN/OUT sets

    // ------- Interval/Callbacks Calculation -------
    bool _loopBlockChecks(BasicBlock *CurrBlock, BasicBlock *PredBB); // Loop level analysis, checks
    bool _functionBlockChecks(BasicBlock *CurrBlock, BasicBlock *PredBB); // Function level analysis, checks
    void _buildLastCallbacksGraph(BasicBlock *BB, vector<BasicBlock *> &Preds); // Setup for WorkListDriver
    void _markCallbackLocations(BasicBlock *BB); // Determine callback injection locations

    // ------- Policies -------
    double _configLatencyCalculation(vector<double> &PL); // Enforcing policy
    double _configConservativenessCalculation(vector<double> &PL); // Enforcing policy
    double _calculateEntryPointLatency(BasicBlock *CurrBlock); // Application of policy
    double _calculateEntryPointLastCallback(BasicBlock *CurrBlock); // Application of policy


    // ------- Utility -------
    void _organizeFunctionBackedges();
    void _setupDFASets();
    bool _isBackEdge(BasicBlock *From, BasicBlock *To);
    uint64_t _buildLoopBlocks(Loop *L);
    bool _isContainedInSubLoop(BasicBlock *BB);
    bool _isContainedInLoop(BasicBlock *BB);
};