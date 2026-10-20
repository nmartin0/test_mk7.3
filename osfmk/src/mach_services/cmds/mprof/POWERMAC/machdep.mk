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
# task_by_pid() is no longer part of libmach/libmach_sa.
# How should we replace it ?
#

${TARGET_MACHINE}_VPATH		= POWERMAC

SFILES	    	                = task_by_pid.o
${TARGET_MACHINE}_OFILES	= ${SFILES} elf.o

.SUFFIXES: .S .s

${SFILES:.o=.S}: $${@:.S=.s}
	${RM} ${_RMFLAGS_} ${.TARGET}
	${CP} ${.ALLSRC} ${.TARGET}

${SFILES}: $${@:.o=.S}
	${_CC_} -E ${_CCFLAGS_} -DASSEMBLER ${${.TARGET:.o=.S}:P} \
		> ${.TARGET:.o=.pp}
	${SED} '/^#/d' ${.TARGET:.o=.pp} > ${.TARGET:.o=.s}
	${_CC_} ${_CCFLAGS_} -c ${.TARGET:.o=.s}
	${RM} ${_RMFLAGS_} ${.TARGET:.o=.pp} ${.TARGET:.o=.s}
