#include "../include/LatencyDFA.hpp"

using namespace std::placeholders;

LatencyDFA::LatencyDFA(Loop *L, int32_t PropPolicy, int32_t ConservPolicy)
{
    if (L == nullptr)
        abort(); // Serious
 
    this->Blocks = vector<BasicBlock *>();
    if (!(_buildLoopBlocks(L)))
        abort(); // Serious

    // Set state
    this->L = L;
    this->LI = nullptr;
    this->F = Blocks.front()->getParent();
    this->EntryBlock = L->getHeader();
    this->PropagationPolicy = PropPolicy;
    this->ConservativenessPolicy = ConservPolicy;
    this->LoopInstructionCount = 0;
    this->LoopLatencySize = 0;

    this->GEN = unordered_map<Instruction *, double>();
    this->IN = unordered_map<Instruction *, double>();
    this->OUT = unordered_map<Instruction *, double>();
    _setupDFASets();

    this->IndividualLatencies = unordered_map<BasicBlock *, double>();
    this->AccumulatedLatencies = unordered_map<BasicBlock *, double>();
    this->LastCallbackLatencies = unordered_map<BasicBlock *, double>();
    this->PredLatencyGraph = unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *>();
    this->LastCallbackLatencyGraph = unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *>();
    this->CallbackLocations = set<Instruction *>();
    for (auto BB : Blocks)
    {
        PredLatencyGraph[BB] = new unordered_map<BasicBlock *, double>();
        LastCallbackLatencyGraph[BB] = new unordered_map<BasicBlock *, double>();
    }


    this->BackEdges = unordered_map<BasicBlock *, set<BasicBlock *>>();
    _organizeFunctionBackedges();
}

LatencyDFA::LatencyDFA(Function *F, LoopInfo *LI, int32_t PropPolicy, int32_t ConservPolicy)
{
    if (F == nullptr)
        abort(); // Serious
 
    // Set state
    this->F = F;
    this->L = nullptr;
    this->LI = LI;
    for (auto L : *LI)
        Loops.push_back(L);

    this->PropagationPolicy = PropPolicy;
    this->ConservativenessPolicy = ConservPolicy;
    this->LoopInstructionCount = 0;
    this->LoopLatencySize = 0;

    this->Blocks = vector<BasicBlock *>();
    this->EntryBlock = &(F->getEntryBlock());
    for (auto &B : *F)
        Blocks.push_back(&B);

    this->GEN = unordered_map<Instruction *, double>();
    this->IN = unordered_map<Instruction *, double>();
    this->OUT = unordered_map<Instruction *, double>();
    _setupDFASets();

    this->IndividualLatencies = unordered_map<BasicBlock *, double>();
    this->AccumulatedLatencies = unordered_map<BasicBlock *, double>();
    this->LastCallbackLatencies = unordered_map<BasicBlock *, double>();
    this->PredLatencyGraph = unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *>();
    this->LastCallbackLatencyGraph = unordered_map<BasicBlock *, unordered_map<BasicBlock *, double> *>();
    this->CallbackLocations = set<Instruction *>();
    for (auto BB : Blocks)
    {
        PredLatencyGraph[BB] = new unordered_map<BasicBlock *, double>();
        LastCallbackLatencyGraph[BB] = new unordered_map<BasicBlock *, double>();
    }

    this->BackEdges = unordered_map<BasicBlock *, set<BasicBlock *>>();
    _organizeFunctionBackedges();
}

/*
 * ComputeDFA()
 * 
 * Execute DFA --- DFA equations are discussed in internal methods,
 * IN and OUT sets are in separate pieces (individual latencies and
 * then scalar transformation)
 * 
 * Executes loop calculations if current LatencyDFA object is analyzing
 * a loop
 */ 

void LatencyDFA::ComputeDFA()
{
    // Execute DFA
    this->_buildGEN();
    this->_buildIndividualLatencies();
    this->_buildINAndOUT();

    // Determine LLS if we have a loop
    if (L != nullptr)
    {
        BasicBlock *Latch = L->getLoopLatch();
        if (Latch != nullptr)
            this->LoopLatencySize = AccumulatedLatencies[Latch];
        else
        {
            BasicBlock *Last = L->getBlocksVector().back();
            this->LoopLatencySize = AccumulatedLatencies[Last];
        }

        LOOP_DEBUG_INFO("\nLoopLatencySize: " + 
                        to_string(this->LoopLatencySize) + "\n");
    }

    return;
}

/*
 * GetAccumulatedLatency(Instruction *)
 * 
 * Accumulated latency for an instruction is the OUT set from
 * the DFA computation --- check the OUT table, return the value
 */ 

