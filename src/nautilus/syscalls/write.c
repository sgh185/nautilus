// Commented code is from kitten system call for reference
#include <nautilus/fs.h>
#include <nautilus/naut_types.h>
#include <nautilus/nautilus.h>

/// TODO: move macros to a unistd replacement
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int sys_write(int fd, int buf, int len, int d, int e, int f) {
  unsigned long flags;
  int ret = -1;

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    int i = 0;
    while (i < len) {
      nk_vc_putchar(*(char*)buf);
      buf++;
      i++;
    }
    ret = len;
  } else {
    ret = (int)nk_fs_write((struct nk_fs_open_file_state*)fd, (void*)buf,
                           (ssize_t)len);
  }
  // open file from descriptor. I have assumed the fd to be a pointer to
  // nk_fs_open_file_state This write assumes a fs is mounted.
  // struct file * const file = get_current_file(fd);
  return ret;
}
