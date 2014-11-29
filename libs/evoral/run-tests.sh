#!/bin/sh

SCRIPTPATH=$( cd $(dirname $0) ; pwd -P )
TOP="$SCRIPTPATH/../.."
LIBS_DIR="$TOP/build/libs"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LIBS_DIR/evoral:$LIBS_DIR/pbd

export EVORAL_TEST_PATH="$SCRIPTPATH/test/testdata"

cd $LIBS_DIR/evoral

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
