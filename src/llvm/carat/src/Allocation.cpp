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

#include "../include/Allocation.hpp"


/*
 * ---------- Constructors ----------
 */ 
AllocationHandler::AllocationHandler(Module *M)
{
    DEBUG_ERRS << "--- Allocation Constructor ---\n";


    /*
     * Set state
     */ 
    this->M = M;
    this->Target = M->getFunction(CARAT_GLOBALS_TARGET);
    assert(!!(this->Target) 
           && "AllocationHandler::AllocationHandler: Couldn't find nk_carat_init!");


    /*
     * Perform initial processing
     */ 
    this->_getAllNecessaryInstructions();
    this->_getAllGlobals();
}


/*
 * ---------- Drivers ----------
 */ 
void AllocationHandler::Inject()
{
    /*
     * TOP --- Instrument all globals, memory allocations
     * (malloc) and memory deallocations (frees)
     */ 

    /*
     * Instrument globals
     */ 
    if (!NoGlobals) InstrumentGlobals();


    /*
     * Instrument allocations
     */ 
    if (!NoMallocs)
    {
        /*
         * Instrument system mallocs
         */ 
        InstrumentMallocs(
            AllocID::SysMalloc,
            0 /* Size operand no */
        );


        /*
         * Instrument aspace/allocator mallocs
         */ 
        InstrumentMallocs(
            AllocID::ASpaceMalloc,
            1 /* Size operand no */
        );
    } 
    if (!NoFrees) 
    {
        /*
         * Instrument system frees
         */ 
        InstrumentFrees(
            AllocID::SysFree,
            0 /* Pointer operand no */
        );


        /*
         * Instrument aspace/allocator frees
         */ 
        InstrumentFrees(
            AllocID::ASpaceFree,
            1 /* Pointer operand no */
        );
    }


    /*
     * Verify transformations
     */ 
    Utils::Verify(*M);


    return;
}


/*
 * ---------- Private methods ----------
 */ 
void AllocationHandler::_getAllNecessaryInstructions()
{
    /*
     * Instrument "malloc"s and "free"s per function
     */ 
    for (auto &F : *M)
    {
        /*
         * Skip if non-instrumentable
         */ 
        if (!(Utils::IsInstrumentable(F))) { continue; }


        /*
         * Debugging
         */ 
        DEBUG_ERRS << "Entering function " << F.getName() << "\n";


        /*
         * Iterate
         */ 
        for (auto &B : F)
        {
            for (auto &I : B)
            {
                /*
                 * Analyze all call instructions --- if a call is 
                 * found to have a "malloc" or "free" callee, mark
                 * the instruction for instrumentation
                 */ 
                
                /*
                 * Ignore all other instructions
                 */ 
                if (!isa<CallInst>(&I)) { continue; }


                /*
                 * Fetch the call instruction
                 */ 
                CallInst *NextCall = cast<CallInst>(&I);


                /*
                 * Fetch the callee
                 */ 
                Function *Callee = NextCall->getCalledFunction();


                /*
                 * If the callee isn't a "malloc" or "free",
                 * ignore the call instruction
                 * 
                 * NOTE --- THIS IGNORES INDIRECT CALLS --- FIX
                 */ 
                if (KernelAllocMethodsToIDs.find(Callee) == KernelAllocMethodsToIDs.end()) { continue; }


                /*
                 * Fetch the AllocID, mark each call for instrumentation
                 */ 
                InstructionsToInstrument[KernelAllocMethodsToIDs[Callee]].insert(NextCall);
            }
        }
    }

    
    return;
}


