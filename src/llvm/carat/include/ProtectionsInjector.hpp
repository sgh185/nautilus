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
 * Copyright (c) 2021, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2021, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2021, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2021, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2021, The V3VEE Project  <http://www.v3vee.org> 
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

#include "ProtectionsDFA.hpp"


class GuardInfo 
{

public:
    GuardInfo(
        Instruction *IL,
        Value *PTG,
        bool IW
    ) : InjectionLocation(IL), PointerToGuard(PTG), IsWrite(IW) {};

private:
    Instruction *InjectionLocation;
    Value *PointerToGuard;
    bool IsWrite; /* i.e. store=TRUE, load=FALSE */

};



class ProtectionsInjector : public InstVisitor<ProtectionsInjector>
{

public:

    /*
     * Constructors
     */ 
    ProtectionsInjector(
        Function *F, 
        DataFlowResult *DFR, 
        Value *NonCanonical,
        Noelle *noelle,
        Function *ProtectionsMethod
    );


    /*
     * Drivers
     */ 
    void Inject (void);


    /*
     * Visitors
     */ 
    void visitCallInst(CallInst &I);
    void visitStoreInst(StoreInst &I);
    void visitLoadInst(LoadInst &I);
    void visitInvokeInst(InvokeInst &I);


private:

    /*
     * Passed state
     */ 
    Function *F, *ProtectionsMethod;
    DataFlowResult *DFR;
    Value *NonCanonical;
    Noelle *noelle;


    /*
     * New analysis state
     */ 
    std::unordered_map<BasicBlock *, LoopDependenceInfo *> BasicBlockToLoopMap;

    Instruction *First;

    bool AllocaOutsideEntry=false;

    std::unordered_map<Function *, bool> InstrumentedFunctions;

    std::unordered_map<
        Instruction *, /* The load/store/call for which the guard is generated --- SYMBOLIC */
        GuardInfo * /* The information needed to generate the guard (see above) */
    > InjectionLocations;


    /*
     * Counters/Statistics
     */     
    uint64_t redundantGuard = 0;
    uint64_t loopInvariantGuard = 0;
    uint64_t scalarEvolutionGuard = 0;
    uint64_t nonOptimizedGuard = 0;
    uint64_t callGuardOpt = 0;


    /*
     * Private methods
     */ 
    void _doTheBusiness(void);

    void _findInjectionLocations(void);

    void _doTheInject(void);

    bool _optimizeForLoopInvariance(
        LoopDependenceInfo *NestedLoop,
        Instruction *I, 
        Value *PointerOfMemoryInstruction, 
        bool IsWrite
    );

    bool _optimizeForInductionVariableAnalysis(
        LoopDependenceInfo *NestedLoop,
        Instruction *I, 
        Value *PointerOfMemoryInstruction, 
        bool IsWrite
    );

    std::function<void (Instruction *inst, Value *pointerOfMemoryInstruction, bool isWrite)> _findPointToInsertGuard(void);

    void _allocaOutsideFirstBBChecker(void);

    template<typename MemInstTy>
    void _invokeLambda(
        MemInstTy *I,
        bool IsWrite
    );

    void _printGuards(void);
};