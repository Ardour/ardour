#!/bin/bash

dir=/tmp
filesize=100 # megabytes
numfiles=128
nocache=
interleave=
needfiles=1
write_blocksize=262144
args=

if uname -a | grep --silent arwin ; then
    ddmega=m
else
    ddmega=M
fi

while [ $# -gt 1 ] ; do
    case $1 in
	-d) dir=$2; shift; shift ;;
	-f) filesize=$2; shift; shift ;;
	-n) numfiles=$2; shift; shift ;;
	-M) args="$args -M"; shift ;;
	-D) args="$args -D"; shift ;;
	-R) args="$args -R"; shift ;;
        *) break ;;
    esac
done

if [ -d $dir -a -f $dir/testfile_1 ] ; then
    # dir exists and has a testfile within it - reuse to avoid
    # recreating files
    echo "# Re-using files in $dir"
    needfiles=
else
    dir=$dir/readtest_$$
    mkdir $dir
    
    if [ $? != 0 ] ; then
	echo "Cannot create testfile directory $dir"
	exit 1
    fi
fi

if [ x$needfiles != x ] ; then
    echo "# Building files for test..."
    if [ x$interleave = x ] ; then
	
	#
	# Create all files sequentially
	#
	
	for i in `seq 1 $numfiles` ; do
	    dd of=$dir/testfile_$i if=/dev/zero bs=1$ddmega count=$filesize >/dev/null 2>&1
	done
    else
	
	#
	# Create files interleaved, adding $write_blocksize to each
	# file in turn.
	#
	
	size=0
	limit=`expr $filesize * 1048576`
	while [ $size -lt $limit ] ; do
	    for i in `seq 1 $numfiles` ; do
		dd if=/dev/zero bs=$write_blocksize count=1 >> $dir/testfile_$i 2>/dev/null
	    done
	    size=`expr $size + $write_blocksize`
	done
    fi
fi

for bs in $@ ; do

    if uname -a | grep --silent arwin ; then
        # clears cache on OS X
        sudo purge
    elif [ -f /proc/sys/vm/drop_caches ] ; then
        # Linux cache clearing
        echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null
    else       
        # need an alternative for other operating systems
        :
    fi
    
    echo "# Blocksize $bs"
    ./readtest $args -b $bs -q $dir/testfile_%d
done
