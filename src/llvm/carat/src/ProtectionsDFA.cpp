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

#include "../include/ProtectionsDFA.hpp"

/*
 * ---------- Constructors ----------
 */
ProtectionsDFA::ProtectionsDFA(
    Function *F,
    Noelle *N
) : F(F), N(N) {}


/*
 * ---------- Drivers ----------
 */
void ProtectionsDFA::Compute(void)
{
    /* 
     * Play god
     */ 
    _initializeUniverse();


    /*
     * Apply the Address Checking for Data Custody (AC/DC) DFA
     */
    auto DFE = N->getDataFlowEngine();

    TheResult = DFE.applyForward(
        F, 
        _computeGEN(), 
        _computeKILL(), 
        _initializeIN(), 
        _initializeOUT(), 
        _computeIN(), 
        _computeOUT()
    );

    return;
}


DataFlowResult *ProtectionsDFA::FetchResult(void)
{
    return this->TheResult;
}


/*
 * ---------- Private methods (DFA setup) ----------
 */
std::function<void (Instruction *, DataFlowResult *)> ProtectionsDFA::_computeGEN(void)
{
    auto DFAGen = 
        []
        (Instruction *I, DataFlowResult *Result) -> void {

        /*
         * Handle memory instructions (stores and loads)
         */
        if (true
            && (!isa<StoreInst>(I))
            && (!isa<LoadInst>(I)))
            { return; }


        /*
         * Find pointer to guard
         */ 
        Value *PointerToGuard = nullptr;

        if (isa<StoreInst>(I))
        {
#if STORE_GUARD
            auto NextStoreInst = cast<StoreInst>(I);
            PointerToGuard = NextStoreInst->getPointerOperand();
#endif
        } 
        else 
        {
#if LOAD_GUARD
            auto NextLoadInst = cast<LoadInst>(I);
            PointerToGuard = NextLoadInst->getPointerOperand();
#endif
        }

        if (!PointerToGuard) { return; }


        /*
         * Fetch the GEN[I] set.
         */
        auto &InstGen = Result->GEN(I);


        /*
         * Add the pointer to guard in the GEN[I] set.
         */
        InstGen.insert(PointerToGuard);


        return;
    };


    return DFAGen;
}


std::function<void (Instruction *, DataFlowResult *)> ProtectionsDFA::_computeKILL(void)
{
    /*
     * TOP --- No concept of a KILL set in AC/DC???
     */ 
    auto DFAKill = 
        []
        (Instruction *I, DataFlowResult *Results) -> void {
        return;
    };

    return DFAKill;
}


void ProtectionsDFA::_initializeUniverse(void)
{
    /*
     * TOP --- Initialize IN and OUT sets, fetch ALL values
     * used in @this->F excluding incoming basic blocks from
     * PHINodes and possible external functions from pointers,
     * indirect calls, etc.
     */
    for(auto &B : *F)
    {
        for (auto &I : B)
        {
            TheUniverse.insert(&I);

            for (auto Index = 0; 
                 Index < I.getNumOperands(); 
                 Index++)
            {
                Value *NextOperand = I.getOperand(Index);

                if (false
                    || (isa<Function>(NextOperand))
                    || (isa<BasicBlock>(NextOperand)))
                    { continue; }
   
                TheUniverse.insert(NextOperand);
            }


        }
    }


    /*
     * Save arguments as well
     */  
    for (auto &Arg : F->args()) { TheUniverse.insert(&Arg); }
    

    /*
     * Set state needed for analysis
     */ 
    Entry = &*F->begin();
    First = &*Entry->begin();
    

    return;
}


std::function<void (Instruction *inst, std::set<Value *> &IN)> ProtectionsDFA::_initializeIN(void)
{
    /*
     * TOP --- initialize the IN set to the universe of 
     * values available in @this->F
     */ 
    auto InitIn = 
        [this] 
        (Instruction *I, std::set<Value *> &IN) -> void {
        if (I == First) { return; }
        IN = TheUniverse;
        return;
    };


    return InitIn;
}


std::function<void (Instruction *inst, std::set<Value *> &OUT)> ProtectionsDFA::_initializeOUT(void)
{
    /*
     * TOP --- initialize the OUT set to the universe of 
     * values available in @this->F
     */ 
    auto InitOut = 
        [this]
        (Instruction *I, std::set<Value *> &OUT) -> void {
        OUT = TheUniverse;
        return ;
    };

    return InitOut;
}


std::function<void (Instruction *inst, std::set<Value *> &IN, Instruction *predecessor, DataFlowResult *df)> ProtectionsDFA::_computeIN(void)
{
    /*
     * Define the IN set --- 
     */
    auto ComputeIn = 
        [] 
        (Instruction *I, std::set<Value *> &IN, Instruction *Pred, DataFlowResult *DF) -> void {
        
        auto &OUTPred = DF->OUT(Pred);

        std::set<Value *> tmpIN{};
        std::set_intersection(
            IN.begin(), 
            IN.end(), 
            OUTPred.begin(), 
            OUTPred.end(),  
            std::inserter(tmpIN, tmpIN.begin())
        );
        
        IN = tmpIN;

        return ;

    };


    return ComputeIn;
}


std::function<void (Instruction *inst, std::set<Value *> &OUT, DataFlowResult *df)> ProtectionsDFA::_computeOUT(void)
{
    auto computeOUT = 
        [] 
        (Instruction *inst, std::set<Value *> &OUT, DataFlowResult *df) -> void {

        /*
         * Fetch the IN[inst] set.
         */
        auto& IN = df->IN(inst);

        /*
         * Fetch the GEN[inst] set.
         */
        auto& GEN = df->GEN(inst);

        /*
         * Set the OUT[inst] set.
         */
        OUT = IN;
        OUT.insert(GEN.begin(), GEN.end());

        return ;
    };


    return computeOUT;
}















