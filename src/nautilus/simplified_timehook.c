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
 *          Souradip Ghosh <sgh@u.northwestern.edu>
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

// maximum number of hooks per CPU
#define MAX_HOOKS  16
#define CACHE_LINE 64

//
// Locking is done using a per-cpu lock, but the user must be explicit
// 
#define LOCAL_LOCK_DECL uint8_t __local_lock_flags
#define LOCAL_LOCK(cpu) __local_lock_flags = spin_lock_irq_save(&cms[cpu].lock)
#define LOCAL_TRYLOCK(cpu) spin_try_lock_irq_save(&cms[cpu].lock, &__local_lock_flags)
#define LOCAL_UNLOCK(cpu) spin_unlock_irq_restore(&cms[cpu].lock, __local_lock_flags)
#define LOCAL_LOCK_NO_IRQ(cpu) spin_lock(&cms[cpu].lock)
#define LOCAL_TRYLOCK_NO_IRQ(cpu) spin_try_lock(&cms[cpu].lock)
#define LOCAL_UNLOCK_NO_IRQ(cpu) spin_unlock(&cms[cpu].lock)


//
// Low-level debugging output to QEMU debug port
//
#define DB(x) outb(x, 0xe9)
#define DHN(x) outb(((x & 0xF) >= 10) ? (((x & 0xF) - 10) + 'a') : ((x & 0xF) + '0'), 0xe9)
#define DHB(x) DHN(x >> 4) ; DHN(x);
#define DHW(x) DHB(x >> 8) ; DHB(x);
#define DHL(x) DHW(x >> 16) ; DHW(x);
#define DHQ(x) DHL(x >> 32) ; DHL(x);
#define DS(x) { char *__curr = x; while(*__curr) { DB(*__curr); *__curr++; } }

#define MAX(x, y)((x > y) ? (x) : (y))
#define MIN(x, y)((x < y) ? (x) : (y))

// Instrument timehook fire if this is enabled
#define GET_WHOLE_HOOK_DATA 0
#define GET_HOOK_DATA 1
#define GET_FIBER_DATA 0
#define MAX_HOOK_DATA_COUNT 1000

uint64_t hook_data[MAX_HOOK_DATA_COUNT], hook_fire_data[MAX_HOOK_DATA_COUNT];
int hook_time_index = 0;
//
// Output of timehook fire stats and raw data
//
void get_time_hook_data()
{
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
    
    extern uint64_t early_count;
    extern uint64_t late_count;

    nk_vc_printf("early count: %lu\n", early_count);
    nk_vc_printf("late count: %lu\n", late_count);

    // reset variables
    memset(hook_data, 0, sizeof(hook_data));
    memset(hook_fire_data, 0, sizeof(hook_fire_data));
    hook_time_index = 0;
    return;
}

// per-cpu timehook info
// if no instrumentation code is included, this should
// be a single cache line
// 
struct _time_hook {
    enum {UNUSED = 0,
	  ALLOCED,
	  DISABLED,
	  ENABLED}  state;
    int (*hook_func)(void *hook_state);     // details of callback
    void *hook_state;                  //   ...
    uint64_t period_cycles;       // our period in cycles
    uint64_t last_start_cycles;   // when the last top-level invocation happened that invoked us
    // the following is instrumentation code
    /*uint64_t early_count;
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
      uint64_t enabled_count;*/
} __attribute__((aligned(CACHE_LINE)));


// time-hook as returned to user
// this is not a performance critical structure
struct nk_time_hook {
    int                count;
    struct _time_hook *per_cpu_hooks[0];
};


// Performance critical per-cpu state
// this is one cache line given no instrumentation
// the intent here is to be sure we have no false sharing
// between CPUs and to reduce chances for conflict missing
// if there are a great number of CPUs.
struct cache_managed_timehook_state {
    spinlock_t      lock;
    enum { INACTIVE=0,                     // before initialization
	   READY_STATE=1,                  // active, not currently in a callback
	   INPROGRESS=2} state;            // active, currently in a callback
    uint64_t        last_start_cycles;     // when we last were invoked by the compiler
    int      count;                        // how many hooks we have
    // instrumentation
    /*uint64_t invocation_count;
    uint64_t try_lock_fail_count;
    uint64_t state_fail_count;*/
} __attribute((aligned(CACHE_LINE))) ;

