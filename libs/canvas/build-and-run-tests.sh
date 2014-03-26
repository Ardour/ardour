#!/bin/bash
#
# Run libcanvas test suite.
#

if [ ! -f './canvas.cc' ]; then
    echo "This script must be run from within the libs/canvas directory";
    exit 1;
fi

waft --targets libcanvas-unit-tests
if [ "$?" != 0 ]; then
  exit
fi

srcdir=`pwd`
cd ../../build/default

libs='libs'

export LD_LIBRARY_PATH=$libs/audiographer:$libs/vamp-sdk:$libs/surfaces:$libs/surfaces/control_protocol:$libs/ardour:$libs/midi++2:$libs/pbd:$libs/rubberband:$libs/soundtouch:$libs/gtkmm2ext:$libs/appleutility:$libs/taglib:$libs/evoral:$libs/evoral/src/libsmf:$libs/timecode:$libs/canvas:$LD_LIBRARY_PATH

if [ "$1" == "--debug" ]; then
        gdb ./libs/canvas/run-tests
elif [ "$1" == "--valgrind" ]; then
        valgrind --tool="memcheck" ./libs/canvas/run-tests
else 
        ./libs/canvas/run-tests
fi
