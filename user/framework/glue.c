#include <nautilus/nautilus_exe.h>

void* __gcc_personality_v0;

void _Unwind_Resume() {
  nk_vc_printf("Call to _Unwind_Resume\n");
  return;
}

double __unordtf2() {
  nk_vc_printf("Call to __unordtf2\n");
  return 0;
}

void __letf2() {
  nk_vc_printf("Call to __letf2\n");
  return;
}