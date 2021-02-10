/*
 * This file is a common preamble for all syscall implementation files. It is
 * not to be used externally.
 */

#ifndef SYSCALL_NAME
#error SYSCALL_NAME is undefined
#endif

#include <nautilus/process.h>
#include <nautilus/syscalls/defs.h>
#include <nautilus/nautilus.h> /* Required for warn/debug prints */

#ifndef NAUT_CONFIG_DEBUG_LINUX_SYSCALLS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#undef INFO_PRINT
#define INFO_PRINT(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT(SYSCALL_NAME ": " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT(SYSCALL_NAME ": " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT(SYSCALL_NAME ": " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT(SYSCALL_NAME ": " fmt, ##args)

/// Get the current process, panicing if not in the context of a process
#define GET_PROC()                                                        \
  nk_process_current();                                                   \
  if (!nk_process_current()) {                                            \
    panic("Call to " SYSCALL_NAME " out of the context of a process.\n"); \
  }