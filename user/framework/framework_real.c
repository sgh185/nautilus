#include <stdint.h>
#include <nautilus/nautilus_exe.h>

// pointer to the NK function table
// to be filled in by the loader
void * (**__nk_func_table)(); 

extern int main(int, char**);

extern int _start();

int __nk_exec_entry(void *in, void **out, void * (**table)())
{
    __nk_func_table = table;
    
    return _start();
}

void* nk_func_table_access(volatile int entry_no, void* arg1, void* arg2) {
  return __nk_func_table[entry_no]((char*)arg1, arg2);
}

void __nk_carat_instrument_malloc(void* ptr, uint64_t size) {
    __nk_func_table[NK_CARAT_INSTRUMENT_MALLOC](ptr, size);
}

void __nk_carat_instrument_free(void* ptr) {
    __nk_func_table[NK_CARAT_INSTRUMENT_FREE](ptr);
}

void __nk_carat_instrument_escape(void* ptr) {
    __nk_func_table[NK_CARAT_INSTRUMENT_ESCAPE](ptr);
}

/*
 WE NEED TO HANDLE THE ACCESS SIZE
 */
int __nk_carat_check_protection(void* ptr, int is_write) {
    return __nk_func_table[NK_CARAT_CHECK_PROTECTION](ptr, is_write);
}

__attribute__((noinline, optnone, used, annotate("nocarat"))) void make_carat_pass_work(void) { return; }
