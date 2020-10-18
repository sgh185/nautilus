#include <stdio.h>

#include <nautilus/nautilus_exe.h>

int main(void *in, void **out)
{
    nk_vc_printf("Got here\n");
    printf("Hello, world, from the user program\n");
    return 0;
}
