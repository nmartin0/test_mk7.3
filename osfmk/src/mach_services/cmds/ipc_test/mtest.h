/*
 * Copyright 1991-1998 by Open Software Foundation, Inc. 
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * cmk1.1
 */

#ifndef _MTEST_H_
#define _MTEST_H_

extern void complex_bit_test_1();
extern void complex_bit_test_2();
extern void complex_bit_test_3();

extern void bad_pointer_test();
extern void zero_pointer_test();
extern void bad_port_test();

extern void deallocate_bit_test_1();
extern void deallocate_bit_test_2();
extern void deallocate_bit_test_3();

extern void too_large_test_1();
extern void too_large_test_2();

extern void null_dest_test();
extern void msg_queue_test();
extern void null_port_test();
extern void dead_port_test();

extern void add_member_test();
extern void remove_member_test();

extern void enabled_receive_test_1();
extern void enabled_receive_test_2();
extern void enabled_receive_test_3();

extern void rcv_notify_test_1();
extern void rcv_notify_test_2();
extern void rcv_notify_test_3();
extern void rcv_notify_test_4();
extern void rcv_notify_test_5();

extern void send_notify_test();

extern void send_cancel_test_1();
extern void send_cancel_test_2();
extern void send_cancel_test_3();

extern void circular_test_1();
extern void circular_test_2();
extern void circular_test_3();
extern void circular_test_4();

extern void no_memory_test_1();
extern void no_memory_test_2();
extern void no_memory_test_3();

extern void no_space_test_1();
extern void no_space_test_2();

extern void header_ports_test();

#endif	/* _MTEST_H_ */
