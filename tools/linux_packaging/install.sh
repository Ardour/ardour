#!/bin/sh

# Make sure we have a terminal for the user to see and then run
# the real install script.

# Some systems don't correctly set the PWD when a script is double-clicked,
# so go ahead and figure out our path and make sure we are in that directory.

SAVED_PWD=$PWD
PKG_PATH=$(dirname "$(readlink -f "$0")")
cd "${PKG_PATH}"

# check for an interactive terminal
# -t fd - Returns true if file descriptor fd is open and refers to a terminal. 
# fd 1 is stdout
if [ ! -t 1 ]; then
	if which xterm > /dev/null; then
		exec xterm -e "${PKG_PATH}/.stage2.run"
	elif which gnome-terminal > /dev/null; then
		exec gnome-terminal -e "${PKG_PATH}/.stage2.run"
	elif which konsole > /dev/null; then
		exec konsole -e "${PKG_PATH}/.stage2.run"
	fi
else
	"${PKG_PATH}/.stage2.run"
fi

cd "${SAVED_PWD}"
