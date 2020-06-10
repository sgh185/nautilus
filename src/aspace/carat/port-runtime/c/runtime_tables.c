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


//This header file holds the functions that will be injected into any program using Carat. The functions are responsible for building the runtime tables
//as well as holding the PDG (will have to figure this out).

//Determine C vs C++ (Probably C++)


#include "runtime_tables.h" // CONV [.hpp] -> [.h]



//Alloc addr, length
nk_slist_uintptr_t_uintptr_t *allocationMap = NULL; // FIX (for pairs, maps)

allocEntry* StackEntry;
uint64_t rsp = 0;

uint64_t escapeWindowSize = 0;
uint64_t totalEscapeEntries = 0;
void*** escapeWindow = NULL; // CONV [nullptr] -> [NULL]

int carat_ready = 0;

// CONV [class with an init constructor] -> [init function] FIX do we need to call this somewhere?
// TODO: throw this in init, eventually call this with every new address space
/*
void texasStartup() {
	user_init();
	texas_init();
} 
*/

// CONV [class constructor] -> [function that returns an instance]
allocEntry* allocEntrySetup(void* ptr, uint64_t len){
	allocEntry* newAllocEntry = (allocEntry*) CARAT_MALLOC(sizeof(allocEntry)); // maybe FIX
	newAllocEntry->pointer = ptr;
	newAllocEntry->length = len;
	newAllocEntry->allocToEscapeMap = nk_slist_build(uintptr_t, 6); // maybe FIX the 6
	return newAllocEntry;
}


sint64_t doesItAlias(void *allocAddr, uint64_t length, uint64_t escapeVal){
	uint64_t blockStart = (uint64_t) allocAddr;
	if ((escapeVal >= blockStart) && (escapeVal < (blockStart + length))) { // CONV [no ()] -> [()]
		return escapeVal - blockStart;
	}
	else{
		return -1;
	}
}

void AddToAllocationTable(void *address, uint64_t length){
	CHECK_CARAT_READY

	CARAT_PRINT("In add to alloc: %p, %lu, %p\n", address, length, allocationMap);

	allocEntry *newEntry = allocEntrySetup(address, length); // CONV [calling class constructor] -> [calling function that returns an instance]
	CARAT_PRINT("Adding to allocationMap\n");

	// CONV [map::insert] -> [nk_map_insert + status check]
	if (!(nk_map_insert(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address), ((uintptr_t) newEntry)))) {
		panic("AddToAllocationTable: nk_map_insert failed on address %p\n", address);
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



/*
 * 1) Search for address escaping within allocation table (an ordered map that contains all current allocations <non overlapping blocks of memory each consisting of an  address and length> made by the program
 * 2) If found (the addressEscaping variable falls within one of the blocks), then a new entry into the escape table is made consisting of the addressEscapingTo, the addressEscaping, and the length of the addressEscaping (for optimzation if consecutive escapes occur)
 */
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

allocEntry* findAllocEntry(void *address){

	// CONV [brian] -> [better than brian] 
	
	__auto_type *prospective = nk_map_better_lower_bound(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address));

	/* 
	 * [prospective] -> [findAllocEntry return]:
	 * 1) the left sentinal (see below) -> NULL
	 * 2) a nk_slist_node that does contain the address -> allocEntry *
	 * 3) a nk_slist_node that does NOT contain the address -> NULL
	 */ 

	// better_lower_bound may return the left sential if the address is 
	// lower than all addresses that exist in the skip list (edge case)
	
	// FIX --- engineering issue
	if (!(prospective->data->second)) { return NULL; }

	// Gather length of the block
	allocEntry *prospective_entry = ((allocEntry *) prospective->data->second); 
	uint64_t blockLen = prospective_entry->length; 

	// Could be in the block (return NULL otherwise)
	if (doesItAlias(((void **) (prospective->data->first)), blockLen, (uint64_t) address) == -1) { return NULL; }

	return prospective_entry;
}

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


// This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void *address){
	CHECK_CARAT_READY

	nk_vc_printf("RemoveFromAllocationTable: %p\n", address);

	nk_slist_node_uintptr_t_uintptr_t *iter;
	nk_pair_uintptr_t_uintptr_t *pair;
	nk_map_foreach(allocationMap, pair, iter) {
		nk_vc_printf("%p : %p\n", pair->first, pair->second);
	}

	// CONV [map::erase] -> [nk_map_remove]
	nk_map_remove(allocationMap, uintptr_t, uintptr_t, ((uintptr_t) address));
}

void ReportStatistics(){
	// CONV [map::size] -> [nk_slist_get_size]
	// NOTE --- nk_slist_get_size ISN'T implemented yet
	CARAT_PRINT("Size of Allocation Table: %lu\n", nk_map_get_size(allocationMap)); 
}


uint64_t getrsp(){
	uint64_t retVal;
	__asm__ __volatile__("movq %%rsp, %0" : "=a"(retVal) : : "memory");
	return retVal;
}


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


