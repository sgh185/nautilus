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
#include <nautilus/skiplist.h>

#ifndef __ALLOC_ENTRY__
#define __ALLOC_ENTRY__


/*
 * =================== Utility Macros ===================  
 */ 

/*
 * Debugging macros --- for QEMU
 */ 
#define DB(x) outb(x, 0xe9)
#define DHN(x) outb(((x & 0xF) >= 10) ? (((x & 0xF) - 10) + 'a') : ((x & 0xF) + '0'), 0xe9)
#define DHB(x) DHN(x >> 4) ; DHN(x);
#define DHW(x) DHB(x >> 8) ; DHB(x);
#define DHL(x) DHW(x >> 16) ; DHW(x);
#define DHQ(x) DHL(x >> 32) ; DHL(x);
#define DS(x) { char *__curr = x; while(*__curr) { DB(*__curr); *__curr++; } }


/*
 * Printing
 */ 
#define DO_CARAT_PRINT 0
#if DO_CARAT_PRINT
#define CARAT_PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define CARAT_PRINT(...) 
#endif


/*
 * Malloc
 */ 
#define CARAT_MALLOC(n) ({void *__p = malloc(n); if (!__p) { CARAT_PRINT("Malloc failed\n"); panic("Malloc failed\n"); } __p;})
#define CARAT_REALLOC(p, n) ({void *__p = realloc(p, n); if (!__p) { CARAT_PRINT("Realloc failed\n"); panic("Malloc failed\n"); } __p;})


/*
 * Conditions check 
 */ 
#define CHECK_CARAT_READY if (!carat_ready) { return; }


/*
 * =================== Data Structures/Definitions ===================  
 */ 

/*
 * Typedefs for CARAT data structures
 */ 
typedef nk_slist_uintptr_t nk_karat_escape_map;
typedef nk_slist_uintptr_t_uintptr_t nk_karat_allocation_map;


/*
 * struct allocEntry
 *
 * Setup for an allocation entry 
 * - An allocEntry stores necessary information to *track* each allocation
 * - There is one allocEntry object for each allocation
 */ 
typedef struct allocEntry_t { // CONV [class] -> [typedef struct] 

    /*
     * Pointer to the allocation, size of allocation
     */ 
    void *pointer; // CONV [nullptr] -> [NULL --- moved to constructor]
    uint64_t length; // CONV [nullptr] -> [NULL --- moved to constructor]

    /*
     * Set of all *potential* escapes for this particular
     * allocation, the pointer -> void **
     */ 
    nk_karat_escape_map *allocToEscapeMap; // CONV [unordered_set<void **>] -> [nk_slist_uintptr_t *]

} allocEntry;


/*
 * Setup/constructor for an allocEntry object
 */ 
allocEntry* allocEntrySetup(void* ptr, uint64_t len); // CONV [class constructor] -> [function that returns an instance]


/*
 * allocationMap
 * - Global definition for the allocation map
 * - Stores [allocation address : allocEntry address]
 */ 
extern nk_karat_allocation_map *allocationMap; // CONV [map<void *, allocEntry *>] -> [nk_slist_uintptr_t_uintptr_t *]


/*
 * Escape window
 * - The escape window is an optimization to conduct escapes handling in batches
 * - Functions in the following way:
 *   
 *   void **a = malloc(); // the data itself is treated as a void * --- therefore, malloc
 *                        // treated with a double pointer
 *   void **escape = a; // an escape
 *   void ***escapeWindow = [escape, escape, escape, ...] // an array of escapes
 * 
 * - Statistics --- escapeWindowSize, totalEscapeEntries helps with batch processing,
 *                  indicating how many escapes are yet to be processed
 */ 
extern void ***escapeWindow;
extern uint64_t escapeWindowSize;
extern uint64_t totalEscapeEntries;


/*
 * =================== Init/Setup ===================  
 */ 

/*
 * TOP --- Setup for KARAT inside init():
 * - texas_init() will handle all setup for the allocation table, escape window,
 *   and any stack address handling (StackEntry and rsp)
 * - carat_ready is a global that will be set only after texas_init() is called, 
 * - allocation and espapes methods only continue if carat_ready is true 
 */


/*
 * Globals necessary for init
 */ 
extern int carat_ready; 
extern allocEntry *StackEntry;
extern uint64_t rsp;


/*
 * Main driver for KARAT initialization
 */ 
void texas_init();


/*
 * Utility for rsp 
 */
uint64_t getrsp();


/*
 * =================== Utility Analysis Methods ===================  
 */ 

/*
 * - Determines if an escaped value aliases with a specified allocation 
 * - Will return the offset of the escape if found, -1 otherwise
 */ 
sint64_t doesItAlias(void *allocAddr, uint64_t length, uint64_t escapeVal);


/*
 * Takes a specified allocation and returns its corresponding allocEntry,
 * otherwise return nullptr
 */ 
allocEntry *findAllocEntry(void *address);


/*
 * Statistics --- obvious
 */
void ReportStatistics();


/*
 * =================== Allocations Handling Methods ===================  
 */ 

/*
 * Instrumentation for "malloc" --- adding
 */
void AddToAllocationTable(void *, uint64_t);


/*
 * Instrumentation for "calloc" --- adding
 */ 
void AddCallocToAllocationTable(void *, uint64_t, uint64_t);


/*
 * Instrumentation for "realloc" --- adding
 */
void HandleReallocInAllocationTable(void *, void *, uint64_t);


/*
 * Instrumentation for "free" --- removing
 */
void RemoveFromAllocationTable(void *);


/*
 * =================== Escapes Handling Methods ===================  
 */ 

/*
 * AddToEscapeTable
 * 1. Search for address escaping within allocation table (an ordered map 
 *    that contains all current allocations <non overlapping blocks of memory 
 *    each consisting of an  address and length> made by the program
 * 2. If found (the addressEscaping variable falls within one of the blocks), 
 *    then a new entry into the escape table is made consisting of the 
 *    addressEscapingTo, the addressEscaping, and the length of the addressEscaping 
 *    (for optimzation if consecutive escapes occur)
 */
void AddToEscapeTable(void *);


/*
 * Batch processing for escapes
 */ 
void processEscapeWindow();



