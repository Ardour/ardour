#!/bin/bash

function copydll () {
	if [ -f $GTK/bin/$1 ] ; then
		echo "cp $GTK/bin/$1 $2"
		cp $GTK/bin/$1 $2 || return 1
		return 0
	fi
	
	if [ -f $GTK/lib/$1 ] ; then
		echo "cp $GTK/lib/$1 $2"
		cp $GTK/lib/$1 $2 || return 1
		return 0
	fi
	
	if [ -f $A3/bin/$1 ] ; then
		echo "cp $A3/bin/$1 $2"
		cp $A3/bin/$1 $2 || return 1
		return 0
	fi

	if [ -f $A3/lib/$1 ] ; then
		echo "$A3/lib/$1 $2"
		cp $A3/lib/$1 $2 || return 1
		return 0
	fi
	if which $1 ; then	  
	  echo "cp `which $1` $2"
	  cp `which $1` $2 || return 1
	  return 0
	fi
	
	echo "there is no $1"
	return 1
}
