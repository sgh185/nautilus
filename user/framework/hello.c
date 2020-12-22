#define _GNU_SOURCE

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define NAUTILUS_EXE
#include <nautilus/nautilus_exe.h>

static volatile int GLOB = 1;

static void func1_a(void) { printf("A implementation\n"); }

static void func1_b(void) { printf("B implementation\n"); }

//static void func1_c(void) { printf("C implementation\n"); }

asm(".type func1, %gnu_indirect_function");
void* func1(void) {
  if (GLOB) {
    return &func1_a;
  } else {
    return &func1_b;
  }
}

int newfunc(void* x) {
  printf("Hello, new thread here. arg is %p\n", x);
  fflush(stdout);
  exit(0);
  return 0;
}

volatile int thread_i = -5;
int ptfunc(void* x) {
  printf("Hello, new thread here (using pthread!). arg is %p\n", x);
  fflush(stdout);
  thread_i = x;
  x = thread_i;
  printf("My th_i = %d\n", thread_i);
  return 0;
}

char stack[0x20000];

int main(int argc, char** argv, char** envp) {

  nk_vc_printf("Hello from vc_printf %d\n", 1);

  char write_msg[] = "Write using write\n";
  write(STDOUT_FILENO, write_msg, sizeof(write_msg));

  /// TODO: figure out why puts and printf cause exceptions
  puts("Write using puts\n");

  printf("Write using printf\n");

  func1();

  if (envp[0]) {
    printf("envp[0] = %s\n", envp[0]);
  }

  printf("Addr is: %p\nStack is %p\n", newfunc, stack + 0x20000);
  
  if (clone(newfunc, stack + 0x20000, CLONE_VM, (void*)0xDEAFBEEFUL) == -1) {
    printf("Error\n");
  }

  pthread_t tid;
  printf("Going to make a pthread\n");
  for (int i = 0; i < 50; i++) {
    if (pthread_create(&tid, NULL, ptfunc, (void*)i)) {
      printf("Error making pthread\n");
    }
  }
  // pthread_join(tid, NULL);
  // printf("pthread joined\n");

  fflush(stdout);
  write(STDOUT_FILENO, write_msg, sizeof(write_msg));

  while (1)
    ;
}
