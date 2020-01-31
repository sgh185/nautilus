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
 * Copyright (c) 2019, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2019, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author:  Souradip Ghosh <sgh@u.northwestern.edu>
 * 	    Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *  
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


/* ******
 * BENCHMARK ROUTINES LISTED AND EXPLAINED IN 'fbREADME'
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/fiber.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/libccompat.h>
#include <nautilus/timehook.h>
#include <nautilus/barrier.h>
#include <nautilus/pmc.h>

#define DO_PRINT       0

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif

extern struct nk_virtual_console *vc;

/******************* Macros/Helper Functions *******************/
#define MALLOC(n) ({void *__p = malloc(n); if (!__p) { PRINT("Malloc failed\n"); panic("Malloc failed\n"); } __p;})

#define M 300
#define DOT 100
#define DIMENSION_SIZE(arr) ((sizeof(arr)) / (sizeof((arr)[0])))
#define M1 20
#define M2 40
#define M3 30

extern void nk_simple_timing_loop(uint64_t);

// Benchmark --- Linked List Implementation
__attribute__((noinline)) List_t * createList(uint64_t start, uint64_t size) 
{
  // List setup
  Node_t *L_head = MALLOC(sizeof(Node_t));
  if (!L_head) return NULL;

  L_head->value = start;
  L_head->next = NULL;
  
  List_t *LL = MALLOC(sizeof(List_t));
  if (!LL) return NULL;
  
  LL->head = L_head;
  LL->tail = LL->head;
  
  // Fill list;
  uint64_t a, incr = start;
  for (a = 0; a < size; a++)
  {
    incr += (a * a) + incr;
    
    Node_t *L_node = MALLOC(sizeof(Node_t));
    if (!L_node) return NULL;
    
    L_node->value = incr;
    L_node->next = NULL;

    LL->tail->next = L_node;
    LL->tail = L_node;  
  }

  return LL;
}

// Benchmark --- Binary Tree Implementation
__attribute__((noinline)) TreeNode_t * createTree(uint64_t start, uint64_t size) {
  // Create root
  TreeNode_t *root = MALLOC(sizeof(TreeNode_t));
  if (!root) return NULL;

  root->value = start;
  root->left = NULL;
  root->right = NULL;

  // Fill tree
  int i = 0;
  for (; i < size; i++)
  {
    TreeNode_t *iterator = root;
    
    uint64_t val = (lrand48() % 10000); 
    srand48(0);

    // Create new node with value val
    TreeNode_t *newNode = MALLOC(sizeof(TreeNode_t));
    if (!newNode) return NULL;
 
    newNode->value = val;
    newNode->left = NULL;
    newNode->right = NULL;
    
    // Place in tree (based on binary search tree invariant)
    while(1)
    {
      if (val > iterator->value)
      {
	if (!(iterator->right))
	{
	  iterator->right = newNode;
	  break; 
	}

	iterator = iterator->right;
      }
      else
      {
	if (!(iterator->left))
	{
	  iterator->left = newNode;
	  break;
	}

	iterator = iterator->left;
      }  
    }
  }

  return root;
}

// Create Queue for BST
#define MAX_QUEUE_SIZE 10000
__attribute__((noinline)) TreeQueue_t * createQueue(void) {
  TreeQueue_t *new_queue = MALLOC(sizeof(TreeQueue_t));
  if (!new_queue) return NULL;

  new_queue->queue = MALLOC(sizeof(TreeNode_t *) * MAX_QUEUE_SIZE);
  if (!new_queue->queue) return NULL;
  
  new_queue->head_pos = 0;
  new_queue->tail_pos = 0;

  return new_queue;
}

// Create array of 25 random unsigned longs 
__attribute__((noinline)) uint64_t * createRandArray50() {
  uint64_t *r_array = MALLOC(sizeof(uint64_t) * 50);
  if(!r_array) return NULL;

  int i = 0;
  for (; i < 50; i++)
    r_array[i] = (lrand48() % 1000);

  return r_array;
}

// Dummy function --- for floating point (non-loop) benchmarks
__attribute__((noinline)) void dummy_func(double a, double b, double c) {
  nk_vc_printf("%f, %f, %f\n", a, b, c);
  return;
}

// Dummy function --- for "sum" variables in loop benchmarks
__attribute__((noinline)) void sum_dummy_func(uint64_t a) {
  nk_vc_printf("%d\n", a);
  return;
}
 
/******************* Test Routines *******************/

int ACCESS_WRAPPER = 0;

