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
 * Copyright (c) 2019, Brian Suchy
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Brian Suchy
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>

#include <nautilus/aspace.h>


#include <aspace/patching.h>


// TODO: path need to be changed
// DATA structure go outside
#include <nautilus/list.h>

#ifdef NAUT_CONFIG_ASPACE_CARAT_REGION_RB_TREE
    #include <aspace/region_tracking/mm_rb_tree.h>

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_SPLAY_TREE
    #include <aspace/region_tracking/mm_splay_tree.h>

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_LINKED_LIST
    #include <aspace/region_tracking/mm_linked_list.h>

#else
    #include <aspace/region_tracking/node_struct.h>

#endif


#ifndef NAUT_CONFIG_DEBUG_ASPACE_CARAT
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-carat: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-carat: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-carat: " fmt, ##args)

#define ASPACE_LOCK_CONF uint8_t _aspace_lock_flags
// Peter fix a bug with this line, TODO: change in paging.c
#define ASPACE_LOCK(a) _aspace_lock_flags = spin_lock_irq_save(&((a))->lock)

// Lock implementation when you have lock outside 
// #define ASPACE_LOCK(a) _aspace_lock_flags = spin_lock_irq_save((a)->lock)
#define ASPACE_TRY_LOCK(a) spin_try_lock_irq_save(&((a)->lock),&_aspace_lock_flags)
#define ASPACE_UNLOCK(a) spin_unlock_irq_restore(&((a)->lock), _aspace_lock_flags)
#define ASPACE_UNIRQ(a) irq_enable_restore(_aspace_lock_flags);


#define ASPACE_NAME(a) ((a)?(a)->aspace->name : "default")
#define THREAD_NAME(t) ((!(t)) ? "(none)" : (t)->is_idle ? "(idle)" : (t)->name[0] ? (t)->name : "(noname)")

// #ifndef NAUT_CONFIG_DEBUG_ASPACE_CARAT
// #define REGION_FORMAT ""
// #define REGION(r)
// #else
// #define REGION_FORMAT "(VA=0x%p to PA=0x%p, len=%lx, prot=%lx)"
// #define REGION(r) (r)->va_start, (r)->pa_start, (r)->len_bytes, (r)->protect.flags
// #endif
#define REGION_FORMAT "(VA=0x%p to PA=0x%p, len=%lx, prot=%lx)"
#define REGION(r) (r)->va_start, (r)->pa_start, (r)->len_bytes, (r)->protect.flags


// ok if r1.permision <= r2.permission
#define PERMISSION_LEQ(r1, r2) \
    (\
        NK_ASPACE_GET_READ(r1->protect.flags)  <= NK_ASPACE_GET_READ(r2->protect.flags) \
    &&  NK_ASPACE_GET_WRITE(r1->protect.flags) <= NK_ASPACE_GET_WRITE(r2->protect.flags) \
    &&  NK_ASPACE_GET_EXEC(r1->protect.flags)  <= NK_ASPACE_GET_EXEC(r2->protect.flags) \
    &&  NK_ASPACE_GET_KERN(r1->protect.flags)  >= NK_ASPACE_GET_KERN(r2->protect.flags) \
    )



typedef struct nk_aspace_carat_thread {
    struct nk_thread * thread_ptr;
    struct list_head thread_node;
} nk_aspace_carat_thread_t;

typedef struct nk_aspace_carat {
    // pointer to the abstract aspace that the
    // rest of the kernel uses when dealing with this
    // address space
    nk_aspace_t *aspace;

    // perhaps you will want to do concurrency control?
    spinlock_t  lock;


    mm_struct_t * mm;

    // Your characteristics
    nk_aspace_characteristics_t chars;

    //We may need the list of threads
    //   struct list_head threads;
    nk_aspace_carat_thread_t threads;

} nk_aspace_carat_t;





/**
 * CARAT_VALID return 1 if region not valid
 * validness = va_addr == pa_addr
 * */
