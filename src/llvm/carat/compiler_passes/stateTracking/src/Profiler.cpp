

#include "profiler.hpp"



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

            bool modified = false;


            std::vector<Instruction *> returns;
            //Map that will map Loops to vectors for the stores and 
            std::unordered_map<std::string, int> functionCalls;
            populateLibCallMap(&functionCalls);

            Instruction* firstInst;

            //Get all exit points to report threads
            Function* main = M.getFunction("main");
            for(auto& B : *main){
                for(auto& I : B){
                    if(isa<ReturnInst>(I)){
                        returns.push_back(&I);
                    }
                }
            }            


            //create a new loopsMap entry
            std::vector<Instruction*>* memUses = new std::vector<Instruction*>();
            std::vector<Instruction*>* memCalls = new std::vector<Instruction*>();




            //This triple nested loop will find all the loads, stores, and mem-based ops
            for(auto& F : M){



                if(F.empty()){
                    continue;
                }

                errs() << "Looking at function: " << F.getName() << "\n";



                //TODO iterate through all instructions and prep them for state tracking
                for(auto& B : F){
                    for(auto& I : B){
                        if(isa<CallInst>(I) || isa<InvokeInst>(I)){
                            Function* fp;
                            if(isa<CallInst>(I)){
                                CallInst* CI = &(cast<CallInst>(I));
                                fp = CI->getCalledFunction();
                            }
                            else{ 
                                InvokeInst* II = &(cast<InvokeInst>(I));
                                fp = II->getCalledFunction();
                            }
                            if(fp != NULL){
                                if(fp->empty()){
                                    StringRef funcName = fp->getName();
                                    int val = functionCalls[funcName];
                                    //Function is a call to mem function
                                    if (val == -3){
                                        memCalls->push_back(&I);
                                    }

                                } 
                            }    
                        }
                        if(isa<StoreInst>(I)){
                            StoreInst* SI = &(cast<StoreInst>(I));
                            memUses->push_back(SI);                                
                        }
                        if(isa<LoadInst>(I)){
                            LoadInst* LI = &(cast<LoadInst>(I));
                            memUses->push_back(LI);
                        }
                    }
                }




            //Build the needed parts for making a callinst
            LLVMContext &TheContext = M.getContext();
            Type* voidType = Type::getVoidTy(TheContext);
            Type* voidPointerType = Type::getInt8PtrTy(TheContext, 0);
            Type* int64Type = Type::getInt64Ty(TheContext);
            //Dealing with printing states at end of run
            if(returns.size() > 0){
                auto signature = FunctionType::get(voidType,false); 
                auto allocFunc = M.getOrInsertFunction(CARAT_STATE_REPORT, signature);
                for(auto* RI : returns){               
                    const Twine &NameStr = "";
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, NameStr, RI);
                    modified = true;
                }
            }

            const Twine &stateInitName = ""; 

            //Mem call instructions
            if(memCalls->size()){

                std::vector<Type*> params;
                params.push_back(voidPointerType);
                params.push_back(voidPointerType);
                params.push_back(int64Type);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_ADD_STATE, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(auto* AI : *(memCalls)){
                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value
                    CastInst* pointerCast = CastInst::CreatePointerCast(AI->getOperand(1), voidPointerType, NameStr, tempI);
                    CastInst* pointerCast2 = CastInst::CreatePointerCast(AI->getOperand(0), voidPointerType, NameStr, tempI); 

                    //TODO get if dinagraph
                    Value* dinaInt = ConstantInt::get(int64Type, DINA_INT, false);   

                    callVals.push_back(pointerCast);
                    callVals.push_back(pointerCast2);
                    callVals.push_back(dinaInt);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    pointerCast->insertAfter(AI);
                    pointerCast2->insertAfter(pointerCast);
                    addToAllocationTable->insertAfter(pointerCast2);

                    modified = true;

                }
            }

            //store and load insts
            if(memUses->size()){
                std::vector<Type*> params;
                params.push_back(voidPointerType);
                params.push_back(voidPointerType);
                params.push_back(int64Type);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_ADD_STATE, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(Instruction* I : *(memUses)){
                    CastInst* firstStatePointer;         
                    CastInst* secondStatePointer = CastInst::CreatePointerCast(ConstantPointerNull::get(PointerType::get(voidPointerType, 0)), voidPointerType, NameStr, tempI);
                    Value* dinaInt = ConstantInt::get(int64Type, DINA_INT, false);   

                    if(isa<StoreInst>(I)){
                        StoreInst* SI = cast<StoreInst>(I);
                        firstStatePointer = CastInst::CreatePointerCast(SI->getPointerOperand(), voidPointerType, NameStr, tempI);
                        if(SI->getValueOperand()->getType()->isPointerTy()){
                            secondStatePointer = CastInst::CreatePointerCast(SI->getValueOperand(), voidPointerType, NameStr, tempI);
                        }
                    }
                    if(isa<LoadInst>(I)){
                        LoadInst* LI = cast<LoadInst>(I);
                        firstStatePointer = CastInst::CreatePointerCast(LI->getPointerOperand(), voidPointerType, NameStr, tempI);

                    }                    

                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value
                    callVals.push_back(firstStatePointer);
                    callVals.push_back(secondStatePointer);
                    callVals.push_back(dinaInt);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    firstStatePointer->insertAfter(I);
                    secondStatePointer->insertAfter(firstStatePointer);
                    addToAllocationTable->insertAfter(secondStatePointer);

                    modified = true;

                }
                errs() << "Done instrumenting Memory\n";
            }
        }
    }

        void getAnalysisUsage (AnalysisUsage &AU) const override {
            AU.addRequired<AssumptionCacheTracker>();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addRequired<ScalarEvolutionWrapperPass>();
            return ;
        }
    };


    // Next there is code to register your pass to "opt"
    char CAT::ID = 0;
    static RegisterPass<CAT> X("StateTracking", "Memory State Tracking Compiler Pass");

    // Next there is code to register your pass to "clang"
    static CAT * _PassMaker = NULL;
    static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -Ox
    static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -O0




} //End namespace
