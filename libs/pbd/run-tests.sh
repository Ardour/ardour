#!/bin/bash

SCRIPTPATH=$( cd $(dirname $0) ; pwd -P )
TOP="$SCRIPTPATH/../.."
LIBS_DIR="$TOP/build/libs"

export LD_LIBRARY_PATH=$LIBS_DIR/audiographer:$LIBS_DIR/surfaces:$LIBS_DIR/surfaces/control_protocol:$LIBS_DIR/ardour:$LIBS_DIR/midi++2:$LIBS_DIR/pbd:$LIBS_DIR/gtkmm2ext:$LIBS_DIR/appleutility:$LIBS_DIR/evoral:$LIBS_DIR/evoral/src/libsmf:$LD_LIBRARY_PATH

export PBD_TEST_PATH=$TOP/libs/pbd/test

cd $LIBS_DIR/pbd

if [ "$1" == "--debug" ]
then
        gdb ./run-tests
elif [ "$1" == "--valgrind" ]
then
        valgrind --tool="memcheck" ./run-tests
else
        ./run-tests
fi
