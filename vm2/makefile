#-----------------------------------------------------------------------
#	global setting
#-----------------------------------------------------------------------
DEBUG = 1
VERBOSE_BUILD = 0
CC = g++
AR = ar crv
DEFS = -D_GNU_SOURCE
ifeq ($(DEBUG), 1)
CFLAG = -Wall -g -march=i686 -fPIC -D_DEBUG -D_RPC_PROF
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
SVPFX = pfm
COREFILE = core.*
LIBS = -L. -L./lib -L$(NBRDIR) -L$(SFCDIR) -lsfc -lnbr
INCS = -I. -Icore -Ilib -Itest -Iclnt -I$(NBRCOREDIR) -I$(SFCCOREDIR) -I$(SFCLIBDIR)
DEPS = dep
OBJS = obj
TOBJS = tobj
TDEPS = tdep
ifeq ($(DEBUG), 1)
LUA = luad
else
LUA = lua
endif


#-----------------------------------------------------------------------
#	master
#-----------------------------------------------------------------------
MDIR = mstr
MSRCS = $(shell find $(MDIR) -regex .*\\.cpp$$)
MOBJS = $(MSRCS:%.cpp=$(OBJS)/%.o)
MDEPS = $(MSRCS:%.cpp=$(DEPS)/%.d)
MREQLIBS = tokyocabinet z bz2 pthread rt m ssl crypto $(LUA) msgpack
MLIBS = $(foreach lib,$(MREQLIBS),$(shell echo "-l$(lib)"))
MASTER = $(SVPFX)m

#-----------------------------------------------------------------------
#	servant
#-----------------------------------------------------------------------
SDIR = svnt
SSRCS = $(shell find $(SDIR) -regex .*\\.cpp$$)
SOBJS = $(SSRCS:%.cpp=$(OBJS)/%.o)
SDEPS = $(SSRCS:%.cpp=$(DEPS)/%.d)
SREQLIBS = tokyocabinet z bz2 pthread rt m ssl crypto $(LUA) msgpack
SLIBS = $(foreach lib,$(SREQLIBS),$(shell echo "-l$(lib)"))
SERVANT = $(SVPFX)s

#-----------------------------------------------------------------------
#	client
#-----------------------------------------------------------------------
CDIR = clnt
CLIENT = $(SVPFX)c
CLIBSRCS = $(CDIR)/clnt.cpp
CSRCS = $(CDIR)/main.cpp
CLIBOBJS = $(CLIBSRCS:%.cpp=$(OBJS)/%.o)
COBJS = $(CSRCS:%.cpp=$(OBJS)/%.o)
CDEPS = $(CSRCS:%.cpp=$(DEPS)/%.d) $(CLIBSRCS:%.cpp=$(DEPS)/%.d)
CREQLIBS = tokyocabinet z bz2 pthread rt m ssl crypto $(LUA) msgpack
CLIBS = $(foreach lib,$(CREQLIBS),$(shell echo "-l$(lib)"))

#-----------------------------------------------------------------------
#	test
#-----------------------------------------------------------------------
TESTDIR = test
TESTSRCS = $(shell find $(TESTDIR) -regex .*\\.cpp$$ -printf %f\\n)
TESTOBJS = $(TESTSRCS:%.cpp=$(TOBJS)/$(TESTDIR)/%.o)
TESTDEPS = $(TESTSRCS:%.cpp=$(TDEPS)/$(TESTDIR)/%.d)
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
TCOREOBJS = $(CORESRCS:%.cpp=$(TOBJS)/%.o)
COREDEPS = $(CORESRCS:%.cpp=$(DEPS)/%.d)
TCOREDEPS = $(CORESRCS:%.cpp=$(TDEPS)/%.d)

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

$(OBJS)/$(MDIR)/%.o: $(MDIR)/%.cpp
	@mkdir -p $(OBJS)/$(MDIR)
	$(CC) $(CFLAG) $(INCS) -c -o $@ $<

$(OBJS)/$(SDIR)/%.o: $(SDIR)/%.cpp
	@mkdir -p $(OBJS)/$(SDIR)
	$(CC) $(CFLAG) $(INCS) -c -o $@ $<
	
$(OBJS)/$(CDIR)/%.o: $(CDIR)/%.cpp
	@mkdir -p $(OBJS)/$(CDIR)
	$(CC) $(CFLAG) $(INCS) -c -o $@ $<

$(TOBJS)/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(TOBJS)/$(TESTDIR)
	$(CC) $(CFLAG) -D_TEST $(INCS) -c -o $@ $<

$(TOBJS)/$(COREDIR)/%.o: $(COREDIR)/%.cpp
	@mkdir -p $(TOBJS)/$(COREDIR)
	$(CC) $(CFLAG) -D_TEST $(INCS) -c -o $@ $<

