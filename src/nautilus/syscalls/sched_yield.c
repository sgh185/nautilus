#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_sched_yield: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_sched_yield: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_sched_yield: " fmt, ##args)

uint64_t sys_sched_yield() {
  nk_yield();
  return 0;
}