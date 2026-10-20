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
# cmk1.1


#
#	Makefile for netmem example.  
#
CFLAGS			= -DMOR=1

PROGRAMS		= netmem_pager netmem_client

USE_STATIC_LIBRARIES	=

ILIST			= netmem_pager ${INCLUDES}
IDIR			= ${MACH3_INCLUDE_IDIR}servers/
IMODE			= 644
netmem_pager_IDIR	= ${MACH3_SERVERS_IDIR}
netmem_pager_IMODE	= 755

netmem_pager_OFILES	= netmem_msg_server.o netmem_pager.o \
			  netmem_msg_reply_user.o 
netmem_client_OFILES	= netmem_msg_user.o netmem_client.o

LIBS			= -lnetname -lmach

MIG_DEFS		= netmem_msg.defs netmem_msg_reply.defs

DATAFILES		= netmem_msg.h netmem_msg_reply.h ${MIG_DEFS}
INCLUDES		= ${DATAFILES}

EXPDIR			= ${MACH3_INCLUDE_IDIR}servers/


.include <${RULES_MK}>


netmem_msg.h netmem_msg_user.c netmem_msg_server.c :      netmem_msg.defs
	${_MIG_} ${_MIGFLAGS_} ${.ALLSRC}  -server netmem_msg_server.c \
				-user netmem_msg_user.c

netmem_msg_reply_user.c : 	netmem_msg_reply.defs
	${_MIG_} ${_MIGFLAGS_} ${.ALLSRC} -user netmem_msg_reply_user.c \
				-server /dev/null  
