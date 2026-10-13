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

VPATH			= ${TARGET_MACHINE}

SETUP_PROGRAMS		= config

PROGRAMS		= config

INCFLAGS		= -I.
CFLAGS			= -D_ANSI_C_SOURCE

OFILES			= main.o parse.o gram.o mkioconf.o mkmakefile.o \
			  mkheaders.o mkswapconf.o machdep.o
HFILES			= gram.h config.h
LIBS			= -lln

.include <${RULES_MK}>
.if exists(${TARGET_MACHINE}/machdep.mk)
.include "${TARGET_MACHINE}/machdep.mk"
.endif

gram.o: gram.c

.ORDER: gram.c gram.h