bool AllocationHandler::_isGlobalInstrumentable(GlobalValue &Global)
{
    /*
     * TOP --- a decision-tree-esque way of determining
     * if a global value should be instrumentable or not
     * 
     * TRUE=Instrumentable
     * FALSE=NotInstrumentable
     */


    /*
     * Check the following:
     * 1)
     * Check linkage type --- if there is a private linkage,
     * we cannot instrument (common in Nautilus)
     * 
     * Private objects are necessarily not lowered to the 
     * symbol table at the end of compilation
     *
     * 2)  
     * If @Global does not have an exact definition (i.e. a variable
     * was declared as an 'extern' but its value was never resolved), 
     * then we cannot instrument it because the symbol table will mark
     * the symbol with size 0 and no classification (not O, F, etc.)
     * 
     * 3) 
     * We cannot instrument the global carat context itself
     * 
     * 4)
     * We cannot instrument the global pointer (that points to
     * a non-canonical address) used for protections checks
     */ 
    if (false
        || Global.hasPrivateLinkage()
        || !(Global.hasExactDefinition())
        || (Global.getName() == "global_carat_context")
        || (Global.getName() == "non_canonical")) return false;


    /*
     * Check the section that @Global will be assigned to. If 
     * there is no discernable section at compile-time, we need
     * to continue analyzing. However, if there IS an assigned
     * section --- we can always instrument @Global as long as 
     * the section isn't the middle-end metadata ("llvm.metadata")
     */ 
    if (Global.hasSection()) return (Global.getSection() != "llvm.metadata");


    /*
     * NOTE --- Many globals have internal linkage --- which can
     * allow the compiler to resolve @Global and other globals
     * with the same value (i.e. unnamed_addr values) as one 
     * single global in the symbol table. Whether or not a global
     * will be resolved in this way is impossible to know in the 
     * middle-end. The runtime needs to handle these globals differently
     */ 


    return true;
}


void AllocationHandler::_getAllGlobals()
{
    /*
     * TOP --- Iterate through all global variables, calculate
     * size, mark for instrumentation if possible
     */ 

    /*
     * Build llvm::DataLayout object to determine info useful
     * to calculate struct sizes, pointer widths, etc.
     */ 
    DataLayout TheLayout = DataLayout(M);


    /*
     * Iterate through globals list
     */ 
    for (auto &Global : M->getGlobalList())
    {
        /*
         * Ignore globals that can't be instrumented
         */ 
        if (!_isGlobalInstrumentable(Global)) continue;


        /*
         * Sanity check each global --- must be of a  
         * pointer type (invariant)
         */  
        assert(Global.getType()->isPointerTy() &&
               "_getAllGlobals: Found a non-pointer typed global!");


        /*
         * Fetch type of global variable
         */ 
        Type *GlobalTy = Global.getValueType();


        /*
         * Calculate the size of the global variable
         */ 
        uint64_t GlobalSize = Utils::CalculateObjectSize(GlobalTy, &TheLayout);


        /*
         * Store the [global : {size, ID}] mapping,
         * increment the next global variable ID
         */ 
        Globals[&Global] = { GlobalSize, NextGlobalID };
        NextGlobalID++;


        /*
         * Debugging
         */ 
        DEBUG_ERRS << "Global: " << Global << "\n"
                   << "Type: " << *GlobalTy << "\n" 
                   << "TypeID: " << GlobalTy->getTypeID() << "\n"
                   << "GlobalID: " << NextGlobalID - 1 << "\n"
                   << "Linkage: " << Global.getLinkage() << "\n"
                   << "Visibility: " << Global.getVisibility() << "\n"
                   << "hasExactDefinition()" << Global.hasExactDefinition() << "\n"
                   << "Section: " << Global.getSection() << "\n"
                   << "Size: " << GlobalSize << "\n\n";
        

        /*
         * Output a warning if the global is thread local 
         * --- in the future, we want to track these variables 
         * in a special way
         */ 
        if (Global.isThreadLocal())
        {
            errs() << "WARNING (ThreadLocal): " 
                   << Global << "\n";
        }
    }


    return;
}


void AllocationHandler::InstrumentGlobals()
{
    /*
     * TOP --- Instrument each global variable --- inject
     * instrumentation into "nk_carat_init"
     */ 

    /*
     * Fetch insertion point as the terminator of "nk_carat_init"
     */
    Instruction *InsertionPoint = Target->back().getTerminator();
    assert(isa<ReturnInst>(InsertionPoint)
           && "InstrumentGlobals: Back block terminator of 'nk_carat_init' is not return!");


    /*
     * Set up IRBuilder
     */  
    IRBuilder<> TargetBuilder = 
        Utils::GetBuilder(
            Target, 
            InsertionPoint
        );


    /*
     * Set up for injections
     */ 
    Type *VoidPointerType = TargetBuilder.getInt8PtrTy();
    Function *CARATGlobalMalloc = CARATNamesToMethods[CARAT_GLOBAL_MALLOC];


    /*
     * Iterate through all globals to be intrumented
     */
    for (auto const &[GV, Info] : Globals)
    {
        /*
         * Deconstruct the "Info" pair
         */  
        uint64_t Length = Info.first,
                 ID = Info.second;


        /*
         * If there is a global with length=0, skip --- FIX
         */ 
        if (Length == 0) 
        { 
            errs() << "Skipping global: "
                   << *GV << "\n";

            continue; 
        }


        /*
         * Build void pointer cast for current global --- necessary
         * to process in the CARAT kernel runtime
         */ 
        Value *PointerCast = 
            TargetBuilder.CreatePointerCast(
                GV, VoidPointerType
            );


        /*
         * Set up call parameters
         */ 
        ArrayRef<Value *> CallArgs = {
            PointerCast,
            TargetBuilder.getInt64(Length),
            TargetBuilder.getInt64(ID)
        };


        /*
         * Inject
         */ 
        CallInst *Instrumentation = 
            TargetBuilder.CreateCall(
                CARATGlobalMalloc, 
                CallArgs
            );
    }


    return;
}


