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
 * Undirected graph implementation (with heavy use on pointer traversal). Also
 * includes graph traversal, spanning tree, cycle detection algorithms, etc.
 * Used as a microbenchmark for the compiler-timing model running on fibers
 */ 

#include <nautilus/nautilus.h>
#include <test/fibers.h>
#include <nautilus/random.h>
#include <nautilus/libccompat.h>
#include <nautilus/stack.h>

// Useful macros
#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )
#define MAX(a, b) ( ((a) > (b)) ? (a) : (b) )
#define ABS(a) ( ((a) < 0) ? (-(a)) : (a) )
#define LEN(arr) ( (sizeof(arr)) / (sizeof(arr[0])) )
#define MALLOC(n) ({void *__p = malloc(n); if (!__p) { panic("Malloc failed\n"); } __p;})
#define SEED() (srand48(rdtsc() % 128))
#define MAX_WEIGHT 32

// ---------------- Undirected graph implementation ----------------

Vertex *build_new_vertex(uint32_t id, int max_neighbors)
{
	// Allocate vertex	
	Vertex *new_vertex = (Vertex *) (MALLOC(sizeof(Vertex)));
	
	// Set fields
	new_vertex->id = id;
	new_vertex->num_neighbors = 0;
	new_vertex->neighbor_ids = (uint32_t *) (MALLOC(sizeof(uint32_t) * max_neighbors));
	new_vertex->neighbor_weights = (int *) (MALLOC(sizeof(int) * max_neighbors));
	new_vertex->neighbors = (Vertex **) (MALLOC(sizeof(Vertex *) * max_neighbors));

	// Zero initialize
	memset(new_vertex->neighbor_ids , -1, (sizeof(uint32_t) * max_neighbors));
	memset(new_vertex->neighbor_weights, 0, (sizeof(int) * max_neighbors));
	memset(new_vertex->neighbors, 0, (sizeof(Vertex *) * max_neighbors));

	return new_vertex;	
}

Vertex *copy_vertex(Vertex *vtx, int max_neighbors)
{	
	// Allocate new vertex for copy
	Vertex *new_vertex = (Vertex *) (MALLOC(sizeof(Vertex)));
	
	// Set fields accordingly
	new_vertex->id = vtx->id;
	new_vertex->num_neighbors = vtx->num_neighbors;
	new_vertex->neighbor_ids = (uint32_t *) (MALLOC(sizeof(uint32_t) * max_neighbors));
	new_vertex->neighbor_weights = (int *) (MALLOC(sizeof(int) * max_neighbors));
	new_vertex->neighbors = (Vertex **) (MALLOC(sizeof(Vertex *) * max_neighbors));

	// Zero initialize
	memset(new_vertex->neighbor_ids , 0, (sizeof(uint32_t) * max_neighbors));
	memset(new_vertex->neighbor_weights, 0, (sizeof(int) * max_neighbors));
	memset(new_vertex->neighbors, 0, (sizeof(Vertex *) * max_neighbors));

	// avoiding memcpy
	int i, n_n = (int) (vtx->num_neighbors);
	for (i = 0; i < n_n; i++) {
		uint32_t next_id = vtx->neighbor_ids[i];
		new_vertex->neighbors[next_id] = vtx->neighbors[next_id];
		new_vertex->neighbor_weights[next_id] = vtx->neighbor_weights[next_id];
	}

	return new_vertex;

}

Graph *build_new_graph(uint32_t num_vtx)
{
	// Allocate graph
	Graph *new_graph = (Graph *) (MALLOC(sizeof(Graph)));

	// Set num_vertices, allocate vertex IDs array, verticies array
	new_graph->num_vertices = num_vtx; // Set prior (probably bad design)
	new_graph->num_edges = 0; // Will increment as edges are added
	new_graph->vertices = (Vertex **) (MALLOC(sizeof(Vertex *) * num_vtx));

	// Zero initialize
	memset(new_graph->vertices, 0, (sizeof(Vertex *) * num_vtx));

	print_graph(new_graph);

	return new_graph;
}

int add_vertex_to_graph(Vertex *vtx, Graph *g)
{
	// Quick error checking
	if (!(check_vertex(vtx))) { return 0; }

	// Vertex with ID already exists
	if (g->vertices[vtx->id]) { return 0; } 	
	
	// Now add it to the graph
	g->vertices[vtx->id] = vtx;

	return 1;	
}

