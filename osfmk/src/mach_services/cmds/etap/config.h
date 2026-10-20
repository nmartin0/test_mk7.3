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
 *  File     : config.h
 *
 *  Author   : Joe CaraDonna
 *
 *  Function : ETAP Configurations
 *
 *             These configuration definitions are used to customize
 *             the ETAP utilities for different environments.
 */ 

#ifndef _ETAP_CONFIG_H_
#define _ETAP_CONFIG_H_


/* ============================================================================
 *  Log names
 * ============================================================================
 */

#define CBUFF_DATA_FILE   "/tmp/etap_cdata.log"
#define MBUFF_DATA_FILE   "/tmp/etap_mdata.log"

/* ============================================================================
 *  General definitions for all utilities
 * ============================================================================
 */

/* MAX number of processors utilities will support */
#define PROCESSORS          1

/* MAX size of a file name given at the command line */
#define FILE_NAME_SIZE      60

/* converted file name size, mainly used for monitored buffer filenames */
#define CONV_FILE_NAME_SIZE (FILE_NAME_SIZE + 6)


/* ============================================================================
 *  Utilities: etap_mstat, etap_cstat, etap_trace
 * ============================================================================
 */


/* The maximum number of objects excepted in object lists.
 * Note: this value must be >= the number of entries in type table.
 * This is a HACK - static arrays, bla
 */
#define MAX_OBJECTS	        200

/* another HACK */
#define NUM_SUBSYS	        12

/* This parameter is used by etap_mstat when reading symbol table information.
 * It determines the maximum length of a function name to be displayed.
 */
#define FUNCTION_NAME_SIZE	40

/* Number of etap_mstat distribution buckets */  
#define MSTAT_IBUCKETS		10

/* Byte size of the file buffers */
#define MSTAT_BUFSIZE		1024000


/* ============================================================================
 *  etap_md Tuning Controls
 * ============================================================================
 */

/* base thread sleep interval in ms */
#define SLEEP_INTERVAL          150

/* time between lost_data.log writes */
#define LOST_WRITE_INTERVAL     1000

/* # of chew cycles for TS update */
#define TIMESTAMP_UPDATE_DELAY  10

#endif /* _ETAP_CONFIG_H_ */


