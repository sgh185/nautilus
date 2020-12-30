#include <nautilus/nautilus.h>
#include <nautilus/process.h>

#ifndef MIN
#define MIN(x, y) (x) > (y) ? (y) : (x)
#endif

#define SYSCALL_NAME "sys_readlink"

#define ERROR(fmt, args...) ERROR_PRINT(SYSCALL_NAME ": " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT(SYSCALL_NAME": " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT(SYSCALL_NAME": " fmt, ##args)

uint64_t sys_readlink(uint64_t path, uint64_t buf, uint64_t bufsiz) {
  DEBUG("Call to fake syscall (readlink)\n");
  DEBUG("%s\n%p\n%ld\n", path, buf, bufsiz);

  // /proc/self/exe points to the executable on disk
  if (strcmp(path, "/proc/self/exe") == 0) {
    unsigned const cpy_size = MIN(MAX_PROCESS_NAME, bufsiz);
    strncpy((char*)buf, nk_process_current()->path, cpy_size);
    DEBUG("Copied to buffer: `%s`\n", (char*)buf);
    return MIN(strlen(nk_process_current()->path), cpy_size);
  }

  // Nothing else is implemented now
  return -1;
}