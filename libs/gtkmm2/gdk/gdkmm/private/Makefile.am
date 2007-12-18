## Copyright (c) 2001
## The gtkmm development team.

include $(srcdir)/../../src/Makefile_list_of_hg.am_fragment
files_built_p_h = $(files_all_hg:.hg=_p.h)

private_includedir	= $(includedir)/gdkmm-2.4/gdkmm/private
private_include_HEADERS	= $(files_built_p_h)

maintainer-clean-local:
	(cd $(srcdir) && rm -f $(files_built_p_h))



