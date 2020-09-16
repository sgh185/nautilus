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
 * Skiplist setup
 */
#define CARAT_INIT_NUM_GEARS 6


/*
 * Sizes
 */ 
#define ONE_MB 1048576
#define THIRTY_TWO_GB 0x800000000ULL
#define ESCAPE_WINDOW_SIZE ONE_MB


/*
 * =================== Data Structures/Definitions ===================  
 */ 

/*
 * Typedefs for CARAT data structures
 */ 
typedef nk_slist_uintptr_t nk_carat_escape_set;
typedef nk_slist_uintptr_t_uintptr_t nk_carat_allocation_map;


/* 
 * Interface for "nk_carat_escape_set" --- generic
 */ 
#define CARAT_ESCAPE_SET_BUILD nk_slist_build(uintptr_t, CARAT_INIT_NUM_GEARS)
#define CARAT_ESCAPE_SET_ADD(set, key) nk_slist_add(uintptr_t, set, ((uintptr_t) key));


/*
 * Interface for "nk_carat_allocation_map" --- specifically for the
 * global "allocationMap" data structure
 */ 
#define CARAT_ALLOCATION_MAP_BUILD nk_map_build(uintptr_t, uintptr_t)
#define CARAT_ALLOCATION_MAP_INSERT(key, val) (nk_map_insert(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val)) 
#define CARAT_ALLOCATION_MAP_INSERT_OR_ASSIGN(key, val) (nk_map_insert_by_force(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val)) 
#define CARAT_ALLOCATION_MAP_REMOVE(key) nk_map_remove(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) key))
#define CARAT_ALLOCATION_MAP_BETTER_LOWER_BOUND(key) nk_map_better_lower_bound(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) key))


/*
 * struct allocation_entry
 *
 * Setup for an allocation entry 
 * - An allocation_entry stores necessary information to *track* each allocation
 * - There is one allocation_entry object for each allocation
 */ 
typedef struct allocation_entry_t { 

    /*
     * Pointer to the allocation, size of allocation
     */ 
    void *pointer; 
    uint64_t length;

    /*
     * Set of all *potential* escapes for this particular
     * allocation, the pointer -> void **
     */ 
    nk_carat_escape_set *allocToEscapeMap;

} allocation_entry;


/*
 * Setup/constructor for an allocation_entry object
 */ 
allocation_entry *_carat_create_allocation_entry(void *ptr, uint64_t allocation_size);


/*
 * Macro expansion utility --- creating allocation_entry objects
 * and adding them to the allocation map
 */ 
#define CREATE_ENTRY_AND_ADD(key, str) \
	/*
	 * Create a new allocation_entry object for the @new_address to be added
	 */ \
	allocation_entry *new_entry = _carat_create_allocation_entry(key, allocation_size); \
    \
    \
	/*
	 * Add the mapping [@##key : newEntry] to the allocationMap
	 */ \
	if (!(CARAT_ALLOCATION_MAP_INSERT(key, new_entry))) { \
		panic(str" %p\n", key);
	}


/*
 * Macro expansion utility --- for deleting allocation_entry objects
 */ 
#define REMOVE_ENTRY(key, str) \
	/*
	 * Delete the @##key from the allocation map
	 */ \
	if (!(CARAT_ALLOCATION_MAP_REMOVE(key))) { \
		panic(str" %p\n", key);
	}



/*
 * allocationMap
 * - Global definition for the allocation map
 * - Stores [allocation address : allocation_entry address]
 */ 
extern nk_carat_allocation_map *allocationMap;


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
extern allocation_entry *StackEntry;
extern uint64_t rsp;


/*
 * Main driver for KARAT initialization
 */ 
void nk_carat_init();


/*
 * Utility for rsp 
 */
uint64_t _carat_get_rsp();


/*
 * =================== Utility Analysis Methods ===================  
 */ 

/*
 * Determines if a target address @query_address aliases with the specified 
 * allocation @alloc_address by performing a range search within the memory
 * pointed to by @alloc_address --- returns boolean values
 */ 
int _carat_does_alias(void *query_address, void *alloc_address, uint64_t allocation_size);


/*
 * Calculates the offset of @query_address within the memory pointed to by 
 * @alloc_address IFF @query_address aliases @alloc_address --- returns -1
 * upon failure
 */ 
sint64_t _carat_get_query_offset(void *query_address, void *alloc_address, uint64_t allocation_size);


/*
 * Takes a specified allocation and returns its corresponding allocation_entry,
 * otherwise return nullptr
 */ 
allocation_entry *_carat_find_allocation_entry(void *address);


/*
 * Statistics --- obvious
 */
void nk_carat_report_statistics();


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



