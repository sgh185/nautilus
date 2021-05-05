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

/*
 * Handler for patching escapes upon moving @entry->pointer to @allocation_target 
 */ 
NO_CARAT_NO_INLINE
int _carat_patch_escapes(
    allocation_entry *entry, 
    void *allocation_target
) 
{
    /*
     * TOP --- Iterate over each escape, and change what each escape  
     * points to --- i.e. "patch" the escape
     */ 

    /*
     * Iterate over the escapes of @entry --- NOTE --- the iteration 
     * macro provides a variable "val," indicating the current element 
     */ 
    if(entry->escapes_set == NULL) {
        return 0;
    }

      
    CARAT_ESCAPES_SET_ITERATE((entry->escapes_set))
    {
        /*
         * Set up the escape
         */ 
		void **curr_escape = FETCH_ESCAPE_FROM_ITERATOR;
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


    return 1;
}


void _reinstrument_contained_escapes(allocation_entry *new_entry) {

    /*
     * Iterate over the contained_escapes of @new_entry --- NOTE --- the iteration 
     * macro provides a variable "val," indicating the current element 
     */ 
    if (new_entry->contained_escapes == NULL) {
        return;
    }


    CARAT_ESCAPES_SET_ITERATE((new_entry->contained_escapes))
    {
        /*
         * val is the offset
         * new_entry->pointer is the allocation location
         * add the allocation location and the offset and we get the location of the current contained_escape
         */ 
        uint64_t offset = ((uint64_t) FETCH_ESCAPE_FROM_ITERATOR); /* HACK */
		void *escape_location = (void *) (offset + ((uint64_t) new_entry->pointer));

        nk_carat_instrument_escapes(escape_location);
    }
    
}


allocation_entry *_carat_update_entry(nk_carat_context *the_context, allocation_entry *old_entry, void *allocation_target) 
{
    /*
     * Create a new entry for @allocation_target
     */ 
    CREATE_ENTRY_AND_ADD (
        the_context,
        allocation_target,
        (old_entry->size),
        "carat_update_entry: CREATE_ENTRY_AND_ADD failed on allocation_target"
    );


    /*
     * Set the new entry's escapes set to @old_entry's escapes set
     * NOTE --- "new_entry" is a variable generated from CREATE_ENTRY_AND_ADD
     */
    new_entry->escapes_set = old_entry->escapes_set;
    new_entry->contained_escapes = old_entry->contained_escapes;


	/*
     * Remove the address corresponding to @old_entry from the global
     * allocation map
     */ 
    REMOVE_ENTRY (
        the_context,
        (old_entry->pointer),
        "carat_update_entry: REMOVE_ENTRY failed on old_entry->pointer"
    );


    return new_entry;
}


/*
 * Debugging
 */ 
NO_CARAT
static void __carat_print_registers (struct nk_regs * r)
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


/*
 * Search through all registers in a thread and patch 
 * any of them that refer to the allocation being moved
 */ 
NO_CARAT
static void _carat_patch_thread_registers(struct nk_thread *t, void *state) 
{
    /*
     * Cast to struct move_context
     */ 
    struct move_context *context = (struct move_context*) state;


    /*
     * Acquire all registers
     */ 
    struct nk_regs * r = ((struct nk_regs*) ((char*)t->rsp - 128)); // FIX: t->rsp - 128 might be wrong, look at garbage collector


    /*
     * Debugging, sanity check for t->rsp - 128
     */  
    CARAT_PRINT(t->name); // should be shell
    __carat_print_registers(r);


    /*
     * Determine if the value stored in the register 
     * aliases @allocation_to_move --- if it does, then
     * "patch" the value to use @allocation_target at the
     * correct offset
     * 
     * Macro-ized for engineering purposes
     */ 

#define REG_PTR(reg) ((void *) (r->reg))
#define HANDLE_REG(context, reg) \
    void *ptr_##reg = REG_PTR(reg); \
    uint64_t offset_##reg = _carat_get_query_offset ( \
        ptr_##reg, \
        (context->allocation_to_move), \
        (context->size) \
    ); \
    \
    if (offset_##reg != -1) { r->reg = (((uint64_t) context->allocation_target) + offset_##reg); }

    HANDLE_REG(context, r15)
	HANDLE_REG(context, r14)
    HANDLE_REG(context, r13)
    HANDLE_REG(context, r12)
    HANDLE_REG(context, r11)
    HANDLE_REG(context, r10)
    HANDLE_REG(context, r9)
    HANDLE_REG(context, r8)
    HANDLE_REG(context, rbp)
    HANDLE_REG(context, rdi)
    HANDLE_REG(context, rsi)
    HANDLE_REG(context, rdx)
    HANDLE_REG(context, rcx)
    HANDLE_REG(context, rbx)
    HANDLE_REG(context, rax)
	// TODO --- handle rip later. rsp is handled by _carat_patch_thread_stack


    return;
}


/*
 * Search through all addresses in the thread's stack 
 * and patch any of them that refer to the allocation being moved
 */ 
NO_CARAT
static void _carat_patch_thread_stack(struct nk_thread *t, void *state) 
{
    /*
     * Cast to struct move_context
     */ 
    struct move_context *context = (struct move_context*) state;

    uint64_t *stack_start = t->stack;
    nk_stack_size_t stack_size = t->stack_size;
    uint64_t *stack_end = stack_start + stack_size;

    /*
     * Loop through every address in the stack
     */ 
    for (uint64_t *stack_addr = stack_start; stack_addr < stack_end; stack_addr += 8) {
        /*
         * Determine if the value stored in the stack location 
         * aliases @allocation_to_move --- if it does, then
         * "patch" the value to use @allocation_target at the
         * correct offset
         */ 
        uint64_t offset = _carat_get_query_offset((void*) *stack_addr, context->allocation_to_move, context->size);
        if (offset != -1) {
            *stack_addr = (((uint64_t) context->allocation_target) + offset);
        }
    }
}


/*
 * Patch the stack and the registers for a given thread.
 * This function is passed into and called by nk_sched_map_threads
 */ 
NO_CARAT
static void _carat_patch_thread_registers_and_stack(struct nk_thread *t, void *state) 
{
    _carat_patch_thread_registers(t, state);
    _carat_patch_thread_stack(t, state);
}


/*
 * Catches the runtime up before any move happens
 */
NO_CARAT
static void _carat_cleanup(nk_carat_context *the_context) 
{
    /*
     * Process the remaining escapes in @the_context->escape_window
     */ 
    CARAT_PRINT(
        "CARAT: processesing escape window of size %lu\n", 
        the_context->total_escape_entries
    );

    _carat_process_escape_window(the_context);


    /*
     * TODO: if we implement batch freeing, process frees here 
     */
    

    return;
}


NO_CARAT 
int _move_allocation(
    nk_carat_context *the_context,
    void *allocation_to_move,
    void *allocation_target
) 
{
    /*
     * Perform any leftover/cleanup processing for @the_context 
     * before moving anything related to @allocation_to_move
     */ 
    _carat_cleanup(the_context);


    /*
     * Find the entry for the @allocation_to_move addr in the 
     * allocation_map --- if not found, return -1
     */
    allocation_entry *entry = 
        _carat_find_allocation_entry(
            the_context,
            allocation_to_move
        );

    if (!entry) 
    {
        CARAT_PRINT("CARAT: Cannot find entry!\n");
        goto out_bad;
    }


	/*
     * Patch all of @allocation_to_move’s escapes (in @allocation_to_moves->escapes_set)
     * to use the address of @allocation_target
     */
    if (!_carat_patch_escapes(entry, allocation_target)) 
    {
        CARAT_PRINT("CARAT: The patch is toast --- can't patch escapes!\n");
        goto out_bad;
    }


    /*
     * For each thread, patch the registers and stack addresses
     * that currently contain references to @allocation_target. 
     * In order to patch each thread, all necessary info is packaged into a move_context struct
     */
    struct move_context package = { allocation_to_move, allocation_target, entry->size, 0 };
    nk_sched_map_threads(-1, _carat_patch_thread_registers_and_stack, &package);
    if (package.failed) 
    {
        CARAT_PRINT("CARAT: Unable to patch threads\n");
        goto out_bad;
    }

    /*
     * Update the allocation entry
     */
    allocation_entry *new_entry = _carat_update_entry(the_context, entry, allocation_target);


    /*
     * Move the contents pointed to by @allocation_to_move to the 
     * location @allocation_target (this is the "actual" move)
     */
    memmove(allocation_target, allocation_to_move, entry->size);

    /*
     * all of the escapes contained in our entry need to be resintrumented, 
     * as they are now new escapes
     */
    _reinstrument_contained_escapes(new_entry);

    /*
     * We need to process those resinstrumented escapes before we continue
     */
    _carat_process_escape_window(the_context);

    return 0;


out_bad:
    CARAT_PRINT(
        "_move_allocation: failed. allocation to move: %p, allocation target: %p", 
        allocation_to_move, 
        allocation_target
    );
    return -1;

}


NO_CARAT
int nk_carat_move_allocation(
    nk_carat_context *the_context,
    void *allocation_to_move, 
    void *allocation_target
)
{
    /*
     * Pauses all execution so we can perform a move
     */
	if (!(nk_sched_stop_world())) 
    {
        CARAT_PRINT("CARAT: nk_sched_stop_world failed\n");
        goto out_bad;
    }


    /*
     * Pause CARAT for the move
     */ 
    CARAT_READY_OFF(the_context);


    /*
     * Perform the move
     */ 
    int move_status = 
        _move_allocation(
            the_context,
            allocation_to_move,
            allocation_target
        );

    if (move_status) { goto out_bad; }


    /*
     * Turn everything back on
     */ 
    CARAT_READY_ON(the_context);
    nk_sched_start_world();


    CARAT_PRINT("CARAT: Move succeeded.\n");
    return 0;


out_bad:
    CARAT_PRINT(
        "nk_carat_move_allocation: faileed. allocation to move: %p, allocation target: %p", 
        allocation_to_move, 
        allocation_target
    );
    
    return -1;
}


NO_CARAT
int nk_carat_move_allocations(
    nk_carat_context *the_context,
    void **allocations_to_move, 
    void **allocation_targets, 
    uint64_t num_moves
) 
{
    /*
     * Pauses all execution so we can perform a series of move
     */
	if (!(nk_sched_stop_world())) 
    {
        CARAT_PRINT("CARAT: nk_sched_stop_world failed\n");
        goto out_bad;
    }


    /*
     * Now turn off CARAT to perform the moves
     */ 
    CARAT_READY_OFF(the_context);

    
    /*
     * Perform a series of moves
     */ 
    for (int i = 0; i < num_moves; i++) 
    {
        int next_move_status =
            _move_allocation(
                the_context,
                allocations_to_move[i],
                allocation_targets[i]
            );

        if (next_move_status) { goto out_bad; }
    }
    

    /*
     * Turn everything back on
     */ 
    CARAT_READY_ON(the_context);
    nk_sched_start_world();


    CARAT_PRINT("CARAT: Moves succeeded.\n");
    return 0;


out_bad:
    CARAT_PRINT("nk_carat_move_allocations: failed to move");
    return -1;
}


NO_CARAT
int nk_carat_move_region(
    nk_carat_context *the_context,
    void *region_start, 
    void *new_region_start, 
    uint64_t region_length, 
    void **free_start
) 
{ 
    /*
     * Debugging
     */ 
    CARAT_PRINT("CARAT: move_region (%p,%lu) -> %p\n", region_start, region_length, new_region_start);
   
    /*
     * create an array to hold all the addresses we want to move
     * Note - this will most likely end up being larger than it needs to be, as we only want 
     * to move a subset of the allocations in the map, but this conservative solution will work
     */
    uint64_t size_of_map = CARAT_ALLOCATION_MAP_SIZE(the_context);
    void* old_addresses[size_of_map];
    uint64_t old_lengths[size_of_map];

    /*
     * Pauses all execution so we can perform a series of move
     */
	if (!(nk_sched_stop_world())) 
    {
        CARAT_PRINT("CARAT: nk_sched_stop_world failed\n");
        goto out_bad;
    }

    CARAT_READY_OFF(the_context);

    
    /*
     * Iterate through the allocation map, searching for allocations within the region
     */ 
    
    uint64_t count = 0;
    CARAT_ALLOCATION_MAP_ITERATE(the_context) // TODO: this is inserting as it is iterating. This is a problem...
    {        
        /*
         * Fetch [key : val]
         */ 
        allocation_entry *the_entry = FETCH_ALLOCATION_ENTRY_FROM_ITERATOR;
        void *the_address = the_entry->pointer;


        /*
         * If the current allocation is within the old region, 
         * move it to the next available spot in the new region  
         */ 
        if (true
            && (the_address >= region_start) 
            && (the_address < (region_start + region_length))) 
        {
            old_addresses[count] = the_address;
            old_lengths[count] = the_entry->size;
            count++;
            
        }
    }

    void *current_destination = new_region_start;
    for (uint64_t i = 0; i < count; i++) {
        if (_move_allocation(the_context, old_addresses[i], current_destination)) {
            goto out_bad;
        };
        //free(old_addresses[i]); TODO maybe do this
        current_destination += old_lengths[i];
    }


    /*
     * We did the damn thing, turn everything on
     */ 
    CARAT_READY_ON(the_context);
    nk_sched_start_world();
    CARAT_PRINT("CARAT: Region move succeeded.\n");
    
    
    /*
     * Return the pointer to the end of the allocations in the new region
     */ 
    *free_start = current_destination;

    return 0;


out_bad:
    CARAT_PRINT("nk_carat_move_region: failed to move");
    return -1;
}

NO_CARAT
int nk_carat_defrag_allocation_table(nk_carat_context *the_context) 
{ 
    /*
     * Debugging
     */ 
    CARAT_PRINT("CARAT: defrag_allocation_table\n");

    /*
     * create an array to hold all the addresses we want to move
     */
    uint64_t size_of_map = CARAT_ALLOCATION_MAP_SIZE(the_context);
    void* old_addresses[size_of_map];
    uint64_t old_lengths[size_of_map];
   

    /*
     * Pauses all execution so we can perform a series of moves
     */
	if (!(nk_sched_stop_world())) 
    {
        CARAT_PRINT("CARAT: nk_sched_stop_world failed\n");
        goto out_bad;
    }

    CARAT_READY_OFF(the_context);

    
    /*
     * Iterate through the allocation map, searching for all allocations 
     */ 
    uint64_t count = 0;
    CARAT_ALLOCATION_MAP_ITERATE(the_context)
    {        
        /*
         * Fetch the address
         */ 
        allocation_entry *the_entry = FETCH_ALLOCATION_ENTRY_FROM_ITERATOR;
        void *the_address = the_entry->pointer;

        old_addresses[count] = the_address;
        old_lengths[count] = the_entry->size;
        count++;
    }

    for (uint64_t i = 0; i < size_of_map; i++) {
        // NOTE - CARAT is off
        void* new_location = malloc(old_lengths[i]);
        _move_allocation(the_context, old_addresses[i], new_location);
        free(old_addresses[i]);
    }

    /*
     * We did the damn thing, turn everything on
     */ 
    CARAT_READY_ON(the_context);
    nk_sched_start_world();
    CARAT_PRINT("CARAT: Region move succeeded.\n");

    return 0;


out_bad:
    CARAT_PRINT("defrag_allocation_table: failed to move");
    return -1;
}



/* ---------- ALLOCATION MAP DEBUGGING ---------- */


/*
 * "Modularism" --- Handle printing of allocation tables
 */ 
static void _print_table(nk_carat_context *the_context)
{
	/*
     * Set up table formatting 
     */
	nk_vc_printf("map node addr  :  ( real addr  :  allocation_entry addr  ---  ( pointer,  len ))\n"); 


    /*
     * Loop through @the_context's allocation map
     */ 
    CARAT_ALLOCATION_MAP_ITERATE(the_context)
    {        
        allocation_entry *the_entry = FETCH_ALLOCATION_ENTRY_FROM_ITERATOR;
        uint64_t num_escapes = 0;
        if (the_entry->escapes_set != NULL) {
            num_escapes = CARAT_ESCAPE_SET_SIZE(the_entry->escapes_set);
        }
		nk_vc_printf(
            "%p : (%p : %p --- (ptr: %p, len: %lu), es : %d)\n", 
			iterator, 
			the_entry->pointer, 
			the_entry, 
			the_entry->pointer, 
			the_entry->size,
            num_escapes
        );
    }
    
    nk_vc_printf(
        "Number of escapes in escape window: %lu\n", 
        FETCH_TOTAL_ESCAPES(the_context)
    );


    return;
}


NO_CARAT
static int handle_print_table(char *buf, void *priv) 
{
    /*
     * Fetch the current context --- a HACK
     */
    nk_carat_context *the_context = FETCH_CARAT_CONTEXT;


    /*
     * Print the table for the current aspace
     */     
    _print_table(the_context);
    

    return 0;
}


static struct shell_cmd_impl print_table_impl = {
    .cmd = "print_table",
    .help_str = "print_table",
    .handler = handle_print_table,
};

nk_register_shell_cmd(print_table_impl);



/* --------- TEST CASE ---------  */

/* 
* Test --- perform a random move:
* - Pick a random allocation from the allocationMap
* - Allocate new memory of the same size as the randomly
*   picked allocation --- this will be the new destination
*   for the move
* - call nk_carat_move_allocation on the old and new 
*   memory addresses
* 
* Test can execute an arbitrary number of sequential
* moves based on user input 
*/

/* 
 * Utility
 */

static uint64_t carat_seed = 29848349;

NO_CARAT
allocation_entry *_carat_find_random_alloc(nk_carat_context *the_context) 
{
    /*
     * Setup --- We have to ignore allocations that aren't 
     * tracked by CARAT. In particular, lots of addresses used
     * upon booting exist below the "data_end" address, and are
     * not tracked --- only pick addresses above "data_end"
     */ 
    extern char *_data_end;
    addr_t min_addr = (addr_t) &_data_end; /* Pick any allocation above this */
    nk_vc_printf("min_addr: %p\n", min_addr);


	/* 
     * Select a random index from the map  
     */
    uint64_t target = (rdtsc() % CARAT_ALLOCATION_MAP_SIZE(the_context)),
             count = 0;

    /* 	
     * Find the allocation at index "target" in the allocation_map
     * The iteration macro produces a variable "pair" that represents the current object
     * pair — [address : allocation_entry object] 
     */
    CARAT_ALLOCATION_MAP_ITERATE(the_context)
    {
        if (false
            || (FETCH_ALLOCATION_ENTRY_FROM_ITERATOR->size != 24656)
            || !(((addr_t) FETCH_ALLOCATION_ENTRY_FROM_ITERATOR->pointer) > min_addr)) { continue; }
#if 0
        if ((count >= target) 
        //&& (((addr_t) CARAT_ALLOCATION_MAP_CURRENT_ADDRESS) > min_addr)
        //&& (nk_slist_is_right_sentinal(CARAT_ALLOCATION_MAP_CURRENT_ADDRESS)) 
        ){ return CARAT_ALLOCATION_MAP_CURRENT_ENTRY; }
        count++;
#endif
        return FETCH_ALLOCATION_ENTRY_FROM_ITERATOR;
    }
     

    return NULL;
}


/* 
 * Shell command handler
 */
static int handle_carat_test(char *buf, void *priv)
{
    uint64_t count, i;
    if (sscanf(buf, "carat_test %lu", &count) == 1) 
    {
        nk_vc_printf("Moving %lu times.\n", count);


        /*
         * Fetch the current CARAT context
         */ 
        nk_carat_context *the_context = FETCH_CARAT_CONTEXT;


        /* 		
         * Perform "count" number of sequential moves 
         */
        for (i = 0; i < count; i++) 
        {
            /* 		
             * Find a random entry from the allocation table
             */
            allocation_entry* entry = _carat_find_random_alloc(the_context);
            if(!entry) {
                nk_vc_printf("CARAT: Random entry not found.\n");
                return -1;
            }


            /* 			
             * Set "old" and "new" --- the new memory allocation
			 * is based on the size of the randomly selected
			 * allocation from the allocation_map 
             */
            void *old = entry->pointer;
            uint64_t size = entry->size;


			/* 
             * ***NOTE*** --- this malloc *must not* be instrumented by the compiler 
             */
            CARAT_READY_OFF(the_context);
            nk_vc_printf("Attempting to move address %p, size %lu\n", old, size);
           	void *new = CARAT_MALLOC(size);
            nk_vc_printf("Attempting to move from %p to %p, size %lu\n", old, new, size);
           

		   	/* 
             * Perform move from "old" to "new"
             * nk_carat_move_allocation does all checks and returns -1 if any fail 
             */
            nk_vc_printf("%d ", i);
            if(nk_carat_move_allocation(the_context, old, new)) {
                panic("carat_test: nk_carat_move_allocation failed!");
            }


            /*
             * Debugging
             */ 
            _print_table(the_context);
			
        
            /* 
             * ***NOTE*** --- this free *must not* be instrumented by
			 * the compiler 
             */
            free(old);
            CARAT_READY_ON(the_context);
            nk_vc_printf("Free succeeded.\n");
        }

    } else {
        nk_vc_printf("Invalid Command.");
    }

    return 0;
}


/* 
 * Shell command handler
 */
static int handle_kernel_defrag_test(char *buf, void *priv)
{
    /*
     * Fetch the current CARAT context
     */ 
    nk_carat_context *the_context = FETCH_CARAT_CONTEXT;

    /*
     * "Defrag" the allocation table
     */ 
    nk_carat_defrag_allocation_table(the_context);
    return 0;
}

static struct shell_cmd_impl carat_test_impl = {
    .cmd = "carat_test",
    .help_str = "carat_test",
    .handler = handle_carat_test,
};

nk_register_shell_cmd(carat_test_impl);

static struct shell_cmd_impl kernel_defrag_test_impl = {
    .cmd = "kernel_defrag_test",
    .help_str = "kernel_defrag_test",
    .handler = handle_kernel_defrag_test,
};

nk_register_shell_cmd(kernel_defrag_test_impl);
