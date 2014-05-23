dnl # Simplethreads Instructional Threads Package
dnl #
dnl # configure.in: Autoconf spec
dnl # Borrows heavily from the GNU Pth configure.in

dnl #   standard Autoconf prolog
AC_PREREQ([2.68])dnl
AC_REVISION([$Revision: 1.21 $])


dnl #   autoconf initialization
dnl # this is the new style, but automake 1.5 doesn't like it
AC_INIT([simplethreads],[2.01])
AC_CONFIG_SRCDIR(lib/sthread.c)
AM_INIT_AUTOMAKE
dnl # AC_INIT
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([lib/sthread.c])
dnl # AM_INIT_AUTOMAKE(simplethreads, 1.30)
					
AC_CANONICAL_HOST

AC_PROG_CC
CFLAGS="-g -O0 -Wall"
AM_PROG_AS
CCASFLAGS="$CCASFLAGS -I ../include -D__ASM__"
AC_PROG_INSTALL
AC_PROG_MAKE_SET
dnl # Turn off shared libaries; must be before AC_PROG_LIBTOOL
AC_DISABLE_SHARED
AC_PROG_LIBTOOL

AC_HEADER_STDC
AC_CHECK_HEADERS(pthread.h assert.h)
AC_CHECK_HEADERS(sched.h sys/time.h sys/socket.h)
AC_CHECK_TYPES([socklen_t], [], [], [#include <sys/types.h>
#include <sys/socket.h>])
AC_CHECK_FUNCS(select sched_yield)
ACX_PTHREAD

AC_MSG_CHECKING([whether to use platform-native threads]);
AC_ARG_WITH([pthreads], [  --with-pthreads         use platform-native threads],
[case $with_pthreads in
      yes)      AC_MSG_RESULT(yes)
		AC_DEFINE(USE_PTHREADS, 1, [Define if you want platform-native threads.])
		dnl # compile everything for pthreads	
		LIBS="$PTHREAD_LIBS $LIBS"
		CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
		CC="$PTHREAD_CC"
		;;
      no)	AC_MSG_RESULT(no)
		;;
      *)        AC_MSG_ERROR([--with-pthreads does not take an argument.])
		;;
esac], AC_MSG_RESULT(no))
AM_CONDITIONAL(USE_PTHREADS, [test "x$with_pthreads" = "xyes"])

AC_MSG_CHECKING([whether to disable preemption]);
AC_ARG_WITH([preemption], [  --without-preemption         disable preemption],
[case $with_preemption in
      yes)      AC_MSG_RESULT(no)
		;;
      no)	AC_MSG_RESULT(yes)
		AC_DEFINE(DISABLE_PREEMPTION, 1, [Define if you want to disable preemption.])
		dnl # compile everything for pthreads	
		CFLAGS="$CFLAGS -DDISABLE_PREEMPTION"
		;;
      *)        AC_MSG_ERROR([--without-preemption does not take an argument.])
		;;
esac], AC_MSG_RESULT(no))
AM_CONDITIONAL(DISABLE_PREEMPTION, [test "x$with_preemption" = "xno"])

case $host_cpu in
i386 | i486 | i586 | i686 )
     AC_DEFINE(STHREAD_CPU_I386, 1, [Define to run on i386 CPUs.])
     ;;
x86_64 )
dnl _GNU_SOURCE is needed so that REG_RIP can be accessed in sthread_preempt.c
dnl (see /usr/include/sys/ucontext.h (#include <sys/ucontext.h>))
     AC_DEFINE(_GNU_SOURCE, 1, [Define to enable GNU extensions.])
     AC_DEFINE(STHREAD_CPU_X86_64, 1, [Define to run on x86_64 CPUs.])
     ;;
* )
     AC_MSG_ERROR([Only i386 and PowerPC support so far.])
     ;;
esac
		
dnl # AM 1.6 still requires AM_CONFIG_HEADER
dnl # AC_CONFIG_HEADERS(include/config.h)
AM_CONFIG_HEADER(include/config.h)
AC_CONFIG_FILES([Makefile include/Makefile lib/Makefile test/Makefile web/Makefile])
AC_OUTPUT
