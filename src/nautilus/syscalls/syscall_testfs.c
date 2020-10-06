

#include <nautilus/fs.h>
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/syscall_user.h>

#define ERROR(fmt, args...) ERROR_PRINT("sycall_test: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_test: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("syscall_test: " fmt, ##args)

#define EXPECT(x)                  \
  if (!(x)) {                      \
    ERROR("Expectation failed\n"); \
  }

static int handle_syscall_testfs(char* buf, void* priv) {
  INFO_PRINT("Shell command for testing file system system calls\n");
  INFO_PRINT("%s\n", buf);

  // Test open / read / write / close
  {
    // TODO: Don't print ERROR here once debug print works
    int fn = open("fs:/new_file2", 3 | 8); // RW/Create
    char* wr_buf = "this used to not exist";

    int wr_bytes = write(fn, wr_buf, 22);

    EXPECT(wr_bytes == 22);

    char read_buf[32] = {0};
    if (read(fn, read_buf, 32)) {
      ERROR("Read returned non-zero\n");
    }

    // ERROR("Content of buffer: %s\n", read_buf);

    EXPECT(strcmp(read_buf, wr_buf) == 0);

    /* // This will crash if the fs is not loaded; TODO: enable
    struct nk_fs_stat statbuf;
    memset(&statbuf, 0, sizeof(struct nk_fs_stat));
    ERROR("Stat result: %d\n", fstat(fn, &statbuf));
    ERROR("Stat size: %d\n", statbuf.st_size);

    EXPECT(close(fn) == 0);
    */
  }

  // Test fork / exit
  {
    // TODO: Don't print ERROR here once debug print works
    int f_val = fork();
    int tid = getpid();
    ERROR("Return value of fork()/getpid(): %d/%d\n", f_val, tid);
    if (f_val == 0) {
      exit(0);
    }
  }

  // get/set timeofday
  {
    /// TODO: include from wherever this is defined correctly
    struct timeval {
      int tv_sec;  // seconds
      int tv_usec; // microseconds
    };
    struct timeval time;
    gettimeofday(&time, NULL);
    printk("Initial time of day in s: %d\n", time.tv_sec);
    time.tv_sec = 100000;
    settimeofday(&time, NULL);
    time.tv_sec = 0; // just to be sure gettimeofday updates it
    gettimeofday(&time, NULL);
    printk("Modified time of day in s: %d\n", time.tv_sec);
  }

  return 0;
}

static struct shell_cmd_impl syscallfs_impl = {

    .cmd = "syscallfs",

    .help_str = "syscallfs (test system calls for fs, no args)",

    .handler = handle_syscall_testfs,

};

nk_register_shell_cmd(syscallfs_impl);
