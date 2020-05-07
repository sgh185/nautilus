#include "./include/Profiler.hpp"

namespace
{
struct CAT : public ModulePass
{
    static char ID;
    unordered_map<string, Function *> NecessaryMethods;

    CAT() : ModulePass(ID) {}

    bool doInitialization(Module &M) override
    {
        return false;
    }

    //This pass should go through all the functions and wrap
    //the memory instructions with the injected calls needed.
    bool runOnModule(Module &M) override
    {
        if (FALSE)
            exit(0);

        // Get methods necessary for injection --- if at least
        // one method doesn't exist --- abort
        for (auto InjectionName : ImportantMethodNames)
        {
            Function *F = M.getFunction(InjectionName);
            if (F == nullptr)
                abort();

            NecessaryMethods[InjectionName] = F;
        }

        // Storing method names to avoid in the bitcode source
        std::set<std::string> FunctionsToAvoid;
        std::unordered_map<std::string, int> FunctionMap;
        bool modified = false;

        DEBUG_INFO("Entering allocation tracking pass\n");

        // Tracking all the memory allocation related calls
        // in the IR source
        unordered_map<GlobalValue *, uint64_t> Globals;
        std::vector<Instruction *> mallocs;
        std::vector<Instruction *> callocs;
        std::vector<Instruction *> reallocs;
        std::vector<Instruction *> frees;
        std::vector<Instruction *> allocas;

        populateLibCallMap(&FunctionMap);

        GetAllGlobals(M, Globals);

        //This triple nested loop will just go through all the instructions and sort all the allocations into their respective types.
        for (auto &F : M)
        {
            if ((FunctionMap.find(F.getName()) != FunctionMap.end())
                || (NecessaryMethods.find(F.getName()) != NecessaryMethods.end())
                || (F.isIntrinsic())
                || (!(F.getInstructionCount()))) {
                    continue;
            }

            DEBUG_INFO("Entering function " + F.getName() + "\n");

            // This will stop us from iterating through printf and malloc and stuff.
            for (auto &B : F)
            {
                for (auto &I : B)
                {
                    DEBUG_INFO("Working on following instruction: ");
                    OBJ_INFO((&I));

                    if (isa<AllocaInst>(I)) { allocas.push_back(&I); }

                    //First we will check to see if the given instruction is a free instruction or a malloc
                    //This requires that we first check to see if the instruction is a call instruction
                    //Then we check to see if the call is not present (implying it is a library call)
                    //If it is present, next; otherwise, we check if it is free()
                    //Next we see if it is an allocation within a database of allocations (malloc, calloc, realloc, jemalloc...)
                    if (isa<CallInst>(I) || isa<InvokeInst>(I))
                    {
                        Function *fp = nullptr;
                        
                        // Make sure it is a library call
                        if (isa<CallInst>(I))
                        {
                            CallInst *CI = &(cast<CallInst>(I));
                            fp = CI->getCalledFunction();
                        }
                        else
                        {
                            InvokeInst *II = &(cast<InvokeInst>(I));
                            fp = II->getCalledFunction();
                        }

                        //Continue if fails
                        if (fp != nullptr)
                        {
                            if (fp->empty()) // Suspicious
                            {
                                // name is fp->getName();
                                StringRef funcName = fp->getName();

                                DEBUG_INFO(funcName + "\n");

                                int32_t val = FunctionMap[funcName];

                                switch(val)
                                {
                                case NULL: //Did not find the function, error
                                {
                                    if (FunctionsToAvoid.find(funcName) == FunctionsToAvoid.end())
                                    {
                                        errs() << "The following function call is external to the program and not accounted for in our map " << funcName << "\n";
                                        FunctionsToAvoid.insert(funcName);
                                    }

                                    //Maybe it would be nice to add a prompt asking if the function is an allocation, free, or meaningless for our program instead of just dying
                                    //Also should the program maybe save the functions in a saved file (like a json/protobuf) so it can share knowledge between runs.
                                    //If we go the prompt route then we should change the below statements to simply if statements.
                                    
                                    break;
                                }
                                case 2: // Function is an allocation instruction
                                {
                                    mallocs.push_back(&I);
                                    break;
                                }
                                case 3: // Function has the signature of calloc
                                {
                                    callocs.push_back(&I);
                                    break;
                                }
                                case 4: // Function has the signature of realloc
                                {
                                    reallocs.push_back(&I);
                                    break;
                                }
                                case 1: //Function is a deallocation instuction
                                {
                                    frees.push_back(&I);
                                    break;
                                }
                                case -1: 
                                {
                                    DEBUG_INFO("The following function call is external to the program, but the signature of the allocation is not supported (...yet)" + funcName + "\n");
                                    break;
                                }
                                default: // Terrible
                                {
                                    break;
                                }
                                }
                            }
                        }
                    }
                }
            }

        }

        // Handle globals in main
        Function *Main = M.getFunction("main");
        if (Main == nullptr)
            abort();

        AddAllocationTableCallToMain(Main, Globals);

        // Inject everything
        InjectMallocCalls(mallocs);
        InjectCallocCalls(callocs);
        InjectReallocCalls(reallocs);
        InjectFreeCalls(frees);

        return false;
    }

