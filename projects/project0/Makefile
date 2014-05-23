SUFFIXES+=.d
NODEPS=clean
CC=gcc
CFLAGS=-std=gnu99 -g -Wall -O0
SRCS=$(shell find . -maxdepth 1 -name "*.c")
DEPFILES=$(patsubst %.c, %.d, $(SRCS))
OBJS=queuetest.o hashtest.o queue.o hash.o
PROGRAMS=queuetest hashtest

default: all

all: queuetest hashtest

queuetest: queuetest.o queue.o
	$(CC) $(CFLAGS) $^ -o $@

hashtest: hashtest.o hash.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c %.d
	$(CC) $(CFLAGS) -o $@ -c $<

%.d: %.c
	$(CC) $(CXXFLAGS) -MM -MT '$(patsubst %.c,%.o,$<)' $< -MF $@

clean:
	rm -f $(OBJS) $(PROGRAMS) $(DEPFILES)

# Don't generate dependencies for all rules
ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
	-include $(DEPFILES)
endif
