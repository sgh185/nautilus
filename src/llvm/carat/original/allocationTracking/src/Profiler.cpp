//This llvm pass is intended to be a final pass before 


#include "profiler.hpp"

#define CARAT_MALLOC "AddToAllocationTable"
#define CARAT_REALLOC "HandleReallocInAllocationTable"
#define CARAT_CALLOC "AddCallocToAllocationTable"
#define CARAT_REMOVE_ALLOC "RemoveFromAllocationTable"
#define CARAT_STATS "ReportStatistics"

namespace {

    class SCEV_CARAT_Visitor : public SCEVVisitor<SCEV_CARAT_Visitor, Value*> {
        public:
            Value * visitConstant (const SCEVConstant *S) {
                return S->getValue();
            }
    };

    struct CAT : public ModulePass {
        static char ID; 

        CAT() : ModulePass(ID) {}

        bool doInitialization (Module &M) override {
            return false;
        }


        //This pass should go through all the functions and wrap
        //the memory instructions with the injected calls needed.	
        bool runOnModule (Module &M) override {

            std::set<std::string> functionsInProgram;
            std::unordered_map<std::string, int> functionCalls;
            bool modified = false;


#if DEBUG == 1
            errs() << "Entering allocation tracking pass\n";
#endif

            /*
             * Fetch the PDG of the module.
             */
            //            auto PDG = getAnalysis<PDGAnalysis>().getPDG();

            std::vector<std::pair<GlobalValue*, uint64_t>> globals;
            std::vector<Instruction *> mallocs;
            std::vector<Instruction *> callocs;
            std::vector<Instruction *> reallocs;
            std::vector<Instruction *> allocas;
            std::vector<Instruction *> frees;
            std::vector<Instruction *> returns;



            Instruction* firstInst;

            bool firstInstBool = false;

            bool mainFunc = 0;
            populateLibCallMap(&functionCalls);	

#if DEBUG == 1
            errs() << "Current state of LibCallMap:\n";
            for(auto it = functionCalls.begin(); it != functionCalls.end(); ++it){
                errs() << it->first << ":" << it->second << "\n";
            }
#endif

            //This will go through the current global variables and make sure we are covering all heap allocations
            for(auto &Global : M.getGlobalList()){
                //All globals are pointer types apparently, but good sanity check
                if(Global.getName() == "llvm.global_ctors"){
                    continue;
                } 
                uint64_t totalSizeInBytes;
                //Each global variable can be either a struct, array, or a primative. We will figure that out to calculate the total size of the global. 
                if(Global.getType()->isPointerTy()){
                    //errs() << "Working with global: " << Global << "That is a pointer of type: " << *(Global.getValueType()) << "\n";
                    Type* iterType = Global.getValueType();
                    if(iterType->isArrayTy()){
                        totalSizeInBytes = findArraySize(iterType);         
                    }
                    //Now get element size per
                    else if(iterType->isStructTy()){
                        totalSizeInBytes = findStructSize(iterType);
                    }
                    //We are worried about bytes not bits
                    else{
                        totalSizeInBytes = iterType->getPrimitiveSizeInBits()/8;
                    }
                    //errs() << "The size of the element is: " << totalSizeInBytes << "\n";
                    globals.push_back(std::make_pair(&(cast<GlobalValue>(Global)), totalSizeInBytes));
                }

            }

            //This triple nested loop will just go through all the instructions and sort all the allocations into their respective types.
            for(auto& F : M){

                if(functionCalls[F.getName()] != NULL){
                    continue;
                }
                if(F.getName() == "main"){
                    mainFunc = 1;
                }

#if DEBUG == 1
                errs() << "Entering function " << F.getName() << "\n";
#endif

                //This will stop us from iterating through printf and malloc and stuff.
                for(auto& B : F){
                    for(auto& I : B){
                        if(mainFunc && !firstInstBool){
                            firstInstBool = true;
                            firstInst = &I;
                        }

#if DEBUG == 1
                        errs() << "Working on following instruction: ";
                        I.print(errs());
                        errs() << "\n";
#endif

                        if(isa<ReturnInst>(I) && mainFunc){
                            returns.push_back(&I);
                        }
                        if(isa<AllocaInst>(I)){
                            allocas.push_back(&I);
                        }

                        //First we will check to see if the given instruction is a free instruction or a malloc
                        //This requires that we first check to see if the instruction is a call instruction
                        //Then we check to see if the call is not present (implying it is a library call)
                        //If it is present, next; otherwise, we check if it is free()
                        //Next we see if it is an allocation within a database of allocations (malloc, calloc, realloc, jemalloc...)
                        if(isa<CallInst>(I) || isa<InvokeInst>(I)){
                            Function* fp;
                            //Make sure it is a libary call
                            if(isa<CallInst>(I)){
#if DEBUG == 1
                                errs() << "This is a call instruction\n";
#endif
                                CallInst* CI = &(cast<CallInst>(I));
                                fp = CI->getCalledFunction();
                            }
                            else{
#if DEBUG == 1
                                errs() << "This is an invoke instruction\n";
#endif
                                InvokeInst* II = &(cast<InvokeInst>(I));
                                fp = II->getCalledFunction();
                            }
                            //Continue if fails
                            if(fp != NULL){
                                if(fp->empty()){
                                    //name is fp->getName();
                                    StringRef funcName = fp->getName();
#if DEBUG == 1
                                    errs() << funcName << "\n";
#endif
                                    int val = functionCalls[funcName];
                                    //Did not find the function, error
                                    if(val == NULL){
                                        if(functionsInProgram.find(funcName) == functionsInProgram.end()){
                                            errs() << "The following function call is external to the program and not accounted for in our map " << funcName << "\n";
                                            functionsInProgram.insert(funcName);
                                        }
                                        //Maybe it would be nice to add a prompt asking if the function is an allocation, free, or meaningless for our program instead of just dying
                                        //Also should the program maybe save the functions in a saved file (like a json/protobuf) so it can share knowledge between runs.
                                        //If we go the prompt route then we should change the below statements to simply if statements.
                                    }
                                    if(val == -2){
                                        // errs() << "Passing by";
                                        //Next inst
                                    }
                                    //Function is an allocation instruction
                                    else if(val == 2){
                                        mallocs.push_back(&I);
                                    }
                                    //Function has the signature of calloc
                                    else if(val == 3){
                                        callocs.push_back(&I);
                                    }
                                    //Function has the signature of realloc
                                    else if(val == 4){
                                        reallocs.push_back(&I);
                                    }
                                    //Function is a deallocation instuction
                                    else if(val == 1){
                                        frees.push_back(&I);
                                    }
                                    //Function is not implemented yet, but accounted for
                                    else if(val == -1){
                                        errs() << "The following function call is external to the program, but the signature of the allocation is not supported (...yet)" << funcName << "\n";
                                    }
                                } 
                            }    
                        }
                    }
                }
                mainFunc = 0; 
            }

            //Build the needed parts for making a callinst
            LLVMContext &TheContext = M.getContext();
            Type* voidType = Type::getVoidTy(TheContext);
            Type* voidPointerType = Type::getInt8PtrTy(TheContext, 0);
            Type* int64Type = Type::getInt64Ty(TheContext);

            //Dealing with global variables
            if(globals.size() > 0){
                std::vector<Type*> params;
                params.push_back(voidPointerType);
                params.push_back(int64Type);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_MALLOC, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(auto AI : globals){
                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value
                    CastInst* pointerCast = CastInst::CreatePointerCast(AI.first, voidPointerType, NameStr, tempI);
                    callVals.push_back(pointerCast);
                    callVals.push_back(ConstantInt::get(IntegerType::get(TheContext, 64), AI.second, false));
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, firstInst);
                    pointerCast->insertBefore(addToAllocationTable);                     
                    modified = true;
                }
            }

