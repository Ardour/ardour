#!/bin/sh
## this script should be run from the top-level source dir
## it remove all fuzzy and obsolte translations and wraps
## long lines.
##
## update .po and .pot files:
 ./waf i18n_pot

TEMPFILE=`mktemp`
for file in `git ls-files | grep -e '.po$'`; do
	cp $file $TEMPFILE
	msgattrib -o $file --no-fuzzy --no-obsolete $TEMPFILE
done
rm $TEMPFILE
