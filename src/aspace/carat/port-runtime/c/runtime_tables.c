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

#include <aspace/runtime_tables.h> // CONV [.hpp] -> [.h]


/*
 * =================== Setting Global State ===================
 */ 

/*
 * Declare allocationMap
 */ 
nk_carat_allocation_map *allocationMap = NULL; // FIX (for pairs, maps)


/*
 * Set up globals for init()
 */ 
int carat_ready = 0;
allocEntry* StackEntry;
uint64_t rsp = 0;


/*
 * Set up escapeWindow and statistics
 */ 
uint64_t escapeWindowSize = 0;
uint64_t totalEscapeEntries = 0;
void*** escapeWindow = NULL; // CONV [nullptr] -> [NULL]


/*
 * Setup for allocations
 */ 
allocEntry *allocEntrySetup(void *ptr, uint64_t len) // CONV [class constructor] -> [function that returns an instance]
{
	/*
	 * Allocate an allocEntry object
	 */
	allocEntry * newAllocEntry = (allocEntry *) CARAT_MALLOC(sizeof(allocEntry));


	/*
	 * Save the address (@ptr) and the size (@len), build a 
	 * new escapes map for the new allocEntry object
	 */ 
	newAllocEntry->pointer = ptr;
	newAllocEntry->length = len;
	newAllocEntry->allocToEscapeMap = NK_CARAT_ESCAPE_MAP_BUILD; 


	/*
	 * Return the new allocEntry object with saved state
	 */
	return newAllocEntry;
}



/*
 * =================== Utility Analysis Methods ===================
 */ 

/*
 * 
 */ 
#if 0
__attribute__((always_inline))
int _carat_does_alias(void *query_address, void *alloc_address, uint64_t length)
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
			 allocation_range_end = allocation_range_start + length,
			 query_address_int = ((uint64_t) query_address);


	/*
	 * Perform the range check and return the value
	 */ 
	int within_range = (true
						&& (query_address_int >= allocation_range_start) 
						&& (query_address_int < allocation_range_end));

	return within_range;
}

__attribute__((always_inline))
sint64_t _carat_get_query_offset(void *alloc_address, void *query_address, uint64_t length)
{	
	/*
	 * Check if @query_address aliases @alloc_address:
	 * - TRUE = return the offset (query_address - allocation_range_start)
	 * - FALSE = -1
	 */ 
	uint64_t allocation_range_start = ((uint64_t) alloc_address),
			 query_address_int = ((uint64_t) query_address); // Free computation

	sint64_t the_offset = 
		(_carat_does_alias(query_address, alloc_address, length)) ?
		(query_address_int - allocation_range_start) : 
		(-1);

	return the_offset;
}
#endif


sint64_t doesItAlias(void *allocAddr, uint64_t length, uint64_t escapeVal)
{
	uint64_t blockStart = (uint64_t) allocAddr;
	if ((escapeVal >= blockStart) && (escapeVal < (blockStart + length))) { // CONV [no ()] -> [()]
		return escapeVal - blockStart;
	}
	else{
		return -1;
	}
}



