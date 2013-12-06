#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on. Make sure to check
 * the permissions on the area to see if the process has
 * permission to do [cause]. If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page (don't forget                
 * about shadow objects, especially copy-on-write magic!). Make
 * sure that if the user writes to the page it will be handled  NOTE : don't know what does this mean
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */

void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        /* This function is called from mm/pagetable.c
There are 5 types of faults:-
1. FAULT_USER-Fault from user space
2. FAULT_PRESENT-Fault present or not
 3. FAULT_RESERVED-Fault if access is PROT_NONE
4. FAULT_WRITE-Fault if access is PROT_WRITE
5. FAULT_EXEC-Fault if access is PROT_EXEC
NOTE: FAULT_READ cannnot take place in OS
*/

    pagedir_t *pagetemp;
     uint32_t vfn=ADDR_TO_PN(vaddr);
    vmarea_t *vma;
    vma=vmmap_lookup(curproc->p_vmmap, vfn);
    dbg_print("== anon object = 0x%p ,area = 0x%p, pagenum = %d vaddr= %d\n",vma->vma_obj,vma,PAGE_OFFSET(vfn),vaddr);

    if (vma==NULL)
    {
        dbg_print("VMA null\n");
        proc_kill(curproc, EFAULT);
        return;
    }
    if ((cause & FAULT_PRESENT) && !(vma->vma_prot & PROT_READ)) {
         proc_kill(curproc, EFAULT);
        return;
    }
    if ((cause & FAULT_WRITE) && !(vma->vma_prot & PROT_WRITE)) {
        proc_kill(curproc, EFAULT);
        return;
    }
     if ((cause & FAULT_EXEC) && !(vma->vma_prot & PROT_EXEC)) {
        proc_kill(curproc, EFAULT);
        return;
    }
    if ((cause & FAULT_RESERVED) && (vma->vma_prot & PROT_NONE)) {
         proc_kill(curproc, EFAULT);
        return;
    }

    pframe_t *pget;
    int a=pframe_get(vma->vma_obj,vfn-vma->vma_start+vma->vma_off,&pget);
    if(a<0)
        return;
     uintptr_t pageaddr=pt_virt_to_phys((uint32_t)pget->pf_addr);
    pt_map(curproc->p_pagedir,(uintptr_t)PAGE_ALIGN_DOWN(vaddr),pageaddr,PD_PRESENT|PD_WRITE|PD_USER, PT_PRESENT|PT_WRITE|PT_USER);

}
