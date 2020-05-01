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
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Souradip Ghosh <sgh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
 * Concurrent skip list implementation --- based on Herlihy et al. '07.
 * More info at https://people.csail.mit.edu/shanir/publications/LazySkipList.pdf 
 */ 

#ifndef __SLIST_H__
#define __SLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/naut_string.h>
#include <nautilus/libccompat.h>

#define DO_SLIST_PRINT 1 

#if DO_SLIST_PRINT
#define SLIST_PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define SLIST_PRINT(...) 
#endif

#define SLIST_MALLOC(n) ({void *__p = malloc(n); if (!__p) { SLIST_PRINT("Malloc failed\n"); panic("Malloc failed\n"); } __p;})
#define SLIST_REALLOC(p, n) ({void *__p = realloc(p, n); if (!__p) { SLIST_PRINT("Realloc failed\n"); panic("Malloc failed\n"); } __p;})

#define RAND_MAGIC 0x7301
#define SEED srand48(rdtsc() % RAND_MAGIC)
#define SLIST_TOP_GEAR 8 // Deprecated

typedef struct nk_slist_node {
	struct nk_slist_node **succ_nodes;
	struct nk_slist_node **pred_nodes;
	int data;
	uint8_t gear;
} nk_slist_node;

typedef struct {
	nk_slist_node **all_gears;
	uint8_t top_gear;
} nk_slist;

// Skip list internals
inline uint8_t nk_slist_get_rand_gear(uint8_t top_gear);
inline void nk_slist_node_link(nk_slist_node *pred, nk_slist_node *succ, uint8_t gear);

// Setup, cleanup
nk_slist *nk_slist_build(uint8_t top_gear);
nk_slist_node *nk_slist_node_build(int val, uint8_t g);
void nk_slist_node_destroy(nk_slist_node *sln);
void nk_slist_destroy(nk_slist *sl);

// Skip list operations
inline nk_slist_node *nk_slist_find_worker(int val, 
										   nk_slist_node *ipts[],
										   nk_slist_node *the_gearbox,
										   uint8_t start_gear,
										   uint8_t record);
nk_slist_node *nk_slist_find(nk_slist *sl, int val);
int nk_slist_add(nk_slist *sl, int val);
int nk_slist_remove(nk_slist *sl, int val);

#ifdef __cplusplus
}
#endif


#endif
