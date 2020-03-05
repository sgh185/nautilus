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
 * Copyright (c) 2019, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2019, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author:  Souradip Ghosh <sgh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *  
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/fiber.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/random.h>
#include <nautilus/libccompat.h>
#include <nautilus/timehook.h>
#include <nautilus/barrier.h>
#include <nautilus/pmc.h>
#include <test/fibers.h>
#include <test/fiberbench/sha1.h>
#include <test/fiberbench/rijndael.h>
#include <test/fiberbench/md5.h>
#include <test/fiberbench/knn.h>

#define DO_PRINT       0

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif

// --------------------------------------------------------------------------------

//******************* Macros/Helper Functions/Globals *******************/
// Seeding macro
#define SEED() (srand48(rdtsc() % 128))

// Malloc with error checking
#define MALLOC(n) ({void *__p = malloc(n); if (!__p) { PRINT("Malloc failed\n"); panic("Malloc failed\n"); } __p;})

#define M 300 // Loop iteration --- timing loops
#define DOT 1000 // Dot product induction variable

// Dimensions for arrays --- matrix multiply
#define M1 20 
#define M2 40
#define M3 30

extern struct nk_virtual_console *vc;
extern void nk_simple_timing_loop(uint64_t);

// A suspicious solution to collecting fiber yield
// data --- when ACCESS_WRAPPER is set to 1, data
// is collected (using methods in src/nautilus/fiber.c)
// and vice versa.
int ACCESS_WRAPPER = 0;

// Linked List Implementation
__attribute__((noinline)) List_t * createList(uint64_t start, uint64_t size) 
{
	// Allocate head
	Node_t *L_head = MALLOC(sizeof(Node_t));
	if (!L_head) { return NULL; }

	// Set up head
	L_head->value = start;
	L_head->next = NULL;

	// Allocate list
	List_t *LL = MALLOC(sizeof(List_t));
	if (!LL) { return NULL; }

	// Set up list
	LL->head = L_head;
	LL->tail = LL->head;

	// Fill list with values based on the "start"
	// argument
	uint64_t a, incr = start;
	for (a = 0; a < size; a++)
  	{
		incr += (a * a) + incr;

		// Allocate a new node
		Node_t *L_node = MALLOC(sizeof(Node_t));
		if (!L_node) { return NULL; }

		// Set up the new node
		L_node->value = incr;
		L_node->next = NULL;

		// Add node to linked-list
		LL->tail->next = L_node;
		LL->tail = L_node;  
  	}
	
  	return LL;
}

// Binary Tree Implementation
__attribute__((noinline)) TreeNode_t * createTree(uint64_t start, uint64_t size) {
  
	// Allocate root node	 
	TreeNode_t *T_root = MALLOC(sizeof(TreeNode_t));
	if (!T_root) { return NULL; }

	// Set up root node
	T_root->value = start;
	T_root->left = NULL;
	T_root->right = NULL;

	// Fill tree
	SEED();
	int i;
	for (i = 0; i < size; i++)
	{
		// Build tree using an iterator --- set it to
		// root for initial traversal/node building
		TreeNode_t *T_iterator = T_root;
		
		// Find a "random" value for the tree node
		uint64_t val = (lrand48() % 10000); 
		srand48(0);

		// Allocate a new node
		TreeNode_t *T_node = MALLOC(sizeof(TreeNode_t));
		if (!T_node) { return NULL; };

		// Set up new node	
		T_node->value = val;
		T_node->left = NULL;
		T_node->right = NULL;
    

    	// Place node in tree (based on binary search tree invariant)
		while (1)
		{
			if (val > T_iterator->value)
		  	{
				if (!(T_iterator->right))
				{
				  T_iterator->right = T_node;
				  break; 
				}

				T_iterator = T_iterator->right;
		  	}
		  	else
		  	{
				if (!(T_iterator->left))
				{
				  T_iterator->left = T_node;
				  break;
				}

				T_iterator = T_iterator->left;
		  	}  
		}

	}

  	return T_root;
}

#define MAX_QUEUE_SIZE 1000
// Create queue for BST --- used for traversal
__attribute__((noinline)) TreeQueue_t * createQueue(void) {

	// Allocate a new queue object
	TreeQueue_t *new_queue = MALLOC(sizeof(TreeQueue_t));
	if (!new_queue) { return NULL; }

	// Allocate the actual queue of length "queue_size"
	new_queue->queue = MALLOC(sizeof(TreeNode_t *) * MAX_QUEUE_SIZE);
	if (!new_queue->queue) { return NULL; }

	// Set up the queue
	new_queue->head_pos = 0;
	new_queue->tail_pos = 0;

	return new_queue;

}

