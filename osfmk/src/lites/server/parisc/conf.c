/* 
 * Copyright (c) 1992, 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: conf.c 1.10 94/12/16$
 */
/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

/*
 * Simplified configuration.
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
#define bdev_nop	null_open, null_close, null_strategy, null_ioctl, \
			null_dump, null_psize

struct bdevsw	bdevsw[] =
{
/*0*/	{ "",		0,		bdev_nop },
/*1*/	{ "",		0,		bdev_nop },
/*2*/	{ "",		0,		bdev_nop },
/*3*/	{ "",		0,		bdev_nop },
/*4*/	{ "",		0,		bdev_nop },
/*5*/	{ "",		0,		bdev_nop },
/*6*/	{ "",		0,		bdev_nop },
/*7*/	{ "sd",		C_BLOCK(8),	bdev_ops },	/* scsi disk */
};
int	nblkdev = sizeof(bdevsw)/sizeof(bdevsw[0]);

int char_open(dev_t, int, int, struct proc *);
int char_close(dev_t, int, int, struct proc *);
int char_read(dev_t, struct uio *, int);
int char_write(dev_t, struct uio *, int);
int char_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
int char_select(dev_t, int, struct proc *);
mach_port_t char_port(dev_t);
#define char_ops \
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

/* Frame buffer */
int grf_ioctl(dev_t, ioctl_cmd_t, caddr_t, int, struct proc *);
#define	grf_ops \
	char_open, char_close, null_read, null_write, grf_ioctl, \
	null_stop, null_reset, null_tty, seltrue, device_map, \
	null_strategy, char_port

/* Input (keyboard, mouse) device loop */
#define	hil_ops \
	char_open, char_close, char_read, null_write, char_ioctl, \
	null_stop, null_reset, null_tty, char_select, nodev_mmap, \
	null_strategy, char_port

/* Mach kernel memory */
int memopen(dev_t, int, int, struct proc *);
int memread(dev_t, struct uio *, int);
int memwrite(dev_t, struct uio *, int);
#define	mem_ops \
	memopen, nodev_close, memread, memwrite, nodev_ioctl, \
	null_stop, null_reset, null_tty, seltrue, nodev_mmap, \
	null_strategy, null_port

struct cdevsw	cdevsw[] =
{
/*0*/	{ "console", 0,		console_ops,	   },	/* console */
/*1*/	{ "",	     0,		no_ops,		   },	/* - */
/*2*/	{ "",	     0,		mm_ops,		   },	/* kmem,null */
/*3*/	{ "",	     0,		no_ops,		   },	/* - */
/*4*/	{ "",	     0,		pts_ops,	   },   /* pts */
/*5*/	{ "",	     0,		ptc_ops,	   },   /* ptc */
/*6*/	{ "",	     0,		log_ops,	   },	/* log */
/*7*/	{ "",	     0,		no_ops,		   },	/* - */
/*8*/	{ "",	     0,		ctty_ops,	   },	/* tty */
/*9*/	{ "",	     0,		no_ops,		   },	/* - */
/*10*/	{ "grf",     0,		grf_ops,	   },	/* grf */
/*11*/	{ "",	     0,		no_ops,		   },	/* - */
/*12*/	{ "sd",	     C_BLOCK(8),disk_ops,	   },	/* sd */
/*13*/	{ "ite",     0,		tty_ops,	   },	/* ite */
/*14*/	{ "hil",     0,		hil_ops,	   },	/* hil */
/*15*/	{ "",	     0,		no_ops,		   },	/* - */
/*16*/	{ "dca",     0,		tty_ops,	   },	/* serial */
/*17*/	{ "",	     0,		no_ops,		   },	/* - */
/*18*/	{ "",	     0,		no_ops,		   },	/* - */
/*19*/	{ "",	     0,		no_ops,		   },	/* - */
/*20*/	{ "",	     0,		no_ops,		   },	/* - */
/*21*/	{ "",	     0,		no_ops,		   },	/* - */
/*22*/	{ "",	     0,		no_ops,		   },	/* - */
/*23*/	{ "",	     0,		no_ops,		   },	/* - */
/*24*/	{ "",	     0,		no_ops,		   },	/* - */
/*25*/	{ "time",    0,		maptime_ops,	   },	/* mapped time */
/*26*/	{ "",	     0,		maptz_ops,	   },	/* mapped timezone */
/*27*/	{ "",	     0,		no_ops,		   },	/* - */
/*28*/	{ "",	     0,		no_ops,		   },	/* - */
/*29*/	{ "",	     0,		no_ops,		   },	/* - */
/*30*/	{ "mkmem",   0,		mem_ops,	   },	/* mkmem */
};
#define NCHRDEV sizeof(cdevsw)/sizeof(cdevsw[0])
int	nchrdev = NCHRDEV;

