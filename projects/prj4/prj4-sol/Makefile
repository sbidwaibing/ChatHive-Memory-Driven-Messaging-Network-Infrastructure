COURSE = cs551


INCLUDE_DIR = $(HOME)/$(COURSE)/include
LIB_DIR = $(HOME)/$(COURSE)/lib

CC = gcc

CFLAGS = -g -Wall -std=gnu17 -I$(INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS = -lcs551 -lchat

TARGETS = chat

# object files needed by client (must include mock.o needed for testing)
OFILES = \
  chat.o \
  common.o \
  main.o \
  mock.o \
  server.o \
  server-loop.o


#default target
.PHONY:		all
all:		$(TARGETS)

chat:		$(OFILES)
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

chat.o: chat.c chat.h server.h common.h
common.o: common.c common.h
main.o: main.c chat.h
mock.o: mock.c
server-loop.o: server-loop.c server-loop.h common.h
server.o: server.c server.h common.h server-loop.h
