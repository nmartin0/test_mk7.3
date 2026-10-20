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

/*
 * Simple POSIX thread test
 */

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_THREADS    512
#define THREAD_DEPTH    32

void *
thread(void *arg)
{
	printf("thread %x here: %s\n", pthread_self(), arg);
	return (arg);
}

main(int argc, char *argv[])
{
	int res, i, j;
	void *thread_res;
	pthread_t t[THREAD_DEPTH];
	char *msg;
	for (i = 0;  i < MAX_THREADS;  i += THREAD_DEPTH)
	{
		printf("Creating threads %d..%d\n", i, i+THREAD_DEPTH-1);
		for (j = 0;  j < THREAD_DEPTH;  j++)
		{
			msg = malloc(64);
			sprintf(msg, "Thread #%d", i+j);
			res = pthread_create(&t[j], (pthread_attr_t *)NULL, thread, msg);
			if (res != ESUCCESS)
			{
				printf("Can't create thread #%d: %d\n", i+j, errno);
				exit(1);
			}
		}
		printf("Waiting for threads\n");
		for (j = 0;  j < THREAD_DEPTH;  j++)
		{
			res = pthread_join(t[j], &thread_res);
			if (res != ESUCCESS)
			{
				printf("Can't join with thread #%d: %d\n", i+j, errno);
				exit(2);
			}
			printf("Thread #%d: %x\n", i+j, thread_res);
		}
	}
	sleep(5);
}
