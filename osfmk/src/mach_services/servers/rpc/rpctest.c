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
 * cmk1.1
 */

#include <mach.h>
#include <stdio.h>
#include <mach_init.h>
#include <mach/rpc.h>
#include <mig_errors.h>
#include <servers/netname.h>
#include <rthreads.h>
#include <mach/exc_server.h>


#include "rpc_defs.h"
#include "rpctestServer.h"
#include "rpctestUser.h"
#include "rpctest.h"

void			init();
#ifdef DEBUG
void			display_port_rights();
#endif /*DEBUG*/

extern void process_printf();

mach_port_t		rpc_port;
mach_port_t		rpc_subsystem_port;
mach_port_t		rpctest_activation;

vm_offset_t		stack;
vm_size_t		stack_size;


#ifndef KERN_RPC_TERMINATE_ORPHAN
#define KERN_RPC_TERMINATE_ORPHAN	(44)
#define KERN_RPC_CONTINUE_ORPHAN	(45)
#endif

kern_return_t orphan_result = KERN_SUCCESS;

/* define various levels of debugging */
/*** this should be moved to someplace reasonable -- rkc, 9/16/94 ***/
#define DEBUG_LEVEL_NONE		( 0 )
#define DEBUG_LEVEL_TRACE		( DEBUG_LEVEL_NONE + 1 )
#define DEBUG_LEVEL_DETAILED_INFO	( DEBUG_LEVEL_TRACE + 1 )
#define DEBUG_LEVEL_MORE_DETAILED_INFO	( DEBUG_LEVEL_DETAILED_INFO + 1 )

#define debug_printf(level) if (debug >= level) rprintf

static char *debug_level[] =
{
    "DEBUG_LEVEL_NONE",
    "DEBUG_LEVEL_TRACE",
    "DEBUG_LEVEL_DETAILED_INFO",
    "DEBUG_LEVEL_MORE_DETAILED_INFO"
};

int	debug = 0;
int name_debug = 0;
boolean_t quit_rpctest = FALSE;

/*** workaround because MIG treats strings as 256-byte fixed-length
 *** arrays -- rkc, 9/16/94 ***/
#define XXX_STR_BUF_SIZE	( 256 )

#define RPC_MAX_MSG_SIZE	4096
	/*** should be changed to something else -- rkc, 9/9/94 ***/

/*** workaround for `printf()' problems from Mach-created threads ***/
mach_port_name_t	main_thread;

mach_port_name_t	task_exc_port = MACH_PORT_NULL;
rthread_t		exc_hdlr_thread;

boolean_t		exception_handler_default = TRUE;
boolean_t		exception_handler_installed = FALSE;

/*** procedure predeclarations -- fix terminology -- rkc, 8/19/94 ***/
void			task_exc_handler( char * );
kern_return_t		install_exception_handler_internal();
void                    init_rthread_printf();

char *taskname = "rpctest";
static int numacts = 1;

extern function_table_entry rpctest_client_functions[];

#define MAX_ARG_SIZE 128

mach_port_t
local_rpc_port()
{
    return rpc_port;
}

char *
get_int_arg(char *args, int *n)
{
    char arg[MAX_ARG_SIZE];

    sscanf(args, " %s", arg);
    sscanf(arg, "%d", n);

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: get_int_arg(args = \"%s\", int = %d)\n",
		taskname, args, *n);

    return( strstr(args, arg)+strlen(arg) );
}

char *
get_string_arg(char *args, char **s)
{
    char arg[MAX_ARG_SIZE];

    sscanf(args, " %s", arg);
    *s = (char *) malloc(strlen(arg+1));
    strcpy(*s, arg);

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: get_string_arg(args = \"%s\", string = %s)\n",
		taskname, args, *s);

    return( strstr(args, arg)+strlen(arg) );
}

char *
get_port_arg(char *args, mach_port_t *p)
{
    char arg[MAX_ARG_SIZE];
    kern_return_t kr;

    sscanf(args, " %s", arg);
    kr = netname_look_up(name_server_port, "", arg, p);

    if (kr != NETNAME_SUCCESS)
    {
	mach_error("rpctest netname_look_up:", kr);
	exit(-1);
    }

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: get_port_arg(args = \"%s\", target = %x)\n",
		taskname, args, *p);

    return( strstr(args, arg)+strlen(arg) );
}


