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

php fmt-luadoc.php > /tmp/luadoc.html
# ^^ needs manual copy to ardour-manual
ls -l /tmp/luadoc.html
