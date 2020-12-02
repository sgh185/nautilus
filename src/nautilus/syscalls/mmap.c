#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_mmap: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_mmap: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_mmap: " fmt, ##args)

int sys_mmap(int addr, int length, int prot, int flags, int fd, int offset) {
  /// TODO: everything
  
  DEBUG("Unimplemented\n");
  return -1;
}