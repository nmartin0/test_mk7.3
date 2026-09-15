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
