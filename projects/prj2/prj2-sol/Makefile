COURSE = cs551


INCLUDE_DIR = $(HOME)/$(COURSE)/include
LIB_DIR = $(HOME)/$(COURSE)/lib

CC = gcc

CFLAGS = -g -Wall -std=gnu17 -I$(INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS = -lcs551 -lchat

TARGET = chat

OFILES = \
  chat.o \
  client.o \
  common.o \
  main.o \
  server.o \
  server-loop.o \


#default target
.PHONY:		all
all:		$(TARGET)

$(TARGET):	$(OFILES)
		$(CC)  $(LDFLAGS) $(OFILES)  $(LDLIBS) -o $@


# use "make clean" to remove generated or backup files.
.PHONY:		clean
clean:
		rm -rf  *.o $(TARGET) :memory: *.bak *~


# use "make DEPEND" to generate dependencies which can be
# pasted in below.
.PHONY:		DEPEND
DEPEND:
		gcc -MM  *.c

chat.o: chat.c chat.h common.h server.h
common.o: common.c common.h
main.o: main.c chat.h
server.o: server.c server.h common.h
