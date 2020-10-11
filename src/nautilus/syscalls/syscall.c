#include <nautilus/irq.h>

#include <nautilus/msr.h>
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/syscall.h>
#include <nautilus/syscall_kernel.h>
#include <nautilus/thread.h>

#define ERROR(fmt, args...) ERROR_PRINT("syscall: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("syscall: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("syscall: " fmt, ##args)
#define MAX_SYSCALL 301

typedef int (*syscall_t)(int, int, int, int, int, int);
syscall_t syscall_table[MAX_SYSCALL];

extern void syscall_entry(void);
void init_syscall_table() {

  int i;

  for (i = 0; i < MAX_SYSCALL; i++) {
    syscall_table[i] = 0;
  }

  syscall_table[READ] = (syscall_t)&sys_read;
  syscall_table[WRITE] = (syscall_t)&sys_write;
  syscall_table[OPEN] = (syscall_t)&sys_open;
  syscall_table[CLOSE] = (syscall_t)&sys_close;
  syscall_table[STAT] = (syscall_t)&sys_stat;
  syscall_table[FSTAT] = (syscall_t)&sys_fstat;
  syscall_table[LSEEK] = (syscall_t)&sys_lseek;
  syscall_table[FORK] = (syscall_t)&sys_fork;
  syscall_table[EXIT] = (syscall_t)&sys_exit;
  syscall_table[FTRUNCATE] = (syscall_t)&sys_ftruncate;
  syscall_table[GETPID] = (syscall_t)&sys_getpid;
  syscall_table[GETTIMEOFDAY] = (syscall_t)&sys_gettimeofday;
  syscall_table[SETTIMEOFDAY] = (syscall_t)&sys_settimeofday;
  syscall_table[MMAP] = (syscall_t)&sys_mmap;
  syscall_table[MPROTECT] = (syscall_t)&sys_mprotect;
  syscall_table[MUNMAP] = (syscall_t)&sys_munmap;
  syscall_table[NANOSLEEP] = (syscall_t)&sys_nanosleep;

  return;
}

int int80_handler(excp_entry_t *excp, excp_vec_t vector, void *state) {

  struct nk_regs *r = (struct nk_regs *)((char *)excp - 128);
  int syscall_nr = (int)r->rax;
  INFO("Inside syscall irq handler\n");
  if (syscall_table[syscall_nr] != 0) {
    r->rax =
        syscall_table[syscall_nr](r->rdi, r->rsi, r->rdx, r->r10, r->r8, r->r9);
  } else {
    INFO("System Call not Implemented!!");
  }
  return 0;
}

uint64_t nk_syscall_handler(struct nk_regs *r) {
  INFO("Inside syscall_syscall handler\n");
  int syscall_nr = (int)r->rax;
  if (syscall_table[syscall_nr] != 0) {
    r->rax =
        syscall_table[syscall_nr](r->rdi, r->rsi, r->rdx, r->r10, r->r8, r->r9);
        INFO("%d\n",r->rax);
  } else {
    INFO("System Call not Implemented!!");
  }
  return r->rax;
}

int syscall_setup() {
  uint64_t r = msr_read(IA32_MSR_EFER);
  r |= EFER_SCE;
  msr_write(IA32_MSR_EFER, r);

  /* SYSCALL and SYSRET CS in upper 32 bits */
  msr_write(AMD_MSR_STAR,
            ((0x8llu & 0xffffllu) << 32) | ((0x8llu & 0xffffllu) << 48));

  /* target address */
  msr_write(AMD_MSR_LSTAR, (uint64_t)syscall_entry);
  return 0;
}

void nk_syscall_init() {
  register_int_handler(0x80, int80_handler, 0);
  syscall_setup();
}

// handle shell command

static int handle_syscall_test(char *buf, void *priv) {
  INFO("Shell command for testing syscall invoked\n");
  INFO("%s\n", buf);

  char syscall_name[32],syscall_argument[32];

  if (sscanf(buf,"syscall %s %s",syscall_name, syscall_argument)!=2) {
      INFO("No arguments\n");
  }
  else if (sscanf(buf,"syscall %s",syscall_name)!=1) {
      INFO("Don't understand %s\n",buf);
  }

  if (strcmp(syscall_name, "getpid") == 0) {
    uint64_t pid = SYSCALL(39, 0, 0, 0, 0, 0, 0);
    nk_vc_printf("%ld\n", pid);
  }

  else if (strcmp(syscall_name, "test") == 0) {
    uint64_t pid = SYSCALL(39, 0, 0, 0, 0, 0, 0);
    nk_vc_printf("%ld\n", pid);
  }

  else if (strcmp(syscall_name, "exit") == 0) {
    uint64_t pid = SYSCALL(60, 0, 0, 0, 0, 0, 0);
    nk_vc_printf("%ld\n", pid);
  }

  else if (strcmp(syscall_name, "fork") == 0) {
    uint64_t pid = SYSCALL(57, 0, 0, 0, 0, 0, 0);
    nk_vc_printf("%ld\n", pid);
  }

  else if (strcmp(syscall_name, "write") == 0) {
    uint64_t pid = SYSCALL(1, 1, syscall_argument, strlen(syscall_argument), 0, 0, 0);
    nk_vc_printf("%ld\n", pid);
  }

  else if (strcmp(syscall_name, "read") == 0) {
    char* buf = "";
    uint64_t pid = SYSCALL(0, 0, (int)buf, atoi(syscall_argument), 0, 0, 0);
    nk_vc_printf("%s\n", buf);
  }

  else {
    syscall_int80(MAX_SYSCALL - 1, 0, 0, 0, 0, 0, 0);
  }

  return 0;
}

static struct shell_cmd_impl syscall_impl = {

    .cmd = "syscall",

    .help_str = "syscall [test]",

    .handler = handle_syscall_test,

};

nk_register_shell_cmd(syscall_impl);
