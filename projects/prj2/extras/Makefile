COURSE = cs551

INCLUDE_DIR = $(HOME)/$(COURSE)/include
LIB_DIR = $(HOME)/$(COURSE)/lib

CC = gcc
CFLAGS = -g -Wall -std=gnu17 -I $(INCLUDE_DIR) $(MAIN_BUILD_FLAGS)
LDFLAGS = -L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LDLIBS = -lcs551 -lchat

MAIN_BUILD_FLAGS = -DNO_MAIN -DTEST_MAIN

#produce a list of all cc files
C_FILES = $(wildcard *.c)

#produce a list of all *.o files by substituting c in C_FILES with o.
OFILES = $(C_FILES:%.c=%.o)

TARGET = chat

#default target
.PHONY:		all
all:		$(TARGET)

$(TARGET):	$(OFILES)
		$(CC)  $(LDFLAGS) $(OFILES)  $(LDLIBS) -o $@

.PHONY:		clean
clean:
		rm -rf *~ *.o $(TARGETS) $(DEPDIR)


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
