#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_munmap: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_munmap: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_munmap: " fmt, ##args)

int sys_munmap(int addr, int length, int prot, int flags, int fd, int offset) {
  /// TODO: everything
  
  DEBUG("Unimplemented");
  return 0;
}