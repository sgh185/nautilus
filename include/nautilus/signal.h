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
 * Copyright (c) 2021, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2021, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2021, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/spinlock.h>
#include <nautilus/intrinsics.h>
#include <nautilus/nautilus.h>
#include <nautilus/signal_consts.h>
#include <nautilus/list.h>

/* Function signature of a signal handler */
typedef void (*nk_signal_handler_t)(int signal_num);
#define DEFAULT_SIG ((nk_signal_handler_t)0)
#define IGNORE_SIG ((nk_signalHandler_t)1)


/* Struct for siginfo -- stores info about a specific occurrence of a signal */
typedef struct signal_info_struct {
  uint64_t signal_num;
  uint64_t signal_err_num;
  uint64_t signal_code;
  union {
    /* kill() syscall */
    struct {
      uint64_t sender_pid;
    } _kill_info;

    /* Posix.1b timers and signals not implemented */
    /* SIGCHLD */
    struct {
      uint64_t child_pid;
      uint64_t exit_status;
      uint64_t utime;
      uint64_t stime;
    } _sigchld_info;
    
    /* SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP, SIGEMT */
    struct {
      void *faulting_addr; /* User ptr, address which caused fault */
      short addr_lsb; /* LSB of reported addr from SIGBUS */
      /* Don't handle SEGV_BNDERR or SEGV_PKUERR */
    } _sigfault_info;

    /* SIGPOLL */
    struct {
      uint64_t band; /* poll in, out, or msg */
      int file_descriptor;
    } _sigpoll_info;
    
    /* SIGSYS */
    struct {
      void *caller_addr; /* addr of caller */
      uint64_t syscall_num; /* syscall triggered by caller */
    } _sigsys_info;
 } signal_info;
} nk_signal_info_t;   

/* Signal Bit Set (For now, we assume SIGSET_SIZE == 1) */
typedef struct signal_set{
  uint64_t sig[SIGSET_SIZE];
} nk_signal_set_t;

/* LL Signal Queue (for RT signals, may not be needed?) */
struct signal_queue {
  struct list_head lst;
  nk_signal_set_t signal;
} nk_signal_queue_t;

/* LL of pending signals */
struct signal_pending {
  struct list_head lst;
  nk_signal_set_t signal;
} nk_signal_pending_t;


/* Signal action tells us what handler to use, if signal is ignored, or if signal is blocked */
typedef struct signal_action {
  nk_signal_handler_t handler; /* Ptr to signal handler, IGNORE_SIG(=1), or DEFAULT_SIG(=0) */ 
  nk_signal_set_t mask; /* what should be masked when running signal handler */
  uint64_t signal_flags; /* How to handle signal */
} nk_signal_action_t;

/* Table of signal action structs. May be shared among processes. */
typedef struct signal_handler_table {
  uint64_t count; /* How many processes are using this signal handler table? */
  nk_signal_action_t *handlers[SIGSET_SIZE]; /* pointers to sig action structs */
  spinlock_t lock; /* Multiple processes can use this table */ 
} nk_signal_handler_table_t;

/* Information for an occurrence of a signal */
typedef struct process_signal {
  nk_signal_action_t signal_action; /* What action the signal will take */
  nk_signal_info_t signal_info; /* Info about signal TODO MAC: Should we support this?*/
  uint64_t signal_num; /* Number of signal (1-64) */
} nk_signal_t;


/* Interface functions */

static inline void sigaddset(nk_signal_set_t *set, uint64_t signal) {
  /* Assumes signal <= BYTES_PER_WORD */
  signal--;
  set->sig[0] |= 1UL << signal;
}

static inline void sigdelset(nk_signal_set_t *set, uint64_t signal) {
  /* Assumes signal <= BYTES_PER_WORD */
  signal--;
  set->sig[0] &= ~(1UL << signal);
}

static inline int sigismember(nk_signal_set_t *set, uint64_t signal) {
  signal--;
  return (set->sig[0] >> signal) & 1;
}

static inline int sigisemptyset(nk_signal_set_t *set) {
  return !(set->sig[0]);
}

static inline int sigequalsets(const nk_signal_set_t *set1, const nk_signal_set_t *set2) {
  return set1->sig[0] == set2->sig[0];
}

#define sigmask(signal) (1UL << ((signal) - 1))

static inline void sigorsets(nk_signal_set_t *result, nk_signal_set_t *set1, nk_signal_set_t *set2) {
  result->sig[0] = ((set1->sig[0]) | (set2->sig[0]));
}

static inline void sigandsets(nk_signal_set_t *result, nk_signal_set_t *set1, nk_signal_set_t *set2) {
  result->sig[0] = ((set1->sig[0]) & (set2->sig[0]));
}

static inline void sigandnsets(nk_signal_set_t *result, nk_signal_set_t *set1, nk_signal_set_t *set2) {
  result->sig[0] = ((set1->sig[0]) & ~(set2->sig[0]));
}

static inline void signotset(nk_signal_set_t *set) {
  set->sig[0] = ~(set->sig[0]);
}

static inline void sigemptyset(nk_signal_set_t *set) {
  set->sig[0] = 0;
}

static inline void sigfillset(nk_signal_set_t *set) {
  set->sig[0] = -1;
}

/* TODO MAC: Should we restrict mask to only first 32 bits? */
static inline void sigaddsetmask(nk_signal_set_t *set, uint64_t mask)
{
	set->sig[0] |= mask;
}

static inline void sigdelsetmask(nk_signal_set_t *set, uint64_t mask)
{
	set->sig[0] &= ~mask;
}

static inline int sigtestsetmask(nk_signal_set_t *set, uint64_t mask)
{
	return (set->sig[0] & mask) != 0;
}

static inline void siginitset(nk_signal_set_t *set, uint64_t mask)
{
	set->sig[0] = mask;
}

static inline void siginitsetinv(nk_signal_set_t *set, uint64_t mask)
{
	set->sig[0] = ~mask;
}

#ifdef __cplusplus
}
#endif

// endif for top ifndef __SIGNAL_H__
#endif
