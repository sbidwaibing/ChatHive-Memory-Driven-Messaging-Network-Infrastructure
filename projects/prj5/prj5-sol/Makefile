COURSE = cs551


INCLUDE_DIR = $(HOME)/$(COURSE)/include
LIB_DIR = $(HOME)/$(COURSE)/lib

CC = gcc

CFLAGS = -g -Wall -std=gnu17 -I$(INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS = -lcs551 -lchat -lpthread

TARGETS = chatd chatc select-chatc

# object files needed by client (must include mock.o needed for testing)
CLIENT_OFILES = \
  chatc.o \
  chat.o \
  common.o \

SELECT_CLIENT_OFILES = \
  select-chatc.o \
  select-chat.o \
  common.o \


# object files needed by server (must include mock.o needed for testing)
SERVER_OFILES = \
  chatd.o \
  common.o \
  server.o \


#default target
.PHONY:		all
all:		$(TARGETS)

chatc:		$(CLIENT_OFILES)
		$(CC)  $(LDFLAGS) $^  $(LDLIBS) -o $@

chatd:		$(SERVER_OFILES)
		$(CC)  $(LDFLAGS) $^  $(LDLIBS) -o $@


select-chatc:	$(SELECT_CLIENT_OFILES)
		$(CC)  $(LDFLAGS) $^  $(LDLIBS) -o $@

# use "make clean" to remove generated or backup files.
.PHONY:		clean
clean:
		rm -rf  *.o $(TARGETS) :memory: *~


# use "make DEPEND" to generate dependencies which can be
# pasted in below.
.PHONY:		DEPEND
DEPEND:
		gcc -MM  *.c

chat.o: chat.c chat.h common.h
chatc.o: chatc.c chat.h common.h
chatd.o: chatd.c common.h server.h
common.o: common.c common.h
select-chat.o: select-chat.c select-chat.h common.h
select-chatc.o: select-chatc.c select-chat.h common.h
server.o: server.c server.h common.h