static struct cache_managed_timehook_state cms[NAUT_CONFIG_MAX_CPUS];

#define CACHE_MANAGED_STATE(cpu) cms[cpu]

// additional per-cpu state - because this has a potentially
// large cache footprint, it is kept separate from the above, and
// stashed in struct cpu
struct nk_time_hook_state {
    struct _time_hook hooks[MAX_HOOKS]; 
};

// for a single time hook per cpu, the cache footprint
// should be one line of cache managed state and one line
// of _timehook.  


// time hook listing, temporarily turned off
__attribute__((optnone)) void nk_time_hook_dump()
{
    return;
    /*
    int i;
    struct sys_info *sys = per_cpu_get(system);

    // for(i = 0; i < MAX_HOOKS; i++) {
    for(i = 0; i < nk_get_num_cpus(); i++) {
      struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
      // nk_vc_printf("cpu %d: %d hooks", i, s->count);
      // nk_vc_printf("  %luic  %lulf  %lusf\n", s->invocation_count, s->try_lock_fail_count, s->state_fail_count);
      
      int j;
      for (j = 0; j < MAX_HOOKS; j++) {
        if (s->hooks[j].state != INACTIVE) {
	   struct _time_hook *h = &(s->hooks[j]);

	   
	   // DEBUG OUTPUT
	   DHQ(h->late_count);
	   DS("  ");
	   DHQ(h->fire_count);
	   DS("\n");
	   
	   
	   	
	   nk_vc_printf("    *%lulc *%lufc\n", h->late_count , h->fire_count);
	   nk_vc_printf("    %dhn %lupc %luls %luec %lulc %lufc %lut %luemi %luema %lulmi %lulma", j, h->period_cycles, h->last_start_cycles, h->early_count, h->late_count, h->fire_count, (h->early_count + h->fire_count), h->early_min, h->early_max, h->late_min, h->late_max);
	  if (h->early_count > 0) {
	       nk_vc_printf("  %lume %luve", (h->early_sum / h->early_count), (((h->early_sum2) - ((h->early_sum * h->early_sum) / h->early_count)) / h->early_count)); 
	   }
	   if (h->late_count > 0) {
	       nk_vc_printf("  %luml %luvl", (h->late_sum / h->late_count), (((h->late_sum2) - ((h->late_sum * h->late_sum) / h->late_count)) / h->late_count)); 
	   }
	
	   // Are we actually late???
	   if (h->late_count > h->fire_count) {
	     nk_vc_printf("\nlate\n");
	   }
	   
	   nk_vc_printf("\n");
		
	}
      
      }
    
    }
*/
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
							    struct nk_time_hook_state *s,
                                                            int cpu)
{
    LOCAL_LOCK_DECL;
    
    LOCAL_LOCK(cpu);
    struct _time_hook *h = alloc_hook(s);
    
	if (!h) {
		ERROR("Failed to allocate internal hook\n");
		LOCAL_UNLOCK(cpu);
		return 0;
    }

    h->hook_func = hook;
    h->hook_state = state;
    h->period_cycles = period_cycles;
    h->last_start_cycles = 0;
    // finally, do not enable yet - wait for wrapper
    h->state = DISABLED;
    CACHE_MANAGED_STATE(cpu).count++;
    LOCAL_UNLOCK(cpu);
    return h;
}

