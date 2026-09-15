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
/*
 * No declaration of malloc or free here, deliberately.
 *
 * LITES declares malloc two different ways itself:
 *
 *   emulator/e_mach_msg_server.c:40   void *malloc(unsigned int);
 *   server/serv/server_defs.h:101     void *malloc(size_t);
 *
 * and its size_t is long unsigned int, so the two disagree. That is
 * harmless as long as each translation unit sees only its own, which
 * is the case -- until a declaration is injected into every file from
 * here, at which point one of the two always conflicts.
 *
 * A third convention exists too: server/ufs/ffs/ffs_inode.c and others
 * declare nothing at all and reach malloc only through the MALLOC and
 * FREE macros in sys/malloc.h, so they need a declaration from
 * somewhere.
 *
 * No prototype can satisfy all three. A K&R declaration with an empty
 * parameter list can: it is compatible with any later prototype, so
 * each file's own declaration still refines it, and files that declare
 * nothing get one. -std=gnu89 accepts it without complaint.
 *
 * The declarations must precede the macros, or the function-like macro
 * rewrites them. The macros are written so that a later prototype
 * survives expansion unchanged: "void *malloc(size_t);" becomes
 * "void *(malloc)(size_t);", which is legal C.
 */
extern void *malloc();
extern void  free();

#define malloc(sz, ...)  (malloc)(sz)
#define free(p, ...)     (free)(p)

/*
 * cthread_mach_msg.
 *
 * LITES supplies its own implementation in server/serv/cprocs.c with
 * the nine-argument signature old cthreads had -- the comment above its
 * declaration in ux_server_loop.c even says "These are missing from
 * cthreads". OSFMK 7.3's libcthreads does provide one, but collapsed
 * into a single struct argument:
 *
 *	kern_return_t cthread_mach_msg(struct cthread_mach_msg_struct *);
 *
 * whose members map one to one onto LITES's nine arguments. Nothing
 * calls ours here, so the two only collide as declarations.
 *
 * Pull cthreads.h in now with our name renamed out of the way. Its
 * include guard makes every later #include a no-op, so LITES's own
 * declaration and definition stand unopposed.
 */
#define cthread_mach_msg osfmk_cthread_mach_msg
#include <cthreads.h>
#undef cthread_mach_msg
