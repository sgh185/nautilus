#include <nautilus/nautilus.h>

static void* cur_brk = 0;
static void* original_brk_max = 0;

uint64_t sys_brk(uint64_t brk) {
  const unsigned heap_size = 2 * 1024 * 1024;
  if (!cur_brk) {
    cur_brk = malloc(heap_size);
    original_brk_max = cur_brk + heap_size;
    return cur_brk;
  }
  if (brk > original_brk_max) {
    nk_vc_printf("Attempt to set brk beyond allocated range\n");
    return -1;
  }
  cur_brk = brk;
  return cur_brk;
}