// BENCHMARK --- Timing loops (interprocedural)
void benchmark1(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0; 
  ACCESS_WRAPPER = 1;

  // #pragma unroll M
  while(a < M){
    nk_simple_timing_loop(200);
    a++;
  }

  // nk_vc_printf("Benchmark 1 is finished\n");
  ACCESS_WRAPPER = 0;
  
  _nk_fiber_print_data();
  
  return;
}

void benchmark2(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
  
  ACCESS_WRAPPER = 1;
  
  // #pragma unroll M
  while(a < M){
    nk_simple_timing_loop(200);
    a++;
  }
  // nk_vc_printf("Benchmark 2 is finished\n");

  ACCESS_WRAPPER = 0;

  _nk_fiber_print_data();
  return;
}


// BENCHMARK --- Staggered timing loops (interprocedural)
void benchmark3(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;

  // #pragma unroll M
  while(a < M){
    if (a % 25 == 0)
      nk_simple_timing_loop(200000000);
    else
      nk_simple_timing_loop(200);

    a++;
  }
  
  nk_vc_printf("Benchmark 1 is finished\n");
  return;
}

void benchmark4(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int a = 0;
 
  // #pragma unroll M
  while(a < M){
    nk_simple_timing_loop(200);
    a++;
  }
  nk_vc_printf("Benchmark 2 is finished\n");

  _nk_fiber_print_data();
  
  return;
}


// Benchmark --- Dot product (known induction variables, parameters, volatile sum, intraprocedural)
void benchmark5(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int k, first = 8, second = 134;
  
  // Allocate on stack
  uint64_t b[DOT];
  uint64_t c[DOT];

  // Fill arrays with some value
  for (k = 0; k < DOT; k++)
  {
    b[k] = first * second;
    c[k] = first + second;

    first += 4;
    second += 2; 
  }

  // Dot product
  volatile uint64_t sum = 0;
  for (k = 0; k < DOT; k++)
    sum += (b[k] * c[k]);


  nk_vc_printf("Benchmark 5 is finished\n");

  return;
}

void benchmark6(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  int k, first = 5, second = 127;
  
  uint64_t b[DOT];
  uint64_t c[DOT];

  // Fill arrays with some value
  for (k = 0; k < DOT; k++)
  {
    b[k] = first * second;
    c[k] = first + second;

    first += 4;
    second += 2; 
  }

  // Dot product
  volatile uint64_t sum = 0;
  for (k = 0; k < DOT; k++)
    sum += (b[k] * c[k]);

  nk_vc_printf("Benchmark 6 is finished\n");
  _nk_fiber_print_data();

  return;
}


