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
     * Check command line arguments
     */
    if (NoRestrictions) return;


    /*
     * TOP --- Find ALL indirect calls and calls to external
     * functions across the entire function via visitor
     */
    this->visit(&F);


    return;
}


void RestrictionsHandler::PinAllEscapingPointers(void)
{

}


void RestrictionsHandler::PrintAnalysis(void)
{
    /*
     * Check command line arguments
     */
    if (NoRestrictions) return;


    errs() << F->getName() << "\n";


    /*
     * Print @this->IndirectCalls
     */
    errs() << "--- IndirectCalls ---\n";
    for (auto Call : IndirectCalls)
        errs() << "\t" << *Call << "\n";


    /*
     * Print @this->ExternalFunctionCalls
     */
    errs() << "--- ExternalFunctionCalls ---\n";
    for (auto Call : ExternalFunctionCalls)
        errs() << "\t" << *Call << "\n";


    /*
     * Print @this->TrackedCallsNotAffectingMemory
     */
    errs() << "--- TrackedCallsNotAffectingMemory ---\n";
    for (auto Call : TrackedCallsNotAffectingMemory)
        errs() << "\t" << *Call << "\n";


    /*
     * Print @this->TrackedCallsMayAffectingMemory
     */
    errs() << "--- TrackedCallsMayAffectingMemory ---\n";
    for (auto Call : TrackedCallsMayAffectingMemory)
        errs() << "\t" << *Call << "\n";


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


    /*
     * Fetch parent/caller
     */
    Function *Parent = I.getFunction();


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
        IndirectCalls[Parent].insert(&I);
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
        ExternalFunctionCalls[Parent].insert(&I);
        AnalyzeArguments |= true;
    }


    /*
     * Check if we need to analyze arguments further
     */
    if (!AnalyzeArguments) return;

    
    /*
     * Understand how @I interacts with memory and its args. This
     * can be accomplished by analyzing the attributes assigned to
     * the function signature and each argument.
     * 
     * First, analyze call instructions that can be identifed to 
     * only either read memory or not interact with memory.
     *
     * CAVEAT --- Certain functions are backstopped b/c they are 
     * arbitrarily difficult to analyze (e.g. printf, since it's 
     * a variadic function) but we (users) know their semantics.
     * TODO --- Implement properly
     */
    if (false
        || I.onlyReadsMemory()
        || (Callee && (Callee->getName() == "printf")) /* FIX */) 
    {
        TrackedCallsNotAffectingMemory[Parent].insert(&I);
        return;
    }


    /*
     * If this determination can't be made, examine each operand
     * of the call inst and/or arguments of the callee separately
     *
     * NOTE --- Check the following attributes for each argument:
     * - readonly
     * - readnone
     * - nocapture (suspicious, but keeping for now ...)
     */
    std::unordered_set<Value *> EscapingOperands;
    for (unsigned ArgNo = 0 ; ArgNo < I.getNumArgOperands() ; ArgNo++)
    {
        /*
         * Check if next argument only reads/does not interact with memory
         */
        if (false
            || I.onlyReadsMemory(ArgNo)
            || I.doesNotCapture(ArgNo)) continue;


        /*
         * No positive attribute analysis, so check operand type manually, 
         * look for a possible pointer type passed in the argument. 
         * 
         * Essentially, may contain pointer type = may modify arg memory
         *
         * Track the corresponding operand as escaping for later processing
         */
        Value *Operand = I.getArgOperand(ArgNo);
        bool MayModifyMemory |= _mayContainPointerType(Operand->getType());
        if (MayModifyMemory) EscapingOperands.insert(Operand);
    }


    /*
     * Track the instruction accordingly --- FIX
     */
    if (!(EscapingOperands.size())) TrackedCallsNotAffectingMemory[Parent].insert(&I);
    else 
    {
        TrackedCallsMayAffectingMemory[Parent].insert(&I);
        for (auto Operand : EscapingOperands)
            PointersEscapingViaTrackedCalls[&I].insert(Operand);
    }


    return;
}


/*
 * ---------- Private methods ----------
 */
bool RestrictionsHandler::_mayContainPointerType(Type *T)
{
    /*
     * TOP --- Compute if @T contains a pointer type
     * - Pointer/vector of pointer types
     * - Struct with reducible types that are pointers/vector of 
     *   pointer types
     * - Array with pointer type, or struct with 
     */
     
    /*
     * Reduce @T to a scalar type --- assert no nested vector types
     */
    Type *ScalarType = T->getScalarType();
    assert(
        true 
        && !(ScalarType->isVectorTy())
        && "_mayContainPointerType: Nested vector type! Can't handle!"
    );


    /*
     * @T does not contain a pointer type if it's an 
     * integer, FP, function, or void type
     */
    if (false
        || T->isIntOrIntVectorTy()
        || T->isFPOrFPVectorTy()
        || T->isFunctionTy()
        || T->isVoidTy()) return false;


    /*
     * @T is definitely a pointer type if LLVM can tell at face value
     */
    if (T->isPtrOrPtrVectorTy()) return true;


    /*
     * At this point, use the scalar type for further analysis, where
     * the only types handled from here on are aggregate types
     */ 
    assert(
        true 
        && !(ScalarType->isAggregateType())
        && "_mayContainPointerType: Non-aggregate type in further analysis!"
    );


    /*
     * Parse through ONLY struct and array types based on TypeID
     */
    Type::TypeID TID = ScalarType->getTypeID();
    
    
    /*
     * Recursively find possible pointer member types
     */
    switch (TID)
    {
        /*
         * Fetch struct type, loop through all element types, recurse
         */ 
        case Type::StructTyID:
        {
            bool MayContainPointerType = false;
            StructType *StructTy = cast<StructType>(ScalarType);
            for (auto ElementTy : StructTy->elements()) 
                MayContainPointerType |= _mayContainPointerType(ElementTy);
            
            return MayContainPointerType;
        }


        /*
         * Fetch array type, recurse on element type
         */ 
        case Type::ArrayTyID:
        {
            ArrayType *ArrayTy = cast<ArrayType>(ScalarType);
            Type *ElementTy = ArrayTy->getElementType();
            return _mayContainPointerType(ElementTy);
        }


        /*
         * Otherwise, abort
         */ 
        default:
        {
            errs() << "_mayContainPointerType: Cannot handle type: "
                   << *ScalarType << "...\n";
            abort();            
        }
    }


    return false; /* Unlikely */
}

