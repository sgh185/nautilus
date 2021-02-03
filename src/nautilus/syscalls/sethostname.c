#include <nautilus/nautilus.h>
#include <nautilus/syscall_types.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_sethostname: " fmt, ##args)

uint64_t sys_sethostname(uint64_t name, uint64_t len) {
  int max_len = NAUT_HOSTNAME_LEN > len ? len : NAUT_HOSTNAME_LEN;
  DEBUG("Setting hostname (not thread safe)\n");
  strncpy(syscall_hostname, (char*)name, NAUT_HOSTNAME_LEN);
  return 0;
}