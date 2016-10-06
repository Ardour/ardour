#!/bin/sh
# Run this in an MSYS2 shell.
#
# Source code dependencies that we have to build, makepkg helps us a lot here.
# This may take 10 minutes or more. 
#
# Shout-out to Guy Sherman who provides all of the PKGBUILDs we need in one 
# of their GitHub repositories. Thanks for that!

export ARG1="${ARG1:-$1}"

msys2_deps()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "./$INCLUDE_SCRIPT"
	mkdir -p "$BUILD_DIR"
	cd "$BUILD_DIR" && PREVIOUS_DIR="$PWD"
	git clone $MSYS2_DEPS_REPO || true
	cd $MSYS2_DEPS_REPO_NAME
	git checkout $MSYS2_DEPS_REPO_BRANCH
	git pull
	MAKE="make -j"
	
	for i in ${MSYS2_DEPS[@]}; do
		i=$(sed "s/\r\|\n//g" <<< $i)
		cd "$i" && makepkg-mingw $MAKEPKG_ARGS $MAKEPKG_EXTRA_ARGS && cd ..
	done
	
	$SOURCE "$BUILD_SCRIPT_DIR/02-msys2-deps-quirks.sh"
	cd "$PREVIOUS_DIR"/../..
}

msys2_deps
