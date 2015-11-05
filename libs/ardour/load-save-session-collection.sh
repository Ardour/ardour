#!/bin/bash
#
# This script will run the load-save-session.sh script over each session in a
# directory containing session directories.
#
# This script only supports the default option of the load-save-session.sh
# script, so no valgind or gdb options as it isn't useful on more than a single
# session at a time, use load-save-session.sh directly for that.

DIR_PATH=$1
if [ "$DIR_PATH" == "" ]; then
	echo "Syntax: load-save-session-collection.sh <session collection dir>"
	exit 1
fi

for SESSION_DIR in `find $DIR_PATH -mindepth 1 -maxdepth 1 -type d`; do
	./load-save-session.sh $SESSION_DIR
done
