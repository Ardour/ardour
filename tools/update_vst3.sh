#!/bin/bash

if ! test -f wscript || ! test -d gtk2_ardour || ! test -d libs/vst3/; then
	echo "This script needs to run from ardour's top-level src tree"
	exit 1
fi

if test -z "`which rsync`" -o -z "`which git`"; then
	echo "this script needs rsync and git"
	exit 1
fi

ASRC=`pwd`
set -e

rm -rf libs/vst3/pluginterfaces/gui
rm -rf libs/vst3/pluginterfaces/vst
rm -rf libs/vst3/pluginterfaces/base
mkdir -p libs/vst3/pluginterfaces/gui
mkdir -p libs/vst3/pluginterfaces/vst
mkdir -p libs/vst3/pluginterfaces/base

TMP=`mktemp -d`
test -d $TMP

trap "rm -rf $TMP" EXIT

cd $TMP
git clone https://github.com/steinbergmedia/vst3_pluginterfaces.git
cd vst3_pluginterfaces/
git reset --hard a21cdf3779726b2fe8eea0a8c36ebd698f4f0e82

rsync -auc --info=name0 \
	base/conststringtable.cpp \
	base/conststringtable.h \
	base/falignpop.h \
	base/falignpush.h \
	base/fplatform.h \
	base/fstrdefs.h \
	base/ftypes.h \
	base/funknown.cpp \
	base/funknown.h \
	base/ibstream.h \
	base/ipluginbase.h \
	base/istringresult.h \
	base/smartpointer.h \
	base/typesizecheck.h \
	"$ASRC/libs/vst3/pluginterfaces/base/"

rsync -auc --info=name0 \
	vst gui \
	"$ASRC/libs/vst3/pluginterfaces/"

cd "$ASRC"

patch -p1 < tools/vst3-patches/vst3-cxx98.diff
patch -p1 < tools/vst3-patches/vst3-uuid.diff

#git add libs/vst3/pluginterfaces/
