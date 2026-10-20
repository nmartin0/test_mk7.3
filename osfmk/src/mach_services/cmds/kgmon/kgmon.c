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
 */
/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
/*
 * cmk1.1
 */

#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: kgmon.c,v $ $Revision: 1.1.7.1 $ (OSF) $Date: 1995/01/06 20:53:00 $";
#endif

#include <assert.h>
#include <mach.h>
#include <device/device.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/vm.h>
#include <profile/profile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <paths.h>
#include <errno.h>

#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach/mig_errors.h>

#define task_by_pid(pid)	syscall(-33, pid)

extern int kgmon_server(int, off_t, void *, size_t);

extern char *optarg;
extern int optind;
extern int syscall(int, ...);
static void *get_record(kgmon_control_t, int, size_t, const char *);
static void *get_memory(void *, void *, size_t, const char *);
static void *get_acontext(acontext_type_t,
			  struct profile_vars *,
			  struct profile_vars *,
			  size_t *);
static void dumpstate(int);
static void do_stats(int);
static void turnonoff(int);
static int kopen(void);
static int kread(kgmon_control_t, int, void *, size_t);
static int kwrite(kgmon_control_t, int, void *, size_t);
static int kread_offset(off_t, void *, size_t);
static void *xmalloc(size_t);

static	int	bflag, hflag, rflag, pflag, vflag;
static	int	mkflag;
static	int	debug;
static	int	stats;
static	int	dummy;
static	const char *outname = GMONNAME;
static	struct profile_vars global_vars;
static	prof_flag_t orig_debug;
static	prof_flag_t orig_active;
static	prof_flag_t new_active;
static	mach_port_t devport;
static	const char *ac_name[ACONTEXT_MAX] = ACONTEXT_NAMES;

#define PROFILING_OFF 0
#define PROFILING_ON 1


int
main(int argc, char **argv)
{
	int ch;
	int cpu;
	int errors = 0;
	const char *kernel_or_server;

	while ((ch = getopt(argc, argv, "bdhmo:prsv")) != EOF) {
		switch((char)ch) {
		default:	errors++;		break;
		case 'b':	bflag++;		break;
		case 'd':	debug++;		break;
		case 'h':	hflag++;		break;
		case 'm':	mkflag++;		break;
		case 'o':	outname = optarg;	break;
		case 'p':	pflag++;		break;
		case 'r':	rflag++;		break;
		case 's':	stats++;		break;
		case 'v':	vflag++;		break;
		}
	}

	kernel_or_server = (mkflag) ? "microkernel" : "server";

	if (argv[optind] || errors) {
		(void)fprintf(stderr, "usage: kgmon [-bdhmprv] [-o name]\n");
		(void)fprintf(stderr, "\nwhere:\n\n");
		(void)fprintf(stderr, "-b\tStarts or resumes profiling.\n");
		(void)fprintf(stderr, "-d\tTurn on debugging.\n");
		(void)fprintf(stderr, "-h\tStops profiling.\n");
		(void)fprintf(stderr, "-m\tGet the micro kernel profiling information.\n");
		(void)fprintf(stderr, "-o name\tUse name instead of gmon.out.\n");
		(void)fprintf(stderr, "-p\tCollect the profiling information.\n");
		(void)fprintf(stderr, "-r\tReset the profiling information.\n");
		(void)fprintf(stderr, "-s\tPrint statistics if they are available.\n");
		(void)fprintf(stderr, "-v\tPrint out the kgmon version number.\n");
		return 1;
	}

	if (vflag) {
		printf ("%s\n", rcsid);
	}

	if (kopen() < 0) {
		(void)fprintf(stderr, "Could not open a communications path with the %s\n", kernel_or_server);
		return 2;
	}

	if (debug) {
		if (kread(KGMON_GET_DEBUG, 0, (void *)&orig_debug, sizeof(orig_debug)) < 0) {
			perror("orig_debug");
			return 3;
		}

		if (!orig_debug) {
			(void) kwrite(KGMON_SET_DEBUG_ON, 0, (void *)&dummy, sizeof(dummy));
		}
	}

	if (kread(KGMON_GET_PROFILE_VARS, 0, (void *)&global_vars, sizeof(global_vars)) < 0) {
		perror("global_vars");
		return 3;
	}

	assert(global_vars.vars_size == sizeof(struct profile_vars));
	assert(global_vars.plist_size == sizeof(struct page_list));
	assert(global_vars.acontext_size == sizeof(struct alloc_context));
	assert(global_vars.callback_size == sizeof(struct callback));
	assert(global_vars.major_version == PROFILE_MAJOR_VERSION);

	if (stats && !global_vars.output_stats) {
		stats = 0;
		(void)fprintf(stderr, "Statistics are not generated in the %s\n", kernel_or_server);
	}

	orig_active = global_vars.active;
	if (hflag) {
		new_active = PROFILING_OFF;

	} else if (bflag) {
		new_active = PROFILING_ON;

	} else {
		new_active = orig_active;
	}

	if (debug) {
		printf("orig_active = %d, new_active = %d\n", orig_active, new_active);
	}

	if (pflag || rflag) {
		turnonoff(PROFILING_OFF);
	}

	if (pflag) {
		if (!global_vars.stats.max_cpu) {
			dumpstate(0);

		} else {
			for (cpu = 0; cpu < global_vars.stats.max_cpu; cpu++) {
				dumpstate(cpu);
			}
		}
	}

	if (stats) {
		if (!global_vars.stats.max_cpu) {
			do_stats(0);

		} else {
			for (cpu = 0; cpu < global_vars.stats.max_cpu; cpu++) {
				do_stats(cpu);
			}
		}
	}

	if (rflag) {
		if (kwrite(KGMON_SET_PROFILE_RESET, 0, (void *)&dummy, sizeof(dummy)) < 0) {
			perror("KGMON_SET_PROFILE_RESET");
			return 4;
		}
	}

	turnonoff(new_active);
	(void)printf("%s profiling is %s.\n",
		     kernel_or_server,
		     new_active ? "running" : "off");

	if (debug && !orig_debug) {
		(void) kwrite(KGMON_SET_DEBUG_OFF, 0, (void *)&dummy, sizeof(dummy));
	}

	return 0;
}

