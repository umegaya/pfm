DEBUG=1

# output path
BIN = ./bin
NBR = ../../core/
EXL = ../../exlib/

# debug stuff 
ifeq ($(DEBUG), 1)
<<<<<<< HEAD
CFLAGS += -g -D_DEBUG
=======
CFLAGS += -g
>>>>>>> b9940129ad8b41943cc632ed6b1c135b8d91908a
else
CFLAGS += -O4
endif

AR  = ar -cqs
# itermidiate objects
OBJ = $(addprefix $(BIN)/, \
	  md5.o \
	  util_rbtree.o  \
	  conhash_inter.o \
	  conhash_util.o  \
	  conhash.o \
	  )

SAMPLE_OBJS = $(addprefix $(BIN)/, \
		sample.o \
		)
		
# include file path
INC = -I. -I. -I$(NBR) -I$(EXL)

TARGETS = $(BIN)/libconhash.a
TESTTARGETS = $(BIN)/sample
 
all : clean prepare $(TARGETS)
test : all $(TESTTARGETS)
	$(BIN)/sample

# build libconhash as a static lib 
$(BIN)/libconhash.a : $(OBJ)
	$(AR) $@ $(OBJ)
	
# build sample
$(BIN)/sample : $(SAMPLE_OBJS)
	gcc -O -o $@ $(SAMPLE_OBJS) -L. -L./bin -L../../ -lconhash -lnbr -lpthread

	
$(BIN)/%.o : %.c
	gcc $(INC) $(CFLAGS) -c $< -o $@	

# prepare the bin dir	
.PHONY : prepare	
prepare : 
		-mkdir $(BIN)
	  
.PHONY : clean
clean  :
		-rm -rf $(BIN)
