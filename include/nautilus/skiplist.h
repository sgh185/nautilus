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
 * Skip list implementation
 *
 * ASIDE: Too many things are referred to as gears here
 */ 

#ifndef __SLIST_H__
#define __SLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/nautilus.h>
#include <nautilus/limits.h>
#include <nautilus/naut_string.h>
#include <nautilus/libccompat.h>

#define DO_SLIST_PRINT 0 

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

#define USED __attribute__((used))

#define UPSHIFT(g) g++
#define WHILE_DOWNSHIFTING(i, start) for (i = start; i >= 0; i--)

// Need more declarations
#define int_MAX INT_MAX
#define int_MIN INT_MIN

#define uint32_t_MAX UINT_MAX
#define uint32_t_MIN 0	

#define NK_SLIST_INIT(type) \
	typedef struct nk_slist_node_##type { \
		struct nk_slist_node_##type **succ_nodes; \
		struct nk_slist_node_##type **pred_nodes; \
		type data; \
		uint8_t gear; \
	} nk_slist_node_##type; \
	typedef struct { \
		nk_slist_node_##type **all_gears; \
		uint8_t top_gear; \
	} nk_slist_##type; \

#define NK_SLIST_DECL(type) \
	NK_SLIST_INIT(type) \
	USED static inline nk_slist_node_##type *_nk_slist_find_worker_##type(int val, \
											  					   		  nk_slist_node_##type *ipts[], \
											   					   		  nk_slist_node_##type *the_gearbox, \
											   					   		  uint8_t start_gear, \
											   					   		  uint8_t record) { \
		int i; \
		nk_slist_node_##type *gearbox = the_gearbox; \
		\
		WHILE_DOWNSHIFTING(i, (start_gear - 1)) \
		{ \
			nk_slist_node_##type *next_node = gearbox->succ_nodes[i]; \
			\
			while (next_node) \
			{ \
				if (next_node->data < val) \
				{ \
					/* Throttle */ \
					next_node = next_node->succ_nodes[i]; \
					continue; \
				} \
				\
				/* Found the right node */ \
				if (next_node->data == val) { return next_node; } \
				\
				/* Clutch */ \
				gearbox = next_node->pred_nodes[i]; \
				\
				/* If we want to record insertion points */ \
				if (record) { ipts[i] = gearbox; } \
				\
				break; \
			} \
		} \
		\
		return NULL; \
	} \

// Skip list internals
inline uint8_t _nk_slist_get_rand_gear(uint8_t top_gear)
{
	uint8_t gear = 1;
	while (((lrand48()) & 1) && (gear < top_gear)) { UPSHIFT(gear); }
	return gear;
}

#define _nk_slist_node_link(pred, succ, gear) ({ \
	pred->succ_nodes[gear] = succ; \
	succ->pred_nodes[gear] = pred; \
})

// Setup, cleanup
#define nk_slist_build(type, tg) ({ \
	SEED; \
	\
	/* Set up skip list parent structure */ \
	nk_slist_##type *sl = (nk_slist_##type *) (SLIST_MALLOC(sizeof(nk_slist_##type))); \
	\
	/* Init gears */ \
	sl->all_gears = (nk_slist_node_##type **) (SLIST_MALLOC(sizeof(nk_slist_node_##type *) * tg)); \
	memset(sl->all_gears, 0, sizeof(*(sl->all_gears))); \
	\
	/* Set top gear */ \
	sl->top_gear = tg; \
	\
	/* Sentinals --- build nodes */ \
	nk_slist_node_##type *left_sentinal = nk_slist_node_build(type, type##_MIN, tg); \
	nk_slist_node_##type *right_sentinal = nk_slist_node_build(type, type##_MAX, tg); \
	\
	/* Set all initial linked-list head and tail pointers */ \
	int i; \
	for (i = 0; i < tg; i++) \
	{ \
		/* Set head to skip list parent structure gears \
		   array --- marks head of each list */ \
		sl->all_gears[i] = left_sentinal; \
		\
		/* Set list node pointers for each gear level \
		   for each sentinal structure */ \
		left_sentinal->pred_nodes[i] = right_sentinal->succ_nodes[i] = NULL; \
		_nk_slist_node_link(left_sentinal, right_sentinal, i); \
	} \
	\
	sl; \
}) \


