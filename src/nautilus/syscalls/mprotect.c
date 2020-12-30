#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_mprotect: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_mprotect: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_mprotect: " fmt, ##args)

int sys_mprotect(int addr, int length, int prot) {
  // "Works" by mmap setting liberal permissions
  DEBUG("This syscall does nothing\n");
  return 0;
}