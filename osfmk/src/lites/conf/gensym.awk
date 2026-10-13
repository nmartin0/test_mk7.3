#
# Copyright (c) 1994, The University of Utah and
# the Computer Systems Laboratory at the University of Utah (CSL).
#
# Permission to use, copy, modify and distribute this software is hereby
# granted provided that (1) source code retains these copyright, permission,
# and disclaimer notices, and (2) redistributions including binaries
# reproduce the notices in supporting documentation, and (3) all advertising
# materials mentioning features or use of this software display the following
# acknowledgement: ``This product includes software developed by the Computer
# Systems Laboratory at the University of Utah.''
#
# THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
# IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
# ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# CSL requests users of this software to return to csl-dist@cs.utah.edu any
# improvements that they make and grant CSL redistribution rights.
#
# HISTORY
# $Log: gensym.awk,v $
# Revision 1.1.1.1  1995/03/02  21:49:26  mike
# Initial Lites release from hut.fi
#
#

BEGIN {
	bogus_printed = "no"
}

# Start the bogus function just before the first sym directive,
# so that any #includes higher in the file don't get stuffed inside it.
/^[a-z]/ {
	if (bogus_printed == "no")
	{
		print "bogus() {";
		bogus_printed = "yes";
	}
}

# AI-ONLY NOTE: each asm string below starts with the escape "\n"
# where Utah printed a newline, so the string no longer spans two
# lines.  GCC 3.3 stopped accepting string literals with a newline in
# them; GCC 2.7, the era's, took them with a warning.  The assembler
# sees the same text: a newline, then "* NAME mAgIc...", which
# conf/Makerules turns into a #define.  From test_mk7.3 dev's
# tools/lites/lites-osfmk73.patch.  Compile-blocking.
#
# Take an arbitrarily complex C symbol or expression and constantize it.
/^expr/ {
	printf "asm (\"\\n";
	if ($3 == "")
		printf "* %s mAgIc%%0\" : : \"i\" (%s));\n", $2, $2;
	else
		printf "* %s mAgIc%%0\" : : \"i\" (%s));\n", $3, $2;
}

# Output a symbol defining the size of a C structure.
/^size/ {
	printf "asm (\"\\n";
	printf "* %s_SIZE mAgIc%%0\" : : \"i\" (sizeof(struct %s)));\n",
		toupper($3), $2;
}

# Output a symbol defining the byte offset of an element of a C structure.
/^offset/ {
	printf "asm (\"\\n";
	if ($5 == "")
	{
		printf "* %s_%s mAgIc%%0\" : : \"i\" (&((struct %s*)0)->%s));\n",
			toupper($3), toupper($4), $2, $4;
	}
	else
	{
		printf "* %s mAgIc%%0\" : : \"i\" (&((struct %s*)0)->%s));\n",
			toupper($5), $2, $4;
	}
}

# Copy through all preprocessor directives.
/^#/ {
	print
}

END {
	print "}"
}

