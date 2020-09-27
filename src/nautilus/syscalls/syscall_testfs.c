

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/syscall_user.h>


#ifndef NAUT_CONFIG_DEBUG_TIMERS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("sycall: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("syscall: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("syscall: " fmt, ##args)

static int handle_syscall_testfs(char *buf, void *priv) {
  INFO_PRINT("Shell command for testing file system system calls\n");
  INFO_PRINT("%s\n", buf);

  int fn = open("fs:/new_file2",3|8);
  char* wr_buf = "this used to not exist";

	int wr_bytes = write(fn, wr_buf, 22);

  return 0;
}

static struct shell_cmd_impl syscallfs_impl = {

    .cmd = "syscallfs",

    .help_str = "syscallfs (test system calls for fs, no args)",

    .handler = handle_syscall_testfs,

};

nk_register_shell_cmd(syscallfs_impl);
