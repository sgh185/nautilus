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
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author:  Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Enrico Deiana <ead@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *  
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/skiplist.h>

#define gen_rand_array(type, len, neg) ({ \
	SEED; \
	type *rand_array = (type *) (SLIST_MALLOC(sizeof(type) * len)); \
	size_t i; \
	for (i = 0; i < len; i++) \
	{ \
		type num = (type) (lrand48()), \
	   		 sign = ((num % 2) || (!(neg))) ? 1 : -1; \
		rand_array[i] = num * sign; \
	} \
	rand_array; \
})

void print_sl(nk_slist *sl)
{
	int i;
	for (i = 0; i < SLIST_TOP_GEAR; i++)
	{
		nk_vc_printf("gear %d: ", i);

		nk_slist_node *iterator = sl->all_gears[i];
		while (iterator != NULL)
		{
			nk_vc_printf("%d ", iterator->data);
			iterator = iterator->succ_nodes[i];
		}

		nk_vc_printf("\n");
	}

	return;
}

static int
handle_sl (char * buf, void * priv)
{
	nk_vc_printf("skiplist test ...\n");
	nk_slist *the_list = nk_slist_build();

	int *rand_array = gen_rand_array(int, 10, 1), i;
	
	for (i = 0; i < 10; i++) { 
		nk_vc_printf("%d ", rand_array[i]);
	}

	nk_vc_printf("\n");
		
	nk_slist_add(the_list, rand_array[0]);
	
	print_sl(the_list);

	for (i = 0; i < 10; i++) {
		nk_slist_add(the_list, rand_array[i]);
	}	

	print_sl(the_list);

    return 0;
}

static struct shell_cmd_impl slist_impl = {
    .cmd      = "slist",
    .help_str = "slist",
    .handler  = handle_sl
};

nk_register_shell_cmd(slist_impl);


