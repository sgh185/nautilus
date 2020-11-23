#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static volatile int GLOB = 1;

static void func1_a(void) { printf("A implementation\n"); }

static void func1_b(void) { printf("B implementation\n"); }

static void func1_c(void) { printf("C implementation\n"); }

asm(".type func1, %gnu_indirect_function");
void* func1(void) {
  if (GLOB) {
    return &func1_a;
  } else {
    return &func1_b;
  }
}

int main(int argc, char** argv) {
  char write_msg[] = "Write using write\n";
  write(STDOUT_FILENO, write_msg, sizeof(write_msg));

  /// TODO: figure out why puts and printf cause exceptions
  puts("Write using puts\n");

  printf("Write using printf\n");

  func1();
}
