#!/bin/sh

dir=/tmp
filesize=100 # megabytes
numfiles=128
nocache=
interleave=
blocksize=262144
needfiles=1


while [ $# -gt 1 ] ; do
    case $1 in
	-d) dir=$2; shift; shift ;;
	-f) filesize=$2; shift; shift ;;
	-n) numfiles=$2; shift; shift ;;
	-N) nocache="-s"; shift; shift ;;
	-b) blocksize=$2; shift; shift ;;
    esac
done

if [ -d $dir -a -f $dir/testfile_1 ] ; then
    # dir exists and has a testfile within it - reuse to avoid
    # recreating files
    echo "Re-using files in $dir"
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
    if [ x$interleave = x ] ; then
	
	#
	# Create all files sequentially
	#
	
	for i in `seq 1 $numfiles` ; do
	    dd of=$dir/testfile_$i if=/dev/zero bs=1M count=$filesize
	    echo $i
	done
    else
	
	#
	# Create files interleaved, adding $blocksize to each
	# file in turn.
	#
	
	size=0
	limit=`expr $filesize * 1048576`
	while [ $size -lt $limit ] ; do
	    for i in `seq 1 $numfiles` ; do
		dd if=/dev/zero bs=$blocksize count=1 >> $dir/testfile_$i
	    done
	    size=`expr $size + $blocksize`
	    echo "Files now @ $size bytes"
	done
    fi
fi

if uname -a | grep -s arwin ; then
    # clears cache on OS X
    sudo purge
elif [ -f /proc/sys/vm/drop_cache ] ; then
     # Linux cache clearing
    echo 3 | sudo tee /proc/sys/vm/drop/cache >/dev/null
else       
    # need an alternative for other operating systems
    :
fi

echo "Ready to run ..."

./readtest $nocache -b $blocksize -q $dir/testfile_%d
