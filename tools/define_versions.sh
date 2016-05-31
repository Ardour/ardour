# 
# this is sourced by build and package, and executed from within build/{osx,linux}_packaging
#

if uname -a | grep arwin >/dev/null 2>&1 ; then
    EXTENDED_RE=-E
else
    EXTENDED_RE=-r
fi

GIT_REV_REGEXP='([0-9][0-9]*)\.([0-9][0-9]*)\-?([pr][rc]e?[0-9]*)?-?([0-9][0-9]*)?(-g([a-f0-9]+))?'

major_version=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\1/"`
minor_version=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\2/"`

# Tracks tags with "MAJOR.MINOR.MICRO"
# Ardour doesn't use this

micro_version=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\3/"`

#
# RC e.g. 8.1-rc3 (optional)
# OR
# PRE e.g. 9.3-pre0 (optional)
#
# Tracks does not use -rcN based tagging
#
rc=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\3/"`

#
# revision count since tag
#
r=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\4/"`

#
# git short sha1 commit ID
#
commit=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed $EXTENDED_RE -e 1d -e "s/$GIT_REV_REGEXP/\6/"`

if [ "x$rc" != "x" ] ; then
    revcount=$rc${r:+.$r}
elif [ "x$r" != "x" ] ; then
    revcount=$r
else
    revcount=0
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
