#!/bin/sh

SCRIPTPATH=$( cd $(dirname $0) ; pwd -P )
TOP="$SCRIPTPATH/../.."
LIBS_DIR="$TOP/build/libs"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LIBS_DIR/midi++:$LIBS_DIR/pbd:$LIBS_DIR/evoral:$LIBS_DIR/timecode

export MIDIPP_TEST_PATH=$TOP/patchfiles

cd $LIBS_DIR/midi++2
if [ "$1" = "debug" ]; then
	gdb ./run-tests
else
	./run-tests
fi
