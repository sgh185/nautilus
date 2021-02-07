/*
 * This file is a common preamble for all syscall implementation files. It is
 * not to be used externally.
 */

#ifndef SYSCALL_NAME
#error SYSCALL_NAME is undefined
#endif

#ifndef NAUT_CONFIG_DEBUG_LINUX_SYSCALLS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT(SYSCALL_NAME ": " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT(SYSCALL_NAME ": " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT(SYSCALL_NAME ": " fmt, ##args)