double LatencyDFA::GetAccumulatedLatency(Instruction *I)
{
    if (OUT.find(I) == OUT.end())
        return 0;

    return OUT[I];
}

/*
 * GetAccumulatedLatency(BasicBlock *)
 * 
 * Accumulated latency for a basic block is the accumulated latency
 * of the terminator instruction of the block (computed from the DFA)
 * --- calculated and saved in the AccumulatedLatencies table
 */ 

double LatencyDFA::GetAccumulatedLatency(BasicBlock *BB)
{
    if (AccumulatedLatencies.find(BB) == AccumulatedLatencies.find(BB))
        return 0;

    return AccumulatedLatencies[BB];
}

/*
 * GetIndividualLatency(Instruction *)
 * 
 * Individual latency for an instruction is the GEN set from
 * the DFA computation --- check the GEN table, return the value
 */ 

double LatencyDFA::GetIndividualLatency(Instruction *I)
{
    if (GEN.find(I) == GEN.end())
        return 0;

    return GEN[I];
}

/*
 * GetIndividualLatency(BasicBlock *)
 * 
 * Individual latency for a basic block is the accumulated latency
 * of the terminator instruction of the block from the DFA computation
 * WITHOUT the scalar transformation --- i.e. accumulated latency of 
 * the entry point instruction is 0, and propagation occurs based on
 * this value (i.e. calculated assuming block is in isolation)
 */ 

double LatencyDFA::GetIndividualLatency(BasicBlock *BB)
{
    if (IndividualLatencies.find(BB) == IndividualLatencies.find(BB))
        return 0;

    return IndividualLatencies[BB];
}

template<typename ChecksFunc, typename SetupFunc, typename WorkerFunc>
void LatencyDFA::WorkListDriver(ChecksFunc &CF, SetupFunc &SF, WorkerFunc &WF)
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
            if (CF(this, CurrBlock, PredBB))
                continue;

            if (!(Visited[PredBB]))
                ReadyToCompute &= false;

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

        SF(this, CurrBlock, ValidPreds);
        WF(this, CurrBlock);
    }
}

// ----------------------------------------------------------------------------------

// DFA

void LatencyDFA::_setupDFASets()
{
    for (auto BB : Blocks)
    {
        for (auto &I : *BB)
        {
            GEN[&I] = 0.0;
            IN[&I] = 0.0;
            OUT[&I] = 0.0;
        }   
    }

    return;
}  

/*
 * _buildGEN
 * 
 * GEN[I] = getInstLatency(I)
 */ 

void LatencyDFA::_buildGEN()
{
    for (auto BB : Blocks)
    {
        for (auto &I : *BB)
            GEN[&I] = getInstLatency(&I);
    }

    return;
}  


/*
 * _buildIndividualLatencies
 * 
 * 
 * OUT[I] = IN[I] + GEN[I]
 * IN[I] = Policy_{P \in Preds} OUT[P]
 * 
 * Here, "Policy" refers to the propagation policy selected
 * for the DFA --- such as expected or maximum latencies
 * 
 * However, this is context-insensitive, so we must start out 
 * with IN[EntryPoint] = 0
 * 
 * So the DFA equations at a per-basic block level:
 * IN[EntryPoint] = 0
 * OUT[I] = IN[I] + GEN[I]
 * IN[I] = OUT[I]
 * 
 * The extra complexity of separating the intra basic block 
 * propgation for IN and OUT sets into this method is for function 
 * analysis --- we need context-insensitive accumulated latencies (i.e.
 * the individual latencies map) at the per-basic block level in 
 * order to implement heuristics to avoid reinjection in loops, 
 * unreachable blocks, etc.
 */ 

void LatencyDFA::_buildIndividualLatencies()
{
    for (auto BB : Blocks)
    {
        Instruction *EntryPoint = &(BB->front());

        // Compute DFA equations for IN and OUT sets for EntryPoint
        // with latency starting at 0
        IN[EntryPoint] = 0;
        OUT[EntryPoint] = IN[EntryPoint] + GEN[EntryPoint];

        double PropagatingLatency = OUT[EntryPoint];
        for (auto IIT = (++BB->begin()); IIT != BB->end(); ++IIT)
        {
            Instruction *Next = &*IIT;
            
            // Compute DFA equations for IN and OUT
            IN[Next] = PropagatingLatency;
            OUT[Next] = IN[Next] + GEN[Next];

            // Set PropagatingLatency for next instruction in the loop
            PropagatingLatency = OUT[Next];
        }

        IndividualLatencies[BB] = PropagatingLatency;
    }

    return;
}

