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
 *          Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "../include/Configurations.hpp"

/*
 * Function names to inject/directly handle
 */ 
const string CARAT_MALLOC = "nk_carat_instrument_malloc",
             CARAT_REALLOC = "nk_carat_instrument_realloc",
             CARAT_CALLOC = "nk_carat_instrument_calloc",
             CARAT_REMOVE_ALLOC = "nk_carat_instrument_free",
             CARAT_STATS = "nk_carat_report_statistics",
             CARAT_ESCAPE = "nk_carat_instrument_escapes",
             PANIC_STRING = "LLVM_panic_string",
             LOWER_BOUND = "lower_bound",
             UPPER_BOUND = "upper_bound",
             CARAT_INIT = "nk_carat_init",
             ENTRY_SETUP = "_carat_create_allocation_entry", // TODO: remove
             ANNOTATION = "llvm.global.annotations",
             NOCARAT = "nocarat";


/*
 * Important/necessary methods/method names to track
 */ 
std::unordered_map<std::string, Function *> NecessaryMethods = 
    unordered_map<string, Function *>();

std::vector<std::string> ImportantMethodNames = {
    CARAT_MALLOC, 
    CARAT_REALLOC, 
    CARAT_CALLOC,
    CARAT_REMOVE_ALLOC, 
    CARAT_STATS,
    CARAT_ESCAPE,
    CARAT_INIT,
    ENTRY_SETUP
};

std::unordered_map<string, int> TargetMethods = {
    { "_kmem_malloc",  2 },
    { "kmem_free", 1 }
};