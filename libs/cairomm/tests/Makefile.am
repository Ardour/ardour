if AUTOTESTS

# build automated 'tests'
noinst_PROGRAMS = test-context
test_context_SOURCES=test-context.cc
TESTS=test-context

else

#don't build anything
TESTS=

endif

#Where to find the header files needed by the source files:
INCLUDES =  -I$(top_srcdir) @CAIROMM_CFLAGS@

#The libraries that the executable needs to link against:
LIBS = $(top_builddir)/cairomm/libcairomm-1.0.la @LIBS@ @CAIROMM_LIBS@ @BOOST_UNIT_TEST_FRAMEWORK_LIB@
