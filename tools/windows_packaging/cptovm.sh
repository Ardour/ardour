#!/bin/bash

. ./mingw-env.sh

TMP_DIR=tmp
LOOP_DEV=/dev/loop4

cd $BASE || exit 1

if [ ! -d $TMP_DIR ]; then
	echo "Creating temp directory to mount vm image ..."
	mkdir $TMP_DIR || exit 1
fi

echo "mounting vm image as loopback device ..."

sudo mount -o loop=$LOOP_DEV,offset=32256 $VIRT_IMAGE_PATH $TMP_DIR || exit 1

if [ -d $TMP_DIR/$PACKAGE_DIR ]; then
	echo "Removing old copy of $PACKAGE_DIR from vm image ..."
	rm -rf $TMP_DIR/$PACKAGE_DIR || exit 1
fi

echo "Copying $PACKAGE_DIR to vm image ..."
cp -r $PACKAGE_DIR $TMP_DIR || exit 1

if [ "$1" == "--data" ]; then
	DATA_DIR=data

	if [ -d $TMP_DIR/$DATA_DIR ]; then
		echo "Removing old copy of $DATA_DIR from vm image ..."
		rm -rf $TMP_DIR/$DATA_DIR || exit 1
	fi

	echo "Copying $DATA_DIR to vm image ..."
	cp -r $DATA_DIR $TMP_DIR || exit 1
fi


# in case mount is busy
sleep 2

echo "Unmounting vm image ..."

sudo umount -d tmp

echo "Removing temp directory used to mount vm image ..."
rm -rf $TMP_DIR || exit 1

if sudo losetup $LOOP_DEV; then
	echo "sleeping for 10 seconds and trying again ..."
	sleep 10
	if sudo losetup -d $LOOP_DEV; then
		echo "Unmounted loopback device successfully"
		exit 0
	else:
		echo "Unmounting loopback device unsuccessful, you will need to use losetup -d to unmount device"
		exit 1
	fi
fi

exit 0
