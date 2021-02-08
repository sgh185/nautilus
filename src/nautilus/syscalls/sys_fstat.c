#include <nautilus/fs.h>
#include <nautilus/nautilus.h>

#define SYSCALL_NAME "sys_fstat"
#include "impl_preamble.h"

// This struct is not necessarily correct.
struct stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint32_t st_mode;
  uint64_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  uint64_t st_rdev;
  uint64_t st_size;
  uint64_t st_blksize;
  uint64_t st_blocks;
};

uint64_t sys_fstat(uint64_t fd, uint64_t statbuf_) {
  uint64_t ret;
  struct stat* statbuf = (struct stat*)statbuf_;
  memset(statbuf, 0, sizeof(struct stat));

  // Mock response:
  statbuf->st_dev = 27;
  statbuf->st_ino = 9;
  statbuf->st_mode = 8592;
  statbuf->st_nlink = 1;
  statbuf->st_uid = 0;
  statbuf->st_gid = 0;
  statbuf->st_rdev = 34822;
  statbuf->st_size = 0;
  statbuf->st_blksize = 1024;
  statbuf->st_blocks = 0;
  return 0;

  switch (fd) {
  case 0:
  case 1:
  case 2: {
    // Mock response:
    // statbuf->st_dev = 27;
    // statbuf->st_ino = 9;
    // statbuf->st_mode = 8592;
    // statbuf->st_nlink = 1;
    // statbuf->st_uid = 0;
    // statbuf->st_gid = 0;
    // statbuf->st_rdev = 34822;
    // statbuf->st_size = 0;
    // statbuf->st_blksize = 1024;
    // statbuf->st_blocks = 0;
    ret = 0;
    break;
  }
  default: {
    DEBUG("WARNING: fstat is not properly implemented for non-std(in,out,err) "
          "file descriptors.");
    ret = nk_fs_fstat((struct nk_fs_open_file_state*)fd,
                      (struct nk_fs_stat*)statbuf_);
  }
  }
  return ret;
}
