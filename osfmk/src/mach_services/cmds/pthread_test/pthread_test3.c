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
 * Test POSIX Thread Specific Data
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

pthread_key_t key;

void *
thread(void *arg)
{
	char *msg;
	int res;
	printf("thread %x here: %s\n", pthread_self(), arg);
	msg = malloc(256);
	sprintf(msg, "This is thread specific data for %x\n", pthread_self());
	if (res = pthread_setspecific(key, msg))
	{
		printf("thread 0x%x: Can't set TSD - %d\n", pthread_self(), res);
	}
	return (arg);
}

void
grim_reaper(void *param)
{
	printf("grim_reaper - self: %x, param: %x\n", pthread_self(), param);
/*	free(param); */
}

main(int argc, char *argv[])
{
	int res;
	void *thread_res;
	pthread_t t1, t2;
	printf("%s: begin\n", argv[0]);
	res = pthread_key_create(&key, grim_reaper);
	printf("%s: create key = %x, res = %d\n", argv[0], *(long *)&key, res);
	res = pthread_create(&t1, (pthread_attr_t *)NULL, thread, "thread #1 arg");
	printf("%s: result = %d, thread = %x\n", argv[0], res, t1);
	res = pthread_create(&t2, (pthread_attr_t *)NULL, thread, "thread #2 arg");
	printf("%s: result = %d, thread = %x\n", argv[0], res, t2);
	res = pthread_join(t1, &thread_res);
	printf("join(%x) = %x.%d\n", t1, thread_res, res);
	res = pthread_join(t2, &thread_res);
	printf("join(%x) = %x.%d\n", t2, thread_res, res);
	sleep(5);
}
