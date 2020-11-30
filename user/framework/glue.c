#include <stdio.h>

void* __gcc_personality_v0;

void _Unwind_Resume() {
  printf("Call to fake _Unwind_Resume\n");
  return;
}

double __unordtf2() {
  printf("Call to fake __unordtf2\n");
  return 0;
}

void __letf2() {
  printf("Call to fake __letf2\n");
  return;
}