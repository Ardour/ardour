#!/bin/bash
#
# Run libardour test suite.
#

if [ ! -f './tempo.cc' ]; then
    echo "This script must be run from within the libs/ardour directory";
    exit 1;
fi

cd ../..
top=`pwd`
cd build

libs='libs'

export LD_LIBRARY_PATH=$libs/audiographer:$libs/vamp-sdk:$libs/surfaces:$libs/surfaces/control_protocol:$libs/ardour:$libs/midi++2:$libs/pbd:$libs/rubberband:$libs/soundtouch:$libs/gtkmm2ext:$libs/appleutility:$libs/taglib:$libs/evoral:$libs/evoral/src/libsmf:$libs/timecode:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH

export ARDOUR_CONFIG_PATH=$top:$top/gtk2_ardour:$libs/..:$libs/../gtk2_ardour
export ARDOUR_PANNER_PATH=$libs/panners/2in2out:$libs/panners/1in2out:$libs/panners/vbap
export ARDOUR_SURFACES_PATH=$libs/surfaces/osc:$libs/surfaces/generic_midi:$libs/surfaces/tranzport:$libs/surfaces/powermate:$libs/surfaces/mackie
export ARDOUR_MCP_PATH="../mcp"
export ARDOUR_DLL_PATH=$libs
export ARDOUR_DATA_PATH=$top/gtk2_ardour:$top/build/gtk2_ardour:.

if [ "$1" == "--debug" ]; then
        gdb ./libs/ardour/run-tests
elif [ "$1" == "--valgrind" ]; then
        valgrind ./libs/ardour/run-tests
else
        ./libs/ardour/run-tests $*
fi