static inline void _nk_time_hook_unregister_cpu(struct _time_hook *h,
						struct nk_time_hook_state *s,
                                                int cpu)
{
    LOCAL_LOCK_DECL;
    
    LOCAL_LOCK(cpu);
    free_hook(s,h);
    CACHE_MANAGED_STATE(cpu).count--;
    LOCAL_UNLOCK(cpu);
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
			
			struct _time_hook *h = _nk_time_hook_register_cpu(hook,state,period_cycles,s,i);
			
			if (!h) {
				ERROR("Failed to register per-cpu hook on cpu %d\n",i);
				fail=1;
				break;
			}
			// h->early_min = -1;
			// h->late_min = -1; 
			uh->per_cpu_hooks[i] = h;
			uh->count++;
			
		}
    }
    
    if (fail) {
		
		DEBUG("Unwinding per-cpu hooks on fail\n");
		for (i=0;i<n;i++) {
			if (uh->per_cpu_hooks[i]) { 
				struct nk_time_hook_state *s = sys->cpus[i]->timehook_state;
				_nk_time_hook_unregister_cpu(uh->per_cpu_hooks[i],s,i);
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
				LOCAL_LOCK_NO_IRQ(i);
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
				LOCAL_UNLOCK_NO_IRQ(i);
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

    INFO("nk_time_hook_register(%p,%p,period_ns=%lu (cycles=%lu), cpu=%d, cpu_mask=%p\n", hook,state,period_ns,period_cycles,cpu,cpu_mask);

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
			_nk_time_hook_unregister_cpu(uh->per_cpu_hooks[i],s,i);
			uh->count--;
		}
    }
    
    free(uh);

    return 0;
    
}


//
// THESE PRIMITIVES ARE INCORRECT IN THE GENERAL CASE
// BUT MAY BE OK FOR THIS 
//

static inline uint8_t hook_irq_disable_save(void)
{

    uint64_t rflags = read_rflags();

    if (rflags & RFLAGS_IF) {
      cli();
    }

    return !!(rflags & RFLAGS_IF);

}
        

static inline void hook_irq_enable_restore (uint8_t iflag)
{
    if (iflag) {
      sti();
    }
}


// Statistics --- global
uint64_t early_count = 0;
uint64_t late_count = 0;


// ready is set once the time hook framework is functional
// on all cpus.  Before that, compiler-injected calls to
// time hook fire must be gnored.
static int ready = 0;

// instrumentation to measure overheads within time hook fire
// non-static because they are set elsewhere once we are ready
// to start timing
int ACCESS_HOOK = 0;
nk_thread_t *hook_compare_fiber_thread = 0;

// this is the part that needs to be fast and low-overhead
// it should not block, nor should anything it calls...
// nor can they have nk_time_hook_fire() calls...
// this is where to focus performance improvement

// Interrupt handling in nk_time_hook_fire
#define IRQ 1

// Per-cpu access for nk_time_hook_fire --- if NO
// option is set --- fire executes on all CPUs
#define ONLY_CPU_ONE 0 // Execute only on CPU 1
#define RANGE 0 // Execute in a range of CPUs (determined by NUM_CPUS_PHI)
#define RANGE_NOT_ZERO 0 // Execute in a range of CPUs EXCLUDING CPU 1 (determined by NUM_CPUS_PHI) 
#define NUM_CPUS_PHI 64 // Number of CPUs to execute on

// Setting and locking options for the 
// time hook state and per-cpu locks
#define USE_ATOMICS 0 // State setting using atomics
#define USE_SET 1 // Simple state setting
#define USE_LOCK_AND_SET 0 // Simple state setting and per-cpu locking/checks

// Testing --- permissions global
#define MAX_WRAPPER_COUNT 1000
extern int ACCESS_WRAPPER;
extern int time_interval;
extern uint64_t last;
extern uint64_t wrapper_data[MAX_WRAPPER_COUNT];
extern uint64_t overhead_count;

