/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2020, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2020, Aaron R. Nelson <arn@u.northwestern.edu>
 * Copyright (c) 2020, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Aaron R. Nelson <arn@northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/process.h>
#include <nautilus/thread.h>
#include <nautilus/printk.h>

#ifndef NAUT_CONFIG_DEBUG_PROCESSES
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define PROCESS_INFO(fmt, args...) INFO_PRINT("process: " fmt, ##args)
#define PROCESS_ERROR(fmt, args...) ERROR_PRINT("process: " fmt, ##args)
#define PROCESS_DEBUG(fmt, args...) DEBUG_PRINT("process: " fmt, ##args)
#define PROCESS_WARN(fmt, args...)  WARN_PRINT("process: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("process: " fmt, ##args)

/* Macros for locking and unlocking processs */
#define _LOCK_PROCESS(proc) spin_lock(&(proc->lock))
#define _UNLOCK_PROCESS(proc) spin_unlock(&(proc->lock))
#define _LOCK_PROCESS_INFO(p_info) spin_lock(&(p_info->lock))
#define _UNLOCK_PROCESS_INFO(p_info) spin_unlock(&(p_info->lock))

/* Globals */
process_info global_process_info;

/* Internal Functions */
process_info* get_process_info() {
  return &global_process_info;
}

void add_to_process_list(nk_process_t *p) {
  struct list_head p_list = get_process_info()->process_list;
  list_add_tail(&(p->process_node), &p_list);
}

int get_new_pid(process_info *p_info) {
  int pid_map_ind;
  do {
    pid_map_ind = p_info->next_pid % MAX_PID;
    (p_info->next_pid)++;
  } while ((p_info->used_pids)[pid_map_ind].val > 0);
  (p_info->used_pids)[pid_map_ind].val = 1;
  return pid_map_ind;
}

void free_pid(process_info *p_info, uint64_t old_pid) {
  (p_info->used_pids)[old_pid].val = 0;
}

void count_and_len(char **arr, uint64_t *len, uint64_t *count) {
  *len = 0;
  *count = 0;
  if (arr) {
    for (*count = *len = 0; *arr[*count]; (*count)++) {
      *len += strlen(arr[*count]) + 1;
      (*count)++;
    }
    (*len)++;
  }
}

char **copy_argv_or_envp(char *arr[], uint64_t count, uint64_t len) {
  if (arr) {
    char **ptr_arr = (char **)malloc(sizeof(char *) * count + 1);
    char *heap_arr = (char *)malloc(sizeof(char) * len);
    uint64_t i, heap_idx, new_str_len;
    new_str_len = 0;
    for (i = heap_idx = 0; i < count; i++) {
      PROCESS_INFO("copying %s to the heap\n", arr[i]);
      new_str_len = strlen(arr[i]) + 1;
      //char *new_str = (char *)malloc(sizeof(char) * new_str_len);
      ptr_arr[i] = &(heap_arr[heap_idx]);
      strcpy(&(heap_arr[heap_idx]), arr[i]);
      heap_arr[new_str_len] = 0;
      heap_idx += new_str_len + 1;
    }
    ptr_arr[i] = 0;
    return ptr_arr;
  }
  return NULL;
}


void __nk_process_wrapper(void *i, void **o) {
  nk_process_t *p = (nk_process_t*)i;
  char *args = NULL;
  if (p->argv) {
    args = *(p->argv);
  }
  struct nk_exec *exe = p->exe;
  // might require more setup
  nk_thread_group_join(p->t_group);
  nk_aspace_move_thread(p->aspace);
  nk_start_exec(exe, (void *)args, 0); 
}

int create_process_aspace(nk_process_t *p, char *aspace_type, char *exe_name, nk_aspace_t **new_aspace) {
  nk_aspace_characteristics_t c;
  if (nk_aspace_query(aspace_type, &c)) {
    PROCESS_ERROR("failed to find %s aspace implementation\n", aspace_type);
    return -1;
  } 

  nk_aspace_t *addr_space = nk_aspace_create(aspace_type, exe_name, &c);
  if (!addr_space) {
    PROCESS_ERROR("failed to create address space\n");
    return -1;
  }  

  // allocate heap
  void *p_addr_start = malloc(PHEAP_1MB);
  if (!p_addr_start) {
    nk_aspace_destroy(addr_space);
    PROCESS_ERROR("failed to allocate aspace heap\n");
    return -1;
  }
  memset(p_addr_start, 0, PHEAP_1MB);
  
  // add heap to addr space
  nk_aspace_region_t r_heap;
  r_heap.va_start = 0;
  r_heap.pa_start = p_addr_start;
  r_heap.len_bytes = (uint64_t)PHEAP_1GB; 
  r_heap.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE | NK_ASPACE_EAGER;

  if (nk_aspace_add_region(addr_space, &r_heap)) {
    PROCESS_ERROR("failed to add initial process aspace heap region\n");
    nk_aspace_destroy(addr_space);
    free(p_addr_start);
    return -1;
  }

  // map executable in address space
  p->exe = nk_load_exec(exe_name);
  nk_aspace_region_t r_exe;
  //r_exe.va_start = (void*)(PHEAP_1GB + PHEAP_4KB);
  r_exe.va_start = p->exe->blob;
  r_exe.pa_start = p->exe->blob;
  r_exe.len_bytes = p->exe->blob_size;
  r_exe.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE | NK_ASPACE_EXEC | NK_ASPACE_EAGER;

  if (nk_aspace_add_region(addr_space, &r_exe)) {
    PROCESS_ERROR("failed to add initial process aspace exe region\n");
    nk_unload_exec(p->exe);
    free(p);
    nk_aspace_destroy(addr_space);
    free(p_addr_start);
    return -1;
  }
  if (new_aspace) {
    *new_aspace = addr_space;
  } 
  return 0;

}

int add_args_to_aspace(nk_aspace_t *addr_space, char **argv, uint64_t argc, uint64_t argv_len, char **envp, uint64_t envc, uint64_t envp_len) {
  
  // initialize region struct
  nk_aspace_region_t r_args;
  r_args.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE | NK_ASPACE_EAGER;
  
  // add memory that holds ptrs to argv strings
  if (argv && argc) {
    r_args.va_start = (void *)argv;
    r_args.pa_start = (void *)argv;
    r_args.len_bytes = sizeof(char *) * (argc + 1); 

    if (nk_aspace_add_region(addr_space, &r_args)) {
      PROCESS_ERROR("failed to add argv aspace region\n");
      nk_aspace_destroy(addr_space);
      return -1;
    }
    
    // add argv string memory
    r_args.va_start = (void *)(*argv);
    r_args.pa_start = (void *)(*argv);
    r_args.len_bytes = sizeof(char) * (argv_len); 

    if (nk_aspace_add_region(addr_space, &r_args)) {
      PROCESS_ERROR("failed to add argv aspace region\n");
      nk_aspace_destroy(addr_space);
      return -1;
    }
  }

  if (envp && envp) {
    r_args.va_start = (void *)envp;
    r_args.pa_start = (void *)envp;
    r_args.len_bytes = sizeof(char *) * (envc + 1); 

    if (nk_aspace_add_region(addr_space, &r_args)) {
      PROCESS_ERROR("failed to add argv aspace region\n");
      nk_aspace_destroy(addr_space);
      return -1;
    }
    
    // add argv string memory
    r_args.va_start = (void *)(*envp);
    r_args.pa_start = (void *)(*envp);
    r_args.len_bytes = sizeof(char) * (envp_len); 

    if (nk_aspace_add_region(addr_space, &r_args)) {
      PROCESS_ERROR("failed to add argv aspace region\n");
      nk_aspace_destroy(addr_space);
      return -1;
    }
  }
  return 0;


}

/* External Functions */
// in the future, we'll create an allocator for the process as well
// path or name, argc, argv, envp, addr space type, 
int nk_process_create(char *exe_name, char *argv[], char *envp[], char *aspace_type, nk_process_t **proc_struct) {
  // Fetch current process info
  process_info *p_info = get_process_info();
  if (p_info->process_count >= MAX_PROCESS_COUNT) {
    PROCESS_ERROR("Max number of processes (%ul) reached. Cannot create process.\n", p_info->process_count);
    return -1;
  }
  
  // alloc new process struct
  nk_process_t *p = NULL;
  p = (nk_process_t*)malloc(sizeof(nk_process_t));
  if (!p) {
    PROCESS_ERROR("Failed to allocate process struct.\n");
    return -1;
  }
  memset(p, 0, sizeof(nk_process_t));
 
  // set parent process if current thread is part of a process
  // use parent process aspace if it exists
  nk_aspace_t *addr_space;
  p->parent = NULL;
  nk_thread_t *curr_thread = get_cur_thread();
  if (curr_thread->process) {
    p->parent = curr_thread->process;
    addr_space = p->parent->aspace;
  } else { // no parent? create new aspace
    if (create_process_aspace(p, aspace_type, exe_name, &addr_space) || !addr_space) {
      PROCESS_ERROR("failed to create process address space\n");
      free(p);
      return -1;
    }
  }
  PROCESS_DEBUG("Created address space\n"); 

  // count argv and envp, allocate them on heap
  uint64_t argc, argv_len, envc, envp_len;
  argc = argv_len = envc = envp_len = 0;
  count_and_len(argv, &argc, &argv_len);
  count_and_len(envp, &envc, &envp_len);
  char **args, **envs;
  args = envs = NULL;
  args = copy_argv_or_envp(argv, argc, argv_len);
  envs = copy_argv_or_envp(envp, envc, envp_len);  
  if (add_args_to_aspace(addr_space, args, argc, argv_len, envs, envc, envp_len)) {
    PROCESS_ERROR("failed to add args to address space\n");
    return -1;
  }
  PROCESS_DEBUG("Added args to address space\n"); 

  // ensure that lock has been initialized to 0
  // CALL spinlock init instead
  spinlock_init(&(p->lock));
  
  // acquire locks and get new pid
  _LOCK_PROCESS(p);
  _LOCK_PROCESS_INFO(p_info);
  p->pid = get_new_pid(p_info);
  add_to_process_list(p);

  // release process_info lock, no global state left to modify
  _UNLOCK_PROCESS_INFO(p_info);

  // name process
  snprintf(p->name, MAX_PROCESS_NAME, "p-%ul-%s", p->pid, exe_name);
  p->name[MAX_PROCESS_NAME-1] = 0;

  // set address space ptr and rename it
  p->aspace = addr_space;
  nk_aspace_rename(p->aspace, p->name); 

  // for now, set arg vars to NULL. Eventually we want to put them into addr space
  p->argc = argc;
  p->argv = argv;
  p->envc = envc;
  p->envp = envp;

  // create thread group (empty for now)
  nk_thread_group_create(p->name);

  // release process lock
  _UNLOCK_PROCESS(p);

  // set output ptr (if not null)
  if (proc_struct) {
    *proc_struct = p;
  }

  return 0;  
}

int nk_process_name(nk_process_id_t proc, char *name)
{
  nk_process_t *p = (nk_process_t*)proc;
  strncpy(p->name,name,MAX_PROCESS_NAME);
  p->name[MAX_PROCESS_NAME-1] = 0;
  return 0;
}

int nk_process_run(nk_process_t *p, int target_cpu) {
  nk_thread_id_t tid;
  return nk_thread_start(__nk_process_wrapper, (void*)p, 0, 0, 0, &tid, target_cpu);
}

int nk_process_start(char *exe_name, char *argv[], char *envp[], char *aspace_type, nk_process_t **p, int target_cpu) {
  if (nk_process_create(exe_name, argv, envp, aspace_type, p)) {
    PROCESS_ERROR("failed to create process\n");
    return -1;
  }
  if (nk_process_run(*p, target_cpu)) {
    PROCESS_ERROR("failed to run new process\n");
    //nk_process_destroy(*p);
    return -1;
  }
  return 0;
}


// add this right after loader init
int nk_process_init() {
  memset(&global_process_info, 0, sizeof(process_info));
  INIT_LIST_HEAD(&(global_process_info.process_list));
  global_process_info.lock = 0;
  global_process_info.process_count = 0;
  global_process_info.next_pid = 0;
  return 0; 
}
