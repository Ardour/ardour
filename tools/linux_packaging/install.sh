#!/bin/sh

# Make sure we have a terminal for the user to see and then run
# the real install script.

if [ -z $WINDOWID ]; then
	if which xterm > /dev/null; then
		exec xterm -e ./stage2.run
	elif which gnome-terminal > /dev/null; then
		exec gnome-terminal -e ./stage2.run
	elif which konsole > /dev/null; then
		exec konsole -e ./stage2.run
	fi
else
	./stage2.run
fi
