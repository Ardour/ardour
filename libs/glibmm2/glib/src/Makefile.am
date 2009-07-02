## Copyright (c) 2002
## The gtkmm development team.

sublib_name = glibmm
sublib_namespace = Glib
sublib_parentdir = glibmm
files_defs = glib.defs glib_enums.defs glib_functions.defs gobject.defs gobject_enums.defs gobject_functions.defs  gmodule_enums.defs gmodule_functions.defs glib_docs.xml glib_docs_override.xml

include $(top_srcdir)/build_shared/Makefile_gensrc.am_fragment

glibmm_files_m4 = signalproxy.h.m4 value_basictypes.cc.m4 value_basictypes.h.m4

files_stamp	+= $(glibmm_files_m4:%.m4=$(gensrc_destdir)/%)
EXTRA_DIST	+= $(glibmm_files_m4) template.macros.m4


$(gensrc_destdir)/%.cc: %.cc.m4 template.macros.m4
	$(M4) -I$(srcdir) $< >$@

$(gensrc_destdir)/%.h: %.h.m4 template.macros.m4
	$(M4) -I$(srcdir) $< >$@

