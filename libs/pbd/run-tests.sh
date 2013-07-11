#!/bin/bash

if [ ! -f './id.cc' ]; then
    echo "This script must be run from within the libs/pbd directory";
    exit 1;
fi

srcdir=`pwd`
cd ../../build
export PBD_TEST_PATH=$srcdir/test

libs='libs'

export LD_LIBRARY_PATH=$libs/audiographer:$libs/vamp-sdk:$libs/surfaces:$libs/surfaces/control_protocol:$libs/ardour:$libs/midi++2:$libs/pbd:$libs/rubberband:$libs/soundtouch:$libs/gtkmm2ext:$libs/sigc++2:$libs/glibmm2:$libs/gtkmm2/atk:$libs/gtkmm2/pango:$libs/gtkmm2/gdk:$libs/gtkmm2/gtk:$libs/libgnomecanvasmm:$libs/libsndfile:$libs/appleutility:$libs/cairomm:$libs/taglib:$libs/evoral:$libs/evoral/src/libsmf:$LD_LIBRARY_PATH

if [ "$1" == "--debug" ]
then
        gdb ./libs/pbd/run-tests
elif [ "$1" == "--valgrind" ]
then
        valgrind --tool="memcheck" ./libs/pbd/run-tests
else
        ./libs/pbd/run-tests
fi
