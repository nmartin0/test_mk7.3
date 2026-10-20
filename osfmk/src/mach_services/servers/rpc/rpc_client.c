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
 *	File: rpc_client.c
 *	Author: Peter Watkins
 *
 * 	Sample client for Mach RPC service testing.
 */

#include <mach.h>
#include <mach_init.h>
#include <mach/rpc.h>
#include <servers/netname.h>
#include <stdio.h>
#include <sys/time.h>

#include "rpc_defs.h"

#if 0
#define NITER		6000
#define TPRINTF(s,a)	printf(s,a)
#else
#define NITER	1
#define TPRINTF(s,a)
#endif


void timevalsub();
void timevalmul();
void timevalfix();

char				char_array[CHAR_ARRAY_SIZE];

fixed_array_small_t		f_array_small;
fixed_array_small_t		f_array_small1;
fixed_array_medium_t		f_array_medium;
fixed_array_large_t		f_array_large;
fixed_array_pointer_t		f_array_ptr;

bva_small_t			bva_small;
bva_small_t			bva_out;
bva_pointer_t			bva_pointer;
bva_pointer_t			bva_pointer1;

uva_t				uva;
uva_pointer_t			uva_pointer;
uva_char_pointer_t		uva_char_ptr;

uvpa_t				uvpa;
bvpa_t				bvpa;

string_t			string;



/*
 *	Lookup server port and initiate RPC.
 */

