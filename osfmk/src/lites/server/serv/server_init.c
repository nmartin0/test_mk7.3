/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1994 Johannes Helander
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND JOHANNES HELANDER ALLOW FREE USE OF THIS
 * SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND JOHANNES
 * HELANDER DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */
/*
 * HISTORY
 * $Log: server_init.c,v $
 * Revision 1.4  1995/08/15  06:49:37  sclawson
 * modifications from lites-1.1-950808
 *
 * Revision 1.3  1995/03/23  01:44:06  law
 * Update to 950323 snapshot + utah changes
 *
 *
 */
/* 
 *	File: 	serv/server_init.c
 *	Authors:
 *	Randall Dean, Carnegie Mellon University, 1992.
 *	Johannes Helander, Helsinki University of Technology, 1994.
 *
 * 	Server startup.
 */

#include "osfmach3.h"
#include "second_server.h"
#include "map_uarea.h"
#include "syscalltrace.h"
#include "machid_register.h"
#include "vnpager.h"
#include "sysvshm.h"
#include "sl.h"

#include <serv/server_defs.h>
#include <sys/reboot.h>
#include <sys/fcntl.h>
#include <sys/resource.h>
#include <sys/msgbuf.h>
#include <sys/user.h>

#include <serv/syscalltrace.h>

extern struct	session session0;
extern struct	pgrp pgrp0;
extern struct	proc proc0;
extern struct	pcred cred0;
extern struct	filedesc0 filedesc0;
extern struct	plimit limit0;
extern struct	vmspace vmspace0;
extern struct	proc *initproc, *pageproc;
extern struct	vnode *rootvp, *swapdev_vp;
extern int	boothowto;
extern struct	timeval boottime;
extern struct	timeval runtime;
extern char sigcode;
extern int szsigcode;
extern int msgbufmapped;
extern struct proc *newproc();
extern void ux_create_thread();
extern void proc_set_condition_names(struct proc *p); /* in kern_fork.c */
cthread_fn_t	system_setup();	/* forward */

struct mutex allproc_lock = MUTEX_NAMED_INITIALIZER("allproc_lock");
int system_procs = 2; /*XXX governed by allproc_lock also.  Start at -3
		        XXX because of task_py_pid garbage*/

dev_t rootdev;
char *trampoline_page;
long		avenrun[3] = {0, 0, 0};	/* XXX */
mach_port_t	privileged_host_port;
mach_port_t	host_port;
mach_port_t	device_server_port;
mach_port_t	default_processor_set;
mach_port_t	default_processor_set_name;

#if	MAP_UAREA
mach_port_t	default_pager_port = MACH_PORT_NULL;
mach_port_t	shared_memory_port = MACH_PORT_NULL;
vm_offset_t	shared_offset = 0;
#endif MAP_UAREA
struct condition kill_main = CONDITION_NAMED_INITIALIZER("kill_main");
struct mutex kill_lock = MUTEX_NAMED_INITIALIZER("kill_lock");
int debugger_thread = 0;

#if OSFMACH3
security_token_t security_id;
mach_port_t security_port = MACH_PORT_NULL;
mach_port_t	root_ledger_wired;
mach_port_t	root_ledger_paged;
#endif

vm_size_t	memory_size;
int	lbolt;
struct user proc0user;
struct user *proc0paddr = &proc0user;
struct loadavg averunnable;

#ifdef USEACTIVATIONS
boolean_t use_activations = TRUE;
#endif

#if MACHID_REGISTER && SECOND_SERVER
mach_port_t mid_server = MACH_PORT_NULL;
extern int pid_registration_type; /* in kern_exec.c XXX tri */
int use_fake_server = 0;
#endif /* MACHID_REGISTER && SECOND_SERVER */

#if	MAP_UAREA
void alloc_mapped_uarea(struct proc *p)
{
	kern_return_t kr;
	vm_address_t shared_address = 0;

	kr = vm_map(mach_task_self(),
		    &shared_address, 4*vm_page_size,
		    0, TRUE, shared_memory_port, shared_offset, FALSE,
		    VM_PROT_READ|VM_PROT_WRITE, VM_PROT_ALL,
		    VM_INHERIT_NONE);
	if (kr != KERN_SUCCESS)
	    panic("alloc_mapped_urea: vm_map shared mem sz=x%x off=x%x %s\n",
		  4*vm_page_size, shared_offset, mach_error_string(kr));

	p->p_shared_off = shared_offset;
	p->p_readwrite = (char *)shared_address;
	p->p_shared_rw = (struct ushared_rw *)
	    (shared_address + 2*vm_page_size);
	p->p_shared_ro = (struct ushared_ro *)
	    (shared_address + 3*vm_page_size);
	shared_offset += 4*vm_page_size;
}

