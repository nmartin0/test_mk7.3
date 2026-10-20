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

#include <mach/bootstrap.h>
#include <mach/clock.h>
#include <mach/clock_reply.h>
#include <mach/default_pager_object.h>
#include <device/device.h>
#include <device/device_reply.h>
#include <device/device_request.h>
#include <mach/exc.h>
#include <mach_debug/mach_debug.h>
#include <mach/mach_host.h>
#include <mach/mach_interface.h>
#include <mach/mach_norma.h>
#include <mach/mach_port.h>
#include <mach/memory_object.h>
#include <mach/memory_object_default.h>
#include <mach/memory_object_server.h>
#include <mach/notify.h>
#include <mach/notify_server.h>
#include <mach/prof.h>
#include <mach/sync.h>

#include <stdio.h>
#include <mach.h>
#include <math.h>
#include <sys/syscall.h>
#include "machine_tables.h"

#include "config.h"
#include <mach/etap.h>
#include <mach/etap_events.h>
#include "convert.h"
#include "unix_map.h"		/* unix system call map */
#include "mach_map.h"		/* Mach system call map */
#include "system_tables.h"


struct table_entry {
	char		name[MAX_NAME_SIZE];
	unsigned int	number;
};

/*
 * Mach message table
 *
 * Note: Most mach system calls are actually implemented as messages.
 */
struct table_entry mach_message_table[] = {
	subsystem_to_name_map_bootstrap,
	subsystem_to_name_map_clock,
	subsystem_to_name_map_clock_reply,
	subsystem_to_name_map_default_pager_object,
	subsystem_to_name_map_device,
	subsystem_to_name_map_device_reply,
	subsystem_to_name_map_device_request,
	subsystem_to_name_map_exc,
	subsystem_to_name_map_mach,
	subsystem_to_name_map_mach_debug,
	subsystem_to_name_map_mach_host,
	subsystem_to_name_map_mach_norma,
	subsystem_to_name_map_mach_port,
	subsystem_to_name_map_memory_object,
	subsystem_to_name_map_memory_object_default,
	subsystem_to_name_map_notify,
	subsystem_to_name_map_prof,
	subsystem_to_name_map_sync
};

int	mach_message_table_entries = sizeof(mach_message_table) /
			             sizeof(struct table_entry);


char *
system_table_lookup(unsigned int table, unsigned int number)
{
	int x;
	char *name = NULL;

	switch (table) {
	case SYS_TABLE_MACH_TRAP:
		name = mach_trap_name(number >> 4);
		break;
	case SYS_TABLE_MACH_MESSAGE:
		for (x=0; x < mach_message_table_entries; x++) {
			if (mach_message_table[x].number == number) {
				name = mach_message_table[x].name;
				break;
			}
		}
		break;
	case SYS_TABLE_UNIX_SYSCALL:
		number = -number;
		name = syscall_name(number);
		break;
	case SYS_TABLE_INTERRUPT:
		name = interrupt_name(number);
		break;
	case SYS_TABLE_EXCEPTION:
		name = exception_name(number);
		break;
	}
	return (name != NULL) ? name : "undefined";
}

void
system_table_print(unsigned int table)
{
	int x;
	char *name;

	switch (table) {
	case SYS_TABLE_MACH_TRAP:
		printf("%-*s %5s %8s\n",MAX_NAME_SIZE,
			"Mach Trap Name", "Real", "Raw");
		printf("===================================================\n");
		for (x=0; x < N_MACH_TRAP_NAMES; x++) {
			name = mach_trap_name(x);
			if (name != NULL)
				printf("%-*s %5d %8x\n",
				       MAX_NAME_SIZE, name, x, x << 4);
		}
		break;

	case SYS_TABLE_MACH_MESSAGE:
		printf("%-*s %8s\n",MAX_NAME_SIZE,
			"Mach Message Name", "Real");
		printf("===================================================\n");
		for (x=0; x < mach_message_table_entries; x++)
			printf("%-*s %8d\n",
			       MAX_NAME_SIZE,
			       mach_message_table[x].name,
			       mach_message_table[x].number);
		break;

	case SYS_TABLE_UNIX_SYSCALL:
		printf("%-*s %5s %8s\n",MAX_NAME_SIZE,
			"UNIX System Call Name", "Real", "Raw");
		printf("===================================================\n");
		for (x=0; x < N_SYSCALL_NAMES; x++)
			printf("%-*s %5d %8x\n", MAX_NAME_SIZE, syscall_name(x),
			       x, -x);
		break;

	case SYS_TABLE_INTERRUPT:
#ifdef RAW_INTERRUPT_NUMBER
		printf("%-*s %5s %8s\n",MAX_NAME_SIZE,
		       "Mach Interrupt Name", "Real", "Raw");
#else
		printf("%-*s %5s\n",MAX_NAME_SIZE,
		       "Mach Interrupt Name", "Real", "Raw");
#endif
		printf("===================================================\n");
		for (x=0; x < N_INTERRUPT_NAMES; x++) {
			name = interrupt_name(x);
			if (name != NULL) {
#ifdef RAW_INTERRUPT_NUMBER
				printf("%-*s %5d %8x\n", MAX_NAME_SIZE,
				       name, x, RAW_INTERRUPT_NUMBER(x));
#else
				printf("%-*s %5d\n", MAX_NAME_SIZE,
				       name, x, x + 0x40);
#endif
			}
		}
		break;

	case SYS_TABLE_EXCEPTION:
		printf("%-*s %8s\n",MAX_NAME_SIZE,
			"Exception Name", "Real");
		printf("===================================================\n");
		for (x=0; x < N_EXCEPTION_NAMES; x++)
			printf("%-*s %8d\n", MAX_NAME_SIZE, exception_name(x),
			       x);
		break;
	}
}
