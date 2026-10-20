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
 *   File     :  sym_ops.c
 *
 *   Author   :  Joe CaraDonna
 *
 *   Function :  Handles symbol table operations
 *
 *   Routines :
 *
 *     - symbol_table_load()
 *
 *            This routine loads all function names (of a given
 *            program) along with their starting addresses into 
 *            a structured array.
 *
 *            This array will later be used to locate functions
 *            by address, so it should be sorted by address.
 *               
 *            Parameters : INPUT:   a filename (whos symbol table will be read) 
 *                         OUTPUT:  the number of entries
 *
 *                         RETURN:  a pointer to the first entry
 *
 *     - print_symbol_table()
 *
 *            This routine simply prints the data stored in the
 *            local symbol table (used for debug purposes).
 *
 *            Parameters: INPUT: pointer to first symbol table entry
 *                               number of entries in the table
 *
 *     - table_compare()
 *      
 *            A comparison function used by qsort to sort the new symbol table
 *
 *
 *     - funky_find()
 *
 *            Performs a binary search on the symbol table by address
 *
 *
 *   Date     :  7/13/92
 *
 */
   

#include <stdio.h>
#include <sys/types.h>

#include <a.out.h>       /* COFF definitions			*/ 
#include <filehdr.h>
#include <syms.h>

#include "sym_ops.h"     /* definition of struct st_entry	*/

#include <mach.h>        /* vm_allocate()			*/



struct st_entry*
symbol_table_load (
	char s_file[],
	int  *entries)

{

    FILE    *sym_fd;        /* maintains pointer throughout symbol table */
    FILE    *str_fd;        /* maintains pointer throughout string table */

    FILHDR  file_header;    /* COFF header                               */
    SYMENT  symbol;         /* an entry in the symbol table              */

    int entry_count = 0;    /* entry counter                             */
    int strings;            /* calculated offset to the string table     */
    int x;                  /* misc counter                              */
    int num_symbols;        /* number of symbols in the symbol table     */
    struct st_entry *ste;   /* symbol table entry pointer                */
    struct st_entry *table_top;    /* pointer to first entry in table    */


    /* ======================================================== */
    /* open file twice so the symbol table and the string table */
    /* can be searched.  Two file pointers are thus needed.     */
    /* ======================================================== */

    if ((sym_fd = fopen (s_file,"r")) == NULL) {
        fprintf (stderr,"unable to open symbol table file: %s\n",s_file);
        exit (1);
    }

    if ((str_fd = fopen (s_file,"r")) == NULL) {
        fprintf (stderr,"unable to open symbol table file: %s\n",s_file);
        exit (1);
    }

    /* ========================= */
    /* read file header and init */
    /* ========================= */

    if (fread (&file_header,sizeof(FILHDR),1,sym_fd) != 1) {
        fprintf (stderr,"unable to read symbol table header\n");
        exit(1);
    }

    num_symbols = file_header.f_nsyms;
    strings = file_header.f_symptr+sizeof(SYMENT)*file_header.f_nsyms;

    /* ======================================= */
    /* set symbol file pointer to symbol table */
    /* ======================================= */

    fseek (sym_fd,file_header.f_symptr,SEEK_SET);

    /* ==================== */
    /* allocate table space */
    /* ==================== */

    x = vm_allocate (mach_task_self(),
                    (vm_address_t *) &ste,
                    sizeof(struct st_entry) * num_symbols,
                    TRUE);

    if (x != KERN_SUCCESS) {
        fprintf (stderr,"symbol table allocation error for: %s\n",s_file);
        exit (1);
    }

    table_top = ste;

    /* ====================== */
    /* get symbol table data  */
    /* ====================== */

    printf ("loading kernel symbol table...\n");

    while (num_symbols-- > 0) {

        if (fread (&symbol,sizeof(SYMENT),1,sym_fd) != 1) {
            fprintf (stderr,"error reading symbol table\n");
            exit(1);
        }

        /* =========================================================== */
        /* only add external symbols defined in text (functions)       */
        /* Note: n_scnum contains a section number, this determines    */
        /*       where the symbol table entry is. 0=data & 1=text      */
        /* =========================================================== */

        if (symbol.n_sclass == C_EXT && symbol.n_scnum == 1) { 
            /* is the symbol name less <= 8 characters */
            if (symbol.n_zeroes) {
                for (x=0; x < 8; x++)
                    ste->f_name[x] = symbol.n_name[x];
            }
            /* if it's not, then look it up in the string table */
            else {
                fseek (str_fd,strings+symbol._n._n_n._n_offset,SEEK_SET);
                fread (ste->f_name,FUNCTION_NAME_SIZE,1,str_fd);
                ste->f_name[FUNCTION_NAME_SIZE-1] = '\0';
            }

            /* load address */
            ste->f_addr = symbol.n_value;
            entry_count++;
            ste++;
        }

        /* =================================== */
        /* skip all, if any, auxillary entries */
        /* =================================== */

        if (fseek (sym_fd,(symbol.n_numaux*sizeof(AUXENT)),SEEK_CUR) != 0) {
            fprintf (stderr,"error skipping auxillary entries\n");
            exit(1);
        }

        num_symbols -= symbol.n_numaux;
    }

    fclose (sym_fd);
    fclose (str_fd);

    /* ================================ */
    /* Sort the symbol table by address */
    /* ================================ */

    qsort (table_top,entry_count,sizeof(struct st_entry),table_compare);

    *entries = entry_count;
    return (table_top);
}



int
table_compare (const void *a, const void *b)

{
    return ((struct st_entry *)a)->f_addr - ((struct st_entry *)b)->f_addr;
}


void
print_symbol_table (
	struct st_entry *table,
	int    size)

{
    while (size--) {
        printf ("%s ..... %x\n",table->f_name,table->f_addr);
        table++;
    }
}


struct st_entry*
funky_find (
	struct st_entry *front,     /* first entry in list       */
	int       size,             /* number of entries in list */
	u_long    addr,             /* address to search for     */
	u_long    *offset)          /* offset into that function */

{
    int mid_range;
    struct st_entry *middle;
    struct st_entry *back;
          
    back      = front+(size-1);
    middle    = front+(size / 2);
    mid_range = size/4;

    /* ============================================ */
    /* Perform binary search on sorted symbol table */
    /* ============================================ */

    while (1) {
        if (addr >= middle->f_addr) {
            front = middle;
            middle += mid_range;
        }
        else {
            back = middle;
            middle -= mid_range;
        }

        if (mid_range <= 1) {
            if (middle->f_addr < addr && middle->f_addr > front->f_addr) {
              *offset = addr - middle->f_addr;
              return (middle);
            }
            else {
              *offset = addr - front->f_addr;
              return (front);
            }
        }

        mid_range = (mid_range/2) + (mid_range % 2);
    }
}    
