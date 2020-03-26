#include "../include/Utils.hpp"

using namespace Utils;
using namespace Debug;

// ----------------------------------------------------------------------------------

// Utils --- Init

/*
 * ExitOnInit
 * 
 * Register pass, execute doInitialization method but do not perform
 * any analysis or transformation --- exit in runOnModule --- mostly
 * for testing scenarios
 * 
 */

void Utils::ExitOnInit()
{
    if (FALSE)
        exit(0);
}

/*
 * GatherAnnotatedFunctions
 * 
 * A user can mark a function in Nautilus with a function attribute to
 * prevent injections into the function. For example, it would be unwise
 * to inject calls to nk_time_hook_fire into nk_time_hook_fire itself. Users
 * should use the annotate attribute with the string "nohook" in order 
 * to prevent injections.
 * 
 * This method parses the global annotations array in the resulting bitcode
 * and collects all functions that have been annotated with the "nohook"
 * attribute. These functions will not have injections.
 * 
 */

void Utils::GatherAnnotatedFunctions(GlobalVariable *GV, 
                                     vector<Function *> &AF)
{
    // First operand is the global annotations array --- get and parse
    // NOTE --- the fields have to be accessed through VALUE->getOperand(0),
    // which appears to be a layer of indirection for these values
    auto *AnnotatedArr = cast<ConstantArray>(GV->getOperand(0));

    for (auto OP = AnnotatedArr->operands().begin(); OP != AnnotatedArr->operands().end(); OP++)
    {
        // Each element in the annotations array is a ConstantStruct --- its
        // fields can be accessed through the first operand (indirection). There are two
        // fields --- Function *, GlobalVariable * (function ptr, annotation)

        auto *AnnotatedStruct = cast<ConstantStruct>(OP);
        auto *FunctionAsStructOp = AnnotatedStruct->getOperand(0)->getOperand(0);         // first field
        auto *GlobalAnnotationAsStructOp = AnnotatedStruct->getOperand(1)->getOperand(0); // second field

        // Set the function and global, respectively. Both have to exist to
        // be considered.
        Function *AnnotatedF = dyn_cast<Function>(FunctionAsStructOp);
        GlobalVariable *AnnotatedGV = dyn_cast<GlobalVariable>(GlobalAnnotationAsStructOp);

        if (AnnotatedF == nullptr || AnnotatedGV == nullptr)
            continue;

        // Check the annotation --- if it matches the NOHOOK global in the
        // pass --- push back to apply (X) transform as necessary later
        ConstantDataArray *ConstStrArr = dyn_cast<ConstantDataArray>(AnnotatedGV->getOperand(0));
        if (ConstStrArr == nullptr)
            continue;

        if (ConstStrArr->getAsCString() != NOHOOK)
            continue;

        AF.push_back(AnnotatedF);
    }

    Debug::PrintFNames(AF);

    return;
}

// ----------------------------------------------------------------------------------

// Utils --- Function tracking/identification

/*
 * IdentifyFiberRoutines
 * 
 * Given a module (in this case the entire Nautilus kernel), this
 * method searches for all fiber routines registered explicitly
 * in the kernel via the arguments in nk_fiber_create, the fiber
 * "registration" method in Nautilus. Returns a pointer to a 
 * set of these routines.
 * 
 */

set<Function *> *Utils::IdentifyFiberRoutines()
{
    set<Function *> *Routines = new set<Function *>();

    // Iterate over uses of nk_fiber_create
    for (auto &FiberCreateUse : SpecialRoutines->at(FIBER_CREATE)->uses())
    {
        User *FiberCreateUser = FiberCreateUse.getUser();
        if (auto *CallToRoutine = dyn_cast<CallInst>(FiberCreateUser))
        {
            // First arg of nk_fiber_create is a function pointer
            // to an explicitly registered fiber routine
            auto FunctionPtr = CallToRoutine->getArgOperand(0);
            if (auto *Routine = dyn_cast<Function>(FunctionPtr))
                Routines->insert(Routine);
        }
    }

    return Routines;
}

/*
 * IdentifyAllNKFunctions
 * 
 * Given a module (in this case the entire Nautilus kernel), this
 * method finds all valid functions represented in the bitcode for
 * injection purposes. This includes all functions that are not LLVM 
 * intrinsics, contain function bodies, aren't nullptr, not in the 
 * NoHookFunctions set and not nk_time_hook_fire. Adds all functions
 * to a passed set.
 * 
 */

