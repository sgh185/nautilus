/// This header could/should be split into distinct headers elsewhere in the
/// kernel. Things are defined here now for simplicity

#define NAUT_HOSTNAME_LEN 64

/// TODO: tie these into the build system?
/// Note that the C runtime expects linux-like versioning and will abort if it
/// finds a value which is far from expected.
static const char uname_sysname[] = "Linux";
static const char uname_release[] = "5.9.6-arch1-1";
static const char uname_version[] =
    "#1 SMP PREEMPT Thu, 05 Nov 2020 21:00:46 +0000";
static const char uname_machine[] = "";

extern char syscall_hostname[NAUT_HOSTNAME_LEN];

/// The size of the struct fields are intended to be determined by the OS, so
/// this can't be included from the build system's headers
#define GLIBC_UTSNAME_LENGTH 65
// struct utsname {
//   char sysname[sizeof(uname_sysname)];
//   char nodename[NAUT_HOSTNAME_LEN];
//   char release[sizeof(uname_release)];
//   char version[sizeof(uname_version)];
//   char machine[sizeof(uname_machine)];
// };

struct utsname {
  char sysname[GLIBC_UTSNAME_LENGTH];
  char nodename[GLIBC_UTSNAME_LENGTH];
  char release[GLIBC_UTSNAME_LENGTH];
  char version[GLIBC_UTSNAME_LENGTH];
  char machine[GLIBC_UTSNAME_LENGTH];
};

// struct utsname {
//   char* sysname;
//   char* nodename;
//   char* release;
//   char* version;
//   char* machine;
// };
