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

#include <nautilus/skiplist.h>

#define UPSHIFT(g) g++
inline uint8_t nk_slist_get_rand_gear(uint8_t top_gear)
{
	uint8_t gear = 1;
	while (((lrand48()) & 1) && (gear < top_gear)) { UPSHIFT(gear); }
	return gear;
}

inline void nk_slist_node_link(nk_slist_node *pred, nk_slist_node *succ, uint8_t gear)
{
	pred->succ_nodes[gear] = succ;
	succ->pred_nodes[gear] = pred;

	return;
}

nk_slist *nk_slist_build(uint8_t top_gear)
{
	SEED;
	
	// Set up skip list parent structure 
	nk_slist *sl = (nk_slist *) (SLIST_MALLOC(sizeof(nk_slist)));
	
	// Init gears 
	sl->all_gears = (nk_slist_node **) (SLIST_MALLOC(sizeof(nk_slist_node *) * top_gear));
	memset(sl->all_gears, 0, sizeof(*(sl->all_gears)));

	// Set top gear
	sl->top_gear = top_gear;

	// Sentinals --- build nodes 
	nk_slist_node *left_sentinal = nk_slist_node_build(INT_MIN, top_gear);
	nk_slist_node *right_sentinal = nk_slist_node_build(INT_MAX, top_gear);

	// Set all initial linked-list head and tail pointers
	int i;
	for (i = 0; i < top_gear; i++)
	{
		// Set head to skip list parent structure gears
		// array --- marks head of each list 
		sl->all_gears[i] = left_sentinal;

		// Set list node pointers for each gear level
		// for each sentinal structure
		left_sentinal->pred_nodes[i] = right_sentinal->succ_nodes[i] = NULL;
		nk_slist_node_link(left_sentinal, right_sentinal, i);	
	}

	return sl;
}

nk_slist_node *nk_slist_node_build(int val, uint8_t g)
{
	// Allocate
	nk_slist_node *sln = (nk_slist_node *) (SLIST_MALLOC(sizeof(nk_slist_node)));

	// Set fields
	sln->data = val;
	sln->gear = g; 
	sln->succ_nodes = (nk_slist_node **) (SLIST_MALLOC(sizeof(nk_slist_node *) * sln->gear));
	sln->pred_nodes = (nk_slist_node **) (SLIST_MALLOC(sizeof(nk_slist_node *) * sln->gear));

	// Zero initialize
	memset(sln->succ_nodes, 0, sizeof(*(sln->succ_nodes)));
	memset(sln->pred_nodes, 0, sizeof(*(sln->pred_nodes)));

	return sln;
}

void nk_slist_node_destroy(nk_slist_node *sln)
{
	free(sln->succ_nodes);
	free(sln->pred_nodes);
	free(sln);

	return;
}

void nk_slist_destroy(nk_slist *sl)
{
	// Gather all nodes via bottom gear list
	nk_slist_node *sln = sl->all_gears[0];

	// Iterate and delete
	while (sln != NULL)
	{
		nk_slist_node *temp = sln;
		sln = sln->succ_nodes[0];
		nk_slist_node_destroy(temp);	
	}

	free(sl);

	return;
}

// Needs serious optimization 
#define WHILE_DOWNSHIFTING(i, start) for (i = start; i >= 0; i--)
inline nk_slist_node *nk_slist_find_worker(int val, 
										   nk_slist_node *ipts[],
										   nk_slist_node *the_gearbox,
										   uint8_t start_gear,
										   uint8_t record)
{
	int i;
	nk_slist_node *gearbox = the_gearbox;

	WHILE_DOWNSHIFTING(i, (start_gear - 1))
	{
		nk_slist_node *next_node = gearbox->succ_nodes[i];

		while (next_node)
		{
			if (next_node->data < val)
			{
				// Throttle
				next_node = next_node->succ_nodes[i];
				continue;
			}

			// Found the right node
			if (next_node->data == val) { return next_node; }
	
			// Clutch
			gearbox = next_node->pred_nodes[i];
	
			// If we want to record insertion points
			if (record) { ipts[i] = gearbox; }
		
			break;		
		}
	}	

	return NULL;	
}

nk_slist_node *nk_slist_find(nk_slist *sl, int val)
{
	nk_slist_node *the_gearbox = sl->all_gears[sl->top_gear - 1];
	return nk_slist_find_worker(val, NULL, the_gearbox, sl->top_gear, 0); 
}

int nk_slist_add(nk_slist *sl, int val)
{
	// Set up new node
	uint8_t new_gear = nk_slist_get_rand_gear(sl->top_gear);
	nk_slist_node *ipts[new_gear];
	
	// Find all insertion points 
	nk_slist_node *the_gearbox = sl->all_gears[(new_gear) - 1],
				  *found_node = nk_slist_find_worker(val, ipts, the_gearbox, new_gear, 1);

	// Not going to add the node if it already exists
	if (found_node) { return 0; }
	
	nk_slist_node *new_node = nk_slist_node_build(val, new_gear);
	
	// Set all successor and predecessor links
	int i;
	for (i = 0; i < new_node->gear; i++) 
	{
		nk_slist_node *succ_node = ipts[i]->succ_nodes[i];
		nk_slist_node_link(ipts[i], new_node, i);
		nk_slist_node_link(new_node, succ_node, i);
	}

	return 1;
}


int nk_slist_remove(nk_slist *sl, int val)
{
	// Find the node
	nk_slist_node *found_node = nk_slist_find(sl, val);

	// Can't remove anything if the node doesn't exist
	if (!found_node) { return 0; }

	// Reset all predecessor and successor pointers
	int i;
	for (i = 0; i < found_node->gear; i++) {
		nk_slist_node_link(found_node->pred_nodes[i], found_node->succ_nodes[i], i);	
	}

	// Free the node
	nk_slist_node_destroy(found_node);

	return 1;
}