/*
 * Print statistics.
 */

static void
do_stats(int cpu)
{
	struct profile_vars *pv, pv_auto;

	(void)printf("%s==================== Stats for cpu/thread %d\n\n",
		     (cpu > 0) ? "\n\f" : "", cpu);

	if (!cpu) {
		pv = &global_vars;

	} else {
		pv = &pv_auto;
		if (kread(KGMON_GET_PROFILE_VARS, cpu, (void *)&pv_auto, sizeof(pv_auto)) < 0) {
			perror("profile_vars");
			exit(4);
		}
	}

	_profile_print_stats(stdout, &pv->stats, &pv->profil_info);
}


/*
 * Get a record from the kernel or server.
 */

static void*
get_record(kgmon_control_t control, int cpu, size_t len, const char *error_msg)
{
	void *ptr = xmalloc(len);

	if (debug) {
		(void)fprintf(stderr, "Getting %ld bytes for %s\n", (long)len, error_msg);
	}

	if (kread(control, cpu, ptr, len) < 0) {
		perror(error_msg);
		exit(6);
	}

	return ptr;
}


/*
 * Get a block of memory from the kernel or server.
 */

static void *
get_memory(void *kaddr, void *ptr, size_t total_len, const char *error_msg)
{
	void *ret;
	size_t len;

	if (!ptr) {
		ptr = xmalloc(total_len > 0 ? total_len : 1);
	}

	ret = ptr;
	while (total_len > 0) {
		len = (total_len > 32768) ? 32768 : total_len;

		if (debug) {
			(void)fprintf(stderr, "Getting %ld bytes for %s, address 0x%lx\n",
				      (long)len, error_msg, (long)kaddr);
		}

		if (kread_offset((off_t)kaddr, ptr, len) < 0) {
			perror(error_msg);
			exit(6);
		}

		ptr = (char *)ptr + len;
		kaddr = (char *)kaddr + len;
		total_len -= len;
	}

	return ret;
}

/*
 * Copy an entire memory context block from the kernel or server.
 */

