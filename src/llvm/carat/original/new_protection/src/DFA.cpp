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

#include "../include/DFA.hpp"

/*
 * ---------- Constructors ----------
 */
DFA::DFA(
    Function *F,
    Noelle *noelle
)
{
    /*
     * Set passed state
     */ 
    this->F = F;
    this->noelle = noelle;
}


/*
 * ---------- Drivers ----------
 */
void DFA::Compute(void)
{
    /* 
     * Play god
     */ 
    _initializeUniverse();


    /*
     * Apply the available CARAT DFA.
     */
    auto dfe = noelle->getDataFlowEngine();

    TheResult = dfe.applyForward(
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


DataFlowResult *DFA::FetchResult(void)
{
    return this->TheResult;
}


/*
 * ---------- Private methods (DFA setup) ----------
 */
std::function<void (Instruction *, DataFlowResult *)> DFA::_computeGEN(void)
{
    auto dfaGEN = 
        []
        (Instruction *inst, DataFlowResult *res) -> void {

        /*
         * Handle store instructions.
         */
        if (true
            && (!isa<StoreInst>(inst))
            && (!isa<LoadInst>(inst)))
            { return; }

        Value *ptrGuarded = nullptr;


        if (isa<StoreInst>(inst))
        {
#if STORE_GUARD
            auto storeInst = cast<StoreInst>(inst);
            ptrGuarded = storeInst->getPointerOperand();
#endif
        } 
        else 
        {
#if LOAD_GUARD
            auto loadInst = cast<LoadInst>(inst);
            ptrGuarded = loadInst->getPointerOperand();
#endif
        }

        if (ptrGuarded == nullptr) { return; }

        /*
         * Fetch the GEN[inst] set.
         */
        auto& instGEN = res->GEN(inst);

        /*
         * Add the pointer guarded in the GEN[inst] set.
         */
        instGEN.insert(ptrGuarded);

        return ;
    };


    return dfaGEN;
}


std::function<void (Instruction *, DataFlowResult *)> DFA::_computeKILL(void)
{
    auto dfaKILL = 
        []
        (Instruction *inst, DataFlowResult *res) -> void {
        return;
    };

    return dfaKILL;
}


void DFA::_initializeUniverse(void)
{
    /*
     * Initialize IN and OUT sets.
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

    for (auto &Arg : F->args()) { TheUniverse.insert(&Arg); }
    
    Entry = &*F->begin();
    First = &*Entry->begin();
    

    return;
}


std::function<void (Instruction *inst, std::set<Value *> &IN)> DFA::_initializeIN(void)
{
    auto initIN = 
        [this] 
        (Instruction *inst, std::set<Value *> &IN) -> void {
        if (inst == First) { return; }
        IN = TheUniverse;
        return ;
    };


    return initIN;
}


std::function<void (Instruction *inst, std::set<Value *> &OUT)> DFA::_initializeOUT(void)
{
    auto initOUT = 
        [this]
        (Instruction *inst, std::set<Value *> &OUT) -> void {
        OUT = TheUniverse;
        return ;
    };

    return initOUT;
}


std::function<void (Instruction *inst, std::set<Value *> &IN, Instruction *predecessor, DataFlowResult *df)> DFA::_computeIN(void)
{
    /*
     * Define the IN and OUT data flow equations.
     */
    auto computeIN = 
        [] 
        (Instruction *inst, std::set<Value *>& IN, Instruction *predecessor, DataFlowResult *df) -> void{
        
        auto& OUTPred = df->OUT(predecessor);

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


    return computeIN;
}


std::function<void (Instruction *inst, std::set<Value *> &OUT, DataFlowResult *df)> DFA::_computeOUT(void)
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















