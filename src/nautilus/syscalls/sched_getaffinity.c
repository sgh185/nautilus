#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_sched_getaffinity: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_sched_getaffinity: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_sched_getaffinity: " fmt, ##args)

uint64_t sys_sched_getaffinity(uint64_t pid, uint64_t len,
                               uint64_t _user_mask_ptr) {
  DEBUG("Args:%p\n%p\n%p\n", pid, len, _user_mask_ptr);
  uint64_t* user_mask_ptr = (uint64_t*)_user_mask_ptr;
  if (pid) {
    ERROR("Not supported for other processes\n");
    return -1;
  }
  if (len == 1) {
    *(uint8_t*)user_mask_ptr = 0x1;
    return len;
  } else if (len == 2) {
    *(uint16_t*)user_mask_ptr = 0x0101;
    return len;
  } else if (len == 4) {
    *(uint32_t*)user_mask_ptr = 0x0101;
    return len;
  }
  *user_mask_ptr = 0x0101; /* Allow CPU 0/1 */
  return 8;           /* Size of the bitmask data structure */
}