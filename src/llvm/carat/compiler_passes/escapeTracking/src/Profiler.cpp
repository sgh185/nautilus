
#include "profiler.hpp"

#define DEBUG 0

#define CARAT_ESCAPE "_Z16AddToEscapeTablePv"





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

            populateLibCallMap(&functionCalls); 


#if DEBUG == 1
            errs() << "Entering escape tracking pass\n";
#endif

            /*
             * Fetch the PDG of the module.
             */
            //            auto PDG = getAnalysis<PDGAnalysis>().getPDG();

            std::vector<Instruction *> memUses;
            std::vector<Instruction *> memCalls;



            //This triple nested loop will find all the escaping variables and add them to memCalls.
            for(auto& F : M){

                if(functionCalls[F.getName()] != NULL){
                    continue;
                }
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
                                        memCalls.push_back(&I);
                                    }
                                } 
                            }    
                        }
                        if(isa<StoreInst>(I)){
                            StoreInst* SI = &(cast<StoreInst>(I));
                            if(SI->getValueOperand()->getType()->isPointerTy()){
                                memUses.push_back(SI);                                
                            }
                        }
                    }
                }
            }

            //Build the needed parts for making a callinst
            LLVMContext &TheContext = M.getContext();
            Type* voidType = Type::getVoidTy(TheContext);
            Type* voidPointerType = Type::getInt8PtrTy(TheContext, 0);
            Type* int64Type = Type::getInt64Ty(TheContext);

            

            //Mem call instructions
            if(memCalls.size() > 0){

                std::vector<Type*> params;
                params.push_back(voidPointerType);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_ESCAPE, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(auto* AI : memCalls){
                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value

                    CastInst* pointerCast = CastInst::CreatePointerCast(AI->getOperand(1), voidPointerType, NameStr, tempI);
                    callVals.push_back(pointerCast);
                    ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
                    CallInst* addToAllocationTable = CallInst::Create(allocFunc, callArgs, NameStr, tempI);
                    pointerCast->insertAfter(AI);
                    addToAllocationTable->insertAfter(pointerCast);

                    modified = true;

                }
            }

            //store insts
            if(memUses.size() > 0){

                std::vector<Type*> params;
                params.push_back(voidPointerType);
                ArrayRef<Type*> args = ArrayRef<Type*>(params);
                auto signature = FunctionType::get(voidType, args, false);                
                auto allocFunc = M.getOrInsertFunction(CARAT_ESCAPE, signature);
                Instruction* tempI = nullptr; 
                const Twine &NameStr = ""; 

                for(Instruction* SI : memUses){
                    StoreInst* AI = (cast<StoreInst>(SI));                    

                    std::vector<Value*> callVals;
                    //cast inst as value to grab returned value

                    CastInst* pointerCast = CastInst::CreatePointerCast(AI->getValueOperand(), voidPointerType, NameStr, tempI);
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
    static RegisterPass<CAT> X("TexasEscape", "TEXAS Escape Tracking Pass");

    // Next there is code to register your pass to "clang"
    static CAT * _PassMaker = NULL;
    static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -Ox
    static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
            [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
            if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -O0




} //End namespace
