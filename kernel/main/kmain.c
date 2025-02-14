#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;

static int gdb_wait = GDBWAIT;
/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
	/* This little loop gives gdb a place to synch up with weenix.  In the
	 * past the weenix command started qemu was started with -S which
	 * allowed gdb to connect and start before the boot loader ran, but
	 * since then a bug has appeared where breakpoints fail if gdb connects
	 * before the boot loader runs.  See
	 *
	 * https://bugs.launchpad.net/qemu/+bug/526653
	 *
	 * This loop (along with an additional command in init.gdb setting
	 * gdb_wait to 0) sticks weenix at a known place so gdb can join a
	 * running weenix, set gdb_wait to zero  and catch the breakpoint in
	 * bootstrap below.  See Config.mk for how to set GDBWAIT correctly.
	 *
	 * DANGER: if GDBWAIT != 0, and gdb isn't run, this loop will never
	 * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
	 * you expect.
	 */
      	while (gdb_wait) ;
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
        /* necessary to finalize page table information */
        pt_template_init();

        /*NOT_YET_IMPLEMENTED("PROCS: bootstrap");*/
	curproc = proc_create("IDLE");
	KASSERT(NULL != curproc); /* make sure that the "idle" process has been created successfully */
	dbg(DBG_PRINT,"(GRADING1A 1.a): kmain.c: bootstrap: The idle process has been created successfully\n");
	KASSERT(PID_IDLE == curproc->p_pid); /* make sure that what has been created is the "idle" process */
	dbg(DBG_PRINT,"(GRADING1A 1.a): kmain.c: bootstrap: The pid of the current process is PID_IDLE\n");
	curthr = kthread_create(curproc, idleproc_run, 0, NULL);
	KASSERT(NULL != curthr); /* make sure that the thread for the "idle" process has been created successfully */
	dbg(DBG_PRINT,"(GRADING1A 1.a): kmain.c: bootstrap: The thread for the IDLE process has been created successfully\n");
	context_make_active(&curthr->kt_ctx);

        panic("weenix returned to bootstrap()!!! BAD!!!\n");
        return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
        int status;
        pid_t child;

        /* create init proc */
        kthread_t *initthr = initproc_create();

        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
	/* Once you have VFS remember to set the current working directory
	 * of the idle and init processes */

	proc_t *init_proc = proc_lookup(PID_INIT);
	proc_t *idle_proc = proc_lookup(PID_IDLE);
	init_proc->p_cwd = vfs_root_vn; 
        curproc->p_cwd = vfs_root_vn;
	vref(vfs_root_vn);
	/*idle_proc->p_cwd = vfs_root_vn;
	vget(idle_proc->p_cwd->vn_fs, vfs_root_vn);*/

	/* Here you need to make the null, zero, and tty devices using mknod */
	/* You can't do this until you have VFS, check the include/drivers/dev.h
	 * file for macros with the device ID's you will need to pass to mknod */
	/*NOT_YET_IMPLEMENTED("VFS: idleproc_run");*/
	/*int mkdir_dev = 0;
	mkdir_dev = ;*/
	if( do_mkdir("/dev") != 0) {
		panic("MKDIR failed\n");
	} else if(do_mknod("/dev/null", S_IFCHR, MKDEVID(1, 0)) != 0) {
		panic("MKNOD failed for /dev/null\n");
	} else if(do_mknod("/dev/zero", S_IFCHR, MKDEVID(1, 1)) != 0) {
		panic("MKNOD failed for /dev/zero\n");
	} else if(do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2, 0)) != 0) {
		panic("MKNOD failed for /dev/tty0\n");
	} else if(do_mknod("/dev/tty1", S_IFCHR, MKDEVID(2, 1)) != 0) {
		panic("MKNOD failed for /dev/tty1\n");
	} else if(do_mknod("/dev/tty2", S_IFCHR, MKDEVID(2, 3)) != 0) {
		panic("MKNOD failed for /dev/tty1\n");
	}
