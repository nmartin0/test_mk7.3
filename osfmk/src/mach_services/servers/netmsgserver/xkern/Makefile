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

VPATH			= ${MAKETOP}mach_services/servers/netname:

PROGRAMS		= MachNetIPC ssr_xk nns_xk mni_proxy

ILIST			= ${PROGRAMS}
IDIR			= ${MACH3_SERVERS_IDIR}

MIG_DEFS		= netname.defs
MIGFLAGS		= -DMIG_SERVER
MIG_OFILES		= netnameServer.o
DATAFILES		= netname_defs.h netname.h ${MIG_DEFS}

GRAPH_COMP_OFILES	= protTbl.o protocols.o traceLevels.o
GRAPH_COMP_CFILES	= protTbl.c protocols.c traceLevels.c
GRAPH_COMP_HFILES	= protocols.h
GRAPH_COMP_FILES	= ${GRAPH_COMP_CFILES} ${GRAPH_COMP_HFILES}
GRAPH_COMP_FILES_IDIR	= ../../s_xkern

mni_OFILES		= hdr_utils.o port_maint.o			\
			  mach_xfer.o machmsg_int.o machnetipc.o	\
			  nns.o nns_boot.o nns_procs.o ssr.o		\
			  ${MIG_OFILES}	

MachNetIPC_OFILES 	= ${mni_OFILES}					\
			  ${GRAPH_COMP_OFILES:S/^/mni_/g}

mni_proxy_OFILES	= ${mni_OFILES}					\
			  ${GRAPH_COMP_OFILES:S/^/proxy_/g}

ssr_xk_OFILES		= hdr_utils.o port_maint.o			\
			  mach_xfer.o machmsg_int.o machnetipc.o ssr.o	\
			  ${GRAPH_COMP_OFILES:S/^/ssr_/g}

nns_xk_OFILES		= nns_sa.o nns_boot_sa.o nns_procs.o		\
			  ${MIG_OFILES}					\
			  ${GRAPH_COMP_OFILES:S/^/nns_/g}

INCFLAGS		= -I../.. -I${MAKETOP}

#DEBUG_FLAGS		= -DXK_DEBUG=1 -g
#DEBUG_FLAGS		= -DXK_DEBUG=0 -DXK_PROFILE
DEBUG_FLAGS		= -DXK_DEBUG=0
DEFINES 		= ${DEBUG_FLAGS}			\
			  -DPORTLOCKS -DPORTBLOCK		\
			  -DMAKESENDCOUNT -DNOMORESENDERS
CFLAGS			+= -ansi ${DEFINES}
protTbl.o_CFLAGS	= -DPROTOCOL_TABLE=\"prottbl\" ${CFLAGS}
nns_sa.o_CFLAGS		= -DMACH_STAND_ALONE ${CFLAGS}
nns_boot_sa.o_CFLAGS	= -DMACH_STAND_ALONE ${CFLAGS}

NO_SHARED_LIBRARIES	= 0
XKERN_LIBS		= -lxk_proxy -lxkern -ls_xkern -lxkern
MACH_SA_LIBS		= -lsa_mach -lmach
MACH_LIBS		= -lcthreads -lnetname -lservice -lmach
STD_LIBS		= -lc
LIBS			= ${XKERN_LIBS} ${MACH_LIBS} ${STD_LIBS}
nns_xk_LIBS		= ${XKERN_LIBS} ${MACH_LIBS} ${MACH_SA_LIBS}

GARBAGE			= netnameServer.c netnameServer.o	\
			  ${GRAPH_COMP_FILES:S/^/mni_/g}	\
			  ${GRAPH_COMP_FILES:S/^/mni_/g:.c=.o}	\
			  ${GRAPH_COMP_FILES:S/^/proxy_/g}	\
			  ${GRAPH_COMP_FILES:S/^/proxy_/g:.c=.o} \
			  ${GRAPH_COMP_FILES:S/^/ssr_/g}	\
			  ${GRAPH_COMP_FILES:S/^/ssr_/g:.c=.o}	\
			  ${GRAPH_COMP_FILES:S/^/nns_/g}	\
			  ${GRAPH_COMP_FILES:S/^/nns_/g:.c=.o}	\
			  ${GRAPH_COMP_FILES} protocols.sed	\
			  ${GRAPH_COMP_FILES:.c=.o}		\
			  nns_sa.c nns_sa.o nns_boot_sa.c nns_boot_sa.o


.include <${RULES_MK}>

${GRAPH_COMP_FILES:S/^/mni_/g} : gen/graph.comp
	compose -f < ${gen/graph.comp:P}
	${SED}	's/protocols\.h/mni_protocols.h/' ${.TARGET:S/^mni_//} > ${.TARGET}
	${CP}	protocols.h ${GRAPH_COMP_FILES_IDIR}/mni_protocols.h

${GRAPH_COMP_FILES:S/^/proxy_/g} : gen/graph.proxy
	compose -f < ${gen/graph.proxy:P}
	${SED}	's/protocols\.h/proxy_protocols.h/' ${.TARGET:S/^proxy_//} > ${.TARGET}
	${CP}	protocols.h ${GRAPH_COMP_FILES_IDIR}/proxy_protocols.h

${GRAPH_COMP_FILES:S/^/ssr_/g} : gen/graph.ssr
	compose -f < ${gen/graph.ssr:P}
	${SED}	's/protocols\.h/ssr_protocols.h/' ${.TARGET:S/^ssr_//} > ${.TARGET}
	${CP}	protocols.h ${GRAPH_COMP_FILES_IDIR}/ssr_protocols.h

${GRAPH_COMP_FILES:S/^/nns_/g} : gen/graph.nns
	compose -f < ${gen/graph.nns:P}
	${SED}	's/protocols\.h/nns_protocols.h/' ${.TARGET:S/^nns_//} > ${.TARGET}
	${CP}	protocols.h ${GRAPH_COMP_FILES_IDIR}/nns_protocols.h

nns_sa.c : nns.c
	${CP}	${nns.c:P} ${.TARGET}

nns_boot_sa.c : nns_boot.c
	${CP}	${nns_boot.c:P} ${.TARGET}

