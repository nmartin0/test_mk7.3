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

/*
 * mcpboot external declarations
 */
#define	MCPBOOT_EXIT_OK			0
#define	MCPBOOT_EXIT_USAGE		1
#define	MCPBOOT_EXIT_INVALID_ARGUMENT	2
#define	MCPBOOT_EXIT_UNKNOWN_FORMAT	3
#define	MCPBOOT_EXIT_INVALID_FORMAT	4

struct objfmt_switch {
    const char	 *name;
    int		(*recog)(void);
    int		(*boot)(void);
};

extern char *buffer;
extern unsigned int buflen;
extern char *progname;
