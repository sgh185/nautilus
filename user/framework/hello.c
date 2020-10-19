#include <stdio.h>
#include <unistd.h>

// #include <nautilus/nautilus_exe.h>

int main(void *in, void **out)
{
    // nk_vc_printf("Write using nk_vc_printf\n");

    char write_msg[] = "Write using write\n";
    write(STDOUT_FILENO, write_msg, sizeof(write_msg));

    /// TODO: figure out why puts and printf cause exceptions
    puts("Write using putc\n");

    // printf("Write using printf\n");
    return 42; // To test the return val
}
