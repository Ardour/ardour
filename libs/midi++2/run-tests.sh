#!/bin/sh
srcdir=`pwd`

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$srcdir/../../build/libs/midi++:$srcdir/../../build/libs/pbd:$srcdir/../../build/libs/evoral:$srcdir/../../build/libs/timecode
if [ ! -d '../../patchfiles' ]; then
    echo "This script must be run from within the libs/midi++ directory";
    exit 1;
fi

# Make symlink to TakeFive.mid in build directory
cd ../../build/libs/midi++2
if [ "$1" == "debug" ]
then 
	gdb ./run-tests
else
	./run-tests
fi
