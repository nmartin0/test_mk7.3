# 
# Copyright 1991-1998 by Open Software Foundation, Inc. 
#              All Rights Reserved 
#  
# Permission to use, copy, modify, and distribute this software and 
# its documentation for any purpose and without fee is hereby granted, 
# provided that the above copyright notice appears in all copies and 
# that both the copyright notice and this permission notice appear in 
# supporting documentation. 
#  
# OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
# FOR A PARTICULAR PURPOSE. 
#  
# IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
# LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
# NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
# 
# 
# Mach Operating System
# Copyright (c) 1992 Carnegie Mellon University
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
# 
# Carnegie Mellon requests users of this software to return to
# 
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
# 
# any improvements or extensions that they make and grant Carnegie the
# rights to redistribute these changes.
# 
#
# cmk1.1
# 

PROGRAMS		= machipc

ILIST			= machipc
IDIR			= ${MACH3_CMD_IDIR}

USE_STATIC_LIBRARIES	=

OFILES			= machipc.o
LIBS			= -lnetname -lmach

INCFLAGS                = -I.


# Also provide rules for palloc, icmp, and tcpip,
# but don't build/install them by default.

palloc : palloc.o
	${RM} $@ $@.out
	${DEPS/%/$@} ${CC} -o $@.out ${CFLAGS} palloc.o ${LIBS} && \
	${MV} $@.out $@

icmp : icmp.o
	${RM} $@ $@.out
	${DEPS/%/$@} ${CC} -o $@.out ${CFLAGS} icmp.o ${LIBS} && \
	${MV} $@.out $@

tcpip : tcpip.o
	${RM} $@ $@.out
	${DEPS/%/$@} ${CC} -o $@.out ${CFLAGS} tcpip.o ${LIBS} && \
	${MV} $@.out $@

.include <${RULES_MK}>
