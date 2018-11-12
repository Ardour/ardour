#!/bin/sh

if ! test -f wscript || ! test -d gtk2_ardour || ! test -d libs/qm-dsp/;then
	echo "This script needs to run from ardour's top-level src tree"
	exit 1
fi

if test -z "`which rsync`" -o -z "`which git`"; then
	echo "this script needs rsync and git"
	exit 1
fi

ASRC=`pwd`
set -e

TMP=`mktemp -d`
test -d "$TMP"
echo $TMP
trap "rm -rf $TMP" EXIT

cd $TMP
git clone git://github.com/c4dm/qm-vamp-plugins.git
VAMPPLUGS="$TMP/qm-vamp-plugins/plugins"

cd "$ASRC/libs/vamp-plugins/"
for src in *.cpp *.h; do
	if test -f "$VAMPPLUGS/$src"; then
		cp "$VAMPPLUGS/$src" ./
		git add $src
	fi
done

## MSVC patch on top of qm-vamp-plugins-v1.7.1-10-g76bc879
patch -p3 << EOF
diff --git b/libs/vamp-plugins/BarBeatTrack.cpp a/libs/vamp-plugins/BarBeatTrack.cpp
index 8d0b887c3..a85c924c4 100644
--- b/libs/vamp-plugins/BarBeatTrack.cpp
+++ a/libs/vamp-plugins/BarBeatTrack.cpp
@@ -25,7 +25,7 @@ using std::vector;
 using std::cerr;
 using std::endl;
 
-#ifndef __GNUC__
+#if !defined(__GNUC__) && !defined(_MSC_VER)
 #include <alloca.h>
 #endif
 
diff --git b/libs/vamp-plugins/OnsetDetect.cpp a/libs/vamp-plugins/OnsetDetect.cpp
index a2c4042c0..c2b6d68db 100644
--- b/libs/vamp-plugins/OnsetDetect.cpp
+++ a/libs/vamp-plugins/OnsetDetect.cpp
@@ -12,6 +12,9 @@
     COPYING included with this distribution for more information.
 */
 
+#ifdef COMPILER_MSVC
+#include <ardourext/float_cast.h>
+#endif
 #include "OnsetDetect.h"
 
 #include <dsp/onsets/DetectionFunction.h>
EOF

git add BarBeatTrack.cpp OnsetDetect.cpp