// Create array of random unsigned integers
__attribute__((noinline)) uint64_t * createRandArray(uint64_t size) {

	SEED();

	// Allocate array of length "size"
	uint64_t *r_array = MALLOC(sizeof(uint64_t) * size);
	if(!r_array) { return NULL; }

	// Fill array with "random" values
	int i;
	for (i = 0; i < size; i++) {
		r_array[i] = (lrand48() % 1000);
	}

	return r_array;

}

// --------------------------------------------------------------------------------
 
/******************* Test Routines *******************/

// NOTE --- ACCESS_WRAPPER is set to 1 and back to 0 in the same 
// fiber even when there may be multiple fibers per benchmark. 
// This is added as a precaution where yielding may not occur
// as expected and we continuously collect (the wrong) data or
// we don't collect enough data.

// ------
// Benchmark 1 --- timing loops (timing_loop_1, timing_loop_2)
void timing_loop_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	int a = 0; 

	ACCESS_WRAPPER = 1;

	while(a < M)
	{
		nk_simple_timing_loop(200);
		a++;
	}

	ACCESS_WRAPPER = 0;
	
	return;
}

void timing_loop_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	int a = 0;

	ACCESS_WRAPPER = 1;

	while(a < M)
	{
		nk_simple_timing_loop(200);
		a++;
	}

	ACCESS_WRAPPER = 0;

	// Print out timing data --- end of test
	_nk_fiber_print_data();
	
	return;
}
// ------


// ------
// Benchmark 2 --- staggered timing loops (s_timing_loop_1, s_timing_loop_2)
void s_timing_loop_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	int a = 0;
	
	ACCESS_WRAPPER = 1;

	while(a < M)
	{
		if (a % 25 == 0) { nk_simple_timing_loop(200000000); }
		else { nk_simple_timing_loop(200); }
		a++;
	}
	
	ACCESS_WRAPPER = 0;

	return;
}

void s_timing_loop_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	int a = 0;

	ACCESS_WRAPPER = 1;

	while(a < M)
	{
		if (a % 25 == 0) { nk_simple_timing_loop(200000000); }
		else { nk_simple_timing_loop(200); }
		a++;
	}

	ACCESS_WRAPPER = 0;

	// Print out timing data --- end of test
	_nk_fiber_print_data();

	return;
}
// ------


// ------
// Benchmark 3 --- dot product (dot_prod_1, dot_prod_2) 
void dot_prod_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	int k, first = 8, second = 134; // Arbitrary

	// Allocate on stack
	uint64_t b[DOT];
	uint64_t c[DOT];

	ACCESS_WRAPPER = 1;

	// Fill arrays
	for (k = 0; k < DOT; k++)
	{
		b[k] = first * second;
		c[k] = first + second;

		first += 4;
		second += 2; 
	}

	// Dot product
	volatile uint64_t sum = 0;
	for (k = 0; k < DOT; k++) {
		sum += (b[k] * c[k]);
	}

	ACCESS_WRAPPER = 0;

	return;
}

void dot_prod_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	int k, first = 5, second = 127; // Arbitrary

	// Allcoate on stack
	uint64_t b[DOT];
	uint64_t c[DOT];

	ACCESS_WRAPPER = 1;

	uint64_t start = rdtsc();
	// Fill arrays
	for (k = 0; k < DOT; k++)
	{
		b[k] = first * second;
		c[k] = first + second;

		first += 4;
		second += 2; 
	}

	uint64_t finish = rdtsc();

	// Dot product
	volatile uint64_t sum = 0;
	for (k = 0; k < DOT; k++) {
		sum += (b[k] * c[k]);
	}
	
	ACCESS_WRAPPER = 0;
	
	// Print out timing data --- end of test
	_nk_fiber_print_data();

	nk_vc_printf("finish - start: %lu\n", finish - start);

	return;
}
// ------


// ------
// Benchmark 4 --- linked-list traversal (ll_traversal_1, ll_traversal_2)
void ll_traversal_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	
	// Create the list
	List_t *LL = createList(7, 800); // Start val of 7, 800 node list
	if (!LL) { return; } 

	ACCESS_WRAPPER = 1;

	// Set up iterator for list
	volatile uint64_t sum = 0;
	Node_t *iterator = LL->head;

	// Traverse list, calculate sum
	while(iterator != NULL)
	{
		sum += iterator->value;
		iterator = iterator->next;
	}

	ACCESS_WRAPPER = 0;

	// No frees at the end --- FIX
	
	return;
}

void ll_traversal_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	
	// Create the list
	List_t *LL = createList(11, 800); // Start val of 11, 800 node list
	if (!LL) { return; } 

	ACCESS_WRAPPER = 1;

	// Set up iterator for list
	volatile uint64_t sum = 0;
	Node_t *iterator = LL->head;

	// Traverse list, calculate sum
	while(iterator != NULL)
	{
		sum += iterator->value;
		iterator = iterator->next;
	}

	ACCESS_WRAPPER = 0;

	_nk_fiber_print_data();
  
	// No frees at the end --- FIX
	
	return;
}
// ------


