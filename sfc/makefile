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
CFLAG = -Wall -g -fPIC -D_DEBUG -D_TEST
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
NBRCOREDIR = $(NBRDIR)/core
COREFILE = core.*
LIBS = -L$(NBRDIR) -L. -L./lib -lsfc -lnbr
INCS = -I$(NBRCOREDIR) -I. -Icore -Ilib -Itest 
DEPS = dep
OBJS = obj


#-----------------------------------------------------------------------
#	libsfc
#-----------------------------------------------------------------------
LIBDIR = core
LIBSRCS = $(shell find $(LIBDIR) -regex .*\\.cpp$$ -printf %f\\n)
LIBOBJS = $(LIBSRCS:%.cpp=$(OBJS)/$(LIBDIR)/%.o)
LIBDEPS = $(LIBSRCS:%.cpp=$(DEPS)/$(LIBDIR)/%.d)
LIBREQLIBS = nbr
LIBNAME = sfc
LIBRARY = lib$(LIBNAME).a

#-----------------------------------------------------------------------
#	test
#-----------------------------------------------------------------------
TESTDIR = test
TESTSRCS = $(shell find $(TESTDIR) -regex .*\\.cpp$$ -printf %f\\n)
TESTOBJS = $(TESTSRCS:%.cpp=$(OBJS)/$(TESTDIR)/%.o)
TESTDEPS = $(TESTSRCS:%.cpp=$(DEPS)/$(TESTDIR)/%.d)
TESTREQLIBS = z bz2 rt m ssl crypto
TESTLIBS = $(foreach lib,$(TESTREQLIBS),$(shell echo "-l$(lib)"))
UNITTEST = ut
RUNTEST = any
TESTRESULT = resp.*

#-----------------------------------------------------------------------
#	lib setting
#-----------------------------------------------------------------------
PLUGIN_DIR = lib
PLUGIN_SRCS = $(shell find $(PLUGIN_DIR) -regex .*\\.cpp$$ -printf %f\\n)
PLUGIN = $(PLUGIN_SRCS:%.cpp=$(PLUGIN_DIR)/lib$(LIBNAME)_%.a)
LIBS += $(PLUGIN_SRCS:%.cpp=-l$(LIBNAME)_%)
PLUGIN_DEPS = $(PLUGIN_SRCS:%.cpp=$(DEPS)/$(PLUGIN_DIR)/%.d)

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
	$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@ $<

$(DEPS)/$(TESTDIR)/%.d: $(TESTDIR)/%.cpp
	@mkdir -p $(DEPS)/$(TESTDIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed 's:.*\.o:${OBJS}/${TESTDIR}/\0:' > $@

$(DEPS)/$(PLUGIN_DIR)/%.d: $(PLUGIN_DIR)/%.cpp
	@mkdir -p $(DEPS)/$(PLUGIN_DIR)
	@$(CC) $(CFLAG) $(DEFS) $(INCS) $< -M | sed -e 's/\(.*\)\.o/${PLUGIN_DIR}\/lib${LIBNAME}_\1\.a/g' > $@
	
$(PLUGIN_DIR)/lib$(LIBNAME)_%.a: $(PLUGIN_DIR)/%.cpp
	@$(CC) $(CFLAG) $(DEFS) $(INCS) -c -o $@.o $<
	@$(AR) $@ $@.o
	@rm $@.o


#-----------------------------------------------------------------------
#	make target
#-----------------------------------------------------------------------
all: $(LIBRARY) $(PLUGIN) $(UNITTEST)

test: all
	./$(TESTDIR)/$(UNITTEST) $(RUNTEST)
	
$(LIBRARY): $(LIBOBJS)
	$(AR) $(LIBRARY) $(LIBOBJS)

$(UNITTEST) : $(TESTOBJS)
	$(CC) -o $(TESTDIR)/$(UNITTEST) $(CFLAG) $(LNKOPT) $(DEFS) $(INCS) $(TESTOBJS) $(LIBS) $(TESTLIBS) $(PLUGIN_LIBS)

clean:
	rm -f $(LIBOBJS) $(TESTOBJS) $(TESTDIR)/$(UNITTEST) $(LIBRARY) $(COREFILE) $(PLUGIN) $(TESTRESULT)
	
cleanup: clean
	rm -rf $(OBJS) $(DEPS)


#-----------------------------------------------------------------------
#	make dependency
#-----------------------------------------------------------------------
-include $(LIBDEPS)
-include $(TESTDEPS)
-include $(PLUGIN_DEPS)
