#!/bin/sh
# Run this in a MinGW64 MSYS2 shell.
#
# IMPORTANT: You must run this and the rest of the scripts from a MinGW64
# MSYS2 shell. So if you're still in the MSYS2 shell, close it and open
# the MinGW64 MSYS2 shell before running the rest of these scripts.
#
# It will take at least 30 minutes to compile Ardour, and this is where
# most potential problems could show up.

export ARG1="${ARG1:-$1}"

build()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "./$INCLUDE_SCRIPT"
	cd "$BUILD_DIR/$SRC_REPO_NAME"
	MSYSTEM=''
	/usr/bin/python2.exe ./waf configure --dist-target=mingw --prefix=/mingw64 --configdir=/share --windows-vst
	/usr/bin/python2.exe ./waf
	cd "$BUILD_SCRIPT_DIR"
}

build
