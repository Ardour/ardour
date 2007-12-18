example_dirs = markup options thread iochannel_stream child_watch regex

# These use gtkmm stuff:
# thread

SUBDIRS = $(example_dirs)
EXTRA_DIST = README Makefile.am_fragment

#Web upload:
include $(top_srcdir)/docs/Makefile_web.am_fragment

web_path = $(web_path_gtkmm)examples

#rsync --cvs-exclude ignores *.o and anything in .cvsignore and some more:
rsync_options = -vzr --rsh ssh --cvs-exclude --delete-excluded --exclude=.libs --exclude=.deps --exclude=core --exclude=.cvsignore --exclude=a.out --exclude=Makefile --exclude=Makefile.in

post-html:
	rsync $(rsync_options) $(example_dirs) $$USER@$(web_host):$(web_path)

.PHONY: post-html