// ------
// Benchmark 5 --- naive matrix multiply with known dimensions (mm_1, mm_2) 
void mm_1(void *i, void **o)
{ 
	nk_fiber_set_vc(vc);

	// Allocate on stack --- known dimensions
	uint64_t first[M1][M2];
	uint64_t second[M2][M3];
	uint64_t result[M1][M3];

	// Zero initialize
	memset(first, 0, sizeof(first));
	memset(second, 0, sizeof(second));
	memset(result, 0, sizeof(result));

	uint64_t start = 4; // arbitrary
	int a, b, c;

	ACCESS_WRAPPER = 1;

	// Fill first and second matricies
	for (a = 0; a < M1; a++) {
		for (b = 0; b < M2; b++) {
		  first[a][b] = a + (a * b) + b;
		}
	}

	for (a = 0; a < M2; a++) {
		for (b = 0; b < M3; b++) {
		  second[a][b] = (a * b);
		}
	}

	// Naive matrix multiply
	for (a = 0; a < M1; a++) {
		for (b = 0; b < M3; b++) {
			uint64_t sum = 0;     

			for (c = 0; c < M2; c++) {
				sum += (first[a][c] * second[c][b]);
			}

			result[a][b] = sum;
		}
	}
	  
	ACCESS_WRAPPER = 0;
  
	return;
}

void mm_2(void *i, void **o)
{
 	nk_fiber_set_vc(vc);

	// Allocate on stack --- known dimensions
	uint64_t first[M1][M2];
	uint64_t second[M2][M3];
	uint64_t result[M1][M3];

	// Zero initialize
	memset(first, 0, sizeof(first));
	memset(second, 0, sizeof(second));
	memset(result, 0, sizeof(result));

	uint64_t start = 8; // arbitrary
	int a, b, c;

	ACCESS_WRAPPER = 1;

	uint64_t start_1 = rdtsc();

	// Fill first and second matricies
	for (a = 0; a < M1; a++) {
		for (b = 0; b < M2; b++) {
		  first[a][b] = a + (a * b) + b;
		}
	}

	for (a = 0; a < M2; a++) {
		for (b = 0; b < M3; b++) {
		  second[a][b] = (a * b);
		}
	}

	// Naive matrix multiply
	for (a = 0; a < M1; a++) {
		for (b = 0; b < M3; b++) {
			uint64_t sum = 0;     

			for (c = 0; c < M2; c++) {
				sum += (first[a][c] * second[c][b]);
			}

			result[a][b] = sum;
		}
	}

	uint64_t finish = rdtsc();	

	ACCESS_WRAPPER = 0;
  
	_nk_fiber_print_data();
	
	nk_vc_printf("finish - start: %lu\n", finish - start_1);
  
	return;
}
// ------


// ------
// Benchmark 6 --- binary tree traversal (bt_traversal_1, bt_traversal_2) 
void bt_traversal_1(void *i, void **o)
{
	nk_fiber_set_vc(vc); 

	// Allocate and build tree
	TreeNode_t *tree = createTree(4520, 800); // Start val = 4520, 800 nodes
	if (!tree) { return; }

	// Generate array of random integers, 50 elements
	int size = 50;
	uint64_t *nums = createRandArray(size); 

	uint64_t totalTraversalSum = 0;
	int a;

	ACCESS_WRAPPER = 1;

	// Calculate a sum using "nums", sum is calculated in the following
	// way: the tree is traversed to search for each number in "nums"
	// and all nodes' "val" fields that are visited during the the tree
	// traversal/search are summed and added to totalTraversalSum
	for (a = 0; a < size; a++)
	{
		uint64_t currTraversalSum = 0;
		volatile TreeNode_t *iterator = tree;

		while (iterator != NULL)
		{
			uint64_t currValue = iterator->value;
			currTraversalSum += currValue;

			if (nums[a] == currValue) { break; } 
			else if (nums[a] > currValue) { iterator = iterator->right; }
			else { iterator = iterator->left; } 
		} 

		totalTraversalSum += currTraversalSum; 
	}

	ACCESS_WRAPPER = 0;

	// Not freeing --- FIX

	return;
}

void bt_traversal_2(void *i, void **o)
{
  	nk_fiber_set_vc(vc); 

	// Allocate and build tree
	TreeNode_t *tree = createTree(5380, 800); // Start val = 5380, 800 nodes
	if (!tree) { return; }

	// Generate array of random integers, 50 elements
	int size = 50;
	uint64_t *nums = createRandArray(size); 

	uint64_t totalTraversalSum = 0;
	int a;

	ACCESS_WRAPPER = 1;

	// Calculate a sum using "nums", sum is calculated in the following
	// way: the tree is traversed to search for each number in "nums"
	// and all nodes' "val" fields that are visited during the the tree
	// traversal/search are summed and added to totalTraversalSum
	for (a = 0; a < size; a++)
	{
		uint64_t currTraversalSum = 0;
		volatile TreeNode_t *iterator = tree;

		while (iterator != NULL)
		{
			uint64_t currValue = iterator->value;
			currTraversalSum += currValue;

			if (nums[a] == currValue) { break; } 
			else if (nums[a] > currValue) { iterator = iterator->right; }
			else { iterator = iterator->left; } 
		} 

		totalTraversalSum += currTraversalSum; 
	}

	ACCESS_WRAPPER = 0;

	_nk_fiber_print_data();
	
	// Not freeing --- FIX
	
	return;
}
// ------


