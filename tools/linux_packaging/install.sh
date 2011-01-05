#!/bin/sh

# Make sure we have a terminal for the user to see and then run
# the real install script.

if [ -z $WINDOWID ]; then
	exec xterm -e ./stage2.run
else
	./stage2.run
fi
