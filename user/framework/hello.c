#include <stdio.h>
#include <stdint.h>

#define NAUTILUS_EXE
#include <nautilus/nautilus_exe.h>

// 0x1400000UL 

int main() {
    printf("Hello, world!\n");
    void* ptrs[500] = {0};
    for (int i = 0; i < 500; i++) {
        printf("Iteration %d\n", i);
        fflush(stdout);
        const int size = 0x1400000UL / 500;
        ptrs[i] = malloc(size);
        *(uint64_t*)ptrs[i] = i;
    }
    for (int i = 0; i < 500; i++) {
        printf("%d: %d\n", i, *(uint64_t*)ptrs[i]);
    }
    return 0;
}