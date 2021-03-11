/*
 * Interface between process/thread and syscall interface
 */

#ifndef _SYSCALL_PROC
#define _SYSCALL_PROC

#include <nautilus/list.h>

// On Linux, this is always(?) an int
typedef int fd_t;

#define SYSCALL_INVALID_FD (fd_t)(-1)

// Number of static FDs supported. Should be replaced with another data structure as needed.
#define SYSCALL_NUM_FDS 16

struct file_descriptor {
  struct nk_fs_open_file_state* nk_fs_ptr;
  fd_t fd_no;
};

// Public-ish interface for file descriptors, add optimizations here
struct file_descriptor_table {
  struct file_descriptor fds[SYSCALL_NUM_FDS];
  size_t next_fd_index;
};

struct nk_thread_linux_syscall_state {
  
};

struct nk_process_linux_syscall_state {
  struct file_descriptor_table fd_table;
};

/// @param table The table to lookup in
/// @param fd The file descriptor number to look for
/// @return The Nautilus file state if it exists; else NULL
struct nk_fs_open_file_state* fd_to_nk_fs(struct file_descriptor_table* table, fd_t fd);

/// Inserts a Nautilus open file into the fd table, returning the lowest
/// unutilized fd number, or -1 if something goes wrong
fd_t fd_add(struct file_descriptor_table* table, struct nk_fs_open_file_state* nk_fd);

#endif // _SYSCALL_PROC
