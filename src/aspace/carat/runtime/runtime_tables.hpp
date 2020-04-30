#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <map>
#include <utility>
#include <unordered_set>
#include <set>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/time.h>
#include <malloc.h>

#ifndef __ALLOC_ENTRY__
#define __ALLOC_ENTRY__

class allocEntry{
    public:
        void* pointer = nullptr;
        uint64_t length = 0;
        std::unordered_set<void**> allocToEscapeMap;
        void* patchPointer = nullptr; 

        //New State Tracking
        std::string variableName = "";
        //file origin, line number, column number
        std::vector<std::tuple<std::string, uint64_t, uint64_t>> origin;
        uint64_t totalPointerWeight = 0;
        uint64_t alignment = 8;

        allocEntry(void* ptr, uint64_t len, std::string varName, std::string fileOri, uint64_t lineNum, uint64_t colNum);

        allocEntry(void* ptr, uint64_t len);
};


#endif//allocEntry ifndef

//Alloc addr, length
extern std::map<void*, allocEntry*>* allocationMap;
extern std::map<allocEntry*, std::map<allocEntry*, uint64_t>*> allocConnections;
//Addr Escaping To , allocAddr, length
extern allocEntry* StackEntry;
extern uint64_t rsp;

//This will hold the escapes yet to be processed
extern uint64_t escapeWindowSize;
extern uint64_t totalEscapeEntries;
extern void*** escapeWindow;

void texas_init();

//This will give a 64-bit value of the stack pointer
uint64_t getrsp();

//Initialize the stack
extern "C" int stack_init();
extern "C" void user_init();

class texasStartup;


//This function will tell us if the escapeAddr aliases with the allocAddr
//If it does the return value will be at what offset it does
//If it does not, the return will be -1
int64_t doesItAlias(void* allocAddr, uint64_t length, uint64_t escapeVal);

void GenerateConnectionGraph();


//This function takes an arbitrary address and return the aliasing allocEntry*, else nullptr
allocEntry* findAllocEntry(void* address);

//These calls will build the allocation table
void AddToAllocationTable(void*, uint64_t);
void AddCallocToAllocationTable(void*, uint64_t, uint64_t);
void HandleReallocInAllocationTable(void*, void*, uint64_t);

/*
 * 1) Search for address escaping within allocation table (an ordered map that contains all current allocations <non overlapping blocks of memory each consisting of an  address and length> made by the program
 * 2) If found (the addressEscaping variable falls within one of the blocks), then a new entry into the escape table is made consisting of the addressEscapingTo, the addressEscaping, and the length of the addressEscaping (for optimzation if consecutive escapes occur)
 */
void AddToEscapeTable(void*, void*, uint64_t);

void processEscapeWindow();

//This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void*);

//This will generate a connectionGraph that breaks down how Carat is connected
void GenerateConnectionGraph();

//This will report out the stats of a program run
void ReportStatistics();

//This will report a histogram relating to escapes per allocation.
void HistogramReport();


