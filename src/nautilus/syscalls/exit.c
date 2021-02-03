#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>


uint64_t sys_exit(uint64_t exit_status)
{
  nk_thread_exit((void*)(long)exit_status);
	return 0;
}