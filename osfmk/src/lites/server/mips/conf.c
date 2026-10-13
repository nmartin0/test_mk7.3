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
 * $Log: conf.c,v $
 * Revision 1.1.1.1  1995/03/02  21:49:42  mike
 * Initial Lites release from hut.fi
 *
 */
/* 
 *	File: mips/server/conf.c
 *	Authors:
 *	Randall Dean, Carnegie Mellon University, 1992.
 *	Alessandro Forin, Carnegie Mellon University, 1992.
 *	Johannes Helander, Helsinki University of Technology, 1994.
 *
 *	Simplified configuration.
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)conf.c	8.3 (Berkeley) 1/21/94
 */

#include <serv/import_mach.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>	/* for B_TAPE */
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/vnode.h>

#include <serv/device.h>
#include <serv/device_utils.h>

#include <sys/ioctl.h>	/* for tty */
#include <sys/tty.h>
#include <sys/proc.h>

#include <serv/tape_io.h>	/* tape_io prototypes */
#include <device/tape_status.h>
#include <chips/screen.h>

/*
 * Block devices all use the same open/close/strategy routines.
 */
int bdev_open(dev_t, int, int, struct proc *);
int bdev_close(dev_t, int, int, struct proc *);
int bio_strategy(struct buf *);
int bdev_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int bdev_dump(dev_t);
int bdev_size(dev_t);
#define	bdev_ops	bdev_open, bdev_close, bio_strategy, bdev_ioctl,\
			bdev_dump, bdev_size

struct bdevsw	bdevsw[] =
{
/*0*/	{ "ip",		C_BLOCK(8),	bdev_ops },

#ifdef	PMAX
/*1*/	{ "md",		C_BLOCK(8),	bdev_ops },
#else
/*1*/	{ "ts",		B_TAPE|C_BLOCK(1), bdev_ops },
#endif	PMAX

/*2*/	{ "",		0,		bdev_ops },
/*3*/	{ "",		0,		bdev_ops },
/*4*/	{ "rd",		C_BLOCK(8),	bdev_ops },
/*5*/	{ "tth",	C_BLOCK(8),	bdev_ops },
/*6*/	{ "",		0,		bdev_ops },
/*7*/	{ "",		0,		bdev_ops },
#ifdef	PMAX
/*8*/	{ "rz",		C_BLOCK(8),	bdev_ops },
/*9*/	{ "tz",		B_TAPE|C_BLOCK(1), bdev_ops },
#else	/* PMAX */
/*8*/	{ "",		0,		bdev_ops },
/*9*/	{ "",		0,		bdev_ops },
#endif	PMAX
/*10*/	{ "fd",		C_BLOCK(8),	bdev_ops },
};
int	nblkdev = sizeof(bdevsw)/sizeof(bdevsw[0]);

int char_open(dev_t, int, int, struct proc *);
int char_close(dev_t, int, int, struct proc *);
int char_read(dev_t, struct uio *, int);
int char_write(dev_t, struct uio *, int);
int char_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int char_select(dev_t, int, struct proc *);
mach_port_t char_port(dev_t);
#define	char_ops \
	char_open, char_close, char_read, char_write, char_ioctl, \
	null_stop, null_reset, null_tty, char_select, device_map, \
	null_strategy, char_port

int disk_open(dev_t, int, int, struct proc *);
int disk_close(dev_t, int, int, struct proc *);
int disk_read(dev_t, struct uio *, int);
int disk_write(dev_t, struct uio *, int);
int disk_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
extern mach_port_t disk_port(dev_t);
#define	disk_ops \
	disk_open, disk_close, disk_read, disk_write, disk_ioctl, \
	null_stop, null_reset, null_tty, seltrue, null_mmap, \
	null_strategy, disk_port

#define	tape_ops \
	tape_open, tape_close, tape_read, tape_write, tape_ioctl, \
	null_stop, null_reset, null_tty, seltrue, null_mmap, \
	null_strategy, tape_port