#if 0
allocEntry * findAllocEntry(void *address)
{
	// CONV [brian] -> [better than brian] 	
	/*
	 * Find a prospective object (skiplist node) in the allocation map 
	 * based on @address --- using "better_lower_bound"
	 * 
	 * "better_lower_bound" returns the node containing the
	 * address that is the closest to @address AND <= @address
	 */
	__auto_type *lower_bound_node = CARAT_ALLOCATION_BETTER_LOWER_BOUND(address);


	/* 
	 * Possible exists --- [prospective] -> [findAllocEntry return]:
	 * 1) the left sentinal (nk_slist_node, see below) -> NULL
	 * 2) an allocEntry that does contain the address -> allocEntry *
	 * 3) a allocEntry that does NOT contain the address -> NULL
	 */ 

	// skiplist node -> data -> second == allocEntry object from [address : allocEntry *]
	allocEntry *prospective_entry = ((allocEntry *) lower_bound_node->data->second);

	/*
	 * CONDITION 1 --- better_lower_bound may return the left sential if  
	 * the address is lower than all addresses that exist in the skip list 
	 * (edge case)
	 * 
	 * Handle with a check against the left sentinal
	 */
	// FIX --- engineering issue
	if (!prospective_entry) { return NULL; }
	

	/*
	 * CONDITION 2 --- gather the prospective entry's address, the length of
	 * the prospective's allocation, and CHECK if @address aliases this allocation
	 */ 
	uint64_t prospective_size = prospective_entry->length; 
	void *prospective_address = ((void *) prospective_entry->data->first);
	if (!(_carat_does_alias((address, prospective_address, prospective_size))) { return NULL; }


	/*
	 * "prospective_entry" is the correct allocEntry object! Return it
	 */ 
	return prospective_entry;
}
#endif

allocEntry* findAllocEntry(void *address){

	// CONV [brian] -> [better than brian] 
	nk_vc_printf("%lu", (uintptr_t) address);	
	__auto_type *prospective = nk_map_better_lower_bound(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address));

	nk_slist_node_uintptr_t_uintptr_t *iter;
    nk_pair_uintptr_t_uintptr_t *pair;
	
	nk_vc_printf("map node addr  :  ( real addr  :  allocEntry addr  ---  ( pointer,  len ))\n"); 
    nk_map_foreach(allocationMap, pair, iter) {        
        allocEntry *the_entry = (allocEntry *) (pair->second);
		nk_vc_printf("%p : (%p : %p --- (ptr: %p, len: %d))\n", iter, pair->first, the_entry, the_entry->pointer, the_entry->length);
    }


	/* 
	 * [prospective] -> [findAllocEntry return]:
	 * 1) the left sentinal (see below) -> NULL
	 * 2) a nk_slist_node that does contain the address -> allocEntry *
	 * 3) a nk_slist_node that does NOT contain the address -> NULL
	 */ 

	// better_lower_bound may return the left sential if the address is 
	// lower than all addresses that exist in the skip list (edge case)
	
	nk_vc_printf("in findAllocEntry\n");
	nk_vc_printf("prospective: %p\n", prospective);
	nk_vc_printf("prospective->data: %p\n", prospective->data);
	nk_vc_printf("prospective->data->first: %p\n", prospective->data->first);
	
	// FIX --- engineering issue
	if (!(prospective->data->second)) { return NULL; }
	
	nk_vc_printf("in findAllocEntry --- part 2\n");
	nk_vc_printf("the address: %p\n", address);
	

	// Gather length of the block
	allocEntry *prospective_entry = ((allocEntry *) prospective->data->second); 
	nk_vc_printf("prospective_entry\n");

	nk_vc_printf("prospective_entry: %p\n", prospective_entry);

	uint64_t blockLen = prospective_entry->length; 
	
	nk_vc_printf("prospective --- %p : (%p, %d)\n", prospective->data->first, prospective_entry->pointer, prospective_entry->length);

	nk_vc_printf("blockLen : %d\n", blockLen);

	// Could be in the block (return NULL otherwise)
	if (doesItAlias(((void **) (prospective->data->first)), blockLen, (uint64_t) address) == -1) { return NULL; }

	return prospective_entry;
}


/*
 * Self explanatory stats for KARAT
 */ 
__attribute__((always_inline))
void ReportStatistics()
{
	/*
	 * Report size of allocation map
	 * 
	 * TODO --- add more statistics
	 */ 
	CARAT_PRINT("Size of Allocation Table: %lu\n", nk_map_get_size(allocationMap)); 
	return;
}


/*
 * =================== Allocations Handling Methods ===================
 */ 
#if 0
void AddToAllocationTable(void *address, uint64_t length)
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
		"AddToAllocationTable: CARAT_ALLOCATION_MAP_INSERT failed on address"
	);


	return;
}
#endif

