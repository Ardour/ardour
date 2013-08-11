#!/bin/bash

. ./mingw-env.sh

. ./wine-env.sh

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
