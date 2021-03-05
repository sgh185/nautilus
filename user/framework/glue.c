#include <stdio.h>

void* __gcc_personality_v0;

void _Unwind_Resume() {
  printf("Call to fake _Unwind_Resume\n");
  return;
}

void _Unwind_GetCFA() {
  return;
}

void _Unwind_ForcedUnwind() {
  
}

double __unordtf2() {
  printf("Call to fake __unordtf2\n");
  return 0;
}
