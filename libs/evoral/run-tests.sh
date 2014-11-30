#!/bin/sh
srcdir=`pwd`

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$srcdir/../../build/libs/evoral:$srcdir/../../build/libs/pbd
if [ ! -f './test/testdata/TakeFive.mid' ]; then
    echo "This script must be run from within the libs/evoral directory";
    exit 1;
fi

SCRIPTPATH=$( cd $(dirname $0) ; pwd -P )
TOP="$SCRIPTPATH/../.."

export EVORAL_TEST_PATH="$SCRIPTPATH/test/testdata"
echo "Setting EVORAL_TEST_PATH=$EVORAL_TEST_PATH"
cd $TOP/build/libs/evoral

lcov -q -d ./src -z
./run-tests
lcov -q -d ./src -d ./test -b ../../.. -c > coverage.lcov
lcov -q -r coverage.lcov *boost* *c++* *usr/include* -o coverage.lcov
mkdir -p ./coverage
genhtml -q -o coverage coverage.lcov
#rm -r coverage/boost
#rm -r coverage/usr
#rm -r coverage/c++
#rm -r coverage/cppunit
#rm -r coverage/glibmm-2.4
#rm -r coverage/sigc++-2.0
echo "Report written to:"
echo "../../build/default/libs/evoral/coverage/index.html"
