ACLOCAL_AMFLAGS	= -I scripts

SUBDIRS = glibmm
DIST_SUBDIRS	= $(SUBDIRS)

EXTRA_DIST	= build_shared/Makefile_build.am_fragment \
		  build_shared/Makefile_build_gensrc.am_fragment \
		  build_shared/Makefile_gensrc.am_fragment \
		  build_shared/Makefile_gensrc_platform.am_fragment \
		  build_shared/Makefile_build_extra.am_fragment \
		  CHANGES glibmmconfig.h.in glibmm-2.4.pc.in

glibmm_includedir       = $(includedir)/glibmm-2.4
glibmm_include_HEADERS  = glibmm.h

glibmm_configdir        = $(libdir)/glibmm-2.4/include
glibmm_config_DATA      = glibmmconfig.h

pkgconfigdir            = $(libdir)/pkgconfig
pkgconfig_DATA          = glibmm-2.4.pc

all-local:
	@echo "*** Everything completed ***"