// ------
// Benchmark 7 --- naive matrix multiply with random dimensions (rand_mm_1, rand_mm_2) 
void rand_mm_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	
	SEED();

	// Generate random dimensions
	uint64_t R1 = (lrand48() % 50);
	uint64_t R2 = (lrand48() % 50);
	uint64_t R3 = (lrand48() % 50);

	// Allocate on stack
	uint64_t first[R1][R2];
	uint64_t second[R2][R3];
	uint64_t result[R1][R3];

	// Zero initialize
	memset(first, 0, sizeof(first));
	memset(second, 0, sizeof(second));
	memset(result, 0, sizeof(result));

	uint64_t start = 9; // arbitrary
	int a, b, c;

	ACCESS_WRAPPER = 1;

	// Fill first and second matricies
	for (a = 0; a < R1; a++) {
		for (b = 0; b < R2; b++) {
		  first[a][b] = a + (a * b) + b;
		}
	}

	for (a = 0; a < R2; a++) {
		for (b = 0; b < R3; b++) {
		  second[a][b] = (a * b);
		}
	}

	// Naive matrix multiply
	for (a = 0; a < R1; a++) {
		for (b = 0; b < R3; b++) {
			uint64_t sum = 0;     

			for (c = 0; c < R2; c++) {
				sum += (first[a][c] * second[c][b]);
			}

			result[a][b] = sum;
		}
	}
	  
	ACCESS_WRAPPER = 0;
  
	return;
}

void rand_mm_2(void *i, void **o)
{
 	nk_fiber_set_vc(vc);

	SEED();

	// Generate random dimensions
	uint64_t R1 = (lrand48() % 50);
	uint64_t R2 = (lrand48() % 50);
	uint64_t R3 = (lrand48() % 50);

	// Allocate on stack
	uint64_t first[R1][R2];
	uint64_t second[R2][R3];
	uint64_t result[R1][R3];

	// Zero initialize
	memset(first, 0, sizeof(first));
	memset(second, 0, sizeof(second));
	memset(result, 0, sizeof(result));

	uint64_t start = 6; // arbitrary
	int a, b, c;

	ACCESS_WRAPPER = 1;

	// Fill first and second matricies
	for (a = 0; a < R1; a++) {
		for (b = 0; b < R2; b++) {
		  first[a][b] = a + (a * b) + b;
		}
	}

	for (a = 0; a < R2; a++) {
		for (b = 0; b < R3; b++) {
		  second[a][b] = (a * b);
		}
	}

	// Naive matrix multiply
	for (a = 0; a < R1; a++) {
		for (b = 0; b < R3; b++) {
			uint64_t sum = 0;     

			for (c = 0; c < R2; c++) {
				sum += (first[a][c] * second[c][b]);
			}

			result[a][b] = sum;
		}
	}
	  
	ACCESS_WRAPPER = 0;
  
  	_nk_fiber_print_data();
	
	return;
}
// ------


// ------
// Benchmark 8 --- level-order binary tree traversal (bt_lo_traversal_1, bt_lo_traversal_2)
void bt_lo_traversal_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	// Allocate and set up queue for level-order traversal
	TreeQueue_t *LO = createQueue();

	// Allocate and set up tree --- Start = 5170, 800 nodes
	TreeNode_t *tree = createTree(5170, 800);

	// Set up iterator for traversal
	TreeNode_t *iterator = tree;
	volatile uint64_t sum = 0;

	ACCESS_WRAPPER = 1;

	// Calculate sum of nodes via level-order traversal
	while (iterator != NULL)
	{
		// Accumulate
		sum += iterator->value;

		if ((LO->tail_pos == MAX_QUEUE_SIZE) || (LO->head_pos > LO->tail_pos)) { break; }

		// Enqueue children of iterator
		if (iterator->left) 
		{
			LO->queue[LO->tail_pos] = iterator->left;
			LO->tail_pos++;
		}

		if (iterator->right) 
		{
			LO->queue[LO->tail_pos] = iterator->right;
			LO->tail_pos++;
		}

		// Dequeue --- set to iterator
		iterator = LO->queue[LO->head_pos];
		LO->head_pos++;
	}
 
	ACCESS_WRAPPER = 0; 
	
	// Not freeing --- FIX

	return;
}

