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

char *mach_trap_names[] = {
/*   0 */	"undefined",
/*   1 */	NULL,
/*   2 */	NULL,
/*   3 */	NULL,
/*   4 */	NULL,
/*   5 */	NULL,
/*   6 */	NULL,
/*   7 */	NULL,
/*   8 */	NULL,
/*   9 */	NULL,
/*  10 */	"task_self",
/*  11 */	"thread_reply",
/*  12 */	"task_notify",
/*  13 */	"thread_self",
/*  14 */	NULL,
/*  15 */	NULL,
/*  16 */	NULL,
/*  17 */	"evc_wait",
/*  18 */	NULL,
/*  19 */	NULL,
/*  20 */	"msg_send_trap",
/*  21 */	"msg_receive_trap",
/*  22 */	"msg_rpc_trap",
/*  23 */	NULL,
/*  24 */	NULL,
/*  25 */	NULL,
/*  26 */	"mach_reply_port",
/*  27 */	"mach_thread_self",
/*  28 */	"mach_task_self",
/*  29 */	"mach_host_self",
/*  30 */	"vm_read_overwrite",
/*  31 */	"vm_write",
/*  32 */	"mach_msg_overwrite_trap",
/*  33 */	NULL,
/*  34 */	NULL,
/*  35 */	"mach_rpc_trap",
/*  36 */	"mach_rpc_return_trap",
/*  37 */	NULL,
/*  38 */	NULL,
/*  39 */	NULL,
/*  40 */	NULL,
/*  41 */	"init_process",
/*  42 */	NULL,
/*  43 */	"map_fd",
/*  44 */	"rfs_make_symlink",
/*  45 */	NULL,
/*  46 */	NULL,
/*  47 */	NULL,
/*  48 */	NULL,
/*  49 */	NULL,
/*  50 */	NULL,
/*  51 */	NULL,
/*  52 */	"htg_syscall",
/*  53 */	NULL,
/*  54 */	NULL,
/*  55 */	"host_self",
/*  56 */	NULL,
/*  57 */	NULL,
/*  58 */	NULL,
/*  59 */	"swtch_pri",
/*  60 */	"swtch",
/*  61 */	"thread_switch",
/*  62 */	"clock_sleep_trap",
/*  63 */	NULL,
/*  64 */	"vm_map",
/*  65 */	"vm_allocate",
/*  66 */	"vm_deallocate",
/*  67 */	NULL,
/*  68 */	"task_create",
/*  69 */	"task_terminate",
/*  70 */	"task_suspend",
/*  71 */	"task_set_special_port",
/*  72 */	"mach_port_allocate",
/*  73 */	"mach_port_deallocate",
/*  74 */	"mach_port_insert_right",
/*  75 */	"mach_port_allocate_name",
/*  76 */	"thread_depress_abort",
/*  77 */	"device_read",
/*  78 */	"device_read_request",
/*  79 */	"device_write",
/*  80 */	"device_write_request",
/*  81 */	"device_read_overwrite",
/*  82 */	"device_read_overwrite_request",
/*  83 */	"clock_get_time",
/*  84 */	"thread_create_running",
/*  85 */	"kernel_task_create",
/*  86 */	"vm_msync",
/*  87 */	"thread_set_exception_ports",
/*  88 */	"mach_port_destroy",
/*  89 */	"task_set_exception_ports",
/*  90 */	"mach_port_rename",
/*  91 */	"vm_protect",
/*  92 */	"vm_region",
/*  93 */	"task_info",
/*  94 */	"device_read_async",
/*  95 */	"device_read_overwrite_async",
/*  96 */	"device_write_async",
/*  97 */	"io_done_queue_wait",
/*  98 */	"device_write_async_inband",
/*  99 */	"device_read_async_inband",
/* 100 */	"mach_port_allocate_subsystem",
/* 101 */	"mach_port_move_member",
/* 102 */	"vm_remap",
/* 103 */	"mach_port_allocate_qos",
/* 104 */	"task_set_port_space",
/* 105 */	"mach_port_allocate_full",
/* 106 */	NULL,
/* 107 */	NULL,
/* 108 */	"etap_probe",
/* 109 */	"etap_trace_event"
};
#define N_MACH_TRAP_NAMES (sizeof mach_trap_names / sizeof mach_trap_names[0])
#define mach_trap_name(nu) \
	(((nu) < N_MACH_TRAP_NAMES) ? mach_trap_names[nu] : NULL)
