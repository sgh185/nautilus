#include <nautilus/syscalls/proc.h>
#include <nautilus/process.h>

#define SYSCALL_NAME "sys_exit_group"
#include "impl_preamble.h"

uint64_t sys_exit_group(uint64_t error_code) {
  DEBUG("exit_group is incomplete and needs to stop all process threads\n");
  
  free_process_syscall_state(&syscall_get_proc()->syscall_state);

  return 0;
}