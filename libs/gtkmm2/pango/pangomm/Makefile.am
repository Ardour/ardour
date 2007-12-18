## Copyright (c) 2001
## The gtkmm development team.

SUBDIRS			= private

sublib_name		= pangomm
sublib_libname		= pangomm-1.4
sublib_libversion = $(LIBGTKMM_SO_VERSION)
sublib_namespace	= Pango
sublib_cflags		= $(PANGOMM_CFLAGS)
sublib_topdir = pango
sublib_win32_dlls_libs	= 


sublib_files_extra_posix_cc =
sublib_files_extra_win32_cc =
sublib_files_extra_general_cc = init.cc
sublib_files_extra_general_deprecated_cc =

sublib_files_extra_posix_h =
sublib_files_extra_win32_h =
sublib_files_extra_general_h = init.h types.h
sublib_files_extra_general_deprecated_h = 

include $(top_srcdir)/build_shared/Makefile_build.am_fragment

lib_LTLIBRARIES = libpangomm-1.4.la
libpangomm_1_4_la_SOURCES	= $(files_built_cc) $(files_extra_cc)
libpangomm_1_4_la_LDFLAGS	= $(common_ldflags)
libpangomm_1_4_la_LIBADD	= $(win32_dlls_extra_libs) $(PANGOMM_LIBS)

