#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

int
sys_fstat(int fd, int st, int c, int d, int e, int f)
{
	int ret = nk_fs_fstat((struct nk_fs_open_file_state*)fd,(struct nk_fs_stat *)st);
	return ret;
}