void bt_lo_traversal_2(void *i, void **o)
{
  	nk_fiber_set_vc(vc);

	// Allocate and set up queue for level-order traversal
	TreeQueue_t *LO = createQueue();

	// Allocate and set up tree --- Start = 4720, 800 nodes
	TreeNode_t *tree = createTree(4720, 800);

	// Set up iterator for traversal
	TreeNode_t *iterator = tree;
	volatile uint64_t sum = 0;

	ACCESS_WRAPPER = 1;

	// Calculate sum of nodes via level-order traversal
	while (iterator != NULL)
	{
		// Accumulate
		sum += iterator->value;

		if ((LO->tail_pos == MAX_QUEUE_SIZE) || (LO->head_pos > LO->tail_pos)) { break; }

		// Enqueue children of iterator
		if (iterator->left) 
		{
			LO->queue[LO->tail_pos] = iterator->left;
			LO->tail_pos++;
		}

		if (iterator->right) 
		{
			LO->queue[LO->tail_pos] = iterator->right;
			LO->tail_pos++;
		}

		// Dequeue --- set to iterator
		iterator = LO->queue[LO->head_pos];
		LO->head_pos++;
	}
 

	ACCESS_WRAPPER = 0; 
  
	_nk_fiber_print_data();
	
	// Not freeing --- FIX

	return;
}
// ------


// ------
// Benchmark 9 --- floating point, integer, casting operations (operations_1, operations_2)
void operations_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	SEED();

	uint64_t count = (lrand48() % 10000), c, d;
	volatile double sum = 0;
	volatile double a = 44.32, b = 2.29;

	ACCESS_WRAPPER = 1;

	// Do computation
	for (c = 0; c < 40; c++)
	{
		for (d = 0; d < 30; d++)
		{
			a += b * (count / b);
			b += (a * sum) / (sum * 20.3);
			sum += (a / b);
		}  

		sum /= (count / a);
	}

	ACCESS_WRAPPER = 0;

	return;
}

void operations_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	SEED();

	uint64_t count = (lrand48() % 10000), c, d;
	volatile double sum = 0;
	volatile double a = 47.11, b = 5.2789;

	ACCESS_WRAPPER = 1;

	uint64_t start = rdtsc();

	// Do computation
	for (c = 0; c < 40; c++)
	{
		for (d = 0; d < 30; d++)
		{
			a += b * (count / b);
			b += (a * sum) / (sum * 16.2);
			sum += (a / b);
		}  

		sum /= (count / b);
	}
	
	uint64_t finish = rdtsc();

	ACCESS_WRAPPER = 0;

	_nk_fiber_print_data();
	
	nk_vc_printf("finish - start: %lu\n", finish - start);

	return;
}
// ------


// ------
// Benchmark 10  --- time_hook data collection, only one fiber
#define TH 10000
extern int ACCESS_HOOK;
void time_hook_test(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	int a = 0;

	ACCESS_HOOK = 1;

	while(a < TH)
	{
		nk_simple_timing_loop(200);
		a++;
	}

	ACCESS_HOOK = 0;

	get_time_hook_data();

	return;
}
// ------


// ------
// Benchmark 11 --- encryption with rijndael (rijndael_1, rijndael_2)
// NOTE --- suspiciously written test case, need to update
void rijndael_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	int a = 0, bit_size = 256;
	
	// Allocate all pieces necessary for rijndael implementation
	// NOTE --- relying on error checking from MALLOC
	unsigned len = (unsigned) bit_size;
	unsigned char *key = (unsigned char *) (MALLOC(sizeof(unsigned char) * bit_size));
	unsigned char *src = (unsigned char *) (MALLOC(sizeof(unsigned char) * bit_size));
	unsigned char *dst = (unsigned char *) (MALLOC(sizeof(unsigned char) * bit_size));
	rijndael_ctx *new_ctx = (rijndael_ctx *) (MALLOC(sizeof(rijndael_ctx)));
	
	ACCESS_WRAPPER = 1;
	
	while(a < M)
	{
		// Set key and src to random set of bytes
		nk_get_rand_bytes(key, len);
		nk_get_rand_bytes(src, len);

		// Set up context struct with key	
		rijndael_set_key(new_ctx, key, bit_size, 1); // Set up for encryption

		// Encrypt the src, place into dst
		rijndael_encrypt(new_ctx, src, dst);

		// Decrypt the dst, place into src
		rijndael_decrypt(new_ctx, src, dst);
		
		a++;
	}

	ACCESS_WRAPPER = 0;
	
	// No freeing --- FIX
	
	return;
}

