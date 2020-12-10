#include <nautilus/nautilus.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_set_robust_list: " fmt, ##args)

uint64_t sys_set_robust_list(uint64_t header, uint64_t len) {
  DEBUG("Call to fake syscall (set_robust_list)\n");
  return 0;
}