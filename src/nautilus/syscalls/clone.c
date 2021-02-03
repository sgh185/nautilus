#include <nautilus/nautilus.h>
#include <nautilus/syscall_decl.h>
#include <nautilus/thread.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_clone: " fmt, ##args)

#define ARCH_SET_FS 0x1002

struct clone_compat_args {
  void* rsp;
  void* sysret_addr;
  void* tls;
};

/// Sends new thread back to the syscall return point, simulating a linux clone.
/// Assumes that the thread state other than rsp is set correctly for the clone
/// args.
void _clone_compat_wrapper(struct clone_compat_args* args) {
  void* rsp = args->rsp;
  void* sysret_addr = args->sysret_addr;
  sys_arch_prctl(ARCH_SET_FS, args->tls, NULL);
  free(args);
  nk_thread_t* me = get_cur_thread();
  me->fake_affinity = 0; // this should be wrapped into a process thread init function (shared with process initialization)
  me->vc = me->process->vc;
  
  DEBUG("Sending clone thread to RIP=%p RSP=%p and glibc thread function %p\n",
        sysret_addr, rsp, *(uint64_t*)rsp);
  /* We do not restore the flags, but this is ok in the most common case (glibc
   * clone), where the thread starts in a new function */
  asm("mov $0, %%rax\n"
      "mov %0, %%rsp\n"
      "jmp *%1\n"
      :
      : "r"(rsp), "r"(sysret_addr)
      : "rax");
  panic("Return from clone compat wrapper\n");
}

uint64_t sys_clone(uint64_t clone_flags, uint64_t newsp, uint64_t parent_tidptr,
                   uint64_t child_tidptr, uint64_t tls_val) {
  DEBUG("%p\n%p\n%p\n%p\n%p\n", clone_flags, newsp, parent_tidptr, child_tidptr,
        tls_val);

  if (clone_flags != 0x100) {
    DEBUG("Flags unsupported, but going to try anyway\n");
    // return -1;
  }

  nk_process_t* current_process = nk_process_current();
  if (!current_process) {
    panic("Call to sys_clone out of the context of a process.\n");
  }

  struct clone_compat_args* args = malloc(
      sizeof(struct clone_compat_args)); /* Free is handled by new thread */
  args->rsp = newsp;
  args->sysret_addr = get_cur_thread()->sysret_addr;
  args->tls = tls_val;

  nk_thread_t* thread;
  nk_process_t* process = get_cur_thread()->process;
  
  // Create the new thread that will handle clone.
  {
    uint32_t bound_cpu;
    spin_lock(&process->lock);
    process->last_cpu_thread = (process->last_cpu_thread + 1) % nk_get_num_cpus();
    bound_cpu = process->last_cpu_thread;
    spin_unlock(&process->lock);
    nk_thread_create(&_clone_compat_wrapper, args, NULL, 1, 0, &thread, bound_cpu);
    // TODO: there seem to be other things missing here (such as the vc)
    thread->process = process;
  }

  nk_thread_run(thread);
  // *(int*)child_tidptr = thread->tid;

  return thread->tid;
}