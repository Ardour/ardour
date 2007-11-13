## Copyright (c) 2001
## The gtkmm development team.

SUBDIRS = images reference internal

EXTRA_DIST = Makefile_web.am_fragment

gtkmm_doc_DATA = 

include $(top_srcdir)/docs/Makefile_web.am_fragment

# Post to the website.

post-html-recursive:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  test "$$subdir" = . || (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) post-html); \
	done

post-html-local: 
	scp $$SSH_OPT $^ $$USER@$(web_host):$(web_path_docs)

post-html: post-html-recursive post-html-local

doc-clean-recursive:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  test "$$subdir" = . || (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) doc-clean); \
	done

doc-clean: doc-clean-recursive

doc-rebuild: doc-clean all

.PHONY: post-html post-html-local post-html-recursive doc-clean doc-clean-recursive doc-rebuild