    void AddDebugInfo(Value *V)
    { 
        Instruction *Injected = static_cast<Instruction *>(V);

        Instruction *FirstInstWithDBG = nullptr;
        Function *F = Injected->getFunction();

        for (auto &I : instructions(F))
        {
            if (I.getDebugLoc())
            {
                FirstInstWithDBG = &I;
                break;
            }
        }

        if (FirstInstWithDBG != nullptr)
        {
            IRBuilder<> Builder{Injected};
            Builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());
        }

        return;
    }

    void GetAllGlobals(Module &M,
                       unordered_map<GlobalValue *, uint64_t> &Globals)
    {
        // This will go through the current global variables and make sure 
        // we are covering all heap allocations
        for (auto &Global : M.getGlobalList())
        {
            // All globals are pointer types apparently, but good sanity check
            if (Global.getName() == "llvm.global_ctors") { continue; }

            uint64_t totalSizeInBytes;

            // Each global variable can be either a struct, array, or a primitive.
            // We will figure that out to calculate the total size of the global.
            if (Global.getType()->isPointerTy())
            {
                // errs() << "Working with global: " << Global << "That is a pointer of type: " << *(Global.getValueType()) << "\n";
                Type *iterType = Global.getValueType();
                if (iterType->isArrayTy())
                {
                    totalSizeInBytes = findArraySize(iterType);
                }
                // Now get element size per
                else if (iterType->isStructTy())
                {
                    totalSizeInBytes = findStructSize(iterType);
                }
                // We are worried about bytes not bits
                else
                {
                    totalSizeInBytes = iterType->getPrimitiveSizeInBits() / 8;
                }
                DEBUG_INFO("The size of the element is: " + to_string(totalSizeInBytes) + "\n");
                Globals[&(cast<GlobalValue>(Global))] = totalSizeInBytes;
            }
        }

        return;
    }

    // For globals into main
    void AddAllocationTableCallToMain(Function *Main, 
                                      unordered_map<GlobalValue *, uint64_t> &Globals)
    {
        return;
        // Set up for IRBuilder, malloc injection
		Instruction *InsertionPoint = Main->getEntryBlock().getFirstNonPHI();
        IRBuilder<> MainBuilder{InsertionPoint};

        LLVMContext &TheContext = Main->getContext();
        Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
        Function *CARATMalloc = NecessaryMethods[CARAT_MALLOC];

        for (auto const &[GV, Length] : Globals)
        {
            // Set up arguments for call instruction to malloc
            std::vector<Value *> CallArgs;

            // Build void pointer cast for global
            Value *PointerCast = MainBuilder.CreatePointerCast(GV, VoidPointerType);
            AddDebugInfo(PointerCast);

            // Add to arguments vector
            CallArgs.push_back(PointerCast);
            CallArgs.push_back(ConstantInt::get(IntegerType::get(TheContext, 64), Length, false));

            // Convert to LLVM data structure
            ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

            // Build call instruction
            CallInst *MallocInjection = MainBuilder.CreateCall(CARATMalloc, LLVMCallArgs);
            AddDebugInfo(MallocInjection);
        }
        
        return;
    }

