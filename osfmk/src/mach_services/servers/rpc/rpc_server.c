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
 * cmk1.1
 */

/*
 *	File:	rpc_server.c
 *	Author:	Peter Watkins
 *
 *	Simple Mach RPC server for testing the new service. Eventually,
 *	all forms of Mach RPC will be testable.
 */	

#include <mach.h>
#include <stdio.h>
#include <mach_init.h>
#include <mach/rpc.h>
#include <mach/machine/vm_param.h>
#include <mig_errors.h>
#include <servers/netname.h>

#include "rpc_defs.h"
#include "rpc_sample_user.h"
#include "rpc_sample_server.h"

void			init();

mach_port_t		rpc_port;
mach_port_t		rpc_subsystem_port;
mach_port_t		rpc_server_activation;

vm_offset_t		stack;
vm_size_t		stack_size;

char			char_array[CHAR_ARRAY_SIZE];
fixed_array_small_t     f_array_small;
fixed_array_medium_t    f_array_medium;
fixed_array_medium_t    f_array_medium1;

mach_port_t		port_array_space[FIXED_ARRAY_SIZE_SMALL];
mach_port_t		port_array_space1[FIXED_ARRAY_SIZE_SMALL];

/*
 * Cache result below.
 */
boolean_t __in_kernel;
boolean_t __in_kernel_init = FALSE;

/*
 * Check for being collocated.
 */
#define in_kernel(addr)	(					\
	(__in_kernel) ? 1 : 					\
	(__in_kernel_init) ? 0 : 				\
	(__in_kernel_init = TRUE, 				\
		__in_kernel = (VM_MIN_KERNEL_ADDRESS <= (unsigned long)(addr))))

/*
 * Return to caller appropriately (according to whether or
 * not collocated).  Stash the return code in a caller-saves
 * register, so that the function return executed below (currently
 * only in collocated case) can't overwrite the code when
 * restoring callee-saves registers .
 */
#define return_from_rpc(v) 						\
do {	 								\
	extern char etext; 						\
									\
	if (in_kernel(&etext)) { 					\
		__asm__ volatile ("movl %0, %%ecx" : : "g" (v) ); 	\
		return; 						\
	}								\
	else { 								\
		__asm__ volatile ("movl %0, %%ecx" : : "g" (v) ); 	\
		mach_rpc_return_trap(); 				\
	}								\
	/* NOTREACHED */ 						\
} while (0)


/*
 *	Initialization routine for RPC server side. 
 */

void
init()
{
	kern_return_t	kr;


	stack_size = vm_page_size * 8;

	/*
	 *	Register our server subsystem with the kernel for upcalls.
	 */
	kr = mach_subsystem_create(mach_task_self(), 
				   (user_subsystem_t) &rpc_sample_subsystem,
				   sizeof(rpc_sample_subsystem),
				   &rpc_subsystem_port);
	
	if (kr != KERN_SUCCESS) {
		mach_error("rpc_server subsystem create:", kr);
		exit(-1);
	}

#ifdef  DEBUG
        printf("rpc_server: subsystem port 0x%x\n", rpc_subsystem_port);
#endif

	/*
	 *	Allocate port for this object.
	 */
	kr = mig_mach_port_allocate_subsystem(mach_task_self(), 
					      rpc_subsystem_port,
		  			      &rpc_port);

	if (kr != KERN_SUCCESS) {
		mach_error("rpc_server port alloc subsys:", kr);
		exit(-1);
	}

	/*
	 *	Allocate a stack for the server thread.
 	 */

	kr = vm_allocate(mach_task_self(), &stack, stack_size, TRUE);

        if (kr != KERN_SUCCESS) {
                mach_error("rpc_server stack vm_allocate:", kr);
                exit(-1);
        }
	
#ifdef  DEBUG
        printf("rpc_server: thread stack 0x%x\n", stack + stack_size);
#endif

	/*
	 *	Create an empty thread_activation to receive incoming
	 *	server requests.
	 */
	kr = thread_activation_create(mach_task_self(), rpc_port, 
				stack + stack_size, stack_size, 
				&rpc_server_activation);

        if (kr != KERN_SUCCESS) {
                mach_error("rpc_server empty thr_act create:", kr);
                exit(-1);
        }


	/*
         *      Now check it into the name service.  No signature port
         *      means this is unprotected.
         */
        kr = netname_check_in(name_server_port, RPC_NAME, MACH_PORT_NULL,
                              rpc_port);

        if (kr != NETNAME_SUCCESS) {
                mach_error("rpc_server check in:", kr);
                exit(-1);
        }

#ifdef  DEBUG
        printf("rpc_server: check in '%s' port 0x%x\n", RPC_NAME, rpc_port);
#endif
}



