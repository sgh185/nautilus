#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/alloc.h>
#include "allocator_test_trace.h"

#define TRACE_NUM 0

#define DEBUG(S, ...) nk_vc_printf("alloc_test: debug(): " S "\n", ##__VA_ARGS__)



int run_alloc_test_trace();
static int handle_alloc_tests_trace(char* buf, void* priv);

static int handle_alloc_tests_trace(char* buf, void* priv){
    run_alloc_test_trace();
    return 0;
}

int run_alloc_test_trace(){
    nk_alloc_t *myalloc = nk_alloc_create("cs213","myalloc");
    if(!myalloc){
        DEBUG("Error - could not create allocator");
        return -1;
    }
    if(nk_alloc_set_associated(myalloc)){
        DEBUG("Error - could not associate allocator");
        return -1;
    }

    int num_allocs = allocator_tests_traces[TRACE_NUM][0];
    int num_ops = allocator_tests_traces[TRACE_NUM][1];
    void* allocs[num_allocs];
    int allocs_completed = 0;
    for(int i=2; i<num_ops;i+=3){
        int alloc_id = allocator_tests_traces[TRACE_NUM][i+1];
        DEBUG("alloc_type: %d", allocator_tests_traces[TRACE_NUM][i]);
        switch (allocator_tests_traces[TRACE_NUM][i])
        {
        case -1: //alloc case
            allocs[alloc_id] = malloc(allocator_tests_traces[TRACE_NUM][i+2]);
            DEBUG("ALLOCed id %d with size %d and location %p", alloc_id, allocator_tests_traces[TRACE_NUM][i+2], allocs[alloc_id]);
            allocs_completed++;
            if(!allocs[alloc_id]){
                DEBUG("Alloc failed - malloc returned null ptr");
                return -1;
            }
            break;
        case -2: //realloc
            allocs[alloc_id] = realloc(allocs[alloc_id], allocator_tests_traces[TRACE_NUM][i+2]);
            DEBUG("REALLOCed id %d with size %d and location %p", alloc_id, allocator_tests_traces[TRACE_NUM][i+2], allocs[alloc_id]);
            if(!allocs[alloc_id]){
                DEBUG("Realloc failed - realloc returned null ptr");
                return -1;
            }
            break;
        case -3: //free
            free(allocs[alloc_id]);
            DEBUG("Freed id %d at location %p", alloc_id, allocs[alloc_id]);
            break;
        
        default:
            DEBUG("How the hell did we get here?");
            return -1;
            break;
        }
        
        allocs_completed++;
    }




    DEBUG("TEST COMPLETED for trace id %d", TRACE_NUM);
    DEBUG("EXPECTED: %d operations", num_ops);
    DEBUG("COMPLETED: %d operations", allocs_completed);

    return 0;
}

static struct shell_cmd_impl alloctesttrace_impl = {
    .cmd = "allocator_test_trace",
    .help_str = "allocator_test_trace - runs an alloc trace with cs213 alloc",
    .handler = handle_alloc_tests_trace,
};
nk_register_shell_cmd(alloctesttrace_impl);