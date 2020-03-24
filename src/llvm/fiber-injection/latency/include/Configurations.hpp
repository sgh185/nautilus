#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
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
#include "llvm/IR/Attributes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Dominators.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SparseBitVector.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Vectorize/LoopVectorizationLegality.h"

#include <vector>
#include <set>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cstdlib>

using namespace llvm;
using namespace std;

// Pass settings
#define DEBUG 1
#define LOOP_DEBUG 1
#define FALSE 0

// Policies
#define LOOP_GUARD 1

#define GRAN 100

#define MAXIMUM 0
#define EXPECTED 1

#define HIGHCON 0
#define MEDCON 1
#define LOWCON 2

#define CONSERV HIGHCON
#define LATCONFIG EXPECTED

// Debugging
#define DEBUG_INFO(str) do { if (DEBUG) { errs() << str; } } while (0)
#define LOOP_DEBUG_INFO(str) do { if (LOOP_DEBUG) { errs() << str; } } while (0)
#define OBJ_INFO(obj) do { if (DEBUG) { obj->print(errs()); errs() << "\n"; } } while (0)
#define LOOP_OBJ_INFO(obj) do { if (LOOP_DEBUG) { obj->print(errs()); errs() << "\n"; } } while (0)