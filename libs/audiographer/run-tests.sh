#!/bin/bash
#
# Run audiographer test suite.
#

TOP=`dirname "$0"`/../..
. $TOP/build/gtk2_ardour/ardev_common_waf.sh
LIB_BUILD_DIR=$TOP/build/libs/audiographer

if [ "$1" == "--single" ] || [ "$2" == "--single" ]; then
        if [ "$1" == "--single" ]; then
	        TESTS="test_*$2*"
        elif [ "$2" == "--single" ]; then
	        TESTS="test_*$3*"
	else
                TESTS='test_*'
        fi
	for test_program in `find $LIB_BUILD_DIR -name "$TESTS" -type f -perm /u+x`;
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
                gdb $LIB_BUILD_DIR/run-tests
        elif [ "$1" == "--valgrind" ]; then
                valgrind $LIB_BUILD_DIR/run-tests
        else
                $LIB_BUILD_DIR/run-tests $*
        fi
fi

