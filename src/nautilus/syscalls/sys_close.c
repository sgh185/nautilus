#include <nautilus/fs.h>
#include <nautilus/syscalls/proc.h>

#define SYSCALL_NAME "sys_close"
#include "impl_preamble.h"

uint64_t sys_close(fd_t fd) {
  if (fd < 3) {
    DEBUG("Closing stdio is not defined\n");
  }

  nk_process_t* current_process = syscall_get_proc();

  if (!fd_to_nk_fs(&current_process->syscall_state.fd_table, fd)) {
    DEBUG("Can't close an unopened file\n");
    return -1;
  }
  WARN("Not actually closing an open file descriptor\n");
  return 0;
}
