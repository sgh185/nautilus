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

#include "nautilus/signal.h"
#include "nautilus/thread.h"
#include "nautilus/mm.h"
#include "nautilus/backtrace.h"


/* DEBUG, INFO, ERROR PRINTS */
//#ifndef NAUT_CONFIG_DEBUG_PROCESSES
#if 1
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SIGNAL_INFO(fmt, args...) INFO_PRINT("signal: " fmt, ##args)
#define SIGNAL_ERROR(fmt, args...) ERROR_PRINT("signal: " fmt, ##args)
//#define SIGNAL_DEBUG(fmt, args...) DEBUG_PRINT("signal: " fmt, ##args)
#define SIGNAL_DEBUG(fmt, args...) INFO_PRINT("signal: " fmt, ##args)
#define SIGNAL_WARN(fmt, args...)  WARN_PRINT("signal: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("signal: " fmt, ##args)


/* Global structures
 * TODO MAC: Move to a different file
 */ 

/* Used for testing */
void sig_hand_hello(int sig_num)
{
    SIGNAL_INFO("Hello World from signal %d.\n", sig_num);

} 

nk_signal_handler_table_t nk_signal_global_handler_table = {
    .count = 1,
    .handlers[0 ... 63] = {
        //.handler = DEFAULT_SIG,
        .handler = sig_hand_hello,
        .mask = {0},
        .signal_flags = 0,
    },
    .lock = SPINLOCK_INITIALIZER,
};

nk_signal_descriptor_t nk_signal_global_descriptor = {
    .count = 1, /* Use counter */
    .live = 1, /* TODO MAC: Not sure if we should initialize to 1 */
    /* TODO MAC: Add wait_queue init (for now we do it elsewhere) */
};


/*
nk_signal_action_t sig_act_table_entry = {
.handler = sig_hand_hello,
.mask = {0},
.signal_flags = 0,
};

nk_signal_handler_table_t global_sig_table = {
.count = 1,
.handlers[0 ... 63] = &sig_act_table_entry,
.lock = 0,
};

*/

/* Signal Macros */
#define GET_SIGNALS_PENDING(dest) (dest)->signal_state->signals_pending
#define GET_SHARED_PENDING(dest) (dest)->signal_state->signal_descriptor->shared_pending

static inline nk_signal_handler_t idx_sig_hand(nk_thread_t *dst, uint64_t signum)
{
    return dst->signal_state->signal_handler->handlers[(signum)-1].handler;
}

static inline uint8_t acquire_sig_hand_lock(nk_signal_handler_table_t *signal_handler) {
    return spin_lock_irq_save(&(signal_handler->lock));
}

static inline void release_sig_hand_lock(nk_signal_handler_table_t *signal_handler, uint8_t flags) {
    spin_unlock_irq_restore(&(signal_handler->lock), flags);
}

void set_current_blocked(nk_signal_set_t *newset)
{
    nk_thread_t *cur_thread = get_cur_thread();
    sigdelsetmask(newset, sigmask(NKSIGKILL) | sigmask(NKSIGSTOP));
    uint8_t irq_state = acquire_sig_hand_lock(cur_thread->signal_state->signal_handler);
    /* TODO MAC: This might be wrong, it may be more complex than this :) */
    cur_thread->signal_state->blocked = *newset;
    release_sig_hand_lock(cur_thread->signal_state->signal_handler, irq_state);
    /* Why do this? */
    //recalc_sigpending();
}

/************ HELPERS FOR SENDING SIGNALS *************/

static inline int sig_handler_ignored(void *handler, uint64_t sig) 
{
    return handler == IGNORE_SIG || (handler == DEFAULT_SIG && sig_kernel_ignore(sig));    
}

static int sig_ignored(uint64_t signal, nk_thread_t *signal_dest, uint64_t is_forced)
{
    /* Blocked signals are not ignored. Race condition with unblocking */
    if (sigismember(&(signal_dest->signal_state->blocked), signal) ||  
        sigismember(&(signal_dest->signal_state->real_blocked), signal))
    { return 0; }

    nk_signal_handler_t signal_handler = idx_sig_hand(signal_dest, signal); 

    /* Don't fully understand this case yet. Let's skip */
    #if 0
    if ((signal_dest->signal_state->signal_descriptor->flags & SIGNAL_UNKILLABLE) &&
        signal_handler == DEFAULT_SIG &&
        !(is_forced && (signal == NKSIGKILL || signal == NKSIGSTOP)))
    { return 1; }
    #endif

    /* TODO MAC: Case where we only send kernel signals (unimplemented) */

    /* Check if signal is ignored */
    return sig_handler_ignored(signal_handler, signal);
}

static int prepare_signal(uint64_t signal, nk_thread_t *signal_dest, uint64_t is_forced) {
    /* Get sig descriptor (shared between all threads in process/group) */
    nk_signal_descriptor_t *sig_desc = signal_dest->signal_state->signal_descriptor;

    /* TODO MAC: These things are unimplemented, but we will need them in future */
    #if 0
    if (0 && sig_desc->flags & (SIGNAL_GROUP_EXIT | SIGNAL_GROUP_COREDUMP)) {
        if (!(signal->flags & SIGNAL_GROUP_EXIT)) {
            return signal == NKSIGKILL
    } else if (sig_kernel_stop_mask(signal)) {
        /* This is a stop signal */
        /* For each thread, remove SIGCONT from sig queues */
        return -1;
    } else if (0 && signal == NKSIGCONT ) {
        /* Sig cont, remove all sig stops from queues and wake threads */
        /* Also do some complicated stuff for notifying parents */
        return -1;
    }
    #endif

    return !sig_ignored(signal, signal_dest, is_forced);
}

static nk_signal_queue_t *__sig_queue_alloc(uint64_t signal, nk_thread_t *signal_dest, uint64_t must_q)
{
    nk_signal_queue_t *q = 0;
    
    /* Fetch number of pending signals for process */
    uint64_t pending = signal_dest->signal_state->signal_descriptor->num_queued;
    atomic_inc(signal_dest->signal_state->signal_descriptor->num_queued); 
    /* Decide whether to queue a signal */
    if (must_q || (pending <= RLIMIT_SIGPENDING)) {
        /* TODO MAC: Should I use kmem alloc? */
        q = (nk_signal_queue_t*)kmem_sys_mallocz(sizeof(nk_signal_queue_t));
    } else {
        SIGNAL_INFO("Dropped signal number %lu for thread %p.\n", signal, signal_dest);
    }

    if (!q) {
        atomic_dec(signal_dest->signal_state->signal_descriptor->num_queued); 
    } else {
        INIT_LIST_HEAD(&(q->lst));
        q->flags = 0;
    }
    return q;
}

static void copy_siginfo(nk_signal_info_t *dst, nk_signal_info_t *src)
{
    memcpy(dst, src, sizeof(*src));
}

/*
 * RIPPED DIRECTLY FROM LINUX SOURCE CODE (source/kernel/signal.c line #973)
 *
 * Test if t wants to take SIG.  After we've checked all threads with this,
 * it's equivalent to finding no threads not blocking SIG.  Any threads not
 * blocking SIG were ruled out because they are not running and already
 * have pending signals.  Such threads will dequeue from the shared queue
 * as soon as they're available, so putting the signal on the shared queue
 * will be equivalent to sending it to one such thread.
 */
static inline int wants_signal(uint64_t sig, nk_thread_t *t)
{
	if (sigismember(&t->signal_state->blocked, sig)) {
		return false;
    }

	if (t->status == NK_THR_EXITED) {
		return false;
    }

	if (sig == NKSIGKILL) {
		return true;
    }

	if (t->status == NK_THR_SUSPENDED) {
		return false;
    }

	return (t->status == NK_THR_RUNNING) || !(t->num_sigs);
}

/*
 * Yanked directly from Linux source (source/kernel/signal.c Line #760)
 * 
 * Tell a process that it has a new active signal..
 *
 * NOTE! we rely on the previous spin_lock to
 * lock interrupts for us! We can only be called with
 * "siglock" held, and the local interrupt must
 * have been disabled when that got acquired!
 *
 * No need to set need_resched since signal event passing
 * goes through ->blocked
 */
void signal_wake_up_thread(nk_thread_t *t)
{
    /* TODO MAC: How do we notify threads in process that a signal is pending? */
	//set_tsk_thread_flag(t, TIF_SIGPENDING);
	
	/*
	 * TASK_WAKEKILL also means wake it up in the stopped/traced/killable
	 * case. We don't check t->state here because there is a race with it
	 * executing another processor and just now entering stopped state.
	 * By using wake_up_state, we ensure the process will wake up and
	 * handle its death signal.
	 */

    /* TODO MAC: This is probably wrong. Consult w/ Peter */ 
    /* Introduces race condition: t->current_cpu (what cpu it's on)
     *                            lock thread
     *                            if (t->status == NK_THR_INIT || EXIT) -> ERROR
     *                                          == RUNNING || SUSPENDED -> kick cpu
     *                                          == WAITING -> Throw in unimpl(), fix later
     *                                                        
     */
    spin_lock(&t->lock);
	if (t->status == NK_THR_INIT || t->status == NK_THR_EXITED) {
        SIGNAL_ERROR("Failed to wake up thread.\n");
    } else if (t->status == NK_THR_RUNNING || t->status == NK_THR_SUSPENDED) {
        nk_sched_kick_cpu(t->current_cpu);
    } else if (t->status == NK_THR_WAITING) {
        SIGNAL_ERROR("Waking threads on wait queue is unimplemented!");
    }
    spin_unlock(&t->lock);
}

static void complete_signal(uint64_t signal, nk_thread_t *signal_dest, uint64_t dest_type)
{
    /*TODO MAC: do some complicated work with choosing the right thread to deliver it to */
    nk_signal_descriptor_t *signal_desc = signal_dest->signal_state->signal_descriptor; 
    nk_thread_t *t;

    /*
     * Determine which thread to send signal to.
     *
     * In main case, we send to main thread
     */
    if (wants_signal(signal, signal_dest))
		t = signal_dest;
	else if ((dest_type == SIG_DEST_TYPE_THREAD) || (nk_thread_group_get_size(signal_dest->process->t_group) <= 1))
		/*
		 * There is just one thread and it does not need to be woken.
		 * It will dequeue unblocked signals before it runs again.
		 */
		return;
	else {
		/*
		 * Otherwise try to find a suitable thread.
		 */
		t = signal_desc->curr_target;
        /* 
         * TODO MAC: No current way to iterate through thread group. Should add this soon
         * Do nothing for now :) simply return early
         */
        SIGNAL_ERROR("UNIMPLEMENTED PORTION OF complete_signal()! Results may be incorrect.\n");		
        return;
        #if 0
        while (!wants_signal(signal, t)) {
			t = next_thread(t);
			if (t == signal_desc->curr_target)
				/*
				 * No thread needs to be woken.
				 * Any eligible threads will see
				 * the signal in the queue soon.
				 */
				return;
		}
		signal_desc->curr_target = t;
        #endif
	}

	/*
     * TODO MAC: Implement fatal signals!  
	 * Found a killable thread.  If the signal will be fatal,
	 * then start taking the whole group down immediately.
	 */
    #if 0
	if (sig_fatal(p, sig) &&
	    !(signal_desc->flags & SIGNAL_GROUP_EXIT) &&
	    !sigismember(&t->real_blocked, sig) &&
	    (sig == SIGKILL || !p->ptrace)) {
		/*
		 * This signal will be fatal to the whole group.
		 */
		if (!sig_kernel_coredump(sig)) {
			/*
			 * Start a group exit and wake everybody up.
			 * This way we don't have other threads
			 * running and doing things after a slower
			 * thread has the fatal signal pending.
			 */
			signal_desc->flags = SIGNAL_GROUP_EXIT;
			signal_desc->group_exit_code = sig;
			signal_desc->group_stop_count = 0;
			t = p;
			do {
				task_clear_jobctl_pending(t, JOBCTL_PENDING_MASK);
				sigaddset(&t->pending.signal, SIGKILL);
				signal_wake_up(t, 1);
			} while_each_thread(p, t);
			return;
		}
	}
    #endif

	/*
	 * The signal is already in the shared-pending queue.
	 * Tell the chosen thread to wake up and dequeue it.
	 */
	signal_wake_up_thread(t); 
    return;
}

static int __send_signal(uint64_t signal, nk_signal_info_t *signal_info, nk_thread_t *signal_dest, uint64_t dest_type, uint64_t is_forced)
{
    nk_signal_pending_t *pending;
    nk_signal_queue_t *signal_q;
    uint64_t must_q;
    int ret;
    ret = 0;
    ret = RES_SIGNAL_IGNORED;
    
    if(!prepare_signal(signal, signal_dest, is_forced)) {
        SIGNAL_DEBUG("Prepare signal failed... signal: %lu, signal_dest: %p.\n", signal, signal_dest);
        return ret;
    }

    /* if sending to thread, get signals pending. Else, shared pending. */
    //pending = dest_type ? &GET_SIGNALS_PENDING(signal_dest) : &GET_SHARED_PENDING(signal_dest);
    pending = dest_type ? &GET_SIGNALS_PENDING(signal_dest) : &GET_SIGNALS_PENDING(signal_dest);

    /* Non-RT signal is already pending. Return already pending */
    if ((signal < SIGRTMIN) && sigismember(&(pending->signal), signal)) {
        SIGNAL_DEBUG("Non-RT signal already pending! Not resending/queuing signal.\n");
        return ret;
        /* TODO MAC: Figure out proper return values! */
    }

    /* TODO MAC: if SIGKILL of kernel thread, skip sig info allocation */
      
    /* Must queue RT signals if they are from Kernel or from special source */
    if (signal < SIGRTMIN) {
        must_q = (signal_info <= (nk_signal_info_t *)SEND_SIGNAL_KERNEL || signal_info->signal_code >= 0);
    } else {
        must_q = 0; /* Can drop signal if needed */
    } 

    SIGNAL_DEBUG("Must_q value is %lu.\n", must_q);

    /* Allocate siq queue if possible/necessary */
    nk_signal_queue_t *q = __sig_queue_alloc(signal, signal_dest, must_q);    
    if (q) {
        SIGNAL_DEBUG("Allocated sig queue.\n");
        list_add_tail(&(q->lst), &(pending->lst));
        atomic_inc(signal_dest->num_sigs);
        /* Should we create sig info? */
        switch ((uint64_t)signal_info) {
        case (uint64_t)SEND_SIGNAL_NO_INFO:
            q->signal_info.signal_num = signal;
            q->signal_info.signal_err_num = 0;
            q->signal_info.signal_code = SI_USER; /*TODO MAC: Define this and add sender pid */
            break;
        case (uint64_t)SEND_SIGNAL_KERNEL:
            q->signal_info.signal_num = signal;
            q->signal_info.signal_err_num = 0;
            q->signal_info.signal_code = SI_KERNEL; /*TODO MAC: Define this and add sender/user pid */
            break;
        default:
            copy_siginfo(&(q->signal_info), signal_info);
        }
    } else if (signal_info > SEND_SIGNAL_KERNEL &&
               signal >= SIGRTMIN &&
               signal_info->signal_code != SI_USER)
    {
        SIGNAL_ERROR("Didn't allocate Sig Queue. Something went wrong.\n");
        return -1; /* TODO MAC: Find out if we do error codes. This should be -EAGAIN */
    } else {
        /* Silently fail */
        SIGNAL_ERROR("Failing silently for some reason :).\n");
    }

    /* TODO MAC: Maybe notify thread of signal? Look for signalfd_notify() */
    
    /* Add signal to set of pending signals */
    sigaddset(&(pending->signal), signal);
    SIGNAL_DEBUG("Added signal to pending set. Pending set: %lu.\n", pending->signal.sig[0]);
    
    /* TODO MAC: Handle multi-process signals here */
    complete_signal(signal, signal_dest, dest_type);
    SIGNAL_DEBUG("Completed signal.\n");
    
    return ret;
}

int send_signal(uint64_t signal, nk_signal_info_t *signal_info, nk_thread_t *signal_dest, uint64_t dest_type)
{
    /* Should we force the signal? No by default. */
    uint64_t is_forced = 0;

    if (signal_info == SEND_SIGNAL_NO_INFO) {
        /* TODO MAC: In what case should we force if no info sent? */
        /* For now, no */
        is_forced = 0;
    } else if (signal_info == (nk_signal_info_t *)SEND_SIGNAL_KERNEL) {
        /* Signal from kernel, we should force it */
        is_forced = 1;
    }// else if (has_si_pid_and_uid(signal_info)) { /* complicated case, skip for now */
    
    SIGNAL_DEBUG("Signal is_forced: %lu\n", is_forced);
    return __send_signal(signal, signal_info, signal_dest, dest_type, is_forced);
}


/*************** Helpers for Receiving Signals **************/

static inline uint64_t __ffz(uint64_t word)
{
	asm volatile ("rep; bsf %1,%0"
		: "=r" (word)
		: "r" (~word));
	return word;
}

#define NK_SYNCHRONOUS_MASK \
	(sigmask(NKSIGSEGV) | sigmask(NKSIGBUS) | sigmask(NKSIGILL) | \
	 sigmask(NKSIGTRAP) | sigmask(NKSIGFPE) | sigmask(NKSIGSYS))

int next_signal(nk_signal_pending_t *pending, nk_signal_set_t *mask)
{
    uint64_t i, *s, *m, x;
	int sig = 0;

	s = pending->signal.sig;
	m = mask->sig;

	/*
	 * Handle the first word specially: it contains the
	 * synchronous signals that need to be dequeued first.
	 */
	x = *s &~ *m;
    SIGNAL_DEBUG("x (%lu) = %lu &~ %lu.\n", x, *s, *m);
	if (x) {
		if (x & NK_SYNCHRONOUS_MASK) {
            x &= NK_SYNCHRONOUS_MASK;
            SIGNAL_DEBUG("x (%lu) &= NK_SYNCHRONOUS_MASK\n", x);
        }
		sig = __ffz(~x) + 1;
        SIGNAL_DEBUG("sig (%d) = __ffs(~x).\n", sig);
		return sig;
	}
    
    /* Need to do more work if we have more than 64 signals, but we don't */	

	return sig;
}

static void __sigqueue_free(nk_signal_queue_t *q)
{
    nk_thread_t *me = get_cur_thread();
	atomic_dec(me->num_sigs);
	kmem_free(q);
}

static void collect_signal(int sig, nk_signal_pending_t *pending, nk_signal_info_t *sig_info)
{
    nk_signal_queue_t *q, *first = NULL;
    list_for_each_entry(q, &pending->lst, lst) {
	    if (q->signal_info.signal_num == sig) {
			if (first) {
			    goto still_pending;
            }
			first = q;
		}
	}
    sigdelset(&pending->signal, sig);

    if (first) {
still_pending:
		list_del_init(&first->lst);
		copy_siginfo(sig_info, &first->signal_info);
		__sigqueue_free(first);
	} else {
		/*
		 * Ok, it wasn't in the queue.  This must be
		 * a fast-pathed signal or we must have been
		 * out of queue space.  So zero out the info.
		 */
		sig_info->signal_num = sig;
		sig_info->signal_err_num = 0;
		sig_info->signal_code = SI_USER;
		//sig_info->si_pid = 0;
		//sig_info->si_uid = 0;
	}
}

/*
 * Remove signals in mask from the pending set and queue.
 * Returns 1 if any signals were found.
 *
 * All callers must be holding the siglock!!
 */
static void flush_sigqueue_mask(nk_signal_set_t *mask, nk_signal_pending_t *s)
{
	nk_signal_queue_t *q, *n;
	nk_signal_set_t m;

	sigandsets(&m, mask, &s->signal);
	if (sigisemptyset(&m)) {
		return;
    }

	sigandnsets(&s->signal, &s->signal, mask);
	list_for_each_entry_safe(q, n, &s->lst, lst) {
		if (sigismember(mask, q->signal_info.signal_num)) {
			list_del_init(&q->lst);
			__sigqueue_free(q);
		}
	}
}



int dequeue_signal(nk_thread_t *thread, nk_signal_set_t *sig_mask, nk_signal_info_t *sig_info)
{
    
    nk_signal_pending_t *pending = &(thread->signal_state->signals_pending);

    int sig = next_signal(pending, sig_mask);

	if (sig) {
        /* TODO MAC: Don't currently handle notifiers... skip 
		if (current->notifier) {
			if (sigismember(current->notifier_mask, sig)) {
				if (!(current->notifier)(current->notifier_data)) {
					clear_thread_flag(TIF_SIGPENDING);
					return 0;
				}
			}
		} */
    
        /* TODO MAC: Will do this in the future... basically getting sig info */
        /* For now, we just remove from queue */
		collect_signal(sig, pending, sig_info);
	} else { /* TODO MAC: check shared pending... */
       return sig; 
    }


	return sig;
}

int get_signal_to_deliver(nk_signal_info_t *sig_info, nk_signal_action_t *ret_sig_act, uint64_t rsp)
{
    nk_thread_t *cur_thread = get_cur_thread();
    nk_signal_handler_table_t *sig_hand = cur_thread->signal_state->signal_handler;
    nk_signal_descriptor_t *sig_desc = cur_thread->signal_state->signal_descriptor;
    int signal_num;
    uint8_t irq_state;
   
    /* Skipping work to check if current task (?) and is user denies signal */
 
    /* Skipping a ton of work for stopped threads. Should implement this later */ 
relock:
    SIGNAL_DEBUG("Attempting to acquire sig_hand lock.\n");
    irq_state = acquire_sig_hand_lock(sig_hand);
    

    /* Do all pending signals */
    for (;;) {
        nk_signal_action_t *tmp_sig_act;

        /* Skipping work to check job ctrl status (?) */

        /* Dequeue signal and check if it's valid */
        signal_num = dequeue_signal(cur_thread, &(cur_thread->signal_state->blocked), sig_info);
        SIGNAL_DEBUG("Dequeued signal num: %d.\n", signal_num);

        if (!signal_num) { /* Signal num == 0, break */
            SIGNAL_DEBUG("Breaking: Signal_Num == 0.\n");
            break;
        }
        // Decrement # of pending signals (nvm, this is done in dequeue signal)
        //cur_thread->num_sigs = cur_thread->num_sigs - 1;

        /* Skip ptrace case */

        tmp_sig_act = &(sig_hand->handlers[signal_num-1]);

        /* Again, ignore trace step */

        if (tmp_sig_act->handler == IGNORE_SIG) {
            continue; /* Signal is ignored, no work to do */
        }

        /* Custom signal handler is being used */
        if (tmp_sig_act->handler != DEFAULT_SIG) {
            SIGNAL_DEBUG("Custom signal handler being used for signal: %d.\n", signal_num);
            *ret_sig_act = *tmp_sig_act; /* We will return this to calling func */

            /* Signal handler should only be run once, reset to default */
            if (tmp_sig_act->signal_flags & SIG_ACT_ONESHOT) {
                tmp_sig_act->handler = DEFAULT_SIG;
            }

            break;
        }

        /* Default action for signal might be to ignore it. */
        if (sig_kernel_ignore(signal_num)) { /* kernel only, default is ignore */
            SIGNAL_DEBUG("Ignoring signal: %d.\n", signal_num);
            continue;
        }

        /* Skipping unkillable case (N/A for Nautilus?) */

        /* TODO MAC: Stop signal, unimpl for now */
        if (sig_kernel_stop(signal_num)) {
            /* Stop all threads in thread group */
            SIGNAL_ERROR("Signal stop unimplemented!\n");
            continue;
        }

        /* All other signals are fatal. */
        /* TODO MAC: Implement fatal signals! */
    
        SIGNAL_ERROR("Fatal signals are unimplemented!\n");
    } 

    /* End of loop, all other cases reach this point */
    SIGNAL_DEBUG("Releasing sig_hand lock.\n");
    release_sig_hand_lock(sig_hand, irq_state);
    return signal_num;
}

void signal_delivered(uint64_t signal, nk_signal_info_t *sig_info, nk_signal_action_t *sig_act, uint64_t rsp)
{
    nk_signal_set_t blocked;

    /* TODO MAC: 
     * signal was successfully delivered and saved sigmask stored on sig frame.
     * Store sig mask will be restored by sigreturn.
     * We can clear the restore sigmask field.
     */

     
    sigorsets(&blocked, &(get_cur_thread()->signal_state->blocked), &(sig_act->mask));
    if (!(sig_act->signal_flags & SIG_ACT_NODEFER)) {
        sigaddset(&blocked, signal);
    }
    set_current_blocked(&blocked);
}

void __attribute__((noinline)) __sighand_wrapper(uint64_t signal, nk_signal_info_t *sig_info, uint64_t rsp, nk_signal_action_t *sig_act)
{
    /* Should only enter here through iretq. I think. */
    (sig_act->handler)(signal); /*sig_info, (void*)rsp); */
    SIGNAL_DEBUG("We returned to __sighand_wrapper.\n");
    return;

}

static int __attribute__ ((noinline)) setup_rt_frame(uint64_t signal, nk_signal_action_t *sig_act, nk_signal_info_t *sig_info, uint64_t rsp)
{
    /* For debugging purposes, print out what the original iframe looks like */
    SIGNAL_DEBUG("Dumping out saved regs at rsp.\n");
    nk_dump_mem((void*)rsp, sizeof(struct nk_regs));

    /* Copy iframe to our current frame */ 
    struct nk_regs this_frame;
    memcpy(&this_frame, (void *)rsp, sizeof(struct nk_regs));

    /* For debugging... print our copied frame */
    SIGNAL_DEBUG("Dumping out saved regs in this frame.\n");
    nk_dump_mem(&this_frame, sizeof(struct nk_regs));  

    /* We will return to a signal handler wrapper which will call the handler */ 
    /* This might not work for other compilers... we'll see */
    this_frame.rsp = (uint64_t) __builtin_frame_address(0) + 0x8;
    this_frame.rip = (uint64_t)__sighand_wrapper;

    /* Now we fill in our registers w/ signal handler args */
    this_frame.rdi = (uint64_t)signal;
    this_frame.rsi = (uint64_t)sig_info;
    this_frame.rdx = (uint64_t)rsp;
    this_frame.rcx = (uint64_t)sig_act;
    //this_frame.rsi = (uint64_t)0xDEADBEEF01234567UL;

    /* For debugging... see what our current copied iframe looks like */
    SIGNAL_DEBUG("Dumping out saved regs after modification.\n");
    nk_dump_mem(&this_frame, sizeof(struct nk_regs));  

    /* set new stack ptr, pop regs, and iret */
    asm volatile("movq %0, %%rsp; \n"
            "popq %%r15; \n"
            "popq %%r14; \n"
            "popq %%r13; \n" 
            "popq %%r12; \n"
            "popq %%r11; \n"
            "popq %%r10; \n"
            "popq %%r9; \n"  
            "popq %%r8; \n"
            "popq %%rbp; \n"
            "popq %%rdi; \n"
            "popq %%rsi; \n"
            "popq %%rdx; \n"
            "popq %%rcx; \n"
            "popq %%rbx; \n"
            "popq %%rax; \n"
            "addq $16, %%rsp; \n" /* Pop off vector and err code */
            "iretq; \n"
            : /* No output */
            : "r"(&this_frame) /* input = ptr to iframe */
            : /* No clobbered registers */
            ); 
    return 0; /* I don't think we'll ever return here, but compiler was complaining */
     
}

static void __attribute__((noinline)) handle_signal(uint64_t signal, nk_signal_info_t *sig_info, nk_signal_action_t *sig_act, uint64_t rsp)
{
    /* TODO MAC: Choosing, once again, to ignore syscall stuff */

    /* TODO MAC: Ignoring debugger case... ? */
    nk_signal_set_t old_blocked = get_cur_thread()->signal_state->blocked;
    if (setup_rt_frame(signal, sig_act, sig_info, rsp) < 0) {
        /* Force sigsegv? We'll just panic :) */
        panic("Things got screwed up during signal handling.\n");
    }
    /* Should return here after signal handler? */
    /* TODO MAC: Clear direction flag as per ABI for function entry ???? */
    SIGNAL_DEBUG("Returned from signal handler in handle_signal.\n");
    
    //signal_delivered(signal, sig_info, sig_act, rsp);
}

/* Simplified version of do_notify_resume. May be used if we implement do_signal() separately */
void __attribute__((noinline)) do_notify_resume(uint64_t rsp, uint64_t num_sigs)
{
    SIGNAL_DEBUG("Entered do_notify_resume() w/ rsp: %#018x and num_sigs = %lu.\n", rsp, num_sigs);
    nk_signal_action_t sig_act;
    nk_signal_info_t sig_info; 
    int signal;
    signal = get_signal_to_deliver(&sig_info, &sig_act, rsp);
    
    SIGNAL_DEBUG("Got signal to deliver: %d.\n", signal);

    if (signal > 0) {
        SIGNAL_DEBUG("Entering handle signal. Sig: %d, sig_info: %p, sig_act: %p.\n", signal, &sig_info, &sig_act);
        handle_signal(signal, &sig_info, &sig_act, rsp);
    }
    
    SIGNAL_DEBUG("Returned from signal handler (past if statement)!\n");

    /* Ignoring sycall stuff... not sure what it's for */
    
    /* TODO MAC: No signal to deliver? Restore sig mask? */
}




/*************** Initializing Signal State *****************/
int nk_signal_init_task_state(nk_signal_task_state **state_ptr, nk_thread_t *t) {
    /* TODO MAC: Might want to use a different malloc? */
    nk_signal_task_state *state = (nk_signal_task_state *) kmem_sys_mallocz(sizeof(nk_signal_task_state));
    if (!state) {
        SIGNAL_ERROR("Failed to allocate task state.\n");
        return -1;
    }
   
    /*
     * TODO MAC: For now, allocate handler and descriptor for each thread.
     * This is super naive because we want to share between threads in process.
     * Must figure out how to relay that thread will be part of a process.
     */
    nk_signal_handler_table_t *handler = (nk_signal_handler_table_t *) kmem_sys_mallocz(sizeof(nk_signal_handler_table_t));
    if (!handler) {
        SIGNAL_ERROR("Failed to allocate signal handler table.\n");
        free(state);
        return -1;
    }

    nk_signal_descriptor_t *desc = (nk_signal_descriptor_t *) kmem_sys_mallocz(sizeof(nk_signal_descriptor_t));
    if (!desc) {
        SIGNAL_ERROR("Failed to allocate signal descriptor.\n");
        free(state);
        free(handler);
        return -1;
    }

    /* Memcpy default handler and desc */
    memcpy(handler, &nk_signal_global_handler_table, sizeof(nk_signal_global_handler_table)); 
    memcpy(desc, &nk_signal_global_descriptor, sizeof(nk_signal_global_descriptor)); 
    
    /* Set all fields in task struct */
    state->signal_handler = handler;
    state->signal_descriptor = desc;
    state->signal_descriptor->curr_target = t;
    state->blocked.sig[0] = 0;
    state->real_blocked.sig[0] = 0;
    init_sigpending(&state->signals_pending); 

    /* Return by reference */
    if (!state_ptr) {
        SIGNAL_ERROR("Cannot return task signal state by reference.\n");
        return -1;
    }
    *state_ptr = state;
    return 0; 
}

/*************** Interface Functions *****************/

int nk_signal_send(uint64_t signal, nk_signal_info_t *signal_info, void *signal_dest, uint64_t dest_type)
{
    uint8_t flags;
    int ret = -1;

    /* Get Signal Handler so we can acquire the lock */
    nk_signal_handler_table_t *sig_hand;
    if (dest_type) {
        sig_hand = ((nk_thread_t*)signal_dest)->signal_state->signal_handler;
    } else {
        sig_hand = ((nk_process_t*)signal_dest)->signal_handler;
    }

    SIGNAL_DEBUG("Got signal handler of type %lu.\n", dest_type);
    
    /* Check if valid signal destination */
    /* May need to acquire lock? */
    nk_thread_t *sig_dest;
    if (dest_type == SIG_DEST_TYPE_PROCESS) {
        nk_process_t *proc = (nk_process_t*)signal_dest;
        nk_thread_group_t *t_group = proc->t_group;
        /* TODO MAC: Pick a random thread within the proces */
        SIGNAL_ERROR("Dest Type (%lu) Unimplemented.\n", dest_type);
        return ret;
    } else if (dest_type == SIG_DEST_TYPE_THREAD) {
        sig_dest = (nk_thread_t*)signal_dest; 
        SIGNAL_DEBUG("Signal destination type: SIG_DEST_TYPE_THREAD.\n");
    } else { /* For now, just thread. Could be thread group in future? */
        SIGNAL_ERROR("Invalid destination type %lu. Failed to send signal.\n", dest_type);
        return ret; /* not thread or process? break!! */
    } 

    /* TODO MAC: Should probably check if the destination exists */
    flags = acquire_sig_hand_lock(sig_hand);
    SIGNAL_DEBUG("Acquired signal handler lock of handler: %p.\n", sig_hand);
    ret = send_signal(signal, signal_info, signal_dest, dest_type);
    release_sig_hand_lock(sig_hand, flags);
    return ret;
}

int do_sigaction(uint64_t sig, nk_signal_action_t *act, nk_signal_action_t *old_act)
{
    nk_thread_t *thread = get_cur_thread();
    nk_process_t *p = thread->process; 
    
	nk_signal_action_t *k;
	nk_signal_set_t mask;

	if (!valid_signal(sig) || sig < 1 || (act && sig_kernel_only(sig))) {
		return -1;
    }

	k = &thread->signal_state->signal_handler->handlers[sig-1];

    uint8_t irq_state = acquire_sig_hand_lock(thread->signal_state->signal_handler);
	if (old_act) {
		*old_act = *k;
    }

    //TODO MAC: I have no clue what this is :)
	//sigaction_compat_abi(act, oact);

	if (act) {
		sigdelsetmask(&act->mask,
			      sigmask(NKSIGKILL) | sigmask(NKSIGSTOP));
		*k = *act;
		/*
		 * POSIX 3.3.1.3:
		 *  "Setting a signal action to SIG_IGN for a signal that is
		 *   pending shall cause the pending signal to be discarded,
		 *   whether or not it is blocked."
		 *
		 *  "Setting a signal action to SIG_DFL for a signal that is
		 *   pending and whose default action is to ignore the signal
		 *   (for example, SIGCHLD), shall cause the pending signal to
		 *   be discarded, whether or not it is blocked"
		 */
        nk_signal_handler_t hand = thread->signal_state->signal_handler->handlers[sig-1].handler;
		if (sig_handler_ignored(hand, sig)) {
			sigemptyset(&mask);
			sigaddset(&mask, sig);
			flush_sigqueue_mask(&mask, &thread->signal_state->signal_descriptor->shared_pending);
            if (p) {
                SIGNAL_ERROR("Sig action not implemented for processes!\n");
                /* TODO MAC: if process, iterate through each thread and clear queues */
			    //for_each_thread(p, t)
				  //  flush_sigqueue_mask(&mask, &t->pending);
            }
		}
	}

	release_sig_hand_lock(thread->signal_state->signal_handler, irq_state);
	return 0;
}

