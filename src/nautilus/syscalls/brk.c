#include <nautilus/nautilus.h>
#include <nautilus/process.h>

#define HEAP_BOT 0x100000000 /* Lowest virtual address for the process heap */
#define HEAP_SIZE_INCREMENT \
  0x1400000 /* Heap is increased by a multiple of this amount */

#ifndef MIN
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#endif

/// @param brk The process's requested new max data segment address, or zero.
/// @return If param brk is 0, returns the beginning of the data segment,
/// creating one if it doesn't exist. If param brk is non-zero, return the
/// requested segment end if possible, or otherwise return the current program
/// segment end.
///
/// @todo this currently causes a memory leak and doesn't use the allocator system, which is wrong
uint64_t sys_brk(const uint64_t brk) {

  nk_vc_printf("BRK arg is %p\n", brk);

  nk_process_t* current_process = nk_process_current();
  if (!current_process) {
    panic("Call to sys_brk out of the context of a process.\n");
  }

  spin_lock(&current_process->lock);

  if (current_process->heap_end == 0) {
    // This is the first call to brk.
    current_process->heap_begin = malloc(HEAP_SIZE_INCREMENT);
    current_process->heap_end =
        current_process->heap_begin + HEAP_SIZE_INCREMENT;
    // goto ret;
  } else {
    // Some memory has already been allocated
    if ((void*)brk > current_process->heap_end) {
      nk_vc_printf("Attempt to set brk beyond allocated range, which is "
                   "unimplemented\n");
    }
    // goto ret;
  }

  // ret:

  uint64_t retval;
  if (brk) {
    retval = MIN(brk, (uint64_t)current_process->heap_end);
  } else {
    retval = current_process->heap_begin;
  }
  spin_unlock(&current_process->lock);
  nk_vc_printf("BRK returning %p\n", retval);
  return (uint64_t)retval;
}