void rijndael_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	int a = 0, bit_size = 256;
	
	// Allocate all pieces necessary for rijndael implementation
	// NOTE --- relying on error checking from MALLOC
	unsigned len = (unsigned) bit_size;
	unsigned char *key = (unsigned char *) (MALLOC(sizeof(unsigned char) * bit_size));
	unsigned char *src = (unsigned char *) (MALLOC(sizeof(unsigned char) * bit_size));
	unsigned char *dst = (unsigned char *) (MALLOC(sizeof(unsigned char) * bit_size));
	rijndael_ctx *new_ctx = (rijndael_ctx *) (MALLOC(sizeof(rijndael_ctx)));

	ACCESS_WRAPPER = 1;
	
	while(a < M)
	{
		// Set key and src to random set of bytes
		nk_get_rand_bytes(key, len);
		nk_get_rand_bytes(src, len);

		// Set up context struct with key	
		rijndael_set_key(new_ctx, key, bit_size, 1); // Set up for encryption

		// Encrypt the src, place into dst
		rijndael_encrypt(new_ctx, src, dst);

		// Decrypt the dst, place into src
		rijndael_decrypt(new_ctx, src, dst);
		
		a++;
	}

	ACCESS_WRAPPER = 0;
	
	_nk_fiber_print_data();
	
	// No freeing --- FIX

	return;
}
// ------


// ------
// Benchmark 12 --- hashing with SHA-1 (sha1_1, sha1_2)
void sha1_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	const int sha_output_size = 20;
	int a = 0, input_size = 192; // 192 byte input
	
	ACCESS_WRAPPER = 1;
	
	// Allocate all pieces necessary for SHA-1 implementation
	// NOTE --- relying on error checking from MALLOC
	unsigned char *input = (unsigned char *) (MALLOC(sizeof(unsigned char) * input_size));
	unsigned char *output = (unsigned char *) (MALLOC(sizeof(unsigned char) * sha_output_size));
	
	while(a < M)
	{
		// Set input to random bytes
		nk_get_rand_bytes(input, input_size);

		// Generate hash
		sha1(input, input_size, output);

		a++;
	}

	ACCESS_WRAPPER = 0;
	
	// No freeing --- FIX
	
	return;
}

void sha1_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	const int sha_output_size = 20;
	int a = 0, input_size = 192; // 192 byte input
	
	ACCESS_WRAPPER = 1;
	
	// Allocate all pieces necessary for SHA-1 implementation
	// NOTE --- relying on error checking from MALLOC
	unsigned char *input = (unsigned char *) (MALLOC(sizeof(unsigned char) * input_size));
	unsigned char *output = (unsigned char *) (MALLOC(sizeof(unsigned char) * sha_output_size));

	while(a < M)
	{
		// Set input to random bytes
		nk_get_rand_bytes(input, input_size);

		// Generate hash
		sha1(input, input_size, output);

		a++;
	}

	ACCESS_WRAPPER = 0;
	
	_nk_fiber_print_data();
	
	// No freeing --- FIX
	
	return;
}
// ------


// ------
// Benchmark 13 --- hashing with MD5 (md5_1, md5_2)
void md5_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	const int md5_output_size = 16;
	int a = 0, input_size = 192; // 192 byte input
	
	ACCESS_WRAPPER = 1;
	
	// Allocate all pieces necessary for SHA-1 implementation
	// NOTE --- relying on error checking from MALLOC
	unsigned char *input = (unsigned char *) (MALLOC(sizeof(unsigned char) * input_size));
	unsigned char *output = (unsigned char *) (MALLOC(sizeof(unsigned char) * md5_output_size));
	
	while(a < M)
	{
		// Set input to random bytes
		nk_get_rand_bytes(input, input_size);

		// Generate hash
		md5(input, input_size, output);

		a++;
	}

	ACCESS_WRAPPER = 0;
	
	// No freeing --- FIX
	
	return;
}

void md5_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	const int md5_output_size = 16;
	int a = 0, input_size = 192; // 192 byte input
	
	ACCESS_WRAPPER = 1;
	
	// Allocate all pieces necessary for SHA-1 implementation
	// NOTE --- relying on error checking from MALLOC
	unsigned char *input = (unsigned char *) (MALLOC(sizeof(unsigned char) * input_size));
	unsigned char *output = (unsigned char *) (MALLOC(sizeof(unsigned char) * md5_output_size));
	
	while(a < M)
	{
		// Set input to random bytes
		nk_get_rand_bytes(input, input_size);

		// Generate hash
		md5(input, input_size, output);

		a++;
	}

	ACCESS_WRAPPER = 0;
	
	_nk_fiber_print_data();
	
	// No freeing --- FIX
	
	return;
}
// ------


// ------
// Benchmark 14 --- fibonacci via dynamic programming (fib_1, fib_2)
void fib_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);
	
	int a = 0;
	SEED();

	ACCESS_WRAPPER = 1;

	while (a < M)
	{
		uint64_t fib_num = lrand48() % 75;
		
		// Allocate table on stack
		uint64_t memo[fib_num + 2];
		memset(memo, 0, sizeof(memo));
		
		// Set base cases
		memo[0] = 0;
		memo[1] = 1;

		uint64_t k;
		for (k = 0; k < fib_num; k++) {
			memo[k] = memo[k - 1] + memo[k - 2];
		}
	
		a++;
	}
	
	ACCESS_WRAPPER = 0;
	
	return;
}

