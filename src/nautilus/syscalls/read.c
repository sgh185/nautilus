// Commented code is from kitten system call for reference
#include <nautilus/fs.h>
#include <nautilus/naut_types.h>
#include <nautilus/nautilus.h>

/// TODO: move macro to a unistd replacement
#define STDIN_FILENO 0

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_read: " fmt, ##args)

uint64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len, uint64_t d, uint64_t e, uint64_t f) {
  int read_bytes;
  DEBUG("Called read with fd %d, buf %p, len %d", fd, buf, len);
  if (fd == STDIN_FILENO) {
    int i = 0;
    char s;
    while (i < len) {
      *(char*)buf = nk_vc_getchar();
      i++;
      if (*(char*)buf == '\n') {
        return i;
      }
      buf++;
    }
    read_bytes = i;
    return read_bytes;
  } else {
    read_bytes = (int)nk_fs_read((struct nk_fs_open_file_state*)fd, (void*)buf,
                          (ssize_t)len);
  }
  // open file from descriptor. I have assumed the fd to be a pointer to
  // nk_fs_open_file_state This write assumes a fs is mounted.
  // struct file * const file = get_current_file(fd);
  return read_bytes;
}
