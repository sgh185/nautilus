#include <nautilus/nautilus.h>
#include <nautilus/syscall_types.h>

uint64_t sys_sethostname(uint64_t name, uint64_t len) {
  int max_len = NAUT_HOSTNAME_LEN > len ? len : NAUT_HOSTNAME_LEN;
  strncpy(syscall_hostname, (char*)name, NAUT_HOSTNAME_LEN);
  return 0;
}