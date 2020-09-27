#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/thread.h>


nk_thread_id_t
sys_fork(int a, int b, int c, int d, int e, int f)
{
	printk( "Attempting to fork!\n");
	return nk_thread_fork();
}
