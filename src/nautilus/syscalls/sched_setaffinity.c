#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_sched_setaffinity: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_sched_setaffinity: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_sched_setaffinity: " fmt, ##args)

uint64_t sys_sched_setaffinity(uint64_t pid, uint64_t len,
                               uint64_t user_mask_ptr) {
  DEBUG("Args:%p\n%p\n%p\n", pid, len, user_mask_ptr);
  if (pid) {
    ERROR("Not supported for other processes\n");
    return -1;
  }
  /* Do not support setting affinity for now */
  return -1;
}
