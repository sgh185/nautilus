#include <stdio.h>
#include <unistd.h>

// #include <nautilus/nautilus_exe.h>

int main(int argc, char** argv)
{
    // nk_vc_printf("Write using nk_vc_printf\n");

    char write_msg[] = "Write using write\n";
    write(STDOUT_FILENO, write_msg, sizeof(write_msg));

    dprintf(STDOUT_FILENO, "Write using dprintf\n");

    puts("Write using puts\n");
    printf("Write using printf\n");
    
    if (argc > 1) {
        printf("Arg0: %s\nArg1: %s\n", argv[0], argv[1]);
    }
    return 42; // To test the return val
}
