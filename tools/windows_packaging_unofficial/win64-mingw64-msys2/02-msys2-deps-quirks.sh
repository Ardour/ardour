#!/bin/sh
# Run this in an MSYS2 shell.
#
# Some things are weird!

export ARG1="${ARG1:-$1}"

# Building pango in a less deeply-nested location is required:
# "distutils.errors.DistutilsFileError: could not create 'tmp-introspectsjppac/msys64/home/<username>/<path to ardour>/tools/windows_packaging_unofficial/ardour-mingw64-msys2/MINGW-packages/mingw-w64-pango': The filename or extension is too long"
pango_build_fails_if_deeply_nested()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "$BUILD_SCRIPT_DIR/$INCLUDE_SCRIPT"
	PREVIOUS_DIR="$PWD"
	cd "$BUILD_DIR"
	ln -s "$PREVIOUS_DIR/mingw-w64-pango" || true
	cd mingw-w64-pango && makepkg-mingw $MAKEPKG_ARGS $MAKEPKG_EXTRA_ARGS && cd ..
	cd "$PREVIOUS_DIR"
}

pango_build_fails_if_deeply_nested
