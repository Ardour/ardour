## Copyright (c) 2001
## The gtkmm development team.

SUBDIRS			= private

sublib_name		= gtkmm
sublib_libname		= gtkmm-2.4
sublib_libversion = $(LIBGTKMM_SO_VERSION)
sublib_namespace	= Gtk
sublib_cflags		= $(GTKMM_CFLAGS)
sublib_topdir = gtk
sublib_win32_dlls_libs	=

sublib_files_extra_posix_cc = 
sublib_files_extra_win32_cc =
sublib_files_extra_general_cc = accelkey.cc accelmap.cc cellrenderer_generation.cc comboboxtext.cc comboboxentrytext.cc \
                                listviewtext.cc \
                                menu_elems.cc radiobuttongroup.cc selectiondata_private.cc stock.cc stockid.cc targetentry.cc \
                                treemodelcolumn.cc treeview_private.cc
sublib_files_extra_general_deprecated_cc = 

sublib_files_extra_posix_h = 
sublib_files_extra_win32_h =
sublib_files_extra_general_h = accelkey.h accelmap.h base.h cellrenderer_generation.h comboboxtext.h comboboxentrytext.h\
                               listviewtext.h \
                               menu_elems.h radiobuttongroup.h selectiondata_private.h stock.h stockid.h targetentry.h \
                               treemodelcolumn.h treeview_private.h
sublib_files_extra_general_deprecated_h = 

include $(top_srcdir)/build_shared/Makefile_build.am_fragment

lib_LTLIBRARIES		= libgtkmm-2.4.la
libgtkmm_2_4_la_SOURCES	= $(files_built_cc) $(files_extra_cc)
libgtkmm_2_4_la_LDFLAGS	= $(common_ldflags)
libgtkmm_2_4_la_LIBADD	= $(top_builddir)/gdk/gdkmm/libgdkmm-2.4.la \
			  $(top_builddir)/pango/pangomm/libpangomm-1.4.la \
			  $(LIBADD_ATK) \
			  $(GTKMM_LIBS)

if GTKMM_ATKMM_ENABLED
libgtkmm_2_4_la_LIBADD += $(top_builddir)/atk/atkmm/libatkmm-1.6.la
endif