__attribute__((noinline, annotate("nohook"))) void nk_time_hook_fire()
{
   // Some CPU is not yet up --- usually
   // before all per-cpu and/or time-hook
   // state is correctly set
   if (!ready) {
       return;
   }


   int mycpu = my_cpu_id();

// PAD: CONSIDER SEEING WHAT HAPPENS WHEN WE DO cpu<n for different
// VALUES OF n 

// Handle CPU access

#if ONLY_CPU_ONE
   if (mycpu != 1) {
	   return;
   }
#endif


#if RANGE
   if (!(mycpu < NUM_CPUS_PHI)) {
       return;
   }
#endif

#if RANGE_NOT_ZERO
   if (!((mycpu < (NUM_CPUS_PHI)) && (mycpu > 0))) { 
	   return;
   }
#endif


// Overhead instrumentation
#if GET_WHOLE_HOOK_DATA

   uint64_t whole_rdtsc_queue = 0, whole_rdtsc_fire;
   if ((mycpu == TARGET_CPU)
	   && (hook_compare_fiber_thread == get_cur_thread())) {

     if (!(nk_fiber_current()->is_idle)) { 

       if (ACCESS_WRAPPER) { whole_rdtsc_queue = rdtsc(); }
	 
	 }
	
   }
  
#endif


// CHANGE --- move data collection for yield hooks into
// nk_time_hook_fire --- much more straightforward to 
// read and control
#if GET_FIBER_DATA

  // Determine if we're on the target CPU and we're on the
  // fiber thread of that target CPU
  if ((mycpu == TARGET_CPU)
	  && (hook_compare_fiber_thread == get_cur_thread())) {

	// Determine if we're on a non-idle fiber on 
	// the fiber thread
    if (!(nk_fiber_current()->is_idle)) { 

	  // Determine if we have enough capacity in our data array
	  // and if we have access/"permissions" to yield --- then
	  // yield if necessary
      if (ACCESS_WRAPPER && (time_interval < MAX_WRAPPER_COUNT)) { 
		wrapper_data[time_interval++] =  rdtsc() - last;
		last = rdtsc();
      }
	
	}
  
  }

#endif


// Handle data collection --- start queueing portion
#if GET_HOOK_DATA
   uint64_t rdtsc_hook_start = 0, rdtsc_hook_end = 0, rdtsc_hook_fire_start = 0, rdtsc_hook_fire_end = 0;
   int local_hook_time_index = hook_time_index; 
   if (mycpu == TARGET_CPU) {	
	   if (ACCESS_HOOK && (hook_compare_fiber_thread == get_cur_thread())) {
		   if (hook_time_index < MAX_HOOK_DATA_COUNT) {
			   hook_time_index++;
		   }
		   if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
			   rdtsc_hook_start = rdtsc();
		   }
	   }
   }
#endif

   // ------ QUEUEING PORTION ------

   
#if 0
   static struct sys_info *sys = 0; // per_cpu_get(system);
   
   if (!sys) {
       sys = per_cpu_get(system);
   }
#endif

   struct sys_info *sys = per_cpu_get(system);
   struct nk_time_hook_state *s = sys->cpus[mycpu]->timehook_state;
   
#if IRQ
   // Disable interrupts --- prevent CPU "mycpu" from reentering nk_time_hook_fire
   // and preventing the CPU from context switching into another thread while in
   // nk_time_hook_fire --- i.e. allow fire to act as an actual interrupt handler
   uint8_t flags;
   flags = hook_irq_disable_save();
#endif



// Set the state accordingly (and/or locks), perform 
// necessary checks

#if USE_SET
   if (CACHE_MANAGED_STATE(mycpu).state != READY_STATE) {
       DEBUG("short circuiting fire because we are in state %d\n",CACHE_MANAGED_STATE(mycpu).state);
   	   // DS("n");
	   // DS("\n");
#if IRQ
	   hook_irq_enable_restore(flags);
#endif
   	   return;
   }

   CACHE_MANAGED_STATE(mycpu).state = INPROGRESS;
#endif


#if USE_ATOMICS
   if (!__sync_bool_compare_and_swap(&(CACHE_MANAGED_STATE(mycpu).state),
				     READY_STATE,
				     INPROGRESS)) {
       DEBUG("short circuiting fire because we are in state %d\n",CACHE_MANAGED_STATE(mycpu).state);
       return;
   }
#endif

