#!/bin/sh
# Run this in an MSYS2 shell.
#
# Get the Ardour source code, and check out the version
# which this set of build scripts is currently targetting.
#
# We try to target the latest Ardour release if it's reasonable
# to do so.

export ARG1="${ARG1:-$1}"

get()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "./$INCLUDE_SCRIPT"
	cd "$BUILD_DIR"
	git clone "$SRC_REPO" || true
	cd "$SRC_REPO_NAME"
	git checkout "$SRC_VER"
	git pull origin "$SRC_VER"
	cd ../..
}

get
