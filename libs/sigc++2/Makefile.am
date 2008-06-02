
# it includes all the autostuff automatically, you just name the
# other stuff here
EXTRA_DIST = autogen.sh sigc++config.h.in libsigc++-2.0.spec.in
# ACLOCAL_FLAGS = -I scripts

SUBDIRS      = sigc++ scripts
DIST_SUBDIRS = $(SUBDIRS)

sigc_configdir = $(libdir)/sigc++-2.0/include
sigc_config_DATA = sigc++config.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sigc++-2.0.pc

all-local:
	@echo "*** Everything completed ***"