            //Dealing with printing stats at end of runtime
            if(returns.size() > 0){
                auto signature = FunctionType::get(voidType,false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_STATS, signature); 
                for(auto* RI : returns){               
                    auto allocFunc = M.getOrInsertFunction(CARAT_STATS, signature);
                    const Twine &NameStr = "";
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, NameStr, RI);
                    modified = true;
                }
            }

            //Dealing with mallocs
            if(mallocs.size() > 0){
                std::vector<Type*> params;
                params.push_back(voidPointerType);
                params.push_back(int64Type);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_MALLOC, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(auto* AI : mallocs){
                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value

                    CastInst* pointerCast = CastInst::CreatePointerCast(AI, voidPointerType, NameStr, tempI);
                    CastInst* int64Cast = CastInst::CreateZExtOrBitCast(AI->getOperand(0), int64Type, NameStr, tempI); 
                    callVals.push_back(pointerCast);
                    callVals.push_back(int64Cast);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    pointerCast->insertAfter(AI);
                    int64Cast->insertAfter(pointerCast);
                    addToAllocationTable->insertAfter(int64Cast);

                    modified = true;
                }
            }
            //Dealing with callocs
            if(callocs.size() > 0){

                std::vector<Type*> params;
                params.push_back(voidPointerType);
                params.push_back(int64Type);
                params.push_back(int64Type);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_CALLOC, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(auto* AI : callocs){
                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value

                    CastInst* pointerCast = CastInst::CreatePointerCast(AI, voidPointerType, NameStr, tempI);
                    CastInst* int64Cast = CastInst::CreateZExtOrBitCast(AI->getOperand(0), int64Type, NameStr, tempI); 
                    CastInst* int64Cast2 = CastInst::CreateZExtOrBitCast(AI->getOperand(1), int64Type, NameStr, tempI);  
                    callVals.push_back(pointerCast);
                    callVals.push_back(int64Cast);
                    callVals.push_back(int64Cast2);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    pointerCast->insertAfter(AI);
                    int64Cast->insertAfter(pointerCast);
                    int64Cast2->insertAfter(int64Cast);
                    addToAllocationTable->insertAfter(int64Cast2);

                    modified = true;

                }
            }

