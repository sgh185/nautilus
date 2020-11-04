
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
 * Copyright (c) 2019, Hongyi Chen
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Hongyi Chen
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


//
// This is a template for the CS343 paging lab at
// Northwestern University
//
// Please also look at the paging_helpers files!
//
//
//
//

#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/shell.h>
#include <nautilus/cpu.h>

#include <nautilus/aspace.h>
#include <nautilus/random.h>

#include "paging_helpers.h"

#ifdef NAUT_CONFIG_ASPACE_PAGING_REGION_RB_TREE
    #include "mm_rb_tree.h"

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_SPLAY_TREE
    #include "mm_splay_tree.h"

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_LINKED_LIST
    #include "mm_linked_list.h"

#else
    #include "node_struct.h"
    
#endif

#ifdef NAUT_CONFIG_ASPACE_PAGING_REGION_STRUCT_TEST
    #include "struct_test.h"
#endif

#ifdef NAUT_CONFIG_ASPACE_PAGING_PCID
    #include "alloc_pcid.h"
#endif



// include struct_test.h to run data structure test
// #include "struct_test.h"

//
// Add debugging and other optional output to this subsytem
//
#ifndef NAUT_CONFIG_DEBUG_ASPACE_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("aspace-paging: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("aspace-paging: " fmt, ##args)
#define INFO(fmt, args...)   INFO_PRINT("aspace-paging: " fmt, ##args)

#define PAGING_PAGE_SIZE PAGE_SIZE_4KB
// Some macros to hide the details of doing locking for
// a paging address space
#define ASPACE_LOCK_CONF uint8_t _aspace_lock_flags
#define ASPACE_LOCK(a) _aspace_lock_flags = spin_lock_irq_save((a)->lock)
#define ASPACE_TRY_LOCK(a) spin_try_lock_irq_save((a)->lock,&_aspace_lock_flags)
#define ASPACE_UNLOCK(a) spin_unlock_irq_restore((a)->lock, _aspace_lock_flags)
#define ASPACE_UNIRQ(a) irq_enable_restore(_aspace_lock_flags);

// graceful printouts of names
#define ASPACE_NAME(a) ((a)?(a)->aspace->name : "default")
#define THREAD_NAME(t) ((!(t)) ? "(none)" : (t)->is_idle ? "(idle)" : (t)->name[0] ? (t)->name : "(noname)")
#define THRESH PAGE_SIZE_2MB

#define PAGE_SIZE_512GB 0x8000000000UL

#ifdef NAUT_CONFIG_ASPACE_PAGING_1GB
    #define PAGE_1GB_ENABLED 1
#else
    #define PAGE_1GB_ENABLED 0
#endif

#ifdef NAUT_CONFIG_ASPACE_PAGING_2MB
    #define PAGE_2MB_ENABLED 1
#else
    #define PAGE_2MB_ENABLED 0
#endif

// You probably want some sort of data structure that will let you
// keep track of the set of regions you are asked to add/remove/change

// You will want some data structure to represent the state
// of a paging address space
typedef struct nk_aspace_paging {
    // pointer to the abstract aspace that the
    // rest of the kernel uses when dealing with this
    // address space
    nk_aspace_t *aspace;
    
    // perhaps you will want to do concurrency control?
    spinlock_t *  lock;

    // Here you probably will want your region set data structure 
    // What should it be...
    mm_struct_t * paging_mm_struct;
    // Your characteristics
    nk_aspace_characteristics_t chars;

    // The cr3 register contents that reflect
    // the root of your page table hierarchy
    ph_cr3e_t     cr3;

    // The cr4 register contents used by the HW to interpret
    // your page table hierarchy.   We only care about a few bits
// #define CR4_MASK 0xb0ULL // bits 4,5,7
#ifdef NAUT_CONFIG_ASPACE_PAGING_PCID
    #define CR4_MASK 0x200b0ULL // bits 4,5,7, 17
    #define CR4_PCIDE_MASK 0x20000
#else
    #define CR4_MASK 0xb0ULL // bits 4,5,7
#endif

    uint64_t      cr4; 

} nk_aspace_paging_t;



