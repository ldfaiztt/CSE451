INCLUDES = -I ../include
CFLAGS = -g -Wall -std=gnu99

lib_LTLIBRARIES = libsthread.la libsthread_start.la

# TMP is required for automake-1.6 compatibility
if USE_PTHREADS
TMP = sthread_pthread.c
endif

libsthread_la_SOURCES = sthread.c sthread_user.c \
			sthread_queue.c sthread_ctx.c sthread_util.c \
			sthread_preempt.c sthread_switch.S $(TMP) sthread_end.c

libsthread_start_la_SOURCES = sthread_start.c

noinst_HEADERS = sthread_pthread.h sthread_user.h sthread_queue.h \
		 sthread_ctx.h sthread_preempt.h sthread_switch_i386.h \
		 sthread_switch_x86_64.h

sthread_switch.lo : sthread_switch_i386.h sthread_switch_x86_64.h
