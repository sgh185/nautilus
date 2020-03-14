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

// Useful macros
#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )
#define MAX(a, b) ( ((a) > (b)) ? (a) : (b) )
#define ABS(a) ( ((a) < 0) ? (-(a)) : (a) )
#define LEN(arr) ( (sizeof(arr)) / (sizeof(arr[0])) )
#define MALLOC(n) ({void *__p = malloc(n); if (!__p) { panic("Malloc failed\n"); } __p;})
#define SEED() (srand48(rdtsc() % 128))
#define MAX_WEIGHT 32

// ---------------- Undirected graph implementation ----------------
// Made in a complicated way --- lots of pointer traversal


Vertex *build_new_vertex(int id, int max_neighbors)
{
	// Allocate vertex	
	Vertex *new_vertex = (Vertex *) (MALLOC(sizeof(Vertex)));
	
	// Set fields
	new_vertex->id = id;
	new_vertex->num_neighbors = 0;
	new_vertex->neighbors = (Vertex **) (MALLOC(sizeof(Vertex *) * max_neighbors));
	new_vertex->neighbor_weights = (int *) (MALLOC(sizeof(int) * max_neighbors));

	// Zero initialize
	memset(new_vertex->neighbors, 0, sizeof(*(new_vertex->neighbors)));
	memset(new_vertex->neighbor_weights, 0, sizeof(*(new_vertex->neighbor_weights)));

	return new_vertex;	
}

Vertex *copy_vertex(Vertex *vtx)
{	
	// Allocate new vertex for copy
	Vertex *new_vertex = (Vertex *) (MALLOC(sizeof(Vertex)));
	
	// Set fields accordingly
	new_vertex->id = vtx->id;
	new_vertex->num_neighbors = vtx->num_neighbors;
	new_vertex->neighbors = (Vertex **) (MALLOC(sizeof(Vertex *) * vtx->num_neighbors));
	new_vertex->neighbor_weights = (int *) (MALLOC(sizeof(int) * vtx->num_neighbors));

	// avoiding memcpy
	int i;
	for (i = 0; i < vtx->num_neighbors; i++) {
		new_vertex->neighbors[i] = vtx->neighbors[i];
		new_vertex->neighbor_weights[i] = vtx->neighbor_weights[i];
	}

	return new_vertex;

}

Graph *build_new_graph(int num_vtx)
{
	// Allocate graph
	Graph *new_graph = (Graph *) (MALLOC(sizeof(Graph)));

	// Set num_vertices, allocate vertex IDs array, verticies array
	new_graph->num_vertices = num_vtx; // Set prior (probably bad design)
	new_graph->num_edges = 0; // Will increment as edges are added
	new_graph->vertices = (Vertex **) (MALLOC(sizeof(Vertex *) * num_vtx));

	// Zero initialize
	memset(new_graph->vertices, 0, sizeof(*(new_graph->vertices)));

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
	
	// SEED();
	
	// Tricky loop b/c of then number of internal branches
	while (i < new_edges)
	{
		// Checks (quite unlikely but precautionary
		// because I tend to write bugs at 4am)
	
		// Find a random id (of a vertex) to build an
		// edge with, depending on conditions
	
		int new_id = (int) (lrand48() % (g->num_vertices));
	
		// If the vertex with new_id doesn't exist
		if (!(g->vertices[new_id])) { continue; }

		Vertex *potential_neighbor = g->vertices[new_id];
		
		// If there's the edge is already there at position new_id
		// (and vice versa) --- very unlikely
		if ((vtx->neighbors[new_id] == potential_neighbor) 
			|| (potential_neighbor->neighbors[vtx->id] == vtx)) { continue; }
		
  
		// Check if we're at max neighbors for potential neighbors
		if (potential_neighbor->num_neighbors == g->num_vertices) { continue; }	
		
		// Now we're ready to add --- set the corresponding
		// positions in each neighbors array to the pointer
		// to the respective vertices set the weights correctly,
		// increment information counters
		potential_neighbor->neighbors[vtx->id] = vtx;
		vtx->neighbors[new_id] = potential_neighbor;

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

Graph *generate_full_graph(int num_vtx, int weighted)
{
	// Build the shell
	Graph *g = build_new_graph(num_vtx);
	
	// Populate with vertices
	int i;
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
	free(vtx->neighbors); // Freeing the actual vertices in
						  // the grph will cause recursion
						  // and invalid pointer frees
	free(vtx);

	return;
}

void destroy_vertex_array(Vertex **vertices)
{
	int length = LEN(vertices), i;
	for (i = 0; i < length; i++) {
		destroy_vertex(vertices[i]);
	}

	free(vertices);

	return;
}

void destroy_graph(Graph *g)
{
	destroy_vertex_array(g->vertices);
	free(g);

	return;	
}


// Graph utility
int check_vertex(Vertex *vtx)
{
	if (!vtx) { return 0; }
	if (!(vtx->neighbors)) { return 0; }

	return 1;	
}

int inline max_edges(Graph *g)
{
	return (g->num_vertices * (g->num_vertices - 1)) / 2;
}



// ------------------------------------------
// Graph algorithms

/*
// Cycle detection
#define MAX_CYCLES 8 // A cutoff more than anything

// Returns the first cycle that detect_cycles finds
// or returns NULL
Vertex **detect_cycles(Graph *g)
{
	int max_cycle_size = g->num_vertices;

	// Pick a random starting point
	Vertex *start_vtx = g->vertices[(lrand48() % g->num_vertices)];
	if (!(check_vertex(start_vtx))) { return NULL; }

	// Build the necessary structures
	
	// Visited array
	int *visited_ids = (int *) (MALLOC(sizeof(int) * g->num_vertices));
	memset(visited_ids, 0, sizeof(visited_ids));

	// 
	int *current_path = (int *) (MALLOC(sizeof(int) * g->num_vertices));
   	memset(current_path, 0, sizeof(current_path);	

}
*/