void build_rand_edges(Vertex *vtx, Graph *g, int new_edges, int weighted)
{
	// Quick error checking
	if (!(check_vertex(vtx))) { return; }
	
	// Check if we're at max edges
	if (g->num_edges >= max_edges(g)) { return; }

	// If the number of neighbors that the vertex has 
	// is the same as the number of possible vertices, 
	// then we can't add any more neighbors to the vertex
	if (vtx->num_neighbors == g->num_vertices) { return; }

	int i = 0;
	
	SEED();
	
	// Tricky loop b/c of then number of internal branches
	while (i < new_edges)
	{
		// Checks (quite unlikely but precautionary
		// because I tend to write bugs at 4am)
	
		// Find a random id (of a vertex) to build an
		// edge with, depending on conditions
	
		uint32_t new_id = (uint32_t) (lrand48() % (g->num_vertices));
		
		// Don't allow self edges --- HACK
		if (new_id == vtx->id) { continue; }  
		
		Vertex *potential_neighbor = g->vertices[new_id];
		
		// If the edge is already there at position new_id
		// (and vice versa)
		if ((vtx->neighbors[new_id] == potential_neighbor) 
			|| (potential_neighbor->neighbors[vtx->id] == vtx)) { continue; }
		
  
		// Check if we're at max neighbors for potential neighbors
		if (potential_neighbor->num_neighbors == g->num_vertices) { continue; }	
		
		// Now we're ready to add --- set the corresponding
		// positions in each neighbors array to the pointer
		// to the respective vertices set the weights correctly,
		// increment information counters
		potential_neighbor->neighbors[vtx->id] = vtx;
		potential_neighbor->neighbor_ids[potential_neighbor->num_neighbors] = vtx->id;
		vtx->neighbors[new_id] = potential_neighbor;
		vtx->neighbor_ids[vtx->num_neighbors] = new_id;

		int weight;
		if (!weighted) { weight = 1; }
		else { SEED(); weight = (int) (lrand48() % MAX_WEIGHT); } 
		
		potential_neighbor->neighbor_weights[vtx->id] = weight;
		vtx->neighbor_weights[new_id] = weight;

		potential_neighbor->num_neighbors++;
		vtx->num_neighbors++;

		g->num_edges++;

		i++;

		// Check if we're at max edges
		if (g->num_edges >= max_edges(g)) { return; }
		
		// Check if vtx is at max neighbors
		if (vtx->num_neighbors == g->num_vertices) { return; }
	}

	return; 
}

Graph *generate_full_graph(uint32_t num_vtx, int weighted)
{
	// Build the shell
	Graph *g = build_new_graph(num_vtx);
	
	// Populate with vertices
	uint32_t i;
	for (i = 0; i < g->num_vertices; i++) {
		g->vertices[i] = build_new_vertex(i, g->num_vertices);
	}

	// Build edges --- (for now, keep it "sparse" and connected,
	// between 1 and 6 neighbors)
	for (i = 0; i < g->num_vertices; i++) 
	{
		// SEED();
		int num_neigh = (int) ((lrand48() % 5) + 1);
		build_rand_edges(g->vertices[i], g, num_neigh, weighted);
	}


	// Return full graph
	return g;
}

// Cleanup
void destroy_vertex(Vertex *vtx)
{
	if (!check_vertex(vtx)) { return; }
	free(vtx->neighbor_ids);
	free(vtx->neighbor_weights);
	free(vtx->neighbors); // Freeing the actual vertices in
						  // the grph will cause recursion
						  // and invalid pointer frees
	free(vtx);

	return;
}

void destroy_vertex_array(Vertex **vertices, uint32_t length)
{
	int i;
	for (i = 0; i < length; i++) {
		destroy_vertex(vertices[i]);
	}

	free(vertices);

	return;
}

void destroy_graph(Graph *g)
{
	destroy_vertex_array(g->vertices, g->num_vertices);
	free(g);

	return;	
}


// Graph utility
__attribute__((always_inline)) int inline check_vertex(Vertex *vtx)
{
	if (!vtx) { return 0; }
	if (!(vtx->neighbor_ids)) { return 0; }
	if (!(vtx->neighbor_weights)) { return 0; }
	if (!(vtx->neighbors)) { return 0; }

	return 1;	
}

__attribute__((always_inline)) int inline max_edges(Graph *g)
{
	return (g->num_vertices * (g->num_vertices - 1)) / 2;
}

