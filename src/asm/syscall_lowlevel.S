#include <asm/lowlevel.h>
#include <nautilus/syscall_kernel.h>

/*
Entry point for syscall handler
*/

.section .text
.code64

// SYSRET sets ring mode to 0. Workaround is manually storing restoring flags and jumping to the return address
// Interesting to see if register can directly be moved to RFLAGS

ENTRY(syscall_entry)
    pushf 
    SAVE_GPRS_SYSCALL()
    movq %rsp, %rdi
    callq nk_syscall_handler
    RESTORE_GPRS_EXCEPT_RAX()
    popf
    jmp *%rcx
