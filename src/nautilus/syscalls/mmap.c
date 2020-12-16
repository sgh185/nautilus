#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_mmap: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_mmap: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_mmap: " fmt, ##args)

uint64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset) {
  
  DEBUG("Unimplemented\n");
  return -1;
}