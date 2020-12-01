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

#include "include/DFA.hpp"
#include "include/Injector.hpp"


cl::opt<bool> Dummy("dummy",
                    cl::init(true), /* TODO : Change to false */
                    cl::desc("Builds a dummy protections method"));

namespace 
{

struct CAT : public ModulePass 
{
    static char ID; 

    CAT() : ModulePass(ID) {}


    bool doInitialization (Module &M) override 
    {
        return false;
    }


    bool runOnModule (Module &M) override 
    {
        /*
         * Fetch NOELLE.
         */
        auto &noelle = getAnalysis<Noelle>();


        /*
         * Fetch the runtime function (or build if no 
         * function is present/command line argument
         * says otherwise)
         */ 
        Function *ProtectionsMethod = 
            (Dummy) ? 
            (BuildDummyProtectionsMethod(M)) :
            (M.getFunction("nk_carat_guard_address")) ; /* FIX */


        /*
         * Iterate through each funtion, compute the DFA and inject guards
         */ 
        for (auto &F : M)
        {
            /*
             * Skip empty function bodies --- FIX
             */
            if (F.empty()) { continue; }


            /*
             * Compute DFA for the function
             */
            DFA *TheDFA = new DFA(&F, &noelle);
            TheDFA->Compute();


            /*
             * Inject guards
             */
            Injector *I = new Injector(
                &F, 
                TheDFA->FetchResult(), 
                numNowPtr, 
                ProtectionsMethod,
                &noelle
            );

            I->Inject();  
        }


        return true;
    }


    Function *BuildDummyProtectionsMethod(Module &M)
    {
        /*
         * Set up IRBuilder
         */
        IRBuilder<> ReturnTypeBuilder{M.getContext()};


        /*
         * Build return type --- void type
         */ 
        Type *VoidType = ReturnTypeBuilder.getVoidTy();


        /*
         * Generate function type
         */
        FunctionType *DummyFunctionType = 
            FunctionType::get(
                VoidType,
                None /* Params */,
                false /* IsVarArg */
            );


        /*
         * Build function signature
         */ 
        Function *DummyProtections = Function::Create(
            DummyFunctionType,
            GlobalValue::InternalLinkage,
            Twine("dummy_protections"),
            M
        );


        /*
         * Add optnone, noinline attributes to the dummy handler
         */
        DummyProtections->addFnAttr(Attribute::OptimizeNone);
        DummyProtections->addFnAttr(Attribute::NoInline);


        /*
         * Build an entry basic block and return void
         */ 
        BasicBlock *Entry = 
            BasicBlock::Create(
                M.getContext(), 
                "entry",
                DummyProtections
            );

        IRBuilder<> EntryBuilder{Entry};
        ReturnInst *DummyReturn = EntryBuilder.CreateRetVoid();


        return DummyProtections;
    }


    void getAnalysisUsage (AnalysisUsage &AU) const override 
    {
        AU.addRequired<Noelle>();
        return ;
    }


};


char CAT::ID = 0;
static RegisterPass<CAT> X("CARATprotector", "Bounds protection for CARAT");

static CAT* _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if (!_PassMaker) { PM.add(_PassMaker = new CAT()); }}); 
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if (!_PassMaker) { PM.add(_PassMaker = new CAT()); }});
}