int CARAT_INVALID(nk_aspace_region_t *region) {
    return region != NULL && region->va_start != region->pa_start;
}

static int destroy(void *state) {
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    
    mm_destory(carat->mm);


    /**
     *  Clean up threads record
     *  Note on list implementation: List head here, carat->threads, is a dummy head
     *  The iteration below iterate through everything in the list but not carat->threads
     * */
    struct list_head *cur;
    list_for_each(cur, &(carat->threads.thread_node)) {
        list_del(cur);

        nk_aspace_carat_thread_t * wrapper_ptr = list_entry(cur, nk_aspace_carat_thread_t, thread_node);

        free(wrapper_ptr);
    }

    nk_aspace_unregister(carat->aspace);
    ASPACE_UNLOCK(carat);

    nk_vc_printf("Everything is fine before free carat at %p", carat);
    free(carat);
    
    return 0;
}


/**
 * Although it sounds intuitive to add lock protection to prevent concurrency issue,
 *      this add_thread is only expected be called in move_thread function in aspace.c
 * */
static int add_thread(void *state)
{   
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    // ASPACE_LOCK_CONF;
    // ASPACE_LOCK(carat);


    DEBUG("adding thread %d (%s) to address space %s\n", t->tid,THREAD_NAME(t), ASPACE_NAME(carat));
    
    nk_aspace_carat_thread_t * new_thread_wrapper = (nk_aspace_carat_thread_t *) malloc(sizeof(nk_aspace_carat_thread_t));
    new_thread_wrapper->thread_ptr = t;

    struct list_head * nelm = &new_thread_wrapper->thread_node;
    // carat->threads
    list_add_tail(nelm, &(carat->threads.thread_node));
    
    // ASPACE_UNLOCK(carat);
    return 0;
}

static int remove_thread(void *state)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("removing thread %d (%s) from address space %s\n", t->tid, THREAD_NAME(t), ASPACE_NAME(carat));
    // ASPACE_LOCK_CONF;
    // ASPACE_LOCK(carat);

    struct list_head *cur;
    int failed = 1;
    list_for_each(cur,&(carat->threads.thread_node)) {
    // if (!strcmp(list_entry(cur,struct nk_aspace,aspace_list_node)->name,name)) { 
    //     target = list_entry(cur,struct nk_aspace, aspace_list_node);
    //     break;
    // }
        nk_aspace_carat_thread_t * wrapper_ptr = list_entry(cur, nk_aspace_carat_thread_t, thread_node);
        if (wrapper_ptr->thread_ptr == t) {
            list_del(cur);
            free(wrapper_ptr);
            failed = 0; 
            break;
        }
    }

    // ASPACE_UNLOCK(carat);
    return failed;
}

