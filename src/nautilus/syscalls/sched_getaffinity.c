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

  // Set up the system bitmask (threads = num cpus), which is the default affinity
  static uint8_t calculated_system_bitmask = false;
  static uint64_t system_bitmask = 0;
  if (!calculated_system_bitmask) {
    for (int i = 0; i < nk_get_num_cpus(); i++) {
      system_bitmask |= 1 << i;
    }
    calculated_system_bitmask = true;
  }

  nk_thread_t* cur_thread = get_cur_thread();
  if (cur_thread->fake_affinity == 0) {
    cur_thread->fake_affinity = system_bitmask;
  }

  if (len == 1) {
    *(uint8_t*)user_mask_ptr = cur_thread->fake_affinity & 0xFF;
    return len;
  } else if (len == 2) {
    *(uint16_t*)user_mask_ptr = cur_thread->fake_affinity & 0xFFFF;
    return len;
  } else if (len == 4) {
    *(uint32_t*)user_mask_ptr = cur_thread->fake_affinity & 0xFFFFFFFF;
    return len;
  }
  *user_mask_ptr = cur_thread->fake_affinity;
  DEBUG("Returning affinity %lx\n", *user_mask_ptr);
  return 8;           /* Size of the bitmask data structure */
}