main( int argc, char **argv )
{

	kern_return_t		kr;
	mach_port_t		server_port;
	mach_rpc_signature_t	sig_ptr;
	mach_rpc_size_t		sig_buf_size;


	kr = netname_look_up(name_server_port, "", RPC_NAME, &server_port);

	if (kr != NETNAME_SUCCESS) {
		mach_error("rpc_client: server lookup failure", kr);
		exit(-1);
	}

#ifdef  DEBUG
        printf("rpc_client: server_port '%s' 0x%x\n", RPC_NAME, server_port);
#endif

	for (;;) {

		register int i;
		short test_short;
		port_array_t port_array;
		int  c, test_int, count, char_cnt;	
		int uva_count, bva_count;
		int uvpaCnt, bvpaCnt;
		string_t string2;
		double t;
		char cmd;
		mach_port_t test_port;
        	mach_port_type_t actual_type;
		struct timeval ts, te, *tsp = &ts, *tep = &te;

		printf("rpc> ");
		scanf("%c", &cmd);

		switch (cmd) {

		   case 'n':		/* null trap */
			printf("rpc_client: testing null trap\n");
	
			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--)
				kr = mach_task_self();	

			gettimeofday(tep, 0);

                       	timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        TPRINTF("rpc_client: null trap %f usec/call \n", t);

			break;

		   case 'r':		/* null rpc */
			printf("rpc_client: testing null rpc \n");

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
#undef 	RPC_TRAP_REGISTERS
#if	defined(RPC_TRAP_REGISTERS)
                       __asm__ volatile ("movl %0, %%esi;	\
				          movl %1, %%edi;	\
				     	  movl %2, %%ecx;	\
				          movl %3, %%edx;	\
				          call _mach_rpc_trap "
				: /* no outputs */
				: "m" (sig_buf_size), "m" (sig_ptr), "a" (600), 
			          "m" (server_port)
				: "esi", "edi", "ecx", "edx"
				);
#elif	defined(RPC_TRAP_CALL)
				kr = mach_rpc_trap(server_port, 600,
						sig_ptr, sig_buf_size);
#else
				kr = test_null(server_port);
#endif	
			}

			gettimeofday(tep, 0);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: null rpc %d return \n", kr);

			TPRINTF("rpc_client: null rpc %f usec/call \n", t);

			break;

		   case 's':		/* simple rpc */
			printf("rpc_client: testing simple rpc \n");

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
#if     defined(RPC_TRAP_REGISTERS)
                       		__asm__ volatile ("movl %0, %%esi;	\
				          movl %1, %%edi;		\
				     	  movl %2, %%ecx;		\
				          movl %3, %%edx;		\
				          call _mach_rpc_trap "
				: /* no outputs */
				: "m" (sig_buf_size), "m" (sig_ptr), "a" (601), 
			          "m" (server_port)
				: "%esi", "%ebx", "%ecx", "%edx" );
#elif   defined(RPC_TRAP_CALL)
				kr = mach_rpc_trap(server_port, 601, 
						   sig_ptr, sig_buf_size);
#else
				kr = test_simple(server_port, 67, 101);
#endif
			}
				

			gettimeofday(tep, 0);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: simple rpc %d return\n",  kr);

			TPRINTF("rpc_client: simple rpc %f usec/call \n", t);

			break;

		   case 'x':		/* simple inout rpc */
			printf("rpc_client: testing simple rpc \n");

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				test_int = 23;
				test_short = 24;
				kr = test_simple_inout(server_port, &test_int,
						       &test_short); 
			}

			gettimeofday(tep, 0);

			if (test_int != 29 || test_short != 30) {
				printf("rpc_client: test_simple_inout:bd int ");
				printf("%d\n", test_int);
			}

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: simple rpc inout %d return\n",  kr);

			TPRINTF("rpc_client: simple rpc inout %f usec/call \n", t);

			break;

		   case 'p':		/* plain port  - send */
			printf("rpc_client: testing rpc plain port - send\n");

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_plain_send_port(server_port, 
							  server_port);
			}

			gettimeofday(tep, 0);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: plain port send %d return\n",  kr);

			TPRINTF("rpc_client: plain port send %f usec/call\n",t);

			break;

		   case 't':		/* plain port - recv */
			printf("rpc_client: testing rpc plain port - recv\n");

			kr = mach_port_allocate(mach_task_self(),
                            		MACH_PORT_RIGHT_RECEIVE, &test_port);

    			if (kr != KERN_SUCCESS) {
				printf("rpc_client: mach_port_allocate: %s\n",
					mach_error_string(kr));
				exit(1);
    			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_plain_recv_port(server_port, 
							  test_port);
			}

			gettimeofday(tep, 0);

			(void) mach_port_destroy(mach_task_self(), test_port);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: plain port rcv %d return\n",  kr);

			TPRINTF("rpc_client: plain port rcv %f usec/call\n",t);

			break;

		   case 'u':		/* plain port - send out */
			printf("rpc_client: testing rpc plain port send out\n");

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_plain_send_port_out(server_port, 
						&test_int, &test_port);
			}

			gettimeofday(tep, 0);

        		kr = mach_port_type(mach_task_self(), test_port, 
					    &actual_type);
        		if (kr != KERN_SUCCESS) {
              			printf("rpc_client: tst snd out: prt typ: %s\n",
                        		mach_error_string(kr));
              			exit(1);
        		}

        		if (!(actual_type & MACH_PORT_TYPE_SEND)) {
           			printf("rpc_client: snd out: not snd 0x%x\n",
                  			actual_type);
           			exit(1);
        		}

			kr = mach_port_deallocate(mach_task_self(), test_port);
                        if (kr != KERN_SUCCESS) {
                                printf("rpc_client: tst snd out: prt dec: %s\n",
                                        mach_error_string(kr));
                                exit(1);
                        }
#if 1
			if (test_int != 99) {
                                printf("rpc_client: tst snd out: wrong int\n");
                                exit(1);
                        }
