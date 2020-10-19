#include <nautilus/nautilus.h>
#include <nautilus/thread.h>

uint64_t sys_gettid(){
  nk_thread_t *thread_id = get_cur_thread();
  uint64_t tid = thread_id->tid;
  return tid;
}
