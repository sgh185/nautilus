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
 * Self contained KNN implementation that's compatible with Nautilus. 
 * Used as a microbenchmark for testing the compiler-timing model on fibers
 */ 

static const double multiplier = 3.14159;

struct KNNPoint
{	
	uint32_t dimensions; // number of features
	uint32_t classification; // assigned class
	double *data; // array of length "dimensions"
};

struct KNNContext
{
	uint32_t k;
	uint32_t num_examples;
	double (*distance_metric)(struct KNNPoint *, struct KNNPoint *);
	double (*aggregator)(struct KNNPoint **);	
	struct KNNPoint **examples;
	uint32_t dimensions;
};

// Setup and cleanup
struct KNNPoint *KNN_build_point(uint32_t dims);
struct KNNPoint *KNN_copy_point(struct KNNPoint *point);
struct KNNContext *KNN_build_context(uint32_t k, uint32_t dims, uint32_t num_ex,
							  double (*dm)(struct KNNPoint *f, struct KNNPoint *s), 
							  double (*agg)(struct KNNPoint **a));
void KNN_point_destroy(struct KNNPoint *point);
void KNN_point_destroy(struct KNNPoint *point);
void KNN_context_destroy(struct KNNContext *ctx);


// Classification
double KNN_classify(struct KNNPoint *point, struct KNNContext *ctx);

// Distance metrics
double distance_euclidean_squared(struct KNNPoint *first, struct KNNPoint *second);
double distance_manhattan(struct KNNPoint *first, struct KNNPoint *second);

// Aggregators
double aggretate_mean(struct KNNPoint **arr);
double aggregate_median(struct KNNPoint **arr);
double aggergate_mode(struct KNNPoint **arr);

// Sorting --- very suspicious
void _swap(double *a, double *b);
void _swap_KNN(struct KNNPoint *a, struct KNNPoint *b);
int _partition(double *arr, int low, int high);
int _partition_KNN(double *arr, struct KNNPoint **KNN_arr, int low, int high);
void _quicksort_KNN(double *arr, struct KNNPoint **KNN_arr, int low, int high);
void _quicksort(double *arr, int low, int high);
double *quicksort(double *arr);
double *quicksort_with_order(double *arr, struct KNNPoint **examples, struct KNNPoint **order_arr);

// Utility
int inline check_pair(struct KNNPoint *first, struct KNNPoint *second);
int inline check_point(struct KNNPoint *point);
double *extract_classifications(struct KNNPoint **arr);

