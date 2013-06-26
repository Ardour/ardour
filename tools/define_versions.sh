# 
# this is sourced by build and package, and executed from within build/{osx,linux}_packaging
#

major_version=`grep -m 1 '^MAJOR = ' ../../wscript | awk '{print $3}' | sed "s/'//g"`
minor_version=`grep -m 1 '^MINOR = ' ../../wscript | awk '{print $3}' | sed "s/'//g"`
release_version=${major_version}.${minor_version}
r=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed -e 1d -e "s/[0-9][0-9]*\.[0-9][0-9]*-//"`
if echo $r | grep -q -e - ; then
    revcount=`echo $r | cut -d- -f1`
fi
commit=`echo $r | cut -d- -f2`
version=${release_version}${revcount:+.$revcount}

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
