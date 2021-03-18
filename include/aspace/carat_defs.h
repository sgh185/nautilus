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
 * Copyright (c) 2019, Brian Suchy
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Brian Suchy
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#pragma once


#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>
#include <nautilus/aspace.h>
#include <nautilus/naut_types.h>
#include <nautilus/naut_string.h>
#include <nautilus/skiplist.h>


// TODO: path need to be changed
// DATA structure go outside
#include <nautilus/list.h>


/*
 * Region structure
 */ 
#ifdef NAUT_CONFIG_ASPACE_CARAT_REGION_RB_TREE
    #include <aspace/region_tracking/mm_rb_tree.h>

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_SPLAY_TREE
    #include <aspace/region_tracking/mm_splay_tree.h>

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_LINKED_LIST
    #include <aspace/region_tracking/mm_linked_list.h>

#else
    #include <aspace/region_tracking/node_struct.h>

#endif


/*
 * =================== Base Utility Macros ===================  
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
 * ========== Definitions --- Core CARAT ==========
 *
 * Definitions for: 
 * 1. CARAT contexts
 * 2. Allocation maps
 * 3. Escapes sets
 * 4. Allocation entries
 * 5. CARAT-based ASpaces
 *
 * Interfaces for 1.-4. can be found in <aspace/runtime_tables.h>
 * Interface for 5. can be found in ___FILL IN LATER___ 
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
 * More names for the same thing
 */ 
typedef carat_context nk_carat_context;


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
 * ========== Definitions --- Globals for CARAT ========== 
 */ 
/*
 * Global carat context declaration
 */ 
extern carat_context global_carat_context;


/*
 * Non-canonical address used for protections checks
 */ 
extern void *non_canonical;



/*
 * ========== Definitions --- ASpace-related CARAT ========== 
 */ 
typedef struct nk_aspace_carat_thread {
    struct nk_thread * thread_ptr;
    struct list_head thread_node;
} nk_aspace_carat_thread_t;

typedef struct nk_aspace_carat {
    // pointer to the abstract aspace that the
    // rest of the kernel uses when dealing with this
    // address space
    nk_aspace_t *aspace;

    /*
     * CARAT state 
     */
    nk_carat_context *context;

    // perhaps you will want to do concurrency control?
    spinlock_t  lock;


    mm_struct_t * mm;

    // Your characteristics
    nk_aspace_characteristics_t chars;

    //We may need the list of threads
    //   struct list_head threads;
    nk_aspace_carat_thread_t threads;

} nk_aspace_carat_t;