$(TOBJS)/%.o:
	@mkdir -p $(TOBJS)/$(dir $<)
	$(CC) $(CFLAG) -D_TEST $(INCS) -c -o $@ $<

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

$(TDEPS)/$(COREDIR)/%.d: $(COREDIR)/%.cpp
	@mkdir -p $(TDEPS)/$(COREDIR)
	@$(CC) $(CFLAG) -D_TEST $(INCS) $< -M | 	\
		sed 's:.*\.o:$(TOBJS)/$(COREDIR)/\0:' > $@

$(TDEPS)/$(TESTDIR)/%.d: $(TESTDIR)/%.cpp
	@mkdir -p $(TDEPS)/$(TESTDIR)
	@$(CC) $(CFLAG) -D_TEST $(INCS) $< -M | 	\
		sed 's:.*\.o:$(TOBJS)/$(TESTDIR)/\0:' > $@

$(DEPS)/$(SDIR)/%.d: $(SDIR)/%.cpp
	@mkdir -p $(DEPS)/$(SDIR)
	@$(CC) $(CFLAG) $(INCS) $< -M |         \
		sed 's:.*\.o:${OBJS}/${SDIR}/\0:' > $@

$(DEPS)/$(MDIR)/%.d: $(MDIR)/%.cpp
	@mkdir -p $(DEPS)/$(MDIR)
	@$(CC) $(CFLAG) $(INCS) $< -M |         \
		sed 's:.*\.o:${OBJS}/${MDIR}/\0:' > $@
		
$(DEPS)/$(CDIR)/%.d: $(CDIR)/%.cpp
	@mkdir -p $(DEPS)/$(CDIR)
	@$(CC) $(CFLAG) $(INCS) $< -M |         \
		sed 's:.*\.o:${OBJS}/${CDIR}/\0:' > $@

$(OBJS)/%.a: 
	@mkdir -p $(OBJS)/$(dir $<)
	@$(CC) $(CFLAG) $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm $@.o

$(TOBJS)/%.a:
	@mkdir -p $(TOBJS)/$(dir $<)
	@$(CC) $(CFLAG) -D_TEST $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm  $@.o

$(DEPS)/%.d: $(dir %)/$(notdir %).cpp
	@mkdir -p $(DEPS)/$(dir $<)
	@$(CC) $(CFLAG) $(INCS) $< -M | \
	sed -e "s/\(.*\)\.o/$(OBJS)\/${subst /,\/,${dir $<}}lib${LIBPFX}\1\.a/g" > $@


#-----------------------------------------------------------------------
#	make target
#-----------------------------------------------------------------------
all: module $(UNITTEST) $(MASTER) $(SERVANT) $(CLIENT)

test: all
	./$(TESTDIR)/$(UNITTEST) $(RUNTEST)

$(MASTER): $(COREOBJS) $(MOBJS)
	$(CC) -o $(MDIR)/$(MASTER) $(CFLAG) $(LNKOPT) $(INCS) $(COREOBJS) $(MOBJS) $(MODULES) $(LIBS) $(MLIBS)

$(SERVANT) : $(COREOBJS) $(SOBJS)
	$(CC) -o $(SDIR)/$(SERVANT) $(CFLAG) $(LNKOPT) $(INCS) $(COREOBJS) $(SOBJS) $(MODULES) $(LIBS) $(SLIBS)
	
lib$(CLIENT).a : $(COREOBJS) $(CLIBOBJS)
	$(AR) lib$(CLIENT).a $(COREOBJS) $(CLIBOBJS)
	
$(CLIENT) : lib$(CLIENT).a $(COBJS)
	$(CC) -o $(CDIR)/$(CLIENT) $(CFLAG) $(LNKOPT) $(INCS) $(COREOBJS) $(COBJS) $(MODULES) -l$(CLIENT) $(LIBS) $(CLIBS)

module: $(MODULES)

$(UNITTEST) : $(TESTOBJS) $(TCOREOBJS)
	$(CC) -o $(TESTDIR)/$(UNITTEST) $(CFLAG) $(LNKOPT) $(INCS) $(TESTOBJS) $(TCOREOBJS) $(MODULES) $(LIBS) $(TESTLIBS)

clean:
	rm -f $(TESTOBJS) $(TESTDIR)/$(UNITTEST) $(MODULES) $(COREFILE) $(MOBJS) $(SOBJS) $(MDIR)/$(MASTER) $(SDIR)/$(SERVANT)
	
cleanup: clean
	rm -rf $(OBJS) $(DEPS) $(TOBJS) $(TDEPS)


#-----------------------------------------------------------------------
#	make dependency
#-----------------------------------------------------------------------
-include $(MODULE_DEPS)
-include $(TESTDEPS)
-include $(COREDEPS)
-include $(TCOREDEPS)
-include $(SDEPS)
-include $(MDEPS)
