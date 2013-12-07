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
vmmap_t *
vmmap_create(void)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_create");*/
        vmmap_t* new_vmmap = (vmmap_t *) slab_obj_alloc(vmmap_allocator);
        if (new_vmmap) {
                        list_init(&new_vmmap->vmm_list);
                        new_vmmap->vmm_proc = NULL;
        }
        return new_vmmap;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_destroy");*/
        KASSERT(NULL != map);
        dbg(DBG_PRINT, "(GRADING3A 3.a) VM map is not NULL.\n");
        vmarea_t *vmarea = NULL;
        mmobj_t* mmobj = NULL;
        list_iterate_begin(&map->vmm_list, vmarea, vmarea_t, vma_plink) {

                mmobj = vmarea->vma_obj;
                if(mmobj!=NULL)
                {
                        if (mmobj->mmo_ops->ref != NULL)
                        {
                                mmobj->mmo_ops->put(mmobj);

                        }
                        else if (mmobj->mmo_shadowed != NULL)
                        {
                                mmobj->mmo_shadowed->mmo_ops->put(mmobj->mmo_shadowed);
                        }
                        list_remove(&vmarea->vma_olink);
                }

                list_remove(&vmarea->vma_plink);
                vmarea_free(vmarea);

        } list_iterate_end();

        slab_obj_free(vmmap_allocator, map);
}


