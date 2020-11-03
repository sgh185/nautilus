#include "paging_benchmark.h"

int paging_wrapper(
    char * _buf, 
    void *_priv,
    int (*fptr)(char *, void *)
){

    nk_thread_id_t t;
    t = nk_thread_fork();
    if(t == NK_BAD_THREAD_ID){
       nk_vc_printf("Failed to fork thread\n");
       return 1;
    }

    if (t == 0) {
        // child thread
        int child_ret = 0;
        char buf[32];
        struct nk_thread *t = get_cur_thread();
        nk_thread_name(get_cur_thread(),buf);
        get_cur_thread()->vc = get_cur_thread()->parent->vc;
        nk_vc_printf("Hello from forked thread tid %lu \n", t->tid);
        // if the function being forked is inlined
        // we must explicitly invoke
        // nk_thread_exit(0);
        // here

     #ifdef NAUT_CONFIG_ASPACE_PAGING

        nk_aspace_t *old_aspace = t->aspace;

        nk_vc_printf("The old aspace is %p\n", old_aspace);

        nk_aspace_characteristics_t c;
       
        if (nk_aspace_query("paging",&c)) {
            nk_vc_printf("failed to find paging implementation\n");
            child_ret = 1;
            goto child_finish;
        }

        // create a new address space for this shell thread
        nk_aspace_t *mas = nk_aspace_create("paging","paging for NAS benchmark",&c);

        if (!mas) {
            nk_vc_printf("failed to create new address space\n");
            child_ret = 1;
            goto child_finish;
        }
        
        nk_aspace_region_t r;
        // create a 1-1 region mapping all of physical memory
        // so that the kernel can work when that thread is active
        r.va_start = 0;
        r.pa_start = 0;
        r.len_bytes = 0x100000000UL;  // first 4 GB are mapped
        // set protections for kernel
        // use EAGER to tell paging implementation that it needs to build all these PTs right now
        r.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE | NK_ASPACE_EXEC | NK_ASPACE_PIN | NK_ASPACE_KERN | NK_ASPACE_EAGER;

        // now add the region
        // this should build the page tables immediately
        if (nk_aspace_add_region(mas,&r)) {
            nk_vc_printf("failed to add initial eager region to address space\n");
            child_ret = 1;
            goto destroy_mas;
        }

        if (nk_aspace_move_thread(mas)) {
            nk_vc_printf("failed to move shell thread to new address space\n");
            child_ret = 1;
            goto destroy_mas;
        }
    
        nk_vc_printf("Survived moving thread into its own address space\n");
    
        nk_vc_printf("Start executing the benchmark\n");
        

        /**
         *  What if function don't return at all ??
         * */
        if((*fptr)(_buf, _priv)){
            nk_vc_printf("Failed running benchmark\n");
            child_ret = 1;
            goto destroy_mas;
        }

        nk_vc_printf("Finish the benchmark\n");
      


       
        if(nk_aspace_move_thread(old_aspace) ) {
            nk_vc_printf("Failed move thread from %p to %p\n", mas, old_aspace);
            child_ret = 1;
            goto destroy_mas;
        }

        nk_vc_printf("Move thread succeeded\n");


    destroy_mas:
        if(nk_aspace_destroy(mas)){
            nk_vc_printf("Something wrong during destorying the new aspace\n");
            return 1;
        } else {
            nk_vc_printf("Destory succeeded\n");
        }
        
    #endif
    child_finish:
        return child_ret;
    }
    else {
        nk_vc_printf("Hello from parent thread \n");
                // parent thread just forks again
    }

    
    if (nk_join_all_children(0)) {
        nk_vc_printf("Failed to join forked threads on pass \n");
        return 1;
    }

    nk_vc_printf("Joined forked threads in pass\n");

    nk_sched_reap(1); // clean up unconditionally

    return 0;
}