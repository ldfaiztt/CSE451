bin_PROGRAMS = test-create test-join test-mutex test-cond test-preempt test-burgers

# these are run by 'make check'
TESTS = test-create test-join test-mutex test-cond test-preempt

ldadd = ../lib/libsthread.la
AM_LDFLAGS = ../lib/sthread_start.o
LDADD = $(ldadd)
INCLUDES = -I ../include

test_create_SOURCES = test-create.c

test_join_SOURCES = test-join.c

test_mutex_SOURCES = test-mutex.c

test_cond_SOURCES = test-cond.c

test_preempt_SOURCES = test-preempt.c

test_burgers_SOURCES = test-burgers.c
