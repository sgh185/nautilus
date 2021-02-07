#include <nautilus/nautilus.h>

#define SYSCALL_NAME "sys_munmap"
#include "syscall_impl_preamble.h"

uint64_t sys_munmap(uint64_t addr, uint64_t length, int prot, int flags, int fd,
                    int offset) {
  DEBUG("Call to unimplemented syscall: possible memory leak\n");
  // DEBUG("Call to minimally-implemented syscall\n");
  // free(addr);
  return 0;
}