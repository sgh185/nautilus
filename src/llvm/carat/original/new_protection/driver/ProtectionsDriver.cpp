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

#include "../include/____"

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
        std::set<string> functionsInProgram;
        std::unordered_map<string, int> functionCalls;
        bool modified = false;


        /*
         * Fetch NOELLE.
         */
        auto &Noelle = getAnalysis<Noelle>();


        /*
         * Fetch all the loops of the program.
         */
        auto ProgramLoops = Noelle.getLoops();





        //COUNTERS 
        uint64_t redundantGuard = 0;
        uint64_t loopInvariantGuard = 0;
        uint64_t scalarEvolutionGuard = 0;
        uint64_t nonOptimizedGuard = 0;
        uint64_t callGuardOpt = 0;

        LLVMContext &MContext = M.getContext();
        Type* int64Type = Type::getInt64Ty(MContext);
        Type* int32Type = Type::getInt32Ty(MContext);
        Type* int64PtrType = Type::getInt64PtrTy(MContext, 0);
        ConstantInt* ptrNum = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, 0x22DEADBEEF22, /*bool*/false));
        Constant* numNowPtr = ConstantExpr::getIntToPtr(ptrNum, int64PtrType, false);

        std::unordered_map<Instruction*, pair<Instruction*, Value*>> storeInsts;
        std::map<Function*, BasicBlock*> functionToEscapeBlock;




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
            DFA *TheDFA = new DFA(&F);
            TheDFA->Compute();


            /*
             * Inject guards
             */
            Injector *I = new Injector(&F, TheDFA, storeInsts);
            I->Inject();
        }

        ConstantInt* constantNum = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, 0, /*bool*/false));
        ConstantInt* constantNum2 = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, 0, /*bool*/false));
        ConstantInt* constantNum1 = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, (((uint64_t)pow(2, 64)) - 1), /*bool*/false));
        Type* voidType = Type::getVoidTy(MContext);
        Instruction* nullInst = nullptr;
        GlobalVariable* tempGlob = nullptr; 
        GlobalVariable* lowerBound = new GlobalVariable(M, int64Type, false, GlobalValue::CommonLinkage, constantNum2, "lowerBound", tempGlob, GlobalValue::NotThreadLocal, 0, false);
        GlobalVariable* upperBound = new GlobalVariable(M, int64Type, false, GlobalValue::ExternalLinkage, constantNum1, "upperBound", tempGlob, GlobalValue::NotThreadLocal, 0, false);
        auto mainFunction = Noelle.getEntryFunction();
        for(auto& myPair : storeInsts){
            auto I = myPair.second.first;
            auto singleCycleStore = new StoreInst(constantNum, lowerBound, true, I);
        }

        //Print results
        errs() << "Guard Information\n";
        errs() << "Unoptimized Guards:\t" << nonOptimizedGuard << "\n"; 
        errs() << "Redundant Optimized Guards:\t" << redundantGuard << "\n"; 
        errs() << "Loop Invariant Hoisted Guards:\t" << loopInvariantGuard << "\n"; 
        errs() << "Scalar Evolution Combined Guards:\t" << scalarEvolutionGuard << "\n"; 
        errs() << "Hoisted Call Guards\t" << callGuardOpt << "\n"; 
        errs() << "Total Guards:\t" << nonOptimizedGuard + loopInvariantGuard + scalarEvolutionGuard << "\n"; 


        return true;

    }


    void getAnalysisUsage (AnalysisUsage &AU) const override 
    {
        AU.addRequired<Noelle>();
        return ;
    }


};


char CAT::ID = 0;
static RegisterPass<CAT> X"CARATprotector", "Bounds protection for CARAT";

static CAT* _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if (!_PassMaker) { PM.add(_PassMaker = new CAT()); }}); 
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if (!_PassMaker) { PM.add(_PassMaker = new CAT()); }});
}
