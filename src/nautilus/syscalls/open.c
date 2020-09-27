#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

int
sys_open(int filename,
	 int     flags,
	 int  mode, int d, int e, int f)
{
	struct nk_fs_open_file_state *file;
	file = nk_fs_open((char*)filename,flags,(int)mode);
	return (int)file;
}