// BENCHMARK --- Linked list traversal (known parameters, volatile sum, intraprocedural)
void benchmark7(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  List_t *LL = createList(7, 800);
 
  if (!LL) return; 

  ACCESS_WRAPPER = 1;

  // Traverse list, calculate sum
  volatile uint64_t sum = 0;
  Node_t *iterator = LL->head;
  
  while(iterator != NULL)
  {
    sum += iterator->value;
    iterator = iterator->next;
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Sum: %d\n", sum);
  // nk_vc_printf("Benchmark 7 is finished\n");
 
  // Purposely avoid free --- limit function calls at end of control flow to 1 call
  return;
}

void benchmark8(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  List_t *LL = createList(11, 800);
 
  if (!LL) return;

  ACCESS_WRAPPER = 1;
  
  // Traverse list, calculate sum
  volatile uint64_t sum = 0;
  Node_t *iterator = LL->head;
  
  while(iterator != NULL)
  {
    sum += iterator->value;
    iterator = iterator->next;
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Benchmark 8 is finished\n");
  _nk_fiber_print_data();
  
  // Purposely avoid free --- limit function calls at end of control flow to 1 call
  return;
}


// BENCHMARK --- Matrix multiply (known dimensions, parameters, volatile sum, intraprocedural) 
void benchmark9(void *i, void **o)
{
  
  return;	
  nk_fiber_set_vc(vc);
  // Allocate on stack
  // Multiply matrices with known dimensions
  volatile uint64_t first[M1][M2];
  volatile uint64_t second[M2][M3];
  uint64_t result[M1][M3];

  memset(result, 0, sizeof(result));

  // Fill first and second matricies
  uint64_t start = 4;
  int a, b, c;

  ACCESS_WRAPPER = 1;

  for (a = 0; a < M1; a++)
    for (b = 0; b < M2; b++)
      first[a][b] = a + (a * b) + b;

  for (a = 0; a < M2; a++)
    for (b = 0; b < M3; b++)
      second[a][b] = (a * b);

  // Naive matrix multiply
  for (a = 0; a < M1; a++) {
    for (b = 0; b < M3; b++) {
      uint64_t sum = 0;
      
      for (c = 0; c < M2; c++)
	sum += (first[a][c] * second[c][b]);
      
      result[a][b] = sum;
    }
  }
  
  ACCESS_WRAPPER = 0;
 
  // nk_vc_printf("Benchmark 9 is finished\n");
  
  return;
}

void benchmark10(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  // Allocate on stack
  // Multiply matrices with known dimensions
  uint64_t first[M1][M2];
  uint64_t second[M2][M3];
  uint64_t result[M1][M3];

  memset(result, 0, sizeof(result));

  // Fill first and second matricies
  uint64_t start = 8;
  int a, b, c;

  ACCESS_WRAPPER = 1;

  for (a = 0; a < M1; a++)
    for (b = 0; b < M2; b++)
      first[a][b] = a + (a * b) + b;

  for (a = 0; a < M2; a++)
    for (b = 0; b < M3; b++)
      second[a][b] = (a * b);

  // Naive matrix multiply
  for (a = 0; a < M1; a++) {
    for (b = 0; b < M3; b++) {
      uint64_t sum = 0;
      
      for (c = 0; c < M2; c++)
	sum += (first[a][c] * second[c][b]);
      
      result[a][b] = sum;
    }
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Benchmark 10 is finished\n");
  _nk_fiber_print_data();
  
  return;
}


// BENCHMARK --- Binary search tree (known parameters, volatile sum, intraprocedural)
void benchmark11(void *i, void **o)
{
  // barrier
  // nk_counting_barrier_t *bar = (nk_counting_barrier_t*)i;
  // nk_fiber_counting_barrier(bar);
  
  nk_fiber_set_vc(vc);
  
  ACCESS_WRAPPER = 1;

  TreeNode_t *tree = createTree(4520, 800);
  uint64_t *nums = createRandArray50(); 
  uint64_t totalTraversalSum = 0;
  int a;
 
  for (a = 0; a < 50; a++)
  {
    uint64_t currTraversalSum = 0;
    volatile TreeNode_t *iterator = tree;
    
    while (iterator != NULL)
    {
      uint64_t currValue = iterator->value;
      currTraversalSum += currValue;

      if (nums[a] == currValue)
        break;      
      else if (nums[a] > currValue)
	iterator = iterator->right;
      else
	iterator = iterator->left;        
    } 

    totalTraversalSum += currTraversalSum; 
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Benchmark 11 is finished\n");
  
  // Purposely avoid free --- limit function calls at end of control flow to 1 call
  return;
}

void benchmark12(void *i, void **o)
{
  // barrier
  // nk_counting_barrier_t *bar = (nk_counting_barrier_t*)i;
  // nk_fiber_counting_barrier(bar);
  
  nk_fiber_set_vc(vc);

  ACCESS_WRAPPER = 1;

  TreeNode_t *tree = createTree(5380, 800);
  uint64_t *nums = createRandArray50();
  
  uint64_t totalTraversalSum = 0;
  int a;
  for (a = 0; a < 50; a++)
  {
    uint64_t currTraversalSum = 0;
    volatile TreeNode_t *iterator = tree;
    
    while (iterator != NULL)
    {
      uint64_t currValue = iterator->value;
      currTraversalSum += currValue;

      if (nums[a] == currValue)
        break;      
      else if (nums[a] > currValue)
	iterator = iterator->right;
      else
	iterator = iterator->left;        
    } 
    
    totalTraversalSum += currTraversalSum; 
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Benchmark 12 is finished\n");
  _nk_fiber_print_data();
  get_time_hook_data(); 
  // Purposely avoid free --- limit function calls at end of control flow to 1 call
  return;
}


// BENCHMARK --- Matrix multiply (random dimensions, unknown induction variables, intraprocedural)
void benchmark13(void *i, void **o)
{
  return;
  nk_fiber_set_vc(vc);
  uint64_t R1 = (lrand48() % 50);
  uint64_t R2 = (lrand48() % 50);
  uint64_t R3 = (lrand48() % 50);
  
  // Random distributions --- allocate on stack
  uint64_t first[R1][R2];
  uint64_t second[R2][R3];
  uint64_t result[R1][R3];

  memset(result, 0, sizeof(result));

  ACCESS_WRAPPER = 1;

  // Fill first and second matricies
  uint64_t start = 8;
  int a, b, c;

  for (a = 0; a < R1; a++)
    for (b = 0; b < R2; b++)
      first[a][b] = a + (a * b) + b;

  for (a = 0; a < R2; a++)
    for (b = 0; b < R3; b++)
      second[a][b] = (a * b);

  // Naive matrix multiply
  for (a = 0; a < R1; a++) {
    for (b = 0; b < R3; b++) {
      uint64_t sum = 0;
      for (c = 0; c < R2; c++)
	sum += (first[a][c] * second[c][b]);
      result[a][b] = sum;
    }
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Benchmark 13 is finished\n");
  
  return;
}

void benchmark14(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  // No need to seed at this point
  uint64_t R1 = (lrand48() % 50);
  uint64_t R2 = (lrand48() % 50);
  uint64_t R3 = (lrand48() % 50);
 
  // Random distributions --- allocate on stack 
  uint64_t first[R1][R2];
  uint64_t second[R2][R3];
  uint64_t result[R1][R3];

  memset(result, 0, sizeof(result));

  ACCESS_WRAPPER = 1;
  
  // Fill first and second matricies
  uint64_t start = 8;
  int a, b, c;

  // #pragma unroll 
  for (a = 0; a < R1; a++)
    for (b = 0; b < R2; b++)
      first[a][b] = a + (a * b) + b;

  //#pragma unroll
  for (a = 0; a < R2; a++)
    for (b = 0; b < R3; b++)
      second[a][b] = (a * b);

  // Naive matrix multiply
  for (a = 0; a < R1; a++) {
    for (b = 0; b < R3; b++) {
      uint64_t sum = 0;
      for (c = 0; c < R2; c++)
	sum += (first[a][c] * second[c][b]);
      result[a][b] = sum;
    }
  }

  ACCESS_WRAPPER = 0;

  // nk_vc_printf("Benchmark 14 is finished\n");
  _nk_fiber_print_data();
  
  return;
}

// BENCHMARK --- Level-order tree traversal (known parameters, volatile sum, intraprocedural)
void benchmark21(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  
  ACCESS_WRAPPER = 1;

  TreeQueue_t *LO = createQueue();
  TreeNode_t *tree = createTree(5170, 800);

  TreeNode_t *iterator = tree;
  volatile uint64_t sum = 0;
  
  while (iterator != NULL)
  {
    sum += iterator->value;
    
    if ((LO->tail_pos == MAX_QUEUE_SIZE) || (LO->head_pos > LO->tail_pos))
      break;

    // Enqueue children of iterator
    if (iterator->left) {
      LO->queue[LO->tail_pos] = iterator->left;
      LO->tail_pos++;
    }

    if (iterator->right) {
      LO->queue[LO->tail_pos] = iterator->right;
      LO->tail_pos++;
    }

    // Dequeue --- set to iterator
    iterator = LO->queue[LO->head_pos];
    LO->head_pos++;
  }
 

  ACCESS_WRAPPER = 0; 
  
  // nk_vc_printf("Benchmark 21 is finished\n");
  // Purposely avoid free --- limit function calls at end of control flow to 1 call

  return;
}

void benchmark22(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  
  ACCESS_WRAPPER = 1;
  
  TreeQueue_t *LO = createQueue();
  TreeNode_t *tree = createTree(4720, 800);

  TreeNode_t *iterator = tree;
  volatile uint64_t sum = 0;
  
  while (iterator != NULL)
  {
    sum += iterator->value;
    
    if ((LO->tail_pos == MAX_QUEUE_SIZE) || (LO->head_pos > LO->tail_pos))
      break;

    // Enqueue children of iterator
    if (iterator->left) {
      LO->queue[LO->tail_pos] = iterator->left;
      LO->tail_pos++;
    }

    if (iterator->right) {
      LO->queue[LO->tail_pos] = iterator->right;
      LO->tail_pos++;
    }
    
    // Dequeue --- set to iterator
    iterator = LO->queue[LO->head_pos];
    LO->head_pos++;
  }
  
  ACCESS_WRAPPER = 0;

  nk_vc_printf("Benchmark 22 is finished\n");
  _nk_fiber_print_data();
  // Purposely avoid free --- limit function calls at end of control flow to 1 call

  return;
}

void benchmark23(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  uint64_t count = (lrand48() % 10000), c, d;
  
  ACCESS_WRAPPER = 1;

  volatile double sum = 0;
  volatile double a = 44.32, b = 2.29;

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
  
  // nk_vc_printf("Benchmark 23 is finished\n");

  return;
}

void benchmark24(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  uint64_t count = (lrand48() % 10000), c, d;
  
  ACCESS_WRAPPER = 1;
  
  volatile double sum = 0;
  volatile double a = 47.11, b = 5.2789;

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
  
  ACCESS_WRAPPER = 0;
  
  // nk_vc_printf("Benchmark 24 is finished\n");
  _nk_fiber_print_data();

  return;
}

// BENCHMARK --- Barriers using timing loops
#define TH 10000
extern int ACCESS_HOOK;
void benchmark25(void *i, void **o)
{
  nk_fiber_set_vc(vc);

  // barrier
  // nk_counting_barrier_t *bar = (nk_counting_barrier_t*)i;
  // nk_fiber_counting_barrier(bar);

  int a = 0;
  
  ACCESS_HOOK = 1;

  while(a < TH){
    nk_simple_timing_loop(200);
    a++;
  }

  ACCESS_HOOK = 0;

  nk_vc_printf("Benchmark 25 is finished\n");
  get_time_hook_data();
  return;
}

void benchmark26(void *i, void **o)
{
  nk_fiber_set_vc(vc);
  
  // barrier
  // nk_counting_barrier_t *bar = (nk_counting_barrier_t*)i;
  // nk_fiber_counting_barrier(bar);

  int a = 0;
  
  ACCESS_HOOK = 1;
  
  while(a < TH){
    nk_simple_timing_loop(200);
    a++;
  }
  ACCESS_HOOK = 0;
  nk_vc_printf("Benchmark 26 is finished\n");

  // print wrapper interval data
  _nk_fiber_print_data();

  // print time_hook interface timing data
  get_time_hook_data();
  return;
}


/******************* Test Wrappers *******************/

int test_fibers_bench(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark1, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark2, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench2(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark3, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark4, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench3(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark5, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark6, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench4(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark7, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark8, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench5(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark9, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark10, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

// test_fibers_bench6 uses create/run instead of start as an
// attempt to hack a pseudo-barrier into the execution of this
// benchmark (not very effective) --- for actual benchmarks using
// barriers --- see test_fibers_bench13 ("timehook1")
int test_fibers_bench6(){
  // set up barrier
  // nk_counting_barrier_t *bar = MALLOC(sizeof(nk_counting_barrier_t));
  // nk_counting_barrier_init(bar, 2); // two fibers

  // set up fibers
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  
  nk_fiber_start(benchmark11, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark12, 0, 0, FSTACK_2MB, 0, &simple2);

  /*
  nk_fiber_create(benchmark11, 0, 0, FSTACK_2MB, &simple1);
  nk_fiber_create(benchmark12, 0, 0, FSTACK_2MB, &simple2);
  nk_fiber_run(simple1, F_CURR_CPU);
  nk_fiber_run(simple2, F_CURR_CPU);
  */  
  return 0;

}

int test_fibers_bench7(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark13, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark14, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench8(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(benchmark15, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark16, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench9(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(benchmark17, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark18, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench10(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(benchmark19, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark20, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench11(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark21, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark22, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

int test_fibers_bench12(){
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  nk_fiber_start(benchmark23, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark24, 0, 0, FSTACK_2MB, 0, &simple2);
  //_nk_fiber_print_data();
  return 0;
}

// time_hook interface timing data collection (utilizing barrier
// functionality)
int test_fibers_bench13(){
  // set up barrier
  // nk_counting_barrier_t *bar = MALLOC(sizeof(nk_counting_barrier_t));
  // nk_counting_barrier_init(bar, 2); // two fibers

  // set up fibers
  nk_fiber_t *simple1;
  nk_fiber_t *simple2;
  vc = get_cur_thread()->vc;
  // nk_fiber_start(benchmark25, (void*)bar, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark26, (void*)bar, 0, FSTACK_2MB, 0, &simple2);
  nk_fiber_start(benchmark25, 0, 0, FSTACK_2MB, 0, &simple1);
  // nk_fiber_start(benchmark26, 0, 0, FSTACK_2MB, 0, &simple2);
 
  //_nk_fiber_print_data();
  return 0;
}



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
  .cmd      = "fiberbench10",
  .help_str = "fiberbench10",
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
  .cmd      = "timehook1",
  .help_str = "timehook1",
  .handler  = handle_fibers13,
};


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




