#!/bin/bash
#
# Run simple session load tester over a corpus of sessions.
#

if [ ! -f './tempo.cc' ]; then
    echo "This script must be run from within the libs/ardour directory";
    exit 1;
fi

. test-env.sh

f=""
if [ "$1" == "--debug" -o "$1" == "--valgrind" ]; then
  f=$1
  shift 1
fi

d=$1
if [ "$d" == "" ]; then
  echo "Syntax: run-session-tests.sh <corpus>"
  exit 1
fi

for s in `find $d -mindepth 1 -maxdepth 1 -type d`; do
  n=`basename $s`
  if [ "$f" == "--debug" ]; then
    gdb --args ./libs/ardour/load-session $s $n
  elif [ "$f" == "--valgrind" ]; then
    valgrind ./libs/ardour/load-session $s $n
  else
    ./libs/ardour/load-session $s $n
  fi
done

