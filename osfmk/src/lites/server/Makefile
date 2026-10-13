# 
# Mach Operating System
# Copyright (c) 1992 Carnegie Mellon University
# Copyright (c) 1994 Johannes Helander
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON AND JOHANNES HELANDER ALLOW FREE USE OF THIS
# SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND JOHANNES
# HELANDER DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
# WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# HISTORY
# $Log: Makefile,v $
# Revision 1.1.1.1  1995/03/02  21:49:40  mike
# Initial Lites release from hut.fi
#
#

CONFIG		?=${LITES_${TARGET_MACHINE}_CONFIG:U${LITES_CONFIG:UDEFAULT}}

SUBDIRS		= conf ${CONFIG}

.include <${RULES_MK}>
