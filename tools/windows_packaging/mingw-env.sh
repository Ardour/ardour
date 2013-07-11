#!/bin/bash

BASE=$(readlink -f $0)
BASE=$(dirname $BASE) # up one
BASE=$(dirname $BASE) # up one more
BASE=$(dirname $BASE) # up one more

HOST=i686-pc-mingw32
MINGW_ROOT=/usr/$HOST/sys-root/mingw

export PKG_CONFIG_PREFIX=$MINGW_ROOT
export PKG_CONFIG_LIBDIR=$MINGW_ROOT/lib/pkgconfig
export PKGCONFIG=mingw32-pkg-config
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

BUILD_DIR=$BASE/build
BUILD_CACHE_FILE=$BUILD_DIR/c4che/_cache.py
TOOLS_DIR=$BASE/tools/windows_packaging

APPNAME=`grep -m 1 '^APPNAME' $BASE/wscript | awk '{print $3}' | sed "s/'//g"`
VERSION=`grep -m 1 '^VERSION' $BASE/wscript | awk '{print $3}' | sed "s/'//g"`

# These are only relevant after a build
if test -f $BUILD_CACHE_FILE
then
	# Figure out the Build Type
	if grep -q "DEBUG = True" $BUILD_CACHE_FILE; then
		DEBUG=1
		PACKAGE_DIR="$APPNAME-$VERSION-win32-dbg"
	else
		PACKAGE_DIR="$APPNAME-$VERSION-win32"
	fi

	if grep -q "BUILD_TESTS = True" $BUILD_CACHE_FILE; then
		WITH_TESTS=1
	fi

	ARDOUR_DATA_DIR=$PACKAGE_DIR/share/ardour3
fi

# put this somewhere better...
VIRT_IMAGE_PATH=$HOME/virt-images/winxp.raw