            //Reallocs
            if(reallocs.size() > 0){
                std::vector<Type*> params;
                params.push_back(voidPointerType);
                params.push_back(voidPointerType);
                params.push_back(int64Type); 
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_REALLOC, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 


                for(auto* AI : reallocs){
                    std::vector<Value*> callVals;

                    CastInst* pointerCast = CastInst::CreatePointerCast(AI->getOperand(0), voidPointerType, NameStr, tempI);
                    CastInst* pointerCast2 = CastInst::CreatePointerCast(AI, voidPointerType, NameStr, tempI);
                    CastInst* int64Cast = CastInst::CreateZExtOrBitCast(AI->getOperand(1), int64Type, NameStr, tempI); 
                    callVals.push_back(pointerCast);
                    callVals.push_back(pointerCast2);
                    callVals.push_back(int64Cast);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    pointerCast->insertAfter(AI);
                    pointerCast2->insertAfter(pointerCast);
                    int64Cast->insertAfter(pointerCast2);
                    addToAllocationTable->insertAfter(int64Cast);

                    modified = true;

                }
            }

            //frees
            if(frees.size() > 0){

                std::vector<Type*> params;
                params.push_back(voidPointerType);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_REMOVE_ALLOC, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(auto* AI : frees){
                    std::vector<Value*> callVals;
                    CastInst* pointerCast = CastInst::CreatePointerCast(AI->getOperand(0), voidPointerType, NameStr, tempI);
                    callVals.push_back(pointerCast);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    pointerCast->insertAfter(AI);
                    addToAllocationTable->insertAfter(pointerCast);

                    modified = true;

                }
            }	

        }

        void getAnalysisUsage (AnalysisUsage &AU) const override {
            AU.addRequired<ScalarEvolutionWrapperPass>();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();
            //AU.addRequired<PDGAnalysis>();

            return ;
        }
    };


    // Next there is code to register your pass to "opt"
    char CAT::ID = 0;
    static RegisterPass<CAT> X("TexasAlloc", "Texas Allocation Tracking Pass");

    // Next there is code to register your pass to "clang"
    static CAT * _PassMaker = NULL;
    static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -Ox
    static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -O0




} //End namespace
