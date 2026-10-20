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
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * cmk1.1
 */

#include <stdio.h>
#include <string.h>
#include <mach.h>
#include <mach_debug/mach_debug.h>
#include <mach_error.h>
#include <servers/netname.h>
#include <servers/machid.h>
#include <servers/machid_debug.h>

#define streql(a, b)	(strcmp((a), (b)) == 0)

static void print_vm_info();
static vm_offset_t print_vm_region();

static mach_port_t server;
static mach_port_t auth;

static boolean_t TaskHold = FALSE;
static boolean_t Verbose = FALSE;

static void
usage()
{
    fprintf(stderr, "usage: vminfo [-host machine] [-v] [-z] [taskid]\n");
    exit(1);
}

static void
parse_args(argc, argv, taskp)
    int argc;
    char *argv[];
    mtask_t *taskp;
{
    char *hostname = "";
    mtask_t task;
    kern_return_t kr;
    int i;

    for (i = 1; i < argc; i++)
	if (streql(argv[i], "-host") && (i < argc-1))
	    hostname = argv[++i];
	else if (streql(argv[i], "-z"))
	    TaskHold = TRUE;
	else if (streql(argv[i], "-Z"))
	    TaskHold = FALSE;
	else if (streql(argv[i], "-v"))
	    Verbose = TRUE;
	else if (streql(argv[i], "-V"))
	    Verbose = FALSE;
	else if (streql(argv[i], "--")) {
	    i++;
	    break;
	} else if (argv[i][0] == '-')
	    usage();
	else
	    break;

    kr = netname_look_up(name_server_port, hostname, "MachID", &server);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "vminfo: netname_lookup_up(MachID): %s\n",
	     mach_error_string(kr));
	exit(1);
    }

    auth = mach_host_priv_self();
    if (auth == MACH_PORT_NULL)
	auth = mach_task_self();

    switch (argc - i) {
      case 0:
	kr = machid_mach_register(server, auth, mach_task_self(),
				  MACH_TYPE_TASK, &task);
	if (kr != KERN_SUCCESS) {
	    fprintf(stderr, "vminfo: machid_mach_register: %s\n",
		 mach_error_string(kr));
	    exit(1);
	}
	break;

      case 1:
	task = atoi(argv[i]);
	break;

      default:
	usage();
    }

    *taskp = task;
}

void
main(argc, argv)
    int argc;
    char *argv[];
{
    mtask_t task;
    kern_return_t kr;

    parse_args(argc, argv, &task);

    if (TaskHold) {
	kr = machid_task_suspend(server, auth, task);
	if (kr != KERN_SUCCESS) {
	    fprintf(stderr, "vminfo: machid_task_suspend: %s\n",
		 mach_error_string(kr));
	    exit(1);
	}
    }

    print_vm_info(task, print_vm_region);

    if (TaskHold) {
	kr = machid_task_resume(server, auth, task);
	if (kr != KERN_SUCCESS) {
	    fprintf(stderr, "vminfo: machid_task_resume: %s\n",
		 mach_error_string(kr));
	    exit(1);
	}
    }

    exit(0);
}

static char *inherit_name[] =
	{ "both", "copy", "none", "move" };
static char *prot_name[] =
	{ "   ", "R  ", " W ", "RW ", "  X", "R X", " WX", "RWX" };
static char *copy_name[] =
	{ "none", "call", "delay" };

static vm_offset_t
print_vm_region(task, addr)
    mtask_t task;
    vm_offset_t addr;
{
    region_info_t info;
    kern_return_t kr;

    kr = machid_vm_region(server, auth, task, addr, &info);
    if (kr == KERN_NO_SPACE || kr == KERN_INVALID_ADDRESS)
	return 0;
    else if (kr != KERN_SUCCESS) {
	fprintf(stderr, "vminfo: machid_vm_region: %s\n",
	     mach_error_string(kr));
	exit(1);
    }

    printf("Region %08x-%08x: ", info.vr_address,
	   info.vr_address + info.vr_size);
    printf("prot=%s/%s/%s, ",
	   prot_name[info.vr_prot],
	   prot_name[info.vr_max_prot],
	   inherit_name[info.vr_inherit]);
    printf("name=%4d, offset=%x\n", info.vr_name, info.vr_offset);

    return info.vr_address + info.vr_size;
}

static void
print_region(region)
    vm_info_region_t region;
{
    printf("Region %08x-%08x: ",
	   region.vir_start, region.vir_end);
    printf("prot=%s/%s/%s",
	   prot_name[region.vir_protection],
	   prot_name[region.vir_max_protection],
	   inherit_name[region.vir_inheritance]);
    if ((region.vir_wired_count != 0) ||
	(region.vir_user_wired_count != 0))
	printf(", wired=%u/%u",
	       region.vir_wired_count, region.vir_user_wired_count);
    if (region.vir_object != 0)
	printf(", offset=%x", region.vir_offset);
    if (region.vir_needs_copy)
	printf(", copy needed");
}

static void
print_object(object)
    vm_info_object_t object;
{
    printf("    Object %08x: size = %x, %u refs, %u pages",
	   object.vio_object, object.vio_size,
	   object.vio_ref_count, object.vio_resident_page_count);
    printf(", %u absent, %u paging ops\n",
	   object.vio_absent_count, object.vio_paging_in_progress);

    if (object.vio_temporary)
	printf("      temp");
    else
	printf("      perm");
    if (object.vio_internal)
	printf(", internal");
    else
	printf(", external");
    if (object.vio_can_persist)
	printf(", cache");
    if (object.vio_pager_created) {
	printf(", created");
	if (!object.vio_pager_ready)
	    printf(", not ready");
	if (!object.vio_pager_initialized)
	    printf(", uninitialized");
    } else {
	if (object.vio_pager_ready)
	    printf(", ready");
	if (object.vio_pager_initialized)
	    printf(", initialized");
    }
    if (object.vio_lock_in_progress)
	printf(", locking");
    if (object.vio_lock_restart)
	printf(", restart");
    if (!object.vio_alive)
	printf(", dead");

    if (object.vio_copy_strategy != 0)
	printf(", copy %s", copy_name[object.vio_copy_strategy]);
    if (object.vio_paging_offset != 0)
	printf(", paging offset = %x", object.vio_paging_offset);
    if (object.vio_shadow != 0)
	printf(", shadow = %08x (%x)",
	       object.vio_shadow, object.vio_shadow_offset);
    if (object.vio_copy != 0)
	printf(", copy = %08x", object.vio_copy);
    printf(", alloc = %x", object.vio_last_alloc);
    printf("\n");
}

static void
print_vm_info(task, func)
    mtask_t task;
    vm_offset_t (*func)();
{
    vm_offset_t addr;

    for (addr = (*func)(task, 0); addr != 0; addr = (*func)(task, addr))
	continue;
}
