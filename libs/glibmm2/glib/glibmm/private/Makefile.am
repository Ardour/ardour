## Copyright (c) 2001
## The gtkmm development team.

include $(srcdir)/../../src/Makefile_list_of_hg.am_fragment

files_built_p_h		= $(files_hg:.hg=_p.h)
files_built_all_p_h	= $(files_all_hg:.hg=_p.h)
files_extra_p_h		= interface_p.h object_p.h
files_extra_all_p_h	= interface_p.h object_p.h

dist_sources = $(files_built_all_p_h) $(files_extra_all_p_h)
DISTFILES = $(DIST_COMMON) $(dist_sources)

private_includedir	= $(includedir)/glibmm-2.4/glibmm/private
private_include_HEADERS	= $(files_built_p_h) $(files_extra_p_h)

maintainer-clean-local:
	(cd $(srcdir) && rm -f $(files_built_p_h))

