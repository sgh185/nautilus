// Commented code is from kitten system call for reference
#include <nautilus/fs.h>
#include <nautilus/naut_types.h>
#include <nautilus/nautilus.h>

/// TODO: move macro to a unistd replacement
#define STDIN_FILENO 0

int sys_read(int fd, int buf, int len, int d, int e, int f) {
  int read_bytes;
  if (fd == STDIN_FILENO) {
    int i = 0;
    char s;
    while (i < len) {
      *(char*)buf = nk_vc_getchar();
      buf++;
      i++;
    }
    read_bytes = len;
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
