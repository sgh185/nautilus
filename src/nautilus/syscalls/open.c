#include <nautilus/nautilus.h>
#include <nautilus/fs.h>

uint64_t
sys_open(uint64_t filename,
	 uint64_t     flags,
	 uint64_t  mode)
{
	struct nk_fs_open_file_state *file;
	file = nk_fs_open((char*)filename,flags, 0);
	return (int)file;
}
