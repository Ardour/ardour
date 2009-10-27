#!/bin/sh

if [ ! -f './tempo.cc' ]; then
    echo "This script must be run from within the libs/ardour directory";
	exit 1;
fi

srcdir=`pwd`
cd ../../build/default

libs='libs'

export LD_LIBRARY_PATH=$libs/vamp-sdk:$libs/surfaces:$libs/surfaces/control_protocol:$libs/ardour:$libs/midi++2:$libs/pbd:$libs/rubberband:$libs/soundtouch:$libs/gtkmm2ext:$libs/sigc++2:$libs/glibmm2:$libs/gtkmm2/atk:$libs/gtkmm2/pango:$libs/gtkmm2/gdk:$libs/gtkmm2/gtk:$libs/libgnomecanvasmm:$libs/libsndfile:$libs/appleutility:$libs/cairomm:$libs/taglib:$libs/evoral:$libs/evoral/src/libsmf:$LD_LIBRARY_PATH

echo $LD_LIBRARY_PATH
./libs/ardour/run-tests
