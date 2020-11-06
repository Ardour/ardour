#!/bin/sh
## ardour needs to be configured with  --luadoc and build should be up-to date.

AMANUAL=$HOME/src/ardour-manual

cd `dirname $0`
DIR=`pwd`
set -e
test -f ../libs/ardour/ardour/ardour.h
test -e ../gtk2_ardour/arluadoc
test -e ../build/gtk2_ardour/luadoc

# generate ../doc/ardourapi.json.gz
if test -z "$1"; then
	./doxy2json/ardourdoc.sh
fi

# generate ../doc/luadoc.json.gz
$DIR/../gtk2_ardour/arluadoc

if test -f $AMANUAL/include/class-reference.html; then
	php $DIR/fmt-luadoc.php -m > $AMANUAL/include/class-reference.html
	ls -l $AMANUAL/include/class-reference.html
	cd $AMANUAL/
	./build.py
else
	php $DIR/fmt-luadoc.php > /tmp/luadoc.html
	ls -l /tmp/luadoc.html
fi
