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
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, Souradip Ghosh
 * Copyright (c) 2019, The Interweaving Project <https://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Souradip Ghosh <souradipghosh2021@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __TIME_HOOK__
#define __TIME_HOOK__


// Target CPU for testing --- 'timehook1' test
#define TARGET_CPU 10

/*
  This mechanism allows you to register to receive callbacks with some
  rate.  These callbacks are *not* driven by timer interrupts, but
  rather by compiler-injected code throughout the kernel.

  The kernel must be built using the compiler-based timing transforms
  in order for this functionality to be visible.  
*/

// granularity, in nanoseconds, supported by the kernel as
// currently compiled.   Any periodicity you require must be
// an integer multiple of this.
uint64_t nk_time_hook_get_granularity_ns();

// register callback along with state it will be given
// period_ns must be a multiple of the granularity
// cpu >=   0 => specific CPU
//     ==   NK_TIME_HOOK_THIS_CPU => this one only
//     ==   NK_TIME_HOOK_ALL_CPUS => all cpus
//     ==   NK_TIME_HOOK_ALL_CPUS_EXCEPT_BSP => all cpus but cpu 0
//     ==   NK_TIME_HOOK_CPU_MASK => cpu_mask is a bitmask for relevant CPUs
struct nk_time_hook *nk_time_hook_register(int (*hook)(void *state),
					   void *state,
					   uint64_t period_ns,
					   int   cpu,
					   char *cpu_mask);

#define NK_TIME_HOOK_THIS_CPU -1
#define NK_TIME_HOOK_ALL_CPUS -2
#define NK_TIME_HOOK_ALL_CPUS_EXCEPT_BSP -3
#define NK_TIME_HOOK_CPU_MASK -4

// unregister an existing callback
int nk_time_hook_unregister(struct nk_time_hook *hook);

// called by the injected code
// user should not call this
void nk_time_hook_fire();

// called by BSP before interrupts are enabled
int nk_time_hook_init();

// called by every AP before interrupts are enabled
int nk_time_hook_init_ap();

// called by bsp after kernel is ready on all CPUs
int nk_time_hook_start();

// gather data on nk_time_hook_fire
void get_time_hook_data(void);

void nk_time_hook_dump(void);


#endif