void Utils::IdentifyAllNKFunctions(Module &M, set<Function *> &Routines)
{
    // Iterate through all functions in the kernel
    for (auto &F : M)
    {
        // Check function for conditions
        if ((&F == nullptr)
            || (F.isIntrinsic())
            || (&F == SpecialRoutines->at(HOOK_FIRE))
            || (find(NoHookFunctions->begin(), NoHookFunctions->end(), &F) != NoHookFunctions->end())
            || (!(F.getInstructionCount() > 0)))
            continue;

        DEBUG_INFO(F.getName() + 
                   ": InstructionCount --- " + to_string(F.getInstructionCount()) + 
                   "; ReturnType --- ");
        OBJ_INFO((F.getReturnType()));

        // Add pointer to set if all conditions check out
        Routines.insert(&F);
    }

    return;
}

// ----------------------------------------------------------------------------------

// Utils --- Transformations

/*
 * InlineNKFunction
 * 
 * Inline a given function (if it passes the sanity checks) by 
 * iterating over all uses and inlining each call invocation
 * to that function --- standard
 * 
 */

void Utils::InlineNKFunction(Function *F)
{
    // If F doesn't check out, don't inline
    if ((F == nullptr)
        || (F->isIntrinsic())
        || (!(F->getInstructionCount() > 0)))
        return;

    // Set up inlining structures
    vector<CallInst *> CIToIterate;
    InlineFunctionInfo IFI;

    DEBUG_INFO("Current Function: " + F->getName() + "\n");

    // Iterate over all uses of F
    for (auto &FUse : F->uses())
    {
        User *FUser = FUse.getUser();

        DEBUG_INFO("Current User: ");
        OBJ_INFO(FUser);

        // Collect all uses that are call instructions
        // that reference F 
        if (auto *FCall = dyn_cast<CallInst>(FUser))
        {
            DEBUG_INFO("    Found a CallInst\n");
            CIToIterate.push_back(FCall);
        }

        DEBUG_INFO("\n");
    }

    // Iterate through all call instructions referencing
    // F and inline all invocations --- if any of them fail,
    // abort (serious)
    for (auto CI : CIToIterate)
    {
        DEBUG_INFO("Current CI: ");
        OBJ_INFO(CI);

        auto Inlined = InlineFunction(CI, IFI);
        if (!Inlined)
        {
            DEBUG_INFO("INLINE FAILED --- ");
            DEBUG_INFO(Inlined.message);
            DEBUG_INFO("\n");
            abort(); // Serious
        }
    }

    return;
}

// ----------------------------------------------------------------------------------

// Utils --- Transformations
void Utils::SetCallbackMetadata(Instruction *I, const string MD)
{       
    // Build metadata node
    MDNode *TheNode = MDNode::get(I->getContext(),
                                  MDString::get(I->getContext(), "callback"));

    // Set callback location metadata
    I->setMetadata(CALLBACK_LOC, TheNode);

    // Set specific metadata
    I->setMetadata(MD, TheNode);

    return;
}

bool Utils::HasCallbackMetadata(Instruction *I)
{
    MDNode *CBMD = I->getMetadata(CALLBACK_LOC);
    if (CBMD == nullptr)
        return false;
    
    return true;
}

bool Utils::HasCallbackMetadata(Loop *L, set<Instruction *> &InstructionsWithCBMD)
{
    for (auto B = L->block_begin(); B != L->block_end(); ++B)
    {
        BasicBlock *CurrBB = *B;
        for (auto &I : *CurrBB)
        {
            if (Utils::HasCallbackMetadata(&I))
                InstructionsWithCBMD.insert(&I);
        }
    }

    if (InstructionsWithCBMD.empty())
        return false;

    return true;
}

bool Utils::HasCallbackMetadata(Function *F, set<Instruction *> &InstructionsWithCBMD)
{
    for (auto &B : *F)
    {
        for (auto &I : B)
        {
            if (Utils::HasCallbackMetadata(&I))
                InstructionsWithCBMD.insert(&I);
        }
    }

    if (InstructionsWithCBMD.empty())
        return false;

    return true;
}

// ----------------------------------------------------------------------------------

// Debug

void Debug::PrintFNames(vector<Function *> &Functions)
{
    for (auto F : Functions)
    {
        if (F == nullptr)
        {
            DEBUG_INFO("F is nullptr\n");
            continue;
        }

        DEBUG_INFO(F->getName() + "\n");
    }


    return;
}

void Debug::PrintFNames(set<Function *> &Functions)
{
    for (auto F : Functions)
    {
        if (F == nullptr)
        {
            DEBUG_INFO("F is nullptr\n");
            continue;
        }

        DEBUG_INFO(F->getName() + "\n");
    }

    return;
}