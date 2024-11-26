COURSE = cs551

LIB = libcs551.so

TARGET = $(LIB)

INCLUDE_DIR = $(HOME)/$(COURSE)/include

CC = gcc
CFLAGS = -g -Wall -std=c17 -fPIC
#LDFLAGS = -lm

#produce a list of all cc files
C_FILES = $(wildcard *.c)

#produce a list of all *.o files by substituting c in C_FILES with o.
OFILES = $(C_FILES:%.c=%.o)

#default target
all:		$(TARGET)

$(TARGET):  	$(OFILES)
		$(CC) -shared $(OFILES) -o $@


test-str-space:	str-space.c str-space.h
		$(CC) -DTEST_STR_SPACE $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@


install:	$(TARGET)
		cp $(TARGET) $(HOME)/$(COURSE)/lib
		cp *.h $(HOME)/$(COURSE)/include


.PHONY:		clean
clean:
		rm -rf *~ *.o $(TARGET) $(DEPDIR)


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

