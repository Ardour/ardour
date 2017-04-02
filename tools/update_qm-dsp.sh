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
git clone git://github.com/c4dm/qm-dsp.git qm-dsp
cd qm-dsp
git describe --tags > "$ASRC/libs/qm-dsp/gitrev.txt"
QMDSP="$TMP/qm-dsp"

cd "$ASRC/libs/qm-dsp"
find base dsp ext maths -type f -exec rsync -c --progress "$QMDSP/{}" "{}" \;


## this applies to qm-vamp-plugins-v1.7.1-20-g4d15479
## (see also Ardour 5.8-250-gc0c24aff7)
patch -p3 << EOF
diff --git a/libs/qm-dsp/dsp/signalconditioning/FiltFilt.cpp b/libs/qm-dsp/dsp/signalconditioning/FiltFilt.cpp
index 714d5755d..c88641de7 100644
--- a/libs/qm-dsp/dsp/signalconditioning/FiltFilt.cpp
+++ b/libs/qm-dsp/dsp/signalconditioning/FiltFilt.cpp
@@ -35,6 +35,13 @@ void FiltFilt::process(double *src, double *dst, unsigned int length)
 
     if (length == 0) return;
 
+    if (length < 2) {
+	for( i = 0; i < length; i++ ) {
+	    dst[i] = src [i];
+	}
+	return;
+    }
+
     unsigned int nFilt = m_ord + 1;
     unsigned int nFact = 3 * ( nFilt - 1);
     unsigned int nExt	= length + 2 * nFact;
@@ -58,11 +65,16 @@ void FiltFilt::process(double *src, double *dst, unsigned int length)
 	filtScratchIn[ index++ ] = sample0 - src[ i ];
     }
     index = 0;
-    for( i = 0; i < nFact; i++ )
+    for( i = 0; i < nFact && i + 2 < length; i++ )
     {
 	filtScratchIn[ (nExt - nFact) + index++ ] = sampleN - src[ (length - 2) - i ];
     }
 
+    for(; i < nFact; i++ )
+    {
+	filtScratchIn[ (nExt - nFact) + index++ ] = 0;
+    }
+
     index = 0;
     for( i = 0; i < length; i++ )
     {
EOF

## this applies to qm-vamp-plugins-v1.7.1-20-g4d15479
## fix OSX 10.5 / PPC builds gcc4.2
patch -p3 << EOF
diff --git a/libs/qm-dsp/base/KaiserWindow.h b/libs/qm-dsp/base/KaiserWindow.h
index f16a4b6c1..0253d6d4c 100644
--- a/libs/qm-dsp/base/KaiserWindow.h
+++ b/libs/qm-dsp/base/KaiserWindow.h
@@ -81,7 +81,7 @@ public:
     }
 
     const double *getWindow() const { 
-	return m_window.data();
+	return &m_window[0];
     }
 
     void cut(double *src) const { 
EOF

git add gitrev.txt base dsp ext maths
