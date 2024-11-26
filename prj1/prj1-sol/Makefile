COURSE = cs551

TARGET = chat

INCLUDE_DIR = $(HOME)/$(COURSE)/include
LIB_DIR = $(HOME)/$(COURSE)/lib

CC = gcc

CFLAGS = -g -Wall -std=gnu17 -I$(INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS = -lcs551

#MAIN_BUILD_FLAGS = -DTEST_MSG_ARGS -DNO_CHAT_IO_MAIN


OFILES = \
  chat-io.o \
  chat.o \
  errnum.o \
  msgargs.o 

#default target
all:		$(TARGET)

$(TARGET):	$(OFILES)
		$(CC)  $(LDFLAGS) $(OFILES)  $(LDLIBS) -o $@


.PHONY:		clean
clean:
		rm -rf *~ *.o $(TARGET) $(DEPDIR)

chat.o: chat.c chat.h errnum.h msgargs.h
chat-io.o: chat-io.c chat-io.h chat.h errnum.h msgargs.h
errnum.o: errnum.c errnum.h
msgargs.o: msgargs.c msgargs.h errnum.h