void AddToAllocationTable(void *address, uint64_t length)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY


	CARAT_PRINT("In add to alloc: %p, %lu, %p\n", address, length, allocationMap);

	allocEntry *newEntry = allocEntrySetup(address, length); // CONV [calling class constructor] -> [calling function that returns an instance]
	CARAT_PRINT("Adding to allocationMap\n");

	nk_slist_node_uintptr_t_uintptr_t *iter;
	nk_pair_uintptr_t_uintptr_t *pair;
	nk_map_foreach(allocationMap, pair, iter) {
		allocEntry *the_entry = (allocEntry *) (pair->second);
		nk_vc_printf("%p : (ptr: %p, len: %d)\n", pair->first, the_entry->pointer, the_entry->length);
	}
	// CONV [map::insert] -> [nk_map_insert + status check]
	if (!(nk_map_insert(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address), ((uintptr_t) newEntry)))) {
		panic("AddToAllocationTable: nk_map_insert failed on address %p\n", address);
	}

	nk_vc_printf("Post add\n");
	nk_map_foreach(allocationMap, pair, iter) {
		allocEntry *the_entry = (allocEntry *) (pair->second);
		nk_vc_printf("%p : (ptr: %p, len: %d)\n", pair->first, the_entry->pointer, the_entry->length);
	}

	CARAT_PRINT("Returning\n");
	/*
	nk_vc_printf("AddToAllocationTable: %p\n", address);

	nk_slist_node_uintptr_t_uintptr_t *iter;
	nk_pair_uintptr_t_uintptr_t *pair;
	nk_map_foreach(allocationMap, pair, iter) {
		nk_vc_printf("%p : %p\n", pair->first, pair->second);
	}
	*/
	return;
}

#if 0
void AddCallocToAllocationTable(void *address, uint64_t num_elements, uint64_t size_of_element)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY

	/*
	 * Create a new allocEntry object for the @address to be added --- here,
	 * the length has to be calculated from the parameters of calloc, which
	 * are @num_elements, and @size_of_element
	 */ 
	uint64_t length = num_elements * size_of_element;
	CREATE_ENTRY_AND_ADD (
		address, 
		"AddCallocToAllocationTable: CARAT_ALLOCATION_MAP_INSERT failed on address"
	);


	return;
}
#endif


void AddCallocToAllocationTable(void *address, uint64_t len, uint64_t sizeOfEntry){
	CHECK_CARAT_READY
	uint64_t length = len * sizeOfEntry;
	allocEntry *newEntry = allocEntrySetup(address, length); // CONV [class constructor] -> [function that returns an instance]

	// CONV [map::insert_or_assign] -> [nk_map_insert_by_force + status check]
	if (!(nk_map_insert_by_force(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address), ((uintptr_t) newEntry)))) {
		panic("AddCallocToAllocationTable: nk_map_insert failed on address %p\n", address);
	}


	return;
}

#if 0
void HandleReallocInAllocationTable(void *old_address, void *new_address, uint64_t length)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * allocation before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY


	/*
	 * Remove @old_address from the allocation map --- need to remove the 
	 * corresponding allocEntry object because of realloc's resizing
	 */ 
	REMOVE_ENTRY (
		old_address,
		"HandleReallocToAllocationTable: REMOVE_ENTRY failed on address"
	);


	/*
	 * Create an entry and add the mapping to the allocationMap
	 */ 
	CREATE_ENTRY_AND_ADD (
		new_address, 
		"HandleReallocToAllocationTable: CARAT_ALLOCATION_MAP_INSERT failed on address"
	);

	
	return;
}
#endif

void HandleReallocInAllocationTable(void *address, void *newAddress, uint64_t length){
	CHECK_CARAT_READY
	nk_map_remove(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address)); // CONV [map::erase] -> [nk_map_remove]
	allocEntry *newEntry = allocEntrySetup(newAddress, length); // CONV [class constructor] -> [function that returns an instance]

	// CONV [map::insert_or_assign] -> [nk_map_insert_by_force + status check]
	if (!(nk_map_insert_by_force(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) newAddress), ((uintptr_t) newEntry)))) {
		panic("HandleReallocToAllocationTable: nk_map_insert failed on newAddress %p\n", newAddress);
	}
	
	return;
}



#if 0
void RemoveFromAllocationTable(void *address)
{
	/*
	 * Only proceed if CARAT is ready (from init()) --- NOTE --- any
	 * free before CARAT is ready will NOT be tracked
	 */
	CHECK_CARAT_READY


	/*
	 * Remove @address from the allocation map
	 */ 
	REMOVE_ENTRY (
		address,
		"RemoveFromAllocationTable: REMOVE_ENTRY failed on address"
	);


	return;
}
#endif

