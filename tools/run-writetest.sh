#!/bin/bash

dir=/tmp
numfiles=128
nocache=
sync=
filesize=`expr 10 \* 1048576`

while [ $# -gt 1 ] ; do
    case $1 in
	-d) dir=$2; shift; shift ;;
	-n) numfiles=$2; shift; shift ;;
	-D) nocache="-D"; shift ;;
        -s) sync="-s"; shift;;
        -S) filesize=$2; shift; shift ;;
        *) break ;;
    esac
done

rm -r $dir/sftest

for bs in $@ ; do
    echo "Blocksize $bs"
    ./sftest $sync $nocache -b $bs -q -d $dir -n $numfiles -S $filesize
    rm -r $dir/sftest
done
