# Build the library, to be installed and used by other defs-generating code:

files_h = generate_extra_defs.h
files_cc = generate_extra_defs.cc

if PLATFORM_WIN32
no_undefined = -no-undefined
else
no_undefined =
endif

lib_LTLIBRARIES		= libglibmm_generate_extra_defs-2.4.la
libglibmm_generate_extra_defs_2_4_la_SOURCES = $(files_h) $(files_cc)
libglibmm_generate_extra_defs_2_4_la_LDFLAGS = $(no_undefined) -version-info $(LIBGLIBMM_SO_VERSION)
libglibmm_generate_extra_defs_2_4_la_LIBADD = $(GLIBMM_LIBS)

sublib_includedir	= $(includedir)/glibmm-2.4/glibmm_generate_extra_defs
sublib_include_HEADERS	= $(files_h)


# Build the generate_properties_defs utility executable.
# This is not used to build the rest of glibmm.

INCLUDES = $(GLIBMM_CFLAGS)
LIBS = $(GLIBMM_LIBS)

noinst_PROGRAMS = generate_extra_defs
generate_extra_defs_SOURCES = generate_defs_glib.cc
generate_extra_defs_LDADD = libglibmm_generate_extra_defs-2.4.la