int tty_open(dev_t, int, int, struct proc *);
int tty_close(dev_t, int, int, struct proc *);
int tty_read(dev_t, struct uio *, int);
int tty_write(dev_t, struct uio *, int);
int tty_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int ttselect(dev_t, int, struct proc *);
int tty_stop(struct tty *, int);
struct tty *tty_find_tty(dev_t);
#define	tty_ops	\
	tty_open, tty_close, tty_read, tty_write, tty_ioctl,\
	tty_stop, null_reset, tty_find_tty, ttselect, null_mmap, \
	null_strategy, null_port

int cons_open(dev_t, int, int, struct proc *);
int cons_write(dev_t, struct uio *, int);
int cons_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
extern mach_port_t cons_port(dev_t);
#define	console_ops	\
	cons_open, tty_close, tty_read, cons_write, cons_ioctl, \
	tty_stop, null_reset, tty_find_tty, ttselect, device_map, \
	null_strategy, cons_port

int cons_open_xxx(dev_t, int, int, struct proc *);
int cons_write_xxx(dev_t, struct uio *, int);
int cons_ioctl_xxx(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
extern mach_port_t cons_port_xxx(dev_t);
#define	console_ops_xxx	\
	cons_open_xxx, tty_close, tty_read, cons_write_xxx, cons_ioctl_xxx, \
	tty_stop, null_reset, tty_find_tty, ttselect, device_map, \
	null_strategy, cons_port_xxx

int cttyopen(dev_t, int, int, struct proc *);
int cttyread(dev_t, struct uio *, int);
int cttywrite(dev_t, struct uio *, int);
int cttyioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int cttyselect(dev_t, int, struct proc *);
#define	ctty_ops \
 	cttyopen, null_close, cttyread, cttywrite, cttyioctl, \
 	null_stop, null_reset, null_tty, cttyselect, null_mmap, \
	null_strategy, null_port

int logopen(dev_t, int, int, struct proc *);
int logclose(dev_t, int, int, struct proc *);
int logread(dev_t, struct uio *, int);
int logioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int logselect(dev_t, int, struct proc *);
#define	log_ops \
	logopen, logclose, logread, null_write, logioctl, \
	null_stop, null_reset, null_tty, logselect, null_mmap, \
	null_strategy, null_port

int mmopen(dev_t, int, int, struct proc *);
int mmread(dev_t, struct uio *, int);
int mmwrite(dev_t, struct uio *, int);
#define	mm_ops \
	mmopen, null_close, mmread, mmwrite, nodev_ioctl, \
	null_stop, null_reset, null_tty, seltrue, null_mmap, \
	null_strategy, null_port

int ptsopen(dev_t, int, int, struct proc *);
int ptsclose(dev_t, int, int, struct proc *);
int ptsread(dev_t, struct uio *, int);
int ptswrite(dev_t, struct uio *, int);
int ptyioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int ptsselect(dev_t, int, struct proc *);
int ptsstop(struct tty *, int);
struct tty *pty_find_tty(dev_t);
#define	pts_ops \
	ptsopen, ptsclose, ptsread, ptswrite, ptyioctl, \
	ptsstop, null_reset, pty_find_tty, ttselect, null_mmap, \
	null_strategy, null_port

int ptcopen(dev_t, int, int, struct proc *);
int ptcclose(dev_t, int, int, struct proc *);
int ptcread(dev_t, struct uio *, int);
int ptcwrite(dev_t, struct uio *, int);
int ptcselect(dev_t, int, struct proc *);
#define	ptc_ops \
	ptcopen, ptcclose, ptcread, ptcwrite, ptyioctl, \
	null_stop, null_reset, null_tty, ptcselect, null_mmap, \
	null_strategy, null_port

/* Like char_ops but less methods */
#define	maptime_ops \
	char_open, char_close, null_read, null_write, null_ioctl, \
	null_stop, null_reset, null_tty, null_select, device_map, \
	null_strategy, char_port

/* Mapped timezone and time offset */
int maptz_open(dev_t, int, int, struct proc *);
int maptz_close(dev_t, int, int, struct proc *);
mach_port_t maptz_port(dev_t);
kern_return_t maptz_map(mach_port_t, vm_prot_t, vm_offset_t, vm_size_t,
			mach_port_t *, int);
#define	maptz_ops \
	maptz_open, maptz_close, null_read, null_write, null_ioctl, \
	null_stop, null_reset, null_tty, null_select, maptz_map, \
	null_strategy, maptz_port

#define	no_ops \
	nodev_open, nodev_close, nodev_read, nodev_write, nodev_ioctl, \
	null_stop, nodev_reset, null_tty, nodev_select, nodev_mmap, \
	nodev_strategy, null_port

#define sy_ops no_ops
#define time_ops no_ops

struct cdevsw	cdevsw[] =
{
/*0*/	{ "console",	C_MINOR,	console_ops_xxx,	},
/*1*/	{ "",		0,		sy_ops,		},
/*2*/	{ "",		0,		mm_ops,		},

#ifdef	MSERIES
/*3*/	{ "ip",		C_BLOCK(8),	disk_ops,	},
#else
/*3*/	{ "",		0,		log_ops,	},
#endif	MSERIES

/*4*/	{ "",		0,		no_ops,		},

#ifdef	MSERIES
/*5*/	{ "ts",		0,		tape_ops,	},
#else
/*5*/	{ "",		0,		no_ops,		},
#endif	MSERIES

#if	NPTY > 0
/*6*/	{ "",		0,		pts_ops,	},
/*7*/	{ "",		0,		ptc_ops,	},
#else	NPTY > 0
/*6*/	{ "",		0,		no_ops,		},
/*7*/	{ "",		0,		no_ops,		},
#endif	NPTY > 0

#ifdef	MSERIES
/*8*/	{ "cp",		0,		tty_ops,	},
/*9*/	{ "",		0,		log_ops,	},
/*10*/	{ "tth",	C_BLOCK(8),	disk_ops,	},
#else
/*8*/	{ "rz",		C_BLOCK(8),	disk_ops,	},
/*9*/	{ "tz",		0,		tape_ops,	},
/*10*/	{ "pm",		0,		tty_ops,	},
#endif	MSERIES

/*11*/	{ "",		0,		no_ops,		},

#if	VICE
/*12*/	{ "",		0,		rmt_ops,	},
#else	VICE
/*12*/	{ "",		0,		no_ops,		},
#endif	VICE

#if	NCMUPTY > 0
/*13*/	{ "",		0,		cmupty_ops,	},
/*14*/	{ "",		0,		cmuptyc_ops,	},
#else	NCMUPTY > 0
/*13*/	{ "",		0,		no_ops,		},
/*14*/	{ "",		0,		no_ops,		},
#endif	NCMUPTY > 0

/*15*/	{ "UXtime",	0,		time_ops	},

#if	NMACH_CFS > 0
/*16*/	{ "vc",		0,		vc_ops,		},
#else	/* NMACH_CFS */
/*16*/	{ "",		0,		no_ops,		},
#endif	/* NMACH_CFS */
/*17*/	{ "",		0,		no_ops,		},	/* was DFSTRACE */

/*18*/	{ "sc",		0,		disk_ops,	},	/* scsi processor */
/*19*/	{ "fd",		C_BLOCK(8),	disk_ops,	},	/* floppy */
/*20*/	{ "",		0,		no_ops,		},	/* ULTRIX compat hole */
/*21*/	{ "",		0,		no_ops,		},	/* ULTRIX compat hole */
/*22*/	{ "cz",		C_BLOCK(8),	tty_ops,	},	/* SCSI serial lines */
/*23*/	{ "cz",		C_BLOCK(8),	tty_ops,	},	/* SCSI serial lines+8 */
/*24*/	{ "mouse",	0,		char_ops,	},      /* mouse */
/*25*/	{ "time",	0,		maptime_ops,	},/* mapped time */
/*26*/	{ "",		0,		maptz_ops,	},/* mapped timezone */
};
#define NCHRDEV  sizeof(cdevsw)/sizeof(cdevsw[0])
int	nchrdev = NCHRDEV;

dev_t	sydev = makedev(1, 0);

/* *** These are written by af */
/*
 * Conjure up a name string for funny devices (not all minors have
 * the same name).
 */
int
check_dev(dev, str)
	dev_t	dev;
	char	*str;
{
	int	major_num = major(dev);
	int	minor_num = minor(dev);

	if (major_num == 0) {
		/*
		 * Console, or ... something else
		 * 	0,0	-> console
		 *	0,1	-> mouse
		 *	0,2	-> serial line (modem)
		 *	0,3	-> serial line (lp)
		 * Minors 1..5 are mapped to the "pm" Mach device
		 */
		if (minor_num == 0) {
			strcpy(str, "console");
			return 0;
		} else if (minor_num < 5) {
			char num[4];
			strcpy(str, "sl");
			itoa(minor_num, num);
			strcat(str, num);
			return 0;
		}
	}
	return (ENXIO);
}

/*
 *	Special hacks for graphics: the same major is used
 *	on the pmax for way too many things.
 */
#ifdef	PMAX
#define IS_CONS(d)  (minor(d)==0)
#define IS_MOUSE(d) (minor(d)==1)
/* else it is a serial line, use tty_ops */
#else
 You got to define the console minors
#endif

int cons_open_xxx(dev_t dev, int flag, int devtype, struct proc *p)
{
	if (IS_MOUSE(dev))
		return char_open(dev, flag, devtype, p);
	else if (IS_CONS(dev))
		return cons_open(dev, flag, devtype, p);
	return tty_open(dev, flag, devtype, p);
}

static
cons_close_xxx(dev_t dev, int flag, int mode, struct proc *p)
{
	if (IS_MOUSE(dev))
		return char_close(dev, flag, mode, p);
	return tty_close(dev, flag, mode, p);
}


static
cons_read_xxx(dev_t dev, struct uio *uio, int flag)
{
	if (IS_MOUSE(dev))
		return char_read(dev, uio, flag);/* whatever that does */
	return tty_read(dev, uio, flag);
}


cons_write_xxx(dev_t dev, struct uio *uio, int flag)
{
	if (IS_MOUSE(dev))
		return char_write(dev, uio, flag);/* whatever that does */
	else if (IS_CONS(dev))
		return cons_write(dev, uio, flag);
	return tty_write(dev, uio, flag);
}

mach_port_t cons_port_xxx(dev_t dev);

cons_ioctl_xxx(dev_t dev, ioctl_cmd_t cmd, caddr_t data, int flag,
	       struct proc *p)
{
	if (IS_MOUSE(dev) && (cmd == SCREEN_ADJ_MAPPED_INFO)) {
		memory_object_t	pager;
		vm_offset_t	address;
		struct {
			vm_size_t	size;
			vm_offset_t	offset;
			boolean_t	isa_3d_board;
		} off_info;
		mach_port_t	dev_port;
		kern_return_t	ret;

		dev_port = cons_port_xxx(dev);
		ret = 3;
		off_info.isa_3d_board = FALSE;
		(void) device_get_status(dev_port,
					 SCREEN_GET_OFFSETS,
					 (dev_status_t) &off_info,
					 &ret);

		if (ret < 3)
			off_info.isa_3d_board = FALSE;

		pager = device_pager_create(dev,
					    0,
					    off_info.size,
					    VM_PROT_READ|VM_PROT_WRITE);
		if (pager == MACH_PORT_NULL)
			return EINVAL;

		address = VM_MIN_ADDRESS;
#		define alignment 0x3fffff	/* 4 meg boundary */

		ret = vm_map(p->p_task, &address, off_info.size,
			     alignment, TRUE, pager, 0, FALSE,
			     VM_PROT_READ|VM_PROT_WRITE,
			     VM_PROT_READ|VM_PROT_WRITE,
			     VM_INHERIT_SHARE);
		if (ret)
			goto out;

		/*
		 *	Now, Jim Gettys says this is a good idea on
		 *	all machines as it prevents the cache from
		 *	being clobbered with useless (screen) data.
		 *	So I followed the verb and tried it out, with
		 *	catastrofic results.  No friends, if you can
		 *	keep it cached do it, the loss is visibly great.
		 */
		if (off_info.offset) {
			vm_machine_attribute_val_t val = MATTR_VAL_OFF;
			vm_machine_attribute(p->p_task,
					     address, off_info.size,
					     MATTR_CACHE, &val);
		}

		(void) device_set_status(dev_port,
					 SCREEN_ADJ_MAPPED_INFO,
					 &address,
					 1);
		*(vm_offset_t *)data = address + off_info.offset;

		/*
		 * The 3D board was done in ... weird ways.
		 */
		if (off_info.isa_3d_board)
			start_3d_thread(dev_port, off_info.size,
					off_info.offset, pager, p);
out:
		device_pager_release(pager);
		return (ret) ? EINVAL : 0;

	}
	if (IS_MOUSE(dev))
		return char_ioctl(dev, cmd, data, flag, p);
	else if (IS_CONS(dev))
		return cons_ioctl(dev, cmd, data, flag, p);
	else
		return tty_ioctl(dev, cmd, data, flag, p);
}

mach_port_t
cons_port_xxx(dev_t dev)
{
	if (IS_MOUSE(dev))
		return char_port(dev);
	else if (IS_CONS(dev))
		return cons_port(dev);
	return MACH_PORT_NULL;
}

cons_select_xxx(dev_t dev, int rw, struct proc *p)
{
	if (IS_MOUSE(dev))
		return char_select(dev, rw, p);
	return ttselect(dev, rw, p);
}

cons_stop_xxx(struct tty *tp, int rw)
{
	return tty_stop(tp, rw); /* XXX */
}

struct tty *
cons_find_xxx(dev_t dev)
{
	if (IS_MOUSE(dev))
		return null_tty(dev);
	return tty_find_tty(dev);
}

/*
 * Special thread to cope with a specially weird device
 */

typedef struct {
	struct proc	*x11_server;
	user_info_t	*screen_info;
} dev_3d_info_t;

dev_3d_thread(dev_3d_info)
	register dev_3d_info_t	*dev_3d_info;
{
	struct proc *p;
	proc_invocation_t pk = get_proc_invocation();

	system_proc(&p, "Dev3D");
	cthread_wire();
	pk->k_ipl = -1;

	for (;;) {

		evc_wait(dev_3d_info->screen_info->event_id);

		switch (dev_3d_info->screen_info->interrupt_info) {
		case 1:
			psignal(dev_3d_info->x11_server, SIGSEGV);
			break;
		case 2:
			psignal(dev_3d_info->x11_server, SIGUSR1);
			break;
		case 3:
			psignal(dev_3d_info->x11_server, SIGILL);
			break;
		default:
			panic("3d_thread");
		}
	}
}

start_3d_thread(dev_port, size, offset, pager, x11_server)
	mach_port_t	dev_port;
	vm_size_t	size;
	vm_offset_t	offset;
	memory_object_t	pager;
	struct proc	*x11_server;
{
	vm_offset_t		address;
	vm_machine_attribute_val_t val = MATTR_VAL_OFF;
	static dev_3d_info_t	*dev_3d_info = 0;

	/* make that horrible call */
        (void) device_set_status(dev_port,
                                 SCREEN_HARDWARE_DEP,
                                 &x11_server->p_task,
                                 1);

	/* Thread running already ? */
	if (dev_3d_info) {
		/* yes, just tell it things have changed */
		dev_3d_info->x11_server = x11_server;
		return;
	}

	dev_3d_info = (dev_3d_info_t *) malloc(sizeof(*dev_3d_info));
	dev_3d_info->x11_server = x11_server;

	/* map it in our space */
	address = 0;
	(void) vm_map(mach_task_self(), &address, size,
			     alignment, TRUE, pager, 0, FALSE,
			     VM_PROT_READ|VM_PROT_WRITE,
			     VM_PROT_READ|VM_PROT_WRITE,
			     VM_INHERIT_NONE);
                
	/* make it uncached */
	vm_machine_attribute(mach_task_self(),
			     address, size,
			     MATTR_CACHE, &val);

	address += offset;
	dev_3d_info->screen_info = (user_info_t *) address;

	cthread_detach(cthread_fork((cthread_fn_t)dev_3d_thread, (void *)dev_3d_info));
}

#if 0
/*
 * More special code (but not quite machdep) to
 * handle the rewind/norewind business.  Also,
 * a good place to add density/speed selection
 * based on the tape's minor.
 */
#define	TAPE_UNIT(dev)		((dev)&(~0xff))|(((dev)&0xe0)>>3)|((dev)&0x3)
#define	TAPE_REWINDS(dev)	(((dev)&0x1c)==0)||(((dev)&0x1c)==8)

tape_open(dev_t dev, int flag, int devtype, struct proc *p)
{
	int ret = disk_open(TAPE_UNIT(dev), flag, devtype, p);
	if ((ret == 0) && (TAPE_REWINDS(dev))) {
		struct tape_status	ts;

		ts.mt_type = ts.speed = ts.density = 0;
		ts.flags = TAPE_FLG_REWIND;
		(void) device_set_status(
			(mach_port_t)dev_number_hash_lookup(TAPE_UNIT(dev)),
			TAPE_STATUS,
			&ts,
			TAPE_STATUS_COUNT);
	}
	return ret;
}

tape_close(dev_t dev, int flag, int mode, struct proc *p)
{
	return disk_close(TAPE_UNIT(dev), flag, mode, p);
}

tape_read(dev_t dev, struct uio *uio, int flag)
{
	return disk_read(TAPE_UNIT(dev), uio, flag);
}

tape_write(dev_t dev, struct uio *uio, int flag)
{
	return disk_write(TAPE_UNIT(dev), uio, flag);
}

tape_ioctl(dev_t dev, ioctl_cmd_t cmd, caddr_t data, int flag, struct proc *p)
{
	return(disk_ioctl(TAPE_UNIT(dev), cmd, data, flag, p));
}
#endif 0

/* *** from Lite with fixes. Cleanup */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(3, 0);

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
iskmemdev(dev)
	dev_t dev;
{

	return FALSE;
}

iszerodev(dev)
	dev_t dev;
{
	return FALSE;
	return (major(dev) == 2 && minor(dev) == 12);
}

/*
 * Routine to determine if a device is a disk.
 *
 * A minimal stub routine can always return 0.
 */
isdisk(dev, type)
	dev_t dev;
	int type;
{

	switch (major(dev)) {
	case 8:
		if (type == VCHR)
			return TRUE;
	case 10:
		if (type == VBLK)
			return TRUE;
		return FALSE;
	case 18:
	case 19:
		if (type == VCHR)
			return TRUE;
		/* fall through */
	default:
		return FALSE;
	}
	/* NOTREACHED */
}

static int chrtoblktbl[NCHRDEV] =  {
      /* VCHR */      /* VBLK */
	/* 0 */		NODEV,
	/* 1 */		NODEV,
	/* 2 */		NODEV,
#ifdef MSERIES
	/* 3 */		0,	/* ip (?) */
#else
	/* 3 */		NODEV,
#endif
	/* 4 */		NODEV,
	/* 5 */		NODEV,
	/* 6 */		NODEV,
	/* 7 */		NODEV,
	/* 8 */		8,	/* rz */
	/* 9 */		9,	/* tz */
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	4,	/* sd */
	/* 14 */	NODEV,
	/* 15 */	6,	/* cd */
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	NODEV,
	/* 19 */	10,	/* fd */
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
};
/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	if (major(dev) >= NCHRDEV || (blkmaj = chrtoblktbl[major(dev)]) == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
}

dev_t blktochr(dev_t bdev)
{
  int i;

  for (i = 0; i < NCHRDEV; i++) {
    if (major(bdev) == chrtoblktbl[i])
      return makedev(i, minor(bdev));
  }
  return NODEV;
}
