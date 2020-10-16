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

#include <aspace/runtime_tables.h>


/*
 * =================== Setting Necessary Global State ===================
 */ 


/*
 * Set ready flag --- no need to set anything else before nk_carat_init
 */ 
carat_context global_carat_context = {
	.carat_ready = 0
};


/*
 * Setup for allocations
 */ 
NO_CARAT
allocation_entry *_carat_create_allocation_entry(void *address, uint64_t allocation_size)
{
	/*
	 * Allocate an allocation_entry object
	 */
	allocation_entry *new_entry = (allocation_entry *) CARAT_MALLOC(sizeof(allocation_entry));


	/*
	 * Save the address (@address) and the size (@allocation_size), 
	 * build a new escapes map for the new allocation_entry object
	 */ 
	new_entry->pointer = address;
	new_entry->size = allocation_size;
	new_entry->escapes_set = CARAT_ESCAPE_SET_BUILD; 


	/*
	 * Return the new allocation_entry object with saved state
	 */
	return new_entry;
}



/*
 * =================== Utility Analysis Methods ===================
 */ 

/*
 * Analysis of address aliasing
 */ 
NO_CARAT
int _carat_does_alias(void *query_address, void *alloc_address, uint64_t allocation_size)
{
	/*
	 * TOP --- Treat @alloc_address and @query_address as integers to perform 
	 * a "range search" within @alloc_address' allocation size --- want
	 * to see if @query_address is within this "range"
	 */ 

	/*
	 * Set the ranges
	 */ 
	uint64_t allocation_range_start = ((uint64_t) alloc_address),
			 allocation_range_end = allocation_range_start + allocation_size,
			 query_address_int = ((uint64_t) query_address);


	/*
	 * Perform the range check and return the value
	 */ 
	int within_range = (true
						&& (query_address_int >= allocation_range_start) 
						&& (query_address_int < allocation_range_end));

	return within_range;
}


NO_CARAT
sint64_t _carat_get_query_offset(void *query_address, void *alloc_address, uint64_t allocation_size)
{	
	/*
	 * Check if @query_address aliases @alloc_address:
	 * - TRUE = return the offset (query_address - allocation_range_start)
	 * - FALSE = -1
	 */ 
	uint64_t allocation_range_start = ((uint64_t) alloc_address),
			 query_address_int = ((uint64_t) query_address); // Free computation

	sint64_t the_offset = 
		(_carat_does_alias(query_address, alloc_address, allocation_size)) ?
		(query_address_int - allocation_range_start) : 
		(-1);

	return the_offset;
}


/*
 * Find a corresponding allocation_entry object for @address
 */ 
NO_CARAT
allocation_entry * _carat_find_allocation_entry(void *address)
{
	// CONV [brian] -> [better than brian] 	
	/*
	 * Find a prospective object (skiplist node) in the allocation map 
	 * based on @address --- using "better_lower_bound"
	 * 
	 * "better_lower_bound" returns the node containing the
	 * address that is the closest to @address AND <= @address
	 */
	__auto_type *lower_bound_node = CARAT_ALLOCATION_MAP_BETTER_LOWER_BOUND(address);


	/* 
	 * Possible exists --- [prospective] -> [_carat_find_allocation_entry return]:
	 * 1) the left sentinal (nk_slist_node, see below) -> NULL
	 * 2) an allocation_entry that does contain the address -> allocation_entry *
	 * 3) a allocation_entry that does NOT contain the address -> NULL
	 */ 

	// skiplist node -> data -> second == allocation_entry object from [address : allocation_entry *]
	allocation_entry *prospective_entry = ((allocation_entry *) lower_bound_node->data->second);

	/*
	 * CONDITION 1 --- better_lower_bound may return the left sential if  
	 * the address is lower than all addresses that exist in the skip list 
	 * (edge case)
	 * 
	 * Handle with a check against the left sentinal
	 */
	if (!prospective_entry) { return NULL; }
	

	/*
	 * CONDITION 2 --- gather the prospective entry's address, the size of
	 * the prospective's allocation, and CHECK if @address aliases this allocation
	 */ 
	uint64_t prospective_size = prospective_entry->size; 
	void *prospective_address = ((void *) lower_bound_node->data->first);
	if (!(_carat_does_alias(address, prospective_address, prospective_size))) { return NULL; }


	/*
	 * "prospective_entry" is the correct allocation_entry object! Return it
	 */ 
	return prospective_entry;
}


