#!/bin/bash

. ./mingw-env.sh

cd $BASE

if test ! -d $PACKAGE_DIR; then
	echo "Win32 package directory does not exist"
	exit 1
fi

LIBS=$BUILD_DIR/libs

export ARDOUR_PATH=$BASE/gtk2_ardour/icons:$BASE/gtk2_ardour/pixmaps:$BASE/build/default/gtk2_ardour:$BASE/gtk2_ardour:.
export ARDOUR_SURFACES_PATH=$LIBS/surfaces/osc:$LIBS/surfaces/generic_midi:$LIBS/surfaces/tranzport:$LIBS/surfaces/powermate:$LIBS/surfaces/mackie
export ARDOUR_PANNER_PATH=$LIBS/panners/2in2out:$LIBS/panners/1in2out:$LIBS/panners/vbap
export ARDOUR_DATA_PATH=$BASE/gtk2_ardour:build/default/gtk2_ardour:.

export VAMP_PATH=$LIBS/vamp-plugins${VAMP_PATH:+:$VAMP_PATH}

export PBD_TEST_PATH=$BASE/libs/pbd/test/

cd $PACKAGE_DIR
