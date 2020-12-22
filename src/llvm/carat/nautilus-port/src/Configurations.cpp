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
const std::string CARAT_MALLOC = "nk_carat_instrument_malloc",
                  CARAT_REALLOC = "nk_carat_instrument_realloc",
                  CARAT_CALLOC = "nk_carat_instrument_calloc",
                  CARAT_REMOVE_ALLOC = "nk_carat_instrument_free",
                  CARAT_STATS = "nk_carat_report_statistics",
                  CARAT_ESCAPE = "nk_carat_instrument_escapes",
                  CARAT_INIT = "nk_carat_init",
                  ENTRY_SETUP = "_carat_create_allocation_entry", // TODO: remove
                  KERNEL_MALLOC = "_kmem_sys_malloc",
                  KERNEL_FREE = "kmem_sys_free",
                  ANNOTATION = "llvm.global.annotations",
                  NOCARAT = "nocarat";


/*
 * Important/necessary methods/method names to track
 */ 
std::unordered_set<std::string> CARATNames = {
    CARAT_MALLOC, 
    CARAT_REALLOC, 
    CARAT_CALLOC,
    CARAT_REMOVE_ALLOC, 
    CARAT_STATS,
    CARAT_ESCAPE,
    CARAT_INIT,
    ENTRY_SETUP
};

std::unordered_map<std::string, Function *> CARATNamesToMethods;

std::unordered_set<Function *> CARATMethods;

std::unordered_map<KernelAllocID, std::string> IDsToKernelAllocMethods = {
    { KernelAllocID::Malloc, KERNEL_MALLOC } ,
    { KernelAllocID::Free, KERNEL_FREE }
};

std::unordered_map<std::string, Function *> KernelAllocNamesToMethods;

std::unordered_map<Function *, KernelAllocID> KernelAllocMethodsToIDs;

std::unordered_set<Function *> AnnotatedFunctions;