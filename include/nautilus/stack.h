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
 * Simple stack implementation for generic data representations
 */ 

#ifndef __STACK_H__
#define __STACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DO_STACK_PRINT 0

#if DO_STACK_PRINT
#define STACK_PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define STACK_PRINT(...) 
#endif


#define INIT_SIZE 16
#define EXP_FACTOR 2
#define USED __attribute__((used))
#define MALLOC_CHECK(n) ({void *__p = malloc(n); if (!__p) { STACK_PRINT("Malloc failed\n"); panic("Malloc failed\n"); } __p;})
#define REALLOC_CHECK(p, n) ({void *__p = realloc(p, n); if (!__p) { STACK_PRINT("Realloc failed\n"); panic("Malloc failed\n"); } __p;})

// Stack type with generic data type representation
#define _NK_STACK_TYPE_INIT(type) \
	typedef struct { \
		uint32_t iterator; \
		uint32_t size; \
		type *data; \
	} nk_stack_##type;


// Generic type methods generation
#define NK_STACK_DECL(type) \
	_NK_STACK_TYPE_INIT(type) \
	USED static void _nk_stack_##type##_init(nk_stack_##type *stk) { \
		stk->iterator = 0; \
		stk->size = INIT_SIZE; \
		stk->data = (type *) (MALLOC_CHECK(sizeof(type) * INIT_SIZE)); \
		memset(stk->data, 0, (sizeof(type) * INIT_SIZE)); \
	} \
	USED nk_stack_##type *nk_stack_##type##_get() { \
		nk_stack_##type *stk = ((nk_stack_##type *) MALLOC_CHECK(sizeof(nk_stack_##type))); \
		_nk_stack_##type##_init(stk); \
		return stk; \
	} \
	USED static inline void _nk_stack_##type##_grow(nk_stack_##type *stk) { \
		if (stk->iterator == stk->size) { stk->data = (type *) (REALLOC_CHECK(stk->data, (stk->size * EXP_FACTOR))); } \
	} \
	USED static void nk_stack_##type##_push(nk_stack_##type *stk, type element) { \
		_nk_stack_##type##_grow(stk); \
		stk->data[stk->iterator] = element; \
		(stk->iterator)++; \
	} \
	USED static type nk_stack_##type##_pop(nk_stack_##type *stk) { \
		if (nk_stack_empty(stk)) { STACK_PRINT("Stack empty\n"); return 0; } \
		type temp_value = stk->data[(stk->iterator - 1)]; \
		stk->data[(stk->iterator - 1)] = 0; \
		(stk->iterator)--; \
		return temp_value; \
	} \

// Kernel method declarations
#define nk_stack_empty(stk) (!(stk->iterator)) 
#define nk_stack_get(type) nk_stack_##type##_get()
#define nk_stack_push(type, stk, element) nk_stack_##type##_push(stk, element)
#define nk_stack_pop(type, stk) nk_stack_##type##_pop(stk)
#define nk_stack_destroy(stk) { free(stk->data); free(stk); }

#ifdef __cplusplus
}
#endif


#endif
