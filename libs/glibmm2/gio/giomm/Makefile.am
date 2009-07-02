## Copyright (c) 2007
## The gtkmm development team.

SUBDIRS			= private

sublib_name		= giomm
sublib_libname		= giomm-2.4
sublib_libversion       = $(LIBGLIBMM_SO_VERSION)
sublib_namespace	= Gio
sublib_cflags		= $(GIOMM_CFLAGS)
sublib_topdir = gio
sublib_win32_dlls_libs	=

sublib_files_extra_posix_cc =
sublib_files_extra_win32_cc =
sublib_files_extra_general_cc = init.cc contenttype.cc slot_async.cc
sublib_files_extra_general_deprecated_cc = 

sublib_files_extra_posix_h =
sublib_files_extra_win32_h =
sublib_files_extra_general_h = init.h contenttype.h
sublib_files_extra_general_deprecated_h = 

include $(top_srcdir)/build_shared/Makefile_build.am_fragment

lib_LTLIBRARIES		= libgiomm-2.4.la
libgiomm_2_4_la_SOURCES	= $(files_built_cc) $(files_extra_cc)
libgiomm_2_4_la_LDFLAGS	= $(common_ldflags)
libgiomm_2_4_la_LIBADD	= $(GIOMM_LIBS) \
                          $(top_builddir)/glib/glibmm/libglibmm-2.4.la

# this header should be distributed, but not installed
EXTRA_DIST=slot_async.h