dev_t	cttydev = makedev(2, 0);
int	mem_no = 3;

/*
 * Conjure up a name string for funny devices (not all minors have
 * the same name).
 */
int
check_dev(dev_t dev, char *str)
{
	return 0;
}

/*
 * GRF support
 */
#define MACH_KERNEL
#include <hpdev/grfioctl.h>
#undef MACH_KERNEL

int grf_ioctl(dev_t dev, ioctl_cmd_t cmd, caddr_t data, int flag, struct proc *p)
{
	memory_object_t	pager;
	vm_offset_t	address;
	vm_size_t	size;
	kern_return_t	error;
	struct grf_fbinfo grfinfo;

	switch (cmd) {
	case GCMAP:
		size = (sizeof(grfinfo) + sizeof(int) - 1) / sizeof(int);
		(void) device_get_status(char_port(dev), GCDESCRIBE,
					 (dev_status_t)&grfinfo, &size);
		address = (vm_offset_t)grfinfo.regbase & 0xFC000000;

		/* passed in address is ignored */
		error = device_set_status(char_port(dev), cmd,
					  (dev_status_t)data, 1);
		if (error != KERN_SUCCESS)
			return(EIO);

		size = 0x2000000;	/* XXX fixme */
		pager = device_pager_create(dev, 0, size,
					    VM_PROT_READ|VM_PROT_WRITE);
		if (pager == MACH_PORT_NULL)
			return(EINVAL);

		error = vm_map(p->p_task, &address, size,
			       0, FALSE, pager, 0, FALSE,
			       VM_PROT_READ|VM_PROT_WRITE,
			       VM_PROT_READ|VM_PROT_WRITE,
			       VM_INHERIT_SHARE);
		device_pager_release(pager);
		if (error)
			return(EINVAL);
		*(vm_offset_t *)data = address;
		break;

	case GCUNMAP:
		size = (sizeof(grfinfo) + sizeof(int) - 1) / sizeof(int);
		(void) device_get_status(char_port(dev), GCDESCRIBE,
					 (dev_status_t)&grfinfo, &size);
		address = (vm_offset_t)grfinfo.regbase & 0xFC000000;
		size = 0x2000000;	/* XXX fixme */
		(void) vm_deallocate(p->p_task, address, size);

		/* passed in address is ignored */
		error = device_set_status(char_port(dev), cmd,
					  (dev_status_t)data, 1);
		if (error != KERN_SUCCESS)
			return(EIO);
		break;

	default:
		return(char_ioctl(dev, cmd, data, flag, p));
	}
	return(0);
}	

/* XXX from Lite with fixes. Cleanup */

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
boolean_t iskmemdev(dev_t dev)
{

	return FALSE;
}

boolean_t iszerodev(dev_t dev)
{
	return (major(dev) == 2 && minor(dev) == 3);
}

/*
 * Routine to determine if a device is a disk.
 *
 * A minimal stub routine can always return 0.
 */
boolean_t isdisk(dev_t dev, int type)
{

	switch (major(dev)) {
	case 0:
		if (type == VBLK)
			return TRUE;
		return FALSE;
	case 4:
		if (type == VCHR)
			return TRUE;
		return FALSE;
	default:
		return FALSE;
	}
	/* NOTREACHED */
}

static int chrtoblktbl[NCHRDEV] =  {
      /* VCHR */      /* VBLK */
	/* 0-3 */	NODEV, NODEV, NODEV, NODEV,
	/* 4 */		0,	/* sd */
	/* 5-7 */	NODEV, NODEV, NODEV,
	/* 8-11 */	NODEV, NODEV, NODEV, NODEV,
	/* 12-15 */	NODEV, NODEV, NODEV, NODEV,
	/* 16-19 */	NODEV, NODEV, NODEV, NODEV,
	/* 20-23 */	NODEV, NODEV, NODEV, NODEV,
	/* 24-27 */	NODEV, NODEV, NODEV, NODEV,
	/* 28-30 */	NODEV, NODEV, NODEV
};
/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
dev_t chrtoblk(dev_t dev)
{
	int blkmaj;

	if (major(dev) >= NCHRDEV || (blkmaj = chrtoblktbl[major(dev)]) == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
}

dev_t blktochr(dev_t bdev)
{
	int i;

	for (i = 0; i < NCHRDEV; i++)
		if (major(bdev) == chrtoblktbl[i])
			return (makedev(i, minor(bdev)));
	return (NODEV);
}
