#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

int
sys_close(int fd, int b, int c, int d, int e, int f)
{
  struct nk_fs_open_file_state* file;
	file = nk_fs_close((struct nk_fs_open_file_state*)fd);
	return (int)file;
}
