#
# AT386 host build of migcom, after POWERMAC/machdep.mk: yacc writes
# parser.c in the object directory, and it includes "lexxer.h" from
# here.
#
INCFLAGS+=-I.
