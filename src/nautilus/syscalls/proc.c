#include <nautilus/syscalls/proc.h>

#define SYSCALL_NAME "proc"
#include "impl_preamble.h"

struct nk_fs_open_file_state* fd_to_nk_fs(struct file_descriptor_table* table,
                                          fd_t fd) {
  for (size_t i = 0; i < SYSCALL_NUM_FDS; i++) {
    if (table->fds[i].fd_no == fd) {
      return table->fds[i].nk_fs_ptr;
    }
  }
  return NULL;
}

fd_t fd_add(struct file_descriptor_table* table, struct nk_fs_open_file_state* nk_fd) {
  
  if (table->next_fd_index >= SYSCALL_NUM_FDS) {
    ERROR("Can't support more fds, please add a better data structure.\n");
    while(1)
      ;
  }

  struct file_descriptor* fd_table_entry = &table->fds[table->next_fd_index];

  fd_table_entry->fd_no = table->next_fd_index + 3; // offset for stdio fds, which are implicit
  fd_table_entry->nk_fs_ptr = nk_fd;

  table->next_fd_index += 1;  

  return fd_table_entry->fd_no;
}