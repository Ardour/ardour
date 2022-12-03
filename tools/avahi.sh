#!/bin/sh

set -e

test -n "`which avahi-publish`"
test -n "`which tail`"

PORT=$1
TYPE=$2
test -n "$PORT"
test -n "$TYPE"

if test -n "$3"; then
	PARENT_PID=$3
else
	PARENT_PID=$$
fi

avahi-publish -s Ardour-$PARENT_PID "$TYPE" "$PORT" &
CHILD_PID=$!

trap "kill -- $CHILD_PID" EXIT

tail --pid=$PARENT_PID -f 2>/dev/null
