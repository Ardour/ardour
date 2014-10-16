#!/bin/bash

BASE=$(readlink -f $0)
BASE=$(dirname $BASE) # up one
BASE=$(dirname $BASE) # up one more
BASE=$(dirname $BASE) # up one more

HOST=x86_64-w64-mingw32
MINGW_ROOT=/mingw
GTK=$HOME/gtk/inst
A3=$HOME/A3/inst

export PKG_CONFIG_PREFIX=$MINGW_ROOT
export PKG_CONFIG_LIBDIR=$MINGW_ROOT/lib/pkgconfig
export PKGCONFIG=pkg-config
export AR=ar
export RANLIB=ranlib
export CC=gcc
export CPP=g++
export CXX=g++
export AS=as
export LINK_CC=gcc
export LINK_CXX=g++
export WINRC=windres
export STRIP=strip

BUILD_DIR=$BASE/build
BUILD_CACHE_FILE=$BUILD_DIR/c4che/_cache.py
TOOLS_DIR=$BASE/tools/windows_packaging

. ../define_versions.sh

APPNAME=`grep -m 1 '^APPNAME' $BASE/wscript | awk '{print $3}' | sed "s/'//g"`

# These are only relevant after a build
if test -f $BUILD_CACHE_FILE
then
	# Figure out the Build Type
	if grep -q "DEBUG = True" $BUILD_CACHE_FILE; then
		DEBUG=1
		PACKAGE_DIR="$HOME/$APPNAME-win32-dbg"
	else
		PACKAGE_DIR="$HOME/$APPNAME-win32"
	fi

	if grep -q "BUILD_TESTS = True" $BUILD_CACHE_FILE; then
		WITH_TESTS=1
	fi

	ARDOUR_DATA_DIR=$PACKAGE_DIR/msys/share/ardour3
fi

# put this somewhere better...
VIRT_IMAGE_PATH=$HOME/Data/virt-images/winxp.raw
