#include "Configurations.hpp"

using namespace std;
using namespace llvm;

namespace
{
double getInlineAsmLatency(string IAString)
{
    double cost = 10; // default latency
    if (IAString.substr(0, 3) == "mov") // Terrible
        cost = 1; // Fog
    else if (IAString == "pause")
        cost = 25; // Fog

    return cost;
}

double getInstLatency(Instruction *I) 
{
    int opcode = I->getOpcode();
    double cost = 0;

    switch (opcode)
    {
    // Terminator instructions
    case Instruction::Call:
    {
        if (auto *Call = dyn_cast<CallInst>(I)) // don't want to involve LLVM internals
        {
            // Handle InlineAsm (in the worst way possible)
            if (auto *IA = dyn_cast<InlineAsm>(Call->getCalledValue()))
                cost = getInlineAsmLatency(IA->getAsmString());

            // Handle real callees
            Function *Callee = Call->getCalledFunction();

            // Possible intrinsic, assembly stub, etc.
            if ((Callee == nullptr)
                || (Callee->isIntrinsic()) // llvm.stacksave() ??? 
                || (Callee->getName().startswith("llvm.lifetime"))) // Unclear
                break;
            else cost = 3; // Fog --- Knights Landing, generic Intel
        }

        break;
    }
    case Instruction::Ret:
        cost = 2; // Fog
        break;
    case Instruction::Br:
    case Instruction::IndirectBr: // NEW
    case Instruction::Unreachable: // NEW
    case Instruction::Switch: // FIX
        break;

    // Standard binary operators --- NOTE --- Fog numbers 
    // for Knights landing much higher
    case Instruction::Add:
    case Instruction::Sub:
        cost = 1;
        break;
    case Instruction::Mul:
        cost = 3;
        break;
    case Instruction::FAdd:
    case Instruction::FSub:
    case Instruction::FMul:
        cost = 4;
        break;
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::URem:
    case Instruction::SRem:
        cost = 17;
        break;
    case Instruction::FDiv:
    case Instruction::FRem:
        cost = 24;
        break;
        
    // Logical operators (integer operands)
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
        cost = 7;
        break;
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor:
        cost = 1;
        break;

    // Vector ops
    case Instruction::ExtractElement:
    case Instruction::InsertElement:
    case Instruction::ShuffleVector:
    case Instruction::ExtractValue:
    case Instruction::InsertValue:
        break; 

    // Memory ops
    case Instruction::Alloca: // Prev = 2
        break;
    case Instruction::Load:
    case Instruction::Store:
        cost = 4; // L1
        break;
    
    // Atomics related
    // case Instruction::Fence:
    case Instruction::AtomicCmpXchg:
        cost = 24; // Fog
        break;
    case Instruction::AtomicRMW:
        cost = 11; // Fog
        break;

    // Control flow 
    case Instruction::ICmp:
        cost = 1; // Fog
        break;
    case Instruction::FCmp:
        cost = 7; // Fog
        break; 

    // Cast operators --- NEW
    case Instruction::IntToPtr:
    case Instruction::PtrToInt:
    case Instruction::BitCast: // Prev = 1
        break;
    case Instruction::ZExt:
    case Instruction::SExt:
    case Instruction::Trunc:
    case Instruction::FPExt:
    case Instruction::FPTrunc:
    case Instruction::AddrSpaceCast:
        cost = 1;
        break; 
    case Instruction::FPToUI:
    case Instruction::FPToSI:
    case Instruction::SIToFP:
    case Instruction::UIToFP:
        cost = 6; // Fog
        break;

    // LLVM IR specifics
    case Instruction::PHI:
    case Instruction::VAArg:
    case Instruction::GetElementPtr: // Unclear
        break;
    case Instruction::Select: // NEW
        cost = 10; // Unclear
        break;
    
    default:
        cost = 4; // Unclear
        break;
    }

    return cost;
}

}