// The function the aspace abstraction will call when it
// wants to destroy your address space
static  int destroy(void *state)
{
    // the pointer it hands you is for the state you supplied
    // when you registered the address space
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("destroying address space %s at %p\n", ASPACE_NAME(p), p);

    ASPACE_LOCK_CONF;

    // lets do that with a lock, perhaps? 
    // DEBUG("Try lock at %p which has value of %lx \n", &p->lock, p->lock);
    spinlock_t * lock_ptr = p->lock;;
    ASPACE_LOCK(p);
    // DEBUG("Acquired lock at %p which has value of %lx \n", &p->lock, p->lock);
    //
    // WRITEME!!    actually do the work
    // 
    // DEBUG("p->paging_mm_struct at %p vptr at %p\n", p->paging_mm_struct, p->paging_mm_struct->vptr);
    mm_destory(p->paging_mm_struct);

    paging_helper_free(p->cr3, 0);
    
#ifdef NAUT_CONFIG_ASPACE_PAGING_PCID
    if(p->cr4 & CR4_PCIDE_MASK) {
        ph_cr3_pcide_t * cr3_pcid_ptr = (ph_cr3_pcide_t *) &p->cr3;
        free_pcid(cr3_pcid_ptr);
    }
#endif
    
    
    nk_aspace_unregister(p->aspace);
    
    // ASPACE_UNLOCK(p);

    free(p);
    DEBUG("Done: destroy the aspace!\n");

    spin_unlock_irq_restore(lock_ptr, _aspace_lock_flags);
    free(lock_ptr);
    return 0;
}

// The function the aspace abstraction will call when it
// is adding a thread to your address space
// do you care? 
static int add_thread(void *state)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("adding thread %d (%s) to address space %s\n", t->tid,THREAD_NAME(t), ASPACE_NAME(p));
    
    return 0;
}
    
    
// The function thscssse aspace abstraction will call when it
// is removing from your address space
// do you care? 
static int remove_thread(void *state)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("removing thread %d (%s) from address space %s\n", t->tid, THREAD_NAME(t), ASPACE_NAME(p));
    
    return 0;
}



// The function the aspace abstraction will call when it
// is adding a region to your address space

ph_pf_access_t access_from_region (nk_aspace_region_t *region) {
    ph_pf_access_t access;
    access.val = 0;
    
    access.write = NK_ASPACE_GET_WRITE(region->protect.flags);
    access.user = !NK_ASPACE_GET_KERN(region->protect.flags);
    access.ifetch = NK_ASPACE_GET_EXEC(region->protect.flags);

    return access;
}


int clear_cache (nk_aspace_paging_t *p, nk_aspace_region_t *region, uint64_t threshold) {
    
    // if we are editing the current address space of this cpu, then we
    // might need to flush the TLB here.   We can do that with a cr3 write
    // like: write_cr3(p->cr3.val);

    // if this aspace is active on a different cpu, we might need to do
    // a TLB shootdown here (out of scope of class)
    // a TLB shootdown is an interrupt to a remote CPU whose handler
    // flushes the TLB

    if (p->aspace == get_cpu()->cur_aspace) {
            write_cr3(p->cr3.val);
            DEBUG("flush TLB DONE!\n");
        // if (region->len_bytes > threshold) {
        //     write_cr3(p->cr3.val);
        //     DEBUG("flush TLB DONE!\n");
        // } else {
        //     uint64_t offset = 0;
        //     while (offset < region->len_bytes) {
        //         invlpg((addr_t)region->va_start + (addr_t) offset);
        //         offset = offset + p->chars.granularity;
        //     }
        //     DEBUG("virtual address cache from %016lx to %016lx are invalidated\n", 
        //         region->va_start, 
        //         region->va_start + region->len_bytes
        //     );
        // }
    } else {
        // TLB shootdown???
    }
    return 0;
}

int region_align_check(nk_aspace_paging_t *p, nk_aspace_region_t *region) {
    // sanity check for region validness
    if ((addr_t) region->va_start % p->chars.alignment != 0) {
        ERROR("region VA_start=0x%p expect to have alignment of %lx", region->va_start, p->chars.alignment);
        return -1;
    }

    if ((addr_t) region->pa_start % p->chars.alignment != 0) {
        ERROR("region PA_start=0x%p expect to have alignment of %lx", region->pa_start, p->chars.alignment);
        return -1;
    }

    if ((addr_t) region->len_bytes % p->chars.granularity != 0) {
        ERROR("region len_bytes=0x%lx expect to have granularity of %lx", region->len_bytes, p->chars.granularity);
        return -1;
    }
    return 0;
}


