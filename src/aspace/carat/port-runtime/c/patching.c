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
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, Gaurav Chaudhary <gauravchaudhary2021@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 * 			Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


#include <aspace/patching.h>

#if 0
/*
 * Handler for patching escapes upon moving @entry->pointer to @allocation_target 
 */ 
void _carat_patch_escapes(allocation_entry *entry, void *allocation_target) 
{
    /*
     * TOP --- Iterate over each escape, and change what each escape  
     * points to --- i.e. "patch" the escape
     */ 

    /*
     * Iterate over the escapes of @entry --- NOTE --- the iteration 
     * macro provides a variable "val," indicating the current element 
     */ 
    CARAT_ESCAPES_SET_ITERATE((entry->escapes_set))
    {
        /*
         * Set up the escape
         */ 
		void **curr_escape = ((void **) val);
        void *curr_escape_value = *curr_escape;


        /*
         * Calculate the offset, if possible, of the escape within the entry
         */
        sint64_t offset = _carat_get_query_offset (
            curr_escape_value,
            entry->pointer,
            entry->size
        );


        /*
         * Sanity check the offset
         */ 
        if (offset < 0) 
        { 
            CARAT_PRINT("_carat_patch_escapes: cannot find offset for %p\n", curr_escape);
            continue;
        }


        /*
         * Change what "curr_escape" points to
         */ 
        *curr_escape = (void *) (((uint64_t) allocation_target) + offset);
    }


    return 0;
}
#endif


int carat_patch_escapes(allocation_entry *entry, void* allocation_target) {

    nk_slist_node_uintptr_t *iter;
    uintptr_t val;
    
	nk_slist_foreach((entry->allocToEscapeMap), val, iter) {
		nk_vc_printf("d: %lu\n", val); 
	}	
	
	nk_slist_foreach((entry->allocToEscapeMap), val, iter) {
        
		
		void** escape = (void**) val;
        sint64_t offset = doesItAlias(entry->pointer, entry->length, (uint64_t) *escape);
        if(offset >= 0) {
            *escape = (void*) ((sint64_t) allocation_target + offset);
        }


    }

    return 0;
}


// DEPRECATED
#if 0
void _carat_update_entry(allocation_entry *old_entry, void *allocation_target) 
{
    /*
     * Create a new entry for @allocation_target
     */ 
    CREATE_ENTRY_AND_ADD (
        allocation_target,
        (old_entry->size),
        "carat_update_entry: CREATE_ENTRY_AND_ADD failed on allocation_target"
    );


    /*
     * Set the new entry's escapes set to @old_entry's escapes set
     * NOTE --- "new_entry" is a variable generated from CREATE_ENTRY_AND_ADD
     */
    new_entry->escapes_set = old_entry->escapes_set;


	/*
     * Remove the address corresponding to @old_entry from the global
     * allocation map
     */ 
    REMOVE_ENTRY (
        (old_entry->pointer),
        "carat_update_entry: REMOVE_ENTRY failed on old_entry->pointer"
    )


    return 0;
}
#endif


// Possibly * DEPRECATED * --- replaced by proper placement of 
// instrumented calls to the runtime
int _carat_update_entry(allocation_entry *entry, void *allocation_target) {

    // Create a new entry
    allocation_entry *newEntry = allocation_entrySetup(allocation_target, entry->length);
    newEntry->allocToEscapeMap = entry->allocToEscapeMap;
    
	// CONV [map::insert_or_assign] -> [nk_map_insert + status check]
	if (!(nk_map_insert(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) allocation_target), ((uintptr_t) newEntry)))) {
		panic("carat_update_entry: nk_map_insert failed on allocation_target %p\n", allocation_target);
	}

	// Remove the old pointer from the map	
    nk_map_remove(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) (entry->pointer)));

    return 0;
}

// Debugging
static void __carat_print_regs (struct nk_regs * r)
{
    #define PRINT3(x, x_extra_space, y, y_extra_space, z, z_extra_space) DS(#x x_extra_space" = " ); DHQ(r->x); DS(", " #y y_extra_space" = " ); DHQ(r->y); DS(", " #z z_extra_space" = " ); DHQ(r->z); DS("\n");

    DS("[-------------- Register Contents --------------]\n");
    
    PRINT3(rip, "   ",  rflags, "", rbp, "   ");
    PRINT3(rsp, "   ",  rax, "   ", rbx, "   ");
    PRINT3(rsi, "   ",  r8, "    ", r9, "    ");
    PRINT3(r10, "   ",  r11, "   ", r12, "   ");
    PRINT3(r13, "   ",  r14, "   ", r15, "   ");

    return;
}



