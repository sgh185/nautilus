#include "patching.h"


int carat_patch_escapes(allocEntry *entry, void* allocationTarget) {

    nk_slist_node_uintptr_t *iter;
    uintptr_t val;
    
	nk_slist_foreach((entry->allocToEscapeMap), val, iter) {
		nk_vc_printf("d: %lu\n", val); 
	}	
	
	nk_slist_foreach((entry->allocToEscapeMap), val, iter) {
        
		
		void** escape = (void**) val;
        sint64_t offset = doesItAlias(entry->pointer, entry->length, (uint64_t) *escape);
        if(offset >= 0) {
            *escape = (void*) ((sint64_t) allocationTarget + offset);
        }


    }

    return 0;
}

// Possibly * DEPRECATED * --- replaced by proper placement of 
// instrumented calls to the runtime
int carat_update_entry(allocEntry *entry, void *allocationTarget) {

    // Create a new entry
    allocEntry *newEntry = allocEntrySetup(allocationTarget, entry->length);
    newEntry->allocToEscapeMap = entry->allocToEscapeMap;
    
	// CONV [map::insert_or_assign] -> [nk_map_insert + status check]
	if (!(nk_map_insert(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) allocationTarget), ((uintptr_t) newEntry)))) {
		panic("carat_update_entry: nk_map_insert failed on allocationTarget %p\n", allocationTarget);
	}

	// Remove the old pointer from the map	
    nk_map_remove(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) (entry->pointer)));

    return 0;
}

// Debugging
static void 
print_regs (struct nk_regs * r)
{
    #define DS(x) nk_vc_printf(x);
    #define DHQ(x) nk_vc_printf("%lx", x);
    #define PRINT3(x, x_extra_space, y, y_extra_space, z, z_extra_space) DS(#x x_extra_space" = " ); DHQ(r->x); DS(", " #y y_extra_space" = " ); DHQ(r->y); DS(", " #z z_extra_space" = " ); DHQ(r->z); DS("\n");

    DS("[-------------- Register Contents --------------]\n");
    
    PRINT3(rip, "   ",  rflags, "", rbp, "   ");
    PRINT3(rsp, "   ",  rax, "   ", rbx, "   ");
    PRINT3(rsi, "   ",  r8, "    ", r9, "    ");
    PRINT3(r10, "   ",  r11, "   ", r12, "   ");
    PRINT3(r13, "   ",  r14, "   ", r15, "   ");

  }

// Patch shared state in the threads, registers manually
static void handle_thread(struct nk_thread *t, void *state) {
    struct move_alloc_state *move_state = (struct move_alloc_state*) state;
    struct nk_regs * r = (struct nk_regs*)((char*)t->rsp - 128); // FIX: t->rsp - 128 might be wrong, look at garbage collector

    // Sanity check for t->rsp - 128
    nk_vc_printf(t->name); // should be shell
    print_regs(r); // r15 should have 0xDEADBEEFB000B000


// moved these comments out of our HANDLE
// if the register is within our alloc
// DEBUG("It aliased %p will now become %p which is offset %ld\n", registerPtr.ptr, newAllocPtr.ptr, offset);
// DEBUG("The register %s is now %p\n", regNames[i], (void*) uc->uc_mcontext.gregs[i]);
#define HANDLE(state, reg) \
    if((r->reg >= (uint64_t) state->allocationToMove) && (r->reg < ((uint64_t) state->allocationToMove + state->length))){ \
            uint64_t offset = r->reg - (uint64_t) state->allocationToMove; \
            uint64_t newAddr = (uint64_t) state->allocationTarget + offset; \
            r->reg = newAddr; \
    } \

    HANDLE(((struct move_alloc_state *) state), r15)
	HANDLE(((struct move_alloc_state *) state), r14)
    HANDLE(((struct move_alloc_state *) state), r13)
    HANDLE(((struct move_alloc_state *) state), r12)
    HANDLE(((struct move_alloc_state *) state), r11)
    HANDLE(((struct move_alloc_state *) state), r10)
    HANDLE(((struct move_alloc_state *) state), r9)
    HANDLE(((struct move_alloc_state *) state), r8)
    HANDLE(((struct move_alloc_state *) state), rbp)
    HANDLE(((struct move_alloc_state *) state), rdi)
    HANDLE(((struct move_alloc_state *) state), rsi)
    HANDLE(((struct move_alloc_state *) state), rdx)
    HANDLE(((struct move_alloc_state *) state), rcx)
    HANDLE(((struct move_alloc_state *) state), rbx)
    HANDLE(((struct move_alloc_state *) state), rax)
	// handle rsp and rip later

}


