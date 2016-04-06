#!/bin/sh
## ardour needs to be configured with  --luadoc and build should be up-to date.

cd `dirname $0`
set -e
test -f ../libs/ardour/ardour/ardour.h
test -e ../gtk2_ardour/arluadoc
test -e ../build/gtk2_ardour/luadoc

# generate ../doc/luadoc.json.gz
../gtk2_ardour/arluadoc

# generate ../doc/ardourapi.json.gz
./doxy2json/ardourdoc.sh

if test -f $HOME/src/ardour-manual/_manual/24_lua-scripting/02_class_reference.html; then
	php fmt-luadoc.php -m > $HOME/src/ardour-manual/_manual/24_lua-scripting/02_class_reference.html
	ls -l $HOME/src/ardour-manual/_manual/24_lua-scripting/02_class_reference.html
	cd $HOME/src/ardour-manual/
	./build.rb
else
	php fmt-luadoc.php > /tmp/luadoc.html
	ls -l /tmp/luadoc.html
fi
