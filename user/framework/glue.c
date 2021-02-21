#include <stdio.h>

__attribute__((section(".naut_secure"))) unsigned char __NAUT_SIGNATURE[16];

double __unordtf2() {
  printf("Call to fake __unordtf2\n");
  return 0;
}

void __letf2() {
  printf("Call to fake __letf2\n");
  return;
}