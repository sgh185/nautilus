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
#define DO_CARAT_PRINT 1
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
 * carat_context
 * 
 * - Main, global context for CARAT in the kernel
 * - Contains the global allocation map, escape window information, and 
 *   state from initialization and about stack allocation tracking
 * 
 * NOTE --- this is an ENGINEERING fix --- it is possible that a global
 * context will create more resource pressure, cache misses, and other 
 * underlying inefficiencies than the original design (scattered globals)
 */

typedef struct carat_context_t {

    /*
     * allocation_map
     * 
     * - Global allocation map
     * - Stores [allocation address : allocation_entry address]
     */ 
    nk_carat_allocation_map *allocation_map;


    /*
     * Escape window
     * - The escape window is an optimization to conduct escapes handling in batches
     * - Functions in the following way:
     *   
     *   void **a = malloc(); // the data itself is treated as a void * --- therefore, malloc
     *                        // treated with a double pointer
     *   void **escape = a; // an escape
     *   void ***escape_window = [escape, escape, escape, ...] // an array of escapes
     * 
     * - Statistics --- total_escape_entries is a counter helps with batch processing,
     *                  indicating how many escapes are yet to be processed
     */ 
    void ***escape_window;
    uint64_t total_escape_entries;


    /*
     * Flag to indicate that CARAT is ready to run
     */ 
    int carat_ready; 

} carat_context;


/*
 * Global carat context declaration
 */ 
extern carat_context global_carat_context;


/*
 * Non-canonical address used for protections checks
 */ 
extern void *non_canonical;


/*
 * Conditions check 
 */ 
#define CHECK_CARAT_READY if (!(global_carat_context.carat_ready)) { return; }
#define CARAT_READY_ON global_carat_context.carat_ready = 1
#define CARAT_READY_OFF global_carat_context.carat_ready = 0


/* 
 * Interface for "nk_carat_escape_set" --- generic
 */ 
#define CARAT_ESCAPE_SET_BUILD nk_slist_build(uintptr_t, CARAT_INIT_NUM_GEARS)
#define CARAT_ESCAPE_SET_SIZE(set) (nk_slist_get_size(set) - 2)
#define CARAT_ESCAPE_SET_ADD(set, key) nk_slist_add(uintptr_t, set, ((uintptr_t) key))
#define CARAT_ESCAPES_SET_ITERATE(set) \
    nk_slist_node_uintptr_t *iterator; \
    uintptr_t val; \
    \
    nk_slist_foreach(set, val, iterator) 


/*
 * Interface for "nk_carat_allocation_map" --- specifically for the
 * global "allocation_map" data structure
 */ 
#define CARAT_ALLOCATION_MAP_BUILD nk_map_build(uintptr_t, uintptr_t)
#define CARAT_ALLOCATION_MAP_SIZE nk_map_get_size((global_carat_context.allocation_map))
#define CARAT_ALLOCATION_MAP_INSERT(key, val) (nk_map_insert((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val))) 
#define CARAT_ALLOCATION_MAP_INSERT_OR_ASSIGN(key, val) (nk_map_insert_by_force((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val)))
#define CARAT_ALLOCATION_MAP_REMOVE(key) nk_map_remove((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key))
#define CARAT_ALLOCATION_MAP_BETTER_LOWER_BOUND(key) nk_map_better_lower_bound((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key))
#define CARAT_ALLOCATION_MAP_ITERATE \
	nk_slist_node_uintptr_t_uintptr_t *iterator; \
	nk_pair_uintptr_t_uintptr_t *pair; \
    \
    nk_slist_foreach((global_carat_context.allocation_map), pair, iterator)

#define CARAT_ALLOCATION_MAP_CURRENT_ADDRESS ((void *) (pair->first)) // Only to be used within CARAT_ALLOCATION_MAP_ITERATE
#define CARAT_ALLOCATION_MAP_CURRENT_ENTRY ((allocation_entry *) (pair->second)) // Only to be used within CARAT_ALLOCATION_MAP_ITERATE



