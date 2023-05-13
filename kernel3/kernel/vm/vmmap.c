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

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        /* NOT_YET_IMPLEMENTED("VM: vmmap_create");
        return NULL;*/
        vmmap_t *map;
        map=(vmmap_t*)slab_obj_alloc(vmmap_allocator);
        list_init(&(map->vmm_list));
        map->vmm_proc=NULL;
        return map;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        //NOT_YET_IMPLEMENTED("VM: vmmap_destroy");
        KASSERT(NULL!=map);
        dbg(DBG_PRINT, "(GRADING3A 3.a)\n");
        vmarea_t* vma=NULL;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                list_remove(&(vma->vma_plink));
                list_remove(&(vma->vma_olink));
                vmarea_free(vma);
        }list_iterate_end();
        slab_obj_free(vmmap_allocator, map);

}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
      //  NOT_YET_IMPLEMENTED("VM: vmmap_insert");
      KASSERT(NULL!=map && NULL!=newvma);
      KASSERT(NULL==newvma->vma_vmmap);
      KASSERT(newvma->vma_start<newvma->vma_end);
      KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
      dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
      vmarea_t *vma, *tmp;
      list_iterate_begin(&(map->vmm_list), vma, vmarea_t, vma_plink) {
              if(vma->vma_start>=newvma->vma_end) {
                      tmp=vma;
                      list_insert_before(&(tmp->vma_plink), &(newvma->vma_plink));
              }
      }list_iterate_end();
      list_insert_tail(&(map->vmm_list),&(newvma->vma_plink));
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        /* NOT_YET_IMPLEMENTED("VM: vmmap_find_range");
        return -1;*/
        vmarea_t *vma;
        vmarea_t *trav=NULL;
        uint32_t higher_bound=ADDR_TO_PN(USER_MEM_HIGH);
        uint32_t lower_bound=ADDR_TO_PN(USER_MEM_LOW);
        uint32_t bounder;
        if(dir==VMMAP_DIR_HILO) {
                list_iterate_begin(&(map->vmm_list), vma, vmarea_t,vma_plink) {
                        if(trav == NULL) {
                                if(higher_bound-vma->vma_end>=npages) {
                                        bounder=higher_bound-npages;
                                        return bounder;
                                }
                        } else {
                                if(trav->vma_start-vma->vma_end>=npages) {
                                        uint32_t bounder=trav->vma_start-npages;
                                        return bounder;
                                }
                        }
                        trav=vma;
                }list_iterate_end();
        }
        return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        /* 
        NOT_YET_IMPLEMENTED("VM: vmmap_lookup");
        return NULL;*/
        KASSERT(NULL!=map);
        dbg(DBG_PRINT, "(GRADING3A 3.C)\n");
        vmarea_t *vma;
        list_iterate_begin(&(map->vmm_list),vma,vmarea_t,vma_plink) {
                if(vfn>=vma->vma_start && vfn<vma->vma_end) {
                        return vma;
                }
        }list_iterate_end();
        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        /* NOT_YET_IMPLEMENTED("VM: vmmap_clone");
        return NULL;*/
        vmmap_t* clone_map;
        clone_map=vmmap_create();
        vmarea_t* vma;
        list_iterate_begin(&(map->vmm_list), vma, vmarea_t, vma_plink) {
                if(!vma) return NULL;
                vmarea_t* obj=vmarea_alloc();
                obj->vma_start=vma->vma_start;
                obj->vma_end=vma->vma_end;
                obj->vma_flags=vma->vma_flags;
                obj->vma_off=vma->vma_off;
                obj->vma_prot=vma->vma_prot;
                list_init(&(obj->vma_plink));
                list_init(&(obj->vma_olink));
                list_insert_tail(&(clone_map->vmm_list),&(obj->vma_plink));
        }list_iterate_end();
        return clone_map;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_map");
        return -1;*/
        KASSERT(NULL!=map);
        KASSERT(0<npages);
        KASSERT((MAP_SHARED&flags)||(MAP_PRIVATE&flags));
        KASSERT((0==lopage)||(ADDR_TO_PN(USER_MEM_LOW)<=lopage));
        KASSERT((0==lopage)||(ADDR_TO_PN(USER_MEM_HIGH)>=(lopage+npages)));
        KASSERT(PAGE_ALIGNED(off));
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        
        vmarea_t *vma;
        if(lopage == 0) {
                int start=vmmap_find_range(map,npages,dir);
                if(start==-1) return -1;
                else lopage=start;
        } 
        else {
                if(!vmmap_is_range_empty(map,lopage,npages)) vmmap_remove(map,lopage,npages);
        }
        vma=vmarea_alloc();
        vma->vma_start=lopage;
        vma->vma_end=lopage+npages;
        vma->vma_prot=prot;
        vma->vma_flags=flags;
        vma->vma_off=ADDR_TO_PN(off);
        list_link_init(&(vma->vma_plink));
        list_link_init(&(vma->vma_olink));

        mmobj_t *mmobj=NULL;
        if(file==NULL) mmobj=anon_create();
        else {
                int retval=file->vn_ops->mmap(file,vma,&mmobj);
                if(retval>=0) vma->vma_obj=mmobj;
        }

        if(flags & MAP_PRIVATE) {
                mmobj_t *ShadowObj=shadow_create();
                ShadowObj->mmo_shadowed=mmobj;
                ShadowObj->mmo_un.mmo_bottom_obj=mmobj;
                vma->vma_obj=ShadowObj;
        } 
        if(new) *new=vma;
        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        /* NOT_YET_IMPLEMENTED("VM: vmmap_remove");
        return -1; */
        vmarea_t *vma;
        list_iterate_begin(&(map->vmm_list),vma,vmarea_t,vma_plink) {
                if((vma->vma_start<lopage) && (vma->vma_end)>lopage+npages) {
                        vmarea_t *newvma1=vmarea_alloc();
                        vmarea_t *newvma2=vmarea_alloc();
                        newvma1->vma_start=vma->vma_start;
                        newvma1->vma_end=lopage;
                        newvma1->vma_off=vma->vma_off;
                        newvma1->vma_prot=vma->vma_prot;
                        newvma1->vma_flags=vma->vma_flags;
                        newvma1->vma_obj=vma->vma_obj;
                        list_init(&newvma1->vma_plink);
                        list_init(&newvma1->vma_olink);
                        vmmap_insert(map,newvma1);
                        newvma2->vma_start=lopage+npages;
                        newvma2->vma_end=vma->vma_end;
                        newvma2->vma_off=vma->vma_off+(lopage+npages - newvma1->vma_start);
                        newvma2->vma_prot=newvma1->vma_prot;
                        newvma2->vma_flags=newvma1->vma_flags;
                        newvma2->vma_obj=vma->vma_obj;
                        list_init(&(newvma2->vma_plink));
                        list_init(&(newvma2->vma_olink));
                        vmmap_insert(map,newvma2);
                        list_remove(&vma->vma_plink);

                } else if(vma->vma_start<lopage && vma->vma_end>lopage && vma->vma_end<=lopage+npages) {
                        vma->vma_end=lopage;
                } else if(vma->vma_start>lopage && vma->vma_start<lopage+npages && vma->vma_end>lopage+npages) {
                        vma->vma_off+=lopage+npages-vma->vma_start;
                        vma->vma_start=lopage+npages;
                } else {
                        list_remove(&(vma->vma_plink));
                        list_remove(&(vma->vma_olink));
                        vmarea_free(vma);
                }
        }list_iterate_end();
        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        /* NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");
        return 0; */
        uint32_t endvfn=startvfn+npages;
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
        dbg(DBG_PRINT, "(GRADING3A 3.e)\n");
        vmarea_t *vma;
        list_iterate_begin(&(map->vmm_list), vma, vmarea_t, vma_plink) {
                if(vma->vma_start<startvfn+npages && vma->vma_end>startvfn) return 1;
                else return 0;
        }list_iterate_end();
        return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
       /* NOT_YET_IMPLEMENTED("VM: vmmap_read");
        return 0;*/
        uint32_t start_vfn=ADDR_TO_PN(vaddr);
        uint32_t start_offset=PAGE_OFFSET(vaddr);
        uint32_t vfn=start_vfn;
        uint32_t PN=0;
        pframe_t *frame=NULL;
        uint32_t ReadNum=0;
        uint32_t offset=start_offset;
        while(count>0) {
                vmarea_t *vma=vmmap_lookup(map,vfn);
                if(vma==NULL) {
                        curthr->kt_errno=1;
                        return -1;
                }
                PN=vfn-vma->vma_start+vma->vma_off;
                int lookup=pframe_lookup(vma->vma_obj,PN,0,&frame);
                uint32_t available_space=PAGE_SIZE-offset;
                uint32_t newBufAddr=(uint32_t)buf+ReadNum;
                uint32_t FrameAddr=(uint32_t)frame->pf_addr+offset;
                if(count>available_space) {
                        memcpy((void*)newBufAddr, (void*)FrameAddr, available_space);
                        frame+=1;
                        offset=0;
                        ReadNum+=available_space;
                        count-=available_space;
                }
                else {
                        memcpy((void*)newBufAddr, (void*)FrameAddr, count);
                        break; 
                }
        }
        return 0;

}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        /* NOT_YET_IMPLEMENTED("VM: vmmap_write");
        return 0; */
        uint32_t start_vfn=ADDR_TO_PN(vaddr);
        uint32_t start_offset=PAGE_OFFSET(vaddr);
        uint32_t vfn=start_vfn;
        uint32_t PN=0;
        pframe_t *frame=NULL;
        uint32_t WriteNum=0;
        uint32_t offset=start_offset;
        while(count>0) {
                vmarea_t *vma=vmmap_lookup(map,vfn);
                if(vma==NULL) {
                        curthr->kt_errno=1;
                        return -1;
                }
                PN=vfn-vma->vma_start+vma->vma_off;
                int lookup=pframe_lookup(vma->vma_obj,PN,0,&frame);
                uint32_t available_space=PAGE_SIZE-offset;
                uint32_t newBufAddr=(uint32_t)buf+WriteNum;
                uint32_t FrameAddr=(uint32_t)frame->pf_addr+offset;
                if(count>available_space) {
                        memcpy((void*)FrameAddr, (void*)newBufAddr, available_space);
                        frame+=1;
                        offset=0;
                        WriteNum+=available_space;
                        count-=available_space;
                }
                else {
                        memcpy((void*)FrameAddr, (void*)newBufAddr, count);
                        break; 
                }
        }
        return 0;

}
