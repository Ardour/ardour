## Copyright (c) 2001
## The gtkmm development team.

SUBDIRS			= private

sublib_name		= gdkmm
sublib_libname		= gdkmm-2.4
sublib_libversion = $(LIBGTKMM_SO_VERSION)
sublib_namespace	= Gdk
sublib_cflags		= $(GDKMM_CFLAGS)
sublib_topdir = gdk
sublib_win32_dlls_libs	=

sublib_files_extra_posix_cc =
sublib_files_extra_win32_cc =
sublib_files_extra_general_cc = general.cc rgb.cc
sublib_files_extra_general_deprecated_cc = 

sublib_files_extra_posix_h =
sublib_files_extra_win32_h =
sublib_files_extra_general_h = list.h general.h rgb.h
sublib_files_extra_general_deprecated_h = 

include $(top_srcdir)/build_shared/Makefile_build.am_fragment

lib_LTLIBRARIES		= libgdkmm-2.4.la
libgdkmm_2_4_la_SOURCES	= $(files_built_cc) $(files_extra_cc)
libgdkmm_2_4_la_LDFLAGS	= $(common_ldflags)
libgdkmm_2_4_la_LIBADD	= $(top_builddir)/pango/pangomm/libpangomm-1.4.la \
			  $(GDKMM_LIBS)

