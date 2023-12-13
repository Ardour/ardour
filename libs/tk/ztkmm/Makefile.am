## Copyright (c) 2010  Openismus GmbH  <http://www.openismus.com/>
##
## This file is part of atkmm.
##
## atkmm is free software: you can redistribute it and/or modify it
## under the terms of the GNU Lesser General Public License as published
## by the Free Software Foundation, either version 2.1 of the License,
## or (at your option) any later version.
##
## atkmm is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## See the GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

lib_LTLIBRARIES = libatkmm-@ATKMM_API_VERSION@.la

include $(srcdir)/../src/filelist.am
include $(srcdir)/filelist.am

files_built_cc = $(atkmm_files_built_cc)
files_built_h  = $(atkmm_files_built_h)
files_built_ph = $(atkmm_files_built_ph)
files_extra_cc = $(atkmm_files_extra_cc)
files_extra_h  = $(atkmm_files_extra_h)
files_extra_ph = $(atkmm_files_extra_ph)

include $(top_srcdir)/build/compile-binding.am

local_cppflags = $(binding_includes) $(binding_cppflags) -DATKMM_BUILD=1

AM_CPPFLAGS = $(local_cppflags) $(GTHREAD_CFLAGS) $(ATKMM_CFLAGS)
AM_CXXFLAGS = $(ATKMM_WXXFLAGS)

libatkmm_@ATKMM_API_VERSION@_la_SOURCES = $(binding_sources)
libatkmm_@ATKMM_API_VERSION@_la_LDFLAGS = -no-undefined -version-info $(ATKMM_SO_VERSION)
libatkmm_@ATKMM_API_VERSION@_la_LIBADD  = $(ATKMM_LIBS)
