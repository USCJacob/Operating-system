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
#include "kernel.h"
#include "errno.h"

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
#include "vm/shadowd.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/disk/ata.h"
#include "drivers/tty/virtterm.h"
#include "drivers/pci.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"
#include "test/s5fs_test.h"


GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;
extern int gdb_wait;

extern void     *sunghan_test(int arg1,void *arg2);
extern void     *sunghan_deadlock_test(int arg1,void *arg2);
extern void     *faber_thread_test(int arg1,void *arg2);

// file system tests
extern void *vfstest_main(int, void*);
extern int faber_fs_thread_test(kshell_t *ksh, int argc, char **argv);
extern int faber_directory_test(kshell_t *ksh, int argc, char **argv);


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
        pci_init();
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
         * DANGER: if GDBWAIT != 0, and gdb is not running, this loop will never
         * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
         * you expect.
         */
        while (gdb_wait) ;
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
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
        /* If the next line is removed/altered in your submission, 20 points will be deducted. */
        dbgq(DBG_TEST, "SIGNATURE: 53616c7465645f5f72729a827b95d944a91697c971e6fba192d69daf78f6b138adfb7b98082290b6322ddca1db9fd765\n");
        /* necessary to finalize page table information */
        pt_template_init();
        
        
        /* NOT_YET_IMPLEMENTED("PROCS: bootstrap"); */
        /* create the idle process */
        proc_t    *idle_proc   = proc_create("idle");                     // create idle process pcb
        curproc = idle_proc;
        
        /* create thread with thread context (default stack size) */
        kthread_t *idle_thread = kthread_create(idle_proc,                // idle process pcb
                                                idleproc_run,             // idle proc main code body
                                                0,                        // arg1 unused
                                                NULL                      // arg2 unused
                                                );
        
        /* update current process and current thread as this will be the thread running */
        curthr = idle_thread;
        
        /* for (GRADING1A 1.a) */
        KASSERT(NULL != curproc);                       /* curproc was uninitialized before, it is initialized here to point to the "idle" process */   
        KASSERT(PID_IDLE == curproc->p_pid);            /* make sure the process ID of the created "idle" process is PID_IDLE */
        KASSERT(NULL != curthr);                        /* curthr was uninitialized before, it is initialized here to point to the thread of the "idle" process */
        
        /* switch context to current thread */
        context_make_active(&(curthr->kt_ctx));

        /* the context should be switched to idle_proc already. The bootstrap shouldn't return here */
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
        // NOT_YET_IMPLEMENTED("VFS: idleproc_run");
        curproc->p_cwd = vfs_root_vn;
        vref(vfs_root_vn);
        
        initthr->kt_proc->p_cwd = vfs_root_vn;
        vref(vfs_root_vn);
        
        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
        // NOT_YET_IMPLEMENTED("VFS: idleproc_run");
        do_mkdir("/dev");
        do_mknod("/dev/null", S_IFCHR, MEM_NULL_DEVID);
        do_mknod("/dev/zero", S_IFCHR, MEM_ZERO_DEVID);
        do_mknod("/dev/tty0", S_IFCHR, (MKDEVID(2, 0)));
        
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


#ifdef __SHADOWD__
        /* wait for shadowd to shutdown */
        shadowd_shutdown();
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
        dbg(DBG_PRINT, "idleproc_run\n");
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
    // NOT_YET_IMPLEMENTED("PROCS: initproc_create");
    /* create init process */
    proc_t *init_proc = proc_create("init");
    
    /* create init thread */
    kthread_t *init_thread = kthread_create(init_proc,                // idle process pcb
                                            initproc_run,             // idle proc main code body
                                            0,                        // arg1 unused
                                            NULL                      // arg2 unused
                                            );
    
    /* GRADING1A 1.b */
    KASSERT(NULL != init_proc);                         // (GRADING1A 1.b)
    KASSERT(PID_INIT == init_proc->p_pid);
    KASSERT(NULL != init_thread);

        
    return init_thread;
}


/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/sbin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */


#ifndef _DRIVERS_
static void* my_sunghan_test(int argc,void* argv) {
                    
        proc_t *p=proc_create("sunghan_test");
        kthread_t* thr=kthread_create(p,(kthread_func_t)&sunghan_test,0,NULL);
        sched_make_runnable(thr);
        int status;
        do_waitpid(p->p_pid,0,&status);
        return NULL;
}
static void* my_sunghan_deadlock_test(int argc,void* argv) {
                    
        proc_t *p=proc_create("sunghan_deadlock_test");
        kthread_t* thr=kthread_create(p,(kthread_func_t)&sunghan_deadlock_test,0,NULL);
        sched_make_runnable(thr);
        int status;
        do_waitpid(p->p_pid,0,&status);
        return NULL;
}
static void* my_faber_thread_test(int argc,void* argv) {
                    
        proc_t *p=proc_create("faber_thread_test");
        kthread_t* thr=kthread_create(p,(kthread_func_t)&faber_thread_test,0,NULL);
        sched_make_runnable(thr);
        int status;
        do_waitpid(p->p_pid,0,&status);
        return NULL;
}
#endif

#ifdef __VFS__
static int my_vfs_test(kshell_t *ksh, int argc, char** argv) {
    proc_t* vfs_test_proc = proc_create("vfs_main");
    kthread_t* vfs_test_thread = kthread_create(vfs_test_proc, &vfstest_main, 1, NULL);
    int status;
    
    (void)ksh;
    (void)argc;
    (void)argv;
    
    sched_make_runnable(vfs_test_thread);
    do_waitpid(vfs_test_proc->p_pid, 0, &status);
    
    return NULL;
}
#endif

static void *
initproc_run(int arg1, void *arg2)
{
    int status;
    
    // NOT_YET_IMPLEMENTED("PROCS: initproc_run");
    
    /********************** FABER TEST REGION BEGIN **********************/
    
    // place your tests here
    /* 
    dbg_print("initproc_run: enter faber test!\n");
    faber_thread_test(0, NULL);
    dbg_print("initproc_run: out of faber test!\n");
    */
    /********************** FABER TEST REGION END   **********************/
    
 #ifdef __DRIVERS__

        kshell_add_command("sunghan", (kshell_cmd_func_t)&my_sunghan_test, "Run my_sunghan_test().");
        kshell_add_command("deadlock", (kshell_cmd_func_t)&my_sunghan_deadlock_test, "Run my_sunghan_deadlock_test().");
        kshell_add_command("faber", (kshell_cmd_func_t)&my_faber_thread_test, "Run my_faber_thread_test().");
        
        // file system kshell commands
        kshell_add_command("thrtest", &faber_fs_thread_test, "Run faber_fs_thread_test().");
        kshell_add_command("dirtest", &faber_directory_test, "Run faber_directory_test().");
        kshell_add_command("vfstest", &my_vfs_test,          "Run my_vfs_test().");

        kshell_t *kshell = kshell_create(0);
        if (NULL == kshell) panic("init: Couldn't create kernel shell\n");
        while (kshell_execute_next(kshell));
        kshell_destroy(kshell);
        dbg(DBG_PRINT, "initproc_run\n");

#endif /* __DRIVERS__ */


    /* before exit back to idle proc, wait for all children to exit */
    while (!(list_empty(&(curproc->p_children)))) {
        (void)do_waitpid(-1, 0, &status);               // don't care which child pid exited
    }
    
    /* exit with return value 0 */
    kthread_exit((void *)0);
    
    /* should never reach here*/
    panic("\nReturned to initproc_run()!!!\n");
    return NULL;
}
