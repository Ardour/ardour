## Copyright (c) 2001
## The gtkmm development team.

SUBDIRS = 

sublib_name = glibmm
sublib_libname = glibmm-2.4
sublib_namespace = Glib
sublib_cflags = $(GLIBMM_CFLAGS)

sublib_files_extra_posix_cc =

sublib_files_general_cc = \
  class.cc \
  containers.cc \
  debug.cc \
  dispatcher.cc \
  error.cc \
  exception.cc \
  exceptionhandler.cc \
  init.cc \
  interface.cc \
  main.cc \
  miscutils.cc \
  object.cc \
  objectbase.cc \
  pattern.cc \
  property.cc \
  propertyproxy.cc \
  propertyproxy_base.cc \
  quark.cc \
  random.cc \
  signalproxy.cc \
  signalproxy_connectionnode.cc  \
  streamiochannel.cc \
  stringutils.cc \
  threadpool.cc \
  timer.cc \
  timeval.cc \
  ustring.cc \
  utility.cc \
  value.cc \
  value_custom.cc \
  wrap.cc

sublib_files_posix_h =

sublib_files_general_h = \
  arrayhandle.h \
  class.h \
  containerhandle_shared.h  \
  containers.h \
  debug.h \
  dispatcher.h \
  error.h \
  exception.h \
  exceptionhandler.h \
  helperlist.h \
  init.h \
  i18n.h \
  interface.h \
  iochannel.h \
  listhandle.h \
  main.h \
  miscutils.h \
  object.h \
  objectbase.h \
  pattern.h \
  property.h \
  propertyproxy.h \
  propertyproxy_base.h \
  quark.h \
  random.h \
  refptr.h \
  sarray.h \
  signalproxy_connectionnode.h  \
  slisthandle.h \
  streamiochannel.h \
  stringutils.h \
  threadpool.h \
  timer.h \
  timeval.h \
  ustring.h \
  utility.h \
  value.h \
  value_custom.h \
  wrap.h

files_built_general_cc  = value_basictypes.cc
files_built_general_h  = signalproxy.h value_basictypes.h

lib_LTLIBRARIES = libglibmm-2.4.la
libglibmm_2_4_la_SOURCES = $(files_all_general_cc) $(files_all_posix_cc)
libglibmm_2_4_la_LDFLAGS = $(common_ldflags)
libglibmm_2_4_la_LIBADD = $(GLIBMM_LIBS)