// This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void *address){
	CHECK_CARAT_READY

	nk_vc_printf("RemoveFromAllocationTable: %p\n", address);

	nk_slist_node_uintptr_t_uintptr_t *iter;
	nk_pair_uintptr_t_uintptr_t *pair;
	nk_map_foreach(allocationMap, pair, iter) {
		allocEntry *the_entry = (allocEntry *) (pair->second);
		nk_vc_printf("%p : (ptr: %p, len: %d)\n", pair->first, the_entry->pointer, the_entry->length);
	}

	// CONV [map::erase] -> [nk_map_remove]
	if (!(nk_map_remove(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address)))) {
		panic("RemoveFromAllocationTable: nk_map_remove failed on address %p\n", address);
	}

	nk_vc_printf("POST TOAST\n");
	
	nk_map_foreach(allocationMap, pair, iter) {
		allocEntry *the_entry = (allocEntry *) (pair->second);
		nk_vc_printf("%p : (ptr: %p, len: %d)\n", pair->first, the_entry->pointer, the_entry->length);
	}

}




/*
 * =================== Escapes Handling Methods ===================
 */ 

/*
 * 1) Search for address escaping within allocation table (an ordered map that contains all current allocations <non overlapping blocks of memory each consisting of an  address and length> made by the program
 * 2) If found (the addressEscaping variable falls within one of the blocks), then a new entry into the escape table is made consisting of the addressEscapingTo, the addressEscaping, and the length of the addressEscaping (for optimzation if consecutive escapes occur)
 */

#if 0
void AddToEscapeTable(void *addressEscaping)
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
	if (totalEscapeEntries >= escapeWindowSize) { processEscapeWindow(); }


	/*
	 * Add the escape to the end of the escapeWindow --- this will be
	 * processed at some point in batch
	 */ 
	escapeWindow[totalEscapeEntries] = (void**)addressEscaping;
	totalEscapeEntries++;


	return;
}
#endif

void AddToEscapeTable(void* addressEscaping){
	CHECK_CARAT_READY
	if(totalEscapeEntries >= escapeWindowSize){
		processEscapeWindow();
	}
	escapeWindow[totalEscapeEntries] = (void**)addressEscaping;
	totalEscapeEntries++;
	CARAT_PRINT(stderr, "CARAT: %016lx\n", __builtin_return_address(0));

	return;
}


#if 0
void processEscapeWindow()
{	
	/*
	 * TOP --- perform batch processing of escapes in the escape window
	 */ 

	/*
	 * Build a set of escapes that are already processed --- if we encounter
	 * another escape that is part of this set, the batch processing will
	 * ignore it
	 */  
	nk_carat_escape_set *processed_escapes = CARAT_ESCAPE_SET_BUILD;


	/*
	 * Iterate through each escape, process it
	 */ 
	for (uint64_t i = 0; i < totalEscapeEntries; i++)
	{
		/*
		 * Get the next escape
		 */ 
		void **escaping_address = escapeWindow[i];


		/*
		 * Perform conditions checks on the escape:
		 * 1. Check if it's a valid pointer
		 * 2. Check if it's already been processed
		 * 3. Check if it corresponds to an allocation (allocEntry object)
		 * 
		 * TRUE == mark as processed, continue
		 * FALSE == process fully, continue
		 * 
		 * NOTE --- Condition 2 and "marking as processed" can be completed 
		 * in the same step because of the design of nk_slist_add --- which
		 * returns a status flag if an addition to the skiplist has actually 
		 * occurred
		 */ 
		allocEntry *corresponding_entry = NULL;

		if (false 
			|| (!escaping_address) /* Condition 1 */
			|| (!(NK_ESCAPE_SET_ADD(processed_escapes, escaping_address))) /* Condition 2, marking */
			|| (!(corresponding_entry = findAllocEntry(*escaping_address)))) /* Condition 3 */
			{ continue; }

		
		/*
		 * At this point --- we have found a corresponding entry where the 
		 * current escape should be recorded --- save this state to the 
		 * corresponding entry and continue
		 */  
		NK_ESCAPE_SET_ADD((corresponding_entry->allocToEscapeMap), escaping_address);
	}


	/*
	 * Reset the global escapes counter
	 */ 
	totalEscapeEntries = 0;


	return;
}
#endif