#define nk_slist_node_build(type, val, g) ({ \
	/* Allocate */ \
	nk_slist_node_##type *sln = (nk_slist_node_##type *) (SLIST_MALLOC(sizeof(nk_slist_node_##type))); \
	\
	/* Set fields */ \
	sln->data = val; \
	sln->gear = g; \
	sln->succ_nodes = (nk_slist_node_##type **) (SLIST_MALLOC(sizeof(nk_slist_node_##type *) * sln->gear)); \
	sln->pred_nodes = (nk_slist_node_##type **) (SLIST_MALLOC(sizeof(nk_slist_node_##type *) * sln->gear)); \
	\
	/* Zero initialize */ \
	memset(sln->succ_nodes, 0, sizeof(*(sln->succ_nodes))); \
	memset(sln->pred_nodes, 0, sizeof(*(sln->pred_nodes))); \
	\
	sln; \
})

#define nk_slist_node_destroy(sln) ({ \
	free(sln->succ_nodes); \
	free(sln->pred_nodes); \
	free(sln); \
})

#define nk_slist_destroy(sl) ({ \
	/* Gather all nodes via bottom gear list */ \
	__auto_type *sln = sl->all_gears[0]; \
	\
	/* Iterate and delete */ \
	while (sln != NULL) \
	{ \
		__auto_type *temp = sln; \
		sln = sln->succ_nodes[0]; \
		nk_slist_node_destroy(temp); \
	} \
	\
	free(sl); \
})

// Skip list operations
#define nk_slist_find(type, sl, val) ({ \
	nk_slist_node_##type *the_gearbox = sl->all_gears[sl->top_gear - 1], \
						 *found = _nk_slist_find_worker_##type (val, NULL, the_gearbox, sl->top_gear, 0); \
	found; \
})

#define nk_slist_add(type, sl, val) ({ \
	/* Set up new node */ \
	uint8_t new_gear = _nk_slist_get_rand_gear(sl->top_gear); \
	nk_slist_node_##type *ipts[new_gear]; \
	\
	SLIST_PRINT("HERE\n"); \
	/* Find all insertion points */ \
	nk_slist_node_##type *the_gearbox = sl->all_gears[(new_gear) - 1], \
				  		 *found_node = _nk_slist_find_worker_##type (val, ipts, the_gearbox, new_gear, 1); \
	SLIST_PRINT("HERE1\n"); \
	\
	/* Not going to add the node if it already exists */ \
	if (found_node) { 0; } \
	\
	nk_slist_node_##type *new_node = nk_slist_node_build(type, val, new_gear); \
	SLIST_PRINT("HERE2\n"); \
	\
	/* Set all successor and predecessor links */ \
	int i; \
	for (i = 0; i < new_node->gear; i++) \
	{ \
		nk_slist_node_##type *succ_node = ipts[i]->succ_nodes[i]; \
		_nk_slist_node_link(ipts[i], new_node, i); \
		_nk_slist_node_link(new_node, succ_node, i); \
	} \
	SLIST_PRINT("HERE3\n"); \
	\
	1; \
})

#define nk_slist_remove(type, sl, val) ({ \
	\
	/* Find the node */ \
	nk_slist_node_##type *found_node = nk_slist_find(type, sl, val); \
	\
	/* Can't remove anything if the node doesn't exist */ \
	if (!found_node) { 0; } \
	\
	/* Reset all predecessor and successor pointers */ \
	int i; \
	for (i = 0; i < found_node->gear; i++) { \
		_nk_slist_node_link(found_node->pred_nodes[i], found_node->succ_nodes[i], i); \
	} \
	\
	/* Free the node */ \
	nk_slist_node_destroy(found_node); \
	\
	1; \
})

NK_SLIST_DECL(int);

#ifdef __cplusplus
}
#endif


#endif
