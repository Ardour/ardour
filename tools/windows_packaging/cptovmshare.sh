#!/bin/bash

. ./mingw-env.sh

cd $BASE || exit 1

if [ -z $ARDOUR_VM_SHARE_DIR ]
then
	echo "You must set ARDOUR_VM_SHARE_DIR in your environment to use this script!"
	exit 1
fi

if [ -d $ARDOUR_VM_SHARE_DIR/$PACKAGE_DIR ]; then
	echo "Removing $PACKAGE_DIR from vm share directory ..."
	rm -rf $ARDOUR_VM_SHARE_DIR/$PACKAGE_DIR || exit 1
fi

echo "Copying $PACKAGE_DIR to vm share directory $ARDOUR_VM_SHARE_DIR ..."
cp -r $PACKAGE_DIR $ARDOUR_VM_SHARE_DIR || exit 1
