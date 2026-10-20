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

#include <stdio.h>

/*
 * If we aren't going to do any timing, have this file have no effect.
 */
#ifndef TIMING
#define TIMER_INIT()
#define TIMER_SAMPLE(s)
#define TIMER_SUBTRACT(a,b,result)
#define TIMER_CONVERT(source, dest)
#else

/*
 *	Various trace flag options, to be passed
 *	to the TR_FLAGS call.
 */
#define				TR_EXTENDED	1
#define				TR_OVERWRITE	2
#define				TR_CLOCK_SYNC	4

/*
 *  getstat/setstat flavors
 */
#define	TR_FLAGS	1	/* set/get trace flags */
#define	TR_GET_RD_SIZE	2	/* get size of trace buffer */
#define	TR_RESET	3	/* reset/calibrate driver (retains old thread)*/
#define	TR_SET_THREAD	4	/* set thread, 0=all, odd=set but don't start */
#define	TR_START	5	/* start trace (clear low order thread bit) */
#define	TR_STOP		6	/* stop trace (set low order thread bit) */

extern int	time_trace_trap(/* VARARGS */);

#define TIMER_LOG_SIZE 4096

/* 
 * Macro interface.
 */

#ifdef PARAGON
typedef struct timer_data {
    unsigned int sample[2];
} timer_sample_data;
typedef timer_sample_data *timer_sample_t;

/* Init and convert are function calls.  */
extern void paragon_timer_init(void);
extern void paragon_timer_convert(timer_sample_t, double *);
volatile unsigned int	*paragon_clock;

#define TIMER_INIT() paragon_timer_init()
#define TIMER_DESTROY() 
#define TIMER_SAMPLE(s) 			\
do {						\
    (s)->sample[1] = paragon_clock[1];		\
    (s)->sample[0] = paragon_clock[0];		\
} while ((s)->sample[1] != paragon_clock[1]);
#define TIMER_SUBTRACT(a,b,result)				\
do {								\
    if ((a)->sample[0] >= (b)->sample[0]) {			\
	(result)->sample[0] = (a)->sample[0] - (b)->sample[0];	\
	(result)->sample[1] = (a)->sample[1] - (b)->sample[1];	\
    } else {							\
	unsigned int tmp = 0xffffffffu - (b)->sample[0];	\
	(result)->sample[0] = tmp + (a)->sample[0] + 1;		\
	(result)->sample[1] = (a)->sample[1] - (b)->sample[1] - 1;\
    }								\
} while (0)
#define TIMER_CONVERT(source, dest) paragon_timer_convert(source, dest)
#else /* PARAGON */
typedef double timer_sample_data;
typedef timer_sample_data *timer_sample_t;

extern void time_map(void);
extern void time_unmap(void);
extern void map_time_sample(timer_sample_t);

/* Don't bother with macros.  */
#define TIMER_INIT()  time_map()
#define TIMER_DESTROY() time_unmap()
#define TIMER_SAMPLE(s) map_time_sample(s)
#define TIMER_SUBTRACT(a,b,result) do { *(result) = *(a) - *(b); } while (0)
#define TIMER_CONVERT(source,dest) do { *(dest) = *(source); } while (0)
#endif	/* PARAGON */

/* Log declaration; same on all platforms.  */

extern struct timer_log_entry {
    timer_sample_data t;
    char *annotation;
} timer_log[TIMER_LOG_SIZE];

extern int timer_log_index;

#define TIMER_LOG(time, note) 				\
do {							\
    timer_log[timer_log_index].t = (time);		\
    timer_log[timer_log_index++].annotation = (note);	\
    if (timer_log_index == TIMER_LOG_SIZE)		\
	timer_log_index = 0;				\
} while (0)

/* Sample directly into the log.  */
#define TIMER_SAMPLE_LOG(note)				\
do {							\
    TIMER_SAMPLE(&(timer_log[timer_log_index].t));	\
    timer_log[timer_log_index++].annotation = (note);	\
    if (timer_log_index == TIMER_LOG_SIZE)		\
	timer_log_index = 0;				\
} while (0)
    
/*
 * The point of these three routines is to allow multiple timing streams to
 * be combined in raw form (ie. data from multiple paragon nodes).  To do
 * this, each of the nodes would when finished do a timer_dump_log_naked,
 * the programmer would combine the outputs, and feed it into another
 * program which would do a timer_read_log_naked followed by a
 * timer_dump_log_clothed.
 *
 * In pursuit of these objectives, there is no size limit on the
 * timer_read_log_naked, and it does not use the internal log
 * variable.  Similarly, timer_dump_log_clothed sorts its entries before
 * dumping them.
 */
 
extern void timer_dump_log_naked(FILE *stream);
extern struct timer_log_entry *timer_read_log_naked(FILE *stream);
extern void timer_dump_log_clothed(FILE *stream, struct timer_log_entry *);

#endif /* TIMING */
