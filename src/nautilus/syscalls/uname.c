#include <nautilus/nautilus.h>
#include <nautilus/syscall_types.h>

uint64_t sys_uname(uint64_t name_) {
  struct utsname* name = (struct utsname*)name_;
  memset(name, 0, sizeof(struct utsname));

  strcpy(name->sysname, uname_sysname);
  strcpy(name->release, uname_release);
  strcpy(name->version, uname_version);
  strcpy(name->machine, uname_machine);
  strncpy(name->nodename, syscall_hostname, NAUT_HOSTNAME_LEN);

  return 0;
}