/*
 *	test_null
 *
 *	Test null rpc. No arguments, ports, out of line data, scatter/gather
 *	io, canned or copied signatures. Basically just tests thread migration
 *	and kernel upcall to the server.
 */

kern_return_t
test_null(mach_port_t object)
{
#ifdef  notdef
	printf("rpc_server: test_null\n");
#endif


	return_from_rpc(0);
	/* NOTREACHED */

	return 0;
}

/*
 *	test_simple
 *
 *	Test simple messages. No ports, out of line data, scatter/gather
 *	io, canned or copied signatures. Basically just tests argument 
 *	copying and kernel upcall to the server.
 */

kern_return_t
test_simple(mach_port_t object, int arg_one, int arg_two)
{
#ifdef  DEBUG
	printf("rpc_server: test_simple\n");
	if (arg_one != 67 || arg_two != 101)
        	printf("rpc_server: test_simple: passed arguments are wrong\n");
#endif


	return_from_rpc(0);
	/* NOTREACHED */

	return 0;
}

/*
 *	test_simple_inout
 */

kern_return_t
test_simple_inout(mach_port_t object, int *arg, short *sht)
{
        kern_return_t           kr = KERN_SUCCESS;

#ifdef  DEBUG
	printf("rpc_server: test_simple_inout\n");
#endif

	if (*arg != 23) {
		printf("rpc_server: test_simple_inout: bad in %d\n", *arg);
		kr = KERN_FAILURE;
		goto finish;
	}

	*arg = 29;

	if (*sht != 24) {
		printf("rpc_server: test_simple_inout: bad in %d\n", *sht);
		kr = KERN_FAILURE;
		goto finish;
	}

	*sht = 30;

finish:

	return_from_rpc(kr);
	/* NOTREACHED */

	return 0;
}



/*
 *	test_plain_recv_port
 *
 *	Test sending of a plain port (recv right).
 */

kern_return_t
test_plain_recv_port(mach_port_t object, mach_port_t test_port)
{
	kern_return_t		kr = KERN_SUCCESS;
	mach_port_type_t 	actual_type;

#ifdef 	DEBUG
	printf("rpc_server: test_plain_recv_port\n");
#endif

	if (!MACH_PORT_VALID(test_port)) {
		printf("rpc_server: test_plain_recv_port: invalid port\n");
		kr = KERN_FAILURE;
		goto finish;
	}

    	kr = mach_port_type(mach_task_self(), test_port, &actual_type);
	if (kr != KERN_SUCCESS) {
	      printf("rpc_server: test_plain_recv_port: mach_port_type: %s\n", 
		 	mach_error_string(kr));
	      goto finish;
	}

	if (actual_type != MACH_PORT_TYPE_RECEIVE) {
		printf("test_plain_recv_port: not a recv right\n");
                kr = KERN_FAILURE;
                goto finish;
	}

	kr = mach_port_destroy(mach_task_self(), test_port);
       	if (kr != KERN_SUCCESS) {
       		printf("rpc_server: mach_port_destroy: %s\n",
               		mach_error_string(kr));
		goto finish;
	}

finish:

	return_from_rpc(kr);
	/* NOTREACHED */

	return 0;
}


/*
 *	test_plain_send_port
 *
 *	Test sending of a plain port (send right).
 */