void print_graph(Graph *g)
{
	int i;
	for (i = 0; i < g->num_vertices; i++)
	{
		nk_vc_printf("Next ID: %d\n", i);
		
		Vertex *curr = g->vertices[i];
		int j;
		for (j = 0; j < curr->num_neighbors; j++) {
			nk_vc_printf("%d ", curr->neighbor_ids[j]);
		}

		nk_vc_printf("\n\n");		
	}

	return;
}



// ------------------------------------------
// Graph algorithms

// Cycle detection
NK_STACK_DECL(uint32_t);

// Returns the first cycle that detect_cycles finds
// or returns NULL
int detect_cycles(Graph *g)
{
	int max_cycle_size = g->num_vertices;

	// Pick a random starting point
	SEED();
	Vertex *start_vtx = g->vertices[(lrand48() % g->num_vertices)];
	if (!(check_vertex(start_vtx))) { return 0; }

	// Build the necessary data structures
	
	// Tracking data structures --- bitsets better and structures can be optimized, but this is easy
	uint32_t visited_ids[g->num_vertices], stack_ids[g->num_vertices], adder_ids[g->num_vertices];
	memset(visited_ids, 0, sizeof(visited_ids)); // All visited vertices 
	memset(stack_ids, 0, sizeof(stack_ids)); // All vertices that are currently in the stack --- easier than parsing the stack
	memset(adder_ids, -1, sizeof(adder_ids)); // The mapping between a vertex and the neighbor vertex that added it to the stack
											  // This is a hack to prevent "cycles of 2" in an undirected graph --- makes no sense

	// Stack for DFS
	nk_stack_uint32_t *visit_stack = nk_stack_get(uint32_t); 

	// Setup --- add first vertex to data structures
	nk_stack_push(uint32_t, visit_stack, start_vtx->id);
	visited_ids[start_vtx->id] |= 1;
	stack_ids[start_vtx->id] |= 1;

	int cycle_found = 0;

	// DFS
	do
	{
		// Get the next vertex ID and vertex pointer off the stack
		uint32_t curr_id = nk_stack_pop(uint32_t, visit_stack);
		Vertex *curr_vertex = g->vertices[curr_id];

		// Loop through the neighbors of the vertices 
		uint32_t neigh_id;
		int i, new_id = 0;
		for (i = 0; i < (curr_vertex->num_neighbors); i++)
		{
			neigh_id = curr_vertex->neighbor_ids[i];
		
			// If we found a vertex that hasn't been visited --- record
			// it aand break --- essential for DFS	
			if (!(visited_ids[neigh_id]))
			{
				new_id |= 1;
				visited_ids[neigh_id] |= 1;
				break;
			}

			// If the neighbor is already in the stack, and the neighbor
			// wasn't the one to add the current vertex to the stack, we've
			// found a cycle
			if ((stack_ids[neigh_id])
				&& (adder_ids[curr_id] != neigh_id))
			{
				cycle_found |= 1;
				break;
			}
		}
		
		// Push the current vertex back to the stack and exit DFS
		if (cycle_found) 
		{ 
			nk_stack_push(uint32_t, visit_stack, curr_id);
			nk_stack_push(uint32_t, visit_stack, neigh_id);
			break;
		}

		// If we have a new vertex to examine, push the current
		// vertex back to the stack, THEN push the new neighbor
		// vertex to examine --- mark the new vertex as currently
		// in the stack, and mark the adder mapping accordinglu 
		if (new_id)
		{
			nk_stack_push(uint32_t, visit_stack, curr_id);
			nk_stack_push(uint32_t, visit_stack, neigh_id);
			stack_ids[neigh_id] |= 1;
			adder_ids[neigh_id] = curr_id;
		}
		else { stack_ids[curr_id] ^= 1; } // Mark the current vertex as 
										  // removed from the stack

	} while (!(nk_stack_empty(visit_stack)));
	
	// We've found a cycle --- to figure out what the cycle is, pop off the 
	// first vertex ID from the stack, and look for the next occurrence
	// of that vertex ID in the stack --- that chain will be the cycle
	if (!cycle_found) 
	{ 
		nk_stack_destroy(visit_stack);	
		return 0; 
	}

#if 0
	uint32_t start_id = nk_stack_pop(uint32_t, visit_stack), next_id;

	do 
	{
		next_id = nk_stack_pop(uint32_t, visit_stack);
		nk_vc_printf("Next ID: %d\n", next_id);
	} while ((start_id != next_id) && (!(nk_stack_empty(visit_stack))));
#endif

	nk_stack_destroy(visit_stack);	

	return 1;
}
