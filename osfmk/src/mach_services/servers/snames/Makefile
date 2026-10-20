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
# cmk1.1

PROGRAMS		= snames

ILIST			= snames
IDIR			= ${MACH3_SERVERS_IDIR}

USE_STATIC_LIBRARIES	=

LIBS			= -lservice -lmach
OFILES			= snames.o nprocs.o netnameServer.o netnameUser.o

MIG_DEFS		= netname.defs

DATAFILES		= netname_defs.h netname.h ${MIG_DEFS}
INCLUDES		= ${DATAFILES}

EXPDIR			= ${MACH3_INCLUDE_IDIR}servers/

.include <${RULES_MK}>
