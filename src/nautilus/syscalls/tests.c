

#include <nautilus/fs.h>
#include <nautilus/loader.h>
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/syscalls/runtime.h>

#define ERROR(fmt, args...) ERROR_PRINT("sycall_test: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_test: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("syscall_test: " fmt, ##args)

#define EXPECT(x)                  \
  if (!(x)) {                      \
    ERROR("Expectation failed\n"); \
    total_tests++;                 \
  } else {                         \
    total_tests++;                 \
    passed_tests++;                \
  }

extern void syscall_test_main();

static int passed_tests;
static int total_tests;

static int handle_syscall_tests(char* buf, void* priv) {
  INFO("Shell command for testing file system system calls\n");
  INFO("%s\n", buf);

  /// Test setup
  passed_tests = 0;
  total_tests = 0;

  // // Test read / wrtie on fd [0,2]
  // {
  //   char msg_stdout[] = "This is a print to 'stdout'\n";
  //   char msg_stderr[] = "This is a print to 'stderr'\n";
  //   EXPECT(write(1, msg_stdout, sizeof(msg_stdout)) == sizeof(msg_stdout));
  //   EXPECT(write(2, msg_stderr, sizeof(msg_stderr)) == sizeof(msg_stderr));

  //   char read_buffer[8] = {0};
  //   printk("Please enter 7 chars to test read from stdin: ");
  //   EXPECT(read(0, read_buffer, 7) == 7);
  //   printk("\nYour input: ");
  //   EXPECT(write(1, read_buffer, strlen(read_buffer)) ==
  //   strlen(read_buffer)); printk("\n");
  // }

  // // Test open / read / write / close on files
  // {
  //   // TODO: Don't print ERROR here once debug print works
  //   int fn = open("/new_file2", 3 | 8); // RW/Create
  //   EXPECT(fn != -1);
  //   char wr_buf[] = "this used to not exist";

  //   int wr_bytes = write(fn, wr_buf, sizeof(wr_buf));

  //   EXPECT(wr_bytes == sizeof(wr_buf));

  //   char read_buf[32] = {0};
  //   EXPECT(read(fn, read_buf, 32) == sizeof(wr_buf));

  //   EXPECT(strcmp(read_buf, wr_buf) == 0);

  //   // This test isn't useful until we can write to the fs
  //   // struct nk_fs_stat statbuf;
  //   // memset(&statbuf, 0, sizeof(struct nk_fs_stat));
  //   // ERROR("Stat result: %d\n", fstat(fn, &statbuf));
  //   // ERROR("Stat size: %d\n", statbuf.st_size);

  //   // EXPECT(close(fn) == 0);
  // }

  // // Test mkdir
  // { EXPECT(mkdir("/test_dir", 0) == 0); }

  // // Test fork / exit
  // {
  //   // TODO: Don't print ERROR here once debug print works
  //   int f_val = fork();
  //   int tid = getpid();
  //   DEBUG("Return value of fork()/getpid(): %d/%d\n", f_val, tid);
  //   if (f_val == 0) {
  //     DEBUG("In forked thread\n");
  //     exit(0);
  //   }
  // }

  // /// get/set timeofday; nanosleep; clock_getres (etc)
  // {
  //   /// TODO: include from wherever this is defined correctly
  //   struct timeval {
  //     uint64_t tv_sec;  /* seconds */
  //     uint64_t tv_usec; /* microseconds */
  //   };
  //   struct timeval timev;

  //   EXPECT(gettimeofday(&timev, NULL) == 0);
  //   printk("Initial time of day in s: %d\n", timev.tv_sec);

  //   const int time_to_set = 100000;
  //   timev.tv_sec = time_to_set;
  //   EXPECT(settimeofday(&timev, NULL) == 0);

  //   timev.tv_sec = 0; // just to be sure gettimeofday updates it
  //   EXPECT(gettimeofday(&timev, NULL) == 0);
  //   EXPECT(timev.tv_sec >= time_to_set);
  //   printk("Modified time of day in s: %d\nSleeping...\n", timev.tv_sec);

  //   /// TODO: include from wherever this is defined correctly
  //   struct timespec {
  //     uint64_t tv_sec;  /* seconds */
  //     uint64_t tv_nsec; /* nanoseconds */
  //   };

  //   struct timespec sleep_time;
  //   sleep_time.tv_sec = 1;
  //   sleep_time.tv_nsec = 0;

  //   struct timespec clock_time = {.tv_sec = 0, .tv_nsec = 0};

  //   EXPECT(clock_getres(0, &clock_time) == 0);
  //   EXPECT((clock_time.tv_nsec != 0) || (clock_time.tv_sec != 0));

  //   uint64_t pre_sleep_time = clock_time.tv_sec;

  //   nanosleep(&sleep_time, NULL);
  //   gettimeofday(&timev, NULL);
  //   printk("Time of day after sleeping: %d\n", timev.tv_sec);

  //   clock_time.tv_sec = 0;
  //   clock_time.tv_nsec = 0;

  //   EXPECT(clock_gettime(0, &clock_time) == 0);
  //   uint64_t post_sleep_time = clock_time.tv_sec;
  //   EXPECT(post_sleep_time > pre_sleep_time);
  // }

  // // Simple get* style syscalls and uname/hostname
  // {
  //   EXPECT(gettid() != 0);
  //   EXPECT(getpid() != 0);

  //   unsigned cpu, node;
  //   EXPECT(getcpu(&cpu, &node, NULL) == 0);
  //   EXPECT(cpu == my_cpu_id());

  //   char newhostname[] = "host.northwestern.edu";
  //   EXPECT(sethostname(newhostname, sizeof(newhostname)) == 0);

  //   struct utsname uname_data;
  //   EXPECT(uname(&uname_data) == 0);
  //   EXPECT(strcmp(uname_data.sysname, "Nautilus") == 0);
  //   EXPECT(strcmp(uname_data.nodename, newhostname) == 0);
  // }

  // struct nk_exec* e = nk_load_exec("/hello.exe");
  // if (e) {
  //   char* argv[] = {"/fbench", "1"};
  //   EXPECT(nk_start_exec(e, 2, &argv, NULL) == 42);
  //   nk_unload_exec(e); // will also free this
  // }

  /// Test teardown
  printk("\nPassed %d of %d tests.\nNote that at this time, 3 tests are "
         "expected to fail\n",
         passed_tests, total_tests);
  return 0;
}