/* Called by proc_died to clean up the server context */
void dealloc_mapped_uarea(struct proc *p)
{
	kern_return_t kr;
	vm_address_t addr = (vm_address_t) p->p_readwrite;

	p->p_readwrite = 0;
	p->p_shared_rw = 0;
	p->p_shared_ro = 0;

	kr = vm_deallocate(mach_task_self(), addr, 4*vm_page_size);
	assert(kr == KERN_SUCCESS);
}
#endif

extern dev_t	parse_root_device(const char *);
extern char **init_program_args;
#if	SECOND_SERVER
boolean_t  second_server = FALSE;	/* Run under another server ? */
/* 
 *  If flag for second server, we return non-zero.
 *  '2' returns 1,  '3' returns 2,  '4' returns 3 etc.
 */
static inline int IS_FLAG_FOR_SECOND_SERVER(int c)
{
    return((c >= '2' && c <= '9') ? ((c + 1) - '1') : 0);
}
#endif	/* SECOND_SERVER */

#if OSFMACH3
/*
 * AI-ONLY NOTE: under OSFMACH3 the first program is /sbin/init, not
 * <server directory>/mach_init: "The OSF/1 server runs /sbin/init.
 * There is no need for mach_init as the service server is already
 * running in the bootstrap task" (OSF RI, server co-location paper,
 * 1993, section 5.1); and the emulator is /mach_servers/emulator on
 * the root until Tier 1 removes it (docs/deferred.md, O-2, O-3).
 * There is no server directory:
 * the OSF/1 server takes flags and a root, nothing more (bootstrap/
 * bootstrap.template).  Utah's -i and -e still name others, now by
 * path; init_program_name is init's argv[0], the path, as 4.4BSD's
 * start_init() passes it.
 */
char init_program_path[128] = "/sbin/init";
char init_program_name[128] = "/sbin/init";
char emulator_path[128] = "/mach_servers/emulator";
char emulator_old_path[128] = "/mach_servers/emulator.old";
#else	/* OSFMACH3 */
char init_program_path[128] = "/mach_servers/mach_init";
char init_program_name[] = "/mach_init";
char emulator_path[128] = "/mach_servers/emulator";
char emulator_old_path[128] = "/mach_servers/emulator.old";
char emulator_name[]    = "/emulator";
char dev_prefix[] = "/dev/";
char server_dir[128];
#endif	/* OSFMACH3 */


/* decide whether we are a second server or not */
void init_second_server_flag(int argc, char **argv)
{
#if	SECOND_SERVER
	if (argc <= 0)
	    return;

	if (argc > 1 && argv[1][0] == '-' && 
	    IS_FLAG_FOR_SECOND_SERVER(argv[1][1])) {

		second_server = TRUE; /* run as a unix process of another server */
	}
#endif	/* SECOND_SERVER */
}

