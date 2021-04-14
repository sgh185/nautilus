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
#pragma once

#include <nautilus/nautilus.h>
#include <aspace/carat.h>



/*
 * =================== Interfaces for runtime table data structures ===================  
 * 
 * Interfaces for:
 * - CARAT contexts
 * - Allocation tables
 * - Escapes sets
 * - Allocation entries
 */ 


/*
 * Helper macros for manipulating CARAT contexts 
 * (nk_carat_context). Note that all parameters
 * are ASSUMED to be pointers
 */ 

/*
 * Conditions check 
 */ 
#define CHECK_CARAT_BOOTSTRAP_FLAG if (!karat_ready) { return; } 
#define CHECK_CARAT_READY(c) if (!(c->carat_ready)) { return; }
#define CARAT_READY_ON(c) c->carat_ready = 1
#define CARAT_READY_OFF(c) c->carat_ready = 0


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
 * per nk_carat_context (i.e. @c) "allocation_map" data structure
 */ 
#define CARAT_ALLOCATION_MAP_BUILD nk_map_build(uintptr_t, uintptr_t)
#define CARAT_ALLOCATION_MAP_SIZE(c) nk_map_get_size((c->allocation_map))
#define CARAT_ALLOCATION_MAP_INSERT(c, key, val) (nk_map_insert((c->allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val))) 
#define CARAT_ALLOCATION_MAP_INSERT_OR_ASSIGN(c, key, val) (nk_map_insert_by_force((c->allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val)))
#define CARAT_ALLOCATION_MAP_REMOVE(c, key) nk_map_remove((c->allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key))
#define CARAT_ALLOCATION_MAP_BETTER_LOWER_BOUND(c, key) nk_map_better_lower_bound((c->allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key))
#define CARAT_ALLOCATION_MAP_ITERATE(c) \
	nk_slist_node_uintptr_t_uintptr_t *iterator; \
	nk_pair_uintptr_t_uintptr_t *pair; \
    \
    nk_slist_foreach((c->allocation_map), pair, iterator)

#define CARAT_ALLOCATION_MAP_CURRENT_ADDRESS ((void *) (pair->first)) // Only to be used within CARAT_ALLOCATION_MAP_ITERATE
#define CARAT_ALLOCATION_MAP_CURRENT_ENTRY ((allocation_entry *) (pair->second)) // Only to be used within CARAT_ALLOCATION_MAP_ITERATE


#define USE_GLOBAL_CARAT 0

#if USE_GLOBAL_CARAT
/*
 * Conditions check 
 */ 
#define CHECK_GLOBAL_CARAT_READY if (!(global_carat_context.carat_ready)) { return; }
#define GLOBAL_CARAT_READY_ON global_carat_context.carat_ready = 1
#define GLOBAL_CARAT_READY_OFF global_carat_context.carat_ready = 0


/*
 * Interface for "nk_carat_allocation_map" --- specifically for the
 * global "allocation_map" data structure
 */ 
#define GLOBAL_CARAT_ALLOCATION_MAP_BUILD nk_map_build(uintptr_t, uintptr_t)
#define GLOBAL_CARAT_ALLOCATION_MAP_SIZE nk_map_get_size((global_carat_context.allocation_map))
#define GLOBAL_CARAT_ALLOCATION_MAP_INSERT(key, val) (nk_map_insert((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val))) 
#define GLOBAL_CARAT_ALLOCATION_MAP_INSERT_OR_ASSIGN(key, val) (nk_map_insert_by_force((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key), ((uintptr_t) val)))
#define GLOBAL_CARAT_ALLOCATION_MAP_REMOVE(key) nk_map_remove((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key))
#define GLOBAL_CARAT_ALLOCATION_MAP_BETTER_LOWER_BOUND(key) nk_map_better_lower_bound((global_carat_context.allocation_map), uintptr_t, uintptr_t, ((uintptr_t) key))
#define GLOBAL_CARAT_ALLOCATION_MAP_ITERATE \
	nk_slist_node_uintptr_t_uintptr_t *iterator; \
	nk_pair_uintptr_t_uintptr_t *pair; \
    \
    nk_slist_foreach((global_carat_context.allocation_map), pair, iterator)

