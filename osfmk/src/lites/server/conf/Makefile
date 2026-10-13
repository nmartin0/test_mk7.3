#
# Mach Operating System
# Copyright (c) 1992,1991,1990,1989 Carnegie Mellon University
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 1.1.1.1  1995/03/02  21:49:41  mike
# Initial Lites release from hut.fi
#
# $EndLog$
# 
#	File:	conf/Makefile
#	Author:	Mary Thompson
#	Date:	1992
#

MASTER		= MASTER
MASTER_TM	= ${target_cpu}/MASTER
MASTER_LOCAL	= MASTER.local
MASTER_TM_LOCAL	= ${target_cpu}/MASTER.local

CONFIG		?=${LITES_${TARGET_MACHINE}_CONFIG:U${LITES_CONFIG:UDEFAULT}}

.if defined (PROFILING)
_CONFIG_	= ${CONFIG:S/+gprof//}+gprof
.else
_CONFIG_	= ${CONFIG}
.endif

VMUNIX		= startup.${VERSION}.${_CONFIG_}
OTHERS		= ../${_CONFIG_}/Makefile

.include <${RULES_MK}>

${_CONFIG_}:	${MASTER} ${MASTER_TM} \
		${MASTER_LOCAL} ${MASTER_TM_LOCAL} 
	@echo "[ generating ${.TARGET} from {,${target_cpu}/}MASTER{,.local} ]"
	@doconf="`wh -q doconf`"; \
	echo "${.TARGET}: $${doconf}" >> ${.TARGET}.d; \
	$${doconf} ${_CONFIG_}\
		${${MASTER}:P}\
		${${MASTER_TM}:P}\
		${${MASTER_LOCAL}:P}\
		${${MASTER_TM_LOCAL}:P} >${_CONFIG_}.tmp
	@-if [ -s ${_CONFIG_}.tmp ]; then \
	    if [ -f ${_CONFIG_} ]; then \
		diff ${_CONFIG_} ${_CONFIG_}.tmp; \
		mv ${_CONFIG_} ${_CONFIG_}.old; \
	    fi; \
	    mv ${_CONFIG_}.tmp ${_CONFIG_}; \
	else \
	    ${RM} -f ${_CONFIG_}.tmp; \
	fi

../${_CONFIG_}/Makefile:	${_CONFIG_} \
			files \
			${target_cpu}/files \
			template.mk \
			${target_cpu}/template.mk 
	@echo "[ configuring ${_CONFIG_} ]"
	@-if [ -d ../${_CONFIG_} ]; then true; else mkdir ../${_CONFIG_}; fi
	@config="`wh -q config`"; \
	echo "${.TARGET}: $${config}" >> ${.TARGET}.d; \
	$${config} `genpath -I.` -c . ${_CONFIG_}
	@${RM} -f ../${_CONFIG_}/Makefile
	@ln -s Makefile.internal ../${_CONFIG_}/Makefile
	@echo ${_CONFIG_} >../${_CONFIG_}/vers.config
	@${RM} -f ../${_CONFIG_}/${VMUNIX}


