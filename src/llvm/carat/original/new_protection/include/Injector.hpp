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
 * Authors: Drew Kersnar, Souradip Ghosh, 
 *          Brian Suchy, Simone Campanoni, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#pragma once

#include "Configurations.hpp"

class Injector
{

public:

    /*
     * Constructors
     */ 
    // TODO: Pass in more globals
    Injector(
        Function *F, 
        DFA *TheDFA, 
        Constant* numNowPtr,
        std::unordered_map<Instruction*, pair<Instruction*, Value*>> storeInsts
        );


    /*
     * Drivers
     */ 
    void Inject(void);


private:

    /*
     * Passed state
     */ 
    // TODO: add more globals
    Function *F;
    DFA *TheDFA;
    Constant* numNowPtr;
    // &Noelle, 
    // &programLoops, 
    std::unordered_map<Instruction*, pair<Instruction*, Value*>> storeInsts;


    /*
     * Counters
     */     
    uint64_t redundantGuard = 0;
    uint64_t loopInvariantGuard = 0;
    uint64_t scalarEvolutionGuard = 0;
    uint64_t nonOptimizedGuard = 0;
    uint64_t callGuardOpt = 0;



    /*
     * Private methods
     */ 
    std::function<void (Instruction *inst, Value *pointerOfMemoryInstruction)> _findPointToInsertGuard(void);

    bool allocaOutsideFirstBB();

    void printGuards();
};