#if USE_LOCK_AND_SET 
   LOCAL_LOCK_DECL;
   
   if (LOCAL_TRYLOCK(mycpu)) {
       DEBUG("failed to acquire lock on fire (cpu %d)\n",mycpu);
       // PAD: DOES THIS EVER HAPPEN?
       return;
   }
   
   
   if (CACHE_MANAGED_STATE(mycpu).state != READY_STATE) {
       DEBUG("short circuiting fire because we are in state %d\n",CACHE_MANAGED_STATE(mycpu).state);
       LOCAL_UNLOCK(mycpu);
       return;
   }


   CACHE_MANAGED_STATE(mycpu).state = INPROGRESS;
#endif
  
   // Start actual set up for queueing hooks 
 
   int i;
   int seen;
   
   uint64_t cur_cycles = rdtsc();

   int count = 0;
   struct _time_hook *queue[MAX_HOOKS];
		
   // TESTING IDEAS 
   // - Remove second if statment (cur_cycles ... ) --- maintain an invariant
   //   that there's only one hook in the entire kernel that's active
   // - Disable injection at 4000 and run a for loop somewhere that just calls
   //   nk_time_hook_fire. Compare the queueing time with no injections (just for
   //   loop) and with injections (everywhere)	

   // full blown code, should still be fast
   for (i = 0, seen = 0; ((i < MAX_HOOKS) && (seen < CACHE_MANAGED_STATE(mycpu).count)); i++) {
       struct _time_hook *h = &s->hooks[i];
       
       if (h->state==ENABLED) {
		   
		   seen++;
		   
		   // if (1 || (cur_cycles >= (h->last_start_cycles + h->period_cycles))) {
		   if (cur_cycles >= (h->last_start_cycles + h->period_cycles)) {
			   DEBUG("queueing hook func=%p state=%p last=%lu cur=%lu\n",
				 h->hook_func, h->hook_state, h->last_start_cycles, cur_cycles);
			   
			   queue[count++] = h;

// Get statistics
#if 0 
			 if (mycpu == TARGET_CPU) {	
		       if (ACCESS_WRAPPER && (hook_compare_fiber_thread == get_cur_thread())) {
			 	late_count++;
		       }
		     }
#endif

		   }    

// Get statistics
#if 0
		   else {
		     if (mycpu == TARGET_CPU) {	
		       if (ACCESS_WRAPPER && (hook_compare_fiber_thread == get_cur_thread())) {
			 	early_count++;
		       }
		     }
		   }
#endif

       }
   }
   
   // Simplified code for one hook
   // struct _time_hook *h = &s->hooks[0];
   // queue[count++] = h;
   
   
   // we now need to prepare for the next batch.
   // note that a hook could context switch away from us, so we need to do
   // handle cleanup *before* we execute any hooks
   
   // ** TODO ** --- need to limit nested "interrupts"


// Reset per-cpu time-hook state accordingly

#if USE_SET
   CACHE_MANAGED_STATE(mycpu).state = READY_STATE;
#endif

#if USE_ATOMICS
   __atomic_store_n(&(CACHE_MANAGED_STATE(mycpu).state),READY_STATE,__ATOMIC_SEQ_CST);
#endif

#if USE_LOCK_AND_SET 
   CACHE_MANAGED_STATE(mycpu).state = READY_STATE;
   
   LOCAL_UNLOCK(mycpu);
#endif

 
// ------ END QUEUEING PORTION ------

// Overhead instrumentation
#if GET_WHOLE_HOOK_DATA

   if ((mycpu == TARGET_CPU)
	   && (hook_compare_fiber_thread == get_cur_thread())) {

     if (!(nk_fiber_current()->is_idle)) { 

       if (ACCESS_WRAPPER) { 
		   whole_rdtsc_queue = rdtsc() - whole_rdtsc_queue;
	  	   overhead_count += whole_rdtsc_queue;
	   }
	 
	 }
	
   }
  
#endif


#if GET_WHOLE_HOOK_DATA

   if ((mycpu == TARGET_CPU)
	   && (hook_compare_fiber_thread == get_cur_thread())) {

     if (!(nk_fiber_current()->is_idle)) { 

       if (ACCESS_WRAPPER) { whole_rdtsc_fire = rdtsc(); }
	 
	 }
	
   }
  
