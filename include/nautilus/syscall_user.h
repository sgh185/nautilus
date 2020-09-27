#include <nautilus/syscall_table.h>
#include <nautilus/syscall_kernel.h>


#define read(X, Y, Z) ({ SYSCALL(READ, X, Y, Z); })
#define write(X, Y, Z) ({ SYSCALL(WRITE, X, Y, Z); })
#define close(X) ({ SYSCALL(CLOSE, X); })
#define open(X,Y) ({ SYSCALL(OPEN, X, Y); })
#define stat(X, Y) ({ SYSCALL(STAT, X, Y); })
#define fstat(X, Y) ({ SYSCALL(FSTAT, X, Y); })
#define lseek(X, Y, Z) ({ SYSCALL(LSEEK, X, Y, Z); })
#define fork() ({ SYSCALL(FORK); })
#define exit(X) ({ SYSCALL(EXIT, X); })
#define ftruncate(X, Y) ({ SYSCALL(FTRUNCATE, X, Y); })
#define getpid() ({ SYSCALL(GETPID); })
