#!/bin/sh
for file in `ag -l 'mem_fun' libs gtk2_ardour` ; do
	sed -i'' 's/\([^:]\)mem_fun/\1sigc::mem_fun/g' $file
	#sed -i'' 's/mem_fun\s*(\s*this/mem_fun (*this/g' $file
done
