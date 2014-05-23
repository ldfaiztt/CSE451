# 
# Makefile.am: The automake source for the Simplethreads makefile
#              run 'automake' to generate a new Makefile.in if you change
#              this file. Then run ./configure to generate the actual
#              Makefile. Isn't portability fun?
#

SUBDIRS = include lib test web
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = strip-solution rtest-all

solution_files = lib/sthread_user.c web/sioux_run.c web/web_queue.c web/web_queue.h

student-dist:
	STRIP_SOLUTION=$(srcdir)/strip-solution $(MAKE) $(AM_MAKEFLAGS) dist

dist-hook:
	if test x$(STRIP_SOLUTION) != x; then                     \
	    for file in $(solution_files); do                     \
		$(STRIP_SOLUTION) $(distdir)/$$file;              \
		if test x$$? != x0; then                           \
		     exit 1;                                      \
		fi;                                               \
		mv $(distdir)/$$file.strip $(distdir)/$$file;     \
            done                                                  \
	fi

# This target makes it easier to test a whole lot of platforms by
# automatically distributing, building, and running the checks.
remote-check-host: $(distdir).tar.gz
	@if test x$(RTEST_HOST) != x; then                       	\
		echo "Testing on $(RTEST_HOST)";		  	\
		scp ./$(distdir).tar.gz $(RTEST_HOST):rtest/;		\
		ssh $(RTEST_HOST) "(rm -rf rtest/$(distdir) && cd rtest/ && tar -xzf ./$(distdir).tar.gz)"; \
		RTEST_CONFIG=""						\
			$(MAKE) $(AM_MAKEFLAGS) remote-check-type;      \
		RTEST_CONFIG="--with-pthreads"				\
			$(MAKE) $(AM_MAKEFLAGS) remote-check-type;      \
	else							  	\
		echo "Set RTEST_HOST for remote-check";		  	\
	fi

# Helper to run configure && make check with different configure options
remote-check-type:
	ssh $(RTEST_HOST) "(cd rtest/$(distdir) && ./configure $(RTEST_CONFIG) && make clean && make check)"


