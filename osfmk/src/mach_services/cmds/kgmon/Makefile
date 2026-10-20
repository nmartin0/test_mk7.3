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

PROGRAMS		= kgmon

OBJECT_FORMAT		= ELF
USE_STATIC_LIBRARIES	= 1

ILIST			= kgmon
IDIR			= ${MACH3_SERVERS_IDIR}

IOWNER			= ${ROOT_OWNER}
IMODE			= ${SETUID_MODE}

CFLAGS			= -D_BSD ${${OBJECT_FORMAT}_EXTRA_WARNINGS}
LIBS			= -lmach -lmachid -lnetname 
OFILES			= kgmon.o

.include <${RULES_MK}>
