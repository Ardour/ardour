include $(top_srcdir)/examples/Makefile.am_fragment

# fork() does not work on win32
if OS_WIN32
else

#Build the executable, but don't install it.
noinst_PROGRAMS = child_watch
child_watch_SOURCES = main.cc

endif 
