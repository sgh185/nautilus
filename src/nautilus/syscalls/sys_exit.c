#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/syscalls/decl.h>
#include <nautilus/thread.h>
#include <nautilus/syscalls/proc.h>

#define SYSCALL_NAME "sys_exit"
#include "impl_preamble.h"

#define FUTEX_WAKE 1

uint64_t sys_exit(uint64_t exit_status) {
  // https://man7.org/linux/man-pages/man2/set_tid_address.2.html
  uint32_t* clear_child_tid = get_cur_thread()->clear_child_tid;
  if (clear_child_tid) {
    *(uint64_t*)clear_child_tid = 0;
    sys_futex(clear_child_tid, FUTEX_WAKE, 1, NULL, NULL, 0);
  }

  nk_thread_exit((void*)exit_status);
  panic("Execution past thread exit\n");
  return 0;
}