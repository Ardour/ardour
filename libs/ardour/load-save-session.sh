#!/bin/bash
#
# Run load-save-session utility program on an existing session, as the session
# may be modified I would not use this utility on a session that you care
# about.
#
# The utility outputs some basic timing information and can be used to look at
# changes in the Session file resulting from the save if for instance the
# session is contained in a git repository.
#

TOP=`dirname "$0"`/../..
. $TOP/build/gtk2_ardour/ardev_common_waf.sh
ARDOUR_LIBS_DIR=$TOP/build/libs/ardour
PROGRAM_NAME=load-save-session

if [ ! -f './tempo.cc' ];
	then echo "This script must be run from within the libs/ardour directory";
	exit 1;
fi

OPTION=""
if [ "$1" == "--debug" -o "$1" == "--valgrind" -o "$1" == "--massif" ]; then
	OPTION=$1
	shift 1
fi

DIR_PATH=$1
if [ "$DIR_PATH" == "" ]; then
	echo "Syntax: load-save-session.sh <session dir>"
	exit 1
fi

NAME=`basename $DIR_PATH`

if [ "$OPTION" == "--debug" ]; then
	gdb --args $ARDOUR_LIBS_DIR/$PROGRAM_NAME $DIR_PATH $NAME
elif [ "$OPTION" == "--valgrind" ]; then
	MEMCHECK_OPTIONS="--leak-check=full"
	valgrind $MEMCHECK_OPTIONS \
	$ARDOUR_LIBS_DIR/$PROGRAM_NAME $DIR_PATH $NAME
elif [ "$OPTION" == "--massif" ]; then
	MASSIF_OPTIONS="--time-unit=ms --massif-out-file=massif.out.$NAME"
	valgrind --tool=massif $MASSIF_OPTIONS \
	$ARDOUR_LIBS_DIR/$PROGRAM_NAME $DIR_PATH $NAME
else
	$ARDOUR_LIBS_DIR/$PROGRAM_NAME $DIR_PATH $NAME
fi