/*
 * allocation_entry
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
    uint64_t size;

    /*
     * Set of all *potential* escapes for this particular
     * allocation, the pointer -> void **
     */ 
    nk_carat_escape_set *escapes_set;

} allocation_entry;


/*
 * Setup/constructor for an allocation_entry object
 */ 
allocation_entry *_carat_create_allocation_entry(void *ptr, uint64_t allocation_size);


/*
 * Macro expansion utility --- creating allocation_entry objects
 * and adding them to the allocation map
 */ 
#define CREATE_ENTRY_AND_ADD(key, size, str) \
	/*
	 * Create a new allocation_entry object for the new_address to be added
	 */ \
	allocation_entry *new_entry = _carat_create_allocation_entry(key, size); \
    \
    \
	/*
	 * Add the mapping [@##key : newEntry] to the allocation_map
	 */ \
	if (!(CARAT_ALLOCATION_MAP_INSERT(key, new_entry))) { \
		panic(str" %p\n", key); \
	}


/*
 * Macro expansion utility --- creating allocation_entry objects
 * and adding them to the allocation map
 */ 
#define CREATE_ENTRY_AND_ADD_SILENT(key, size) \
	/*
	 * Create a new allocation_entry object for the new_address to be added
	 */ \
	allocation_entry *new_entry = _carat_create_allocation_entry(key, size); \
    \
    \
	/*
	 * Add the mapping [@##key : newEntry] to the allocation_map BUT do
     * not panic if the entry already exists in the allocation_map
	 */ \
	if (!(CARAT_ALLOCATION_MAP_INSERT(key, new_entry))) { \
        DS("dup: "); \
        DHQ(((uint64_t) key)); \
        DS("\n"); \
    }


/*
 * Macro expansion utility --- for deleting allocation_entry objects
 */ 
#define REMOVE_ENTRY(key, str) \
	/*
	 * Delete the @##key from the allocation map
	 */ \
	if (!(CARAT_ALLOCATION_MAP_REMOVE(key))) { \
		panic(str" %p\n", key); \
	}


/*
 * =================== Init/Setup ===================  
 */ 

/*
 * TOP --- 
 * 
 * Setup for CARAT inside init(): nk_carat_init() will handle all 
 * setup for the allocation table, escape window, and any stack 
 * address handling
 */

/*
 * Compiler target for globals allocation tracking
 */ 
void _nk_carat_globals_compiler_target(void);


/*
 * Main driver for CARAT initialization
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
void nk_carat_instrument_global(void *address, uint64_t allocation_size, uint64_t global_ID);


/*
 * Instrumentation for "malloc" --- adding
 */
void nk_carat_instrument_malloc(void *address, uint64_t allocation_size);


/*
 * Instrumentation for "calloc" --- adding
 */ 
void nk_carat_instrument_calloc(void *address, uint64_t num_elements, uint64_t size_of_element);


/*
 * Instrumentation for "realloc" --- adding
 */
void nk_carat_instrument_realloc(void *old_address, void *new_address, uint64_t allocation_size);


/*
 * Instrumentation for "free" --- removing
 */
void nk_carat_instrument_free(void *address);


/*
 * =================== Escapes Handling Methods ===================  
 */ 

/*
 * Instrumentation for escapes
 * 1. Search for @escaping_address within global allocation map
 * 2. If an aliasing entry is found, the escape must be recorded into
 *    the aliasing entry's escapes set
 */
void nk_carat_instrument_escapes(void *escaping_address);


/*
 * Batch processing for escapes
 */ 
void _carat_process_escape_window();

/*
 * =================== Protection Handling Methods ===================  
 */ 

/*
 * Instrumentation for memory accesses
 * If the type of access specified by @is_write 
 * is determined to be illegal, panic. Otherwise, 
 * do nothing
 */
void nk_carat_guard_address(void *memory_address, int is_write);


