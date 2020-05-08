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

#include "../include/Protection.hpp"

ProtectionsHandler::ProtectionsHandler(Module *M)
{
    DEBUG_INFO("--- Protections Constructor ---\n");

    // Set state
    this->M = M;
    this->FunctionMap = FunctionMap;
    this->Panic = M->getFunction("panic");
    if (this->Panic == nullptr) { abort(); }

    // Set up data structures
    this->MemoryInstructions = std::unordered_map<Instruction *, pair<Instruction *, Value *>>();
    this->PanicString = M->getGlobalVariable(PANIC_STRING);
    if (this->PanicString == nullptr) { abort(); }

    this->_getAllNecessaryInstructions();
}

void ProtectionsHandler::_getAllNecessaryInstructions()
{
    return;
}

void ProtectionsHandler::Inject()
{
    std::unordered_map<Function *, BasicBlock *> FunctionToEscapeBlock;
    for (auto const &[MI, InjPair] : MemoryInstructions)
    {
        Instruction *InjectionLocation = InjPair.first;
        Value *AdderssToCheck = InjPair.second;

        /*
            %i = InjectionLocation
            
            ---

            ... :
              %0 = icmp addressToCheck vs lower
              %1 = br %0 %continue-block %escape

            contiue-block:
              %2 = icmp addressToCheck vs upper
              %3 = br %3 %continue-block2 %escape

            continue-block2:
              %i = InjectionLocation

            ...
            ...
            
            escape:
              %escape1 = panic ...
              unreachable
            ---
        */

        // ---- NOT FINISHED -----

        // Build escape block IFF an escape block doesn't already exist
        Function *StoreFunction = I->getFunction();
        BasicBlock *EscapeBlock = nullptr;
        if (FunctionToEscapeBlock.find(StoreFunction) == FunctionToEscapeBlock.end())
        {
            // Set up builder
            IRBuilder<> EscapeBuilder = Utils::GetBuilder(StoreFunction, EscapeBlock);

            // Create escape basic block
            EscapeBlock = BasicBlock::Create(StoreFunction->getContext(), "escape", StoreFunction);
            FunctionToEscapeBlock[StoreFunction] = EscapeBlock;

            // Set up arguments for call to panic
            std::vector<Value *> CallArgs;
            CallArgs.push_back(PanicString);
            ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

            // Build call to panic
            CallInst *PanicCall = EscapeBuilder.CreateCall(Panic, LLVMCallArgs)
            
            // Add LLVM unreachable
            UnreachableInst *Unreachable = EscapeBuilder.CreateUnreachable();
        }
        else { EscapeBlock = FunctionToEscapeBlock[StoreFunction]; } // Block already exists

        BasicBlock* oldBlock = I->getParent();
        BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
        BasicBlock* oldBlock1 = BasicBlock::Create(MContext, "", oldBlock->getParent(), newBlock);

        Instruction* unneededBranch = oldBlock->getTerminator();
        if(isa<BranchInst>(unneededBranch)){
            unneededBranch->eraseFromParent();
        }
        Value* compareVal;
        Function* tFp;
        compareVal = addressToCheck;
        const Twine &NameString = ""; 
        Instruction* tempInst = nullptr; 
        CastInst* int64CastInst = CastInst::CreatePointerCast(compareVal, int64Type, NameString, tempInst);
        LoadInst* lowerCastInst = new LoadInst(int64Type, lowerBound, NameString, tempInst);
        LoadInst* upperCastInst = new LoadInst(int64Type, upperBound, NameString, tempInst);
        CmpInst* compareInst = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_ULT, int64CastInst, lowerCastInst, "", oldBlock);
        int64CastInst->insertBefore(compareInst);
        lowerCastInst->insertBefore(compareInst);
        BranchInst* brInst = BranchInst::Create(escapeBlock, oldBlock1, compareInst, oldBlock);

        //CastInst* int64CastInst1 = CastInst::CreatePointerCast(compareVal, int64Type, NameString, tempInst);
        CmpInst* compareInst1 = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_UGT, int64CastInst, upperCastInst, "", oldBlock1);
        //int64CastInst1->insertBefore(compareInst1);
        upperCastInst->insertBefore(compareInst1);
        BranchInst* brInst1 = BranchInst::Create(escapeBlock, newBlock, compareInst1, oldBlock1);
        modified = true;
    }














    Function *CARATEscape = NecessaryMethods[CARAT_ESCAPE];
    LLVMContext &TheContext = CARATEscape->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection

    for (auto MU : MemUses)
    {
        Instruction *InsertionPoint = MU->getNextNode();
        if (InsertionPoint == nullptr)
        {
            errs() << "Not able to instrument: ";
            MU->print(errs());
            errs() << "\n";

            continue;
        }

        // Set up injections and call instruction arguments
        IRBuilder<> MUBuilder = Utils::GetBuilder(MU->getFunction(), InsertionPoint);
        std::vector<Value *> CallArgs;

        // Get pointer operand from store instruction --- this is the
        // only parameter (casted) to the AddToEscapeTable method
        StoreInst *SMU = static_cast<StoreInst>(MU);
        Value *PointerOperand = SMU->getPointerOperand();

        // Pointer operand casted to void pointer
        Value *PointerOperandCast = MUBuilder.CreatePointerCast(PointerOperand, VoidPointerType);

        // Add all necessary arguments, convert to LLVM data structure
        CallArgs.push_back(PointerOperandCast);
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build the call instruction to CARAT escape
        CallInst *AddToAllocationTable = MUBuilder.CreateCall(CARATEscape, LLVMCallArgs);
    }

    return;
}