#if 0
// Patch shared state in the threads, registers manually
static void _carat_patch_thread_state(struct nk_thread *t, struct move_alloc_state *state) 
{
    /*
     * Acquire all registers
     */ 
    struct nk_regs * r = ((struct nk_regs*) ((char*)t->rsp - 128)); // FIX: t->rsp - 128 might be wrong, look at garbage collector

    /*
     * Debugging, sanity check for t->rsp - 128
     */  
    CARAT_PRINT(t->name); // should be shell
    __carat_print_regs(r); // r15 should have 0xDEADBEEFB000B000


    /*
     * Determine if the value stored in the register 
     * aliases @allocation_to_move --- if it does, then
     * "patch" the value to use @allocation_target at the
     * correct offset
     * 
     * Macro-ized for engineering purposes
     */ 

#define REG_PTR(reg) ((void *) (r->reg))
#define HANDLE_REG(state, reg) \
    void *ptr_##reg = REG_PTR(reg); \
    uint64_t offset_##reg = _carat_get_query_offset ( \
        ptr_##reg, \
        (state->allocation_to_move), \
        (state->size) \
    ); \
    \
    if (offset_##reg > 0) { r->reg = (((uint64_t) state->allocation_target) + offset); }

    HANDLE_REG(state, r15)
	HANDLE_REG(state, r14)
    HANDLE_REG(state, r13)
    HANDLE_REG(state, r12)
    HANDLE_REG(state, r11)
    HANDLE_REG(state, r10)
    HANDLE_REG(state, r9)
    HANDLE_REG(state, r8)
    HANDLE_REG(state, rbp)
    HANDLE_REG(state, rdi)
    HANDLE_REG(state, rsi)
    HANDLE_REG(state, rdx)
    HANDLE_REG(state, rcx)
    HANDLE_REG(state, rbx)
    HANDLE_REG(state, rax)
	// TODO --- handle rsp and rip later


    return;
}
#endif



// Patch shared state in the threads, registers manually
static void handle_thread(struct nk_thread *t, void *state) {
    struct move_alloc_state *move_state = (struct move_alloc_state*) state;
    struct nk_regs * r = (struct nk_regs*)((char*)t->rsp - 128); // FIX: t->rsp - 128 might be wrong, look at garbage collector

    // Sanity check for t->rsp - 128
    nk_vc_printf(t->name); // should be shell
    __carat_print_regs(r); // r15 should have 0xDEADBEEFB000B000


// moved these comments out of our HANDLE_REG
// if the register is within our alloc
// DEBUG("It aliased %p will now become %p which is offset %ld\n", registerPtr.ptr, newAllocPtr.ptr, offset);
// DEBUG("The register %s is now %p\n", regNames[i], (void*) uc->uc_mcontext.gregs[i]);
#define HANDLE_REG(state, reg) \
    if((r->reg >= (uint64_t) state->allocation_to_move) && (r->reg < ((uint64_t) state->allocation_to_move + state->length))){ \
            uint64_t offset = r->reg - (uint64_t) state->allocation_to_move; \
            uint64_t newAddr = (uint64_t) state->allocation_target + offset; \
            r->reg = newAddr; \
    } \

    HANDLE_REG(state, r15)
	HANDLE_REG(state, r14)
    HANDLE_REG(state, r13)
    HANDLE_REG(state, r12)
    HANDLE_REG(state, r11)
    HANDLE_REG(state, r10)
    HANDLE_REG(state, r9)
    HANDLE_REG(state, r8)
    HANDLE_REG(state, rbp)
    HANDLE_REG(state, rdi)
    HANDLE_REG(state, rsi)
    HANDLE_REG(state, rdx)
    HANDLE_REG(state, rcx)
    HANDLE_REG(state, rbx)
    HANDLE_REG(state, rax)
	// handle rsp and rip later

}


