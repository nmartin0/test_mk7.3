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
# This Makefile builds the ETAP utilities
#

PROGRAMS =	etap_trace etap_status etap_config	 \
		etap_md etap_mstat etap_mscan etap_mview \
		etap_cget 				 \
		etap_ttc				 \
		mview cview
# etap_cscan is broken

USE_STATIC_LIBRARIES =

CFLAGS	 =

ILIST    = ${PROGRAMS}
IDIR	 = ${MACH3_CMD_IDIR}

LIBS	 	   = -lmach
INCFLAGS	   = -I${TARGET_MACHINE}
INCDIRS		  := ${MACH3_INCDIRS} ${INCDIRS}

etap_md_LIBS	   = -lrthreads -lmach
etap_mstat_LIBS	   = -lm -lmach
etap_mscan_LIBS	   = -lm
etap_mview_LIBS	   = -lm -lmach
etap_cstat_LIBS	   = -lm
etap_ttc_LIBS	   = -lrthreads -lmach
mview_LIBS	   = -lm

etap_trace_OFILES  = etap_trace.o convert.o
etap_status_OFILES = etap_status.o
etap_config_OFILES = etap_config.o
etap_md_OFILES     = etap_md.o convert.o
etap_mstat_OFILES  = etap_mstat.o convert.o skiplist.o \
		     ${OBJECT_FORMAT}_sym_ops.o
etap_mscan_OFILES  = etap_mscan.o convert.o
etap_mview_OFILES  = etap_mview.o convert.o system_tables.o
etap_cget_OFILES   = etap_cget.o
etap_cstat_OFILES  = etap_cstat.o convert.o skiplist.o
etap_ttc_OFILES    = etap_ttc.o
mview_OFILES       = mview.o
cview_OFILES  	   = cview.o


.include <${RULES_MK}>
