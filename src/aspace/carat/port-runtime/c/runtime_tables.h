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
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, Gaurav Chaudhary <gauravchaudhary2021@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 * 			Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
 * Runtime Tables --- CARAT Runtime --- ported to C 
 * 
 * Keywords:
 * - FIX: obvious
 * - CONV: conversion from C++ to C, format: CONV [C++] -> [C]
 */ 

#include <nautilus/nautilus.h>
#include <nautilus/naut_types.h>
#include <nautilus/naut_string.h>

#ifndef __ALLOC_ENTRY__
#define __ALLOC_ENTRY__

// CONV [class] -> [typedef struct]
typedef struct allocEntry {
        void *pointer = NULL; // CONV [nullptr] -> [NULL]
        uint64_t length = 0; // CONV [nullptr] -> [NULL]
        std::unordered_set<void **> allocToEscapeMap; // FIX
        void *patchPointer = NULL; // CONV [nullptr] -> [NULL]

        // New State Tracking
        char *variableName = ""; // CONV [std::string] 

        // file origin, line number, column number
        std::vector<std::tuple<std::string, uint64_t, uint64_t>> origin; // FIX
        uint64_t totalPointerWeight = 0; 
        uint64_t alignment = 8;

} allocEntry;

allocEntry* allocEntry(void* ptr, uint64_t len, char* varName, char* fileOri, uint64_t lineNum, uint64_t colNum); // CONV [class constructor] -> [function that returns an instance]

allocEntry* allocEntry(void* ptr, uint64_t len); // CONV [class constructor] -> [function that returns an instance]


#endif//allocEntry ifndef

//Alloc addr, length
extern std::map<void *, allocEntry *> *allocationMap; // FIX (also can we still use extern on these?)
extern std::map<allocEntry *, std::map<allocEntry *, uint64_t> *> allocConnections; // FIX
//Addr Escaping To , allocAddr, length
extern allocEntry *StackEntry;
extern uint64_t rsp;

//This will hold the escapes yet to be processed
extern uint64_t escapeWindowSize;
extern uint64_t totalEscapeEntries;
extern void ***escapeWindow;

void texas_init();

//This will give a 64-bit value of the stack pointer
uint64_t getrsp();

//Initialize the stack
extern "C" int stack_init();
extern "C" void user_init();

void texasStartup(); // CONV [class] -> [init function]
//class texasStartup;


//This function will tell us if the escapeAddr aliases with the allocAddr
//If it does the return value will be at what offset it does
//If it does not, the return will be -1
int64_t doesItAlias(void *allocAddr, uint64_t length, uint64_t escapeVal);

void GenerateConnectionGraph();


//This function takes an arbitrary address and return the aliasing allocEntry*, else nullptr
allocEntry* findAllocEntry(void *address);

//These calls will build the allocation table
void AddToAllocationTable(void *, uint64_t);
void AddCallocToAllocationTable(void *, uint64_t, uint64_t);
void HandleReallocInAllocationTable(void *, void *, uint64_t);

/*
 * 1) Search for address escaping within allocation table (an ordered map that contains all current allocations <non overlapping blocks of memory each consisting of an  address and length> made by the program
 * 2) If found (the addressEscaping variable falls within one of the blocks), then a new entry into the escape table is made consisting of the addressEscapingTo, the addressEscaping, and the length of the addressEscaping (for optimzation if consecutive escapes occur)
 */
void AddToEscapeTable(void *, void *, uint64_t);

void processEscapeWindow();

//This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void *);

//This will generate a connectionGraph that breaks down how Carat is connected
void GenerateConnectionGraph();

//This will report out the stats of a program run
void ReportStatistics();

//This will report a histogram relating to escapes per allocation.
void HistogramReport();


