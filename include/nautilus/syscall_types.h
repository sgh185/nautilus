/// This header could/should be split into distinct headers elsewhere in the
/// kernel. Things are defined here now for simplicity

#define NAUT_HOSTNAME_LEN 64

/// TODO: tie these into the build system
static const char uname_sysname[] = "Nautilus";
static const char uname_release[] = "0";
static const char uname_version[] = "0";
static const char uname_machine[] = "";

extern char syscall_hostname[NAUT_HOSTNAME_LEN];

/// The size of the struct fields are intended to be determined by the OS, so
/// this can't be included from the build system's headers
struct utsname {
  char sysname[sizeof(uname_sysname)];
  char nodename[NAUT_HOSTNAME_LEN];
  char release[sizeof(uname_release)];
  char version[sizeof(uname_version)];
  char machine[sizeof(uname_machine)];
};