// Top-level interface for patching ---
// - allocationToMove --- address to move/needs patching
// - allocationTarget --- address to move to/patch to
int nk_carat_move_allocation(void *allocationToMove, void *allocationTarget) {

	// Oaklahoma will indeed reopen if the damn 
	// world can't be stopped
	if (!(nk_sched_stop_world())) {
        CARAT_PRINT("Oaklahoma has reopened!\n");
        return -1;
    }


	// Find the entry for the allocationToMove addr in the 
	// allocationMap --- if not found, return immediately
    allocEntry* entry = findAllocEntry(allocationToMove);
    if (!entry) {
        CARAT_PRINT("Cannot find entry\n");
        goto out_bad;
    }

	
    // Generate what patches need to be executed for overwriting 
	// memory, then executes patches for escapes to allocationTarget
    if (carat_patch_escapes(entry, allocationTarget) == 1) {
        CARAT_PRINT("The patch is toast --- can't patch escapes !\n");
        goto out_bad;
    }


    // For each thread, patch registers to the allocationTarget --- 
	// takes a "state," which is just a packaged structure containing 
	// necessary info to perform the patching --- like to and from 
	// addrs, length, etc.
    struct move_alloc_state state = {allocationToMove, allocationTarget, entry->length, 0};
    nk_sched_map_threads(-1, handle_thread, &state);
    if (state.failed) {
        CARAT_PRINT("Unable to patch threads\n");
        goto out_bad;
    }
   
	// Perform the real memory movement (the "actual patch") --- move
	// the memory at allocationToMove to allocationTarget 
    memmove(allocationToMove, allocationTarget, entry->length);
   

   	// DEPRECATED
    /* carat_update_entry(entry, allocationTarget); */

    /*
	 * Do we need to handle our own stack? --- this is the possible 
	 * location in the handler where the stack needs to be handled 
	 */

    nk_sched_start_world();
    return 0;

out_bad:
    nk_sched_start_world();
    return -1;

}

allocEntry *findRandomAlloc() {

	// Select a random index from the map 
    srand48(29848349); 
	uint64_t target = lrand48() % nk_map_get_size(allocationMap), // randomized
    		 count = 0;

	// Find the allocation at that index in the 
	// allocationMap
	nk_slist_node_uintptr_t_uintptr_t *iter;
	nk_pair_uintptr_t_uintptr_t *pair;
	nk_map_foreach(allocationMap, pair, iter) 
	{
		if (count == target) { return ((allocEntry *) (pair->second)); }
		count++;
	}

    return 0;
}

/* --------- TEST CASE ---------  */

// Test --- perform a random move:
// - Pick a random allocation from the allocationMap
// - Allocate new memory of the same size as the randomly
//   picked allocation --- this will be the new destination
//   for the move
// - call nk_carat_move_allocation on the old and new 
//   memory addresses
// 
// Test can execute an arbitrary number of sequential
// moves based on user input

// Shell command handler
static int handle_karat_test(char *buf, void *priv)
{
    uint64_t count;
    uint64_t i;
    if(sscanf(buf, "karat_test %lu", &count) == 1) {

        nk_vc_printf("Moving %lu times.\n", count);

		// count --- peform "count" number of sequential
		// moves
        for(i = 0; i < count; i++) {

			// Find a random entry from the allocation table
            allocEntry* entry = findRandomAlloc();
            if(!entry) {
                nk_vc_printf("Random entry not found.\n");
                return -1;
            }

			// Set old and new --- the new memory allocation
			// is based on the size of the randomly selected
			// allocation from the allocationMap
            void* old = entry->pointer;
            uint64_t length = entry->length;

			// NOTE --- this malloc will be instrumented by
			// the compiler
           	void *new = malloc(length);

            if(!new) {
                nk_vc_printf("Malloc failed.\n");
                return -1;
            }

            nk_vc_printf("Attempting to move from %p to %p, size %lu\n", old, new, length);

			// Debugging step --- necessary to make sure that
			// nk_carat_move_allocation is targetting the right
			// threads, etc. The below value represents a tracker
			// that can be printed when perform the patches at
			// the thread level
			//
			// NOTE --- may cause issues with current implementation
			//
			// FIX
			//
			//
#if CARAT_STACK_CHECK
            __asm__ __volatile__("movabsq $0xDEADBEEFB000B000, %r15"); // check if our stack offset is correct

            nk_vc_printf("Got past the stack check\n");
#endif
           
		   	// Perform the move	
			if(nk_carat_move_allocation(old, new)) {
                nk_vc_printf("Move failed.\n");
                return -1;
            }

            nk_vc_printf("Move succeeded.\n");

			// NOTE --- this will be instrumented by the compiler
            free(old);

			// NOTE --- nk_carat_update_entry is rendered useless
			// at this stage (see old versions) --- compiler
			// instrumentation should take care of the new
			// allocation and old allocation removal

            nk_vc_printf("Free succeeded.\n");
        }

    } else {
        nk_vc_printf("Invalid Command.");
    }

    return 0;
}

static struct shell_cmd_impl karat_test_impl = {
    .cmd = "karat_test",
    .help_str = "karat_test",
    .handler = handle_karat_test,
};

nk_register_shell_cmd(karat_test_impl);


/* ---------- ALLOCATION MAP DEBUGGING ---------- */

// Handler --- printing the allocationMap
static int handle_print_table() {

	// Table formatting:
	nk_vc_printf("map node addr  :  ( real addr  :  allocEntry addr  ---  ( pointer,  len ))\n"); 

	// Loop through the allocationMap, print all 
	// information necessary
    nk_slist_node_uintptr_t_uintptr_t *iter;
    nk_pair_uintptr_t_uintptr_t *pair;
    nk_map_foreach(allocationMap, pair, iter) {        
        allocEntry *the_entry = (allocEntry *) (pair->second);
		nk_vc_printf("%p : (%p : %p --- (ptr: %p, len: %d))\n", 
					 iter, 
					 pair->first, 
					 the_entry, 
					 the_entry->pointer, 
					 the_entry->length);
    }

    return 0;
}

static struct shell_cmd_impl print_table_impl = {
    .cmd = "print_table",
    .help_str = "print_table",
    .handler = handle_print_table,
};

nk_register_shell_cmd(print_table_impl);

