#include <nautilus/nautilus.h>
#include <nautilus/thread.h>

/// When a process abstraction is added, this needs to be updated
uint64_t sys_getpid(){
  nk_thread_t *thread_id = get_cur_thread();
  uint64_t tid = thread_id->tid;
  return tid;
}
