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
#include <nautilus/cpu_state.h>
#include <nautilus/naut_assert.h>
#include <nautilus/percpu.h>
#include <nautilus/list.h>
#include <nautilus/atomic.h>
#include <nautilus/timehook.h>
#include <nautilus/spinlock.h>
#include <nautilus/shell.h>
#include <nautilus/backtrace.h>

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

#define DB(x) outb(x, 0xe9)
#define DHN(x) outb(((x & 0xF) >= 10) ? (((x & 0xF) - 10) + 'a') : ((x & 0xF) + '0'), 0xe9)
#define DHB(x) DHN(x >> 4) ; DHN(x);
#define DHW(x) DHB(x >> 8) ; DHB(x);
#define DHL(x) DHW(x >> 16) ; DHW(x);
#define DHQ(x) DHL(x >> 32) ; DHL(x);
#define DS(x) { char *__curr = x; while(*__curr) { DB(*__curr); *__curr++; } }

#define MAX(x, y)((x > y) ? (x) : (y))
#define MIN(x, y)((x < y) ? (x) : (y))

#define GET_STACK_CALLER_TRACE 0

// maximum number of hooks per CPU
#define MAX_HOOKS 16
#define GET_HOOK_DATA 0
#define MAX_HOOK_DATA_COUNT 1000
uint64_t hook_data[MAX_HOOK_DATA_COUNT], hook_fire_data[MAX_HOOK_DATA_COUNT];
int hook_time_index = 0;
   
void get_time_hook_data() {
  // compute and print print hook_start --- hook_end average
  // skip first 5 data entries
  int i, sum = 0;
  nk_vc_printf("hook_time_index %d\n", hook_time_index);
  
  nk_vc_printf("th_one_start\n");
  for (i = 5; i < hook_time_index; i++) {
    nk_vc_printf("%lu\n", hook_data[i]);
    sum += hook_data[i]; 
  }
  nk_vc_printf("th_one_end\n");

  double hook_data_average = (double)sum / hook_time_index;
  nk_vc_printf("hook_data average, %lf\n", hook_data_average);
  
  // compute and print hook_fire_start --- hook_fire_end average
  // skip first 5 data entries
  sum = 0;

  nk_vc_printf("th_two_start\n");
  for (i = 5; i < hook_time_index; i++) {
    nk_vc_printf("%lu\n", hook_fire_data[i]);
    sum += hook_fire_data[i]; 
  }
  nk_vc_printf("th_two_end\n");

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
    uint64_t early_count;
    uint64_t early_sum;
    uint64_t early_sum2;
    uint64_t early_max;
    uint64_t early_min;
    uint64_t late_count;
    uint64_t late_sum;
    uint64_t late_sum2;
    uint64_t late_max;
    uint64_t late_min;
    uint64_t fire_count;
    uint64_t enabled_count;
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
    uint64_t invocation_count;
    uint64_t try_lock_fail_count;
    uint64_t state_fail_count;
    int      count;                 // how hooks we have
    struct _time_hook hooks[MAX_HOOKS]; 
};


void nk_time_hook_dump()
{
    int i;
    struct sys_info *sys = per_cpu_get(system);

    for(i = 0; i < nk_get_num_cpus(); i++) {
      struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
      nk_vc_printf("cpu %d: %d hooks", i, s->count);
      nk_vc_printf("  %luic  %lulf  %lusf\n", s->invocation_count, s->try_lock_fail_count, s->state_fail_count);
      
      int j;
      for (j = 0; j < MAX_HOOKS; j++) {
        if (s->hooks[j].state != INACTIVE) {
	   struct _time_hook *h = &(s->hooks[j]);
	   nk_vc_printf("    *%lulc *%lufc\n", h->late_count , h->fire_count);
	   nk_vc_printf("    %dhn %lupc %luls %luec %lulc %lufc %lut %luemi %luema %lulmi %lulma", j, h->period_cycles, h->last_start_cycles, h->early_count, h->late_count, h->fire_count, (h->early_count + h->fire_count), h->early_min, h->early_max, h->late_min, h->late_max);
	  if (h->early_count > 0) {
	       nk_vc_printf("  %lume %luve", (h->early_sum / h->early_count), (((h->early_sum2) - ((h->early_sum * h->early_sum) / h->early_count)) / h->early_count)); 
	   }
	   if (h->late_count > 0) {
	       nk_vc_printf("  %luml %luvl", (h->late_sum / h->late_count), (((h->late_sum2) - ((h->late_sum * h->late_sum) / h->late_count)) / h->late_count)); 
	   }


	   nk_vc_printf("\n");
	
	}
      
      }
    
    }

}

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
	    h->early_min = -1;
	    h->late_min = -1; 
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