int eager_drill_wrapper(nk_aspace_paging_t *p, nk_aspace_region_t *region) {
    /*
        Only to be called if region passed the following check:
            1. alignment and granularity check 
            2. region overlap or other region validnesss check (involved using p->paging_mm_struct)
            3. region allocation must be eager
    */

    ph_pf_access_t access_type = access_from_region(region);

    addr_t vaddr = (addr_t) region->va_start;
    addr_t paddr = (addr_t)region->pa_start;
    uint64_t remained = region->len_bytes;
    addr_t va_end = (addr_t) region->va_start + region->len_bytes;

    uint64_t page_granularity = 0;
    int ret = 0;
    int (*paging_helper_drill) (ph_cr3e_t cr3, addr_t vaddr, addr_t paddr, ph_pf_access_t access_type);

    while (vaddr < va_end) {
        if (
            PAGE_1GB_ENABLED && 
            vaddr % PAGE_SIZE_1GB == 0 && 
            paddr % PAGE_SIZE_1GB == 0 && 
            remained >= PAGE_SIZE_1GB
        ) {
            paging_helper_drill = &paging_helper_drill_1GB;
            page_granularity = PAGE_SIZE_1GB;
        } 
        else if (
            PAGE_2MB_ENABLED && 
            vaddr % PAGE_SIZE_2MB == 0 && 
            paddr % PAGE_SIZE_2MB == 0 && 
            remained >= PAGE_SIZE_2MB 
        ) {
            paging_helper_drill = &paging_helper_drill_2MB;
            page_granularity = PAGE_SIZE_2MB;
        } 
        else if (
            vaddr % PAGE_SIZE_4KB == 0 && 
            paddr % PAGE_SIZE_4KB == 0 && 
            remained >= PAGE_SIZE_4KB 
        ) {
            // vaddr % PAGE_SIZE_4KB == 0
            // must be the case as we require 4KB alignment
            paging_helper_drill = &paging_helper_drill_4KB;
            page_granularity = PAGE_SIZE_4KB;
        } else {
            char region_buf[REGION_STR_LEN];
            region2str(region, region_buf);
            ERROR("Region %s doesnot meet drill requirement at vaddr=0x%p and paddr=0x%p\n", region_buf, vaddr, paddr);
            return -1;
        }

        ret = (*paging_helper_drill) (p->cr3, vaddr, paddr, access_type);

        if (ret < 0) {
            ERROR("Failed to drill at virtual address=%p"
                    " physical adress %p"
                    " and ret code of %d"
                    " page_granularity = %lx\n",
                    vaddr, paddr, ret, page_granularity
            );
            return ret;
        }

        vaddr += page_granularity;
        paddr += page_granularity;
        remained -= page_granularity;
    }

    return ret;
}

