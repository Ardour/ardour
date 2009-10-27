#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../../build/default/libs/ardour
if [ ! -f './bbt_time.cc' ]; then
    echo "This script must be run from within the libs/ardour directory";
	exit 1;
fi

srcdir=`pwd`

cd ../../build/default/libs/ardour
./run-tests