static int ready = 0;
int ACCESS_HOOK = 0;
// this is the part that needs to be fast and low-overhead
// it should not block, nor should anything it calls...
// nor can they have nk_time_hook_fire() calls...
// this is where to focus performance improvement
__attribute__((noinline)) void nk_time_hook_fire()
{
   if (!ready) {
     return;
   }

#if GET_STACK_CALLER_TRACE
   // Debug output --- get caller address and stack frame
   // (roughly) based on local variable
 
   int test = 0;
   DS("r ");
   DHQ(((uint64_t)__builtin_return_address(0)));
   DS(" ");
   DHQ(((uint64_t)&test));
   DS("\n");
#endif 


#if GET_HOOK_DATA
   uint64_t rdtsc_hook_start = 0, rdtsc_hook_end = 0, rdtsc_hook_fire_start = 0, rdtsc_hook_fire_end = 0;
   int local_hook_time_index = hook_time_index; 
   if (ACCESS_HOOK) {
	   if (hook_time_index < MAX_HOOK_DATA_COUNT) {
	     hook_time_index++;
	   }
	   if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
	     rdtsc_hook_start = rdtsc();
	   }
   }
#endif


    struct sys_info *sys = per_cpu_get(system);
    struct nk_time_hook_state *s = sys->cpus[my_cpu_id()]->timehook_state;

    s->invocation_count++;

    LOCAL_LOCK_DECL;
    
    if (LOCAL_TRYLOCK(s)) {
	// failed to get lock - we will simply not execute this round
	DEBUG("failed to acquire lock on fire (cpu %d)\n",my_cpu_id());

#if GET_STACK_CALLER_TRACE
	// Same logic as above
	
	DS("failed try lock ");
	DS("r ");
	DHQ(((uint64_t)__builtin_return_address(0)));
        DS(" ");
        DHQ(((uint64_t)&test));
        DS("\n");
	// while(1) {}
	
	// Set ready to 0 to print on panic, output
	// backtrace	
	
	__sync_fetch_and_and(&ready, 0); // atomically write ready
	BACKTRACE(INFO, 20);
        panic("Try lock failed\n");
#endif	

	s->try_lock_fail_count++;	
	return;
    }
    

    if (s->state!=READY_STATE) {
	DEBUG("short circuiting fire because we are in state %d\n",s->state);
	
#if GET_STACK_CALLER_TRACE	
	// Same logic as above
	
	DS("failed ready state ");
	DS("r ");
	DHQ(((uint64_t)__builtin_return_address(0)));
        DS(" ");
        DHQ(((uint64_t)&test));
        DS("\n");
	// while (1) {} 

	__sync_fetch_and_and(&ready, 0); // atomically write ready
	BACKTRACE(INFO, 20);
        panic("Ready state failed\n");
#endif

	s->state_fail_count++;	
	return;
    }
    
    s->state = INPROGRESS;
    
    int i;
    int seen = 0;

    uint64_t cur_cycles = rdtsc();

    int count = 0;
    struct _time_hook *queue[MAX_HOOKS];
   
    for (i=0, seen=0; ((i < 2) && (seen < s->count)); i++) {
    //for (i=0; i < 15; i++) {
	struct _time_hook *h = &s->hooks[i];
	if (h->state==ENABLED) {
	    seen++;
	    if (cur_cycles >= (h->last_start_cycles + h->period_cycles)) {
		
		DEBUG("queueing hook func=%p state=%p last=%lu cur=%lu\n",
		      h->hook_func, h->hook_state, h->last_start_cycles, cur_cycles);
		
		    queue[count++] = h;
		    
		    if (h->last_start_cycles) {
			    // Debug
			    uint64_t temp_late_count = h->late_count;

			    h->late_count++;
			    
			    // Debug
			    if ((h->late_count - temp_late_count) > 1) {
			       DHQ(h->late_count);
			       DS("\n");
			    }

			    h->late_sum += (cur_cycles - (h->last_start_cycles + h->period_cycles));
			    h->late_sum2 += (cur_cycles - (h->last_start_cycles + h->period_cycles)) * (cur_cycles - (h->last_start_cycles + h->period_cycles)); 
			    h->late_min = MIN((cur_cycles - (h->last_start_cycles + h->period_cycles)), h->late_min);
			    h->late_max = MAX((cur_cycles - (h->last_start_cycles + h->period_cycles)), h->late_max);
		   }

	    } else {
	      // First call cannot be early
	      h->early_count++;
	      h->early_sum += (h->last_start_cycles + h->period_cycles - cur_cycles);
	      h->early_sum2 += (h->last_start_cycles + h->period_cycles - cur_cycles) * (h->last_start_cycles + h->period_cycles - cur_cycles); 
	      h->early_min = MIN((h->last_start_cycles + h->period_cycles - cur_cycles), h->early_min);
	      h->early_max = MAX((h->last_start_cycles + h->period_cycles - cur_cycles), h->early_max);
	    }
	}
    }


    // Extra check --- for loop debugging
    if ((count != 1) && (count != 0)) {
	    DHL(count);
	    panic("Count isn't one\n");
    }
    

    // we now need to prepare for the next batch.
    // note that a hook could context switch away from us, so we need to do
    // handle cleanup *before* we execute any hooks

    // ** TODO ** --- need to limit nested "interrupts"

       
    // now we actually fire the hooks.   Note that the execution of one batch of hooks
    // can race with queueing/execution of the next batch.  that's the hook
    // implementor's problem


