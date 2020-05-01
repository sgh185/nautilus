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

#include "runtime_tables.h"

extern "C" void user_init();

extern allocEntry *StackEntry;

// For whole file
// CONV [struct] -> [typedef struct]


typedef struct {
    void *paddr_of_ptr;
    void *new_paddr_to_write;
} patch_item;

typedef struct {
    uint64_t count = 0;
    patch_item *patches;
} patchlist;

union ptrComps {
    void *ptr;
    uint64_t val;
    sint64_t sval;
};

typedef struct {
    void *oldAddr;
    void *newAddr;
    uint64_t length;
} regions;


typedef enum {
    PAGE_4K_GRANULARITY,
    ALLOCATION_GRANULARITY,
} granularity_t;

extern "C" int patching_entry(void **, regions *, uint64_t *, granularity_t);

extern "C" int safe_to_patch(void *begin);

//Executes patch prepared by carat_prepare_patch
void execute_patch(patchlist *);

//This will calloc all the new memory spots
bool verifyPatch(std::unordered_set<allocEntry *> *allocs); // FIX

//Prepares patching for memory
void *prepare_patch(void **, std::unordered_set<allocEntry *>, granularity_t); // FIX

