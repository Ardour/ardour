#!/bin/bash

function copydll () {
	if [ -f $MINGW_ROOT/bin/$1 ] ; then
		cp $MINGW_ROOT/bin/$1 $2 || return 1
		return 0
	fi

	echo "ERROR: File $1 does not exist"
	return 1
}
