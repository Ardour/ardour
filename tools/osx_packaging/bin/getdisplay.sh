#!/bin/sh
#
# Author: Aaron Voisine <aaron@voisine.org>

if [ "$DISPLAY"x == "x" ]; then
    echo :0 > /tmp/$UID/TemporaryItems/display
else
    echo $DISPLAY > /tmp/$UID/TemporaryItems/display
fi
