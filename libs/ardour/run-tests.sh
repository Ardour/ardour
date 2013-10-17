#!/bin/bash
#
# Run libardour test suite.
#
. test-env.sh

if [ "$1" == "--single" ] || [ "$2" == "--single" ]; then
        if [ "$1" == "--single" ]; then
	        TESTS="test_*$2*"
        elif [ "$2" == "--single" ]; then
	        TESTS="test_*$3*"
	else
                TESTS='test_*'
        fi
	for test_program in `find libs/ardour -name "$TESTS" -type f -perm /u+x`;
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
                gdb ./libs/ardour/run-tests
        elif [ "$1" == "--valgrind" ]; then
                valgrind ./libs/ardour/run-tests
        else
                ./libs/ardour/run-tests $*
        fi
fi

