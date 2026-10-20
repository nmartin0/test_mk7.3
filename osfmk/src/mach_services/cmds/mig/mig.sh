#!/bin/sh
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
#	Created.
#
#
# Mach Operating System
# Copyright (c) 1991,1990 Carnegie Mellon University
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

C=${MIGCC-cc}
M=${MIGCOM-/usr/ccs/lib/migcom}

cppflags=
migflags=
files=

# If an argument to this shell script contains whitespace,
# then we will screw up.  migcom will see it as multiple arguments.
#
# As a special hack, if -i is specified first we don't pass -user to migcom.
# We do use the -user argument for the dependencies.
# In this case, the -user argument can have whitespace.

until [ $# -eq 0 ]
do
    case "$1" in
	-[dtqkKQvVtTrRsSlLxX] ) migflags="$migflags $1"; shift;;
	-i	) sawI=1; migflags="$migflags $1 $2"; shift; shift;;
	-user   ) user="$2"; if [ ! "${sawI-}" ]; then migflags="$migflags $1 $2"; fi; shift; shift;;
	-server ) server="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-header ) header="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-sheader ) sheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-iheader ) iheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-dheader ) dheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-maxonstack ) migflags="$migflags $1 $2"; shift; shift;;
	-split ) migflags="$migflags $1"; shift;;
	-MD ) sawMD=1; cppflags="$cppflags $1"; shift;;
	-cpp) shift; shift;;
	-cc) C=$2; shift; shift;;
	-migcom) M=$2; shift; shift;;
	-* ) cppflags="$cppflags $1"; shift;;
	* ) files="$files $1"; shift;;
    esac
done

for file in $files
do
    base="`basename "$file" .defs`"
    temp="$base".$$
    rm -f "$temp".c "$temp".d
    (echo '#line 1 '\""$file"\"; cat "$file") > "$temp".c
    $C -E $cppflags "$temp".c | $M $migflags || exit
    if [ "$sawMD" -a -f "$temp".d ]
    then
	deps=
	s=
	rheader="${header-${base}.h}"
	if [ "$rheader" != /dev/null ]; then
		deps="${deps}${s}${rheader}"; s=" "
	fi
	ruser="${user-${base}User.c}"
	if [ "$ruser" != /dev/null ]; then
		deps="${deps}${s}${ruser}"; s=" "
	fi
	rserver="${server-${base}Server.c}"
	if [ "$rserver" != /dev/null ]; then
		deps="${deps}${s}${rserver}"; s=" "
	fi
	rsheader="${sheader-/dev/null}"
	if [ "$rsheader" != /dev/null ]; then
		deps="${deps}${s}${rsheader}"; s=" "
	fi
	riheader="${iheader-/dev/null}"
	if [ "$riheader" != /dev/null ]; then
		deps="${deps}${s}${riheader}"; s=" "
	fi
	rdheader="${dheader-/dev/null}"
	if [ "$rdheader" != /dev/null ]; then
		deps="${deps}${s}${rdheader}"; s=" "
	fi
	for target in ${deps}
	do
		sed -e 's;^'"${temp}"'.o[ 	]*:;'"${target}"':;' \
		    -e 's;: '"${temp}"'.c;: '"$file"';' \
		< "${temp}".d > "${target}".d
	done
	rm -f "$temp".d
    fi
    rm -f "$temp".c
done

exit 0