static struct shell_cmd_impl syscalltest_impl = {

    .cmd = "syscalltest",

    .help_str = "syscalltest (run system call tests)",

    .handler = handle_syscall_tests,

};

nk_register_shell_cmd(syscalltest_impl);

static uint64_t omp_num_threads = 0;

static int handle_exec(char* buf, void* priv) {
  int argc = 0;
  char* argv[64] = {0};
  char* argp = buf;
  argv[0] = buf;
  for (argc = 1; argc < 64; argc++) {
    while (*argp != ' ' && *argp != 0) {
      argp++;
    }
    if (*argp == 0) {
      break;
    }
    *argp = 0;
    argp++;
    argv[argc] = argp;
  }

  // struct nk_exec* e = nk_load_exec(argv[1]);
  // if (e) {
  //   nk_start_exec(e, argc - 1,
  //                     argv + 1); /* Programs will expect argv[0] to be the
  //                                   program name, so must modify this */
  //   nk_unload_exec(e);
  // }

  nk_process_t* process;
  char omp_threads[64] = {0};

  if (omp_num_threads == 0) {
    // The number of threads was not explicitly set.
    omp_num_threads = nk_get_num_cpus();
  }

  nk_vc_printf("Going to start a process with OMP_NUM_THREADS=%d\n",
               omp_num_threads);

  sprintf((char*)&omp_threads, "OMP_NUM_THREADS=%d", omp_num_threads);
  char* envp[] = {(char*)&omp_threads, "OMP_DISPLAY_ENV=\"TRUE\"", NULL};
  if (nk_process_create(argv[1], argv + 1, envp, "paging", &process)) {
    nk_vc_printf("Failed to create new process\n");
    return -1;
  }
  if (nk_process_run(process, 0)) {
    nk_vc_printf("Failed to run process\n");
    return -1;
  }
  nk_vc_printf("Started %s in the background.\n", argv[1]);
  return 0;
}

static struct shell_cmd_impl exec_impl = {

    .cmd = "exec",

    .help_str = "execute a process",

    .handler = handle_exec,

};

static int handle_omp_num_threads(char* buf, void* priv) {
  uint64_t num_threads;
  sscanf(buf, "omp_num_threads %ld", &num_threads);
  if (num_threads == 0) {
    nk_vc_printf("0 threads makes no sense; doing nothing.\n");
    return 0;
  } else if (num_threads > nk_get_num_cpus()) {
    nk_vc_printf(
        "WARNING: Setting number of threads (%ld) greater than the detected "
        "number of CPUs (%d). This may not be handled correctly by the "
        "process/syscall abstraction when spawning new threads.\n",
        num_threads, nk_get_num_cpus());
  }
  omp_num_threads = num_threads;
  return 0;
}

static struct shell_cmd_impl omp_num_threads_impl = {
    .cmd = "omp_num_threads",
    .help_str = "set the number of omp threads",
    .handler = handle_omp_num_threads,
};

nk_register_shell_cmd(exec_impl);
nk_register_shell_cmd(omp_num_threads_impl);