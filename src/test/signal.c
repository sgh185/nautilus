#include <nautilus/nautilus.h>
#include <nautilus/process.h>
#include <nautilus/shell.h>

#define DO_PRINT 1

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...)
#endif

void
sig_thread1 (void *in, void **out)
{
  int n = 0;
  nk_thread_t *me = get_cur_thread();
  while (1) {
    if (n == 10000) {
        nk_vc_printf("Yeah, I'm looping! From thread: %p.\n", me); 
    }
    n++;
  }

  return;
}

void
sig_thread2 (void *in, void **out)
{
    nk_thread_t *thread1 = (nk_thread_t*)in;
    nk_vc_printf("Sending signal to thread: %p.\n", thread1);
    if (nk_signal_send(12, 0, thread1, SIG_DEST_TYPE_THREAD)) {
        nk_vc_printf("Couldn't send signal. Sigtest failed.\n");
        return;
    }
    if (nk_signal_send(17, 0, thread1, SIG_DEST_TYPE_THREAD)) {
        nk_vc_printf("Couldn't send signal. Sigtest failed.\n");
        return;
    }
    nk_vc_printf("Sent signal... recieve it please!\n"); 
}


// create two threads, have 1 thread signal the other
static int
handle_sigtest (char * buf, void * priv)
{
    nk_thread_t *thread1 = 0;
    nk_thread_t *thread2 = 0;

    if (nk_thread_create(sig_thread1, 0, 0, 0, 0, (void **)&thread1, -1)) {
        nk_vc_printf("handle_sigtest: Failed to create new thread\n");
        return -1;
    }
    if (nk_thread_run(thread1)) {
        nk_vc_printf("handle_sigtest: Failed to run thread 1\n");
        return -1;
    }

    if (nk_thread_create(sig_thread2, (void *)thread1, 0, 0, 0, (void **)&thread2, -1)) {
        nk_vc_printf("handle_sigtest: Failed to create new thread\n");
        return -1;
    }
    if (nk_thread_run(thread2)) {
        nk_vc_printf("handle_sigtest: Failed to run thread 2\n");
        return -1;
    }
  
    return 0;
}

static struct shell_cmd_impl signal_test_impl = {
  .cmd      = "sigtest",
  .help_str = "sigtest",
  .handler  = handle_sigtest,
};

nk_register_shell_cmd(signal_test_impl);