void fib_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);
		
	int a = 0;
	SEED();

	ACCESS_WRAPPER = 1;

	while (a < M)
	{
		uint64_t fib_num = lrand48() % 75;
		
		// Allocate table on stack
		uint64_t memo[fib_num + 2];
		memset(memo, 0, sizeof(memo));

		// Set base cases
		memo[0] = 0;
		memo[1] = 1;

		uint64_t k;
		for (k = 0; k < fib_num; k++) {
			memo[k] = memo[k - 1] + memo[k - 2];
		}
	
		a++;
	}

	ACCESS_WRAPPER = 0;
	
	_nk_fiber_print_data();
	
	return;
}
// ------


// ------
// Benchmark 15 --- knn classifier (knn_1, knn_2)
void knn_1(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	// Declare parameters
	uint32_t k = 3, dims = 5, num_ex = 20;
	
	ACCESS_WRAPPER = 1;
	
	// Set up classifier
	struct KNNContext *ctx = KNN_build_context(k, dims, num_ex, distance_manhattan, aggregate_median);

	// Build a new point to classify
	struct KNNPoint *kp = KNN_build_point(dims);

	// Classify the point
	double classification = KNN_classify(kp, ctx);

	// Clean up
	KNN_point_destroy(kp);
	KNN_context_destroy(ctx);	

	ACCESS_WRAPPER = 0;
	
	return;
}

void knn_2(void *i, void **o)
{
	nk_fiber_set_vc(vc);

	// Declare parameters
	uint32_t k = 3, dims = 5, num_ex = 20;
	
	ACCESS_WRAPPER = 1;
	
	// Set up classifier
	struct KNNContext *ctx = KNN_build_context(k, dims, num_ex, distance_manhattan, aggregate_median);

	// Build a new point to classify
	struct KNNPoint *kp = KNN_build_point(dims);

	// Classify the point
	double classification = KNN_classify(kp, ctx);

	// Clean up
	KNN_point_destroy(kp);
	KNN_context_destroy(ctx);	

	ACCESS_WRAPPER = 0;
	
	_nk_fiber_print_data();

	nk_vc_printf("class: %f\n", classification);

	return;
}
// ------
// --------------------------------------------------------------------------------


/******************* Test Wrappers *******************/

