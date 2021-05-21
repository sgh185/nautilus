#include <nautilus/aspace.h>
#include <nautilus/nautilus.h>
#include <nautilus/process.h>

#define SYSCALL_NAME "sys_brk"
#include "impl_preamble.h"

#define HEAP_BOT \
  (void*)KERNEL_ADDRESS_END /* Lowest virtual address for the process heap */
#define HEAP_SIZE_INCREMENT \
  0x2000000UL /* Heap is increased by a multiple of this amount */

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

/// @param brk The process's requested new max data segment address, or zero.
/// @return If param brk is 0, returns the beginning of the data segment,
/// creating one if it doesn't exist. If param brk is non-zero, return the
/// requested segment end if possible, or otherwise return the current program
/// segment end.
uint64_t sys_brk(const uint64_t brk) {

  nk_process_t* current_process = syscall_get_proc();

  DEBUG("Called with brk=%p\n", brk);

  spin_lock(&current_process->lock);

  if (current_process->heap_end == 0) {
    // This is the first call to brk.
    void* new_heap = malloc(HEAP_SIZE_INCREMENT);
    if (!new_heap) {
      // Something terrible has happened. This may not be the correct response,
      // but the program will fail anyway.
      goto out;
    }
    nk_aspace_region_t heap_expand;
    heap_expand.va_start = (void*)HEAP_BOT;
    heap_expand.pa_start = new_heap;
    heap_expand.len_bytes = HEAP_SIZE_INCREMENT;
    heap_expand.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE |
                                NK_ASPACE_EXEC | NK_ASPACE_PIN |
                                NK_ASPACE_EAGER;
    if (nk_aspace_add_region(syscall_get_proc()->aspace, &heap_expand)) {
      nk_vc_printf("Fail to allocate initial heap to aspace\n");
      free(new_heap);
      goto out;
    }
    current_process->heap_begin = HEAP_BOT;
    current_process->heap_end =
        current_process->heap_begin + HEAP_SIZE_INCREMENT;
  } else {
    // Some memory has already been allocated
    if ((void*)brk > current_process->heap_end) {
      void* new_heap = malloc(HEAP_SIZE_INCREMENT);
      if (!new_heap) {
        goto out;
      }
      nk_aspace_region_t heap_expand;
      heap_expand.va_start = current_process->heap_end;
      heap_expand.pa_start = new_heap;
      heap_expand.len_bytes = HEAP_SIZE_INCREMENT;
      heap_expand.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE |
                                  NK_ASPACE_EXEC | NK_ASPACE_PIN |
                                  NK_ASPACE_EAGER;
      if (nk_aspace_add_region(syscall_get_proc()->aspace, &heap_expand)) {
        nk_vc_printf("Fail to allocate more heap to aspace\n");
        free(new_heap);
        goto out;
      }
      current_process->heap_end += HEAP_SIZE_INCREMENT;
    }
  }

out:; // empty statement required to allow following declaration
  uint64_t retval;
  if (brk) {
    retval = MIN(brk, (uint64_t)current_process->heap_end);
  } else {
    retval = (uint64_t)current_process->heap_begin;
  }
  spin_unlock(&current_process->lock);
  DEBUG("Going to return %p\n", retval);
  return retval;
}