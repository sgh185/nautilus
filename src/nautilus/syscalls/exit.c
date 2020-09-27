#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>


int sys_exit(int	exit_status, int b, int c, int d, int e, int f)
{
  nk_thread_exit((void*)(long)exit_status);
	return 0;
}

static int handle_syscall_exit(char *buf, void *priv) {
  sys_exit(0, 0, 0, 0, 0, 0);
  return 0;
}

static struct shell_cmd_impl syscall_exit = {

    .cmd = "syscall_exit",

    .help_str = "syscall_exit",

    .handler = handle_syscall_exit,

};


nk_register_shell_cmd(syscall_exit);