/*
 * _buildINAndOUT
 * 
 * Original data-flow equations:
 * OUT[I] = IN[I] + GEN[I]
 * IN[I] = Policy_{P \in Preds} OUT[P]
 * 
 * Modified to incorporate IndividualLatencies map:
 * IN[EntryPoint] = Policy_{P \in Preds} OUT[P]
 * OUT[I] = OUT[I] + IN[EntryPoint]
 * IN[I] = IN[I] + IN[EntryPoint]
 * 
 * This is a scalar transformation from the IndivdiualLatencies
 * map because IN[EntryPoint] = 0 in a context-insensitive
 * environment, so the difference in a context-sensntive 
 * environment is Policy_{P \in Preds} OUT[P] - 0 = 
 * Policy_{P \in Preds} OUT[P]. Propagating this value to all
 * instructions in a basic block is therefore also a scalar 
 * transformation 
 */ 

void LatencyDFA::_buildINAndOUT()
{
    auto Checks = bind(mem_fn(&LatencyDFA::_DFAChecks), _1, _2, _3);
    auto Setup = bind(mem_fn(&LatencyDFA::_buildPredLatencyGraph), _1, _2, _3);
    auto Worker = bind(mem_fn(&LatencyDFA::_modifyINAndOUT), _1, _2);

    WorkListDriver(Checks, Setup, Worker);

    return;
}

bool LatencyDFA::_DFAChecks(BasicBlock *CurrBlock, BasicBlock *PredBB)
{
    if (_isBackEdge(CurrBlock, PredBB))
        return true;

    // Loop specific
    if ((L != nullptr)
        && (!(L->contains(PredBB))))
        return true;

    return false;
}

void LatencyDFA::_buildPredLatencyGraph(BasicBlock *BB, vector<BasicBlock *> &Preds)
{
    auto PLGMap = PredLatencyGraph[BB];
    for (auto PredBB : Preds)
        (*PLGMap)[PredBB] = AccumulatedLatencies[PredBB];

    return;
}     

void LatencyDFA::_modifyINAndOUT(BasicBlock *BB)
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

// ---

set<Instruction *> *LatencyDFA::BuildIntervalsFromZero()
{
    if (L != nullptr)
    {
        auto Checks = bind(mem_fn(&LatencyDFA::_loopBlockChecks), _1, _2, _3);
        auto Setup = bind(mem_fn(&LatencyDFA::_buildLastCallbacksGraph), _1, _2, _3);
        auto Worker = bind(mem_fn(&LatencyDFA::_markCallbackLocations), _1, _2);

        WorkListDriver(Checks, Setup, Worker);
    }
    else    
    {
        auto Checks = bind(mem_fn(&LatencyDFA::_functionBlockChecks), _1, _2, _3);
        auto Setup = bind(mem_fn(&LatencyDFA::_buildLastCallbacksGraph), _1, _2, _3);
        auto Worker = bind(mem_fn(&LatencyDFA::_markCallbackLocations), _1, _2);

        WorkListDriver(Checks, Setup, Worker);
    }

    return &CallbackLocations;
}

bool LatencyDFA::_loopBlockChecks(BasicBlock *CurrBlock, BasicBlock *PredBB)
{
    if ((_isBackEdge(CurrBlock, PredBB))
        || (!(L->contains(PredBB)))
        || (_isContainedInSubLoop(PredBB)))
        return true;

    return false;
}   

bool LatencyDFA::_functionBlockChecks(BasicBlock *CurrBlock, BasicBlock *PredBB)
{
    if ((_isBackEdge(CurrBlock, PredBB))
        || (_isContainedInLoop(PredBB)))
        return true;

    return false;
}   

void LatencyDFA::_buildLastCallbacksGraph(BasicBlock *BB, vector<BasicBlock *> &Preds)
{
    auto LCLMap = LastCallbackLatencyGraph[BB];
    for (auto PredBB : Preds)
        (*LCLMap)[PredBB] = LastCallbackLatencies[PredBB];

    return;
}   

void LatencyDFA::_markCallbackLocations(BasicBlock *BB)
{
    Instruction *EntryPoint = &(BB->front());
    double IncomingLastCallback = _calculateEntryPointLastCallback(BB);

    for (auto &I : *BB)
    {
        if (isa<PHINode>(&I))
            continue;
            
        if ((OUT[&I] - IncomingLastCallback) > GRAN)
        {
            CallbackLocations.insert(&I);
            IncomingLastCallback = OUT[&I];
        }
    }

    LastCallbackLatencies[BB] = IncomingLastCallback;

    return;
}
// ----------------------------------------------------------------------------------

