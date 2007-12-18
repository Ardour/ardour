include $(top_srcdir)/examples/Makefile.am_fragment

INCLUDES += $(GTHREAD_CFLAGS)

## Build the executable, but don't install it.
noinst_PROGRAMS = dispatcher dispatcher2 thread threadpool

dispatcher_SOURCES = dispatcher.cc
dispatcher_LDFLAGS = $(GTHREAD_LIBS)

dispatcher2_SOURCES = dispatcher2.cc
dispatcher2_LDFLAGS = $(GTHREAD_LIBS)

thread_SOURCES = thread.cc
thread_LDFLAGS = $(GTHREAD_LIBS)

threadpool_SOURCES = threadpool.cc
threadpool_LDFLAGS = $(GTHREAD_LIBS)