static void *
get_acontext(acontext_type_t ac,
	     struct profile_vars *pv,
	     struct profile_vars *pv_kernel,
	     size_t *p_size)
{
	size_t size = 0;
	size_t alloc_size;
	struct alloc_context acontext, *p_acontext;
	struct page_list plist, *p_plist;
	char err_msg[50];
	int ac_num = 0;
	int plist_num;
	void *ret, *ptr;
	struct {
		struct alloc_context acontext;
		struct page_list plist;
		char data[sizeof (void *)];
	} *header;

	/* Figure out how many bytes total are allocated */
	p_acontext = pv_kernel->acontext[ac];
	while (p_acontext != (struct alloc_context *)0) {
		if (debug) {
			putc('\n', stderr);
		}

		(void)sprintf(err_msg, "%s alloc_context#%d", ac_name[ac], ++ac_num);
		(void)get_memory(p_acontext, &acontext, sizeof(acontext), err_msg);
		p_acontext = acontext.next;

		plist_num = 0;
		p_plist = acontext.plist;
		while (p_plist != (struct page_list *)0) {
			(void)sprintf(err_msg, "%s page_list#%d", ac_name[ac], ++plist_num);
			(void)get_memory(p_plist, &plist, sizeof(plist), err_msg);
			size += plist.bytes_allocated;
			p_plist = plist.next;
		}
	}

	if (p_size) {
		*p_size = size;
	}

	if (size == 0) {
		return (void *)0;
	}

	alloc_size = ROUNDUP(size + sizeof(*header) - sizeof(header->data), pv->page_size);
	header = _profile_alloc_pages(alloc_size);
	header->acontext.next = pv->acontext[ac];
	header->acontext.plist = &header->plist;
	pv->acontext[ac] = &header->acontext;

	header->plist.first = ptr = ret = &header->data[0];
	header->plist.ptr = &header->data[0] + size;
	header->plist.next = (struct page_list *)0;
	header->plist.bytes_free = alloc_size - sizeof(*header) + sizeof(header->data);
	header->plist.bytes_allocated = size;
	header->plist.num_allocations = 0;

	/* Grab the bytes now */
	p_acontext = pv_kernel->acontext[ac];
	while (p_acontext != (struct alloc_context *)0) {
		if (debug) {
			putc('\n', stderr);
		}

		(void)sprintf(err_msg, "%s alloc_context#%d", ac_name[ac], ++ac_num);
		(void)get_memory(p_acontext, &acontext, sizeof(acontext), err_msg);
		p_acontext = acontext.next;

		plist_num = 0;
		p_plist = acontext.plist;
		while (p_plist != (struct page_list *)0) {
			(void)sprintf(err_msg, "%s page_list#%d", ac_name[ac], ++plist_num);
			(void)get_memory(p_plist, &plist, sizeof(plist), err_msg);
			p_plist = plist.next;

			if (plist.bytes_allocated > 0) {
				(void)sprintf(err_msg, "%s memory#%d", ac_name[ac], plist_num);
				(void)get_memory(plist.first, ptr, plist.bytes_allocated, err_msg);
				ptr = (char *)ptr + plist.bytes_allocated;
				header->plist.num_allocations += plist.num_allocations;
			}
		}
	}

	return ret;
}


