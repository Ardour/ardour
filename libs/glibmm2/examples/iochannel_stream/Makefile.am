include $(top_srcdir)/examples/Makefile.am_fragment

#Build the executable, but don't install it.

# Don't build natively (mingw32) on win32 as mkfifo is
# not supported (TODO: implement with CreateNamedPipe).
if OS_WIN32
else
noinst_PROGRAMS = example
example_SOURCES = main.cc fdstream.h fdstream.cc
endif
