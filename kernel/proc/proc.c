#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
        list_init(&_proc_list);
        proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
        KASSERT(proc_allocator != NULL);
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
        proc_t *p;
        pid_t pid = next_pid;
        while (1) {
failed:
                list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                        if (p->p_pid == pid) {
                                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                                        return -1;
                                } else {
                                        goto failed;
                                }
                        }
                } list_iterate_end();
                next_pid = (pid + 1) % PROC_MAX_COUNT;
                return pid;
        }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
proc_t *
proc_create(char *name)
{
	/*NOT_YET_IMPLEMENTED("PROCS: proc_create");*/
	pid_t pid = _proc_getid();

	proc_t* new_proc =(proc_t *) slab_obj_alloc(proc_allocator);
	memset(new_proc, 0, sizeof(proc_t));
	new_proc->p_pid = pid;
	new_proc->p_state = PROC_RUNNING;
        new_proc->p_status = 0;
	new_proc->p_pproc = curproc;
	new_proc->p_pagedir = pt_create_pagedir();
	strcpy(new_proc->p_comm, name);
	list_init(&new_proc->p_children);
	list_init(&new_proc->p_threads);
	list_link_init(&new_proc->p_child_link);
	list_link_init(&new_proc->p_list_link);

	KASSERT(PID_IDLE != pid || list_empty(&_proc_list)); /* pid can only be PID_IDLE if this is the first process */
	dbg(DBG_PRINT,"(GRADING1A 2.a): proc.c: proc_create: For the first process(idle process) the pid is PID_IDLE\n");
	KASSERT(PID_INIT != pid || PID_IDLE == curproc->p_pid); /* pid can only be PID_INIT when creating from idle process */
	dbg(DBG_PRINT,"(GRADING1A 2.a): proc.c: proc_create: pid is PID_INT when init process (child of idle process) is being created\n");
	list_insert_tail(&_proc_list, &new_proc->p_list_link);

	if(curproc!=NULL)
		list_insert_tail(&curproc->p_children, &new_proc->p_child_link);
	if (pid == PID_INIT) {
		proc_initproc = new_proc;
	}
	vmmap_t *map=vmmap_create();
	new_proc->p_vmmap=map;
	sched_queue_init(&new_proc->p_wait);
	return new_proc;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{

	/*NOT_YET_IMPLEMENTED("PROCS: proc_cleanup");*/
	KASSERT(NULL != proc_initproc); /* should have an "init" process */
	dbg(DBG_PRINT,"(GRADING1A 2.b): proc.c: proc_cleanup: (pre-condition) init process is not NULL\n");
	KASSERT(1 <= curproc->p_pid); /* this process should not be idle process */
	dbg(DBG_PRINT,"(GRADING1A 2.b): proc.c: proc_cleanup: (pre-condition) current process pid is not idle process\n");
	KASSERT(NULL != curproc->p_pproc); /* this process should have parent process */
	dbg(DBG_PRINT,"(GRADING1A 2.b): proc.c: proc_cleanup: (pre-condition) the current process has parent process\n");
	proc_t *p = curproc;
	proc_t *child = NULL;
	list_t parent_waitlist;


		if (!list_empty(&p->p_children)) {
			if(p != proc_initproc) {
				list_iterate_begin(&p->p_children, child, proc_t, p_child_link)
					{
						list_remove_head(&p->p_children);
						list_insert_tail(&proc_initproc->p_children,
								&child->p_child_link);
						child->p_pproc = proc_initproc;

					}list_iterate_end();
			}
		}
	curproc->p_state = PROC_DEAD;
	curproc->p_status = 0;
	int i = 0;
        
	for(i=0;i<NFILES;i++)
	{
		file_t *f = curproc->p_files[i];
		if(f)
		{
		   fref(f);
		}
	}
	struct vnode *vnode = NULL;
	if(p->p_pid==PID_INIT || p->p_pid == PID_IDLE){
	list_iterate_begin(&p->p_cwd->vn_link, vnode, struct vnode, vn_link)
                                        {
											while(vnode->vn_refcount > 0) {
                                                vput(vnode);
											}
                                        }list_iterate_end();
	}
	/*kthread_destroy(curthr);*/
	sched_wakeup_on(&curproc->p_pproc->p_wait);
	sched_switch();
	KASSERT(NULL != curproc->p_pproc); /* this process should have parent process */
	dbg(DBG_PRINT,"(GRADING1A) 2.b: proc.c: proc_cleanup: (post-condition) The current process has parent process\n");
}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
void
proc_kill(proc_t *p, int status)
{
	/*NOT_YET_IMPLEMENTED("PROCS: proc_kill");*/
	/*p->p_status = status;			(not sure) */
	if (p == curproc) {
		do_exit(status);
	} else {
		kthread_t *kthr;
		list_iterate_begin(&curproc->p_threads, kthr, kthread_t, kt_plink) {
			/*cancel each thread*/
			kthread_cancel(kthr, kthr->kt_retval);
			sched_make_runnable(kthr);
		}list_iterate_end();
	}
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */
void
proc_kill_all()
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_kill_all");*/
	proc_t *proc;
	list_iterate_begin(&_proc_list, proc, proc_t, p_list_link) {
		if(proc != proc_initproc && proc->p_pid != PID_IDLE) {
			proc_kill(proc, proc->p_status);
		}
	}list_iterate_end();
}

proc_t *
proc_lookup(int pid)
{
        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid == pid) {
                        return p;
                }
        } list_iterate_end();
        return NULL;
}

