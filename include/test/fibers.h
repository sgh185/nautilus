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
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Souradip Ghosh <sgh@u.northwestern.edu>
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
 * Data structures for subset of microbenchmarks used for 
 * compiler-timing testing via fibers
 */ 

#ifndef __TEST_FIBERS_H__
#define __TEST_FIBERS_H__

#include <nautilus/nautilus.h>
#include <nautilus/dynarray.h>
#define TEST_MALLOC(n) ({void *__p = malloc(n); if (!__p) { panic("Malloc failed\n"); } __p;})

typedef struct NodeTy Node_t;
typedef struct TreeNode TreeNode_t;

// Custom linked list
typedef struct NodeTy
{
  uint64_t value;
  Node_t *next;
} Node_t;

typedef struct LinkedList
{
  Node_t *head;
  Node_t *tail;
} List_t;

// Custom binary tree 
typedef struct TreeNode
{
  uint64_t value;
  TreeNode_t *left;
  TreeNode_t *right;
} TreeNode_t;

// Custom queue structure for BT 
typedef struct TreeQueue
{
  TreeNode_t **queue;
  uint64_t head_pos; // Index of first tree node
  uint64_t tail_pos; // Index of next available entry to add a tree node to queue
} TreeQueue_t;

typedef struct Vertex_t Vertex;

typedef struct Vertex_t
{
	nk_dynarray_uint32_t *neighbor_ids;
	int *neighbor_weights;
	uint32_t id;
} Vertex;

typedef struct Graph_t
{
	uint32_t num_vertices;
	uint32_t num_edges;
	Vertex **vertices;
} Graph;

// Linked-list set up
List_t *createList(uint64_t start, uint64_t size);

// Binary tree set up
TreeNode_t *createTree(uint64_t start, uint64_t size);

TreeQueue_t *createQueue(void);

// Undirected graph implementation --- new (complicated)
// so there's a lot of pointer traversal

// Set up
Graph *generate_full_graph(uint32_t num_vtx, int weighted);

Vertex *build_new_vertex(uint32_t id, int max_neighbors);
Graph *build_new_graph(uint32_t num_vtx);
Vertex *copy_vertex(Vertex *vtx, int max_neighbors);
int add_vertex_to_graph(Vertex *vtx, Graph *g);
void add_edge(Vertex *a, Vertex *b, int weight);
void build_rand_edges(Vertex *vtx, Graph *g, int new_edges, int weighted);

// Clean up
void destroy_vertex(Vertex *vtx);
void destroy_vertex_array(Vertex **vertices, uint32_t length);
void destroy_graph(Graph *g);

// Graph utility
int check_vertex(Vertex *vtx);
int inline max_edges(Graph *g);
void print_graph(Graph *g);

// Analysis
int detect_cycles(Graph *g);
void build_mst_unweighted(Graph *g);
void dijkstra(Graph *g);

// Utility
uint64_t *createRandArray(uint64_t size);

// Sorting --- qsort
#define _swap_helper(a, b) ({ \
	__auto_type temp = *a; \
	*a = *b; \
	*b = temp; \
}) \

#define _partition_helper(arr, low, high) ({ \
	int pivot = low - 1, i; \
	__auto_type high_val = arr[high]; \
	\
	for (i = low; i <= high - 1; i++) \
	{ \
		if (arr[i] <= high_val) \
		{ \
			pivot++; \
			_swap_helper(&arr[pivot], &arr[i]); \
		} \
	} \
	\
	pivot++; \
	_swap_helper(&arr[pivot], &arr[high]); \
	\
	pivot; \
})

#define QSORT_DECL(type) \
	static void _quicksort_internal_##type(type *arr, int low, int high) \
	{ \
		if (low < high) \
		{ \
			int pivot = _partition_helper(arr, low, high); \
			_quicksort_internal_##type(arr, low, pivot - 1); \
			_quicksort_internal_##type(arr, pivot + 1, high); \
		} \
		return; \
	}


#define quicksort_driver(arr, len, type) ({ \
	type *arr_copy = (type *) (TEST_MALLOC(sizeof(type) * len)); \
	\
	int i; \
	for (i = 0; i < len; i++) { arr_copy[i] = arr[i]; } \
	\
	_quicksort_internal_##type(arr_copy, 0, len - 1); \
	arr_copy; \
})

QSORT_DECL(int);
QSORT_DECL(sint64_t);
QSORT_DECL(uint32_t);

// Sorting --- MSB radix sort 
#define INT8_MIN (-128) 
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483647 - 1)
#define INT64_MIN  (-9223372036854775807LL - 1)

#define RADIX_DECL(type) \
	static void _radix_sort_##type(type *from, type *to, type bit) \
	{ \
		if (!bit || (to < from + 1)) { return; } \
		type *ll = from, *rr = to - 1; \
		while (1) \
		{ \
			while ((ll < rr) && (!(*ll & bit))) { ll++; } \
			while ((ll < rr) && (*rr & bit)) { rr--; } \
			if (ll >= rr) break; \
			_swap_helper(ll, rr); \
		} \
		\
		if ((!(bit & *ll)) && (ll < to)) { ll++; } \
		bit >>= 1; \
		\
		_radix_sort_##type(from, ll, bit); \
		_radix_sort_##type(ll, to, bit); \
	}

#define radix_driver(arr, len, bit_width) ({ \
	size_t i; \
	sint##bit_width##_t bw = INT##bit_width##_MIN; \
	uint##bit_width##_t *x = (uint##bit_width##_t *) (arr); \
	for (i = 0; i < len; i++) { x[i] ^= INT##bit_width##_MIN; } \
	_radix_sort_uint##bit_width##_t(x, x + len, bw); \
	for (i = 0; i < len; i++) { x[i] ^= INT##bit_width##_MIN; } \
})

RADIX_DECL(uint8_t);
RADIX_DECL(uint16_t);
RADIX_DECL(uint32_t);
RADIX_DECL(uint64_t);

#define RAND_MAGIC 0x7301
#define gen_rand_array(type, len, neg) ({ \
	srand48(rdtsc() % RAND_MAGIC); \
	type *rand_array = (type *) (TEST_MALLOC(sizeof(type) * len)); \
	size_t i; \
	for (i = 0; i < len; i++) \
	{ \
		type num = (type) (lrand48()), \
	   		 sign = ((num % 2) || (!(neg))) ? 1 : -1; \
		rand_array[i] = num * sign; \
	} \
	rand_array; \
})

#endif
