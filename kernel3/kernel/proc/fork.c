/******************************************************************************/
/* Important Spring 2023 CSCI 402 usage information:                          */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t)kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8);         /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}

/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int do_fork(struct regs *regs)
{
        KASSERT(regs != NULL);
        KASSERT(curproc != NULL);
        KASSERT(curproc->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        proc_t *newproc = proc_create("child");
        vmmap_destroy(newproc->p_vmmap);
        vmmap_t *childMap = vmmap_clone(curproc->p_vmmap);
        newproc->p_vmmap = childMap;

        // clone proc
        if(NULL != newproc->p_cwd){
                vput(newproc->p_cwd);
        }
        newproc->p_cwd = curproc->p_cwd;
        if(NULL != newproc->p_cwd){
                vref(newproc->p_cwd);
        }
        newproc->p_start_brk = curproc->p_start_brk;
        newproc->p_brk = curproc->p_brk;
        newproc->p_vmmap->vmm_proc = newproc;
        newproc->p_state = curproc->p_state;
        newproc->p_status = curproc->p_status;

        /*to do list: For each private mapping, point the vmarea_t at the new shadow object,
        which in turn should point to the original mmobj_t for the vmarea_t.
        This is how you know that the pages corresponding to this mapping are copy-on-write.
        Be careful with reference counts. Also note that for shared mappings, there is no need to copy the mmobj_t.*/
        // for ()
        // {
        // }
        vmarea_t *vma;
        list_iterate_begin(&childMap->vmm_list,vma,vmarea_t,vma_plink) {
                vmarea_t *p=vmmap_lookup(curproc->p_vmmap,vma->vma_start);
                struct mmobj *obj=p->vma_obj;
                struct mmobj *bottom=obj->mmo_un.mmo_bottom_obj;
                if((vma->vma_flags&MAP_TYPE) == MAP_SHARED) {
                        vma->vma_obj=obj;
                        vma->vma_obj->mmo_ops->ref(vma->vma_obj);
                } else {
                        mmobj_t *Shadow=shadow_create();
                        Shadow->mmo_shadowed=obj;
                        Shadow->mmo_un.mmo_bottom_obj=bottom;
                        obj->mmo_ops->ref(obj);
                }
        }list_iterate_end();
        /*
        Unmap the user land page table entries and flush the TLB (using pt_unmap_range() and tlb_flush_all()).
        This is necessary because the parent process might still have some entries marked as "writable", but since we are implementing copy-on-write we would like access to these pages to cause a trap.
        */
        pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();


        regs->r_eax = 0;

        // Set up the new process thread context (kt_ctx)
        kthread_t *newthr = kthread_clone(curthr);
        newthr->kt_ctx.c_pdptr = newproc->p_pagedir;
        newthr->kt_ctx.c_eip = (uint32_t)userland_entry;
        newthr->kt_ctx.c_esp = fork_setup_stack(regs, newthr->kt_kstack);
        newthr->kt_ctx.c_kstack = (uintptr_t)newthr->kt_kstack;
        newthr->kt_ctx.c_kstacksz = DEFAULT_STACK_SIZE;
        //
        newthr->kt_proc = newproc;
        newthr->kt_kstack = strcpy(newthr->kt_kstack, curthr->kt_kstack);
        list_insert_tail(&(newproc->p_threads), &(newthr->kt_plink));

        // set the return value in the child process
        regs->r_eax = newproc->p_pid;

        // Copy the file descriptor table of the parent into the child
        for (int index = 0; index < NFILES; index++)
        {
                newproc->p_files[index] = curproc->p_files[index];
                if (NULL != curproc->p_files[index])
                {
                        fref(curproc->p_files[index]);
                }
        }

        KASSERT(newproc->p_state == PROC_RUNNING);
        KASSERT(newproc->p_pagedir != NULL);
        KASSERT(newthr->kt_kstack != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        sched_make_runnable(newthr);
        // self check
        dbg(DBG_PRINT, "(GRADING3F)\n");
        return newproc->p_pid;
}
