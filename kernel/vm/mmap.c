#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,int fd, off_t off, void **ret)
{
	/*NOT_YET_IMPLEMENTED("VM: do_mmap");*/

	int address = (uintptr_t) addr;

	tlb_flush(address);

	KASSERT(NULL != curproc->p_pagedir);
	dbg(DBG_PRINT, "(GRADING3A 2.a)curproc->p_pagedir; first ten bits of the virtual address is NOT NULL\n ");

	vmmap_map(curproc->p_vmmap, curproc->p_files[fd]->f_vnode, 0, 0, prot, flags, off, NULL, (vmarea_t**) ret);
	return 0;


}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{

	/*NOT_YET_IMPLEMENTED("VM: do_munmap");*/

	int temp_addr = (uintptr_t) addr;

	tlb_flush(temp_addr);

	KASSERT(NULL != curproc->p_pagedir);
	dbg(DBG_PRINT, "(GRADING3A 2.b)curproc->p_pagedir; first ten bits of the virtual address is NOT NULL\n ");

	vmmap_remove(curproc->p_vmmap, 0, 0);
	return 0;

}
