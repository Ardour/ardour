#!/bin/sh
# Run this in an MSYS2 shell.
#
# Install dependencies with pacman.

export ARG1="${ARG1:-$1}"

bin_deps()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "./$INCLUDE_SCRIPT"
	$PACMAN $PACMAN_UPDATE_ARGS
	$PACMAN $PACMAN_INSTALL_ARGS $( printf " %s" "${BIN_DEPS[@]}" | sed "s/\r\|\n//g" )
}

bin_deps
