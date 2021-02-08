#include <nautilus/fs.h>
#include <nautilus/nautilus.h>

#define SYSCALL_NAME "sys_close"
#include "impl_preamble.h"

uint64_t sys_close(uint64_t fd) {
  // TODO: add protection against bad fd
  return (uint64_t)nk_fs_close((struct nk_fs_open_file_state*)fd);
}