    void InjectMallocCalls(vector<Instruction *> &MallocInstructions)
    {
        Function *CARATMalloc = NecessaryMethods[CARAT_MALLOC];
        LLVMContext &TheContext = CARATMalloc->getContext();

        // Set up types necessary for injections
        Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
        Type *Int64Type = Type::getInt64PtrTy(TheContext, 0); // For pointer injection

        for (auto MI : MallocInstructions)
        {
            Instruction *InsertionPoint = MI->getNextNode();
            if (InsertionPoint == nullptr)
            {
                errs() << "Not able to instrument: ";
                MI->print(errs());
                errs() << "\n";

                continue;
            }

            // Set up injections and call instruction arguments
            IRBuilder<> MIBuilder{InsertionPoint};
            std::vector<Value *> CallArgs;

            // Cast inst as value to grab returned value
            Value *MallocReturnCast = MIBuilder.CreatePointerCast(MI, VoidPointerType);
            AddDebugInfo(MallocReturnCast);
            
            // Cast inst for size argument to original malloc call (MI)
            Value *MallocSizeArgCast = MIBuilder.CreateZExtOrBitCast(MI->getOperand(0), Int64Type);
            AddDebugInfo(MallocSizeArgCast);
            
            // Add CARAT malloc call instruction arguments
            CallArgs.push_back(MallocReturnCast);
            CallArgs.push_back(MallocSizeArgCast);
            ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

            // Build the call instruction to CARAT malloc
            CallInst *AddToAllocationTable = CallInst::Create(CARATMalloc, LLVMCallArgs);
            AddDebugInfo(AddToAllocationTable);
        }

        return;
    }

    void InjectCallocCalls(vector<Instruction *> &CallocInstructions)
    {
        Function *CARATCalloc = NecessaryMethods[CARAT_CALLOC];
        LLVMContext &TheContext = CARATCalloc->getContext();

        // Set up types necessary for injections
        Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
        Type *Int64Type = Type::getInt64PtrTy(TheContext, 0); // For pointer injection

        for (auto CI : CallocInstructions)
        {
            Instruction *InsertionPoint = CI->getNextNode();
            if (InsertionPoint == nullptr)
            {
                errs() << "Not able to instrument: ";
                CI->print(errs());
                errs() << "\n";

                continue;
            }

            // Set up injections and call instruction arguments
            IRBuilder<> CIBuilder{InsertionPoint};
            std::vector<Value *> CallArgs;

            // Cast inst as value to grab returned value
            Value *CallocReturnCast = CIBuilder.CreatePointerCast(CI, VoidPointerType);
            AddDebugInfo(CallocReturnCast);
            
            // Cast inst for size argument to original calloc call (CI)
            Value *CallocSizeArgCast = CIBuilder.CreateZExtOrBitCast(CI->getOperand(0), Int64Type);
            AddDebugInfo(CallocSizeArgCast);

            // Cast inst for second argument (number of elements) to original calloc call (CI)
            Value *CallocNumElmCast = CIBuilder.CreateZExtOrBitCast(CI->getOperand(1), Int64Type);
            AddDebugInfo(CallocNumElmCast);

            // Add CARAT calloc call instruction arguments
            CallArgs.push_back(CallocReturnCast);
            CallArgs.push_back(CallocSizeArgCast);
            CallArgs.push_back(CallocNumElmCast);
            ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

            // Build the call instruction to CARAT calloc
            CallInst *AddToAllocationTable = CallInst::Create(CARATCalloc, LLVMCallArgs);
            AddDebugInfo(AddToAllocationTable);
        }

        return;
    }

