/*
 * Compatibility shim for building LITES 1.1u3 against OSFMK 7.3.
 * Injected with -include; neither project is modified.
 *
 * OSFMK 7.3 uses untyped (NDR) IPC, in which the MIG error reply is
 * mig_reply_error_t -- a Head, an NDR_record_t and a RetCode. The
 * typed-IPC name LITES still uses in a few places, mig_reply_header_t,
 * had a mach_msg_type_t in place of the NDR record.
 *
 * LITES touches the differing member (RetCodeType) only inside its
 * #else arm for typed IPC, which UNTYPED_IPC compiles out, so the two
 * structures are interchangeable for every use that remains.
 */
#ifndef _LITES_OSFMK_COMPAT_H_
#define _LITES_OSFMK_COMPAT_H_
#include <mach/mig_errors.h>
typedef mig_reply_error_t mig_reply_header_t;
#endif

/*
 * BSD kernel malloc arity.
 *
 * LITES's include/sys/malloc.h supplies MALLOC, FREE, bsd_malloc and
 * bsd_free, all resolving to a one-argument malloc, because the server
 * links against a normal allocator rather than a BSD kernel one. But 53
 * call sites across 46 BSD-derived files in server/net, server/netccitt
 * and server/isofs were never converted and still call
 *
 *	malloc(size, type, flags)
 *	free(addr, type)
 *
 * directly. Converting them all would be a large patch against LITES.
 * These macros drop the extra arguments instead, so the existing calls
 * compile unchanged. The parentheses around the function names stop the
 * macro recursing into its own expansion.
 *
 * Both arities work: malloc(n) and malloc(n, M_RTABLE, M_DONTWAIT) both
 * reach the one-argument allocator.
 */
extern void *malloc(unsigned long);
extern void  free(void *);

#define malloc(sz, ...)  (malloc)(sz)
#define free(p, ...)     (free)(p)
