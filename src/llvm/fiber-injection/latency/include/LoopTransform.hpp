#include "LatencyDFA.hpp"

using namespace std;
using namespace llvm;

const double ExpansionFactor = 2.4; // Weight to account for LLSs that are small due to 
                                    // possible vectorization in the middle-end or clever 
                                    // instruction selection at the backend
const uint64_t MaxExtensionCount = 12;
const uint64_t MaxExtensionSize = 0;
const uint64_t MaxMargin = 50; // Number of cycles maximum to miss (by compile-time analysis)

// Options to transform the loop
// - EXTEND --- unroll the loop --- based on a calculated factor
// - BRANCH --- inject a biased branch into the loop --- based on a calculated factor
// - MANUAL --- determine callback locations manually via LatencyDFA traversal, 
//              occurs when the LLS is large
enum TransformOption {EXTEND, BRANCH, MANUAL};

class LoopTransform
{
public:
    LoopTransform(Loop *L, Function *F,  LoopInfo *LI, DominatorTree *DT,
                  ScalarEvolution *SE, AssumptionCache *AC, 
                  OptimizationRemarkEmitter *ORE, uint64_t Gran);

    void Transform();
    void BuildBiasedBranch(Instruction *InsertionPoint, uint64_t ExtensionCount);
    void ExtendLoop(uint64_t ExtensionCount);
    set<Instruction *> *GetCallbackLocations() { return &CallbackLocations; }
    TransformOption GetTransformationTy() { return this->Extend; }

    void PrintCurrentLoop();


private:
    Loop *L;
    Function *F;
    uint64_t Granularity;
    LatencyDFA *LoopLDFA;

    // Wrapper pass state
    LoopInfo *LI;
    DominatorTree *DT;
    ScalarEvolution *SE;
    AssumptionCache *AC;
    OptimizationRemarkEmitter *ORE;

    // Initialization state
    bool CorrectForm=false;
    MDNode *CBNode;

    // Transform info, statistics
    TransformOption Extend=TransformOption::BRANCH; // default
    set<Instruction *> CallbackLocations; // We want to record the important points in the bitcode 
                                          // at which the new loop which are eligible for future
                                          // callback injections, transformations, etc. --- if (Extend),
                                          // the callback location is the last instruction in the loop,
                                          // else, the callback location is the branch instruction of
                                         // the new basic block inserted (biased branch)

    void _transformSubLoops();
    
    Instruction *_buildCallbackBlock(CmpInst *CI, Instruction *InsertionPoint, const string MD);
    void _buildIterator(PHINode *&NewPHI, Instruction *&Iterator);
    void _setIteratorPHI(PHINode *ThePHI, Value *Init, Value *Iterator);
    void _buildBottomGuard(BasicBlock *Source, BasicBlock *Exit, PHINode *IteratorPHI);
    void _designateTopGuardViaPredecessors();
    void _designateBottomGuardViaExits();
    void _collectUnrolledCallbackLocations();
    bool _canVectorizeLoop() { return true; } // TODO 
    uint64_t _calculateLoopExtensionStats(uint64_t LLS, uint64_t *MarginOffset);
    Instruction *_findOffsetInst(uint64_t MarginOffset);
};