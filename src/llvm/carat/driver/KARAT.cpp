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

#include "autoconf.h"

#ifdef NAUT_CONFIG_USE_NOELLE
#include "./include/Protections.hpp"
#else
#include "./include/Escapes.hpp"
#endif


#define FetchAllocMethods(type) \
    for (auto const &[ID, Name] : IDsTo##type##AllocMethods) \
    { \
        Function *AllocMethod = Utils::GetMethod(&M, Name); \
        type##AllocNamesToMethods[Name] = AllocMethod; \
        type##AllocMethodsToIDs[AllocMethod] = ID; \
    } \


namespace
{
struct CAT : public ModulePass
{
    static char ID;

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
        /*
         * Debugging
         */  
        Utils::ExitOnInit();


        /*
         * Fetch the "nocarat" annotation attribute --- necessary
         * to find user-marked functions in kernel code
         */
        GlobalVariable *GV = M.getGlobalVariable(ANNOTATION);
        assert(!!GV && "doInitialization: 'nocarat' attribute not found!");


        /*
         * Fetch the annotated functions
         */
        Utils::FetchAnnotatedFunctions(GV);

        
        /*
         * Mark all annotated functions as noinline --- we
         * don't want marked functions to become instrumented
         * indirectly via inlining
         */ 
        for (auto AF : AnnotatedFunctions)
        {
            /*
             * Sanity check the annotated function
             */
            assert(!!AF && "Annotated function is nullptr!");
            

            /*
             * Add attribute
             */ 
            AF->addFnAttr(Attribute::NoInline);
        }


        /*
         * Now fetch all CARAT methods, stash them
         */
        for (auto Name : CARATNames)
        {
            Function *CARATMethod = Utils::GetMethod(&M, Name);
            CARATNamesToMethods[Name] = CARATMethod;
            CARATMethods.insert(CARATMethod);
        }


        /*
         * Now fetch all kernel allocation methods, stash them
         */
        if (InstrumentingUserCode) FetchAllocMethods(User)
        else FetchAllocMethods(Kernel)


        return false;
    }


    bool runOnModule(Module &M) override
    {
        /*
         * Debugging
         */  
        Utils::ExitOnInit();


        if (Debug || true)
        {
            /*
             * Output all intrinsics that exist in the module
             */ 
            errs() << "Now intrinsics!\n";
            for (auto &F : M) if (F.isIntrinsic()) errs() << F.getName() << "\n";


            /*
             * Vet allocation methods (of kernel OR userspace)
             */ 
            Utils::VetAllocMethods();
        }


        /*
         * --- Perform all CARAT instrumentation on the code ---
         */ 
#if NAUT_CONFIG_USE_NOELLE
        if (!NoProtections)
        {
            /*  
             * Fetch Noelle
             */
            Noelle &NoelleAnalysis = getAnalysis<Noelle>();


            /*
             * Protections
             */ 
            ProtectionsHandler PH = ProtectionsHandler(&M, &NoelleAnalysis);
            PH.Protect();
        }
#endif


        /*
         * Allocation tracking
         */ 
        AllocationHandler AH = AllocationHandler(&M);
        AH.Inject();


        /*
         * Escapes tracking
         */ 
        EscapesHandler EH = EscapesHandler(&M);
        EH.Inject(); // Only memory uses


        return true;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const override
    {   

#if NAUT_CONFIG_USE_NOELLE
        /*
         * Use NOELLE IFF we need protections only
         */
        if (!NoProtections) AU.addRequired<Noelle>();
#endif

        return;
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
