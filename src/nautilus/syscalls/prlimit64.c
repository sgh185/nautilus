#include <nautilus/nautilus.h>
#include <sys/resource.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_prlimit64: " fmt, ##args)

uint64_t sys_prlimit64(uint64_t pid, uint64_t resource, uint64_t new_rlim,
                       uint64_t old_rlim) {
  DEBUG("Called with args\n%ld\n%ld\n%ld\n%ld\n", pid, resource, new_rlim, old_rlim);
  return -1;
}