/* setup all the magic ports */
void init_ports()
{
	mach_port_t	bootstrap_port = MACH_PORT_NULL;
	kern_return_t	kr;

	/* first bootstrap port */
#if OSFMACH3
	mach_msg_type_number_t security_token_count;

	kr = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (kr != KERN_SUCCESS)
	    panic("get bootstrap port %d", kr);

	kr = bootstrap_ports(bootstrap_port,
			     &privileged_host_port,
			     &device_server_port,
#if OSF_LEDGERS
			     &root_ledger_wired,
			     &root_ledger_paged,
#endif /* OSF_LEDGERS */
			     &security_port);

	assert(kr == KERN_SUCCESS);

	security_token_count = TASK_SECURITY_TOKEN_COUNT;
	kr = task_info(mach_task_self(),
		       TASK_SECURITY_TOKEN,
		       (task_info_t) &security_id,
		       &security_token_count);
	assert(kr == KERN_SUCCESS);

#else /* OSFMACH3 */
	mach_port_t	reply_port; /* should deallocate the port too. XXX */
	struct imsg {
	    mach_msg_header_t	hdr;
	    mach_msg_type_t	port_desc_1;
	    mach_port_t		port_1;
	    mach_msg_type_t	port_desc_2;
	    mach_port_t		port_2;
	} imsg;
	extern mach_port_t second_task_by_pid(int n);

#if SECOND_SERVER
	if (second_server) {
		privileged_host_port = second_task_by_pid(-1);
		device_server_port = second_task_by_pid(-2);
		if (privileged_host_port == MACH_PORT_NULL)
			panic("failed to get privileged host port");	
		if (device_server_port == MACH_PORT_NULL)
			panic("failed to get device server port");
	} else {
#endif /* SECOND_SERVER */
		kr = task_get_bootstrap_port(mach_task_self(),
						 &bootstrap_port);
		if (kr != KERN_SUCCESS)
		    panic("get bootstrap port %d", kr);

		/*
		 * Allocate a reply port
		 */
		reply_port = mach_reply_port();
		if (reply_port == MACH_PORT_NULL)
		    panic("allocate reply port");

		/*
		 * Send a message to it, asking for the host and device ports
		 * XXX should be MiG interface.
		 */
		imsg.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
						    MACH_MSG_TYPE_MAKE_SEND_ONCE);
		imsg.hdr.msgh_size = 0;
		imsg.hdr.msgh_remote_port = bootstrap_port;
		imsg.hdr.msgh_local_port = reply_port;
		imsg.hdr.msgh_id = 999999;

		kr = mach_msg(&imsg.hdr, MACH_SEND_MSG|MACH_RCV_MSG,
			      sizeof imsg.hdr, sizeof imsg, reply_port,
			      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != MACH_MSG_SUCCESS)
		    panic("mach_msg: get privileged host and device ports: %s",
			  mach_error_string(kr));

		privileged_host_port = imsg.port_1;
		device_server_port = imsg.port_2;
#if SECOND_SERVER
	}
#endif /* SECOND_SERVER */
#endif /* OSFMACH3 */

	host_port = mach_host_self();

	/*
	 * Lookup our default processor-set name/control ports.
	 */

	(void) processor_set_default(mach_host_self(),
				     &default_processor_set_name);
	(void) host_processor_set_priv(privileged_host_port,
				       default_processor_set_name,
				       &default_processor_set);
}

#if OSFMACH3
/*
 * AI-ONLY NOTE: written for OSFMACH3, in place of Utah's
 * parse_arguments() below, which stays for the other kernels.  The
 * OSF/1 server's arguments are flags and a root name: "-s
 * (single-user), -a (prompt for root device), and a root file system
 * name" (bootstrap/bootstrap.template).  get_config_info() passes
 * this function the kernel's boot information first, as O-7 and O-4
 * already did, and bootstrap.conf's arguments after: flags
 * accumulate, and a root named on the startup line replaces the boot
 * device, which MK7.3 Release Notes, bug 2271, make the default root.
 * Nothing public gives the server any other word, so words after the
 * root are ignored, with a message.  No server directory:
 * docs/deferred.md, O-1, O-2, O-4, O-5.
 */
static char *root_name;			/* the root so far, or 0 */
static char root_name_buf[50];		/* -a's answer */
static char default_root[] = "hd0a";	/* Utah's, the last resort */

static void copy_path(
	char		*to,
	const char	*from,
	const char	*suffix)
{
	if (strlen(from) + strlen(suffix) >= 128)
		panic("(lites) path too long: %s", from);
	strcpy(to, from);
	strcat(to, suffix);
}

