#include <nautilus/nautilus.h>
#include <nautilus/syscall_decl.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_openat: " fmt, ##args)

uint64_t sys_openat(uint64_t dfd, uint64_t filename, uint64_t flags,
                    uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to fake syscall (openat)\nTry to open %s\n", filename);
  return sys_open(filename, flags, mode);
}