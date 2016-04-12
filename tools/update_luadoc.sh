#!/bin/sh
## ardour needs to be configured with  --luadoc and build should be up-to date.

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

if test -f $HOME/src/ardour-manual/_manual/24_lua-scripting/02_class_reference.html; then
	php $DIR/fmt-luadoc.php -m > $HOME/src/ardour-manual/_manual/24_lua-scripting/02_class_reference.html
	ls -l $HOME/src/ardour-manual/_manual/24_lua-scripting/02_class_reference.html
	cd $HOME/src/ardour-manual/
	./build.rb
else
	php $DIR/fmt-luadoc.php > /tmp/luadoc.html
	ls -l /tmp/luadoc.html
fi
