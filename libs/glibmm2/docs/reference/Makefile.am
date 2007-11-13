doxygen_configfile = Doxyfile
doxygen_configfile_source = $(srcdir)/Doxyfile.in
beautify_docs_source = $(srcdir)/beautify_docs.pl.in
beautify_docs = $(srcdir)/beautify_docs.pl

include $(top_srcdir)/docs/Makefile_web.am_fragment
web_path_reference = $(web_path_docs)reference

EXTRA_DIST = README Doxyfile.in \
             glibmm_header.html_fragment glibmm_footer.html_fragment beautify_docs.pl \
             html glibmm_doxygen_tags libstdc++.tag.xml
#Ideally, libstdc++.tag.xml would be installed by gcc, instead of being copied into every project.

html/index.html: $(doxygen_configfile_source) $(beautify_docs_source) $(top_srcdir)/glib/glibmm/*.h
	-rm -rf html
	image_path=""; \
	dir="`pkg-config --silence-errors --variable=prefix gtk+-2.0`/share/gtk-doc/html/gtk"; \
	test -d "$$dir" && image_path="$$dir"; \
	dir="$(top_srcdir)/../gtk+/gtk/stock-icons"; \
	test -d "$$dir" && image_path="$$image_path $$dir"; \
	DOXYGEN_IMAGE_PATH="$${DOXYGEN_IMAGE_PATH:=$$image_path}" doxygen $(doxygen_configfile)
	$(PERL_PATH) $(beautify_docs) html


glibmm-reference-html.tar.gz: html/index.html
	tar cf - html | gzip -c --best >$@

glibmm-reference-html.tar.bz2: html/index.html
	tar cf - html | bzip2 -c >$@

post-html: html/index.html
	rsync $(rsync_args) -r html/ $$USER@$(web_host):$(web_path_reference)/html/

doc-clean:
	-rm -rf html
	-rm -f glibmm_doxygen_tags
	-rm -f glibmm-reference-html.tar.gz glibmm-reference-html.tar.bz2

referencedir = $(gtkmm_docdir)/reference/html

install-reference: html/index.html
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(referencedir)
	@dir='$(<D)'; for p in $$dir/*.html $$dir/*.css $$dir/*.png $$dir/*.dot ; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(referencedir)/$$f"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(referencedir)/$$f; \
	done

uninstall-reference: html/index.html
	@$(NORMAL_UNINSTALL)
	@dir='$(<D)'; for p in $$dir/*.html $$dir/*.css $$dir/*.png $$dir/*.dot ; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $(DESTDIR)$(referencedir)/$$f"; \
	  rm -f $(DESTDIR)$(referencedir)/$$f; \
	done

# Install beautify_docs.pl, next to gmmproc.
gmmproc_bin_SCRIPTS = $(beautify_docs)
gmmproc_bindir = $(libdir)/glibmm-2.4/proc

all-local: html/index.html

install-data-local: install-reference

uninstall-local: uninstall-reference

maintainer-clean-local: doc-clean

.PHONY: post-html doc-clean install-reference uninstall-reference