/*
 * Self explanatory stats for CARAT
 */ 
NO_CARAT
void nk_carat_report_statistics()
{
	/*
	 * Report size of allocation map
	 * 
	 * TODO --- add more statistics
	 */ 
	CARAT_PRINT("Size of Allocation Table: %lu\n", CARAT_ALLOCATION_MAP_SIZE); 
	return;
}


/*
 * =================== Allocations Handling Methods ===================
 */ 
NO_CARAT_NO_INLINE
void nk_carat_instrument_malloc(void *address, uint64_t allocation_size)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY


	/*
	 * Create an entry and add the mapping to the allocationMap
	 */ 
	CREATE_ENTRY_AND_ADD (
		address, 
		allocation_size,
		"nk_carat_instrument_malloc: CARAT_ALLOCATION_MAP_INSERT failed on address"
	);


	return;
}


NO_CARAT_NO_INLINE
void nk_carat_instrument_calloc(void *address, uint64_t num_elements, uint64_t size_of_element)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY

	/*
	 * Create a new allocation_entry object for the @address to be added --- here,
	 * the size has to be calculated from the parameters of calloc, which
	 * are @num_elements, and @size_of_element
	 */ 
	uint64_t allocation_size = num_elements * size_of_element;
	CREATE_ENTRY_AND_ADD (
		address, 
		allocation_size,
		"nk_carat_instrument_calloc: CARAT_ALLOCATION_MAP_INSERT failed on address"
	);


	return;
}


NO_CARAT_NO_INLINE
void nk_carat_instrument_realloc(void *old_address, void *new_address, uint64_t allocation_size)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY


	/*
	 * Remove @old_address from the allocation map --- need to remove the 
	 * corresponding allocation_entry object because of realloc's resizing
	 */ 
	REMOVE_ENTRY (
		old_address,
		"nk_carat_instrument_realloc: REMOVE_ENTRY failed on address"
	);


	/*
	 * Create an entry and add the mapping to the allocationMap
	 */ 
	CREATE_ENTRY_AND_ADD (
		new_address, 
		allocation_size,
		"HandleReallocToAllocationTable: CARAT_ALLOCATION_MAP_INSERT failed on address"
	);

	
	return;
}


NO_CARAT_NO_INLINE
void nk_carat_instrument_free(void *address)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * free before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY


	/*
	 * Remove @address from the allocation map
	 */ 
	DS("RE: ");
	DHQ(((uint64_t) address));
	DS("\n");
	
	REMOVE_ENTRY (
		address,
		"nk_carat_instrument_free: REMOVE_ENTRY failed on address"
	);


	return;
}


/*
 * =================== Escapes Handling Methods ===================
 */ 
/*
* nk_carat_instrument_escapes gets called when a pointer is stored in memory, or escaped
* @new_destination_of_escaping_address is the place we store our pointer that we now need to track
* In case that pointer ever gets moved, we need to keep track of everywhere that it is stored so 
* we can patch all the escapes
*/ 
NO_CARAT_NO_INLINE
void nk_carat_instrument_escapes(void *new_destination_of_escaping_address)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY

	
	/*
	 * Escapes are processed using batch processing --- if the escapeWindow
	 * is completely filled --- we need to process it first
	 */ 
	uint64_t num_entries = global_carat_context.total_escape_entries;
	// todo: disable CARAT before and after this call
	if (num_entries >= ESCAPE_WINDOW_SIZE) { _carat_process_escape_window(); }


	/*
	 * Add the escape to the end of the escapeWindow --- this will be
	 * processed at some point in batch, update the counter
	 */ 
	global_carat_context.escape_window[num_entries] = ((void **) new_destination_of_escaping_address);
	global_carat_context.total_escape_entries++;
	// DS("ES: ");
	// DHQ((global_carat_context.total_escape_entries));
	// DS("\n");

	return;
}


