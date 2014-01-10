#!/bin/bash

. ./mingw-env.sh

cd $BASE || exit 1

if test ! -d $PACKAGE_DIR; then
	echo "Win32 package directory does not exist"
	exit 1
fi

cp $TOOLS_DIR/ardour.nsi $PACKAGE_DIR || exit 1
cp $BASE/icons/icon/ardour.ico $PACKAGE_DIR || exit 1

cd $PACKAGE_DIR && makensis ardour.nsi