static void
dumpstate(int cpu)
{
	char filename[40];
	struct profile_vars *pv_kernel;
	struct profile_vars pv;
	struct callback *cptr;

	/* Get the main profile variables */
	if (cpu == 0) {
		pv_kernel = &global_vars;

	} else {
		pv_kernel = get_record(KGMON_GET_PROFILE_VARS, cpu, sizeof(struct profile_vars), "vars");

		assert(pv_kernel->vars_size == sizeof(struct profile_vars));
		assert(pv_kernel->plist_size == sizeof(struct page_list));
		assert(pv_kernel->acontext_size == sizeof(struct alloc_context));
		assert(pv_kernel->callback_size == sizeof(struct callback));
		assert(pv_kernel->major_version == PROFILE_MAJOR_VERSION);
	}

	if (cpu == 0 && pv_kernel->stats.max_cpu < 2) {
		strcpy(filename, outname);

	} else {
		sprintf(filename, "%s.%02d", outname, cpu);
	}

	/* Initialize the profile output variables */
	memset((void *)&pv, '\0', sizeof (pv));
	_profile_common_init(&pv, PROFILE_NONE, PROFILE_ALLOC_MEM_NO);
	_profile_output_init(&pv, filename);

	pv.use_dci = 0;
	pv.use_profil = 0;
	pv.output_uarea = 0;
	pv.output_stats = 0;
	pv.error_msg = "Error in kgmon";
	pv.profil_info = pv_kernel->profil_info;
	pv.check_funcs = pv_kernel->check_funcs;
	pv.bogus_func  = pv_kernel->bogus_func;

	/* Set up for outputing the profil buffer */
	cptr = _profile_output_callback(&pv, _profile_output_generic, ".profhdr");
	cptr->sec_length  = sizeof(struct profile_profil);
	cptr->sec_ptr     = (void *)&pv_kernel->profil_info;

	cptr = _profile_output_callback(&pv,
					_profile_output_generic,
					(global_vars.profil_info.counter_size == sizeof(HISTCOUNTER))
						? ".profil" : ".lprofil");

	cptr->sec_val1    = pv_kernel->profil_info.lowpc;
	cptr->sec_val2    = pv_kernel->profil_info.highpc;
	cptr->sec_length  = pv_kernel->profil_info.profil_len;
	cptr->sec_recsize = global_vars.profil_info.counter_size;
	cptr->sec_ptr     = (void *)get_acontext(ACONTEXT_PROFIL, &pv, pv_kernel, (size_t *)0);
	pv.profil_buf     = cptr->sec_ptr;

	/* Set up for writing the gprof arcs */
	(void)get_acontext(ACONTEXT_PROF, &pv, pv_kernel, (size_t *)0);
	(void)get_acontext(ACONTEXT_GPROF, &pv, pv_kernel, (size_t *)0);
	(void)get_acontext(ACONTEXT_GFUNC, &pv, pv_kernel, (size_t *)0);

	cptr = _profile_output_callback(&pv, _gprof_write, ".gprof");
	cptr->sec_recsize = sizeof (struct gprof_arc);

	/* If the output file can handle it, write out prof records too */
	if (pv.multiple_sections) {
		cptr = _profile_output_callback(&pv, _prof_write, ".prof");
		cptr->sec_recsize = sizeof (struct prof_ext);
	}

	/* Write out stats if we have them, and if the output file can handle it */
	if (pv_kernel->output_stats && pv.multiple_sections) {
		cptr = _profile_output_callback(&pv, _profile_output_generic, ".stats");
		cptr->sec_ptr     = (void *)&pv_kernel->stats;
		cptr->sec_length  = sizeof (pv_kernel->stats);
	}

	/* Write out the file */
	_profile_output_write (&pv);
}


static void
turnonoff(int onoff)
{
	if (kwrite((onoff) ? KGMON_SET_PROFILE_ON : KGMON_SET_PROFILE_OFF,
		   0,
		   (void *)&dummy,
		   sizeof(dummy)) < 0) {

		perror("profile on/off");
		exit(7);
	}
}