#endif


/*
 * Setup/constructor for an allocation_entry object
 */ 
allocation_entry *_carat_create_allocation_entry(void *ptr, uint64_t allocation_size);


/*
 * Macro expansion utility --- creating allocation_entry objects
 * and adding them to the allocation map
 */ 
#define CREATE_ENTRY_AND_ADD(ctx, key, size, str) \
	/*
	 * Create a new allocation_entry object for the new_address to be added
	 */ \
	allocation_entry *new_entry = _carat_create_allocation_entry(key, size); \
    \
    \
	/*
	 * Add the mapping [@##key : newEntry] to the allocation_map
	 */ \
	if (!(CARAT_ALLOCATION_MAP_INSERT(ctx, key, new_entry))) { \
		panic(str" %p\n", key); \
	}


/*
 * Macro expansion utility --- creating allocation_entry objects
 * and adding them to the allocation map
 */ 
#define CREATE_ENTRY_AND_ADD_SILENT(ctx, key, size) \
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
	if (!(CARAT_ALLOCATION_MAP_INSERT(ctx, key, new_entry))) { \
        DS("dup: "); \
        DHQ(((uint64_t) key)); \
        DS("\n"); \
    }


/*
 * Macro expansion utility --- for deleting allocation_entry objects
 */ 
#define REMOVE_ENTRY(ctx, key, str) \
	/*
	 * Delete the @##key from the allocation map
	 */ \
	if (!(CARAT_ALLOCATION_MAP_REMOVE(ctx, key))) { \
		panic(str" %p\n", key); \
	}


/*
 * Macro expansion utility --- fetch the current CARAT context
 */ 
#define FETCH_CARAT_CONTEXT (((nk_aspace_carat_t *) get_cur_thread()->aspace->state)->context) 


/*
 * CARAT context fetchers, setters
 */ 
#define FETCH_TOTAL_ESCAPES(ctx) (ctx->total_escape_entries)
#define FETCH_ESCAPE_WINDOW(ctx) (ctx->escape_window)
#define RESET_ESCAPE_WINDOW(ctx) ctx->total_escape_entries = 0
#define ADD_ESCAPE_TO_WINDOW(ctx, addr) \
    ctx->escape_window[FETCH_TOTAL_ESCAPES(ctx)] = ((void **) addr); \
    ctx->total_escape_entries++;


/*
 * Debugging
 */
#define PRINT_ASPACE_INFO \
    if (DO_CARAT_PRINT) \
    { \
        DS("gct: "); \
        DHQ(((uint64_t) get_cur_thread())); \
        DS("\na: "); \
        DHQ(((uint64_t) get_cur_thread()->aspace)); \
        DS("\nctx "); \
        DHQ(((uint64_t) (FETCH_CARAT_CONTEXT))); \
        DS("\n"); \
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
 * Main driver for global CARAT initialization
 */ 
void nk_carat_init(void);


/*
 * Driver for per-aspace CARAT initialization
 */ 
nk_carat_context * initialize_new_carat_context(void);


/*
 * Utility for rsp 
 */
uint64_t _carat_get_rsp(void);


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
allocation_entry * _carat_find_allocation_entry(
    nk_carat_context *the_context,
    void *address
);

/*
 * Statistics --- obvious
 */
void nk_carat_report_statistics(void);


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
void nk_carat_instrument_calloc(void *address, uint64_t size_of_element, uint64_t num_elements);


/*
 * Instrumentation for "realloc" --- adding
 */
void nk_carat_instrument_realloc(void *new_address, uint64_t allocation_size, void *old_address);


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
void _carat_process_escape_window(nk_carat_context *the_context);

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

/*
 * Instrumentation for call instructions
 * Make sure the stack has enough space to grow to support this guarded call instruction. 
 */
void nk_carat_guard_callee_stack(uint64_t stack_frame_size);

