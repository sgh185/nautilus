//This llvm pass is intended to be a final pass before 

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/IRBuilder.h"

#include <unordered_map>
#include <set>
#include <queue>
#include <deque>

using namespace llvm;

void populateLibCallMap(std::unordered_map<std::string, int>* functionCalls){
    std::pair<std::string, int> call;

    //Allocations
    call.first = "malloc";
    call.second = 2;
    functionCalls->insert(call);

    call.first = "calloc";
    call.second = 3;
    functionCalls->insert(call);

    call.first = "realloc";
    call.second = 4;
    functionCalls->insert(call);

    call.first = "jemalloc";
    call.second = -1;
    functionCalls->insert(call);

    call.first = "mmap";
    call.second = -1;
    functionCalls->insert(call);

    //Mem Uses
    //TODO Ask if memset is important...
    call.first = "llvm.memset.p0i8.i64";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.memcpy.p0i8.p0i8.i64";
    call.second = -3;
    functionCalls->insert(call);

    call.first = "llvm.memmove.p0i8.p0i8.i64";
    call.second = -3;
    functionCalls->insert(call);






    //Frees
    call.first = "free";
    call.second = 1;
    functionCalls->insert(call);

    //Unimportant calls
    call.first = "printf";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "lrand48";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fwrite";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fputc";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fprintf";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "strtol";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "log";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "exit";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fread";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "feof";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fclose";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fflush";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "strcpy";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "srand48";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "ferror";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fopen";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.dbg.value";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.dbg.declare";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.lifetime.end.p0i8";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.lifetime.start.p0i8";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "sin";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "cos";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "tan";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.fabs.f64";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_read";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_stop";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_start";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_clear";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "randlc";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "vranlc";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "c_print_results";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "puts";
    call.second = -2;
    functionCalls->insert(call);




}

bool outputPDG(){
    //TODO: Determine how we are going to store the PDG for the program. Should it be a json file
    //or if Simone/Peter have a better idea.
    return 1;
}

