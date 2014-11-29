#!/bin/bash
#
# Run libardour test suite.
#

TOP=`dirname "$0"`/../..
. $TOP/build/gtk2_ardour/ardev_common_waf.sh
ARDOUR_LIBS_DIR=$TOP/build/libs/ardour

if [ "$1" == "--single" ] || [ "$2" == "--single" ]; then
        if [ "$1" == "--single" ]; then
	        TESTS="test_*$2*"
        elif [ "$2" == "--single" ]; then
	        TESTS="test_*$3*"
	else
                TESTS='test_*'
        fi
	for test_program in `find $ARDOUR_LIBS_DIR -name "$TESTS" -type f -perm /u+x`;
	do
		echo "Running $test_program..."
                if [ "$1" == "--debug" ]; then
	                gdb ./"$test_program"
                elif [ "$1" == "--valgrind" ]; then
	                valgrind ./"$test_program"
	        else
	                ./"$test_program"
	        fi
	done
else
        if [ "$1" == "--debug" ]; then
                gdb $ARDOUR_LIBS_DIR/run-tests
        elif [ "$1" == "--valgrind" ]; then
                valgrind $ARDOUR_LIBS_DIR/run-tests
        else
                $ARDOUR_LIBS_DIR/run-tests $*
        fi
fi

