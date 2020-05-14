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

#include "./include/Utils.hpp"

namespace
{
struct CAT : public ModulePass
{
    static char ID;

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
        Utils::ExitOnInit();

        return false;
    }

    bool runOnModule(Module &M) override
    {
        // --- SET UP ---

        // Get methods necessary for injection --- if at least
        // one method doesn't exist --- abort
        for (auto InjectionName : ImportantMethodNames)
        {
            Function *F = M.getFunction(InjectionName);
            if (F == nullptr)
            {
                errs() << "Aborting --- could not find "
                       << InjectionName << "\n";
                abort();
            }

            NecessaryMethods[InjectionName] = F;
        }

        // Storing method names to avoid in the bitcode source
        // std::set<std::string> FunctionsToAvoid;
        // std::unordered_map<std::string, int> FunctionMap;
        // populateLibCallMap(&FunctionMap);

        // --- END SET UP ---

        // --- Allocation tracking ---
        // AllocationHandler *AH = new AllocationHandler(&M, &FunctionMap);
        // AH->Inject();

        // // --- Escapes tracking ---
        // EscapesHandler *EH = new EscapesHandler(&M, &FunctionMap);
        // EH->Inject(); // Only memory uses

        // // --- Protection ---
        // ProtectionsHandler *PH = new ProtectionsHandler(&M, &FunctionMap);
        // PH->Inject();

        return false;
    }
};
    
char CAT::ID = 0;
static RegisterPass<CAT> X("karat", "KARAT");

static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());} }); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());} }); // ** for -O0
}
