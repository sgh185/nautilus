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


List_t * createList(uint64_t start, uint64_t size);

TreeNode_t * createTree(uint64_t start, uint64_t size);

TreeQueue_t * createQueue(void);

uint64_t * createRandArray(uint64_t size);

/*
List_t * createList(uint64_t start, uint64_t size);
TreeNode_t * createTree(uint64_t start, uint64_t size);
TreeQueue_t * createQueue(uint64_t queue_size);
uint64_t * createRandArray(uint64_t size);
*/

#endif
