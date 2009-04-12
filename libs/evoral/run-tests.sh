#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../../build/default/libs/evoral
if test -f ./test/testdata/TakeFive.mid
then
    ../../build/default/libs/evoral/run-tests
else
    echo "This script must be run from within the libs/evoral directory"
fi
