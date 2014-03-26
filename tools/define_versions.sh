# 
# this is sourced by build and package, and executed from within build/{osx,linux}_packaging
#

if uname -a | grep arwin >/dev/null 2>&1 ; then
    EXTENDED_RE=-E
else
    EXTENDED_RE=-r
fi

GIT_REV_REGEXP='([0-9][0-9]*)\.([0-9][0-9]*)-?([0-9][0-9]*)?-?([a-z0-9]*)'

major_version=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE  -e 1d -e "s/$GIT_REV_REGEXP/\1/"`
minor_version=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\2/"`
r=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\3/"`
commit=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\4/"`

if [ "x$r" != "x" ] ; then
    revcount=$r
fi

release_version=${major_version}.${minor_version}${revcount:+.$revcount}

#
# Figure out the Build Type
#
# Note that the name of the cache file may vary from to time
#

if grep -q "DEBUG = True" ../../build/c4che/_cache.py; then
	DEBUG="T"
else
	DEBUG="F"
fi
