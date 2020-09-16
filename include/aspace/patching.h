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
 * Patching --- CARAT Runtime --- ported to C 
 * 
 * Keywords:
 * - FIX: obvious
 * - CONV: conversion from C++ to C, format: CONV [C++] -> [C]
 */ 

#include <aspace/runtime_tables.h>
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>

#define CARAT_STACK_CHECK 0

/*
 * =================== Data Structures/Definitions ===================  
 */

/*
 * A packaged context for moving an allocation to a target address
 */ 
struct move_alloc_state {
    void *allocationToMove; // Address to move
    void *allocationTarget; // Address to move to 
    uint64_t length; // Length of allocation to move
    int failed; // Move status
};


/*
 * =================== Patching Methods ===================  
 */ 

/*
 * Driver for performing a move of an allocation that's already tracked
 */ 
int nk_carat_move_allocation(void* allocationToMove, void* allocationTarget);


/*
 * Upon a move --- "carat_patch_escapes" will iterate through all escapes
 * from @entry (existing at @entry + (an offset)), and update each escaped 
 * pointer to use @allocationTarget (at @allocationTarget + (an offset))
 *
 * TODO --- [Rename : _carat_patch_escapes]
 */ 
int carat_patch_escapes(allocEntry *entry, void* allocationTarget);


/*
 * Upon a move --- "handle_thread" will update each thread's register 
 * state to use @state->allocationTarget instead of @state->allocationToMove
 *
 * TODO --- [Rename : _carat_patch_thread_state]
 * TODO --- @state needs to be a type [struct move_alloc_state *]
 */ 
static void handle_thread(struct nk_thread *t, void *state);


/*
 * DEPRECATED --- Handled by proper/known use of compiler instrumentation
 *
 * Manually performs an update from @entry, pointing it to @allocationTarget 
 * by explicitly removing @entry's allocEntry object and adding an object
 * for @allocationTarget
 * 
 * TODO --- [Rename : _carat_update_entry]
 */ 
int carat_update_entry(allocEntry *entry, void* allocationTarget);