char *
set_activation_name(rthread_t act, char *taskname, int num)
{
    kern_return_t kr;
    char actname[128];
    char *name;

    sprintf(actname, "%s_%d", taskname, num);
    name = (char *) malloc(strlen(actname)+1);
    strcpy(name, actname);
    rthread_set_name(act, name);
    
    /*** insert test later -- rkc, 9/21/94 ***/

    kr = netname_register_send_right(name_server_port,
				     actname,
				     MACH_PORT_NULL,
				     rpctest_activation);

    if (kr != NETNAME_SUCCESS)
    {
	mach_error("rpctest activation check in:", kr);
	exit(-1);
    }
    return name;
}

void
new_activation()
{
    rthread_t act;
    char *name;

    act = rthread_activation_create(rpc_port);
    rpctest_activation = rthread_kernel_port(act);
    name = set_activation_name(act, taskname,  numacts);
    rprintf("%s: new activation %s created.\n", taskname, name);
    numacts++;
}

boolean_t
is_from_remote(mach_port_t port)
{
    return (port != MACH_PORT_NULL);
}

boolean_t
known_to_server()
{
    return (mach_thread_self() == main_thread);
}


/*
 *	Initialization routine for RPC server side. 
 */

void
init_exceptions()
{
    void task_exc_handler(char *dummy);
    kern_return_t kr;

    /*
     *	Allocate port for this task's exception handler.
     */
    kr = mach_port_allocate(mach_task_self(),
        			MACH_PORT_RIGHT_RECEIVE, 
				&task_exc_port);
    if (kr != KERN_SUCCESS) 
    {
	mach_error("init_exceptions - port allocate:", kr);
	exit(-1);
    }

    /*
     *	Get send rights for the port.
     */
    kr = mach_port_insert_right(mach_task_self(),
				    task_exc_port,
				    task_exc_port,
				    MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS)
    {
	mach_error("rpc server: mach_port_insert_right() failed",
		   kr);
	exit(1);
    }

    /*
     *	Create thread to wait for exceptions.
     */
    exc_hdlr_thread = rthread_spawn((rthread_fn_t) task_exc_handler, (void *) 0);

    /*
     *	(Optionally) install exception handler.
     */
    kr = install_exception_handler_internal(exception_handler_default);
    if (kr != KERN_SUCCESS)
    {
	mach_error("rpc server: install_exception_handler_internal() failed",
		   kr);
	exit(1);
    }
}

/*
 *	Initialization routine for RPC test. 
 */

void
init()
{
    kern_return_t	kr;
    char name[128];


    init_rthread_printf();
    stack_size = vm_page_size * 8;

    /*
     *	Register our server subsystem with the kernel for upcalls.
     */
    kr = mach_subsystem_create(mach_task_self(), 
			       (user_subsystem_t) &rpctest_subsystem,
			       sizeof(rpctest_subsystem),
			       &rpc_subsystem_port);
	
    if (kr != KERN_SUCCESS)
    {
	mach_error("rpctest subsystem create:", kr);
	exit(-1);
    }

    debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	("  %s: rpctest: subsystem port 0x%x\n", taskname, rpc_subsystem_port);

    /*
     *	Allocate port for this object.
     */
    kr = mig_mach_port_allocate_subsystem(mach_task_self(), 
					  rpc_subsystem_port,
					  &rpc_port);

    if (kr != KERN_SUCCESS)
    {
	mach_error("rpctest port alloc subsys:", kr);
	exit(-1);
    }

    if (name_debug == 1)
	netname_debug_on(name_server_port);
    if (name_debug == 2)
	netname_debug_off(name_server_port);

    new_activation();

    /*
     *      Now check it into the name service.  No signature port
     *      means this is unprotected.
     */

    kr = netname_check_in(name_server_port, taskname, MACH_PORT_NULL, rpc_port);

    if (kr != NETNAME_SUCCESS)
    {
	mach_error("rpctest check in rpc_port:", kr);
	exit(-1);
    }

    debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	("  %s: rpctest: rpc_port %x\n", taskname, rpc_port);

    sprintf(name, "%s_0", taskname);
    kr = netname_register_send_right(name_server_port,
				     name,
				     MACH_PORT_NULL,
				     mach_thread_self()
				     );

    if (kr != NETNAME_SUCCESS)
    {
	mach_error("rpctest check in main_thread:", kr);
	exit(-1);
    }

    rthread_set_name(rthread_self(), name);
    debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	("  %s: rpctest rpc_port 0x%x checked in\n", taskname, rpc_port);

    sprintf(name, "t_%s", taskname);
    kr = netname_register_send_right(name_server_port,
				     name,
				     MACH_PORT_NULL,
				     mach_task_self()
				     );

    if (kr != NETNAME_SUCCESS)
    {
	mach_error("rpctest check in task:", kr);
	exit(-1);
    }

    init_exceptions();
}


