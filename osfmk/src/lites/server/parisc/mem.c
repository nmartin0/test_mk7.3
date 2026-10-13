/* Copyright (c) 1991, 1992, 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Subject to your agreements with CMU,
 * permission to use, copy, modify and distribute this software and its
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
 * 	Utah $Hdr: mem.c 1.8 94/12/15$
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
 * Server interface to kernel memory.  Derived from serv/device_misc.c
 */

#include <serv/import_mach.h>
#include <serv/server_defs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/synch.h>

#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/synch.h>

#include <serv/device.h>
#include <serv/device_utils.h>
#include <mach/kern_return.h>
#include <sys/proc.h>

#include <sys/parallel.h>

mach_port_t	mem_device_port = MACH_PORT_NULL; /* port to memory device */

#define M_MEM		0	/* /dev/mem  - physical kernel memory */
#define M_KMEM		1	/* /dev/kmem - virtual kernel memory & I/O */
#define M_NULL		2	/* /dev/null - EOF & Rathole */
#define M_ZERO		3	/* /dev/zero - return 0 fill */

memopen(dev_t dev, int flag, int devtype, struct proc *p)
{
	char		name[32];
	mach_port_t	device_port;
	int 		error = 0;
	
	if (mem_device_port == MACH_PORT_NULL) {
		/*
		 * Try to find the device, and open it.
		 */
		if ((error = cdev_name_string(dev, name)) != 0)
			return(error);
		if ((error = device_open(device_server_port,
					 0, name, &device_port)) != D_SUCCESS)
			return(error);

		mem_device_port = device_port;
	}
	return (0);
}

memread(dev_t dev, struct uio *uio, int flag)
{
	return (memrw(dev, uio, UIO_READ));
}

memwrite(dev_t dev, struct uio *uio, int flag)
{
	return (memrw(dev, uio, UIO_WRITE));
}	

#define ZBUFSIZE	1024

memrw(dev_t dev, struct uio *uio, int rw)
{
	register u_int	 	c;
	register struct iovec 	*iov;
	io_buf_ptr_inband_t	data;	/* inline array */
	int 			offset, count, error = 0, didzero = 0;
	char			zbuf[ZBUFSIZE];

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("memrw");
			continue;
		}
		switch (minor(dev)) {

		case M_KMEM:
			c = iov->iov_len;
			if (c > IO_INBAND_MAX)
				c = IO_INBAND_MAX;

			if (rw == UIO_READ) {
				count = IO_INBAND_MAX;
				error = device_read_inband(mem_device_port, 0,
							   uio->uio_offset,
							   c, &data[0], &count);
				if (error != 0)
					return (dev_error_to_errno(error));
			
				error = uiomove(&data[0], count, uio);
			}
			else {
				offset = uio->uio_offset;
				error = uiomove(&data[0], c, uio);
				if (error != 0)
					continue;
				
				count = 0;
				error = device_write_inband(mem_device_port, 0,
							    offset, &data[0], c,
							    &count);
				
				if (error != 0)
					return (dev_error_to_errno(error));
			}
			break;

		case M_NULL:
			if (rw == UIO_READ)
				return (0);
			c = iov->iov_len;
			iov->iov_base += c;
			iov->iov_len -= c;
			uio->uio_offset += c;
			uio->uio_resid -= c;
			break;

		case M_ZERO:
#if 0
			if (rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (!didzero) {
				bzero(zbuf, ZBUFSIZE);
				didzero = 1;
			}
			c = min(iov->iov_len, ZBUFSIZE);
			error = uiomove(zbuf, (int)c, uio);
#endif
			break;
		}
	}
	return (error);
}
