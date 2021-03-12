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


#include <aspace/runtime_tables.h>


// TODO: path need to be changed
// DATA structure go outside
#include <nautilus/list.h>

#ifdef NAUT_CONFIG_ASPACE_CARAT_REGION_RB_TREE
    #include <aspace/region_tracking/mm_rb_tree.h>

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_SPLAY_TREE
    #include <aspace/region_tracking/mm_splay_tree.h>

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_LINKED_LIST
    #include <aspace/region_tracking/mm_linked_list.h>

#else
    #include <aspace/region_tracking/node_struct.h>

#endif

typedef struct carat_context_t nk_carat_context;

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


