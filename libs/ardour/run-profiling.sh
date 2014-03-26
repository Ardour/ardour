#!/bin/bash
#
# Run libardour profiling tests.
#

if [ "$1" == "" ]; then
   echo "Syntax: run-profiling.sh [flag] <test> [<args>]"
   exit 1;
fi

. test-env.sh

export LD_PRELOAD=/home/carl/src/libfakejack/libjack.so
# session='32tracks'

p=$1
if [ "$p" == "--debug" -o "$p" == "--valgrind" -o "$p" == "--callgrind" ]; then
  f=$p
  p=$2
  shift 1
fi
shift 1

if [ "$f" == "--debug" ]; then
        gdb --args ./libs/ardour/$p $*
elif [ "$f" == "--valgrind" ]; then
        valgrind ./libs/ardour/$p $*
elif [ "$f" == "--callgrind" ]; then
        valgrind --tool=callgrind ./libs/ardour/$p $*
else
        ./libs/ardour/$p $*
fi