void parse_arguments(
	int	argc,
	char	**argv)
{
	/*
	 * Arg 0 is program name
	 */
	argv++, argc--;

	while (argc > 0 && argv[0][0] == '-') {
	    register char *cp = argv[0];
	    register char c;

	    while ((c = *cp++) != '\0') {
		switch (c) {
		    case 'q': /* this is what kernel (and bootstrap) passes */
		    case 'a':
			boothowto |= RB_ASKNAME;	/* asked below */
			break;
		    case 's':
			boothowto |= RB_SINGLE;
			break;
		    case 'd':
			boothowto |= RB_KDB;
			break;
		    case 'n':
			boothowto |= RB_INITNAME;
			break;
#if	SECOND_SERVER
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
			if (second_server == FALSE)
			    panic("error: '-%c' must be the first argument.", c);
#if MACHID_REGISTER
			pid_registration_type = IS_FLAG_FOR_SECOND_SERVER(c);
#endif /* MACHID_REGISTER */
			break;
		    case 'h':
			if (second_server) {
			    boothowto |= RB_KDB;
			    printf("Suspended and ready to continue.\n");
			    task_suspend(mach_task_self());
			}
			break;
		    case 'e':
			/* Allow non-default emulator file name: */
			if (argc > 1) {
			    copy_path(emulator_path, argv[1], "");
			    copy_path(emulator_old_path, argv[1], ".old");
			    argv++, argc--;
			}
			break;
		    case 'i':
			/* Allow non-default init program file name: */
			if (argc > 1) {
			    copy_path(init_program_path, argv[1], "");
			    copy_path(init_program_name, argv[1], "");
			    argv++, argc--;
			}
			break;
#endif	/* SECOND_SERVER */
		    case 'r':
			/* set the root partition */
			if (argc > 1) {
			    root_name = argv[1];
			    argv++, argc--;
			}
			break;
#if SYSCALLTRACE
		    case 'v':
			/* Turn on syscall tracing: */
			syscalltrace = -1;
			break;
#endif /* SYSCALLTRACE */
#if MACHID_REGISTER && SECOND_SERVER
		    case 'f':
			{
			    mach_port_t	new_dev_server_port;
			    use_fake_server = 1;
			    if(second_server) {
			        mach_error_t kr;
				kr = netname_look_up(name_server_port,
							 "",
							 "FakeDeviceServer",
							 &new_dev_server_port);
				if (kr != KERN_SUCCESS)
				       panic("can't find fake device server.");
			    }
			    (void)mach_port_deallocate(mach_task_self(),
						       device_server_port);
			    device_server_port = new_dev_server_port;
			}
			break;
#endif /* MACHID_REGISTER && SECOND_SERVER */
#ifdef USEACTIVATIONS
		    case 'A':
			use_activations = TRUE;
			break;
#endif
		    default:
			break;
		}
	    }
	    argv++, argc--;
	}

	/*
	 * The first word that is not a flag is the root.
	 */
	if (argc > 0) {
		root_name = argv[0];
		argv++, argc--;
	}

	/*
	 * Nothing after the root is the server's.
	 */
	for (; argc > 0; argv++, argc--)
		dprintf("(lites) startup argument ignored: %s\r\n", argv[0]);
}

/*
 * AI-ONLY NOTE: added.  After both parses: -a asks for the root,
 * with OSF's prompt, "root device: [fd0b]" (MK7.3 Release Notes,
 * 2.4.2.1), the root so far as its default; then, with no root at
 * all, Utah's default_root, as Lites has always had it.
 * docs/deferred.md, O-4, O-5.
 */
static void set_root(void)
{
	if (boothowto & RB_ASKNAME) {
		if (root_name)
			dprintf("root device: [%s] ", root_name);
		else
			dprintf("root device: ");
		dget_string((char *)0, root_name_buf, sizeof(root_name_buf));
		if (root_name_buf[0])
			root_name = root_name_buf;
	}
	if (root_name == (char *)0)
		root_name = default_root;

	dprintf("(lites): root(%s)\r\n", root_name);
	dprintf("(lites): emulator_path(%s)\r\n", emulator_path);
	dprintf("(lites): init_program(%s)\r\n", init_program_path);

	rootdev = parse_root_device(root_name);
}

#else	/* OSFMACH3 */
char default_root[] = "hd0a"; /* XXX */