/*
 *	install_exception_handler
 */

kern_return_t
install_exception_handler(mach_port_t object, boolean_t install_handler)
{
        kern_return_t           kr = KERN_SUCCESS;

	if (debug >= DEBUG_LEVEL_TRACE)
	    printf("%s: rpctest: install_exception_handler(%x)\n",
		   taskname, install_handler);

	/* call common subroutine */
	kr = install_exception_handler_internal(install_handler);
	if (kr != KERN_SUCCESS) {
	  mach_error("rpc server: install_exception_handler_internal() failed",
		     kr);
	  exit(1);
	}

finish:
        __asm__ volatile ("movl %0, %%ebx" : : "g" (kr) );

        (void) mach_rpc_return_trap();
        /* NOTREACHED */

	return kr;
}	


void
task_exc_handler(char *dummy)
{
	kern_return_t	kr;

	while (1) {
		kr = mach_msg_server(exc_server,
				     RPC_MAX_MSG_SIZE /***???***/,
				     task_exc_port, 
				     MACH_MSG_OPTION_NONE);
	}
}

kern_return_t
catch_exception_raise(
		      mach_port_t	exc_port,
		      mach_port_t	thd,
		      mach_port_t	tsk,
		      exception_type_t	exception_typ,
		      exception_data_t	code,
		      mach_msg_type_number_t	subcode
		      )
{
	rprintf("%s: catch_exception_raise: task exception handler executed\n",
	       taskname);

	return orphan_result;
}

kern_return_t
catch_exception_raise_state(
			    mach_port_t		exc_port,
			    exception_type_t	exception_typ,
			    exception_data_t	code,
			    mach_msg_type_number_t	subcode,
			    int				*flavor,
			    thread_state_t		old_state,
			    mach_msg_type_number_t	old_stateCnt,
			    thread_state_t		new_state,
			    mach_msg_type_number_t	*new_stateCnt
			    )
{
	rprintf("%s: UNEXPECTED: catch_exception_raise_state called\n",
	    taskname);

	return orphan_result;
}

kern_return_t
catch_exception_raise_state_identity(
				     mach_port_t	exc_port,
				     mach_port_t	thd,
				     mach_port_t	tsk,
				     exception_type_t	exception_typ,
				     exception_data_t	code,
				     mach_msg_type_number_t	subcode,
				     int			*flavor,
				     thread_state_t		old_state,
				     mach_msg_type_number_t	old_stateCnt,
				     thread_state_t		new_state,
				     mach_msg_type_number_t	*new_stateCnt
				     )
{
    rprintf("%s: UNEXPECTED: catch_exception_raise_state_identity called\n",
		taskname);
	return orphan_result;
}


/*
 *	install_exception_handler_internal
 */

kern_return_t
install_exception_handler_internal(boolean_t install_handler)
{
        kern_return_t           kr = KERN_SUCCESS;

	/*** should make this MP-safe someday ***/

	/* look at possible cases */
	switch(install_handler) {
		case TRUE: { /* wish to have exception handler installed */
		  /* do not install if a handler is already installed */
		  if (exception_handler_installed == FALSE) {
		    /* notify kernel to use exception port for alerts */
		    kr = task_set_exception_ports(mach_task_self(),
						  EXC_MASK_RPC_ALERT,
						  task_exc_port,
						  EXCEPTION_DEFAULT,
						  THREAD_STATE_NONE /*** temp ***/);
		    if (kr != KERN_SUCCESS) {
		      mach_error("rpc server: task_set_exception_ports() failed",
				 kr);
		      exit(1);
		    }

		    /* indicate handler is now installed */
		    exception_handler_installed = TRUE;
		  }

		  break;
		}

		case FALSE: { /* do not want handler installed */
		  /* action is only required if handler is already installed */
		  if (exception_handler_installed == TRUE) {
		    /* notify kernel not to use exception port for alerts */
		    kr = task_set_exception_ports(mach_task_self(),
						  EXC_MASK_RPC_ALERT,
						  MACH_PORT_NULL,
						  EXCEPTION_DEFAULT,
						  THREAD_STATE_NONE /*** temp ***/);
		    if (kr != KERN_SUCCESS) {
		      mach_error("rpc server: task_set_exception_ports() failed",
				 kr);
		      exit(1);
		    }

		    /* indicate handler is no longer installed */
		    exception_handler_installed = FALSE;
		  }

		  break;
		}

		default: { /* invalid argument */
		  kr = KERN_INVALID_ARGUMENT;
		  break;
		}
	}

	return kr;
}	