#endif

   
// Set up data collection for firing portion
#if GET_HOOK_DATA
   if (mycpu == TARGET_CPU) { 
	   if (ACCESS_HOOK && (hook_compare_fiber_thread == get_cur_thread())) {
			rdtsc_hook_end = rdtsc();
			if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
			  hook_data[local_hook_time_index] = rdtsc_hook_end - rdtsc_hook_start;
			}
			rdtsc_hook_fire_start = rdtsc();
		}
    }
#endif
    
   // now we actually fire the hooks.   Note that the execution of one batch of hooks
   // can race with queueing/execution of the next batch.  that's the hook
   // implementor's problem

    // ------ FIRING PORTION ------
    
    for (i=0; i<count; i++) {
		struct _time_hook *h = queue[i];
		DEBUG("launching hook func=%p state=%p last=%lu cur=%lu\n",
			  h->hook_func, h->hook_state, h->last_start_cycles, cur_cycles);
		
		h->last_start_cycles = cur_cycles;
		h->hook_func(h->hook_state);
    }
    
// ------ END FIRING PORTION ------

// Overhead instrumentation
#if GET_WHOLE_HOOK_DATA

   if ((mycpu == TARGET_CPU)
	   && (hook_compare_fiber_thread == get_cur_thread())) {

     if (!(nk_fiber_current()->is_idle)) { 

       if (ACCESS_WRAPPER) { 
		   whole_rdtsc_fire = rdtsc() - whole_rdtsc_fire;
	  	   overhead_count += whole_rdtsc_fire;
	   }
	 
	 }
	
   }
  
#endif

    
    
#if GET_HOOK_DATA
	if (mycpu == TARGET_CPU) {
		if (ACCESS_HOOK && (hook_compare_fiber_thread == get_cur_thread())) {
			rdtsc_hook_fire_end = rdtsc();
			if (local_hook_time_index < MAX_HOOK_DATA_COUNT) {
				hook_fire_data[local_hook_time_index] = rdtsc_hook_fire_end - rdtsc_hook_fire_start;
			}
		}
	}
#endif

#if IRQ
	// Enable interrupts --- location in code is suspicious
	// THIS MUST REALLY BE BEFORE THE HOOK FUNCTION IS FIRED!!!!!!!
	// WHAT IF THE HOOK FUNCTION YIELDS>>>>>>>
	hook_irq_enable_restore(flags);
#endif
   

    return;
}


static int shared_init()
{
    int mycpu = my_cpu_id();
    struct sys_info *sys = per_cpu_get(system);
    struct cpu *cpu = sys->cpus[mycpu];
    struct nk_time_hook_state *s;

    s = malloc_specific(sizeof(struct nk_time_hook_state),mycpu);

    if (!s) {
		ERROR("Failed to allocate per-cpu state\n");
		return -1;
    }
    
    memset(s,0,sizeof(struct nk_time_hook_state));
    
    cpu->timehook_state = s;
    
    INFO("inited\n");
    
    return 0;
    
}

int nk_time_hook_init()
{
    int cpu;
    memset(cms,0,sizeof(cms));

    for (cpu=0;cpu<NAUT_CONFIG_MAX_CPUS;cpu++) {
        spinlock_init(&cms[cpu].lock);
    }
    
    return shared_init();
}

int nk_time_hook_init_ap()
{
    return shared_init();
}

static int cpu_count = 0;
int nk_time_hook_start()
{
  int mycpu = my_cpu_id();
  struct sys_info *sys = per_cpu_get(system);
  struct nk_time_hook_state *s = sys->cpus[mycpu]->timehook_state;

/* #if 1
  ready = 1;
  if (my_cpu_id() == 1) {
    s->state = READY_STATE;
  }
#else*/
  CACHE_MANAGED_STATE(mycpu).state = READY_STATE;
 
  if ((__sync_fetch_and_add(&cpu_count, 1) + 1) == nk_get_num_cpus())  {
      ready = 1;
      INFO("time hook ready set\n");
  } 
  
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

