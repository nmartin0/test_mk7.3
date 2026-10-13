/*
 *  (c) Copyright 1986 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */

#include <sys/param.h>
#include <sys/mbuf.h>

in_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_short *w;
	register u_int sum = 0;
	register int mlen = 0;
	register u_int odd_sum;

	for (;;) {	/* Each time thru adds in data from an mbuf */
		  w = mtod(m, u_short *);
		  if (mlen == -1) {	/* first time == 0 */
		  	 /*
			  * There is a byte left from the last segment;
			  * add it into the checksum.
			  */
			 sum += *(u_char *)w;
			 w = (u_short *)((char *)w + 1);
			 mlen = m->m_len - 1;
			 len--;
			 if (len == 0) break;	/* done */
		  	}
		    else
		    	mlen = m->m_len;
		  m = m->m_next;	/* prepare for next iteration */
		  if (len < mlen) mlen = len;
		  len -= mlen;
		  /*
		   * Here, mlen contains the number of bytes in this mbuf.
		   * The short pointer may be even or odd.  HP 9000 Series 800
		   * can only add half-word aligned shorts.  If odd alignment,
		   * do the additions then rotate the end result into proper
		   * alignment.
		   */
		  if (mlen && ((int)w & 0x01)) {	/* odd alignment */
		  		 odd_sum = *(u_char *)w; /* account for odd */
				 mlen--;
				 w = (u_short *) ((char *)w + 1);
				 while (mlen > 1) {
				 		   odd_sum += *w++;
						   mlen -= 2;
						  }
				 if (mlen) /* if 1 left, good */
				 	    odd_sum += (((u_int)*(u_char *)w) << 8);
				    else mlen = -1; /* else we had odd # */
				 odd_sum = (odd_sum & 0xffff)+(odd_sum >> 16);
				 odd_sum = (odd_sum & 0xffff)+(odd_sum >> 16);
				 sum += ((odd_sum & 0xff) << 8); /* rotate */
				 sum += ((odd_sum & 0xff00) >> 8); /* sum in*/
		  		}
			else {		/* no data or even alignment */
		  	      while (mlen > 1) {
						sum += *w++;
						mlen -= 2;
					       }
		  	      if (mlen) {   /* add odd end */
					 sum += (((u_int)*(u_char *)w) << 8);
					 mlen = -1;
					}
			     }
		  if (len == 0) break;	/* done all data */
		  /*
		   * Locate the next block with some data.  If there is a word
		   * split across a boundary, we will wrap to the top with
		   * mlen == -1 and then add it in shifted appropriately.
		   */
		  for (;;) {	/* m was loaded with next above */
		  	    if (m == 0) { /* len>0 but mbuf chain ends */
					 printf("cksum: out of data\n");
					 goto done; /* should not occur */
					}
			    if (m->m_len) break;
			    m = m->m_next;
			   }
		 }

done:
	/*
	 * Add together high and low parts of sum and carry to get
	 * cksum.  Do not drop the last carry here.
	 */
	 sum = (sum & 0xffff) + (sum >> 16); /* high + low */
	 sum = (sum & 0xffff) + (sum >> 16); /* high + low again for carry */
	 return ((~sum) & 0xffff);	     /* return complement short */
}