kern_return_t
test_plain_send_port(mach_port_t object, mach_port_t test_port)
{
	kern_return_t           kr = KERN_SUCCESS;
	mach_port_type_t        actual_type;


#ifdef DEBUG
	printf("rpc_server: test_plain_send_port\n");
#endif

	if (!MACH_PORT_VALID(test_port)) {
		printf("rpc_server: test_plain_send_port: invalid port\n");
		kr = KERN_FAILURE;
		goto finish;
	}

        kr = mach_port_type(mach_task_self(), test_port, &actual_type);
        if (kr != KERN_SUCCESS) {
	      printf("rpc_server: test_plain_send_port: mach_port_type: %s\n", 
                        mach_error_string(kr));
	      goto finish;
        }

        if (!(actual_type & MACH_PORT_TYPE_SEND)) {
	   printf("rpc_server: test_plain_send_port: not a send right 0x%x \n",
	          actual_type);
           kr = KERN_FAILURE;
           goto finish;
	}

finish:

	return_from_rpc(kr);
	/* NOTREACHED */

	return 0;
}


/*
 *	test_plain_send_port_out
 *
 *	Test returning of an integer and port (out args).
 */

kern_return_t
test_plain_send_port_out(mach_port_t object, int *test_int, 
			 mach_port_t *test_port)
{
	kern_return_t           kr = KERN_SUCCESS;
	mach_port_type_t        actual_type;


#ifdef DEBUG
	printf("rpc_server: test_plain_send_port_out\n");
#endif

	kr = mach_port_insert_right(mach_task_self(), rpc_port, rpc_port,
				    MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		printf("rpc_server: test_plain_send_port_out: inst fail: %s\n",
			mach_error_string(kr));
		goto finish;
	}

	*test_int = 99;

	*test_port = rpc_port;
	
finish:

	return_from_rpc(kr);
	/* NOTREACHED */

	return 0;
}

/*
 *	test_plain_port_inout
 *
 *	Test returning of an integer and port (out args).
 */

kern_return_t
test_plain_port_inout(mach_port_t object, mach_port_t *test_port)
{
	kern_return_t           kr = KERN_SUCCESS;
	mach_port_type_t        actual_type;


#ifdef DEBUG
	printf("rpc_server: test_plain_port_inout\n");
#endif

	/*
	 *	Check that the port sent to us is OK.
	 */
        if (!MACH_PORT_VALID(*test_port)) {
                printf("rpc_server: test_plain_port_inout: invalid port\n");
                kr = KERN_FAILURE;
                goto finish;
        }

        kr = mach_port_type(mach_task_self(), *test_port, &actual_type);
        if (kr != KERN_SUCCESS) {
              printf("rpc_server: test_plain_port_inout: mach_port_type: %s\n",
                        mach_error_string(kr));
              goto finish;
        }

        if (!(actual_type & MACH_PORT_TYPE_SEND)) {
           printf("rpc_server: test_plain_port_inout: not a send right 0x%x \n",
                  actual_type);
           kr = KERN_FAILURE;
           goto finish;
        }

	/*
	 *	Remove the send right we were sent.
	 */
	kr = mach_port_deallocate(mach_task_self(), *test_port);
       	if (kr != KERN_SUCCESS) {
       		printf("rpc_server: plain_port_inout: prt dealloc: %s\n",
			mach_error_string(kr));
	}

	/*
	 *	Now make a new port and return a send right to client.
	 */
	kr = mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_RECEIVE, test_port);
	if (kr != KERN_SUCCESS) {
       		printf("rpc_server: test_plain_port_inout: aloc\n");
               	printf("error %s\n", mach_error_string(kr));
	}

       	kr = mach_port_insert_right(mach_task_self(),
       				    *test_port, *test_port,
                                    MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
       		printf("rpc_server: test_plain_port_inout: ins\n");
               	printf("error %s\n", mach_error_string(kr));
	}

finish:

	return_from_rpc(kr);
	/* NOTREACHED */

	return 0;
}


/*
 *	test_fixed_array_small
 *
 *	Test small fixed arrays.
 */

