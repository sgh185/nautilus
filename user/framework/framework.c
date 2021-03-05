#include <stdint.h>
#include <stdlib.h>
#include <nautilus/nautilus_exe.h>

// Space for the signature of the final binary. Set by nsign after final link.
__attribute__((section(".naut_secure"))) unsigned char __NAUT_SIGNATURE[16];

// pointer to the NK function table
// to be filled in by the loader
void * (**__nk_func_table)(); 

extern int _start();

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

void* nk_func_table_access(volatile int entry_no, void* arg1, void* arg2) {
  return __nk_func_table[entry_no]((char*)arg1, arg2);
}

void nk_carat_instrument_global(void* ptr, uint64_t size, uint64_t global_ID) {
    __nk_func_table[NK_CARAT_INSTRUMENT_GLOBAL](ptr, size, global_ID);
}

void nk_carat_instrument_malloc(void* ptr, uint64_t size) {
    __nk_func_table[NK_CARAT_INSTRUMENT_MALLOC](ptr, size);
}

void nk_carat_instrument_calloc(void* ptr, uint64_t size) {
    __nk_func_table[NK_CARAT_INSTRUMENT_CALLOC](ptr, size);
}

void nk_carat_instrument_realloc(void* ptr, uint64_t size) {
    __nk_func_table[NK_CARAT_INSTRUMENT_REALLOC](ptr, size);
}

void nk_carat_instrument_free(void* ptr) {
    __nk_func_table[NK_CARAT_INSTRUMENT_FREE](ptr);
}

void nk_carat_instrument_escapes(void* ptr) {
    __nk_func_table[NK_CARAT_INSTRUMENT_ESCAPE](ptr);
}

void _nk_carat_globals_compiler_target(void) {
    __nk_func_table[NK_CARAT_GLOBALS_COMPILER_TARGET]();
}

void nk_carat_init(void) {
    __nk_func_table[NK_CARAT_INIT]();
}

__attribute__((noinline, used, annotate("nocarat")))
void kmem_sys_free(void *ptr) {
    free(ptr);
}

__attribute__((noinline, used, annotate("nocarat")))
void * _kmem_sys_malloc(uint64_t size) {
    return malloc(size);
}

/*
 WE NEED TO HANDLE THE ACCESS SIZE
 */
int nk_carat_check_protection(void* ptr, int is_write) {
    return __nk_func_table[NK_CARAT_CHECK_PROTECTION](ptr, is_write);
}

__attribute__((noinline, optnone, used, annotate("nocarat"))) void make_carat_pass_work(void) { return; }
