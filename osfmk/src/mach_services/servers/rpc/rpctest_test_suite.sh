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

#
# Try a number of basic thread termiate scenarios
#

echo "Exercise basic test scenarios"

rpctest=./rpctest
typeset -i testno=1
typeset pid=${$}
typeset U=u_${pid}
typeset V=v_${pid}
typeset X=x_${pid}
typeset Y=y_${pid}
typeset Z=z_${pid}

echo "*** TEST "$testno\
". Kill head activation in a (different) 3-activation RPC chain."
$rpctest ${Z} &
$rpctest ${Y} &
sleep 2
$rpctest ${X} "call ${Y} call ${Z} spin" &
sleep 2
$rpctest ${U} "kill ${Z}_1" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${U} term t_${V}" &


sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno\
". Kill middle activation in a (different) 3-activation RPC chain."
$rpctest ${Z} &
$rpctest ${Y} &
sleep 2
$rpctest ${X} "call ${Y} call ${Z} spin" &
sleep 2
$rpctest ${U} "kill ${Y}_1" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${U} term t_${V}" &


sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno\
". Kill root activation in a (different) 3-activation RPC chain."
$rpctest ${Z} &
$rpctest ${Y} &
sleep 2
$rpctest ${X} "call ${Y} call ${Z} spin" &
sleep 2
$rpctest ${U} "kill ${X}_0" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${U} term t_${V}" &


sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno\
". Kill head activation in same 3-activation RPC chain.  (Suicide)"
$rpctest ${Z} &
$rpctest ${Y} &
sleep 2
$rpctest ${X} "call ${Y} call ${Z} kill ${Z}_1" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${V}" &


sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno\
". Kill middle activation in same 3-activation RPC chain.  (Suicide)"
$rpctest ${Z} &
$rpctest ${Y} &
sleep 2
$rpctest ${X} "call ${Y} call ${Z} kill ${Y}_1" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${V}" &


sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno\
". Kill root activation in same 3-activation RPC chain.  (Suicide)"
$rpctest ${Z} &
$rpctest ${Y} &
sleep 2
$rpctest ${X} "call ${Y} call ${Z} kill ${X}_0" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${V}" &


#
# NOTE: The two tests that follow must be kept together in this order.
#       They depend on having state preserved between them.
#
sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno". Kill empty activation."
$rpctest ${Z} &
sleep 2
$rpctest ${Y} "kill ${Z}_1" &

sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno\
". Kill an activation that is blocked doing an RPC upcall."
$rpctest ${X} "call ${Z} spin" &
sleep 2
$rpctest ${U} "kill ${X}_0" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${X} term t_${U} term t_${V}" &


sleep 3
testno=$testno+1
echo ""
echo "*** TEST "$testno". Kill simple rooted activation."
$rpctest ${Z} &
sleep 2
$rpctest ${Y} "kill ${Z}_0" &

# clean up
sleep 2
$rpctest ${V} "term t_${Z} term t_${Y} term t_${V}" &


# sleep 3
# testno=$testno+1
# echo ""
# echo "*** TEST "$testno\
# ". Kill middle activation in a (different) 3-activation RPC chain."

sleep 3