int nk_carat_move_allocation(void *allocation_to_move, void *allocation_target) 
{

	// Oaklahoma will indeed reopen if the damn 
	// world can't be stopped
	if (!(nk_sched_stop_world())) {
        CARAT_PRINT("Oaklahoma has reopened!\n");
        return -1;
    }


	// Find the entry for the allocation_to_move addr in the 
	// allocationMap --- if not found, return immediately
    allocation_entry* entry = findAllocEntry(allocation_to_move);
    if (!entry) {
        CARAT_PRINT("Cannot find entry\n");
        goto out_bad;
    }

	
    // Generate what patches need to be executed for overwriting 
	// memory, then executes patches for escapes to allocation_target
    if (carat_patch_escapes(entry, allocation_target) == 1) {
        CARAT_PRINT("The patch is toast --- can't patch escapes !\n");
        goto out_bad;
    }


    // For each thread, patch registers to the allocation_target --- 
	// takes a "state," which is just a packaged structure containing 
	// necessary info to perform the patching --- like to and from 
	// addrs, length, etc.
    struct move_alloc_state state = {allocation_to_move, allocation_target, entry->length, 0};
    nk_sched_map_threads(-1, handle_thread, &state);
    if (state.failed) {
        CARAT_PRINT("Unable to patch threads\n");
        goto out_bad;
    }
   
	// Perform the real memory movement (the "actual patch") --- move
	// the memory at allocation_to_move to allocation_target 
    memmove(allocation_to_move, allocation_target, entry->length);
   

   	// DEPRECATED
    /* carat_update_entry(entry, allocation_target); */

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



// Top-level interface for patching ---
// - allocation_to_move --- address to move/needs patching
// - allocation_target --- address to move to/patch to
int nk_carat_move_allocation(void *allocation_to_move, void *allocation_target) {

	// Oaklahoma will indeed reopen if the damn 
	// world can't be stopped
	if (!(nk_sched_stop_world())) {
        CARAT_PRINT("Oaklahoma has reopened!\n");
        return -1;
    }


	// Find the entry for the allocation_to_move addr in the 
	// allocationMap --- if not found, return immediately
    allocation_entry* entry = findAllocEntry(allocation_to_move);
    if (!entry) {
        CARAT_PRINT("Cannot find entry\n");
        goto out_bad;
    }

	
    // Generate what patches need to be executed for overwriting 
	// memory, then executes patches for escapes to allocation_target
    if (carat_patch_escapes(entry, allocation_target) == 1) {
        CARAT_PRINT("The patch is toast --- can't patch escapes !\n");
        goto out_bad;
    }


    // For each thread, patch registers to the allocation_target --- 
	// takes a "state," which is just a packaged structure containing 
	// necessary info to perform the patching --- like to and from 
	// addrs, length, etc.
    struct move_alloc_state state = {allocation_to_move, allocation_target, entry->length, 0};
    nk_sched_map_threads(-1, handle_thread, &state);
    if (state.failed) {
        CARAT_PRINT("Unable to patch threads\n");
        goto out_bad;
    }
   
	// Perform the real memory movement (the "actual patch") --- move
	// the memory at allocation_to_move to allocation_target 
    memmove(allocation_to_move, allocation_target, entry->length);
   

   	// DEPRECATED
    /* carat_update_entry(entry, allocation_target); */

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

// Utility
allocation_entry *findRandomAlloc() {

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
		if (count == target) { return ((allocation_entry *) (pair->second)); }
		count++;
	}

    return 0;
}

// Shell command handler
static int handle_carat_test(char *buf, void *priv)
{
    uint64_t count;
    uint64_t i;
    if(sscanf(buf, "carat_test %lu", &count) == 1) {

        nk_vc_printf("Moving %lu times.\n", count);

		// count --- peform "count" number of sequential
		// moves
        for(i = 0; i < count; i++) {

			// Find a random entry from the allocation table
            allocation_entry* entry = findRandomAlloc();
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

static struct shell_cmd_impl carat_test_impl = {
    .cmd = "carat_test",
    .help_str = "carat_test",
    .handler = handle_carat_test,
};

nk_register_shell_cmd(carat_test_impl);


/* ---------- ALLOCATION MAP DEBUGGING ---------- */

// Handler --- printing the allocationMap
static int handle_print_table() {

	// Table formatting:
	nk_vc_printf("map node addr  :  ( real addr  :  allocation_entry addr  ---  ( pointer,  len ))\n"); 

	// Loop through the allocationMap, print all 
	// information necessary
    nk_slist_node_uintptr_t_uintptr_t *iter;
    nk_pair_uintptr_t_uintptr_t *pair;
    nk_map_foreach(allocationMap, pair, iter) {        
        allocation_entry *the_entry = (allocation_entry *) (pair->second);
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

