#!/bin/bash

. ./wine-env.sh

if [ "$1" == "--single" ] || [ "$2" == "--single" ]; then
        if [ "$1" == "--single" ]; then
	        TESTS="test_*$2*"
        elif [ "$2" == "--single" ]; then
	        TESTS="test_*$3*"
	else
                TESTS='test_*'
        fi
	for test_program in `find . -name "$TESTS" -type f -perm /u+x`;
	do
		echo "Running $test_program..."
	        wine "$test_program"
	done
else
        wine run-tests.exe
fi