list_t *
proc_list()
{
        return &_proc_list;
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
void
proc_thread_exited(void *retval)
{
    proc_cleanup((int)retval);
	/*NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");*/
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
	/*NOT_YET_IMPLEMENTED("PROCS: do_waitpid");*/
	proc_t * cur_child;
	kthread_t *cur_thread;

	/*log error: not supported*/
	if (list_empty(&curproc->p_children)) {
		return -ECHILD;
	}

	if (pid == -1) {
		/*log error: not supported*/
		while (1) {
			list_iterate_begin(&curproc->p_children,cur_child,proc_t,p_child_link) {
				KASSERT(NULL != cur_child);
				dbg(DBG_PRINT,"(GRADING1A 2.c): proc.c: do_waitpid: the process is not NULL\n");
	   			KASSERT(-1 == pid || cur_child->p_pid == pid);
	   			dbg(DBG_PRINT,"(GRADING1A 2.c): proc.c: do_waitpid: the pid is either -1 or pid of child process\n");
	   			if(cur_child->p_state==PROC_DEAD) {
	   				list_iterate_begin(&cur_child->p_threads,cur_thread,kthread_t,kt_plink) {
	   					KASSERT(KT_EXITED == cur_thread->kt_state);
	   					dbg(DBG_PRINT,"(GRADING1A 2.c): proc.c: do_waitpid: the child process's state is KT_EXITED\n");
	   					kthread_destroy(cur_thread);
	   				}list_iterate_end();

	   				*status = cur_child->p_status;
	   				list_remove(&cur_child->p_child_link);
	   				list_remove(&cur_child->p_list_link);
	   				KASSERT(NULL != cur_child->p_pagedir);
	   				dbg(DBG_PRINT,"(GRADING1A 2.c): proc.c: do_waitpid: current process's pagedir is not NULL\n");
	   				pt_destroy_pagedir(cur_child->p_pagedir);
	   				slab_obj_free(proc_allocator,cur_child);
	   				return(cur_child->p_pid);
	   			}
			}list_iterate_end();
	   		sched_sleep_on(&curproc->p_wait);
		}
	}
	int flag =0;
	list_iterate_begin(&curproc->p_children, cur_child, proc_t, p_child_link) {
	   if(cur_child->p_pid == pid) {
	   	 flag = 1;
	   }
	}list_iterate_end();
	if(flag != 1) {
		return -ECHILD;
	}
	if (pid > 0) {
		list_iterate_begin(&curproc->p_children,cur_child,proc_t,p_child_link) {
			KASSERT(NULL != cur_child);
			dbg_print("proc.c: do_waitpid: the process is not NULL\n");
	   		if(cur_child->p_pid== pid) {
	   			sched_sleep_on(&curproc->p_wait);
	   			list_iterate_begin(&cur_child->p_threads,cur_thread,kthread_t,kt_plink){
	   				KASSERT(KT_EXITED == cur_thread->kt_state);
	   				dbg_print("proc.c: do_waitpid: the child process's state is KT_EXITED\n");
	   				kthread_destroy(cur_thread);
	   			}list_iterate_end();

	   			*status = cur_child->p_status;
	   			list_remove(&cur_child->p_child_link);
	   			list_remove(&cur_child->p_list_link);
	   			KASSERT(NULL != cur_child->p_pagedir);
	   			dbg_print("proc.c: do_waitpid: current process's pagedir is not NULL\n");
	   			pt_destroy_pagedir(cur_child->p_pagedir);
	   			slab_obj_free(proc_allocator,cur_child);
	   			return(cur_child->p_pid);
	   		}
		}list_iterate_end();
	}
	return -ECHILD;
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: do_exit");*/
	kthread_t *kthr;
	/*list_iterate_begin(&curproc->p_threads, kthr, kthread_t, kt_plink){

		kthread_cancel(kthr,kthr->kt_retval);
	}list_iterate_end();*/
	kthread_cancel(curthr,0);

	/*proc_cleanup(curproc->p_status);*/
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
        const proc_t *p = (proc_t *) arg;
        size_t size = osize;
        proc_t *child;

        KASSERT(NULL != p);
        KASSERT(NULL != buf);

        iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
        iprintf(&buf, &size, "name:         %s\n", p->p_comm);
        if (NULL != p->p_pproc) {
                iprintf(&buf, &size, "parent:       %i (%s)\n",
                        p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
                iprintf(&buf, &size, "parent:       -\n");
        }

#ifdef __MTP__
        int count = 0;
        kthread_t *kthr;
        list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
                ++count;
        } list_iterate_end();
        iprintf(&buf, &size, "thread count: %i\n", count);
#endif

        if (list_empty(&p->p_children)) {
                iprintf(&buf, &size, "children:     -\n");
        } else {
                iprintf(&buf, &size, "children:\n");
        }
        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
                iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
        } list_iterate_end();

        iprintf(&buf, &size, "status:       %i\n", p->p_status);
        iprintf(&buf, &size, "state:        %i\n", p->p_state);

#ifdef __VFS__
#ifdef __GETCWD__
        if (NULL != p->p_cwd) {
                char cwd[256];
                lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                iprintf(&buf, &size, "cwd:          %-s\n", cwd);
        } else {
                iprintf(&buf, &size, "cwd:          -\n");
        }
#endif /* __GETCWD__ */
#endif

#ifdef __VM__
        iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
        iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif

        return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
        size_t size = osize;
        proc_t *p;

        KASSERT(NULL == arg);
        KASSERT(NULL != buf);

#if defined(__VFS__) && defined(__GETCWD__)
        iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
        iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif

        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                char parent[64];
                if (NULL != p->p_pproc) {
                        snprintf(parent, sizeof(parent),
                                 "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
                } else {
                        snprintf(parent, sizeof(parent), "  -");
                }

#if defined(__VFS__) && defined(__GETCWD__)
                if (NULL != p->p_cwd) {
                        char cwd[256];
                        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                        iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                                p->p_pid, p->p_comm, parent, cwd);
                } else {
                        iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                                p->p_pid, p->p_comm, parent);
                }
#else
                iprintf(&buf, &size, " %3i  %-13s %-s\n",
                        p->p_pid, p->p_comm, parent);
#endif
        } list_iterate_end();
        return size;
}
