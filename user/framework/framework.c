#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nautilus/nautilus_exe.h>


// Space for the signature of the final binary. Set by nsign after final link.
__attribute__((section(".naut_secure"))) unsigned char __NAUT_SIGNATURE[16];

// pointer to the NK function table
// to be filled in by the loader
void * (**__nk_func_table)(); 

extern int _start();

__attribute__((noinline, used, annotate("nocarat")))
void __nk_exec_entry(void *in, void **out, void * (**table)())
{
    __nk_func_table = table;

    struct nk_crt_proc_args *crt_args = (struct nk_crt_proc_args*)(in);
    
    // Prepare to jump to the C runtime, which expects a specific stack configuration
    __asm__(
        "pushq $0\n" /* NULL after envp */
        "mov $0, %%rax\n" /* rax is the iterator */
        "nk_loader_crt_env_loop:\n"
        "mov (%2, %%rax, 8), %%rcx\n" /* Read members of envp, rcx is the pointer */
        "cmpq $0, %%rcx\n" /* If pointer is NULL, don't push and exit loop */
        "je nk_loader_crt_env_loop_done\n"
        "pushq %%rcx\n" /* Push env pointer to stack */
        "inc %%rax\n"
        "je nk_loader_crt_env_loop_done\n"
        "nk_loader_crt_env_loop_done:\n"
        "pushq $0\n" /* NULL after argv */
        "test %0, %0\n"
        "je nk_loader_crt_arg_loop_done\n"
        "mov %0, %%rax\n"
        "dec %%rax\n"
        "\n"
        "nk_loader_crt_arg_loop:\n"
        "pushq (%1, %%rax, 8)\n" /* Push members of argv */
        "dec %%rax\n"
        "cmpq $0, %%rax\n"
        "jge nk_loader_crt_arg_loop\n"
        "nk_loader_crt_arg_loop_done:\n"
        "pushq %0\n" /* argc */
        "movq $0, %%rdx\n" /* Shared library termination function, which doesn't exist.
                              RDX is the only gpr read by _start (other than RSP, which is implicitly used) */
        "jmp _start\n" 
        :
        : "r"((uint64_t)crt_args->argc), "r"(crt_args->argv), "r"(crt_args->envp)
        : "rax", "rcx"
    );
}

#ifdef USE_NK_MALLOC

__attribute__((malloc))
void* malloc(size_t x) {
    return __nk_func_table[NK_MALLOC](x);
}

__attribute__((malloc))
void* calloc(size_t num, size_t size) {
    const size_t total_size = num * size;
    void* allocation = __nk_func_table[NK_MALLOC](total_size);
    memset(allocation, 0, total_size);
    return allocation;
}

void free(void* x) {
    __nk_func_table[NK_FREE](x);
}

void* realloc(void* p, size_t s) {
    return __nk_func_table[NK_REALLOC](p, s);
}

#endif

__attribute__((noinline, used, annotate("nocarat")))
void * nk_func_table_access(volatile int entry_no, void *arg1, void *arg2) {
  return __nk_func_table[entry_no]((char*)arg1, arg2);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_instrument_global(void *ptr, uint64_t size, uint64_t global_ID) {
    __nk_func_table[NK_CARAT_INSTRUMENT_GLOBAL](ptr, size, global_ID);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_instrument_malloc(void *ptr, uint64_t size) {
    __nk_func_table[NK_CARAT_INSTRUMENT_MALLOC](ptr, size);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_instrument_calloc(void *ptr, uint64_t size_of_element, uint64_t num_elements) {
    __nk_func_table[NK_CARAT_INSTRUMENT_CALLOC](ptr, size_of_element, num_elements);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_instrument_realloc(void *ptr, uint64_t size, void *old_address) {
    __nk_func_table[NK_CARAT_INSTRUMENT_REALLOC](ptr, size, old_address);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_instrument_free(void *ptr) {
    __nk_func_table[NK_CARAT_INSTRUMENT_FREE](ptr);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_instrument_escapes(void *ptr) {
    __nk_func_table[NK_CARAT_INSTRUMENT_ESCAPE](ptr);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_guard_address(void *memory_address, int is_write) {
    __nk_func_table[NK_CARAT_GENERIC_PROTECT](memory_address, is_write);
}

__attribute__((noinline, used, annotate("nocarat")))
void nk_carat_guard_callee_stack(uint64_t stack_frame_size) {
    __nk_func_table[NK_CARAT_STACK_PROTECT](stack_frame_size);
}


#if 0
/*
 * WE NEED TO HANDLE THE ACCESS SIZE
 */
__attribute__((noinline, used, annotate("nocarat")))
int nk_carat_check_protection(void *ptr, int is_write) {
    return __nk_func_table[NK_CARAT_CHECK_PROTECTION](ptr, is_write);
}
#endif


