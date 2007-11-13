
EXTRA_DIST = gtkmm_logo.gif top.gif

# Post to the website.
include $(top_srcdir)/docs/Makefile_web.am_fragment

imagesdir   = $(gtkmm_docdir)/images
images_DATA = gtkmm_logo.gif top.gif

post-html: gtkmm_logo.gif top.gif
	rsync $(rsync_args) $^ $$USER@$(web_host):$(web_path_docs)images/

doc-clean:

.PHONY: post-html doc-clean

