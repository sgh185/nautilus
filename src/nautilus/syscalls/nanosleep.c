#include <nautilus/nautilus.h>
#include <nautilus/timer.h>

#define ERROR(fmt, args...) ERROR_PRINT("sys_nanosleep: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("sys_nanosleep: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("sys_nanosleep: " fmt, ##args)

/// TODO: move these to a better place
struct timespec {
  uint64_t tv_sec; /* seconds */
  uint64_t tv_nsec;  /* nanoseconds */
};
#define NSEC_PER_SEC (uint64_t)1000000000

uint64_t sys_nanosleep(int req, int rem) { 
  const struct timespec* req_sleep = (struct timespec*)req;

  if (!req_sleep) {
    return -1;
  }

  nk_sleep(req_sleep->tv_sec * NSEC_PER_SEC + req_sleep->tv_nsec);

  return 0;
}