# Makefile for libnbr.a

#-----------------------------------------------------------------------
#	global setting
#-----------------------------------------------------------------------
DEBUG = 1
VERBOSE_BUILD = 0
CC = g++
AR = ar crv
DEFS = 
DEFS += -D_GNU_SOURCE
ifeq ($(DEBUG), 1)
CFLAG = -Wall -g -fPIC -D_DEBUG
else 
CFLAG = -O4 -fPIC
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
COREFILE = core.*
LIBS = -L. -L./lib -L$(NBRDIR) -L$(SFCDIR) -lpvm -lsfc -lnbr
INCS = -I. -Icore -Ilib -Itest -I$(NBRCOREDIR) -I$(SFCCOREDIR) -I$(SFCLIBDIR)
DEPS = dep
OBJS = obj
ifeq ($(DEBUG), 1)
LUA = luad
else
LUA = lua
endif


#-----------------------------------------------------------------------
#	libpvm
#-----------------------------------------------------------------------
LIBDIR = core
LIBSRCS = $(shell find $(LIBDIR) -regex .*\\.cpp$$ -printf %f\\n)
LIBOBJS = $(LIBSRCS:%.cpp=$(OBJS)/$(LIBDIR)/%.o)
LIBDEPS = $(LIBSRCS:%.cpp=$(DEPS)/$(LIBDIR)/%.d)
LIBREQLIBS = nbr sfc
LIBNAME = pvm
LIBRARY = lib$(LIBNAME).a

#-----------------------------------------------------------------------
#	test
#-----------------------------------------------------------------------
TESTDIR = test
TESTSRCS = $(shell find $(TESTDIR) -regex .*\\.cpp$$ -printf %f\\n)
TESTOBJS = $(TESTSRCS:%.cpp=$(OBJS)/$(TESTDIR)/%.o)
TESTDEPS = $(TESTSRCS:%.cpp=$(DEPS)/$(TESTDIR)/%.d)
TESTREQLIBS = z bz2 rt m ssl crypto $(LUA) msgpack
TESTLIBS = $(foreach lib,$(TESTREQLIBS),$(shell echo "-l$(lib)"))
UNITTEST = ut
RUNTEST = any

#-----------------------------------------------------------------------
#	lang setting
#-----------------------------------------------------------------------
LANG_DIR = lang
LANG_SRCS = $(shell find $(LANG_DIR) -regex .*\\.cpp$$ -printf %f\\n)
LANG = $(LANG_SRCS:%.cpp=$(LANG_DIR)/lib$(LIBNAME)_%.a)
LANG_LIBS += $(LANG_SRCS:%.cpp=-l$(LIBNAME)_%)
LANG_DEPS = $(LANG_SRCS:%.cpp=$(DEPS)/$(LANG_DIR)/%.d)
INCS += -I$(LANG_DIR)

#-----------------------------------------------------------------------
#	kvs setting
#-----------------------------------------------------------------------
KVS_DIR = kvs
KVS_SRCS = $(shell find $(KVS_DIR) -regex .*\\.cpp$$ -printf %f\\n)
KVS = $(KVS_SRCS:%.cpp=$(KVS_DIR)/lib$(LIBNAME)_%.a)
KVS_LIBS += $(KVS_SRCS:%.cpp=-l$(LIBNAME)_%)
KVS_DEPS = $(KVS_SRCS:%.cpp=$(DEPS)/$(KVS_DIR)/%.d)
INCS += -I$(KVS_DIR)


#-----------------------------------------------------------------------
#       sr setting
#-----------------------------------------------------------------------
SR_DIR = sr
SR_SRCS = $(shell find $(SR_DIR) -regex .*\\.cpp$$ -printf %f\\n)
SR = $(SR_SRCS:%.cpp=$(SR_DIR)/lib$(LIBNAME)_%.a)
SR_LIBS += $(SR_SRCS:%.cpp=-l$(LIBNAME)_%)
SR_DEPS = $(SR_SRCS:%.cpp=$(DEPS)/$(SR_DIR)/%.d)
INCS += -I$(SR_DIR)

#-----------------------------------------------------------------------
#	suffix rules
#-----------------------------------------------------------------------
.SUFFIXES: .cpp .o .a .d

$(OBJS)/$(LIBDIR)/%.o: $(LIBDIR)/%.cpp
	@mkdir -p $(OBJS)/$(LIBDIR)
	$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@ $<

$(DEPS)/$(LIBDIR)/%.d: $(LIBDIR)/%.cpp
	@mkdir -p $(DEPS)/$(LIBDIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed 's:.*\.o:${OBJS}/${LIBDIR}/\0:' > $@
	
$(OBJS)/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(OBJS)/$(TESTDIR)
	$(CC) $(CFLAG) -D_TEST $(DEFS) $(INCS) -c -o $@ $<

$(DEPS)/$(TESTDIR)/%.d: $(TESTDIR)/%.cpp
	@mkdir -p $(DEPS)/$(TESTDIR)
	@$(CC) $(CFLAG) -D_TEST $(DEFS) $(INCS) $< -M | sed 's:.*\.o:${OBJS}/${TESTDIR}/\0:' > $@

$(DEPS)/$(LANG_DIR)/%.d: $(LANG_DIR)/%.cpp
	@mkdir -p $(DEPS)/$(LANG_DIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed -e 's/\(.*\)\.o/${LANG_DIR}\/lib${LIBNAME}_\1\.a/g' > $@
	
$(LANG_DIR)/lib$(LIBNAME)_%.a: $(LANG_DIR)/%.cpp
	@$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm $@.o
	
$(KVS_DIR)/lib$(LIBNAME)_%.a: $(KVS_DIR)/%.cpp
	@$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm $@.o



#-----------------------------------------------------------------------
#	make target
#-----------------------------------------------------------------------
all: $(LIBRARY) $(UNITTEST)

test: all
	./$(TESTDIR)/$(UNITTEST) $(RUNTEST)
	
$(LIBRARY): $(LIBOBJS)
	$(AR) $(LIBRARY) $(LIBOBJS)

$(UNITTEST) : $(TESTOBJS)
	$(CC) -o $(TESTDIR)/$(UNITTEST) $(CFLAG) $(LNKOPT) $(DEFS) $(INCS) $(TESTOBJS) $(LIBS) $(TESTLIBS) $(LANG_LIBS) $(KVS_LIBS)

clean:
	rm -f $(LIBOBJS) $(TESTOBJS) $(TESTDIR)/$(UNITTEST) $(LIBRARY) $(COREFILE) $(LANG) $(KVS) $(TESTRESULT)
	
cleanup: clean
	rm -rf $(OBJS) $(DEPS)


#-----------------------------------------------------------------------
#	make dependency
#-----------------------------------------------------------------------
-include $(LIBDEPS)
-include $(TESTDEPS)
-include $(PLUGIN_DEPS)
