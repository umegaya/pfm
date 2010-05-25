# Makefile for libnbr.a

#-----------------------------------------------------------------------
#	global setting
#-----------------------------------------------------------------------
DEBUG = 1
CC = gcc
AR = ar crv
DEFS = -DHAVE_CONFIG_H
DEFS += -D_GNU_SOURCE
ifeq ($(DEBUG), 1)
CFLAG = -Wall -g -march=i686 -fPIC -D_DEBUG -D_TEST
else 
CFLAG = -O4 -march=i686 -fPIC
endif
ifeq ($(VALGRIND), 1)
CFLAG += -D_VALGRIND
endif

CONFS = configure *.log config.status config.h.in confdefs.h
COREFILE = core.*
MISC = autom4te.cache
LIBS = $(foreach lib,$(LIBREQLIBS),$(shell echo "-l$(lib)")) -L. -l$(LIBNAME)
INCS = -I$(LIBDIR) -I$(EXLIB) -I$(PLUGIN_DIR) -I.
DEPS = dep
OBJS = obj

#-----------------------------------------------------------------------
#	libnbr setting
#-----------------------------------------------------------------------
LIBDIR = core
EXLIB = exlib
LIBSRCS = $(shell find $(LIBDIR) -regex .*\\.c$$ -printf %f\\n)
LIBOBJS = $(LIBSRCS:%.c=$(OBJS)/$(LIBDIR)/%.o)
LIBDEPS = $(LIBSRCS:%.c=$(DEPS)/$(LIBDIR)/%.d)
LIBREQLIBS = pthread gcc_s
define EXTLIBS 
	$(shell find $(EXLIB) -regex .*\\.a$$)
endef
LIBNAME = nbr
LIBRARY = lib$(LIBNAME).a

#-----------------------------------------------------------------------
#	unit test setting
#-----------------------------------------------------------------------
TESTDIR = test/unit
TESTSRCS = $(shell find $(TESTDIR) -regex .*\\.c$$ -printf %f\\n)
TESTOBJS = $(TESTSRCS:%.c=$(OBJS)/$(TESTDIR)/%.o)
TESTDEPS = $(TESTSRCS:%.c=$(DEPS)/$(TESTDIR)/%.d)
TESTREQLIBS = z bz2 rt m tokyocabinet ssl crypto
TESTLIBS = $(foreach lib,$(TESTREQLIBS),$(shell echo "-l$(lib)"))
UNITTEST = ut
RUNTEST = all

#-----------------------------------------------------------------------
#	module for test setting
#-----------------------------------------------------------------------
MODDIR = test/module
MODREQOBJS = $(OBJS)/$(TESTDIR)/tutil.o
MODSRCS = $(shell find $(MODDIR) -regex .*\\.c$$ -printf %f\\n)
MODULES = $(MODSRCS:%.c=$(MODDIR)/%.elf)
MODDEPS = $(MODSRCS:%.c=$(DEPS)/$(MODDIR)/%.d)

#-----------------------------------------------------------------------
#	plugin setting
#-----------------------------------------------------------------------
PLUGIN_DIR = plugin
PLUGIN_SRCS = $(shell find $(PLUGIN_DIR) -regex .*\\.c$$ -printf %f\\n)
PLUGIN = $(PLUGIN_SRCS:%.c=$(PLUGIN_DIR)/lib$(LIBNAME)_%.a)
PLUGIN_DEPS = $(PLUGIN_SRCS:%.c=$(DEPS)/$(PLUGIN_DIR)/%.d)

#-----------------------------------------------------------------------
#	autoconf setting
#-----------------------------------------------------------------------
AUTOCONF = autoconf
AUTOHEADER = autoheader

#-----------------------------------------------------------------------
#	suffix rules
#-----------------------------------------------------------------------
.SUFFIXES: .c .o .elf .a .d

#.c.o:
#	$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@ $<

$(OBJS)/$(LIBDIR)/%.o: $(LIBDIR)/%.c
	@mkdir -p $(OBJS)/$(LIBDIR)
	$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@ $<

$(DEPS)/$(LIBDIR)/%.d: $(LIBDIR)/%.c
	@mkdir -p $(DEPS)/$(LIBDIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed 's:.*\.o:${OBJS}/${LIBDIR}/\0:' > $@
	
$(OBJS)/$(TESTDIR)/%.o: $(TESTDIR)/%.c
	@mkdir -p $(OBJS)/$(TESTDIR)
	$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@ $<

$(DEPS)/$(TESTDIR)/%.d: $(TESTDIR)/%.c
	@mkdir -p $(DEPS)/$(TESTDIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed 's:.*\.o:${OBJS}/${TESTDIR}/\0:' > $@

$(DEPS)/$(MODDIR)/%.d: $(MODDIR)/%.c
	@mkdir -p $(DEPS)/$(MODDIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) -I$(TESTDIR) $< -M | sed 's:\(.*\)\.o:${MODDIR}\/\1\.elf:' > $@
	@cat $@ | sed -e '1a ${LIBRARY} ${PLUGIN} \\' > $@.tmp
	@mv $@.tmp $@
	
$(DEPS)/$(PLUGIN_DIR)/%.d: $(PLUGIN_DIR)/%.c
	@mkdir -p $(DEPS)/$(PLUGIN_DIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed -e 's/\(.*\)\.o/${PLUGIN_DIR}\/lib${LIBNAME}_\1\.a/g' > $@
	
$(PLUGIN_DIR)/lib$(LIBNAME)_%.a: $(PLUGIN_DIR)/%.c
	@$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm $@.o
	
$(MODDIR)/%.elf: $(MODDIR)/%.c
	$(CC) -o $@ $(CFLAG) $(DEFS) $(INCS) -I$(TESTDIR) $< $(LIBS) $(TESTLIBS) $(MODREQOBJS) $(PLUGIN)


#-----------------------------------------------------------------------
#	make target
#-----------------------------------------------------------------------
all: $(EXLIB) $(LIBRARY) $(PLUGIN) $(UNITTEST) $(MODULES) 

test: all
	./$(TESTDIR)/$(UNITTEST) $(RUNTEST)
	make -C $(EXLIB) test

.PHONY : $(EXLIB)
$(EXLIB) : ; make -C $(EXLIB)

$(LIBRARY) : $(LIBOBJS)
	$(AR) $(LIBRARY) $(LIBOBJS) $(shell find $(EXLIB) -regex .*\\.o$$)

$(UNITTEST) : $(TESTOBJS)
	$(CC) -o $(TESTDIR)/$(UNITTEST) $(CFLAG) $(DEFS) $(INS) $(TESTOBJS) $(LIBS) $(TESTLIBS) $(PLUGIN)

clean:
	rm -f $(LIBOBJS) $(TESTOBJS) $(TESTDIR)/$(UNITTEST) $(MODULES) $(LIBRARY) $(COREFILE) $(PLUGIN)
	make -C $(EXLIB) clean
	
cleanup: clean
	rm -rf $(OBJS) $(DEPS)

conf:
	$(AUTOHEADER)
	$(AUTOCONF)

rmcf: cleanup
	rm -f $(CONFS)
	rm -rf $(MISC)

#-----------------------------------------------------------------------
#	make dependency
#-----------------------------------------------------------------------
-include $(LIBDEPS)
-include $(TESTDEPS)
-include $(PLUGIN_DEPS)
-include $(MODDEPS)
