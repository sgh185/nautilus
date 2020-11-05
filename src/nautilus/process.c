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
  list_add_tail(p, &p_list);
}

int get_new_pid(process_info *p_info) {
  do {
    pid_map_ind = p_info->next_pid % MAX_PID;
    next_pid++;
  } while (!(p_info->pid_map[pid_map_ind]));
  p_info->pid_map[pid_map_ind] = 1;
  return pid_map_ind;
}

void free_pid(process_info *p_info, uint64_t old_pid) {
  p_info->pid_map[old_pid] = 0;
}

/* External Functions */
// in the future, we'll create an allocator for the process as well
// path or name, argc, argv, envp, addr space type, 
int nk_process_create(char *exe_name, char *cmd_line, char *cmd_line_args, void **addr_space, nk_process_t **proc_struct) {
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
 

  // TODO MAC: allocate aspace of type addr_space_type

  // set parent process if current thread is part of a process
  curr_thread = get_cur_thred();

  // ensure that lock has been initialized to 0
  // CALL spinlock init instead
  p->lock = 0;
  
  // acquire the process lock
  _LOCK_PROCESS(p);
  _LOCK_PROCESS_INFO(p_info);

  // assign new pid
  p->pid = get_new_pid(p_info);
  add_to_process_list(p);

  // release process info lock
  _UNLOCK_PROCESS_INFO(p_info);

  // name process
  snprintf(p->name, MAX_PROCESS_NAME, "p-%ul-%s", p->pid, exe_name);
  p->name[MAX_PROCESS_NAME-1] = 0;

  // release process lock
  _UNLOCK_PROCESS(p);

  return 0;  
}

int nk_process_name(nk_process_id_t proc, char *name)
{
  nk_process_t *p = (nk_process_t*)proc;
  strncpy(p->name,name,MAX_PROCESS_NAME);
  p->name[MAX_PROCESS_NAME-1] = 0;
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