static function_ptr_t
find_function(function_table_entry *function_table, char *name)
{
    char *function_name;
    int index;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: find_function(function_table @ 0x%x, name = \"%s\")\n",
	taskname, function_table, name);

    for (index = 0, function_name = function_table[index].name;
	 function_name;
	 index++, function_name = function_table[index].name)
    {
	if (strcmp(function_name, name) == 0)
	    return function_table[index].function;
    }
    return (function_ptr_t) 0;
}


void
process_command(mach_port_t target,
		function_table_entry *function_table,
		char *commands)
{
    char command[MAX_ARG_SIZE];
    char arg[MAX_ARG_SIZE];
    function_ptr_t function;
    kern_return_t kr = KERN_FAILURE;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: process_command(target = %x, function_table @ 0x%x, commands = \"%s\")\n",
	 taskname, target, function_table, commands);

    sscanf(commands, " %s", command);

    debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	("  %s: process_command - command = \"%s\"\n", taskname, command);

    function = find_function(function_table, &command[0]);

    debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	("  %s: process_command - function = 0x%x\n", taskname, function);
    
    debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	("  %s: process_command - next commands = \"%s\"\n",
	 taskname, strstr(commands, command)+strlen(command));
    commands = strstr(commands, command)+strlen(command);
    if (function)
	(*function)(target, commands, strlen(commands) + 1);
}


/*
 *	Main loop
 */

main(int argc, char **argv)
{
    kern_return_t kr;

    /* note name of this thread; other threads cannot access UNIX services */
    main_thread = mach_thread_self();

    taskname = argc > 1 ? argv[1] : argv[0];

    if (argc == 1)
    {
	rprintf("Please specify a task name as the first argument.\n");
	exit(1);
    }

    argc -= 2;
    argv += 2;

    while (argc > 1)
    {
	if (argv[0][0] == '-')
	{
	    switch (argv[0][1])
	    {
	    case 'v':
		argc--;
		argv++;
		if (argc)
		{
		    debug = atoi(argv[0]);
		    argc--;
		    argv++;
		}
		break;
	    case 'e':
		argc--;
		argv++;
		if (argc)
		{
		    exception_handler_default = atoi(argv[0]);
		    argc--;
		    argv++;
		}
		break;
	    case 'n':
		argc--;
		argv++;
		if (argc)
		{
		    name_debug = atoi(argv[0]);
		    argc--;
		    argv++;
		}
		break;
	    case 'o':
		argc--;
		argv++;
		orphan_result = KERN_RPC_CONTINUE_ORPHAN;
		break;
	    default:
		argc--;
		argv++;
		break;
	    }
	    continue;
	}
	argc--;
	argv++;
    }

    init();

    if (debug > ((sizeof(debug_level)/sizeof(char *)) - 1))
	debug = (sizeof(debug_level)/sizeof(char *)) - 1;

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: debug level: %s\n", taskname, debug_level[debug]);

    if (argc)
    {
	debug_printf(DEBUG_LEVEL_TRACE)
	    ("%s: Commands: %s\n",taskname, argv[0]);
	process_command(rpc_port, rpctest_client_functions, argv[0]);
    }
    else
	debug_printf(DEBUG_LEVEL_TRACE)
	    ("%s: No command.\n", taskname);

    /* execute print loop -- i.e., print strings sent by other threads */
    while (! quit_rpctest)
    {
	process_printf();
    }
}

kern_return_t
client_call(mach_port_t target, char *args, int length)
{
    kern_return_t kr;
    char str[80];

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: client_call(target = %x, args = \"%s\")\n", taskname, target, args);

    if (target == rpc_port)
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling call(%x, \"%s\")\n", taskname, target, args);
	kr = call(MACH_PORT_NULL, args, length);
	sprintf(str, "%s: call return", taskname);
    }
    else
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling remote_call(%x, \"%s\")\n", taskname, target, args);

	kr = remote_call(target, args, length);
	sprintf(str, "%s: remote_call return", taskname);
    }
    mach_error(str, kr);
    return KERN_SUCCESS;
}

kern_return_t
client_spin(mach_port_t target, char *args, int length)
{
    kern_return_t kr;
    char str[80];

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: client_spin(target = %x, args = \"%s\")\n", taskname, target, args);

    if (target == rpc_port)
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling spin(%x, \"%s\")\n", taskname, target, args);
	kr = spin(MACH_PORT_NULL, args, length);
	sprintf(str, "%s: spin return", taskname);
    }
    else
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling remote_spin(%x, \"%s\")\n", taskname, target, args);

	kr = remote_spin(target, args, length);
	sprintf(str, "%s: remote_spin return", taskname);
    }
    mach_error(str, kr);
    return KERN_SUCCESS;
}