#endif			
			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: plain port send out %d return\n",  kr);

			TPRINTF("rpc_client: plain port send out %f usec/call\n",t);

			break;

		   case 'w':		/* plain port - in out */
			printf("rpc_client: testing rpc plain port in out\n");

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			kr = mach_port_allocate(mach_task_self(), 
					MACH_PORT_RIGHT_RECEIVE, &test_port);
                        if (kr != KERN_SUCCESS) {
                           printf("rpc_client: test_plain_port_inout: aloc\n");
                           printf("error %s\n", mach_error_string(kr));
                           exit(1);
                        }

        		kr = mach_port_insert_right(mach_task_self(), 
					test_port, test_port, 
					MACH_MSG_TYPE_MAKE_SEND);
        		if (kr != KERN_SUCCESS) {
                	   printf("rpc_client: test_plain_port_inout: ins\n");
			   printf("error %s\n", mach_error_string(kr));
                	   exit(1);
        		}

			for (i = NITER ; i ; i--) {
				kr = test_plain_port_inout(server_port, 
						&test_port);
			}

			gettimeofday(tep, 0);

        		kr = mach_port_type(mach_task_self(), test_port, 
					    &actual_type);
        		if (kr != KERN_SUCCESS) {
              			printf("rpc_client: test_plain_port_inout: prt typ: %s\n",
                        		mach_error_string(kr));
              			exit(1);
        		}

        		if (!(actual_type & MACH_PORT_TYPE_SEND)) {
           			printf("rpc_client: test_plain_port_inout: not snd 0x%x\n",
                  			actual_type);
           			exit(1);
        		}

			kr = mach_port_deallocate(mach_task_self(), 
						  test_port);
                        if (kr != KERN_SUCCESS) {
                                printf("rpc_client: test_plain_port_inout: prt dec: %s\n",
                                        mach_error_string(kr));
                                exit(1);
                        }

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: test_plain_port_inout %d return\n",  kr);

			TPRINTF("rpc_client: test_plain_port_inout %f usec/call\n",t);

			break;

		   case 'f':		/* fixed array  -  small */
			printf("rpc_client: testing small fixed array \n");

			for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
				f_array_small[i] = i;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_fixed_array_small(server_port, 
							    f_array_small);
			}

			gettimeofday(tep, 0);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: small fixed array %d return\n", kr);

			TPRINTF("rpc_client: small fixed array %f usec/call\n",
				t);

			break;

		   case 'g':		/* fixed array  -  medium */
			printf("rpc_client: testing medium fixed array \n");

			for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
				f_array_medium[i] = i;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_fixed_array_medium(server_port, 
							    f_array_medium);
			}

			gettimeofday(tep, 0);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: medium fixed array %d return\n",kr);

			TPRINTF("rpc_client: medium fixed array %f usec/call\n",
				t);

			break;

		   case 'h':		/* fixed array  -  large */
			printf("rpc_client: testing large fixed array \n");

			for (i = 0 ; i < FIXED_ARRAY_SIZE_LARGE ; i++) {
				f_array_large[i] = i;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_fixed_array_large(server_port, 
							    f_array_large);
			}

			gettimeofday(tep, 0);

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: large fixed array %d return\n",kr);

			TPRINTF("rpc_client: large fixed array %f usec/call\n",
				t);

			break;

		   case 'i':		/* fixed array out  -  small */
			printf("rpc_client: testing small fixed array out \n");

			for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
				f_array_small[i] = 0;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_fixed_array_out_small(server_port, 
							    f_array_small);
			}

			gettimeofday(tep, 0);

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
				if (f_array_small[i] != i) {
					printf("rpc_client: test_fixed_array_out_small: error (got %d) \n", f_array_small[i]);
					break;
				}
                        }

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: small fixed array out %d return\n", kr);

			TPRINTF("rpc_client: small fixed array out %f usec/call\n",
				t);

			break;

		   case 'j':		/* fixed array out  -  medium */
			printf("rpc_client: testing medium fixed array out\n");

			for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
				f_array_medium[i] = 0;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_fixed_array_out_medium(server_port, 
							    f_array_medium);
			}

			gettimeofday(tep, 0);

			for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
				if (f_array_medium[i] != i) {
					printf("rpc_client: test_fixed_array_out_medium: error (got %d)\n", f_array_medium[i]);
					break;
				}
			}

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: medium fixed array out %d return\n",kr);

			TPRINTF("rpc_client: medium fixed array out %f usec/call\n",
				t);

			break;

		   case 'k':		/* fixed array out  -  large */
			printf("rpc_client: testing large fixed array out \n");

			for (i = 0 ; i < FIXED_ARRAY_SIZE_LARGE ; i++) {
				f_array_large[i] = 0;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_fixed_array_out_large(server_port, 
							    f_array_large);
			}

			gettimeofday(tep, 0);

			for (i = 0 ; i < FIXED_ARRAY_SIZE_LARGE ; i++) {
				if (f_array_large[i] != i) {
					printf("rpc_client: test_fixed_array_out_large: error (got %d)\n", f_array_large[i]);
					break;
				}
			}

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: large fixed array out %d return\n",kr);

			TPRINTF("rpc_client: large fixed array out %f usec/call\n",
				t);

			break;

                   case 'l':            /* bv array  -  small */
                        printf("rpc_client: testing small bv array \n");

			bva_small = (bva_small_t) f_array_small;
			bva_out   = (bva_small_t) f_array_small1;

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                bva_small[i] = i;
				bva_out[i] = 0;
                        }

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			fflush(stdout);

                        for (i = NITER ; i ; i--) {
                                kr = test_bva_small(server_port, bva_small,
					FIXED_ARRAY_SIZE_SMALL,
					bva_out, &count);
                        }

                        gettimeofday(tep, 0);

			if (count != FIXED_ARRAY_SIZE_SMALL) {
				printf("rpc_client: small bv array: ");
				printf("bad out count %d \n", count);
			}

			for (i = 0 ; i < count ; i++) {
				if (bva_out[i] != FIXED_ARRAY_SIZE_SMALL-i) {
					printf("rpc_client: small bv array: ");
					printf("bad out value i=%d\n", i);
					break;
				}
			}

                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: small bv array %d return\n", kr);

                        TPRINTF("rpc_client: small bv array %f usec/call\n",
                                t);

                        break;

                   case 'm':            /* uv array */
                        printf("rpc_client: testing uv array \n");

			uva = (uva_t) f_array_small;

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                uva[i] = i;
                        }

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

                        for (i = NITER ; i ; i--) {
                                kr = test_uva(server_port, uva,
					FIXED_ARRAY_SIZE_SMALL);
                        }

                        gettimeofday(tep, 0);

                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: uv array %d return\n", kr);

                        TPRINTF("rpc_client: uv array %f usec/call\n",
                                t);

                        break;


                   case 'o':            /* uv pointer array */
                        printf("rpc_client: testing uva_pointer \n");

			uva_pointer = (uva_pointer_t) f_array_small;
			uva_char_ptr = (uva_char_pointer_t) char_array;

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                uva_pointer[i] = i;
                        }

                        for (i = 0 ; i < sizeof(char_array) ; i++) {
                                uva_char_ptr[i] = 'p';
                        }

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

                        for (i = NITER ; i ; i--) {
                                kr = test_uva_pointer(server_port, uva_pointer,
					FIXED_ARRAY_SIZE_SMALL,
					uva_char_ptr, CHAR_ARRAY_SIZE);
                        }

                        gettimeofday(tep, 0);

                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: uva_pointer array %d return\n", kr);

                        TPRINTF("rpc_client: uva_pointer array %f usec/call\n",
                                t);

                        break;


                   case 'v':            /* uv pointer array out */
                        printf("rpc_client: testing uva_pointer_out \n");

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			count = 0; char_cnt = 0;
			uva_pointer = (uva_pointer_t) NULL;
			uva_char_ptr = (uva_char_pointer_t) NULL;

                        for (i = NITER ; i ; i--) {
                                kr = test_uva_pointer_out(server_port, 
					&uva_pointer, &count,
					&uva_char_ptr, &char_cnt);
                        }

                        gettimeofday(tep, 0);

			if ((char *)uva_pointer == NULL ||
			           uva_char_ptr == NULL) {
				printf("rpc_client: uva_pointer_out: badptr\n");
				exit(1);
			}

			if (count != FIXED_ARRAY_SIZE_SMALL) {
				printf("rpc_client: uva_pointer_out: bdicnt\n");
				printf("count = %d\n", count);
			}

			if (char_cnt != CHAR_ARRAY_SIZE) {
				printf("rpc_client: uva_pointer_out: bdccnt\n");
				printf("count = %d\n", char_cnt);
			}

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                if (uva_pointer[i] != i) {
                        		printf("rpc_client: uva_pointer_out: ");
					printf("int error (got %d)\n", 
						uva_pointer[i]);
                                        break;
                                }
                        }

                        for (i = 0 ; i < CHAR_ARRAY_SIZE ; i++) {
                                if (uva_char_ptr[i] != 'q') {
                        		printf("rpc_client: uva_pointer_out: ");
					printf("char error (got %c)\n", 
						uva_char_ptr[i]);
                                        break;
                                }
                        }

                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: uva_pointer array %d return\n", kr);

                        TPRINTF("rpc_client: uva_pointer array %f usec/call\n",
                                t);

                        break;

                   case 'z':            /* uv pointer array out dealloc*/
                        printf("rpc_client: testing uva_pointer_out dealloc\n");

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			count = 0;
			uva_pointer = (uva_pointer_t) NULL;

                        for (i = NITER ; i ; i--) {
                                kr = test_uva_pointer_out_dealloc(server_port, 
					&uva_pointer, &count);
                        }

                        gettimeofday(tep, 0);

			if ((char *)uva_pointer == NULL) {
				printf("rpc_client: uva_pter_out_de: badptr\n");
				exit(1);
			}

			if (count != FIXED_ARRAY_SIZE_SMALL) {
				printf("rpc_client: uva_pter_out_de: bdcnt\n");
				printf("count = %d\n", count);
			}

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                if (uva_pointer[i] != i) {
                        		printf("rpc_client: uva_pter_out_de: ");
					printf("error (got %d)\n", 
						uva_pointer[i]);
                                        break;
                                }
                        }

                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: uva_ptr_de array %d return\n", kr);

                        TPRINTF("rpc_client: uva_ptr_de array %f usec/call\n",
                                t);

                        break;

                   case 'a':            /* uv pointer port array out */
                        printf("rpc_client: testing test_port_array_out \n");

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

                        count = uvpaCnt = bvpaCnt = 0; 
			uvpa = NULL;
			bvpa = (bvpa_t) f_array_medium;
                        port_array = (port_array_t) NULL;

                        for (i = NITER ; i ; i--) {
                                kr = test_port_array_out(server_port,
                                        &port_array, &count,
					&uvpa, &uvpaCnt,
					bvpa, &bvpaCnt);
                        }

                        gettimeofday(tep, 0);

                        if ((char *)port_array == NULL || uvpa == NULL ||
			    bvpa == NULL) {
                                printf("rpc_client: port_array_out: badptr\n");
                                exit(1);
                        }

                        if (count   != FIXED_ARRAY_SIZE_SMALL ||
			    uvpaCnt != FIXED_ARRAY_SIZE_SMALL ||
			    bvpaCnt != FIXED_ARRAY_SIZE_SMALL) {
                                printf("rpc_client: port_array_out: bdicnt\n");
                                printf("count = %d\n", count);
				exit(1);
                        }

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                        	kr = mach_port_type(mach_task_self(), 
						    port_array[i],
                                                    &actual_type);

                        	if (kr != KERN_SUCCESS) {
                                	printf("rpc_client: port_array_out: ");
					printf("port type: %s\n", 
                                        	mach_error_string(kr));
                                	break;
                        	}

                        	if (!(actual_type & MACH_PORT_TYPE_SEND)) {
                                	printf("rpc_client: port_array_out: ");
					printf("not send 0x%x\n", actual_type);
                                	break;
                        	}

                        	kr = mach_port_deallocate(mach_task_self(),
                                                  	  port_array[i]);
                        	if (kr != KERN_SUCCESS) {
                                	printf("rpc_client: port_array_out: ");
					printf("port dealloc: %s\n",
                                        	mach_error_string(kr));
                                	break;
                        	}
                        }

                        for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                kr = mach_port_type(mach_task_self(),
                                                    uvpa[i],
                                                    &actual_type);

                                if (kr != KERN_SUCCESS) {
                                        printf("rpc_client: port_array_out: ");
                                        printf("port type: %s\n",
                                                mach_error_string(kr));
                                        break;
                                }

                                if (!(actual_type & MACH_PORT_TYPE_SEND)) {
                                        printf("rpc_client: port_array_out: ");
                                        printf("not send 0x%x\n", actual_type);
                                        break;
                                }

                                kr = mach_port_deallocate(mach_task_self(),
                                                          uvpa[i]);
                                if (kr != KERN_SUCCESS) {
                                        printf("rpc_client: port_array_out: ");
                                        printf("port dealloc: %s\n",
                                                mach_error_string(kr));
                                        break;
                                }
                        }

                       for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                                kr = mach_port_type(mach_task_self(),
                                                    bvpa[i],
                                                    &actual_type);

                                if (kr != KERN_SUCCESS) {
                                        printf("rpc_client: port_array_out: ");
                                        printf("port type: %s\n",
                                                mach_error_string(kr));
                                        break;
                                }

                                if (!(actual_type & MACH_PORT_TYPE_SEND)) {
                                        printf("rpc_client: port_array_out: ");
                                        printf("not send 0x%x\n", actual_type);
                                        break;
                                }

                                kr = mach_port_deallocate(mach_task_self(),
                                                          bvpa[i]);
                                if (kr != KERN_SUCCESS) {
                                        printf("rpc_client: port_array_out: ");
                                        printf("port dealloc: %s\n",
                                                mach_error_string(kr));
                                        break;
                                }
                        }


                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: port_array_out %d return\n", kr);

                        TPRINTF("rpc_client: port_array_out %f usec/call\n",
                                t);

                        break;

                   case 'c':            /* uv pointer port array in */
                        printf("rpc_client: test_port_array_in \n");

                        if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

                        count   = FIXED_ARRAY_SIZE_SMALL;
			uvpaCnt = FIXED_ARRAY_SIZE_MEDIUM;
			bvpaCnt = FIXED_ARRAY_SIZE_SMALL;

                        port_array = (port_array_t) f_array_small1;
                        uvpa = (uvpa_t) f_array_medium;
                        bvpa = (bvpa_t) f_array_small;

        		/*
         		 *      Build three arrays of ports
         		 */
                       for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
                                kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE,
                                        &( (uvpa)[i] ));

                                if (kr != KERN_SUCCESS) {
                                   printf("rpc_client: test_port_array_in: ");
                                   printf("aloc\n");
                                   printf("error %s\n", mach_error_string(kr));
                                   break;
                                }

                                kr = mach_port_insert_right(mach_task_self(),
                                            (uvpa)[i], (uvpa)[i],
                                            MACH_MSG_TYPE_MAKE_SEND);

                                if (kr != KERN_SUCCESS) {
                                   printf("rpc_client: test_port_array_in: ");
                                   printf("ins\n");
                                   printf("error %s\n", mach_error_string(kr));
                                   break;
                                }
                        }

        		for (i = 0 ; i < FIXED_ARRAY_SIZE_SMALL ; i++) {
                		kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE,
                                        &( (port_array)[i] ));

                		if (kr != KERN_SUCCESS) {
                        	   printf("rpc_client: test_port_array_in: ");
                        	   printf("aloc\n");
                        	   printf("error %s\n", mach_error_string(kr));
                        	   break;
                		}

                		kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE,
                                        &( (bvpa)[i] ));

                		if (kr != KERN_SUCCESS) {
                        	   printf("rpc_client: test_port_array_in: ");
                        	   printf("aloc\n");
				   printf("error %s\n", mach_error_string(kr));
                        	   break;
                		}

                		kr = mach_port_insert_right(mach_task_self(),
                                            (port_array)[i], (port_array)[i],
                                            MACH_MSG_TYPE_MAKE_SEND);

                		if (kr != KERN_SUCCESS) {
                        	   printf("rpc_client: test_port_array_in: ");
                        	   printf("ins\n");
				   printf("error %s\n", mach_error_string(kr));
                        	   break; 
                		}
        		
                		kr = mach_port_insert_right(mach_task_self(),
                                            (bvpa)[i], (bvpa)[i],
                                            MACH_MSG_TYPE_MAKE_SEND);

                		if (kr != KERN_SUCCESS) {
                       		   printf("rpc_client: test_port_array_in: ");
                       		   printf("ins\n");
				   printf("error %s\n", mach_error_string(kr));
                        	   break;
                		}
        		}


                        for (i = NITER ; i ; i--) {
                                kr = test_port_array_in(server_port,
                                        port_array, count,
                                        uvpa, uvpaCnt,
                                        bvpa, bvpaCnt);
                        }

                        gettimeofday(tep, 0);
                        timevalsub(tep, tsp);
                        t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec;
                        t = t/(double)NITER;

                        printf("rpc_client: port_array_in %d return\n", kr);

                        TPRINTF("rpc_client: port_array_in %f usec/call\n",
                                t);

                        break;

		   case 'y':		/* cstring in */
			printf("rpc_client: testing cstring \n");

			for (i = 0 ; i < STRG_ARRAY_SIZE ; i++) {
				string[i] = 'z';
				string2[i] = 'a';
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_string(server_port, string,
						    string2);
			}

			gettimeofday(tep, 0);

			
                        for (i = 0 ; i < STRG_ARRAY_SIZE ; i++) {
                                if (string2[i] != 'y') { 
					printf("rpc_client: test_string:err\n");
					break;
				}
                        }

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: cstring in %d return\n", kr);

			TPRINTF("rpc_client: cstring in %f usec/call\n",
				t);

			break;

		   case 'b':		/* test_pointer_arrays */
			printf("rpc_client: test_pointer_arrays \n");

			uva = (uva_t) NULL;	
			bva_pointer1 = (bva_pointer_t) NULL;
			bva_pointer  = (bva_pointer_t) f_array_large;
			f_array_ptr  = (fixed_array_pointer_t) f_array_medium;

			for (i = 0 ; i < FIXED_ARRAY_SIZE_MEDIUM ; i++) {
				f_array_ptr[i] = i;
				bva_pointer[i] = FIXED_ARRAY_SIZE_MEDIUM - i;
			}

			if (gettimeofday(tsp, 0) < 0) perror("gettimeofday");

			for (i = NITER ; i ; i--) {
				kr = test_pointer_arrays( server_port, 
					f_array_ptr, bva_pointer, 
					FIXED_ARRAY_SIZE_MEDIUM,
					&bva_pointer1, &bva_count,
					&uva, &uva_count );
			}

			gettimeofday(tep, 0);

			if (bva_count != 112 || uva_count != 112) {
				printf("rpc_client: test_pointer_arrays: ");
                                printf("bad out count %d %d\n", bva_count, 
					uva_count);
			}

			for (i = 0 ; i < 112 ; i++) {
				if ( bva_pointer1[i] != i ||
				     uva[i] != 112 - i ) {
					printf("rpc_client: pointer_arrays: ");
					printf("bad data %d %d %d\n", i,
						bva_pointer1[i], uva[i]);
					break;
				}
                        }

			timevalsub(tep, tsp);
			t = 1.0e6 * (double)tep->tv_sec + (double)tep->tv_usec; 
			t = t/(double)NITER;

			printf("rpc_client: pointer_arrays in %d return\n", kr);

			TPRINTF("rpc_client: pointer_arrays in %f usec/call\n",
				t);

			break;

		   default:
		   case 'q':
			exit(0);
		}

		 while ((c = getc(stdin)) != '\n');
	}
}





void
timevalsub(t1, t2)
        struct timeval *t1, *t2;
{

        t1->tv_sec -= t2->tv_sec;
        t1->tv_usec -= t2->tv_usec;
        timevalfix(t1);
}

void
timevalmul(t, n)
        long n;
        struct timeval *t;
{
        t->tv_sec *= n;
        t->tv_usec *= n;
        timevalfix(t);
}

void
timevalfix(t1)
        struct timeval *t1;
{

        while (t1->tv_usec < 0) {
                t1->tv_sec--;
                t1->tv_usec += 1000000;
        }
        while (t1->tv_usec >= 1000000) {
                t1->tv_sec++;
                t1->tv_usec -= 1000000;
        }
}