void parse_arguments(
	int	argc,
	char	**argv)
{
	extern char emulator_path[];
	extern char emulator_name[];
	extern char init_program_path[];
	char		*pname;
	char   rootdev_name[50];
	char   *rootname = (char *)NULL;


	/*
	 * Parse the arguments.
	 */

	/*
	 * Arg 0 is program name
	 */
	pname = argv[0];
	argv++, argc--;

	/*
	 * Arg 1 should be flags
	 */
	if (argc == 0)
	    return;

	while (argv[0][0] == '-') {
	    register char *cp = argv[0];
	    register char c;

	    while ((c = *cp++) != '\0') {
		switch (c) {
		    case 'q': /* this is what kernel (and bootstrap) passes */
		    case 'a':
			boothowto |= RB_ASKNAME;
			dget_string("[lites root device]? ", rootdev_name, 50);
			if (rootdev_name[0])
			  rootname = rootdev_name;
			break;
		    case 's':
			boothowto |= RB_SINGLE;
			break;
		    case 'd':
			boothowto |= RB_KDB;
			break;
		    case 'n':
			boothowto |= RB_INITNAME;
			break;
#if	SECOND_SERVER
		    case '2':	 
		    case '3':	 
		    case '4':	 
		    case '5':	 
		    case '6':	 
		    case '7':	 
		    case '8':	 
		    case '9':	 
			if (second_server == FALSE)
			    panic("error: '-%c' must be the first argument.", c);
#if MACHID_REGISTER
			pid_registration_type = IS_FLAG_FOR_SECOND_SERVER(c);
#endif /* MACHID_REGISTER */
			break;
		    case 'h':
			if (second_server) {
			    boothowto |= RB_KDB;
			    printf("Suspended and ready to continue.\n");
			    task_suspend(mach_task_self());
			}
			break;
		    case 'e':
			/* Allow non-default emulator file name: */
			strcpy(emulator_name, argv[1]);
			/* Skip over the file name argument: */
			argv++, argc--;
			break;
		    case 'i':
			/* Allow non-default init program file name: */
			strcpy(init_program_name, argv[1]);
			/* Skip over the file name argument: */
			argv++, argc--;
			break;
#endif	/* SECOND_SERVER */
		    case 'r':
			/* set the root partition */
			strcpy(rootdev_name, argv[1]);
			rootname = rootdev_name;
			/* Skip over the root partition name: */
			argv++, argc--;
			break;
#if SYSCALLTRACE
		    case 'v':
			/* Turn on syscall tracing: */
			syscalltrace = -1;
			break;
#endif /* SYSCALLTRACE */
#if MACHID_REGISTER && SECOND_SERVER
		    case 'f':
			{
			    mach_port_t	new_dev_server_port;
			    use_fake_server = 1;
			    if(second_server) {
			        mach_error_t kr;
				kr = netname_look_up(name_server_port, 
							 "",
							 "FakeDeviceServer", 
							 &new_dev_server_port);
				if (kr != KERN_SUCCESS)
				       panic("can't find fake device server.");
			    }
			    (void)mach_port_deallocate(mach_task_self(),
						       device_server_port);
			    device_server_port = new_dev_server_port;
			}
			break;
#endif /* MACHID_REGISTER && SECOND_SERVER */
#ifdef USEACTIVATIONS
		    case 'A':
			use_activations = TRUE;
			break;
#endif
		    default:
			break;
		}
	    }
	    argv++, argc--;
	}
	/* 
	 * Arg 2 (now 0) should be root name
	 * Arg 3 (now 1) should be server_dir_name (3.0 style)
	 */

	if (! rootname)
	  rootname = argc ? argv[0] : default_root;

	if (argc < 2) {
		strcpy(server_dir, dev_prefix);
		strcat(server_dir, rootname);
		if (*pname == '/') {
			char *cp = pname + strlen(pname);
			while (*--cp != '/') ;
			*cp = 0;
			strcat(server_dir, pname);
			*cp = '/';
		} else {
			strcat(server_dir, "/mach_servers");	/* XXX */
		}
		dprintf("(lites): WARNING! no server directory specified.\r\n");
		dprintf("(lites): path(%s) derived from root\r\n", server_dir);
	} else
		strcpy(server_dir, argv[1]);

	if (bcmp(server_dir, dev_prefix, strlen(dev_prefix))) {
		dprintf("(lites): server_dir(%s) ignored.  It does not start with %s\r\n",
			server_dir, dev_prefix);
		dprintf("(lites): TILT TILT!\r\n");
	} else {
		int len = strlen(rootname);

		if (bcmp(server_dir+5, rootname, len)) {
			char *cp = server_dir+5;
			while (*cp++ != '/') ;
			len = cp - (server_dir + 5) - 1;

			dprintf("(lites): WARNING! specified server_dir(%s) NOT on root(%s)!\r\n",
				server_dir, rootname);
			dprintf("(lites): %s and %s will be taken from %s on the localroot(%s)!\r\n",
				init_program_name, emulator_name, server_dir+5+len, rootname);
		} else { 
			dprintf("(lites): server_dir(%s) on root.\r\n",
				server_dir+5+len);
		}
		strcpy(emulator_path, server_dir+5+len);
		strcat(emulator_path, emulator_name);
		strcpy(emulator_old_path, emulator_path);
		strcat(emulator_old_path, ".old");
		strcpy(init_program_path, server_dir+5+len);
		strcat(init_program_path, init_program_name);
	}
	dprintf("(lites): emulator_path(%s)\r\n", emulator_path);
	dprintf("(lites): init_program(%s)\r\n", init_program_path);

	rootdev = parse_root_device(rootname);
}
#endif	/* OSFMACH3 */