// Policy (or policy based methods)

/*
 * _calculateEntryPointLatency
 * 
 * For IN and OUT set computation in the data flow analysis --- we want to determine
 * the entry point instruction's IN set to reflect all predecessor's OUT sets (accumulated
 * latencies)
 * 
 * If a block has no real predecessors --- no predecessors, for loops --- no predecessors that
 * are backedges, not in the loop, etc. --- start accumulated latency at 0
 * 
 * If there are multiple predecessors --- the DFA must follow a policy selected and passed
 * to the LatencyDFA object --- the policy will determine the IN set of the entry point
 * instruction. Propagation to the rest of the instructions in the basic block will occur
 * in other internal methods (_buidlIndividualLatencies, _modifyINAndOUT)
 */ 

double LatencyDFA::_calculateEntryPointLatency(BasicBlock *CurrBlock)
{
    auto PredLatencyNode = PredLatencyGraph[CurrBlock];

    vector<double> PredLatencies;
    for (auto const &[EdgeBlock, EdgeLatency] : (*PredLatencyNode))
        PredLatencies.push_back(EdgeLatency);

    if (!(PredLatencies.size() > 0))
        return 0.0;

    return _configLatencyCalculation(PredLatencies);
}

/*
 * _calculateEntryPointLastCallback
 * 
 * For interval/callback marking --- we want to determine the accumulated latency
 * of the last instruction marked for callback injection --- we look at the node
 * in the LastCallbackLatencyGraph, which provides the accumulated latencies of 
 * the last instruction marked for callback injection from all predecessors
 * 
 * LastCallbackLatencyGraph is deliberately not complete --- for example, if we 
 * are conducting function level analysis, we do not want to mark more instructions
 * inside a function's outermost loops again (that analysis must be complete prior
 * at the loop level). We want to skip those blocks --- and the graph reflects 
 * this idea
 * 
 * As a result, !(LastCallbackLatencies.size() > 0) is possible if there are no 
 * predecessors for a block, no real predecessors to consider at the loop level
 * (i.e. backedges, not contained in the loop body, contained in a subloop), or
 * no real predecessors to consider at the function level (i.e. contained in an
 * outermost loop). At this point, we want to reset the last callback latency to
 * 0, and we return as such for this case. 
 * 
 * NOTE --- the analysis to factor out blocks that we do NOT want to consider is 
 * conducted in _loopBlockChecks or _functionBlockChecks, as a part of the invocation
 * to the WorkListDriver
 * 
 * If there are multiple predecessors, we want to follow the policy to determine
 * what the last callback latency should be for block passed. This is determined
 * by the conservativeness policy passed to the LatencyDFA object, and calculated
 * in the _configConservativenessCalculation method
 */

double LatencyDFA::_calculateEntryPointLastCallback(BasicBlock *CurrBlock)
{
    auto LastCallbackNode = LastCallbackLatencyGraph[CurrBlock];

    vector<double> LastCallbackLatencies;
    for (auto const &[EdgeBlock, EdgeLatency] : (*LastCallbackNode))
        LastCallbackLatencies.push_back(EdgeLatency);

    if (!(LastCallbackLatencies.size() > 0))
        return 0.0;

    return _configConservativenessCalculation(LastCallbackLatencies);
}

/*
 * _configLatencyCalculation
 * 
 * Method to determine a latency calculation based on a vector of predecessor
 * accumulated latencies --- policies inlcude EXPECTED (mean) and MAXIMUM
 * (max element) --- different policies enforce different levels of strictness
 * when it comes to estimating accumulated latencies 
 */ 

double LatencyDFA::_configLatencyCalculation(vector<double> &PL)
{
    switch (PropagationPolicy)
    {
    case EXPECTED:
        return (accumulate(PL.begin(), PL.end(), 0.0) / PL.size());
    case MAXIMUM:
        return *max_element(PL.begin(), PL.end());
    default:
        abort();
    }
}

/*
 * _configConservativenessCalculation
 * 
 * Method to determine how conservatively a call instruction to a 
 * callback function should be placed --- policies include HIGHCON (min
 * element), MEDCON (mean), LOWCON (max element) --- enforces different
 * levels of conservativeness with respect to meeting the deadline (the
 * granularity specified)
 */ 