static int
kopen(void)
{
	if (!mkflag) {
		if (kread(KGMON_GET_DEBUG, 0, (void *)&orig_debug, sizeof(orig_debug)) < 0) {
			(void)fprintf(stderr, "server is not compiled for profiling\n");
			return -1;
		}

		return 0;

	} else {
		/* use special mach kernel access */
		mach_port_t	device_server_port;
		kern_return_t	result;
		security_token_t null_security_token = {{0, 0}};

#ifdef notyet
		mach_port_t	bootstrap_port;
		mach_port_t	reply_port;
		struct imsg {
			mach_msg_header_t	hdr;
			mach_msg_type_t		port_desc_1;
			mach_port_t		port_1;
			mach_msg_type_t		port_desc_2;
			mach_port_t		port_2;
		} imsg;
#endif

		if (debug) {
			fprintf(stderr, "kopen: using mach device open\n");
		}

		device_server_port = task_by_pid(-2);
		if (device_server_port == MACH_PORT_NULL)
			return(-1);

#ifdef notyet
		/*
		 * Get our bootstrap port
		 */
		result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
		if (result != KERN_SUCCESS)
			return(-1);

		/*
		 * Allocate a reply port
		 */
		reply_port = mach_reply_port();
		if (reply_port == MACH_PORT_NULL)
			return(-1);

		if (debug) {
			printf("reply port %d\n", reply_port);
		}

		/*
		 * Send a message to it, asking for the host and device ports
		 */
		imsg.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
						    MACH_MSG_TYPE_MAKE_SEND_ONCE);
		imsg.hdr.msgh_size = 0;
		imsg.hdr.msgh_remote_port = bootstrap_port;
		imsg.hdr.msgh_local_port = reply_port;
		imsg.hdr.msgh_kind = MACH_MSGH_KIND_NORMAL;
		imsg.hdr.msgh_id = 999999;

		result = mach_msg(&imsg.hdr, MACH_SEND_MSG|MACH_RCV_MSG,
				  sizeof imsg.hdr, sizeof imsg, reply_port,
				  MACH_MSG_TIMEOUT_NONE, NULL);

		if (result != MACH_MSG_SUCCESS)
			return(-1);

		device_server_port = imsg.port_2;
#endif
		result = device_open(device_server_port, MACH_PORT_NULL,
				     D_READ|D_WRITE, null_security_token,
				     (char *)"gprof", &devport);

		if (result != KERN_SUCCESS) {
			fprintf(stderr, "device_open status %d\n", result);
			return (-1);
		}

		return (0);
	}
}


static int
kread(kgmon_control_t control, int cpu, void *buf, size_t len)
{
	off_t koffset;

	ENCODE_KGMON(koffset, control, cpu);

	if (!mkflag) {
		return kgmon_server(0, koffset, buf, len);

	} else {
		/* use special mach kernel access */

		kern_return_t result;
		mach_msg_type_number_t count;
		io_buf_ptr_t data;

		count = 0;
		result = device_read(devport,
				     0,	/* mode */
				     koffset,
				     len,
				     &data,
				     &count);

		if (result != KERN_SUCCESS) {
			(void)fprintf(stderr, "device_read returned %d\n", result);
			_Seterrno(EINVAL);
			return -1;
		}

		/* copy data to buf and deallocate ool data area */
		memcpy(buf, (void *)data, count);
		vm_deallocate(mach_task_self(), (vm_offset_t)data, count);

		return len;
	}
}


static int
kread_offset(off_t koffset, void *buf, size_t len)
{
	if (!mkflag) {
		return kgmon_server(0, koffset, buf, len);

	} else {
		/* use special mach kernel access */

		kern_return_t result;
		mach_msg_type_number_t count;
		io_buf_ptr_t data;

		count = 0;
		result = device_read(devport,
				     0,	/* mode */
				     koffset,
				     len,
				     &data,
				     &count);

		if (result != KERN_SUCCESS) {
			(void)fprintf(stderr, "device_read returned %d\n", result);
			_Seterrno(EINVAL);
			return -1;
		}

		/* copy data to buf and deallocate ool data area */
		memcpy(buf, (void *)data, count);
		vm_deallocate(mach_task_self(), (vm_offset_t)data, count);

		return len;
	}
}


static int
kwrite(kgmon_control_t control, int cpu, void *buf, size_t len)
{
	off_t koffset;

	ENCODE_KGMON(koffset, control, cpu);

	if (!mkflag) {
		return kgmon_server(1, koffset, buf, len);

	} else {
		/* use special mach kernel access */

		kern_return_t result;
		int count;

		count = 0;
		result = device_write(devport,
				      0,	/* mode */
				      koffset,
				      buf,
				      len,
				      &count);

		if (result != KERN_SUCCESS) {
			(void)fprintf(stderr, "device_read returned %d\n", result);
			_Seterrno(EINVAL);
			return -1;
		}
		return len;
	}

	(void)fprintf(stderr, "no support yet.\n");
	_Seterrno(EINVAL);
	return -1;
}


static void *
xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (!ptr) {
		perror ("malloc");
		exit (1);
	}

	return ptr;
}
