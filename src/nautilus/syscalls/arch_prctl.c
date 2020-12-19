#include <asm/prctl.h>
#include <nautilus/nautilus.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_arch_prctl: " fmt, ##args)

/// This is an x86-specific implementation
uint64_t sys_arch_prctl(uint64_t task, uint64_t option, uint64_t addrlen) {
  DEBUG("Task: %lx\n Option: %lx\n Addrlen %p\n", task, option, addrlen);

  // NOTE: the implementation here is based on reverse engineering of a specific
  // version of the c runtime. It is not consistent with either the linux kernel
  // or the manpage for arch_prctl! Further, this has not been extensively
  // tested and is most likely wrong in general.

  uint64_t ret = 0;
  switch (task) {
  case ARCH_SET_FS: {
    __asm__("movl %0, %%edx\n"
            "movl %1, %%eax\n"
            "movq $0xC0000100, %%rcx\n"
            "wrmsr\n" ::"g"(option >> 32),
            "g"(option & 0xFFFFFFFF));
    break;
  }
  case ARCH_SET_GS: {
    __asm__("movl %0, %%edx\n"
            "movl %1, %%eax\n"
            "movq $0xC0000101, %%rcx\n"
            "wrmsr\n" ::"g"(option >> 32),
            "g"(option & 0xFFFFFFFF));
    break;
  }
  case ARCH_GET_FS: {
    __asm__("movq $0xC0000100, %%rcx\n"
            "rdmsr\n"
            "salq $32, %%rdx\n"
            "orq %%rdx, %%rax\n"
            "movq %%rax, %0"
            : "=r"(ret));
    break;
  }
  case ARCH_GET_GS: {
    __asm__("movq $0xC0000101, %%rcx\n"
            "rdmsr\n"
            "salq $32, %%rdx\n"
            "orq %%rdx, %%rax\n"
            "movq %%rax, %0"
            : "=r"(ret));
    break;
  }
  default: {
    DEBUG("Unimplemented option\n");
    ret = -1;
    (void)0;
  }
  }
  return ret;
}