void AllocationHandler::InstrumentMallocs(
    AllocID MallocTypeID,
    unsigned SizeOperandNo
)
{
    /*
     * Set up for injection
     */ 
    Function *CARATMalloc = CARATNamesToMethods[CARAT_MALLOC];

    IRBuilder<> TypeBuilder{M->getContext()};
    Type *VoidPointerType = TypeBuilder.getInt8PtrTy(),
         *Int64Type = TypeBuilder.getInt64Ty();


    /*
     * Instrument all collected "malloc"ss
     */ 
    for (auto NextMalloc : InstructionsToInstrument[MallocTypeID])
    {
        /*
         * Debugging
         */ 
        errs() << "NextMalloc: " << *NextMalloc << "\n";
        
    
        /*
         * Set up insertion point
         */ 
        Instruction *InsertionPoint = NextMalloc->getNextNode();
        assert(!!InsertionPoint 
               && "InstrumentMallocs: Can't find an insertion point!");


        /*
         * Set up IRBuilder
         */ 
        IRBuilder<> Builder = 
            Utils::GetBuilder(
                NextMalloc->getFunction(), 
                InsertionPoint
            );


        /*
         * Cast return value from "malloc" to void pointer
         */
        Value *MallocReturnCast = 
          Builder.CreatePointerCast(
                NextMalloc, 
                VoidPointerType
            );


        /*
         * Cast size parameter to "malloc" into i64
         */
        Value *MallocSizeArgCast = 
            Builder.CreateZExtOrBitCast(
                NextMalloc->getOperand(SizeOperandNo), 
                Int64Type
            );


        /*
         * Set up call parameters
         */ 
        ArrayRef<Value *> CallArgs = {
            MallocReturnCast,
            MallocSizeArgCast
        };


        /*
         * Inject
         */ 
        CallInst *InstrumentMalloc = 
            Builder.CreateCall(
                CARATMalloc, 
                CallArgs
            );
    }


    return;
}


void AllocationHandler::InstrumentFrees(
    AllocID FreeTypeID,
    unsigned PointerOperandNo
)
{
    /*
     * Set up for injection
     */ 
    Function *CARATFree = CARATNamesToMethods[CARAT_REMOVE_ALLOC];

    IRBuilder<> TypeBuilder{M->getContext()};
    Type *VoidPointerType = TypeBuilder.getInt8PtrTy();


    /*
     * Iterate
     */ 
    for (auto NextFree : InstructionsToInstrument[FreeTypeID])
    {
        /*
         * Debugging
         */ 
        errs() << "NextFree: " << *NextFree << "\n";


        /*
         * Set up insertion point
         */ 
        Instruction *InsertionPoint = NextFree->getNextNode();
        assert(!!InsertionPoint 
               && "InstrumentFrees: Can't find an insertion point!");


        /*
         * Set up IRBuilder
         */ 
        IRBuilder<> Builder = 
            Utils::GetBuilder(
                NextFree->getFunction(), 
                InsertionPoint
            );


        /*
         * Cast inst as value passed to free
         */ 
        Value *ParameterToFree = 
            Builder.CreatePointerCast(
                NextFree->getOperand(PointerOperandNo), 
                VoidPointerType
            );


        /*
         * Set up call parameters
         */ 
        ArrayRef<Value *> CallArgs = {
            ParameterToFree
        };


        /*
         * Inject
         */ 
        CallInst *InstrumentFree = 
            Builder.CreateCall(
                CARATFree, 
                CallArgs
            );
    }


    return;
}
