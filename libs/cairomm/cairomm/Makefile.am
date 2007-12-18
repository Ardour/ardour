SUBDIRS = 

INCLUDES = -I$(top_srcdir) @CAIROMM_CFLAGS@

h_sources_public = cairomm.h context.h enums.h fontface.h fontoptions.h path.h pattern.h quartz_surface.h surface.h xlib_surface.h win32_surface.h exception.h refptr.h scaledfont.h
h_sources_private = private.h
cc_sources = context.cc fontface.cc fontoptions.cc path.cc pattern.cc quartz_surface.cc surface.cc xlib_surface.cc win32_surface.cc exception.cc scaledfont.cc
cc_sources_private = private.cc

# Support for DLL on cygwin/mingw using libtool > 1.4
if PLATFORM_WIN32
win32_dlls_ldflags = -no-undefined -Wl,--export-all-symbols
else
win32_dlls_ldflags =
endif

lib_LTLIBRARIES = libcairomm-1.0.la
libcairomm_1_0_la_LDFLAGS = -version-info $(GENERIC_LIBRARY_VERSION) $(win32_dlls_ldflags) 
libcairomm_1_0_la_LIBADD = @CAIROMM_LIBS@
libcairomm_1_0_la_SOURCES = $(cc_sources) $(h_sources_public) $(h_sources_private) $(cc_sources_private)

# Install the headers:
library_includedir=$(includedir)/cairomm-1.0/cairomm
library_include_HEADERS = $(h_sources_public)