#if GET_HOOK_DATA
    if (ACCESS_HOOK) {
	    rdtsc_hook_end = rdtsc();
	    if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
	      hook_data[local_hook_time_index] = rdtsc_hook_end - rdtsc_hook_start;
	    }
	    rdtsc_hook_fire_start = rdtsc();
    }
#endif
    
    
    for (i=0; i<count; i++) {
	struct _time_hook *h = queue[i];
	DEBUG("launching hook func=%p state=%p last=%lu cur=%lu\n",
	      h->hook_func, h->hook_state, h->last_start_cycles, cur_cycles);
	
	h->hook_func(h->hook_state);
	h->last_start_cycles = cur_cycles;
	h->fire_count++;
    }


#if GET_HOOK_DATA
    if (ACCESS_HOOK) {
	    rdtsc_hook_fire_end = rdtsc();
	    if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
	      hook_fire_data[local_hook_time_index] = rdtsc_hook_fire_end - rdtsc_hook_fire_start;
	    }
    }
#endif
   

    // Unlocking at the end --- for loop debugging 
    s->state = READY_STATE;
    LOCAL_UNLOCK(s);

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

int nk_time_hook_start()
{
  struct sys_info *sys = per_cpu_get(system);
  struct nk_time_hook_state *s = sys->cpus[my_cpu_id()]->timehook_state;
  s->state = READY_STATE;
  
  INFO("time hook started\n");

  ready = 1;

  INFO("time hook ready set\n");

  return 0;
}

static int
handle_ths(char * buf, void * priv)
{
  nk_time_hook_dump();
  return 0;
}

static struct shell_cmd_impl ths_impl = {
  .cmd      = "ths",
  .help_str = "ths",
  .handler  = handle_ths,
};

nk_register_shell_cmd(ths_impl);

