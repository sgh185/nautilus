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

#include "../include/Restrictions.hpp"

/*
 * ---------- Constructors ----------
 */ 
RestrictionsHandler::RestrictionsHandler(Module *M) : M(M) {}


/*
 * ---------- Drivers ----------
 */
void RestrictionsHandler::AnalyzeAllCalls(void)
{
    /*
     * TOP --- Find ALL indirect calls and calls to external
     * functions across the entire module via visitor
     */
    for (auto &F : *M) 
        if (Utils::IsInstrumentable(F))
            this->visit(&F);


    return;
}


void RestrictionsHandler::PrintAnalysis(void)
{
    /*
     * Print @this->IndirectCalls
     */
    errs() << "--- IndirectCalls ---\n";
    for (auto const &[F, Calls] : IndirectCalls)
    {
        errs() << "\n" << F->getName() << "\n";

        for (auto Call : Calls)
            errs() << "\t" << *Call << "\n";
    }



    /*
     * Print @this->ExternalFunctionCalls
     */
    errs() << "--- ExternalFunctionCalls ---\n";
    for (auto const &[F, Calls] : ExternalFunctionCalls)
    {
        errs() << "\n" << F->getName() << "\n";

        for (auto Call : Calls)
            errs() << "\t" << *Call << "\n";
    }


    return;
}


/*
 * ---------- Visitors ----------
 */
void RestrictionsHandler::visitCallInst(CallInst &I)
{
    /*
     * TOP --- Fetch and analyze the callee and arguments of @I
     */

    /*
     * Fetch callee
     */
    Function *Callee = I.getCalledFunction();


    // errs() << I << "\n";


    /*
     * Vet callee:
     * 1) If @I is an indirect call, add to @this->IndirectCalls
     * 2) Otherwise, if the callee of @I is a valid, empty function 
     *    that is NOT an intrinsic call and NOT inline assembly, then
     *    add to @this->ExternalFunctionCalls
     *
     * If the callee falls into at least one of these categories,
     * then we must analyze the arguments of @I further
     */
    bool AnalyzeArguments = false;
    if (I.isIndirectCall()) /* <Condition 1.> */
    {
        IndirectCalls[I.getFunction()].insert(&I);
        AnalyzeArguments |= true;
    }
    else if (
        true 
        && !(I.isInlineAsm())
        && Callee
        && Callee->empty()
        && !(Callee->isIntrinsic())
    ) /* <Condition 2.> */
    {
        ExternalFunctionCalls[I.getFunction()].insert(&I);
        AnalyzeArguments |= true;
    }


    /*
     * Check if we need to analyze arguments further
     */
    if (!AnalyzeArguments) return;

    // ...

    return;
}