void msgbuf_init()
{
    kern_return_t kr;

    kr = vm_allocate(mach_task_self(), (vm_address_t *) &msgbufp, 
			sizeof(struct msgbuf),
		     TRUE);
    if (kr != KERN_SUCCESS)
	panic("allocating msgbuf",kr);
    msgbufmapped = 1;
}

void system_proc(
    struct proc **np,
    char *name)
{
	struct proc **hash;
	struct proc *p;
	proc_invocation_t pk = get_proc_invocation();

	proc_allocate(np);
	p = *np;
	pk->k_p = p;
	queue_init(&p->p_servers);
	queue_enter(&p->p_servers, pk, proc_invocation_t, k_servers_chain);

	condition_init(&p->p_condition);
	mutex_init(&p->p_lock);
	p->p_task = mach_task_self();
	p->p_thread = mach_thread_self();

	cthread_set_name(cthread_self(), name);

	strcpy(p->p_comm, name);

	mutex_lock(&allproc_lock);
	p->p_pid = -(++system_procs);
	mutex_unlock(&allproc_lock);
	hash = &pidhash[PIDHASH(p->p_pid)];
	p->p_hash = *hash;
	*hash = p;

/*	p->p_pptr = &pgrp0; may be needed */
	p->p_pgrp = &pgrp0;
	p->p_flag = P_INMEM | P_SYSTEM;
	p->p_stat = SRUN;
	p->p_nice = NZERO;
	p->p_cred = &cred0;
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */
	p->p_limit = proc0.p_limit;
	p->p_vmspace = proc0.p_vmspace;
	p->p_addr = proc0paddr;				/* XXX */
	p->p_stats = &p->p_addr->u_stats;
	p->p_sigacts = &p->p_addr->u_sigacts;
	proc_set_condition_names(p);
}

/* gprof needs its behavior slightly tweaked if profiling UX */
int gprof_ux_server = TRUE;

/* 
 * On some architectures, failure to define this explicitly as 0 will force
 * the loader to pull in too much of libprof.
 */
void (*_monstartup_routine)() = 0;


void __main() {}

/*
 * System startup; initialize the world, create process 0,
 * mount root filesystem, and fork to create init and pagedaemon.
 * Most of the hard work is done in the lower-level initialization
 * routines including startup(), which does memory initialization
 * and autoconfiguration.
 */