static int add_region(void *state, nk_aspace_region_t *region)
{   
    
    // add the new node into region_list
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    char region_buf[REGION_STR_LEN];
    region2str(region, region_buf);
    DEBUG("adding region %s to address space %s\n", region_buf, ASPACE_NAME(p));

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!!
    // DEBUG("cr3 = %016lx\tcr4 = %016lx\n", p->cr3, p->cr4);
    // DEBUG("alignment = %lx\talignment = %lx\n", p->chars.alignment, p->chars.granularity);
    // first you should sanity check the region and then place it into
    // your region data structure
    int ret = 0;
    
    ret = region_align_check(p, region);
    if (ret < 0) {
        ASPACE_UNLOCK(p);
        return ret;
    }

    // sanity check to be sure it doesn't overlap an existing region...
    nk_aspace_region_t * overlap_ptr = mm_check_overlap(p->paging_mm_struct, region);
    if (overlap_ptr) {
        DEBUG("region Overlapping:\n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n"
                "\t(va=%016lx pa=%016lx len=%lx, prot=%lx) \n", 
            region->va_start, region->pa_start, region->len_bytes, region->protect.flags,
            overlap_ptr->va_start, overlap_ptr->pa_start, overlap_ptr->len_bytes, overlap_ptr->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }
    // DEBUG("no region overlapped\n");

    if (NK_ASPACE_GET_EAGER(region->protect.flags)) {
	
        // an eager region means that we need to build all the corresponding
        // page table entries right now, before we return

        // DRILL THE PAGE TABLES HERE
        ret = eager_drill_wrapper(p, region);
        if (ret < 0) {
            ASPACE_UNLOCK(p);
            return ret;
        }
    }
    else {
        // lazy drilling 
        // nothing to do
        nk_vc_printf("lazy drilling!\n");
    }

    // DEBUG("before mm_insert\n");

    mm_insert(p->paging_mm_struct, region);
    // DEBUG("after mm_insert\n");
    mm_show(p->paging_mm_struct);

    clear_cache(p, region, THRESH);
    
    ASPACE_UNLOCK(p);
    
    return 0;
}

// The function the aspace abstraction will call when it
// is removing a region from your address space
static int remove_region(void *state, nk_aspace_region_t *region)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("removing region (va=%016lx pa=%016lx len=%lx) "
            "from address space %s\n", 
            region->va_start, region->pa_start, region->len_bytes,
            ASPACE_NAME(p)
    );

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!
    
    // first, find the region in your data structure
    // it had better exist and be identical.
    if (NK_ASPACE_GET_PIN(region->protect.flags)) {
        char buf[REGION_STR_LEN];
        region2str(region, buf);
        ERROR("Cannot remove pinned region%s\n", buf);
        ASPACE_UNLOCK(p);
        return -1;
    }

    uint8_t check_flag = VA_CHECK | PA_CHECK | LEN_CHECK | PROTECT_CHECK;
    int remove_failed = mm_remove(p->paging_mm_struct, region, check_flag);

    if (remove_failed) {
        DEBUG("region to remove \
            (va=%016lx pa=%016lx len=%lx, prot=%lx) not FOUND\n", 
            region->va_start, 
            region->pa_start, 
            region->len_bytes,
            region->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }    

    // next, remove all corresponding page table entries that exist
    ph_pf_access_t access_type = access_from_region(region);
    uint64_t offset = 0;
    
    while (offset < region->len_bytes){
        uint64_t *entry;
        addr_t virtaddr = (addr_t) region->va_start + (addr_t) offset;
        int ret = paging_helper_walk(p->cr3, virtaddr, access_type, &entry);
        if (ret == 1 || ret == -1) {
            ((ph_pte_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_4KB;
        } 
        else if (ret == 2 || ret == -2) {
            ((ph_pde_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_2MB;
        } 
        else if (ret == 3 || ret == -3 ) {
            ((ph_pdpe_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_1GB;
        } 
        else if (ret == -4) {
            ((ph_pml4e_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_512GB;
        } 
        else {
            panic("unexpected return from page walking = %d\n", ret);
        }
    }
    
    clear_cache(p, region, THRESH);

    ASPACE_UNLOCK(p);

    return 0;

}
   
// The function the aspace abstraction will call when it
// is changing the protections of an existing region
static int protect_region(void *state, nk_aspace_region_t *region, nk_aspace_protection_t *prot)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("protecting region" 
            "(va=%016lx pa=%016lx len=%lx, prot=%lx)" 
            "from address space %s"
            "to new protection = %lx\n", 
            region->va_start, region->pa_start, region->len_bytes, region->protect.flags,
            ASPACE_NAME(p),
            prot->flags
    );

    DEBUG("Old protection details:" 
        "(read=%d write=%d exec=%d pin=%d kern=%d swap=%d eager=%d)\n",
        NK_ASPACE_GET_READ(region->protect.flags),
        NK_ASPACE_GET_WRITE(region->protect.flags),
        NK_ASPACE_GET_EXEC(region->protect.flags),
        NK_ASPACE_GET_PIN(region->protect.flags), 
        NK_ASPACE_GET_KERN(region->protect.flags), 
        NK_ASPACE_GET_SWAP(region->protect.flags), 
        NK_ASPACE_GET_EAGER(region->protect.flags)
    );

    DEBUG("new protection details:" 
        "(read=%d write=%d exec=%d pin=%d kern=%d swap=%d eager=%d)\n",
        NK_ASPACE_GET_READ(prot->flags),
        NK_ASPACE_GET_WRITE(prot->flags),
        NK_ASPACE_GET_EXEC(prot->flags),
        NK_ASPACE_GET_PIN(prot->flags), 
        NK_ASPACE_GET_KERN(prot->flags), 
        NK_ASPACE_GET_SWAP(prot->flags), 
        NK_ASPACE_GET_EAGER(prot->flags)
    );

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!
    int ret = region_align_check(p, region);
    if (ret < 0) {
        ASPACE_UNLOCK(p);
        return ret;
    }

    nk_aspace_region_t new_prot_wrapper = *region;
    new_prot_wrapper.protect = *prot;
    // first, find the region in your data structure
    // it had better exist and be identical except for protections
    uint8_t check_flag = VA_CHECK | LEN_CHECK | PA_CHECK;
    nk_aspace_region_t* reg_ptr = mm_update_region(p->paging_mm_struct, region, &new_prot_wrapper, check_flag);
    
    if (reg_ptr == NULL) {
        DEBUG("region to update protect \
             (va=%016lx pa=%016lx len=%lx, prot=%lx) not FOUND", 
            region->va_start, 
            region->pa_start, 
            region->len_bytes,
            region->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }
    // next, update the region protections from your data structure
    ph_pf_access_t access_type = access_from_region(region);
    ph_pf_access_t new_access = access_from_region(reg_ptr);

    if (NK_ASPACE_GET_EAGER(reg_ptr->protect.flags)) {
        ret = eager_drill_wrapper(p, reg_ptr);
        if (ret < 0) {
            ASPACE_UNLOCK(p);
            return ret;
        }
    } else if (access_type.val != new_access.val) {
            // next, update all corresponding page table entries that exist
        uint64_t offset = 0;

        while (offset < reg_ptr->len_bytes){
            uint64_t *entry;
            addr_t virtaddr = (addr_t) region->va_start + (addr_t) offset;
            int ret = paging_helper_walk(p->cr3, virtaddr, access_type, &entry);
            
            if (ret == 1 || ret == -1) {
                ((ph_pte_t *) entry)->present = 0;
                offset = offset + PAGE_SIZE_4KB;
            } 
            else if (ret == 2 || ret == -2) {
                ((ph_pde_t *) entry)->present = 0;
                offset = offset + PAGE_SIZE_2MB;
            } 
            else if (ret == 3 || ret == -3 ) {
                ((ph_pdpe_t *) entry)->present = 0;
                offset = offset + PAGE_SIZE_1GB;
            } 
            else if (ret == -4) {
                ((ph_pml4e_t *) entry)->present = 0;
                offset = offset + PAGE_SIZE_512GB;
            } 
            else {
                panic("unexpected return from page walking = %d\n", ret);
            }
            
            if(ret > 0) {
                perm_set(entry, new_access);
            }
            
        }
    }
    // next, if we are editing the current address space of this cpu,
    // we need to either invalidate individual pages using invlpg()
    // or do a full TLB flush with a write to cr3.
    clear_cache(p, reg_ptr, THRESH);
    // next, if this address space is active on a different cpu, we
    // would need to do a TLB shootdown for that cpu
    ASPACE_UNLOCK(p);

    return 0;
}

static int move_region(void *state, nk_aspace_region_t *cur_region, nk_aspace_region_t *new_region)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;

    DEBUG("moving region (va=%016lx pa=%016lx len=%lx, prot=%lx)"
            "in address space %s" 
            "to (va=%016lx pa=%016lx len=%lx, prot=%lx)\n", 
            cur_region->va_start, cur_region->pa_start, cur_region->len_bytes, cur_region->protect.flags,
            ASPACE_NAME(p),
            new_region->va_start, new_region->pa_start, new_region->len_bytes, new_region->protect.flags
    );

    ASPACE_LOCK_CONF;

    ASPACE_LOCK(p);

    // WRITE ME!
    
    // first, find the region in your data structure
    // it had better exist and be identical except for the physical addresses
    int ret = region_align_check(p, cur_region);
    if (ret < 0) {
        ASPACE_UNLOCK(p);
        return ret;
    }
    
    ret = region_align_check(p, new_region);
    if (ret < 0) {
        ASPACE_UNLOCK(p);
        return ret;
    }

    
    uint8_t check_flag = VA_CHECK | LEN_CHECK | PROTECT_CHECK;
    int reg_eq = region_equal(cur_region, new_region, check_flag);
    if (!reg_eq) {
        DEBUG("regions differ in attributes other than physical address!\n");
        ASPACE_UNLOCK(p);
        return -1;
    }

    if (NK_ASPACE_GET_PIN(cur_region->protect.flags)) {
        char buf[REGION_STR_LEN];
        region2str(cur_region, buf);
        ERROR("Cannot move pinned region%s\n", buf);
        ASPACE_UNLOCK(p);
        return -1;
    }

    nk_aspace_region_t* reg_ptr = mm_update_region(
                                    p->paging_mm_struct, 
                                    cur_region,
                                    new_region,
                                    check_flag
                                );
    if (!reg_ptr) {
        DEBUG(
            "region to update"
            "(va=%016lx pa=%016lx len=%lx, prot=%lx) not FOUND", 
            cur_region->va_start, 
            cur_region->pa_start, 
            cur_region->len_bytes,
            cur_region->protect.flags
        );
        ASPACE_UNLOCK(p);
        return -1;
    }
    // next, update the region in your data structure
    // you can assume that the caller has done the work of copying the memory
    // contents to the new physical memory

    // next, update all corresponding page table entries that exist
    ph_pf_access_t access_type = access_from_region(cur_region);
    uint64_t offset = 0;

    // invalidate pages for cur_region
    while (offset < cur_region->len_bytes){
        uint64_t *entry;
        addr_t virtaddr = (addr_t) cur_region->va_start + (addr_t) offset;
        int ret = paging_helper_walk(p->cr3, virtaddr, access_type, &entry);
        if (ret == 1 || ret == -1) {
            ((ph_pte_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_4KB;
        } 
        else if (ret == 2 || ret == -2) {
            ((ph_pde_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_2MB;
        } 
        else if (ret == 3 || ret == -3 ) {
            ((ph_pdpe_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_1GB;
        } 
        else if (ret == -4) {
            ((ph_pml4e_t *) entry)->present = 0;
            offset = offset + PAGE_SIZE_512GB;
        } 
        else {
            panic("unexpected return from page walking = %d\n", ret);
        }
    }
    // next, if we are editing the current address space of this cpu,
    // we need to either invalidate individual pages using invlpg()
    // or do a full TLB flush with a write to cr3.

    clear_cache(p, cur_region, THRESH );
    // next, if this address space is active on a different cpu, we
    // would need to do a TLB shootdown for that cpu


    // ADVANCED VERSION: allow for splitting the region - if cur_region
    // is a subset of some region, then split that region, and only move
    // the affected addresses.   The granularity of this is that reported
    // in the aspace characteristics (i.e., page granularity here).
    

    // next, remove all corresponding page table entries that exist 


    if (NK_ASPACE_GET_EAGER(new_region->protect.flags)) {
	// an eager region means that we need to build all the corresponding
	// page table entries right now, before we return
	// DRILL THE PAGE TABLES HERE
        ret = eager_drill_wrapper(p, new_region);
        if (ret < 0) {
            ASPACE_UNLOCK(p);
            return ret;
        }
    } else {
        // nothing to do for uneager region
    }
    

    mm_show(p->paging_mm_struct);
    ASPACE_UNLOCK(p);

    return 0;
}


// called by the address space abstraction when it is switching away from
// the noted address space.   This is part of the thread context switch.
// do you care?
static int switch_from(void *state)
{
    struct nk_aspace_paging *p = (struct nk_aspace_paging *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching out address space %s from thread %d (%s)\n",ASPACE_NAME(p), thread->tid, THREAD_NAME(thread));
    
    return 0;
}

// called by the address space abstraction when it is switching to the
// noted address space.  This is part of the thread context switch.
static int switch_to(void *state)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    DEBUG("switching in address space %s from thread %d (%s)\n", ASPACE_NAME(p),thread->tid,THREAD_NAME(thread));
    
    // Here you will need to install your page table hierarchy
    // first point CR3 to it
    write_cr3(p->cr3.val);

    // next make sure the interpretation bits are set in cr4
    uint64_t cr4 = read_cr4();
    cr4 &= ~CR4_MASK;
    cr4 |= p->cr4;
    write_cr4(cr4);
    
    return 0;
}

// called by the address space abstraction when a page fault or a
// general protection fault is encountered in the context of the
// current thread
//
// exp points to the hardware interrupt frame on the stack
// vec indicates which vector happened
//
static int exception(void *state, excp_entry_t *exp, excp_vec_t vec)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *thread = get_cur_thread();
    
    // DEBUG("exception 0x%x for address space %s in context of thread %d (%s)\n",vec,ASPACE_NAME(p),thread->tid,THREAD_NAME(thread));
    
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
    
    // It must be a page fault
    
    // find out what the fault address and the fault reason
    uint64_t virt_addr = read_cr2();
    ph_pf_error_t  error; error.val = exp->error_code;
    DEBUG("Page fault at virt_addr = %llx, error = %llx\n", virt_addr, error.val);
    
    DEBUG("Page fault at error.present = %x, "
            "error.write = %x " 
            "error.user = %x " 
            "error.rsvd_access = %x " 
            "error.ifetch = %x \n", 
            error.present,
            error.write,
            error.user,
            error.rsvd_access,
            error.ifetch    
    );
    ASPACE_LOCK_CONF;
    // DEBUG("try to lock at %p\n", &p->lock);
    ASPACE_LOCK(p);

    //
    // WRITE ME!
    //
    // DEBUG("looking for region contains %lx\n", virtaddr);
    nk_aspace_region_t * region = mm_find_reg_at_addr(p->paging_mm_struct, (addr_t) virt_addr);
    if (region == NULL) {
        // if there is no such region, this is an unfixable fault
        //   if this is a user thread, we now would signal it or kill it
        //   if it's a kernel thread, the kernel should panic
        //   if it's within an interrupt handler, the kernel should panic
        panic("Page Fault at %p, but no matching region found\n", virt_addr);
        ASPACE_UNLOCK(p);
        return -1;
    }
    // DEBUG("region found at%p\n", region);
    // Now find the region corresponding to this address
    // Is the problem that the page table entry is not present?
    // if so, drill the entry and then return from the function
    // so the faulting instruction can try agai
    //    This is the lazy construction of the page table entries

    // Assuming the page table entry is present, check the region's
    // protections and compare to the error code
    
    // the region is returned from the data structure, 
    // so expected to pass the alignment and granularity check

    int ret;
    ph_pf_access_t access_type = access_from_region(region);


    if(!error.present) {
        addr_t vaddr, paddr, vaddr_try, paddr_try;
        uint64_t remained, remained_try, page_granularity;
        
        addr_t va_start = (addr_t) region->va_start;
        addr_t pa_start = (addr_t) region->pa_start;

        int (*paging_helper_drill) (ph_cr3e_t cr3, addr_t vaddr, addr_t paddr, ph_pf_access_t access_type);
        

        addr_t vaddr_1GB_align = virt_addr - ADDR_TO_OFFSET_1GB(virt_addr);
        addr_t vaddr_2MB_align = virt_addr - ADDR_TO_OFFSET_2MB(virt_addr);
        addr_t vaddr_4KB_align = virt_addr - ADDR_TO_OFFSET_4KB(virt_addr);

        
        if (PAGE_1GB_ENABLED && vaddr_1GB_align >= va_start) {
            vaddr = vaddr_1GB_align;
            paddr = vaddr - va_start + pa_start;
            remained = region->len_bytes - (vaddr - va_start);
            
            if (
                vaddr % PAGE_SIZE_1GB == 0 && 
                paddr % PAGE_SIZE_1GB == 0 && 
                remained >= PAGE_SIZE_1GB
            ) {
                paging_helper_drill = &paging_helper_drill_1GB;
                page_granularity = PAGE_SIZE_1GB;
                
                goto drill;
            }
        }

        if (PAGE_2MB_ENABLED && vaddr_2MB_align >= va_start) {
            vaddr = vaddr_2MB_align;
            paddr = vaddr - va_start + pa_start;
            remained = region->len_bytes - (vaddr - va_start);
            
            if (
                vaddr % PAGE_SIZE_2MB == 0 && 
                paddr % PAGE_SIZE_2MB == 0 && 
                remained >= PAGE_SIZE_2MB
            ) {
                paging_helper_drill = &paging_helper_drill_2MB;
                page_granularity = PAGE_SIZE_2MB;
                goto drill;
            }
        }

        vaddr = vaddr_4KB_align;
        paddr = vaddr - va_start + pa_start;
        remained = region->len_bytes - (vaddr - va_start);
        paging_helper_drill = &paging_helper_drill_4KB;
        page_granularity = PAGE_SIZE_4KB;

    drill:

        ret = (*paging_helper_drill) (p->cr3, vaddr, paddr, access_type);

        if (ret < 0) {
            ERROR("Failed to drill at virtual address=%p"
                    " physical adress %p"
                    " and ret code of %d"
                    " page_granularity = %lx\n",
                    vaddr, paddr, ret, page_granularity
            );
            ASPACE_UNLOCK(p);
            return ret;
        }
        
    } else{
        
        int ok = (access_type.write >= error.write) && 
                    (access_type.user>= error.user) && 
                    (access_type.ifetch >= error.ifetch);

        DEBUG(
            "region.protect.write=%d, error.write=%d\n" 
            "region.protect.user=%d, error.user=%d\n" 
            "region.protect.ifetch=%d, error.ifetch=%d\n",
            access_type.write, error.write,
            access_type.user, error.user,
            access_type.ifetch, error.ifetch
        );

        if(ok){
            ASPACE_UNLOCK(p);
            panic("weird Page fault with permission ok and page present\n");
            return 0;
        } else{
            ASPACE_UNLOCK(p);
            panic("Permission not allowed\n");
            return -1;
        }
    }
    // if the region has insufficient permissions for the request,
    // then this is an unfixable fault
    //   if this is a user thread, we now would signal it or kill it
    //   if it's a kernel thread, the kernel should panic
    //   if it's within an interrupt handler, the kernel should panic
    
    ASPACE_UNLOCK(p);
    
    return 0;
}
    
// called by the address space abstraction when it wants you
// to print out info about the address space.  detailed is
// nonzero if it wants a detailed output.  Use the nk_vc_printf()
// function to print here
static int print(void *state, int detailed)
{
    nk_aspace_paging_t *p = (nk_aspace_paging_t *)state;
    struct nk_thread *thread = get_cur_thread();
    

    // basic info
    nk_vc_printf("%s: paging address space [granularity 0x%lx alignment 0x%lx]\n"
		 "   CR3:    %016lx  CR4m: %016lx\n",
		 ASPACE_NAME(p), p->chars.granularity, p->chars.alignment, p->cr3.val, p->cr4);

    if (detailed) {
        // print region set data structure here
        mm_show(p->paging_mm_struct);
        // perhaps print out all the page tables here...
    }

    return 0;
}    

//
// This structure binds together your interface functions
// with the interface definition of the address space abstraction
// it will be used later in registering an address space
//
static nk_aspace_interface_t paging_interface = {
    .destroy = destroy,
    .add_thread = add_thread,
    .remove_thread = remove_thread,
    .add_region = add_region,
    .remove_region = remove_region,
    .protect_region = protect_region,
    .move_region = move_region,
    .switch_from = switch_from,
    .switch_to = switch_to,
    .exception = exception,
    .print = print
};


//
// The address space abstraction invokes this function when
// someone asks about your implementations characterstics
//
static int   get_characteristics(nk_aspace_characteristics_t *c)
{
    // you must support 4KB page granularity and alignment
    c->granularity = c->alignment = PAGE_SIZE_4KB;
    
    return 0;
}


//
// The address space abstraction invokes this function when
// someone wants to create a new paging address space with the given
// name and characteristics
//
static struct nk_aspace * create(char *name, nk_aspace_characteristics_t *c)
{
    struct naut_info *info = nk_get_nautilus_info();
    nk_aspace_paging_t *p;
    
    p = malloc(sizeof(*p));
    
    if (!p) {
	ERROR("cannot allocate paging aspace %s\n",name);
	return 0;
    }

    // DEBUG("Allocate paging aspace at %p with size of %x\n", p, sizeof(*p));
    
    memset(p,0,sizeof(*p));
    
    p->lock = (spinlock_t *) malloc(sizeof(spinlock_t));
    spinlock_init(p->lock);


    // initialize your region set data structure here!

#ifdef NAUT_CONFIG_ASPACE_PAGING_REGION_RB_TREE
    p->paging_mm_struct = mm_rb_tree_create();

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_SPLAY_TREE
    p->paging_mm_struct = mm_splay_tree_create();

#elif defined NAUT_CONFIG_ASPACE_PAGING_REGION_LINKED_LIST
    p->paging_mm_struct = mm_llist_create();

#else
    p->paging_mm_struct = mm_struct_create();
    
#endif
    
    if(paging_helper_create(&(p->cr3)) == -1){
	    ERROR("unable create aspace cr3 in address space %s\n", name);
        return 0;
    }

    
    // note also the cr4 bits you should maintain
    DEBUG("default cr4=%lx\n", nk_paging_default_cr4());
    p->cr4 = nk_paging_default_cr4() & CR4_MASK;
    DEBUG("p->cr4=%lx\n", p->cr4);

#ifdef NAUT_CONFIG_ASPACE_PAGING_PCID
    if(p->cr4 & CR4_PCIDE_MASK) {
        ph_cr3_pcide_t * cr3_pcid_ptr = (ph_cr3_pcide_t *) &p->cr3;
        int res = alloc_pcid(cr3_pcid_ptr);
        if (res) {
            panic("Fail to acquire pcid!\n");
        }
        
        DEBUG("acquired pcid = %lx\n", cr3_pcid_ptr->pcid);
    }
#endif
    p->chars = *c;

    // if we supported address spaces other than long mode
    // we would also manage the EFER register here

    // Register your new paging address space with the address space
    // space abstraction
    // the registration process returns a pointer to the abstract
    // address space that the rest of the system will use
    p->aspace = nk_aspace_register(name,
				   // we want both page faults and general protection faults (NO, no GPF)
				//    NK_ASPACE_HOOK_PF | NK_ASPACE_HOOK_GPF,
                    NK_ASPACE_HOOK_PF,
				   // our interface functions (see above)
				   &paging_interface,
				   // our state, which will be passed back
				   // whenever any of our interface functiosn are used
				   p);
    
    if (!p->aspace) {
	ERROR("Unable to register paging address space %s\n",name);
	return 0;
    }
    
    DEBUG("paging address space %s configured and initialized at 0x%p (returning 0x%p)\n", name,p, p->aspace);
    
#ifdef NAUT_CONFIG_ASPACE_PAGING_REGION_STRUCT_TEST
    rbtree_llist_test();
    splay_llist_test();
    rbtree_splay_test();
#endif
    // you are returning the aspace pointer
    return p->aspace; 
}

//
// This structure binds together the interface functions of our
// implementation with the relevant interface definition
static nk_aspace_impl_t paging = {
				.impl_name = "paging",
				.get_characteristics = get_characteristics,
				.create = create,
};


// this does linker magic to populate a table of address space
// implementations by including this implementation
nk_aspace_register_impl(paging);
