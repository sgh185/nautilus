#include <nautilus/nautilus.h>

uint64_t sys_set_tid_address(uint64_t tldptr) {
  nk_process_t* current_process = nk_process_current();

  get_cur_thread()->set_child_tid = tldptr;

  return nk_get_tid();
}