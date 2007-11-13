## Process this file with automake to produce Makefile.in

SUBDIRS = cairomm examples MSVC $(DOCS_SUBDIR)
#docs examples

EXTRA_DIST = MAINTAINERS cairomm-1.0.pc.in

DIST_SUBDIRS = $(SUBDIRS)

# Install the pkg-config file:
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cairomm-1.0.pc

# Copied from cairo/Makefile.am:
#
# Some custom targets to make it easier to release things.
# Use either:
#		make release-check
# or		make release-publish

RELEASE_UPLOAD_HOST =   cairographics.org
RELEASE_UPLOAD_BASE =	/srv/cairo.freedesktop.org/www
RELEASE_UPLOAD_DIR =	$(RELEASE_UPLOAD_BASE)/releases
RELEASE_URL_BASE = 	http://cairographics.org/releases
RELEASE_ANNOUNCE_LIST = cairo-announce@cairographics.org (and CC gnome-announce-list@gnome.org)
CVS = cvs

#MANUAL_DATED =		cairo-manual-`date +%Y%m%d`
#MANUAL_TAR_FILE = 	$(MANUAL_DATED).tar.gz
#MANUAL_UPLOAD_DIR =	$(RELEASE_UPLOAD_BASE)

tar_file = $(PACKAGE)-$(VERSION).tar.gz
sha1_file = $(tar_file).sha1
gpg_file = $(sha1_file).asc

$(sha1_file): $(tar_file)
	sha1sum $^ > $@

$(gpg_file): $(sha1_file)
	@echo "Please enter your GPG password to sign the checksum."
	gpg --armor --sign $^ 

release-verify-even-micro:
	@echo -n "Checking that $(VERSION) has an even micro component..."
	@test "$(GENERIC_MICRO_VERSION)" = "`echo $(GENERIC_MICRO_VERSION)/2*2 | bc`" \
		|| (echo "Ouch." && echo "The version micro component '$(GENERIC_MICRO_VERSION)' is not an even number." \
		&& echo "The version in configure.in must be incremented before a new release." \
		&& false)
	@echo "Good."

release-verify-newer:
	@echo -n "Checking that no $(VERSION) release already exists..."
	@ssh $(RELEASE_UPLOAD_HOST) test ! -e $(RELEASE_UPLOAD_DIR)/$(tar_file) \
		|| (echo "Ouch." && echo "Found: $(RELEASE_UPLOAD_HOST):$(RELEASE_UPLOAD_DIR)/$(tar_file)" \
		&& echo "Are you sure you have an updated CVS checkout?" \
		&& echo "This should never happen." \
		&& false)
	@echo "Good."

release-remove-old:
	rm -f $(tar_file) $(sha1_file) $(gpg_file)

# Maybe it's just my system, but somehow group sticky bits keep
# getting set and this causes failures in un-tarring on some systems.
# Until I figure out where the sticky bit is coming from, just clean
# these up before building a release.
release-cleanup-group-sticky:
	find . -type f | xargs chmod g-s

release-check: release-verify-even-micro release-verify-newer release-remove-old release-cleanup-group-sticky distcheck

release-upload: release-check $(tar_file) $(sha1_file) $(gpg_file)
	mkdir -p releases
	scp $(tar_file) $(sha1_file) $(gpg_file) $(RELEASE_UPLOAD_HOST):$(RELEASE_UPLOAD_DIR)
	mv $(tar_file) $(sha1_file) $(gpg_file) releases
	ssh $(RELEASE_UPLOAD_HOST) "rm -f $(RELEASE_UPLOAD_DIR)/LATEST-$(PACKAGE)-[0-9]* && ln -s $(tar_file) $(RELEASE_UPLOAD_DIR)/LATEST-$(PACKAGE)-$(VERSION)"
	$(CVS) tag RELEASE_$(GENERIC_MAJOR_VERSION)_$(GENERIC_MINOR_VERSION)_$(GENERIC_MICRO_VERSION)

release-publish: release-upload releases/$(sha1_file)
	@echo ""
	@echo "Please send an announcement to $(RELEASE_ANNOUNCE_LIST)"
	@echo "including the following:"
	@echo ""
	@echo "Subject: $(PACKAGE) release $(VERSION) now available"
	@echo ""
	@echo "============================== CUT HERE =============================="
	@echo "cairomm is a C++ API for the cairo graphics library.  For more "
	@echo "information, see http://cairographics.org/cairomm"
	@echo ""
	@echo "A new $(PACKAGE) release $(VERSION) is now available from:"
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(tar_file)"
	@echo ""
	@echo "    which can be verified with:"
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(sha1_file)"
	@echo -n "	"
	@cat releases/$(sha1_file)
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(gpg_file)"
	@echo "	(signed by `getent passwd "$$USER" | cut -d: -f 5 | cut -d, -f 1`)"
	@echo ""
	@echo "WHAT'S NEW"
	@echo "=========="
	@echo ""
	@echo "============================== CUT HERE =============================="
	@echo "Also, please include the new entries from the NEWS file."
	@echo ""
	@echo "Last but not least, do not forget to bump up the micro"
	@echo "version component to the next (odd) number and commit."


doc-clean-recursive:
	(cd docs && $(MAKE) $(AM_MAKEFLAGS) doc-clean)

doc-clean: doc-clean-recursive

doc-rebuild:
	(cd docs && $(MAKE) $(AM_MAKEFLAGS) doc-rebuild)

.PHONY: release-verify-even-micro release-verify-newer release-remove-old release-cleanup-group-sticky release-check release-upload release-publish doc-clean doc-clean-recursive doc-rebuild

