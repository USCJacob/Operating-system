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

#include "globals.h"
#include "errno.h"

#include "main/interrupt.h"

#include "proc/sched.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/debug.h"

void ktqueue_enqueue(ktqueue_t *q, kthread_t *thr);
kthread_t * ktqueue_dequeue(ktqueue_t *q);

/*
 * Updates the thread's state and enqueues it on the given
 * queue. Returns when the thread has been woken up with wakeup_on or
 * broadcast_on.
 *
 * Use the private queue manipulation functions above.
 */
void
sched_sleep_on(ktqueue_t *q)
{
        //NOT_YET_IMPLEMENTED("PROCS: sched_sleep_on");
         curthr->kt_state=KT_SLEEP;         // set the thread state as sleep
         dbg(DBG_PRINT, "(GRADING1A)\n");
         ktqueue_enqueue(q,curthr);         // push thread into the blocking queue
	 sched_switch();                    // switch to next thread in runnable queue
}

kthread_t *
sched_wakeup_on(ktqueue_t *q)
{
        //NOT_YET_IMPLEMENTED("PROCS: sched_wakeup_on");
        //return NULL;
        if(!list_empty(&q->tq_list)){
                kthread_t *thr = ktqueue_dequeue(q);
                dbg(DBG_PRINT, "(GRADING1A)\n");
                KASSERT(thr != NULL);
                KASSERT((thr->kt_state == KT_SLEEP) || (thr->kt_state == KT_SLEEP_CANCELLABLE));
                dbg(DBG_PRINT, "(GRADING1A 4.a)\n");
                sched_make_runnable(thr);
                return thr;
        }
        return NULL;
}

void
sched_broadcast_on(ktqueue_t *q)
{
        //NOT_YET_IMPLEMENTED("PROCS: sched_broadcast_on");
        while(!list_empty(&q->tq_list)){
                kthread_t *thr = ktqueue_dequeue(q);
                dbg(DBG_PRINT, "(GRADING1A)\n");
                KASSERT(thr != NULL);
                KASSERT((thr->kt_state == KT_SLEEP) || (thr->kt_state == KT_SLEEP_CANCELLABLE));
                dbg(DBG_PRINT, "(GRADING1A 4.a)\n");
                sched_make_runnable(thr);
        }
}