int test_fibers_bench(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(timing_loop_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(timing_loop_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench2(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(s_timing_loop_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(s_timing_loop_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench3(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(dot_prod_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(dot_prod_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench4(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(ll_traversal_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(ll_traversal_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench5(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(mm_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(mm_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

// Possible barriers test
#define BARRIER_USE 0
int test_fibers_bench6(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  
#if BARRIER_USE

  // set up barrier
  nk_counting_barrier_t *bar = MALLOC(sizeof(nk_counting_barrier_t));
  nk_counting_barrier_init(bar, 2); // two fibers

  nk_fiber_create(bt_traversal_1, 0, 0, FSTACK_2MB, &simple1);
  nk_fiber_create(bt_traversal_2, 0, 0, FSTACK_2MB, &simple2);
  nk_fiber_run(simple1, F_CURR_CPU);
  nk_fiber_run(simple2, F_CURR_CPU);

#else
  
  // nk_fiber_start(bt_traversal_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(bt_traversal_2, 0, 0, FSTACK_2MB, 0, &simple2);

#endif
  
  return 0;

}

int test_fibers_bench7(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(rand_mm_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(rand_mm_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench8(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(bt_lo_traversal_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(bt_lo_traversal_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench9(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(operations_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(operations_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench10(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(time_hook_test, 0, 0, FSTACK_2MB, 0, &simple1);
  return 0;
}

int test_fibers_bench11(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(rijndael_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(rijndael_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench12(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(sha1_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(sha1_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench13(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(md5_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(md5_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench14(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(fib_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(fib_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}

int test_fibers_bench15(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(knn_1, 0, 0, FSTACK_2MB, 0, &simple1);
  nk_fiber_start(knn_2, 0, 0, FSTACK_2MB, 0, &simple2);
  return 0;
}


// --------------------------------------------------------------------------------

/******************* Test Handlers *******************/

static int
handle_fibers1 (char * buf, void * priv)
{
  test_fibers_bench();
  return 0;
}

static int
handle_fibers2 (char * buf, void * priv)
{
  test_fibers_bench2();
  return 0;
}

static int
handle_fibers3 (char * buf, void * priv)
{
  test_fibers_bench3();
  return 0;
}

static int
handle_fibers4 (char * buf, void * priv)
{
  test_fibers_bench4();
  return 0;
}

static int
handle_fibers5 (char * buf, void * priv)
{
  test_fibers_bench5();
  return 0;
}

static int
handle_fibers6 (char * buf, void * priv)
{
  test_fibers_bench6();
  return 0;
}

static int
handle_fibers7 (char * buf, void * priv)
{
  test_fibers_bench7();
  return 0;
}

static int
handle_fibers8 (char * buf, void * priv)
{
  test_fibers_bench8();
  return 0;
}

static int
handle_fibers9 (char * buf, void * priv)
{
  test_fibers_bench9();
  return 0;
}

static int
handle_fibers10 (char * buf, void * priv)
{
  test_fibers_bench10();
  return 0;
}

static int
handle_fibers11 (char * buf, void * priv)
{
  test_fibers_bench11();
  return 0;
}

static int
handle_fibers12 (char * buf, void * priv)
{
  test_fibers_bench12();
  return 0;
}

static int
handle_fibers13 (char * buf, void * priv)
{
  test_fibers_bench13();
  return 0;
}

static int
handle_fibers14 (char * buf, void * priv)
{
  test_fibers_bench14();
  return 0;
}

static int
handle_fibers15 (char * buf, void * priv)
{
  test_fibers_bench15();
  return 0;
}


// --------------------------------------------------------------------------------

static struct shell_cmd_impl fibers_impl1 = {
  .cmd      = "fiberbench1",
  .help_str = "fiberbench1",
  .handler  = handle_fibers1,
};

static struct shell_cmd_impl fibers_impl2 = {
  .cmd      = "fiberbench2",
  .help_str = "fiberbench2",
  .handler  = handle_fibers2,
};

static struct shell_cmd_impl fibers_impl3 = {
  .cmd      = "fiberbench3",
  .help_str = "fiberbench3",
  .handler  = handle_fibers3,
};

static struct shell_cmd_impl fibers_impl4 = {
  .cmd      = "fiberbench4",
  .help_str = "fiberbench4",
  .handler  = handle_fibers4,
};

static struct shell_cmd_impl fibers_impl5 = {
  .cmd      = "fiberbench5",
  .help_str = "fiberbench5",
  .handler  = handle_fibers5,
};

static struct shell_cmd_impl fibers_impl6 = {
  .cmd      = "fiberbench6",
  .help_str = "fiberbench6",
  .handler  = handle_fibers6,
};

static struct shell_cmd_impl fibers_impl7 = {
  .cmd      = "fiberbench7",
  .help_str = "fiberbench7",
  .handler  = handle_fibers7,
};

static struct shell_cmd_impl fibers_impl8 = {
  .cmd      = "fiberbench8",
  .help_str = "fiberbench8",
  .handler  = handle_fibers8,
};

static struct shell_cmd_impl fibers_impl9 = {
  .cmd      = "fiberbench9",
  .help_str = "fiberbench9",
  .handler  = handle_fibers9,
};

static struct shell_cmd_impl fibers_impl10 = {
  .cmd      = "timehook1",
  .help_str = "timehook1",
  .handler  = handle_fibers10,
};

static struct shell_cmd_impl fibers_impl11 = {
  .cmd      = "fiberbench11",
  .help_str = "fiberbench11",
  .handler  = handle_fibers11,
};

static struct shell_cmd_impl fibers_impl12 = {
  .cmd      = "fiberbench12",
  .help_str = "fiberbench12",
  .handler  = handle_fibers12,
};

static struct shell_cmd_impl fibers_impl13 = {
  .cmd      = "fiberbench13",
  .help_str = "fiberbench13",
  .handler  = handle_fibers13,
};

static struct shell_cmd_impl fibers_impl14 = {
  .cmd      = "fiberbench14",
  .help_str = "fiberbench14",
  .handler  = handle_fibers14,
};

static struct shell_cmd_impl fibers_impl15 = {
  .cmd      = "fiberbench15",
  .help_str = "fiberbench15",
  .handler  = handle_fibers15,
};


// --------------------------------------------------------------------------------

/******************* Shell Commands *******************/

nk_register_shell_cmd(fibers_impl1);
nk_register_shell_cmd(fibers_impl2);
nk_register_shell_cmd(fibers_impl3);
nk_register_shell_cmd(fibers_impl4);
nk_register_shell_cmd(fibers_impl5);
nk_register_shell_cmd(fibers_impl6);
nk_register_shell_cmd(fibers_impl7);
nk_register_shell_cmd(fibers_impl8);
nk_register_shell_cmd(fibers_impl9);
nk_register_shell_cmd(fibers_impl10);
nk_register_shell_cmd(fibers_impl11);
nk_register_shell_cmd(fibers_impl12);
nk_register_shell_cmd(fibers_impl13);
nk_register_shell_cmd(fibers_impl14);
nk_register_shell_cmd(fibers_impl15);




