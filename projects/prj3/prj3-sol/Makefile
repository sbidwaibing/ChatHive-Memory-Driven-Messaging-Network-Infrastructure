COURSE = cs551


INCLUDE_DIR = $(HOME)/$(COURSE)/include
LIB_DIR = $(HOME)/$(COURSE)/lib

CC = gcc

CFLAGS = -g -Wall -std=gnu17 -I$(INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS = -lcs551 -lchat

TARGETS = chatd chatc

# object files needed by client (must include mock.o needed for testing)
CLIENT_OFILES = \
  chatc.o \
  chat.o \
  client.o \
  common.o \
  mock.o \
  utils.o

# object files needed by server (must include mock.o needed for testing)
SERVER_OFILES = \
  chatd.o \
  common.o \
  server-loop.o \
  mock.o \
  utils.o 


#default target
.PHONY:		all
all:		$(TARGETS)

chatc:		$(CLIENT_OFILES)
		$(CC)  $(LDFLAGS) $^  $(LDLIBS) -o $@

chatd:		$(SERVER_OFILES)
		$(CC)  $(LDFLAGS) $^  $(LDLIBS) -o $@


# use "make clean" to remove generated or backup files.
.PHONY:		clean
clean:
		rm -rf  *.o $(TARGETS) :memory: *.bak *~


# use "make DEPEND" to generate dependencies which can be
# pasted in below.
.PHONY:		DEPEND
DEPEND:
		gcc -MM  *.c

chat.o: chat.c chat.h client.h utils.h
chatc.o: chatc.c chat.h
chatd.o: chatd.c common.h server-loop.h utils.h
client.o: client.c client.h chat.h common.h
common.o: common.c common.h
server-loop.o: server-loop.c server-loop.h common.h
utils.o: utils.c utils.h
