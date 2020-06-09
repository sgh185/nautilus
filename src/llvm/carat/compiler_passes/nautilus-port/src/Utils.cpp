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

#include "../include/Utils.hpp"

using namespace Utils;
using namespace Debug;

/*
 * ExitOnInit
 * 
 * Register pass, execute doInitialization method but do not perform
 * any analysis or transformation --- exit in runOnModule --- mostly
 * for testing scenarios
 * 
 */

void Utils::ExitOnInit()
{
    if (FALSE)
    {
        errs() << "Exiting KARAT Transforms ...\n";
        exit(0);
    }
}


/*
 * GetBuilder
 * 
 * Generates a specific IRBuilder instance that is fitted with 
 * the correct debug location --- necessary for injections 
 * into the Nautilus bitcode
 * 
 */

IRBuilder<> Utils::GetBuilder(Function *F, Instruction *InsertionPoint)
{
    IRBuilder<> Builder{InsertionPoint};
    Instruction *FirstInstWithDBG = nullptr;

    for (auto &I : instructions(F))
    {
        if (I.getDebugLoc())
        {
            FirstInstWithDBG = &I;
            break;
        }
    }

    if (FirstInstWithDBG != nullptr)
        Builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());

    return Builder;
}

IRBuilder<> Utils::GetBuilder(Function *F, BasicBlock *InsertionPoint)
{
    IRBuilder<> Builder{InsertionPoint};
    Instruction *FirstInstWithDBG = nullptr;

    for (auto &I : instructions(F))
    {
        if (I.getDebugLoc())
        {
            FirstInstWithDBG = &I;
            break;
        }
    }

    if (FirstInstWithDBG != nullptr)
        Builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());

    return Builder;
}

/*
 * GatherAnnotatedFunctions
 * 
 * A user can mark a function in Nautilus with a function attribute to
 * prevent injections into the function. For example, it would be unwise
 * to inject calls to nk_time_hook_fire into nk_time_hook_fire itself. Users
 * should use the annotate attribute with the string "nohook" in order 
 * to prevent injections.
 * 
 * This method parses the global annotations array in the resulting bitcode
 * and collects all functions that have been annotated with the "nohook"
 * attribute. These functions will not have injections.
 * 
 */

void Utils::GatherAnnotatedFunctions(GlobalVariable *GV, 
                                     vector<Function *> &AF)
{
    // First operand is the global annotations array --- get and parse
    // NOTE --- the fields have to be accessed through VALUE->getOperand(0),
    // which appears to be a layer of indirection for these values
    auto *AnnotatedArr = cast<ConstantArray>(GV->getOperand(0));

    for (auto OP = AnnotatedArr->operands().begin(); OP != AnnotatedArr->operands().end(); OP++)
    {
        // Each element in the annotations array is a ConstantStruct --- its
        // fields can be accessed through the first operand (indirection). There are two
        // fields --- Function *, GlobalVariable * (function ptr, annotation)

        auto *AnnotatedStruct = cast<ConstantStruct>(OP);
        auto *FunctionAsStructOp = AnnotatedStruct->getOperand(0)->getOperand(0);         // first field
        auto *GlobalAnnotationAsStructOp = AnnotatedStruct->getOperand(1)->getOperand(0); // second field

        // Set the function and global, respectively. Both have to exist to
        // be considered.
        Function *AnnotatedF = dyn_cast<Function>(FunctionAsStructOp);
        GlobalVariable *AnnotatedGV = dyn_cast<GlobalVariable>(GlobalAnnotationAsStructOp);

        if (AnnotatedF == nullptr || AnnotatedGV == nullptr)
            continue;

        // Check the annotation --- if it matches the ANNOTATION global in the
        // pass --- push back to apply (X) transform as necessary later
        ConstantDataArray *ConstStrArr = dyn_cast<ConstantDataArray>(AnnotatedGV->getOperand(0));
        if (ConstStrArr == nullptr)
            continue;

        if (ConstStrArr->getAsCString() != NOCARAT)
            continue;

        AF.push_back(AnnotatedF);
    }

    // Debug::PrintFNames(AF);
    for (auto F : AF)
    {
        errs() << "Annotated: " + F->getName() << "\n";
    }

    return;
}