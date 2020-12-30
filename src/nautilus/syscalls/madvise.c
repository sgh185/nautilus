#include <nautilus/nautilus.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_madvise: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_madvise: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_madvise: " fmt, ##args)

uint64_t sys_madvise(uint64_t start, uint64_t len_in, uint64_t behavior) {
  DEBUG("Thanks for the advice, but we will ignore it :)\n");
  return 0;
}
