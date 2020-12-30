#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>


int sys_exit(int exit_status)
{
  nk_thread_exit((void*)(long)exit_status);
	return 0;
}