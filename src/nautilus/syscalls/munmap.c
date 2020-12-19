#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_munmap: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_munmap: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_munmap: " fmt, ##args)

uint64_t sys_munmap(uint64_t addr, uint64_t length, int prot, int flags, int fd, int offset) {
  DEBUG("Call to minimally-implemented syscall\n");
  free(addr);
  return 0;
}