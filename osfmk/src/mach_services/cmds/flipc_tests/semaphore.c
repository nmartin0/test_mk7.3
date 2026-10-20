/*
 * Copyright 1991-1998 by Open Software Foundation, Inc. 
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * cmk1.1
 */

#define INSTANCES 1

/*
 * Includes main, usage, and file system synchronization routines.
 */
#include <main.h>
#include <mach/sync.h>
#include <mach/mach_traps.h>
#include <mach/mach_port.h>

typedef void *any_t;

any_t thread_1(any_t arg);
any_t thread_2(any_t arg);

int syncvar = 0;
semaphore_port_t sema;

void usermain(char *filename, int instance)
{
    mach_port_t my_task = mach_task_self();
    kern_return_t kr;
    cthread_t thread1, thread2;

    ftest1(semaphore_create(my_task, &sema, 0, 0),
	   kr, KERN_SUCCESS, 1);
    mach_port_deallocate(my_task, my_task);

    /* Signal before wait is easy.  If the following doesn't hang, it
       works.  */
    ftest1(semaphore_signal(sema), kr, KERN_SUCCESS, 1);
    ftest1(semaphore_wait(sema), kr, KERN_SUCCESS, 1);

    /* Wait before signal is harder.  Fork threads.  */

    /* Fork threads.  */
    thread1 = cthread_fork(thread_1, (any_t)100);
    thread2 = cthread_fork(thread_2, (any_t)100);

    cthread_join(thread1);
    cthread_join(thread2);
}

any_t thread_1(any_t arg)
{
    kern_return_t kr;
    int i;

    /* Wait until the other thread has indicated it's about to wait.  */
    while (!syncvar)
	;

    for (i = 0; i < 10000; i++)
	;

    /* Then signal the semaphore.  */
    ftest1(semaphore_signal(sema), kr, KERN_SUCCESS, 1);
    return (any_t) 0;
}

any_t thread_2(any_t arg)
{
    kern_return_t kr;

    /* Dive in.  */
    syncvar = 1;
    ftest1(semaphore_wait(sema), kr, KERN_SUCCESS, 1);
    return (any_t) 0;
}


