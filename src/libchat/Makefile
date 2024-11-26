COURSE = cs551


COURSE_INCLUDE_DIR = $(HOME)/$(COURSE)/include
COURSE_LIB_DIR = $(HOME)/$(COURSE)/lib

SQLITE3_DIR = /usr/lib/x86_64-linux-gnu

CC = gcc

CFLAGS = -g -Wall -std=gnu17 -I$(COURSE_INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(COURSE_LIB_DIR) 
LDLIBS = -lcs551 -lsqlite3

TEST_CHAT_DB_TARGET = test-chat-db
TEST_MSGARGS_TARGET = test-msgargs
LIB_TARGET = libchat.so

ifdef TEST_CHAT_DB
  TARGET = $(TEST_CHAT_DB_TARGET)
  CFLAGS += -DTEST_CHAT_DB
  ifdef MANUAL_TEST_CHAT_DB
    CFLAGS += -DMANUAL_TEST_CHAT_DB
  endif
else ifdef TEST_MSGARGS
  TARGET = $(TEST_MSGARGS_TARGET)
  CFLAGS += -DTEST_MSGARGS
else
  TARGET = $(LIB_TARGET)
  CFLAGS += -fPIC
endif

C_FILES = $(wildcard *.c)
O_FILES = $(patsubst %.c, %.o, $(C_FILES))


.PHONY:		all
all:		$(TARGET)


$(LIB_TARGET):		$(O_FILES)
			$(CC) -shared $(LDFLAGS) $(O_FILES) $(LDLIBS) -o $@

$(TEST_CHAT_DB_TARGET):	$(O_FILES)
			$(CC) $(CFLAGS) $(LDFLAGS) $(O_FILES) $(LDLIBS) -o $@

$(TEST_MSGARGS_TARGET):	$(O_FILES)
			$(CC) $(CFLAGS) $(LDFLAGS) $(O_FILES) $(LDLIBS) -o $@

install:	$(LIB_TARGET)
		cp $(LIB_TARGET) $(HOME)/$(COURSE)/lib
		cp *.h $(HOME)/$(COURSE)/include


CPP_TO_SQL = sed -e '/ *\#/d' -e 's@//@--@' -e 's/\\$$//'

%.sql:			%.sql.cpp
			$(CPP_TO_SQL) -e 's/@{FILE}/$</' $< > $@

.PHONY:		clean
clean:
		rm -rf *~ *.o $(LIB_TARGET) $(TEST_TARGET) $(DEPDIR)


#auto-dependences
#<http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/>

#dir in which to generate dependences
DEPDIR = .deps

#have DEPDIR/*.d file for each *.c file
DEPFILES = $(C_FILES:%.c=$(DEPDIR)/%.d)

#-MT $@ sets target name in dependency file
#-MMD tells compiler to generate prereqs without including system headers
#-MP adds target for each prereq
#-MF $(DEPDIR)/$*.d sets name of output dependency file
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

#delete built-in implicit rule for building .o from .c
%.o: %.c

#create our own implicit rule for building %.o from %.c
#| $(DEPDIR) makes DEPDIR an order only prereq
%.o:		%.c $(DEPDIR)/%.d | $(DEPDIR)
		$(CC) $(DEPFLAGS) $(CFLAGS) -c $<

#create DEPDIR if it does not exist
$(DEPDIR):
		@mkdir -p $@

#add a target to include all DEPFILES
$(DEPFILES):
include $(wildcard $(DEPFILES))