    void InjectReallocCalls(vector<Instruction *> &ReallocInstructions)
    {
        Function *CARATRealloc = NecessaryMethods[CARAT_REALLOC];
        LLVMContext &TheContext = CARATRealloc->getContext();

        // Set up types necessary for injections
        Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
        Type *Int64Type = Type::getInt64PtrTy(TheContext, 0); // For pointer injection

        for (auto RI : ReallocInstructions)
        {
            Instruction *InsertionPoint = RI->getNextNode();
            if (InsertionPoint == nullptr)
            {
                errs() << "Not able to instrument: ";
                RI->print(errs());
                errs() << "\n";

                continue;
            }

            // Set up injections and call instruction arguments
            IRBuilder<> RIBuilder{InsertionPoint};
            std::vector<Value *> CallArgs;

            // Cast inst for the old pointer passed to realloc
            Value *ReallocOldPtrCast = RIBuilder.CreatePointerCast(RI->getOperand(0), VoidPointerType);
            AddDebugInfo(ReallocOldPtrCast);
            
            // Cast inst for return value from original Realloc call (RI)
            Value *ReallocReturnCast = RIBuilder.CreateZExtOrBitCast(RI, VoidPointerType);
            AddDebugInfo(ReallocReturnCast);

            // Cast inst for size argument to original Realloc call (RI)
            Value *ReallocNewSizeCast = RIBuilder.CreateZExtOrBitCast(RI->getOperand(1), Int64Type);
            AddDebugInfo(ReallocNewSizeCast);

            // Add CARAT Realloc call instruction arguments
            CallArgs.push_back(ReallocOldPtrCast);
            CallArgs.push_back(ReallocReturnCast);
            CallArgs.push_back(ReallocNewSizeCast);
            ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

            // Build the call instruction to CARAT Realloc
            CallInst *AddToAllocationTable = CallInst::Create(CARATRealloc, LLVMCallArgs);
            AddDebugInfo(AddToAllocationTable);
        }

        return;
    }

    void InjectFreeCalls(vector<Instruction *> &FreeInstructions)
    {
        Function *CARATFree = NecessaryMethods[CARAT_REMOVE_ALLOC];
        LLVMContext &TheContext = CARATFree->getContext();

        // Set up types necessary for injections
        Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection

        for (auto FI : FreeInstructions)
        {
            Instruction *InsertionPoint = FI->getNextNode();
            if (InsertionPoint == nullptr)
            {
                errs() << "Not able to instrument: ";
                FI->print(errs());
                errs() << "\n";

                continue;
            }

            // Set up injections and call instruction arguments
            IRBuilder<> FIBuilder{InsertionPoint};
            std::vector<Value *> CallArgs;

            // Cast inst as value passed to free
            Value *FreePassedPtrCast = FIBuilder.CreatePointerCast(FI->getOperand(0), VoidPointerType);
            AddDebugInfo(FreePassedPtrCast);

            // Add CARAT Free call instruction arguments
            CallArgs.push_back(FreePassedPtrCast);
            ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

            // Build the call instruction to CARAT Free
            CallInst *AddToAllocationTable = CallInst::Create(CARATFree, LLVMCallArgs);
            AddDebugInfo(AddToAllocationTable);
        }

        return;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override
    {
        AU.setPreservesAll();
        return;
    }
};

// Next there is code to register your pass to "opt"
char CAT::ID = 0;
static RegisterPass<CAT> X("TexasAlloc", "Texas Allocation Tracking Pass");

// Next there is code to register your pass to "clang"
static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());} }); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                        [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());} }); // ** for -O0

} //End namespace
