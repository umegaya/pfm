#-----------------------------------------------------------------------
#	global setting
#-----------------------------------------------------------------------
DEBUG = 1
VERBOSE_BUILD = 0
CC = g++
AR = ar crv
DEFS = -D_GNU_SOURCE
ifeq ($(DEBUG), 1)
CFLAG = -Wall -g -march=i686 -fPIC -D_DEBUG -D_TEST -D_RPC_PROF
else 
CFLAG = -O4 -march=i686 -fPIC -D_RPC_PROF
endif
ifeq ($(VERBOSE_BUILD), 1)
CFLAG += -v
LNKOPT = -Wl,-verbose
else
LNKOPT =
endif

NBRDIR = ../nbr
SFCDIR = ../sfc
NBRCOREDIR = $(NBRDIR)/core
SFCCOREDIR = $(SFCDIR)/core
SFCLIBDIR = $(SFCDIR)/lib
LIBPFX = pfm
COREFILE = core.*
LIBS = -L. -L./lib -L$(NBRDIR) -L$(SFCDIR) -lsfc -lnbr
INCS = -I. -Icore -Ilib -Itest -I$(NBRCOREDIR) -I$(SFCCOREDIR) -I$(SFCLIBDIR)
DEPS = dep
OBJS = obj
ifeq ($(DEBUG), 1)
LUA = luad
else
LUA = lua
endif


#-----------------------------------------------------------------------
#	test
#-----------------------------------------------------------------------
TESTDIR = test
TESTSRCS = $(shell find $(TESTDIR) -regex .*\\.cpp$$ -printf %f\\n)
TESTOBJS = $(TESTSRCS:%.cpp=$(OBJS)/$(TESTDIR)/%.o)
TESTDEPS = $(TESTSRCS:%.cpp=$(DEPS)/$(TESTDIR)/%.d)
TESTREQLIBS = tokyocabinet z bz2 pthread rt m ssl crypto $(LUA) msgpack
TESTLIBS = $(foreach lib,$(TESTREQLIBS),$(shell echo "-l$(lib)"))
UNITTEST = ut
RUNTEST = any


#-----------------------------------------------------------------------
#	core
#-----------------------------------------------------------------------
COREDIR = core
CORESRCS = $(shell find $(COREDIR) -regex .*\\.cpp$$)
COREOBJS = $(CORESRCS:%.cpp=$(OBJS)/%.o)
COREDEPS = $(CORESRCS:%.cpp=$(DEPS)/%.d)


#-----------------------------------------------------------------------
#	module setting
#-----------------------------------------------------------------------
MODULE_DIR = module
MODULE_SRCDIRS = dbm ll serializer uuid
USE_MODULE = tc mac
MODULE_SRCS = $(foreach mdir,$(MODULE_SRCDIRS),$(shell find $(mdir) -regex .*\\.cpp$$))
MODULES = $(foreach src,$(MODULE_SRCS),$(OBJS)/$(dir $(src))lib$(LIBPFX)$(patsubst %.cpp,%.a,$(notdir $(src))))
MODULE_DEPS = $(MODULE_SRCS:%.cpp=$(DEPS)/%.d)
INCS += $(foreach mdir,$(MODULE_SRCDIRS),$(shell echo "-I$(mdir)"))
TMPDIR = 


#-----------------------------------------------------------------------
#	suffix rules
#-----------------------------------------------------------------------
.SUFFIXES: .cpp .o .a .d

$(OBJS)/%.o:
	@mkdir -p $(OBJS)/$(dir $<)
	$(CC) $(CFLAG) $(INCS) -c -o $@ $<

$(DEPS)/$(TESTDIR)/%.d: $(TESTDIR)/%.cpp
	@mkdir -p $(DEPS)/$(TESTDIR)
	@$(CC) $(CFLAG) $(INCS) $< -M | 	\
		sed 's:.*\.o:${OBJS}/${TESTDIR}/\0:' > $@

$(DEPS)/$(COREDIR)/%.d: $(COREDIR)/%.cpp
	@mkdir -p $(DEPS)/$(COREDIR)
	@$(CC) $(CFLAG) $(INCS) $< -M |         \
		sed 's:.*\.o:${OBJS}/${COREDIR}/\0:' > $@

$(OBJS)/%.a: 
	@mkdir -p $(OBJS)/$(dir $<)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm $@.o

$(DEPS)/%.d: $(dir %)/$(notdir %).cpp
	@mkdir -p $(DEPS)/$(dir $<)
	@$(CC) $(CFLAG) $(INCS) $< -M | \
	sed -e "s/\(.*\)\.o/$(OBJS)\/${subst /,\/,${dir $<}}lib${LIBPFX}\1\.a/g" > $@


#-----------------------------------------------------------------------
#	make target
#-----------------------------------------------------------------------
all: module $(UNITTEST)

test: all
	./$(TESTDIR)/$(UNITTEST) $(RUNTEST)

module: $(MODULES)

$(UNITTEST) : $(TESTOBJS) $(COREOBJS)
	$(CC) -o $(TESTDIR)/$(UNITTEST) $(CFLAG) $(LNKOPT) $(INCS) $(TESTOBJS) $(COREOBJS) $(MODULES) $(LIBS) $(TESTLIBS)

clean:
	rm -f $(TESTOBJS) $(TESTDIR)/$(UNITTEST) $(MODULES) $(COREFILE)
	
cleanup: clean
	rm -rf $(OBJS) $(DEPS)


#-----------------------------------------------------------------------
#	make dependency
#-----------------------------------------------------------------------
-include $(MODULE_DEPS)
-include $(TESTDEPS)
-include $(COREDEPS)