kern_return_t
test_fixed_array_small(mach_port_t object, fixed_array_small_t farray)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_fixed_array_small\n");
#endif

	if ((char *)farray == NULL) {
		printf("rpc_server: test_fixed_array_small: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
		if (farray[i] != i) {
			printf("rpc_server: test_fixed_array_small: bad data\n");
			kr = KERN_FAILURE;
			goto finish;
		}
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	

/*
 *	test_fixed_array_medium
 *
 *	Test medium fixed arrays.
 */

kern_return_t
test_fixed_array_medium(mach_port_t object, fixed_array_medium_t farray)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_fixed_array_medium\n");
#endif

	if ((char *)farray == NULL) {
		printf("rpc_server: test_fixed_array_medium: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
		if (farray[i] != i) {
			printf("rpc_server: test_fixed_array_medium: bad data\n");
			kr = KERN_FAILURE;
			goto finish;
		}
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	

/*
 *	test_fixed_array_large
 *
 *	Test large fixed arrays.
 */

kern_return_t
test_fixed_array_large(mach_port_t object, fixed_array_large_t farray)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_fixed_array_large\n");
#endif

	if ((char *)farray == NULL) {
		printf("rpc_server: test_fixed_array_large: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < FIXED_ARRAY_SIZE_LARGE ; i++) {
		if (farray[i] != i) {
			printf("rpc_server: test_fixed_array_large: bad data\n");
			kr = KERN_FAILURE;
			goto finish;
		}
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	


/*
 *	test_fixed_array_out_small
 *
 *	Test small fixed arrays.
 */

kern_return_t
test_fixed_array_out_small(mach_port_t object, fixed_array_small_t farray)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_fixed_array_out_small 0x%x\n", farray);
#endif

	if ((char *)farray == NULL) {
		printf("rpc_server: test_fixed_array_out_small: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
		farray[i] = i;
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	

/*
 *	test_fixed_array_out_medium
 *
 *	Test medium fixed arrays.
 */

kern_return_t
test_fixed_array_out_medium(mach_port_t object, fixed_array_medium_t farray)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_fixed_array_out_medium\n");
#endif

	if ((char *)farray == NULL) {
		printf("rpc_server: test_fixed_array_out_medium: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
		farray[i] = i;
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	

/*
 *	test_fixed_array_out_large
 *
 *	Test large fixed arrays.
 */

kern_return_t
test_fixed_array_out_large(mach_port_t object, fixed_array_large_t farray)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_fixed_array_out_large \n");
#endif

	if ((char *)farray == NULL) {
		printf("rpc_server: test_fixed_array_out_large: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < FIXED_ARRAY_SIZE_LARGE ; i++) {
		farray[i] = i;
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	


/*
 *	test_bva_small
 *
 *	Test small bounded variable arrays
 */

kern_return_t
test_bva_small(mach_port_t object, bva_small_t array, unsigned int count,
	       bva_small_t out_array, unsigned int *outcnt)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_bva_small count=%d\n", count);
#endif


        if ((char *)array == NULL) {
                printf("rpc_server: test_bva_small: null arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

	if (count > FIXED_ARRAY_SIZE_SMALL) {
		printf("rpc_server: test_bva_small: count too big\n");
		count = FIXED_ARRAY_SIZE_SMALL;
	}

        for (i = 0 ; i < count ; i++) {
                if (array[i] != i) {
                        printf("rpc_server: test_bva_small: bad data\n");
                        kr = KERN_FAILURE;
                        goto finish;
                }
        }

	/*
	 *	Setup the out array.
	 */	
        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
		out_array[i] = FIXED_ARRAY_SIZE_SMALL - i;
	}

	*outcnt = FIXED_ARRAY_SIZE_SMALL;	

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	

/*
 *      test_uva
 *
 *      Test unbounded variable arrays
 */

kern_return_t
test_uva(mach_port_t object, uva_t array, mach_msg_type_number_t count)
{
        int                     i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
        printf("rpc_server: test_uva count=%d array=%x\n", count, array);
#endif


        if ((char *)array == NULL) {
                printf("rpc_server: test_uva: null arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

        for (i = 0 ; i < count ; i++) {
                if (array[i] != i) {
                        printf("rpc_server: test_uva: bad data\n");
                        kr = KERN_FAILURE;
                        goto finish;
                }
        }

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}

/*
 *      test_uva_pointer
 *
 *      Test unbounded variable arrays
 */

kern_return_t
test_uva_pointer(mach_port_t object, uva_pointer_t array, 
		 mach_msg_type_number_t count, uva_char_pointer_t char_array,
		 mach_msg_type_number_t char_cnt)
{
        int                     i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
        printf("rpc_server: test_uva_pointer count=%d array=%x\n", 
		count, array);
#endif


        if ((char *)array == NULL || char_array == NULL) {
                printf("rpc_server: test_uva_pointer: null arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

        for (i = 0 ; i < count ; i++) {
                if (array[i] != i) {
                        printf("rpc_server: test_uva_pointer: bad int data\n");
                        kr = KERN_FAILURE;
                        goto finish;
                }
        }

        for (i = 0 ; i < char_cnt ; i++) {
                if (char_array[i] != 'p') {
                        printf("rpc_server: test_uva_pointer: bad char data\n");
                        kr = KERN_FAILURE;
                        goto finish;
                }
        }

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}

/*
 *      test_uva_pointer_out
 *
 *      Test unbounded variable arrays
 */

kern_return_t
test_uva_pointer_out(mach_port_t object, uva_pointer_t *arrayp, 
		     mach_msg_type_number_t *count, uva_char_pointer_t *charp,
		     mach_msg_type_number_t *char_cnt)
{
        int                     i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
        printf("rpc_server: test_uva_pointer_out *count=%d arrayp=%x\n", 
		*count, arrayp);
#endif


        if ((char *)arrayp == NULL || charp == NULL) {
                printf("rpc_server: test_uva_pointer_out: null arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

	*count = FIXED_ARRAY_SIZE_SMALL;
	*arrayp = (uva_pointer_t) f_array_small;

        for (i = 0 ; i < (*count) ; i++) {
                (*arrayp)[i] = i;
        }

	*char_cnt = sizeof(char_array);
	*charp = (uva_char_pointer_t) char_array;

        for (i = 0 ; i < (*char_cnt) ; i++) {
                (*charp)[i] = 'q';
        }

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}

/*
 *      test_pointer_arrays
 *
 *      Test more arrays
 */

kern_return_t
test_pointer_arrays(mach_port_t object, fixed_array_pointer_t fix_ptr, 
		bva_pointer_t bva_ptr, mach_msg_type_number_t bva_size,
		bva_pointer_t *bva_ptr1, mach_msg_type_number_t *bva_cnt,
		uva_t *uva_ptr, mach_msg_type_number_t *uva_cnt)
{
        int                     i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
        printf("rpc_server: test_pointer_arrays count=%d arrayp=%x\n", 
		bva_size, bva_ptr);
#endif


        if ((char *)fix_ptr == NULL || bva_ptr == NULL || bva_ptr1 == NULL ||
	    uva_ptr == NULL || bva_cnt == NULL || uva_cnt == NULL ||
	    bva_size != FIXED_ARRAY_SIZE_MEDIUM) {
                printf("rpc_server: test_pointer_arrays: bad args\n");
                kr = KERN_FAILURE;
                goto finish;
        }

	for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
       		if (fix_ptr[i] != i || 
		    bva_ptr[i] != FIXED_ARRAY_SIZE_MEDIUM - i) {
			printf("rpc_server: test_pointer_arrays: ");
			printf("values i=%d %d %d\n", i, fix_ptr[i], 
				bva_ptr[i]);
			break;
		}
	}

	*bva_cnt = 112;
	*uva_cnt = 112;
	*bva_ptr1 = (bva_pointer_t) f_array_medium;
	*uva_ptr = (uva_t) f_array_medium1;

        for (i = 0 ; i < (*bva_cnt) ; i++) {
                (*bva_ptr1)[i] = i;
		(*uva_ptr)[i]  = 112 - i;
        }

finish:
        return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}

/*
 *      test_uva_pointer_out_dealloc
 *
 *      Test unbounded variable arrays, with dealloc
 */

kern_return_t
test_uva_pointer_out_dealloc(mach_port_t object, uva_pointer_t *arrayp, 
		     	     mach_msg_type_number_t *count)
{
        int                     i;
	vm_offset_t		data;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
        printf("rpc_server: test_uva_pointer_out_dealloc *count=%d arrayp=%x\n",
		*count, arrayp);
#endif


        if ((char *)arrayp == NULL) {
                printf("rpc_server: test_uva_pointer_out_dealloc: null arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

	*count = FIXED_ARRAY_SIZE_SMALL;

	kr = vm_allocate(mach_task_self(), &data, 
			 (vm_size_t) ((*count) * sizeof(int)), TRUE);
    	if (kr != KERN_SUCCESS) {
        	printf("rpc_server: test_uva_pointer_out_dealloc: ");
		printf("vm_allocate: %s\n", mach_error_string(kr));
		goto finish;
    	}

	*arrayp = (uva_pointer_t) data;

        for (i = 0 ; i < (*count) ; i++) {
                (*arrayp)[i] = i;
        }

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}




/*
 *      test_port_array_out
 *
 *      Test unbounded variable port arrays
 */

kern_return_t
test_port_array_out(mach_port_t object, port_array_t *arrayp, 
		    mach_msg_type_number_t *count,
		    uvpa_t *uvpa, mach_msg_type_number_t *uvpaCnt,
		    bvpa_t bvpa, mach_msg_type_number_t *bvpaCnt
			)
{
        int                     i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
        printf("rpc_server: test_port_array_out arrayp=%x\n", arrayp);
#endif

        if ((char *)arrayp == NULL || uvpa == NULL || bvpa == NULL) {
                printf("rpc_server: test_port_array_out: null ap arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

        if (count == NULL || uvpaCnt == NULL || bvpaCnt == NULL) {
                printf("rpc_server: test_port_array_out: null cnt arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

	/*
	 *	Build an array of ports
	 */
	*count = FIXED_ARRAY_SIZE_SMALL;
	*arrayp = (port_array_t) port_array_space;

        for (i = 0 ; i < (*count) ; i++) {
		kr = mach_port_allocate(mach_task_self(),
               				MACH_PORT_RIGHT_RECEIVE, 
					&( (*arrayp)[i] ));
		if (kr != KERN_SUCCESS) {
			printf("rpc_server: test_port_array_out: aloc\n");
			printf("error %s\n", mach_error_string(kr));
			goto finish;
		}

		kr = mach_port_insert_right(mach_task_self(),
					    (*arrayp)[i], (*arrayp)[i],
					    MACH_MSG_TYPE_MAKE_SEND);
		if (kr != KERN_SUCCESS) {
               		printf("rpc_server: test_port_array_out: ins\n");
                       	printf("error %s\n", mach_error_string(kr));
                       	goto finish;
		}
        }

        /*
         *      Build an array of ports
         */
        *uvpaCnt = FIXED_ARRAY_SIZE_SMALL;
        *uvpa = (port_array_t) port_array_space1;

        for (i = 0 ; i < (*uvpaCnt) ; i++) {
                kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE,
                                        &( (*uvpa)[i] ));
                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_out: aloc\n");
                        printf("error %s\n", mach_error_string(kr));
                        goto finish;
                }

                kr = mach_port_insert_right(mach_task_self(),
                                            (*uvpa)[i], (*uvpa)[i],
                                            MACH_MSG_TYPE_MAKE_SEND);
                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_out: ins\n");
                        printf("error %s\n", mach_error_string(kr));
                        goto finish;
                }
        }

        /*
         *      Build an array of ports
         */
        *bvpaCnt = FIXED_ARRAY_SIZE_SMALL;

        for (i = 0 ; i < (*bvpaCnt) ; i++) {
                kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE,
                                        &( (bvpa)[i] ));
                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_out: aloc\n");
                        printf("error %s\n", mach_error_string(kr));
                        goto finish;
                }

                kr = mach_port_insert_right(mach_task_self(),
                                            (bvpa)[i], (bvpa)[i],
                                            MACH_MSG_TYPE_MAKE_SEND);
                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_out: ins\n");
                        printf("error %s\n", mach_error_string(kr));
                        goto finish;
                }
        }

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}

/*
 *      test_port_array_in
 *
 *      Test bounded/unbounded variable port arrays
 */

kern_return_t 
test_port_array_in
	(
        mach_port_t object,
        uvpap_t uvpap,
        mach_msg_type_number_t uvpapCnt,
        uvpa_t uvpa,
        mach_msg_type_number_t uvpaCnt,
        bvpa_t bvpa,
        mach_msg_type_number_t bvpaCnt
	)
{

        int                     i;
        kern_return_t           kr = KERN_SUCCESS;
	mach_port_type_t 	actual_type;


#ifdef DEBUG
        printf("rpc_server: test_port_array_in \n");
#endif

        if (uvpap == NULL || uvpa == NULL || bvpa == NULL) {
                printf("rpc_server: test_port_array_in: null arg\n");
                kr = KERN_FAILURE;
                goto finish;
        }

        if (uvpapCnt != FIXED_ARRAY_SIZE_SMALL || 
	    uvpaCnt  != FIXED_ARRAY_SIZE_MEDIUM || 
	    bvpaCnt  != FIXED_ARRAY_SIZE_SMALL) {
                printf("rpc_server: test_port_array_in: bad count\n");
		printf(" %d %d %d \n", uvpapCnt, uvpaCnt, bvpaCnt);
                kr = KERN_FAILURE;
                goto finish;
        }


        /*
         *      Check array of ports
         */
	for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
       		kr = mach_port_type(mach_task_self(), uvpap[i],
                                    &actual_type);

		if (kr != KERN_SUCCESS) {
               		printf("rpc_server: test_port_array_in: ");
                       	printf("uvpap port type: %s\n", mach_error_string(kr));
			break;
		}

		if (!(actual_type & MACH_PORT_TYPE_SEND)) {
               		printf("rpc_server: test_port_array_in: ");
                       	printf("uvpap not send 0x%x\n", actual_type);
                       	break;
		}

               	kr = mach_port_deallocate(mach_task_self(), uvpap[i]);
		if (kr != KERN_SUCCESS) {
               		printf("rpc_server: test_port_array_in: ");
                       	printf("uvpap port dealloc: %s\n", 
				mach_error_string(kr));
			break;
		}
	}

        for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
                kr = mach_port_type(mach_task_self(), uvpa[i],
                                    &actual_type);

                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_in: ");
                        printf("uvpa port type: %s\n", mach_error_string(kr));
                        break;
                }

                if (!(actual_type & MACH_PORT_TYPE_SEND)) {
                        printf("rpc_server: test_port_array_in: ");
                        printf("uvpa not send 0x%x\n", actual_type);
                        break;
                }

                kr = mach_port_deallocate(mach_task_self(), uvpa[i]);
                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_in: ");
                        printf("uvpa port dealloc: %s\n",
                                mach_error_string(kr));
                        break;
                }
        }

        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                kr = mach_port_type(mach_task_self(), bvpa[i],
                                    &actual_type);

                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_in: ");
                        printf("bvpa port type: %s\n", mach_error_string(kr));
                        break;
                }

                if (!(actual_type & MACH_PORT_TYPE_SEND)) {
                        printf("rpc_server: test_port_array_in: ");
                        printf("bvpa not send 0x%x\n", actual_type);
                        break;
                }

                kr = mach_port_deallocate(mach_task_self(), bvpa[i]);
                if (kr != KERN_SUCCESS) {
                        printf("rpc_server: test_port_array_in: ");
                        printf("bvpa port dealloc: %s\n",
                                mach_error_string(kr));
                        break;
                }
        }


finish:
		return_from_rpc(kr);
        /* NOTREACHED */

        return kr;
}


/*
 *	test_string
 */

kern_return_t
test_string(mach_port_t object, string_t str_in, string_t str_out)
{
	int			i;
        kern_return_t           kr = KERN_SUCCESS;

#ifdef DEBUG
	printf("rpc_server: test_string\n");
#endif

	if ((char *)str_in == NULL) {
		printf("rpc_server: test_string: null arg\n");
		kr = KERN_FAILURE;
		goto finish;
	}

	for (i = 0 ; i < STRG_ARRAY_SIZE ; i++) {
		if (str_in[i] != 'z') {
			printf("rpc_server: test_string: bad data\n");
			kr = KERN_FAILURE;
			goto finish;
		}
	}

        for (i = 0 ; i < STRG_ARRAY_SIZE ; i++) {
                str_out[i] = 'y';
	}

finish:

        return_from_rpc(kr);
        /* NOTREACHED */

	return kr;
}	



/*
 *	Main loop
 */

#define RPC_MAX_MSG_SIZE	4096

boolean_t
rpc_demux(mach_msg_header_t inmsg, mach_msg_header_t outmsg)
{
	printf("rpc_server: msg recv'd inmsg 0x%x outmsg 0x%x\n",
		inmsg, outmsg);
	
	return KERN_SUCCESS;

}


main(int argc, char **argv)
{
	kern_return_t	kr;


	init();

	while (1) {
		
		kr = mach_msg_server(rpc_demux, RPC_MAX_MSG_SIZE, rpc_port, 
				     MACH_MSG_OPTION_NONE);
	}
}

