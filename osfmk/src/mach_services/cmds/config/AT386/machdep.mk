#
# AT386 host build of config, after POWERMAC/machdep.mk, which names
# the host's lex the same way.  gram.y writes its actions in the old
# "= { ... }" form, which byacc still reads and bison does not.
# -lln was OSF/1's lex library; parse.l supplies its own yywrap and
# needs none.
#
YACC=byacc
LIBS=
