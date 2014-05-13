#!/bin/bash

if [ -z "$ARCH" ]; then
	echo "ARCH not set defaulting to win32"
	ARCH=win32
elif [ "$ARCH" == "win32" ]; then
	echo "ARCH set to win32"
elif [ "$ARCH" == "win64" ]; then
	echo "ARCH set to win64"
else
	echo "ARCH set invalid value aborting..."
	exit 1
fi

if [ "$ARCH" == "win32" ]; then
	HOST=i686-w64-mingw32
else
	HOST=x86_64-w64-mingw32
fi

MINGW_ROOT=/usr/$HOST/sys-root/mingw

export PKG_CONFIG_PREFIX=$MINGW_ROOT
export PKG_CONFIG_LIBDIR=$MINGW_ROOT/lib/pkgconfig
export PKGCONFIG=pkg-config
export AR=$HOST-ar
export RANLIB=$HOST-ranlib
export CC=$HOST-gcc
export CPP=$HOST-g++
export CXX=$HOST-g++
export AS=$HOST-as
export LINK_CC=$HOST-gcc
export LINK_CXX=$HOST-g++
export WINRC=$HOST-windres
export STRIP=$HOST-strip

BASE=$(readlink -f $0)
BASE=$(dirname $BASE) # up one
BASE=$(dirname $BASE) # up one more
BASE=$(dirname $BASE) # up one more

BUILD_DIR=$BASE/build
BUILD_CACHE_FILE=$BUILD_DIR/c4che/_cache.py
TOOLS_DIR=$BASE/tools/windows_packaging

APPNAME=`grep -m 1 '^APPNAME' $BASE/wscript | awk '{print $3}' | sed "s/'//g"`

# These are only relevant after a build
if test -f $BUILD_CACHE_FILE
then
        . ../define_versions.sh

	# Figure out the Build Type
	if [ x$DEBUG = xT ]; then
		PACKAGE_DIR="$APPNAME-${release_version}-$ARCH-dbg"
	else
		PACKAGE_DIR="$APPNAME-${release_version}-$ARCH"
	fi

	if grep -q "BUILD_TESTS = True" $BUILD_CACHE_FILE; then
		WITH_TESTS=1
	fi

	ARDOUR_DATA_DIR=$PACKAGE_DIR/share/ardour3
fi