kern_return_t
client_kill(mach_port_t target, char *args, int length)
{
    kern_return_t kr;
    char str[80];

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: client_kill(target = %x, args = \"%s\")\n", taskname, target, args);

    if (target == rpc_port)
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling kill(%x, \"%s\")\n", taskname, target, args);
	kr = kill(MACH_PORT_NULL, args, length);
	sprintf(str, "%s: kill return", taskname);
    }
    else
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling remote_kill(%x, \"%s\")\n", taskname, target, args);

	kr = remote_kill(target, args, length);
	sprintf(str, "%s: remote_kill return", taskname);
    }
    mach_error(str, kr);
    return KERN_SUCCESS;
}

kern_return_t
client_term(mach_port_t target, char *args, int length)
{
    kern_return_t kr;
    char str[80];

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: client_term(target = %x, args = \"%s\")\n", taskname, target, args);

    if (target == rpc_port)
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling term(%x, \"%s\")\n", taskname, target, args);
	kr = term(MACH_PORT_NULL, args, length);
	sprintf(str, "%s: term return", taskname);
    }
    else
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling remote_term(%x, \"%s\")\n", taskname, target, args);

	kr = remote_term(target, args, length);
	sprintf(str, "%s: remote_term return", taskname);
    }
    mach_error(str, kr);
    return KERN_SUCCESS;
}

kern_return_t
client_nop(mach_port_t target, char *args, int length)
{
    kern_return_t kr;
    char str[80];

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: client_nop(target = %x, args = \"%s\")\n", taskname, target, args);

    if (target == rpc_port)
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling nop(%x, \"%s\")\n", taskname, target, args);
	kr = nop(MACH_PORT_NULL, args, length);
	sprintf(str, "%s: nop return", taskname);
    }
    else
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling remote_nop(%x, \"%s\")\n", taskname, target, args);

	kr = remote_nop(target, args, length);
	sprintf(str, "%s: remote_nop return", taskname);
    }
    mach_error(str, kr);
    return KERN_SUCCESS;
}

kern_return_t
client_quit(mach_port_t target, char *args, int length)
{
    kern_return_t kr;
    char str[80];

    debug_printf(DEBUG_LEVEL_TRACE)
	("%s: client_nop(target = %x, args = \"%s\")\n", taskname, target, args);

    if (target == rpc_port)
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling quit(%x, \"%s\")\n", taskname, target, args);
	kr = quit(MACH_PORT_NULL, args, length);
	sprintf(str, "%s: nop return", taskname);
    }
    else
    {
	debug_printf(DEBUG_LEVEL_DETAILED_INFO)
	    ("  %s: calling remote_quit(%x, \"%s\")\n", taskname, target, args);

	kr = remote_quit(target, args, length);
	sprintf(str, "%s: remote_quit return", taskname);
    }
    mach_error(str, kr);
    return KERN_SUCCESS;
}

#ifdef DEBUG
void
display_port_rights(mach_port_t port)
{
    kern_return_t kr;
    mach_port_type_t port_type;

    kr = mach_port_type(mach_task_self(), rpctest_activation, &port_type);
    if (kr == KERN_SUCCESS) {
	printf("\nrights for rpctest_activation:\n");
	if (port_type & MACH_PORT_TYPE_SEND) printf("  MACH_PORT_TYPE_SEND\n");
	if (port_type & MACH_PORT_TYPE_RECEIVE)
	    printf("  MACH_PORT_TYPE_RECEIVE\n");
	if (port_type & MACH_PORT_TYPE_SEND_ONCE)
	    printf("  MACH_PORT_TYPE_SEND_ONCE\n");
	if (port_type & MACH_PORT_TYPE_PORT_SET)
	    printf("  MACH_PORT_TYPE_PORT_SET\n");
	if (port_type & MACH_PORT_TYPE_DEAD_NAME)
	    printf("  MACH_PORT_TYPE_DEAD_NAME\n");
	if (port_type & MACH_PORT_TYPE_DNREQUEST)
	    printf("  MACH_PORT_TYPE_DNREQUEST\n");
    }
    else {
	mach_error("rpctest: mach_port_type() failed", kr);
	exit(1);
    }

    return;
}
#endif /*DEBUG*/