#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
        /*NOT_YET_IMPLEMENTED("PROCS: initproc_create");*/
	proc_t *init_proc = proc_create("INIT");
	KASSERT(NULL != init_proc);
	dbg(DBG_PRINT,"(GRADING1A 1.b): kmain.c:initproc_create: init_proc has been created successfully\n ");
	KASSERT(PID_INIT == init_proc->p_pid);
	dbg(DBG_PRINT,"(GRADING1A 1.b): kmain.c:initproc_create: init_proc PID is equal to PID_INIT\n ");
	kthread_t *init_thread = kthread_create(init_proc,initproc_run,0,NULL);
	KASSERT(init_thread != NULL);
	dbg(DBG_PRINT,"(GRADING1A 1.b): kmain.c:initproc_create: init_thread has been created successfully and is valid\n ");
	/*sched_make_runnable(init_thread);*/
	return init_thread;
}

int faber_test(kshell_t* kshell, int argc, char **argv) {

	testproc(0, NULL);
	return 0;
}

int pc_test(kshell_t* kshell, int argc, char **argv) {
	sunghan_test(0, NULL);
	return 0;
}

int deadlock_test(kshell_t* kshell, int argc, char **argv) {
	sunghan_deadlock_test(0,NULL);
	return 0;
}

int vfs_test(kshell_t* kshell, int argc, char **argv) {
	vfstest_main(argc,argv);
	return 0;
}


int helloWorldProg(kshell_t* kshell, int arg1, char **argv)
{
	kshell_destroy(kshell);
	char *argb[] = { NULL };
	char *envp[] = { NULL };
	kernel_execve("/usr/bin/hello", argb, envp);
	return 0;

}

int u_name(kshell_t *kshell, int argc, char **argv)
{
 	char *argb[] = { NULL };
 	char *envp[] = { NULL };
 	kernel_execve("/bin/uname", argb,envp);
 	return 0;
}

int arguments(kshell_t *kshell, int argc, char **argv)
{
	char *argb[] = { NULL };
	char *envp[] = { "ab cde fghi j" };
	kernel_execve("/usr/bin/args", argb,envp);
	return 0;
}

int segment_fault(kshell_t *kshell, int argc, char **argv)
{
	char *argb[] = { NULL };
	char *envp[] = { NULL };
	kernel_execve("/usr/bin/segfault", argb,envp);
	return 0;
}

int vfs578(kshell_t *kshell, int argc, char **argv)
{
	char *argb[] = { NULL };
	char *envp[] = { NULL };
	kernel_execve("/usr/bin/vfstest", argb,envp);
	return 0;
}

int init_shell(kshell_t *kshell, int argc, char **argv)
{
	char *argb[] = { NULL };
	char *envp[] = { NULL };
	kernel_execve("/sbin/init", argb,envp);
	return 0;
}

int extra_vfs_test(kshell_t* kshell, int arg1, char **argv)
{
    char *before="file1";
    char *after="file2";
    int fd = 0;
    fd = do_open(before,O_RDWR|O_CREAT);
    if(fd > 0)
    {
    int rename = do_rename(before,after);
    if(rename == 0)
    {
        dbg(DBG_PRINT,"(GRADING2C) Student rename test successful \n");
    }
    else
    { 
        dbg(DBG_PRINT,"(GRADING2C) Student rename test unsuccessful \n");
    }
    do_close(fd);
    }
    return 0;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
initproc_run(int arg1, void *arg2)
{
	/*NOT_YET_IMPLEMENTED("PROCS: initproc_run");*/

#ifdef __DRIVERS__

	kshell_add_command("pct",pc_test, "Producer Consumer test");
	kshell_add_command("deadlock", deadlock_test, "Deadlock test");
	kshell_add_command("testproc", faber_test, "Faber test");
#ifdef __VFS__

	kshell_add_command("renametest", extra_vfs_test, "student rename test(vfs)");
#endif
#ifdef __VM__
	kshell_add_command("hello", helloWorldProg, "Run helloworld program");
	kshell_add_command("args", arguments, "Executes arguments test");
	kshell_add_command("vfstest", vfs578, "Tests 578 tests in vftest");
	kshell_add_command("user_shell", init_shell, "get the user space shell");
	kshell_add_command("uname", u_name, "Executes uname test");
	kshell_add_command("segfault", segment_fault, "Test segment fault");
#endif
	kshell_t *kshell = kshell_create(0);
	if (NULL == kshell) panic("init: Couldn't create kernel shell\n");
	while (kshell_execute_next(kshell));
	kshell_destroy(kshell);

#endif  /* __DRIVERS__ */
	
	return NULL;
}



/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}
