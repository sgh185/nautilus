#include <nautilus/fs.h>
#include <nautilus/nautilus.h>

#define SYSCALL_NAME "sys_lseek"
#include "impl_preamble.h"

uint64_t sys_lseek(uint64_t fd, uint64_t position, uint64_t whence) {
  if (fd <= 2) {
    DEBUG(
        "WARNING: lseek may not be properly implemented for std(in,out,err)\n");
    return 0;
  }
  uint64_t ret;
  ret = (uint64_t)nk_fs_seek((struct nk_fs_open_file_state*)fd, (off_t)position,
                             (off_t)whence);
  return ret;
}
