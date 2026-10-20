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
 * 
 */
/*
 * cmk1.1
 */

#ifndef s_xkern_assert_h
#define s_xkern_assert_h

#define assertMessage "Assertion failed: file %s, line %d\n"

extern void Kabort(char *);
#define ABORT Kabort
#define PRINT(A,B,C) fprintf(stderr, (A), (B), (C))

#if XK_DEBUG
#   define _xAssert(ex) { if ( ! (ex) ) PRINT(assertMessage, __FILE__, __LINE__), ABORT("after assert"); }
#   define xAssert(ex) { if ( ! (ex) ) PRINT(assertMessage, __FILE__, __LINE__), ABORT("after assert"); }
#else
#   define _xAssert(ex) {}
#   define xAssert(ex) {}
#endif /* XK_DEBUG */

#endif /* s_xkern_assert_h */
