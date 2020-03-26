#include "../include/Configurations.hpp"

// --------- Global Declarations ---------

// Metadata strings
const string CALLBACK_LOC = "cb.loc";
const string UNROLL_MD = "unr.";
const string MANUAL_MD = "man.";
const string BIASED_MD = "bias.";
const string TOP_MD = "top.";
const string BOTTOM_MD = "bot.";
const string BB_MD = "cb.bb";
const string LOOP_MD = "loop.";
const string FUNCTION_MD = "func.";

// Functions necessary to find in order to inject (hook_fire, fiber_start/create for fiber explicit injections)
const vector<uint32_t> NKIDs = {HOOK_FIRE, FIBER_START, FIBER_CREATE, IDLE_FIBER_ROUTINE};
const vector<string> NKNames = {"nk_time_hook_fire", "nk_fiber_start", "nk_fiber_create", "__nk_fiber_idle"};

// No hook function names functionality --- (non-injectable)
const string ANNOTATION = "llvm.global.annotations";
const string NOHOOK = "nohook";

// Globals
unordered_map<uint32_t, Function *> *SpecialRoutines = new unordered_map<uint32_t, Function *>();
vector<string> *NoHookFunctionSignatures = new vector<string>();
vector<Function *> *NoHookFunctions = new vector<Function *>();