double LatencyDFA::_configConservativenessCalculation(vector<double> &PL)
{
    switch (ConservativenessPolicy)
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

// ----------------------------------------------------------------------------------

// Utility

/*
 * _buildLoopBlocks()
 * 
 * Adds all basic blocks of the loop to the blocks vector, returns
 * the instruction count of the loop body 
 */ 

uint64_t LatencyDFA::_buildLoopBlocks(Loop *L)
{
    // Gets instruction count for loop, also builds
    // Blocks vector

    uint64_t size = 0;
    for (auto B = L->block_begin(); B != L->block_end(); ++B)
    {
        BasicBlock *CurrBB = *B;

        // Build blocks vector
        Blocks.push_back(CurrBB);

        // Calculate number of instructions
        size += distance(CurrBB->instructionsWithoutDebug().begin(),
                         CurrBB->instructionsWithoutDebug().end());
    }

    this->LoopInstructionCount = size;

    return size;
}

/*
 * _organizeFunctionBackedges()
 * 
 * Wrapper for FindFunctionBackedges API --- transfer from LLVM
 * ADTs to C++ STL containers --- build two way map for backedges
 * so querying is easier
 */ 

void LatencyDFA::_organizeFunctionBackedges()
{
    SmallVector<pair<const BasicBlock *, const BasicBlock *>, 32> Edges;
    FindFunctionBackedges(*F, Edges);

    for (int32_t i = 0, e = Edges.size(); i != e; ++i)
    {
        BasicBlock *From = const_cast<BasicBlock *>(Edges[i].first);
        BasicBlock *To = const_cast<BasicBlock *>(Edges[i].second);
        BackEdges[From].insert(To);
        BackEdges[To].insert(From); // Suspicious
    }

    return;
}

/*
 * _isBackEdge()
 * 
 * Determines if a pair of basic blocks are a backedge according
 * to the backedges map generated by _organizeFunctionBackedges
 */ 

bool LatencyDFA::_isBackEdge(BasicBlock *From, BasicBlock *To)
{
    if (BackEdges.find(From) != BackEdges.end())
    {
        // Possibly redundant
        if (BackEdges[From].find(To) != BackEdges[From].end())
            return true;
    }

    return false;
}

/*
 * _isContainedInSubLoop() --- FIX (not general)
 * 
 * Determines if a basic block is part of a subloop --- this is
 * an important analysis piece for LatencyDFAs on loops, if we are
 * analyzing a function, return false --- we only analyze at the 
 * outermost loop level for function analysis
 */ 

bool LatencyDFA::_isContainedInSubLoop(BasicBlock *BB)
{
    // If function level analysis
    if (L == nullptr)
        return false;

    // Analyze all subloops
    bool Contained = false;
    for (auto SL : L->getSubLoops())
    {
        if (SL->contains(BB))
            Contained |= true;
    }

    return Contained;
}

/*
 * _isContainedInLoop()
 * 
 * Determines if a basic block is part of a loop --- if the 
 * LatencyDFA is on a loop --- check the current object's 
 * loop to compare, if the LatencyDFA is on a function, check
 * the entire loops vector instead (NOTE --- no subloops 
 * analyzed for function-level analysis)
 */ 

bool LatencyDFA::_isContainedInLoop(BasicBlock *BB)
{
    bool Contained = false;

    if (L != nullptr) // Loop level analysis
    { 
        if (L->contains(BB))
            Contained = true;
    }
    else // Function level analysis
    {
        for (auto Loop : Loops)
        {
            if (Loop->contains(BB))
                Contained |= true;
        }
    }

    return Contained;
}

// ----------------------------------------------------------------------------------

// Debugging

void LatencyDFA::PrintBBLatencies()
{
    DEBUG_INFO("\n\n----------\nPrintBBAccumulatedLatencies: " + 
               F->getName() + "\n\n");

    for (auto BB : Blocks)
    {
        DEBUG_INFO("CurrBB: \n");
        OBJ_INFO(BB);
        DEBUG_INFO("Individual Latency: " + 
                   to_string(IndividualLatencies[BB]) + "\n");
        DEBUG_INFO("Accumulated Latency: " + 
                   to_string(AccumulatedLatencies[BB]) + "\n---\n");
    }

    DEBUG_INFO("----------\n");

    return;
}

void LatencyDFA::PrintInstLatencies()
{
    DEBUG_INFO("\n\n----------\nPrintInstAccumulatedLatencies: " + 
               F->getName() + "\n\n");

    for (auto BB : Blocks)
    {
        for (auto &I : *BB)
        {
            DEBUG_INFO("CurrI: ");
            OBJ_INFO((&I));
            DEBUG_INFO("Individual Latency: " + 
                       to_string(GEN[&I]) + "\n");
            DEBUG_INFO("Accumulated Latency: " + 
                       to_string(OUT[&I]) + "\n\n");
        }

    }

    DEBUG_INFO("----------\n");

    return;
}