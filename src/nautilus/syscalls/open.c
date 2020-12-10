#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

int
sys_open(int filename,
	 int     flags,
	 int  mode)
{
	struct nk_fs_open_file_state *file;
	file = nk_fs_open((char*)filename,flags, 0);
	return (int)file;
}
