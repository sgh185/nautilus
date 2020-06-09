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

// Function names to inject
const string CARAT_MALLOC = "AddToAllocationTable",
             CARAT_REALLOC = "HandleReallocInAllocationTable",
             CARAT_CALLOC = "AddCallocToAllocationTable",
             CARAT_REMOVE_ALLOC = "RemoveFromAllocationTable",
             CARAT_STATS = "ReportStatistics",
             CARAT_ESCAPE = "AddToEscapeTable",
             PANIC_STRING = "LLVM_panic_string",
             LOWER_BOUND = "lower_bound",
             UPPER_BOUND = "upper_bound";

// Important/necessary methods/method names to track
unordered_map<string, Function *> NecessaryMethods = unordered_map<string, Function *>();
const vector<string> ImportantMethodNames = {CARAT_MALLOC, 
                                             CARAT_REALLOC, 
                                             CARAT_CALLOC,
                                             CARAT_REMOVE_ALLOC, 
                                             CARAT_STATS,
                                             CARAT_ESCAPE};

const unordered_map<string, int> TargetMethods = {
    { "kmem_malloc" : 2 }, 
    { "kmem_mallocz" : 2 }, 
    { "kmem_realloc" : 3 },
    { "kmem_free" : 1 }
};

// Other methods --- FIX --- NEED TO REFACTOR
uint64_t findStructSize(Type *sType)
{
    uint64_t size = 0;
    for (int i = 0; i < sType->getStructNumElements(); i++)
    {
        if (sType->getStructElementType(i)->isArrayTy())
        {
            size = size + findArraySize(sType->getStructElementType(i));
        }
        else if (sType->getStructElementType(i)->isStructTy())
        {
            size = size + findStructSize(sType->getStructElementType(i));
        }
        else if (sType->getStructElementType(i)->getPrimitiveSizeInBits() > 0)
        {
            size = size + (sType->getStructElementType(i)->getPrimitiveSizeInBits() / 8);
        }
        else if (sType->getStructElementType(i)->isPointerTy())
        {
            //This is bad practice to just assume 64-bit system... but whatever
            size = size + 8;
        }
        else
        {
            errs() << "Error(Struct): Cannot determine size:" << *(sType->getStructElementType(i)) << "\n";
            return 0;
        }
    }

    DEBUG_INFO("Returning: " + to_string(size) + "\n");

    return size;
}

uint64_t findArraySize(Type *aType)
{
    Type *insideType;
    uint64_t size = aType->getArrayNumElements();

    DEBUG_INFO("Num elements in array: " + to_string(size) + "\n");

    insideType = aType->getArrayElementType();
    if (insideType->isArrayTy())
    {
        size = size * findArraySize(insideType);
    }
    else if (insideType->isStructTy())
    {
        size = size * findStructSize(insideType);
    }
    else if (insideType->getPrimitiveSizeInBits() > 0)
    {
        size = size * (insideType->getPrimitiveSizeInBits() / 8);
    }
    else if (insideType->isPointerTy())
    {
        size = size + 8;
    }
    else
    {
        errs() << "Error(Array): cannot determing size: " << *insideType << "\n";
        return 0;
    }

    DEBUG_INFO("Returning: " + to_string(size) + "\n");

    return size;
}