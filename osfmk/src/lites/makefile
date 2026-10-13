#
# Utah makefile, more BSD like
#
# $Hdr: makefile 1.3 95/08/18$
#
DESTDIR=

MAKE=gmake
CONFIGURE_OPTS=--target=parisc --host=parisc --build=parisc \
	--enable-mach4 --with-release=/usr/mach4 --prefix=/usr/mach4

# Lites
LITES_SRC=/usr/src/lites
LITES_OBJ=/usr/obj/lites
CONFIG=STD+WS+ACT
CONFIGURE_OPTS+=--srcdir=$(LITES_SRC) --with-config=$(CONFIG)

# HPBSD
#LITES_SRC=/home/css/mike/lites
#LITES_OBJ=/home/css/mike/lites/obj
#CONFIG=STD+WS+ACT+DEBUG
#CONFIGURE_OPTS+=--srcdir=$(LITES_SRC) --with-config=$(CONFIG)

all .DEFAULT: $(LITES_OBJ)/config.status
	cd $(LITES_OBJ); $(MAKE) $@

#
# platform-specific configuration
#
$(LITES_OBJ)/config.status: $(LITES_OBJ)/.mkdirstamp
	cd $(LITES_OBJ); $(LITES_SRC)/conf/configure $(CONFIGURE_OPTS)

$(LITES_OBJ)/.mkdirstamp:
	-@mkdir $(LITES_OBJ)
	-@touch $(LITES_OBJ)/.mkdirstamp

clean:
	-@echo "Don't do this, you can't build afterwards.  Instead remove"
	-@echo "all the .o files using find, or remove the entire object"
	-@echo "directory"
	
