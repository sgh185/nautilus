#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

int
sys_lseek(int fd, int position, int whence, int d, int e, int f)
{
  int ret;
	ret = (int)nk_fs_seek((struct nk_fs_open_file_state*)fd, (off_t)position, (off_t)whence);
	return ret;
}
