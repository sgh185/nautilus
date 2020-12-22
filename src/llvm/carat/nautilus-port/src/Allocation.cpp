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
    this->Init = M->getFunction(CARAT_INIT);
    assert(!!(this->Init) 
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
    if (HANDLE_GLOBALS) InstrumentGlobals();


    /*
     * Instrument allocations
     */ 
    InstrumentMallocs();
    InstrumentFrees();


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
                 */ 
                if (KernelAllocMethods.find(Callee) == KernelAllocMethods.end()) { continue; }


                /*
                 * Fetch the kernel alloc method ID, switch
                 * based on the ID to mark each call for 
                 * instrumentation
                 */ 
                KernelAllocID KAID = KernelAllocMethods[Callee];
                switch (KAID)
                {
                    case KernelAllocID::Malloc: Mallocs.insert(NextCall);
                    case KernelAllocID::Free: Frees.insert(NextCall);
                    default:
                    {
                        errs() << "AllocationHandler::_getAllNecessaryInstructions: Failed to fetch KernelAllocID!";
                        abort();
                    }
                }
            }
        }
    }

    
    return;
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
         * Ignore LLVM-specific metadata globals
         */ 
        if (false
            || (Global.getSection() == "llvm.metadata")
            || (Global.isDiscardableIfUnused())) /* FIX */
            { continue; }


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
         * Store the [global : size] mapping
         */ 
        Globals[&Global] = GlobalSize;


        /*
         * Debugging
         */ 
        DEBUG_ERRS << "Global: " << Global << "\n"
                   << "Type: " << *GlobalTy << "\n" 
                   << "TypeID: " << GlobalTy->getTypeID() << "\n"
                   << "Size: " << GlobalSize << "\n\n";
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
    Instruction *InsertionPoint = Init->back().getTerminator();
    assert(isa<ReturnInst>(InsertionPoint)
           && "InstrumentGlobals: Back block terminator of 'nk_carat_init' is not return!");


    /*
     * Set up IRBuilder
     */  
    IRBuilder<> InitBuilder = 
        Utils::GetBuilder(
            Init, 
            InsertionPoint
        );


    /*
     * Set up for injections
     */ 
    Type *VoidPointerType = InitBuilder.getInt8PtrTy();
    Function *CARATMalloc = CARATNamesToMethods[CARAT_MALLOC];


    /*
     * Iterate through all globals to be intrumented
     */
    for (auto const &[GV, Length] : Globals)
    {
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
            InitBuilder.CreatePointerCast(
                GV, VoidPointerType
            );


        /*
         * Set up call parameters
         */ 
        ArrayRef<Value *> CallArgs = {
            PointerCast,
            InitBuilder.getInt64(Length)
        };


        /*
         * Inject
         */ 
        CallInst *Instrumentation = 
            InitBuilder.CreateCall(
                CARATMalloc, 
                CallArgs
            );
    }


    return;
}


void AllocationHandler::InstrumentMallocs()
{
    /*
     * Set up for injection
     */ 
    Function *CARATMalloc = NecessaryMethods[CARAT_MALLOC];

    IRBuilder<> TypeBuilder{M->getContext()};
    Type *VoidPointerType = TypeBuilder.getInt8PtrTy(),
         *Int64Type = TypeBuilder.getInt64Ty();


    /*
     * Instrument all collected "malloc"ss
     */ 
    for (auto NextMalloc : Mallocs)
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
                NextMalloc->getOperand(0), 
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


void AllocationHandler::InstrumentFrees()
{
    /*
     * Set up for injection
     */ 
    Function *CARATFree = NecessaryMethods[CARAT_REMOVE_ALLOC];

    IRBuilder<> TypeBuilder{M->getContext()};
    Type *VoidPointerType = TypeBuilder.getInt8PtrTy();


    /*
     * Iterate
     */ 
    for (auto NextFree : Frees)
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


        // Cast inst as value passed to free
        Value *ParameterToFree = 
            Builder.CreatePointerCast(
                NextFree->getOperand(0), 
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