/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
        KASSERT(NULL != map && NULL != newvma);
        dbg(DBG_PRINT, "(GRADING3A 3.b) VM area and VM map are not NULL.\n");

        KASSERT(NULL == newvma->vma_vmmap);
        dbg(DBG_PRINT, "(GRADING3A 3.b) VM area does have a VM map .\n");

        KASSERT(newvma->vma_start < newvma->vma_end);
        dbg(DBG_PRINT, "(GRADING3A 3.b) VM area has a start VFN less than its end VFN.\n");

        KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
        dbg(DBG_PRINT, "(GRADING3A 3.b) VM area within memory boundaries.\n");


        vmarea_t *iterate;
        if(list_empty(&map->vmm_list))
        {
                list_insert_head(&(map->vmm_list),&(newvma->vma_plink));

        }
        else{

                list_iterate_begin(&map->vmm_list,iterate,vmarea_t,vma_plink)
                {
                        if(newvma->vma_start >iterate->vma_start){
                                list_insert_tail(&map->vmm_list,&newvma->vma_plink );
                                newvma->vma_vmmap=map;
                                return;
                        }
                        else
                        {
                                list_insert_before(&iterate->vma_plink,&newvma->vma_plink);
                                newvma->vma_vmmap=map;
                                return;
                        }
                }list_iterate_end();
        }

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
        KASSERT(NULL != map);
        dbg(DBG_PRINT, "(GRADING3A 3.c) vm map is not NULL.\n");

        KASSERT(0 < npages);
        dbg(DBG_PRINT, "(GRADING3A 3.c) npages which has to be looked us is greater than zero.\n");

        int startvfn = -1, i=-1;
        vmarea_t *area;

        if(list_empty(&(map->vmm_list)))
        {
                        return -1;
        }

        if(dir==VMMAP_DIR_HILO){
                                        list_iterate_reverse(&(map->vmm_list), area, vmarea_t,vma_plink){
                                                                        i = vmmap_is_range_empty(map,area->vma_start-(npages),area->vma_start-1);
                                                                        if(i==1){
                                                                                                        startvfn = area->vma_start-(npages);

                                                                                                        break;
                                                                        }
                                        }list_iterate_end();
                                        return startvfn;

        }
        if(dir==VMMAP_DIR_LOHI)
        {
                                        list_iterate_begin(&(map->vmm_list), area, vmarea_t,vma_plink){
                                                                        i = vmmap_is_range_empty(map,area->vma_end+1,area->vma_end+npages);
                                                                        if(i==1){
                                                                                                        startvfn = area->vma_end+1;
                                                                                                        break;
                                                                        }
                                        }list_iterate_end();
                                        return startvfn;
        }
        return -1;
}
/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_lookup");*/
        KASSERT(NULL != map);
        dbg(DBG_PRINT, "(GRADING3A 3.d) Map is not NULL\n ");
        vmarea_t *vmarea;
        if(!list_empty(&map->vmm_list))
        {
                list_iterate_begin(&map->vmm_list,vmarea,vmarea_t,vma_plink){
                        if((vfn >= vmarea->vma_start) && (vfn < vmarea->vma_end))
                                                        return vmarea;
                }list_iterate_end();
        }

		return NULL;
        /*return NULL;*/
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        vmarea_t *vmarea=NULL;
                vmarea_t *vmclone=NULL;
                vmmap_t * clonemap=vmmap_create();
                if(!clonemap)
                {
                                return NULL;
                }
                list_iterate_begin(&map->vmm_list,vmarea,vmarea_t,vma_plink){
                                vmclone=vmarea_alloc();
                                if(vmclone){
                                                vmclone->vma_prot=vmarea->vma_prot;
                                                vmclone->vma_flags=vmarea->vma_flags;
                                                vmclone->vma_start= vmarea->vma_start;
                                                vmclone->vma_end=vmarea->vma_end;
                                                vmclone->vma_off=vmarea->vma_off;
                                                vmmap_insert(clonemap,vmclone);
                                }
                                else{
                                        vmmap_destroy(vmclone);
                                        return NULL;
                                }
                }list_iterate_end();

                return clonemap;
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
        KASSERT(NULL != map);
        dbg(DBG_PRINT, "(GRADING3A 3.f)Map is not NULL\n ");
        KASSERT(0 < npages);
        dbg(DBG_PRINT, "(GRADING3A 3.f) npages > 0 \n ");
        KASSERT(!(~(PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC) & prot));
        dbg(DBG_PRINT, "(GRADING3A 3.f) !(~(PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC) & prot)\n");
        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags));
        dbg(DBG_PRINT, "(GRADING3A 3.f) flags is either set to MAP_SHARED or MAP_PRIVATE\n");
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage));
        dbg(DBG_PRINT, "(GRADING3A 3.f)(0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage)\n ");
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
        dbg(DBG_PRINT, "(GRADING3A 3.f)(0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages))\n ");
        KASSERT(PAGE_ALIGNED(off));
        dbg(DBG_PRINT, "(GRADING3A 3.f)PAGE_ALIGNED(off)\n ");
        int gap,j;
        vmarea_t *new_vma;
        mmobj_t *mobj;

        if(lopage==0){
                        int x=vmmap_find_range(map,npages,dir);
						if(!x)
								return NULL;
						lopage=x;

                }
		else{
				if(!vmmap_is_range_empty(map,lopage,npages)){
						if(vmmap_remove(map,lopage,npages)!=0){
								return -1;
						}
				}


         }
        new_vma=vmarea_alloc();
        new_vma->vma_flags=flags;
        new_vma->vma_end=lopage+npages;
        new_vma->vma_off=off;
        new_vma->vma_prot=prot;
        new_vma->vma_start=lopage;
        vmmap_insert(map, new_vma);
        if(file)
        {
                KASSERT(file->vn_ops!=NULL&&file->vn_ops->mmap!=NULL);
                j=(file->vn_ops->mmap)(file,new_vma,&mobj);
                if(j<0)
                return j;
                else
                {
                        mmobj_t *shadow;
                        new_vma->vma_obj = mobj;
                        mobj->mmo_ops->ref(mobj);
                        /*if((shadow = shadow_create()) !=NULL)
                        {
                                shadow->mmo_shadowed = mobj;
                                (shadow)->mmo_un.mmo_bottom_obj = mobj;
                                mobj->mmo_ops->ref(mobj);
                                new_vma->vma_obj = shadow;

                        }*/

                }
        }
        else
        {
                if(flags!=MAP_PRIVATE)
                         mobj=anon_create();
                else
                        mobj=shadow_create();

                 if(mobj==NULL)
                         return -1;
                 new_vma->vma_obj = mobj;
                 mobj->mmo_ops->ref(mobj);
        }
        if (new!=NULL)
                new=&new_vma;

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
	if(list_empty(&(map->vmm_list))){
	                return -1;
	}
	  uint32_t start,end;
	  start = lopage;
	  end = start + npages ;
	vmarea_t *vma,*vmnew;
	list_iterate_begin(&(map->vmm_list), vma, vmarea_t, vma_plink)
	{
		if( start > vma->vma_start && end<vma->vma_end ) /* case 1 */
		{
            vmnew=vmarea_alloc();
			vmnew->vma_start=lopage+npages;
			vmnew->vma_end=vma->vma_end;
			vmnew->vma_obj=vma->vma_obj;
			vmnew->vma_prot=vma->vma_prot;
			vmnew->vma_flags=vma->vma_flags;

			vma->vma_off += npages;
			vma->vma_end=lopage;
			if(vmnew->vma_obj)
					vmnew->vma_obj->mmo_refcount++;
                    vmmap_insert(map, vmnew);
		}
		else if( start>vma->vma_start && start<=vma->vma_end && end>=vma->vma_end ) /* case 2 */
		{

		   vma->vma_end = lopage;

		}
		else if( start<=vma->vma_start && end<vma->vma_end && end>=vma->vma_start ) /* case 3 */
		{

		   vma->vma_off = vma->vma_off +lopage+npages-vma->vma_start;
		   vma->vma_start = lopage+npages;

		}
		else if(start<=vma->vma_start && end>=vma->vma_end) /* case 4 */
		{

		   list_remove(&vma->vma_plink);
		   list_remove(&vma->vma_olink);
		   if ( vma->vma_obj != NULL )
						   vma->vma_obj->mmo_ops->put(vma->vma_obj);
		   vmarea_free(vma);
		}
		else if( start<vma->vma_start && end<vma->vma_start ) /* case 5 */
		{

				   return 0;
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
        /*NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");
        return 0;*/
        uint32_t endvfn = startvfn+npages;
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
        dbg(DBG_PRINT, "(GRADING3A 3.e)(startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn)\n ");
        vmarea_t *vmarea;
        if(!list_empty(&map->vmm_list)){


			list_iterate_begin(&map->vmm_list,vmarea,vmarea_t,vma_plink)
			{
					if(vmarea->vma_start>=startvfn && vmarea->vma_end < endvfn )
							return 0;

			}list_iterate_end();
        }
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
    uint32_t vfn = ADDR_TO_PN(vaddr);
    int size=0;
    if(count/PAGE_SIZE==0){
    	size=1;
    }
    else {
    	if(count%PAGE_SIZE==0)
    		size=count/PAGE_SIZE;
    	else
    		size=count/PAGE_SIZE+1;

    }

    /*First iterate through the list to get correct vmareas*/
    if(list_empty(&(map->vmm_list)))
    {
        return -EFAULT;

    }

	vmarea_t *area;
	pframe_t *frame;
	while(size>0)
	{
			area = vmmap_lookup(map,vfn);
			if(!area)
				return -EFAULT;
			if(area!=NULL)
			{

				pframe_get(area->vma_obj,vfn-area->vma_start+area->vma_off,&frame);
				if(frame)
				{
						if(count<=PAGE_SIZE-PAGE_OFFSET((uintptr_t)vaddr))
								memcpy(buf,frame->pf_addr+PAGE_OFFSET((uintptr_t)vaddr),count);
						else
						{
								memcpy(buf,frame->pf_addr,PAGE_SIZE-PAGE_OFFSET((uintptr_t)vaddr));
								count-=PAGE_SIZE-PAGE_OFFSET((uintptr_t)vaddr);
						}
				}

						size--;

						vfn=vfn+1;
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
        uint32_t vfn = ADDR_TO_PN(vaddr);
        int size=0;
        if(count/PAGE_SIZE==0){
        		size=1;
        }
        else {
        	if(count%PAGE_SIZE==0)
        		size=count/PAGE_SIZE;
        	else
        		size=count/PAGE_SIZE+1;

        }

        /*First iterate through the list to get correct vmareas*/
        if(list_empty(&(map->vmm_list)))
        {
            return -EFAULT;

        }

		vmarea_t *area;
		pframe_t *frame;
		while(size>0)
		{
				area = vmmap_lookup(map,vfn);
				if(!area)
					return -EFAULT;
				if(area!=NULL)
				{

					pframe_get(area->vma_obj,vfn-area->vma_start+area->vma_off,&frame);
					if(frame)
					{
							if(count<=PAGE_SIZE-PAGE_OFFSET((uintptr_t)vaddr))
									memcpy(frame->pf_addr+PAGE_OFFSET((uintptr_t)vaddr),buf,count);
							else
							{
									memcpy(frame->pf_addr,buf,PAGE_SIZE-PAGE_OFFSET((uintptr_t)vaddr));
									count=count+PAGE_SIZE-PAGE_OFFSET((uintptr_t)vaddr);
							}
					}
							size--;

							vfn=vfn+1;
				}
		}

        return 0;
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
