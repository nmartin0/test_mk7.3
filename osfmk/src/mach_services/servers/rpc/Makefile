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

#
#	Build the sample RPC server and client
#

MIGFLAGS	       += -t

CFLAGS		       += -O
RPCTEST_CFLAGS		= ${CFLAGS} -DAUTOTEST

USE_STATIC_LIBRARIES	=

PROGRAMS		= rpc_server rpc_client rpctest

ILIST			= rpc
IDIR			= ${MACH3_SERVERS_IDIR}

rpctest_LIBS		= -lnetname -lrthreads -lmach
rpc_server_LIBS		= -lnetname -lmach
rpc_client_LIBS		= -lnetname -lmach
rpc_server_OFILES	= rpc_server.o rpc_sampleServer.o
rpc_client_OFILES	= rpc_client.o rpc_sampleUser.o
rpctest_OFILES		= rpctest.o rpctestFunctions.o rpctestUser.o \
			  rpctestServer.o

rpctest.o_CFLAGS		= ${RPCTEST_CFLAGS}
rpctestFunctions.o_CFLAGS	= ${RPCTEST_CFLAGS}
rpctestUser.o_CFLAGS		= ${RPCTEST_CFLAGS}
rpctestServer.o_CFLAGS		= ${RPCTEST_CFLAGS}

MIG_DEFS		= rpc_sample.defs rpctest.defs

DATAFILES		= rpc_sample_user.h rpc_sample_server.h \
			  rpctest.h rpctestServer.h rpctestUser.h \
			  ${MIG_DEFS}

INCLUDES		= ${DATAFILES}

EXPDIR			= ${MACH3_INCLUDE_IDIR}servers/

.include <${RULES_MK}>


rpc_sampleServer.c rpc_sample_user.h rpc_sample_server.h :	rpc_sample.defs
	${_MIG_} ${_MIGFLAGS_} ${.ALLSRC} -server rpc_sampleServer.c 	\
	-user rpc_sampleUser.c -sheader rpc_sample_server.h 		\
	-header rpc_sample_user.h

rpctestServer.c rpctestUser.c rpctest.h rpctestServer.h rpctestUser.h :	rpctest.defs
	${_MIG_} ${_MIGFLAGS_} ${.ALLSRC} -server rpctestServer.c \
	-user rpctestUser.c -sheader rpctestServer.h -header rpctestUser.h

