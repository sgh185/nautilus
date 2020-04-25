#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#define DEBUG_OUTPUT 0

#if DEBUG_OUTPUT
#define DEBUG(S, ...) fprintf(stderr, "carat_user: debug(%8d): " S, ##__VA_ARGS__)
#else 
#define DEBUG(S, ...) 
#endif

#pragma once
#include "runtime_tables.hpp"

extern uint64_t currentStateID;

class state;

extern state* activeState;

class state{
    public:
        std::set<allocEntry*>* allocations;
        //These two things will point to the starting spot of state
        std::string functionName = "";
        uint64_t lineNumber = 0;
        
        //These two things will point to the end spot for the state
        std::string endName = "";
        uint64_t endLineNum = 0;

        uint64_t stateID = 0;

        uint64_t temporalGraph = 0;

        //Parent and child states
        state* parent = nullptr;
        std::set<state*> children;

        std::map<allocEntry*, std::map<allocEntry*, uint64_t>*> temporalConnections;
        std::map<allocEntry*, uint64_t> usesInState;

        void** stateWindow = nullptr;
        uint64_t stateWindowSize = 0x8000000;
        uint64_t totalStateEntries = 0;
        bool calcUsesInState = true;

        //stats version of state creation
        state(char* funcName, uint64_t lineNum, uint64_t temporalTracking);

        //default state creation
        state();

        //O(allocs * allocationMap * averageSize(allocToEscapeMap))
        //This algorithm looks at each allocEntry and builds a mapping to every other allocEntry
        //with a weight that gives how many escapes to itself are within those entries.

        //One potential issue with this is it currently only builds a mapping for allocs that are within the
        //state initially given. We could expand it to add allocs that have escapign references as well.
        void ProcessTemporalGraph();

        //This will process the batch of state additions
        void processStateWindow();

};



extern std::map<uint64_t, state*> stateMap;


//This will be placed outside of a block of code to indicate we are beginning to track a state
//Build a new one if it doesn't exist
uint64_t GetState(char* funcName, uint64_t lineNum, uint64_t temporalTracking); 
uint64_t EndState(uint64_t stateID);
extern "C" uint64_t CGetState(char* fN, uint64_t lN, uint64_t temporalTracking);
extern "C" uint64_t CEndState(uint64_t sID);

void setParentState(char* funcName, uint64_t lineNum);

void GenerateTemporalGraph();

//This will be put ahead of all store/load instructions during tracking to help build the current state
void AddToState(void* allocPtr);
extern "C" uint64_t CAddToState(void* allocPtr);

void AddToStateMulti(void* allocPtr1, void* allocPtr2, uint64_t dinaGraph);

void PrintJSON();
//This function will dump out the allocations associated with a particular stateID
void ReportState(uint64_t stateID);
//This will call caratReportState for every state
void ReportStates();

//These will handle building/destructing the allocation table with allocEntry's
void StateAlloc(void* address, uint64_t length, std::string variableName, std::string fileOrigin, uint64_t lineNumber, uint64_t columnNumber);
void StateCalloc(void* address, uint64_t len, uint64_t sizeOfEntry, std::string variableName, std::string fileOrigin, uint64_t lineNumber, uint64_t columnNumber);
void StateRealloc(void* address, void* newAddress, uint64_t length, std::string variableName, std::string fileOrigin, uint64_t lineNumber, uint64_t columnNumber);
void StateFree(void* address);


