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

#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_assert.h>
#include <nautilus/percpu.h>
#include <nautilus/list.h>
#include <nautilus/atomic.h>
#include <nautilus/timehook.h>
#include <nautilus/spinlock.h>

/*
  This is the run-time support code for compiler-based timing transforms
  and is meaningless without that feature enabled.
*/


/* Note that since code here can be called in interrupt context, it
   is potentially dangerous to turn on debugging or other output */

#ifndef NAUT_CONFIG_DEBUG_COMPILER_TIMING
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("timehook: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("timehook: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("timehook: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("timehook: " fmt, ##args)


#define LOCAL_LOCK_DECL // nothing for now
#define LOCAL_LOCK(s) spin_lock(&s->lock)
#define LOCAL_TRYLOCK(s) spin_try_lock(&s->lock)
#define LOCAL_UNLOCK(s) spin_unlock(&s->lock)

// maximum number of hooks per CPU
#define MAX_HOOKS 16
#define GET_HOOK_DATA 1
#define MAX_HOOK_DATA_COUNT 1000
uint64_t hook_data[MAX_HOOK_DATA_COUNT], hook_fire_data[MAX_HOOK_DATA_COUNT];
int hook_time_index = 0;
    
void get_time_hook_data() {
  // compute and print print hook_start --- hook_end average
  // skip first 5 data entries
  int i, sum = 0;
  nk_vc_printf("hook_time_index %d\n", hook_time_index);
  for (i = 5; i < hook_time_index; i++) {
    nk_vc_printf("%d: %lu\n", i, hook_data[i]);
    sum += hook_data[i]; 
  }

  double hook_data_average = (double)sum / hook_time_index;
  nk_vc_printf("hook_data average, %lf\n", hook_data_average);
  
  // compute and print hook_fire_start --- hook_fire_end average
  // skip first 5 data entries
  sum = 0;
  for (i = 5; i < hook_time_index; i++) {
    nk_vc_printf("f %d: %lu\n", i, hook_fire_data[i]);
    sum += hook_fire_data[i]; 
  }

  double hook_fire_data_average = (double)sum / hook_time_index;
  nk_vc_printf("hook_fire_data average, %lf\n", hook_fire_data_average);
 
  // reset variables
  memset(hook_data, 0, sizeof(hook_data));
  memset(hook_fire_data, 0, sizeof(hook_fire_data));
  hook_time_index = 0;
  return;
}

// per-cpu timehook
struct _time_hook {
    enum {UNUSED = 0,
	  ALLOCED,
	  DISABLED,
	  ENABLED}  state;
    int (*hook_func)(void *hook_state);     // details of callback
    void *hook_state;                  //   ...
    uint64_t period_cycles;       // our period in cycles
    uint64_t last_start_cycles;   // when the last top-level invocation happened that invoked us
};


// time-hook as returned to user (this is hideous)
struct nk_time_hook {
    int                count;
    struct _time_hook *per_cpu_hooks[0];
};


// per-cpu state
struct nk_time_hook_state {
    spinlock_t      lock;
    enum { INACTIVE=0,                     // before initialization
	   READY_STATE=1,                        // active, not currently in a callback
	   INPROGRESS=2} state;            // active, currently in a callback
    uint64_t        last_start_cycles;     // when we last were invoked by the compiler
    int             count;                 // how hooks we have
    struct _time_hook hooks[MAX_HOOKS]; 
};


// assumes lock held
static struct _time_hook *alloc_hook(struct nk_time_hook_state *s)
{
    int i;
    for (i=0;i<MAX_HOOKS;i++) {
	if (s->hooks[i].state==UNUSED) {
	    s->hooks[i].state=ALLOCED;
	    return &s->hooks[i];
	}
    }
    return 0;
}

// assumes lock held
static void free_hook(struct nk_time_hook_state *s, struct _time_hook *h)
{
    h->state=UNUSED;
}



uint64_t nk_time_hook_get_granularity_ns()
{
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;
    
    return apic_cycles_to_realtime(apic,NAUT_CONFIG_COMPILER_TIMING_PERIOD_CYCLES);
}
	


static inline struct _time_hook *_nk_time_hook_register_cpu(int (*hook)(void *state),
							    void *state,
							    uint64_t period_cycles,
							    struct nk_time_hook_state *s)
{
    LOCAL_LOCK_DECL;

    LOCAL_LOCK(s);
    struct _time_hook *h = alloc_hook(s);
    if (!h) {
	ERROR("Failed to allocate internal hook\n");
	LOCAL_UNLOCK(s);
	return 0;
    }
    h->hook_func = hook;
    h->hook_state = state;
    h->period_cycles = period_cycles;
    h->last_start_cycles = 0;
    // finally, do not enable yet - wait for wrapper
    h->state = DISABLED;
    s->count++;
    LOCAL_UNLOCK(s);
    return h;
}

static inline void _nk_time_hook_unregister_cpu(struct _time_hook *h,
						struct nk_time_hook_state *s)
{
    LOCAL_LOCK_DECL;

    LOCAL_LOCK(s);
    free_hook(s,h);
    s->count--;
    LOCAL_UNLOCK(s);
}

#define SIZE(n)      ((n)/8 + 1)
#define ZERO(x,n)    memset(x,0,SIZE(n))
#define SET(x,i)     (((x)[(i)/8]) |= (0x1<<((i)%8)))
#define CLEAR(x,i)   (((x)[(i)/8])) &= ~(0x1<<((i)%8))
#define IS_SET(x,i) (((x)[(i)/8])>>((i)%8))&0x1


static inline struct nk_time_hook *_nk_time_hook_register(int (*hook)(void *state),
							  void *state,
							  uint64_t period_cycles,
							  char *cpu_mask)
{

    
    struct sys_info *sys = per_cpu_get(system);
    int n = nk_get_num_cpus();
    int i;
    int fail=0;

    // make sure we can actually allocate what we will return to the user

#define HOOK_SIZE  sizeof(struct nk_time_hook)+sizeof(struct _time_hook *)*n
    
    struct nk_time_hook *uh = malloc(HOOK_SIZE);

    if (!uh) {
	ERROR("Can't allocate user hook\n");
	return 0;
    }

    memset(uh,0,HOOK_SIZE);

    // allocate all the per CPU hooks, prepare to roll back
    for (i=0;i<n;i++) {
	if (IS_SET(cpu_mask,i)) {
	    struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
	    if (!s) {
		ERROR("Failed to find per-cpu state\n");
		fail=1;
		break;
	    }
	    struct _time_hook *h = _nk_time_hook_register_cpu(hook,state,period_cycles,s);
	    if (!h) {
		ERROR("Failed to register per-cpu hook on cpu %d\n",i);
		fail=1;
		break;
	    }
	    uh->per_cpu_hooks[i] = h;
	    uh->count++;
	}
    }

    if (fail) {
	DEBUG("Unwinding per-cpu hooks on fail\n");
	for (i=0;i<n;i++) {
	    if (uh->per_cpu_hooks[i]) { 
		struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
		_nk_time_hook_unregister_cpu(uh->per_cpu_hooks[i],s);
		uh->count--;
	    }
	}
	
	free(uh);

	return 0;
	
    } else {

	// All allocations done.   We now collectively enable 
	
	// now we need to enable each one
	// lock relevant per-cpu hooks
	for (i=0;i<n;i++) {
	    LOCAL_LOCK_DECL;
	    if (uh->per_cpu_hooks[i]) { 
		struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
		LOCAL_LOCK(s);
	    }
	}

	// enable all the hooks
	for (i=0;i<n;i++) {
	    if (uh->per_cpu_hooks[i]) {
		uh->per_cpu_hooks[i]->state = ENABLED;
	    }
	}
    

	// now release all locks
	for (i=0;i<n;i++) {
	    LOCAL_LOCK_DECL;
	    if (uh->per_cpu_hooks[i]) { 
		struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
		LOCAL_UNLOCK(s);
	    }
	}

	// and we are done
	return uh;
    }
}

struct nk_time_hook *nk_time_hook_register(int (*hook)(void *state),
					   void *state,
					   uint64_t period_ns,
					   int   cpu,
					   char *cpu_mask)
{
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;
    int i;
    int n = nk_get_num_cpus();
    
    char local_mask[SIZE(n)];
    char *mask_to_use = local_mask;

    ZERO(local_mask,n);

    uint64_t period_cycles = apic_realtime_to_cycles(apic,period_ns);

    DEBUG("nk_time_hook_register(%p,%p,period_ns=%lu (cycles=%lu), cpu=%d, cpu_mask=%p\n", hook,state,period_ns,period_cycles,cpu,cpu_mask);

    switch (cpu) {
    case NK_TIME_HOOK_THIS_CPU:
	SET(local_mask,my_cpu_id());
	break;
    case NK_TIME_HOOK_ALL_CPUS:
	for (i=0;i<n;i++) { SET(local_mask,i); }
	break;
    case NK_TIME_HOOK_ALL_CPUS_EXCEPT_BSP:
	for (i=1;i<n;i++) { SET(local_mask,i); }
	break;
    case NK_TIME_HOOK_CPU_MASK:
	mask_to_use = cpu_mask;
	break;
    default:
	if (cpu<n) {
	    SET(local_mask,cpu);
	} else {
	    ERROR("Unknown cpu masking (cpu=%d)\n",cpu);
	}
	break;
    }

    return _nk_time_hook_register(hook,state,period_cycles,mask_to_use);
}


int nk_time_hook_unregister(struct nk_time_hook *uh)
{
    struct sys_info *sys = per_cpu_get(system);
    int n = nk_get_num_cpus();
    int i;

    for (i=0;i<n;i++) {
	if (uh->per_cpu_hooks[i]) { 
	    struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
	    _nk_time_hook_unregister_cpu(uh->per_cpu_hooks[i],s);
	    uh->count--;
	}
    }
	
    free(uh);

    return 0;
    
}


// this is the part that needs to be fast and low-overhead
// it should not block, nor should anything it calls...
// nor can they have nk_time_hook_fire() calls...
// this is where to focus performance improvement
void nk_time_hook_fire()
{
#ifdef GET_HOOK_DATA
   uint64_t rdtsc_hook_start = 0, rdtsc_hook_end = 0, rdtsc_hook_fire_start = 0, rdtsc_hook_fire_end = 0;
   int local_hook_time_index = hook_time_index; 
   if (hook_time_index < MAX_HOOK_DATA_COUNT) {
     hook_time_index++;
   }
   if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
     rdtsc_hook_start = rdtsc();
   }
#endif
    struct sys_info *sys = per_cpu_get(system);
    struct nk_time_hook_state *s = sys->cpus[my_cpu_id()]->timehook_state;

    LOCAL_LOCK_DECL;

    if (LOCAL_TRYLOCK(s)) {
	// failed to get lock - we will simply not execute this round
	DEBUG("failed to acquire lock on fire (cpu %d)\n",my_cpu_id());
	return;
    }

    if (s->state!=READY_STATE) {
	DEBUG("short circuiting fire because we are in state %d\n",s->state);
	LOCAL_UNLOCK(s);
    }

    s->state = INPROGRESS;
    
    int i;
    int seen;

    uint64_t cur_cycles = rdtsc();

    int count = 0;
    struct _time_hook *queue[MAX_HOOKS];
    
    for (i=0, seen=0;i<MAX_HOOKS && seen<s->count;i++) {
	struct _time_hook *h = &s->hooks[i];
	if (h->state==ENABLED) {
	    seen++;
	    if (cur_cycles >= (h->last_start_cycles + h->period_cycles)) {
		
		DEBUG("queueing hook func=%p state=%p last=%lu cur=%lu\n",
		      h->hook_func, h->hook_state, h->last_start_cycles, cur_cycles);
		
		    queue[count++] = h;
	    }
	}
    }
    

    // we now need to prepare for the next batch.
    // note that a hook could context switch away from us, so we need to do
    // handle cleanup *before* we execute any hooks
    
    s->state = READY_STATE;
    LOCAL_UNLOCK(s);

    // now we actually fire the hooks.   Note that the execution of one batch of hooks
    // can race with queueing/execution of the next batch.  that's the hook
    // implementor's problem
#ifdef GET_HOOK_DATA
    rdtsc_hook_end = rdtsc();
    if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
      hook_data[local_hook_time_index] = rdtsc_hook_end - rdtsc_hook_start;
    }
    rdtsc_hook_fire_start = rdtsc();
#endif
    for (i=0; i<count; i++) {
	struct _time_hook *h = queue[i];
	DEBUG("launching hook func=%p state=%p last=%lu cur=%lu\n",
	      h->hook_func, h->hook_state, h->last_start_cycles, cur_cycles);
	
	h->hook_func(h->hook_state);
	h->last_start_cycles = cur_cycles;
    }
#ifdef GET_HOOK_DATA
    rdtsc_hook_fire_end = rdtsc();
    if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
      hook_fire_data[local_hook_time_index] = rdtsc_hook_fire_end - rdtsc_hook_fire_start;
    }
#endif

}


static int shared_init()
{
    struct sys_info *sys = per_cpu_get(system);
    struct cpu *cpu = sys->cpus[my_cpu_id()];
    struct nk_time_hook_state *s;

    s = malloc_specific(sizeof(struct nk_time_hook_state),my_cpu_id());

    if (!s) {
	ERROR("Failed to allocate per-cpu state\n");
	return -1;
    }
    
    memset(s,0,sizeof(struct nk_time_hook_state));
	   
    spinlock_init(&s->lock);

    cpu->timehook_state = s;

    INFO("inited\n");

    return 0;
    
}
    
int nk_time_hook_init()
{
    // nothing currently special about the BSP at this point
    return shared_init();
}

int nk_time_hook_init_ap()
{
    return shared_init();
}