void main(
    int argc,
    char **argv)
{
	extern void console_init(),init_mapped_time(),spl_init(),zone_init();
	extern void rqinit(),callout_init(),dev_utils_init();
	extern void device_reply_hdlr(),ux_server_init(),timer_init();

#if 0 /* if the server is built under Linux it will not prosper with this */
        /* Thou shalt not follow the null pointer... */
        (void)vm_protect(mach_task_self(), (vm_address_t)0,
			 vm_page_size, 0,  VM_PROT_NONE);
#endif
	allproc = &proc0;
	proc0.p_prev = &allproc;

	/*
	 * Wire down this thread until it becomes a server thread
	 */

	cthread_wire();

	/*
	 * Initialize msgbuf for logging
	 */

	msgbuf_init();

	init_second_server_flag(argc, argv);
	init_ports();



	/*
	 * Get arguments.
	 */
	get_config_info(argc, argv);

#ifdef	i386
	/*
	 * yes this is MI code, but ... how else would you do it ...
	 */
	if (major(rootdev) == 2) {	/* floppy. XXX search for string */
		dprintf("\r\n\r\nYou need to make sure that the File System floppy is");
		dprintf(" in the A: floppy drive.\r\n");
		dprintf("Type a carriage return when you are ready.\r\n\r\n");
		dgetc();
	}
#endif	/* i386*/

	/*
	 * Get a port to talk to the world.
	 */
	console_init();

	/*
	 * Setup mappable time
	 */
	init_mapped_time();

	/*
	 * Initialize SPL emulator
	 */
	spl_init();

	zone_init();

	sleep_init();

	/* Start timer thread */
	timer_init();

	/*
	 * Start device reply server
	 */
	dev_utils_init();
	device_reply_hdlr();

	ux_server_init();

	/* Initialize port to object mapper */
	port_object_init();

#ifdef USEACTIVATIONS
	/* Enable migrating threads */
	if (use_activations) {
		dprintf("*** USING ACTIVATIONS ***\n");
		ux_act_init();
	}
#endif

	/*
	 * Turn into a U*X thread, to do the rest of the
	 * initialization
	 */
	(void) ux_create_thread(system_setup);

	/*
	 * Unwire now
	 */
	cthread_unwire();

	/*
	 * This should never return from this condition wait
	 */
	if (debugger_thread) {
	    mach_port_t		bogus_port;
	    mach_msg_header_t	bogus_msg;
	    int			limit;

	    cthread_wire();
	    limit = cthread_kernel_limit();
	    if (limit != 0)
		cthread_set_kernel_limit(limit + 1);
	    (void) mach_port_allocate(mach_task_self(),
				      MACH_PORT_RIGHT_RECEIVE, &bogus_port);
	    while (1) {
		(void) mach_msg(&bogus_msg, MACH_RCV_MSG|MACH_RCV_TIMEOUT,
				0, 0, bogus_port, 5000, MACH_PORT_NULL);
	    }
	} else {
	    mutex_lock(&kill_lock);
	    condition_wait(&kill_main,&kill_lock);
	}
}

#if OSFMACH3
kernel_boot_info_t kernel_boot_info;

/*
 * AI-ONLY NOTE: rewritten for OSF's kernel and bootstrap task, in place
 * of Utah's, which parsed bootstrap.conf's arguments alone and, with
 * none, printed "Boot info IGNORED" and used a fixed vector.  The
 * kernel's boot information is parsed first, as a command line, and
 * the server's own arguments after it, so that a root on the startup
 * line wins (docs/deferred.md, O-1, O-4, O-7).  The reply comes from
 * i386/AT386/model_dep.c's machine_boot_info(): the boot device's
 * name, then " -s" when the boot line had -s, then any old-convention
 * boot string.  Its first word is a root unless the kernel could only
 * say "boot_device", an alias Lites' device table does not know; its
 * flag words are flags; other words are not for the server.
 */
void get_config_info(
	int	argc,
	char	**argv)
{
	kern_return_t	kr;
	char	*boot_argv[32];
	int	kboot_argc = 0;
	char	*boot_root = (char *)0;
	char	*cp;
	int	first = 1;

	kr = host_get_boot_info(privileged_host_port, kernel_boot_info);
	if (kr != KERN_SUCCESS)
		kernel_boot_info[0] = '\0';
	kernel_boot_info[KERNEL_BOOT_INFO_MAX - 1] = '\0';

	boot_argv[kboot_argc++] = (argc > 0) ? argv[0] : "startup";
	cp = kernel_boot_info;
	while (*cp != '\0') {
		char *word;

		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (*cp == '\0')
			break;
		word = cp;
		while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			cp++;
		if (*cp != '\0')
			*cp++ = '\0';
		if (first && word[0] != '-') {
			if (strcmp(word, "boot_device") != 0)
				boot_root = word;
		} else if (word[0] == '-' && word[1] != '\0'
			   && kboot_argc < 30) {
			dprintf("(lites) boot flag from the kernel: %s\n", word);
			boot_argv[kboot_argc++] = word;
		}
		first = 0;
	}
	if (boot_root)
		boot_argv[kboot_argc++] = boot_root;
	boot_argv[kboot_argc] = (char *)0;

	parse_arguments(kboot_argc, boot_argv);
	if (argc > 0)
		parse_arguments(argc, argv);
	set_root();
}

#else /* OSFMACH3 */

void get_config_info(
	int	argc,
	char	**argv)
{
	parse_arguments(argc, argv);
}

#endif /* OSFMACH3 */