static int add_region(void *state, nk_aspace_region_t *region)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);
    
    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    DEBUG("adding region %s to address space %s\n", region_buf, ASPACE_NAME(carat));
    
    // check region input validness
    if (CARAT_INVALID(region)) {
        DEBUG("Add region Failed: INVALID input (%s): CARAT expects equal VA and PA\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }
    
    // check if region to insert overlap with tracked region
    nk_aspace_region_t * overlap_ptr = mm_check_overlap(carat->mm, region);
    if (overlap_ptr) {
        DEBUG("Add region Failed: region Overlapping:\n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n", 
            region->va_start, region->pa_start, region->len_bytes, region->protect.flags,
            overlap_ptr->va_start, overlap_ptr->pa_start, overlap_ptr->len_bytes, overlap_ptr->protect.flags
        );
        ASPACE_UNLOCK(carat);
        return -1;
    }

    mm_insert(carat->mm, region);
    mm_show(carat->mm);
    
    ASPACE_UNLOCK(carat);
    return 0;
}

static int remove_region(void *state, nk_aspace_region_t *region) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    DEBUG("adding region %s to address space %s\n", region_buf, ASPACE_NAME(carat));
    
    // check region input validness
    if (CARAT_INVALID(region)) {
        DEBUG("Remove region Failed: INVALID input (%s): CARAT expects equal VA and PA\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }
    
    if (NK_ASPACE_GET_PIN(region->protect.flags)) {
        DEBUG("Cannot remove pinned region%s\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }

    uint8_t check_flag = VA_CHECK | PA_CHECK | LEN_CHECK | PROTECT_CHECK;
    int remove_failed = mm_remove(carat->mm, region, check_flag);

    if (remove_failed) {
        DEBUG("Remove region Failed: %s\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    } 

    // TODO: remove region with CARAT
    ASPACE_UNLOCK(carat);
    return 0;
}

static int protect_region(void *state, nk_aspace_region_t *region, nk_aspace_protection_t *prot) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    if (region == NULL) {
        prot->flags = 0;
        ASPACE_UNLOCK(carat);
        return -1;
    }

    
    int requested_permissions = region->requested_permissions;

    /*
     * requested_permissions will contain the highest level of access (write, read, or nothing) that a program expects from this region.
     * If this attempted region change invalidates that expectation, the protection change is not allowed (panic)
     */

    if (requested_permissions) {
        int is_write = requested_permissions - 1;
        /*
        * If @is_write == 0: we are trying to read the address
        * If @is_write == 1: we are trying to write the address
        * 
        * Note: this is making the assunption that if we have write access 
        * we also have read access for performance
        * 
        * Given this assumption, there are two ways for this to be a legal access:
        * 1. If the memory is writable, either a write or a read is allowed
        * 2. If the memory is readable, only a read is allowed
        * 
        */ 

        int is_memory_writable = NK_ASPACE_GET_WRITE(prot->flags);
        int is_memeory_readable = NK_ASPACE_GET_READ(prot->flags);
        int is_legal_access = is_memory_writable // cond. 1
                            || (is_memeory_readable && !is_write); // cond. 2


        if (!is_legal_access) {
            panic("Changing the permission of the region will invalidate existing usage of the region \n");
        }
    }

    /*
     * The protection change is valid
     */
    region->protect = *prot;

    // TODO: remove region with data structure
    ASPACE_UNLOCK(carat);
    return 0;
}


/**
 * Question: overalapping region is not legal in CARAT, right?
 * */
static int protection_check(void * state, nk_aspace_region_t * region) {

    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    
    DEBUG("Check protection of region %s\n", region_buf);
    // check region input validness
    if (CARAT_INVALID(region)) {
        DEBUG("Protection check Failed: INVALID input (%s): CARAT expects equal VA and PA\n", region_buf);
        ASPACE_UNLOCK(carat);
        return -1;
    }
    
    /**
     * Checking overlap
     * case 1
     * If @region is a subset of an tracked region, 
     *      the overlap_ptr should be deterministic;
     *      overlap_ptr = tracked region that contains @region
     *      return 0
     * 
     * case 2
     * If @region has intersection with several regions (at most two), but no single of them completely contains @region
     *      overlap_ptr may be any of them. 
     *      Yet, it is not a subset of the tracked region. 
     *      return -1
     * 
     * case 3
     * If @region has no inteserction with any tracked regions, 
     *      overlap_ptr = NULL
     *      return -1
     * */
    nk_aspace_region_t * overlap_ptr = mm_check_overlap(carat->mm, region);
    
    region2str(overlap_ptr, region_buf);

    // case 3
    if (overlap_ptr == NULL) {
        DEBUG("Protection check NOT passed! No overalapping region!\n");
        ASPACE_UNLOCK(carat);
        return -1;
    }

    // case 1
    if (overlap_ptr->pa_start <= region->pa_start 
        && overlap_ptr->pa_start + overlap_ptr->len_bytes >= region->pa_start + region->len_bytes 
        && PERMISSION_LEQ(region, overlap_ptr)
    ) { 
        
        DEBUG("Protection check passed! contained by %s\n", region_buf);
        ASPACE_UNLOCK(carat);
        return 0;
    }

    // case 2
    DEBUG("Protection check NOT passed! overlapped region = %s\n", region_buf);
    ASPACE_UNLOCK(carat);
    return -1;
}

static int request_permission(void * state, void * address, int is_write) {

    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    nk_aspace_region_t * region = mm_find_reg_at_addr(carat->mm, (addr_t) address);

    if (region == NULL) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    nk_aspace_protection_t prot = region->protect;
    /*
 	 * If @is_write == 0: we are trying to read the address
	 * If @is_write == 1: we are trying to write the address
	 * 
	 * Note: this is making the assunption that if we have write access 
	 * we also have read access for performance
	 * 
	 * Given this assumption, there are two ways for this to be a legal access:
	 * 1. If the memory is writable, either a write or a read is allowed
	 * 2. If the memory is readable, only a read is allowed
	 * 
 	 */ 

	int is_memory_writable = NK_ASPACE_GET_WRITE(prot.flags);
	int is_memeory_readable = NK_ASPACE_GET_READ(prot.flags);
	int is_legal_access = is_memory_writable // cond. 1
						  || (is_memeory_readable && !is_write); // cond. 2


	if (!is_legal_access) {
		return -1;
	}

    /* 
	 * If the access is valid, we need to store the fact that the region has allowed this access *within* the region
     * When a protection change happens for the region, it will confirm that the outstanding access is still valid.
 	 */ 
    region->requested_permissions = is_write + 1;
    ASPACE_UNLOCK(carat);

    return 0;
}

static int defragment_region(
    void *state, 
    nk_aspace_region_t *cur_region, // also output
    uint64_t new_size,
    void ** free_space_start
){
    /**
     *  Defragmentation illustration
     *  xxx means allocated chunks in the region
     *  -- means unallocated chunks in the region
     * 
     *      xxxx--xxxx--xx----xx 
     *  =>
     *      xxxxxxxxxxxx--------
     *      ^               ^
     * new_region_start   free_space_start
     * */

  DEBUG("defragmentation initialized - (%p,%lu) -> %lu\n",
        cur_region->va_start, cur_region->len_bytes, new_size);

    if (CARAT_INVALID(cur_region)) {
        return -1;
    }

    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);
    
    
    nk_aspace_region_t * reg = mm_contains(carat->mm, cur_region, all_eq_flag );
    
    if (!reg) {
        ERROR("Region"REGION_FORMAT" not existed\n", REGION(reg));
        ASPACE_UNLOCK(carat);
        return -1;
    }

    if (new_size < cur_region->len_bytes)  {
      ASPACE_UNLOCK(carat);
      ERROR("Cannot currently shrink regions\n");
      return -1;
    }

    // nk_aspace_region_t* new_region =  (nk_aspace_region_t*) malloc(sizeof(nk_aspace_region_t));
    nk_aspace_region_t new_region;
    /**
     *  new_region_chunk is the real "drilled" region with length specified by cur_region
     *  We are not super sure if this is the expected/appropriate way to add new region in CARAT.
     *  
     *  Note: we are using the malloc macro defined in mm.h here,
     *      but it's really kmem_malloc which is sperate from Alex's allocator. 
     * */
    void * new_region_chunk = kmem_sys_malloc_specific(new_size,my_cpu_id(),0);

    if (!new_region_chunk) {
      ASPACE_UNLOCK(carat);
      ERROR("cannot allocate new region of size %lu - system defragmentation needed\n",new_size);
      // Do system defrag here, then try again
      return -1;
    }

    DEBUG("new memory allocated at %p\n",new_region_chunk);
    
    new_region = *cur_region;
    new_region.va_start = new_region_chunk;
    new_region.pa_start = new_region_chunk;

    /**
     *  Called nk_carat_move_region to actually do the work of defragmentation
     * */
    if (nk_carat_move_region(cur_region->pa_start, new_region.pa_start, cur_region->len_bytes, free_space_start)) {
      ASPACE_UNLOCK(carat);
      ERROR("failed to move region...\n");
      return -1;
    }

    DEBUG("carat move completed\n");

    mm_remove(carat->mm, cur_region, all_eq_flag );
    mm_insert(carat->mm,  &new_region);


    DEBUG("region data structure updated\n");
    
    *cur_region = new_region;
    
    ASPACE_UNLOCK(carat);
    return 0;
}


static int move_region(void *state, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    // TODO: move region with data structure
    
    // sanity checks
    if (CARAT_INVALID(cur_region)) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    if (CARAT_INVALID(new_region)) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    if (cur_region->len_bytes != new_region->len_bytes) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    void *free_space_start; // don't care
    // call CARAT runtime
    int res = nk_carat_move_region(cur_region->pa_start, new_region->pa_start, cur_region->len_bytes, &free_space_start);
    if (res) {
        ASPACE_UNLOCK(carat);
        return -1;
    }

    // TODO: ask Peter if we need remove and insert new regions here or expect user to make those calls?

    
    ASPACE_UNLOCK(carat);
    return 0;
}

static int switch_from(void *state)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching out address space %s from thread %d (%s)\n",ASPACE_NAME(carat), thread->tid, THREAD_NAME(thread));
    
    return 0;
}

static int switch_to(void *state)
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching in address space %s from thread %d (%s)\n", ASPACE_NAME(carat),thread->tid,THREAD_NAME(thread));
    
    return 0;
}

static int exception(void *state, excp_entry_t *exp, excp_vec_t vec) 
{   
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    if (vec==GP_EXCP) {
    ERROR("general protection fault encountered.... uh...\n");
    ERROR("i have seen things that you people would not believe.\n");
    panic("general protection fault delivered to paging subsystem\n");
    return -1; // will never happen
    }

    if (vec!=PF_EXCP) {
    ERROR("Unknown exception %d delivered to paging subsystem\n",vec);
    panic("Unknown exception delivered to paging subsystem\n");
    return -1; // will never happen
    }

    ASPACE_LOCK_CONF;
    ASPACE_LOCK(carat);

    // TODO: move region with data structure
    ASPACE_UNLOCK(carat);
    return 0;
}


static int print(void *state, int detailed) 
{
    nk_aspace_carat_t *carat = (nk_aspace_carat_t *)state;
    struct nk_thread *thread = get_cur_thread();
    

    // basic info
    nk_vc_printf("%s: carat address space [granularity 0x%lx alignment 0x%lx]\n",
         ASPACE_NAME(carat), carat->chars.granularity, carat->chars.alignment);

    if (detailed) {
        // print region set data structure here
      nk_vc_printf("printing detailed\n");
        mm_show(carat->mm);
        // perhaps print out all the page tables here...
    }

    return 0;
}
static nk_aspace_interface_t carat_interface = {
    .destroy = destroy,
    .add_thread = add_thread,
    .remove_thread = remove_thread,
    .add_region = add_region,
    .remove_region = remove_region,
    .protect_region = protect_region,
    .protection_check = protection_check,
    .move_region = move_region,
    .defragment_region = defragment_region,
    .switch_from = switch_from,
    .switch_to = switch_to,
    .exception = exception,
    .print = print
};

static int get_characteristics(nk_aspace_characteristics_t *c)
{   
    /**
     * CARAT has alignment and granuarity of ??
     * */
    c->alignment = 0;
    c->granularity = 0;
    return 0;
}


static struct nk_aspace *create(char *name, nk_aspace_characteristics_t *c)
{	
    struct naut_info *info = nk_get_nautilus_info();
    nk_aspace_carat_t *carat;
    
    carat = malloc(sizeof(*carat));
    
    if (!carat) {
        ERROR("cannot allocate carat aspace %s\n",name);
        return 0;
    }
    
    memset(carat,0,sizeof(*carat));

    spinlock_init(&(carat->lock));
    // initialize spinlock for carat
    // carat->lock = (spinlock_t *) malloc(sizeof(spinlock_t));
    // if (!carat->lock) {
    //     ERROR("cannot allocate spinlock for carat aspace %s at %p\n", name, carat);
    //     return 0;
    // }
    // spinlock_init(carat->lock);
    INIT_LIST_HEAD(&(carat->threads.thread_node));

    // initialize region dat structure
#ifdef NAUT_CONFIG_ASPACE_CARAT_REGION_RB_TREE
    carat->mm = mm_rb_tree_create();

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_SPLAY_TREE
    carat->mm = mm_splay_tree_create();

#elif defined NAUT_CONFIG_ASPACE_CARAT_REGION_LINKED_LIST
    carat->mm = mm_llist_create();

#else
    carat->mm = mm_struct_create();

#endif

    
    // Define characteristic
    carat->chars = *c;

    carat->aspace = nk_aspace_register(name,
                    // we want both page faults and general protection faults (NO, no GPF)
                    //    NK_ASPACE_HOOK_PF | NK_ASPACE_HOOK_GPF,
                        NK_ASPACE_HOOK_GPF,
                    // our interface functions (see above)
                    &carat_interface,
                    // our state, which will be passed back
                    // whenever any of our interface functiosn are used
                    carat);
    if (!carat->aspace) {
        ERROR("Unable to register carat address space %s\n",name);
        return 0;
    }

    DEBUG("carat address space %s configured and initialized at 0x%p (returning 0x%p)\n", name,carat, carat->aspace);
    
    return carat->aspace;
}


static nk_aspace_impl_t carat = {
                .impl_name = "carat",
                .get_characteristics = get_characteristics,
                .create = create,
};

nk_aspace_register_impl(carat);

static int CARAT_Protection_sanity(char *_buf, void* _priv) {
#define LEN_1KB (0x400UL)
#define LEN_4KB (0x1000UL)
#define LEN_256KB (0x40000UL)
#define LEN_512KB (0x80000UL)

#define LEN_1MB (0x100000UL)
#define LEN_4MB (0x400000UL)
#define LEN_6MB (0x600000UL)
#define LEN_16MB (0x1000000UL)

#define LEN_1GB (0x40000000UL)
#define LEN_4GB (0x100000000UL)

#define ADDR_4GB ((void *) 0x100000000UL)
#define ADDR_8GB ((void *) 0x200000000UL)
#define ADDR_12GB ((void *) 0x300000000UL)
#define ADDR_16GB ((void *) 0x400000000UL)
#define ADDR_UPPER ((void *) 0xffff800000000000UL)
    nk_vc_printf("Start: CARAT Protection check sanity test.\n");
    
    // CARAT ASPACE + Protection Check test
    nk_aspace_characteristics_t c;
    if (nk_aspace_query("carat",&c)) {
        nk_vc_printf("failed to find carat implementation\n");
        goto test_fail;
    }

    nk_aspace_t *carat_aspace = nk_aspace_create("carat", "carat protection check",&c);
    

    if (!carat_aspace) {
        nk_vc_printf("failed to create new address space\n");
        goto test_fail;
    }

    nk_aspace_region_t carat_r0, carat_r1, carat_r2, carat_r3, carat_r4;
    // create a 1-1 region mapping all of physical memory
    // so that the kernel can work when that thread is active
    carat_r0.va_start = 0;
    carat_r0.pa_start = 0;
    carat_r0.len_bytes = 0x100000000UL;  // first 4 GB are mapped
    // set protections for kernel
    // use EAGER to tell paging implementation that it needs to build all these PTs right now
    carat_r0.protect.flags = NK_ASPACE_READ | NK_ASPACE_WRITE | NK_ASPACE_EXEC | NK_ASPACE_PIN | NK_ASPACE_KERN | NK_ASPACE_EAGER;

    // now add the region
    if (nk_aspace_add_region(carat_aspace, &carat_r0)) {
        nk_vc_printf("failed! to add initial eager region to address space\n");
        goto test_fail;
    }

    // should not add region successfully due to overlapping
    carat_r1 = carat_r0;
    if (!nk_aspace_add_region(carat_aspace, &carat_r1)) {
        nk_vc_printf("Failed! check overlap\n");
        goto test_fail;
    }

    // should not add region successfully due to VA != PA for CARAT
    carat_r1.va_start = (void *) 0x200000000UL;
    if (!nk_aspace_add_region(carat_aspace, &carat_r1)) {
        nk_vc_printf("Failed! check CARAT region validness\n");
        goto test_fail;
    }

    // should add region successfully
    carat_r1.pa_start = carat_r1.va_start;
    if (nk_aspace_add_region(carat_aspace, &carat_r1)) {
        nk_vc_printf("Failed! to add second initial eager region to address space\n");
        goto test_fail;
    }

    // should not remove region successfully due to pinnned region
    if (!nk_aspace_remove_region(carat_aspace, &carat_r1)) {
        nk_vc_printf("Failed! Should not remove pinned region\n");
        goto test_fail;
    }

    // should add region sucessfully
    carat_r2 = carat_r0;
    carat_r2.va_start = carat_r2.pa_start = (void *) 0x300000000UL;
    carat_r2.protect.flags = NK_ASPACE_PIN | NK_ASPACE_KERN | NK_ASPACE_EAGER;
    if (nk_aspace_add_region(carat_aspace, &carat_r2)) {
        nk_vc_printf("failed! to add initial eager region to address space\n");
        goto test_fail;
    }

    // should pass protection check
    if (nk_aspace_protection_check(carat_aspace, &carat_r2)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to partial overlap
    carat_r3 = carat_r2;
    carat_r3.va_start = carat_r3.pa_start = carat_r2.va_start + 0x80000000UL;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r3)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to partial overlap
    carat_r3.va_start = carat_r3.pa_start = carat_r1.va_start + 0x80000000UL;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r3)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to no overlapped region
    carat_r3.va_start = carat_r3.pa_start = carat_r2.va_start + 0x100000000UL;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r3)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should pass protection check, subset, permission good
    carat_r4 = carat_r2;
    carat_r4.va_start = carat_r4.pa_start = carat_r2.va_start + 0x80000000UL;
    carat_r4.len_bytes = 0x20000000UL;
    if (nk_aspace_protection_check(carat_aspace, &carat_r4)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to no read permission
    carat_r4.protect.flags = NK_ASPACE_READ | NK_ASPACE_PIN | NK_ASPACE_KERN | NK_ASPACE_EAGER;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r4)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to no write permission
    carat_r4.protect.flags = NK_ASPACE_WRITE | NK_ASPACE_PIN | NK_ASPACE_KERN | NK_ASPACE_EAGER;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r4)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to no exec permission
    carat_r4.protect.flags = NK_ASPACE_EXEC | NK_ASPACE_PIN | NK_ASPACE_KERN | NK_ASPACE_EAGER;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r4)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    // should not pass protection check due to user access
    carat_r4.protect.flags = NK_ASPACE_PIN | NK_ASPACE_EAGER;
    if (!nk_aspace_protection_check(carat_aspace, &carat_r4)) {
        nk_vc_printf("failed! protection check\n");
        goto test_fail;
    }

    /**
     *  Test case for get_permission
     * */
    // nk_aspace_protection_t prot;
    // if (nk_aspace_protection_check(carat_aspace, , &prot) || prot.flags != carat_r0.protect.flags) {
    //     nk_vc_printf("failed! protection check\n");
    //     goto test_fail;
    // }


    nk_vc_printf("Before Destroy\n");
    nk_aspace_destroy(carat_aspace);

    nk_vc_printf("CARAT Protection check sanity test Passed!\n");
    return 0;
test_fail:
    nk_vc_printf("CARAT Protection check sanity test failed!\n");
    return 0;
}

static struct shell_cmd_impl carat_protect_sanity = {
    .cmd      = "carat-protect-sanity",
    .help_str = "Sanity check for CARAT protection",
    .handler  = CARAT_Protection_sanity,
};

nk_register_shell_cmd(carat_protect_sanity);



