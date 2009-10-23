#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../../build/default/libs/evoral
if [ ! -f './test/testdata/TakeFive.mid' ]; then
    echo "This script must be run from within the libs/evoral directory";
	exit 1;
fi

srcdir=`pwd`

# Make symlink to TakeFive.mid in build directory
cd ../../build/default/libs/evoral
mkdir -p ./test/testdata
ln -fs $srcdir/test/testdata/TakeFive.mid \
	./test/testdata/TakeFive.mid

lcov -d ./src -z
./run-tests
lcov -d ./src -d ./test -b ../../.. -c > coverage.lcov
lcov -r coverage.lcov *boost* *c++* *usr/include* -o coverage.lcov
mkdir -p ./coverage
genhtml -o coverage coverage.lcov
#rm -r coverage/boost
#rm -r coverage/usr
#rm -r coverage/c++
#rm -r coverage/cppunit
#rm -r coverage/glibmm-2.4
#rm -r coverage/sigc++-2.0
echo "Report written to:"
echo "../../build/default/libs/evoral/coverage/index.html"