void processEscapeWindow(){
	CARAT_PRINT("\n\n\nI am invoked!\n\n\n");
	
	nk_slist_uintptr_t *processedEscapes = nk_slist_build(uintptr_t, 6); // Confirm top gear	

	for (uint64_t i = 0; i < totalEscapeEntries; i++)
	{
		void **addressEscaping = escapeWindow[i];

		// CONV [processedEscapes.find() ... != processedEscapes.end()] -> [nk_slist_find]
		if (nk_slist_find(uintptr_t, processedEscapes, ((uintptr_t) addressEscaping))) { continue; }	

		// CONV [unordered_set::insert] -> [nk_slist_add]
		nk_slist_add(uintptr_t, processedEscapes, ((uintptr_t) addressEscaping));

 		if (addressEscaping == NULL) { continue; }

		// FIX --- Dereferencing a void * in C --- ????
		allocEntry *alloc = findAllocEntry(*addressEscaping); // CONV [auto] -> [allocEntry*]

		// CONV [nullptr] -> [NULL]
		if (!alloc) { continue; }

		// We have found block
		// 
		// Inserting the object address escaping along with the length it is running for
		// and the length (usually 1) into escape table with key of addressEscapingTo
		// 
		// One can reverse engineer the starting point if the length is longer than 1
		// by subtracting from value in dereferenced addressEscapingTo
		
		// CONV [unordered_set::insert] -> [nk_slist_add]
		nk_slist_add(uintptr_t, (alloc->allocToEscapeMap), ((uintptr_t) addressEscaping));
	}

	totalEscapeEntries = 0;
}



/*
 * =================== Initilization Methods ===================
 */ 

/*
 * Utility to get %rsp
 */ 
__attribute__((always_inline))
uint64_t getrsp()
{
	uint64_t retVal;
	__asm__ __volatile__("movq %%rsp, %0" : "=a"(retVal) : : "memory");
	return retVal;
}


/*
 * Main driver for initialization
 */ 
#if 0
void texas_init()
{
	/*
	 * Stash %rsp for later initialization
	 */ 
	rsp = getrsp();
	

	/*
	 * Set up global allocation map
	 */ 
	allocationMap = CARAT_ALLOCATION_MAP_BUILD;

	
	/*
	 * Block 32 GB worth of "stack space" --- to "handle" the stack --- this
	 * is a hack/precursor for tracking stack allocations
	 * 
	 * Add the stack and its allocEntry object to the allocation map
	 */ 
	allocEntry *newEntry;
	uint64_t length = THIRTY_TWO_GB;
	void *rspVoidPtr = ((void *)(rsp - length)); // Set global

	CREATE_ENTRY_AND_ADD(
		rspVoidPtr,
		"texas_init: nk_map_insert failed on rspVoidPtr"
	);

	StackEntry = newEntry; // Set global from macro expansion


	/*
	 * Set of escape window and its corresponding statistics/counters
	 */ 
	escapeWindowSize = ESCAPE_WINDOW_SIZE;
	totalEscapeEntries = 0;
	escapeWindow = ((void ***) CARAT_MALLOC(escapeWindowSize * sizeof(void *)));


	/*
	 * KARAT is ready --- set the flag
	 */ 
	carat_ready = 1; // FIX --- Global needs change


	return;
}
#endif

void texas_init(){
	rsp = getrsp();
	
	allocationMap = nk_map_build(uintptr_t, uintptr_t); // CONV [new map<void *, allocEntry *>] -> [nk_map_build(uintptr_t, uintptr_t)]

	DS("texas"); 

	escapeWindowSize = 1048576;
	totalEscapeEntries = 0;

	//This adds the stack pointer to a tracked alloc that is length of 32GB which encompasses all programs we run
	void* rspVoidPtr = (void*)(rsp-0x800000000ULL);
	StackEntry = allocEntrySetup(rspVoidPtr, 0x800000000ULL); // CONV [constructor] -> [function that returns an instance]

	// CONV [map::insert_or_assign] -> [nk_map_insert_by_force + status check]
	if (!(nk_map_insert_by_force(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) rspVoidPtr), ((uintptr_t) StackEntry)))) {
		panic("texas_init: nk_map_insert failed on rspVoidPtr %p\n", rspVoidPtr);
	}

	escapeWindow = (void ***) CARAT_MALLOC(escapeWindowSize * sizeof(void *)); // CONV[calloc] -> [CARAT_MALLOC]

	// CARAT_PRINT("Leaving texas_init\n");

	carat_ready = 1; // WORST DESIGN EVER

	return;
}


