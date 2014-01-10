#!/bin/bash

. ./mingw-env.sh

cd $BASE

LIBS=$BUILD_DIR/libs

export ARDOUR_PATH=$BASE/gtk2_ardour/icons:$BASE/gtk2_ardour/pixmaps:$BASE/build/default/gtk2_ardour:$BASE/gtk2_ardour:.
export ARDOUR_SURFACES_PATH=$LIBS/surfaces/osc:$LIBS/surfaces/generic_midi:$LIBS/surfaces/tranzport:$LIBS/surfaces/powermate:$LIBS/surfaces/mackie
export ARDOUR_PANNER_PATH=$LIBS/panners/2in2out:$LIBS/panners/1in2out:$LIBS/panners/vbap
export ARDOUR_DATA_PATH=$BASE/gtk2_ardour:build/default/gtk2_ardour:.

export VAMP_PATH=$LIBS/vamp-plugins${VAMP_PATH:+:$VAMP_PATH}

export PBD_TEST_PATH=$BASE/libs/pbd/test/

if test ! -d $PACKAGE_DIR; then
	echo "Win32 package directory does not exist"
	exit 1
fi

cd $PACKAGE_DIR 


if [ "$1" == "--run-tests" ]; then
	if test x$WITH_TESTS != x ; then
		echo "<<<<<<<<<<<<<<<<<<  RUNNING LIBPBD TESTS >>>>>>>>>>>>>>>>>>>"
		wine pbd-run-tests.exe
		echo "<<<<<<<<<<<<<<<<<<  RUNNING EVORAL TESTS >>>>>>>>>>>>>>>>>>>"
		wine evoral-run-tests.exe
		echo "<<<<<<<<<<<<<<<<<<  RUNNING ARDOUR TESTS >>>>>>>>>>>>>>>>>>>"
		wine ardour-run-tests.exe
	else
		echo "No tests to run ..."
	fi
else
        wine ardour-3.0.exe
fi
