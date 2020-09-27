#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

int
sys_stat(int a,int b, int c, int d, int e, int f)
{
	char* pathname = (char*)a;
	struct nk_fs_stat *st = (struct nk_fs_stat *)b;
	int ret = nk_fs_stat(pathname,st);
	return ret;
}