NO_CARAT_NO_INLINE
void _carat_process_escape_window()
{	
	/*
	 * TOP --- perform batch processing of escapes in the escape window
	 */ 
	DS("CARAT: pew\n");
	uint64_t num_entries = global_carat_context.total_escape_entries;
	void ***the_escape_window = global_carat_context.escape_window;

	/*
	 * Build a set of escapes that are already processed --- if we encounter
	 * another escape that is part of this set, the batch processing will
	 * ignore it
	 */  
	nk_carat_escape_set *processed_escapes = CARAT_ESCAPE_SET_BUILD;


	/*
	 * Iterate through each escape, process it
	 */ 
	uint64_t missed_escapes_counter = 0;
	for (uint64_t i = 0; i < num_entries; i++)
	{
		/*
		 * Get the next escape
		 */ 
		void **escape_address = the_escape_window[i];


		/*
		 * Perform conditions checks on the escape:
		 * 1. Check if it's a valid pointer
		 * 2. Check if it's already been processed
		 * 3. Check if it points to an allocation (allocation_entry object)
		 * 
		 * TRUE == mark as processed, continue
		 * FALSE == process fully, continue
		 * 
		 * NOTE --- Condition 2 and "marking as processed" can be completed 
		 * in the same step because of the design of nk_slist_add --- which
		 * returns a status flag if an addition to the skiplist has actually 
		 * occurred
		 */ 
		allocation_entry *corresponding_entry = NULL;

		if (false 
			|| (!escape_address) /* Condition 1 */
			|| (!(CARAT_ESCAPE_SET_ADD(processed_escapes, escape_address))) /* Condition 2, marking */
			|| (!(corresponding_entry = _carat_find_allocation_entry(*escape_address)))) /* Condition 3 */
			{ 
				missed_escapes_counter++;
				continue; }

		
		/*
		 * At this point --- we have found a corresponding entry where the 
		 * current escape should be recorded --- save this state to the 
		 * corresponding entry and continue
		 */  
		CARAT_ESCAPE_SET_ADD((corresponding_entry->escapes_set), escape_address);
	}
	DS("me: ");
	DHQ(missed_escapes_counter);
	DS("\n");
	/*
	 * Reset the global escapes counter
	 */ 
	global_carat_context.total_escape_entries = 0;


	return;
}


/*
 * =================== Initilization Methods ===================
 */ 

/*
 * Utility to get %rsp
 */ 
NO_CARAT
uint64_t _carat_get_rsp()
{
	uint64_t rsp;
	__asm__ __volatile__("movq %%rsp, %0" : "=a"(rsp) : : "memory");
	return rsp;
}


/*
 * Main driver for initialization
 */ 
NO_CARAT
void nk_carat_init()
{
	/*
	 * Stash %rsp for later initialization
	 */ 
	uint64_t rsp = _carat_get_rsp(); // Set global
	 

	/*
	 * Set up global allocation map
	 */ 
	global_carat_context.allocation_map = CARAT_ALLOCATION_MAP_BUILD;

	
	/*
	 * Block 32 GB worth of "stack space" --- to "handle" the stack --- this
	 * is a hack/precursor for tracking stack allocations
	 * 
	 * Add the stack and its allocation_entry object to the allocation map
	 */ 
	uint64_t allocation_size = THIRTY_TWO_GB;
	void *rsp_as_void_ptr = ((void *)(rsp - allocation_size));
 
	CREATE_ENTRY_AND_ADD (
		rsp_as_void_ptr,
		allocation_size,
		"CARATInit: nk_map_insert failed on rsp_as_void_ptr"
	);


	/*
	 * Set of escape window and its corresponding statistics/counters
	 */ 
	global_carat_context.total_escape_entries = 0;
	global_carat_context.escape_window = ((void ***) CARAT_MALLOC(ESCAPE_WINDOW_SIZE * sizeof(void *)));


	/*
	 * CARAT is ready --- set the flag
	 */ 
	CARAT_READY_ON;


	return;
}

