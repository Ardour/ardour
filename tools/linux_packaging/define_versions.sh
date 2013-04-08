# 
# this is sourced by build and package, and executed from within build/linux_packaging
#

release_version=`grep -m 1 '[^A-Za-z_]LINUX_VERSION = ' ../../wscript | awk '{print $3}' | sed "s/'//g"`
r=`cut -d'"' -f2 < ../../libs/ardour/revision.cc | sed -e 1d -e "s/$release_version-//"`
if echo $r | grep -q -e - ; then
    revcount=`echo $r | cut -d- -f1`
fi
commit=`echo $r | cut -d- -f2`
version=${release_version}${revcount:+:.$revcount}

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
