#!/bin/sh

LOCAL_AAF_PROJECT="$1"

if ! test -f wscript || ! test -d gtk2_ardour; then
	echo "This script needs to run from ardour's top-level src tree"
	exit 1
fi

if test -z "`which rsync`" -o -z "`which git`"; then
	echo "this script needs rsync and git"
	exit 1
fi

ASRC=`pwd`
set -e
mkdir -p "$ASRC/libs/aaf/aaf"

if [ -z "${LOCAL_AAF_PROJECT}" ]; then
	# Using git repos
	TMP=`mktemp -d`
	test -d "$TMP"
	echo $TMP

	trap "rm -rf $TMP" EXIT

	cd $TMP
	git clone https://github.com/agfline/LibAAF.git aaf

	cd aaf
	git describe --tags
	LIBAAF_VERSION=$(git describe --tags --dirty --match "v*")
	cd $TMP

	AAF=aaf/
	RSYNC_OPT="-auc --info=progress2"
else
	# Using local project
	LIBAAF_VERSION="v0-local"
	cd "${LOCAL_AAF_PROJECT}"
	echo $PWD
	AAF="./"
	RSYNC_OPT="-aI --size-only --info=progress2"
fi

rsync $RSYNC_OPT \
  ${AAF}src/LibCFB/LibCFB.c \
  ${AAF}src/LibCFB/CFBDump.c \
  ${AAF}src/AAFCore/AAFCore.c \
  ${AAF}src/AAFCore/AAFClass.c \
  ${AAF}src/AAFCore/AAFToText.c \
  ${AAF}src/AAFCore/AAFDump.c \
  ${AAF}src/AAFIface/AAFIface.c \
  ${AAF}src/AAFIface/AAFIParser.c \
  ${AAF}src/AAFIface/AAFIEssenceFile.c \
  ${AAF}src/AAFIface/RIFFParser.c \
  ${AAF}src/AAFIface/URIParser.c \
	${AAF}src/AAFIface/ProTools.c \
	${AAF}src/AAFIface/MediaComposer.c \
  ${AAF}src/AAFIface/Resolve.c \
  ${AAF}src/common/utils.c \
  ${AAF}src/common/log.c \
  \
  "$ASRC/libs/aaf"

rsync -auc --info=progress2 \
	${AAF}include/libaaf.h \
	${AAF}include/libaaf/Resolve.h \
	${AAF}include/libaaf/AAFIParser.h \
	${AAF}include/libaaf/AAFCore.h \
	${AAF}include/libaaf/log.h \
	${AAF}include/libaaf/AAFIEssenceFile.h \
	${AAF}include/libaaf/ProTools.h \
	${AAF}include/libaaf/MediaComposer.h \
	${AAF}include/libaaf/AAFToText.h \
	${AAF}include/libaaf/AAFIface.h \
	${AAF}include/libaaf/CFBDump.h \
	${AAF}include/libaaf/AAFDump.h \
	${AAF}include/libaaf/AAFTypes.h \
	${AAF}include/libaaf/LibCFB.h \
	${AAF}include/libaaf/utils.h \
	\
  ${AAF}src/AAFCore/AAFClass.h \
  ${AAF}src/AAFIface/RIFFParser.h \
  ${AAF}src/AAFIface/URIParser.h \
	\
	${AAF}include/libaaf/AAFDefs \
	\
	"$ASRC/libs/aaf/aaf/"

cat > "$ASRC/libs/aaf/aaf/version.h" << EOF
#pragma once
#define LIBAAF_VERSION "${LIBAAF_VERSION}"
EOF

cd "$ASRC/libs/aaf"
for file in $(find . -type f); do
	sed -i 's%#include *<libaaf/\([^>]*\)>%#include "aaf/\1"%; s%#include *<libaaf.h>%#include "aaf/libaaf.h"%;s%#include.*utils.h"%#include "aaf/utils.h"%' $file
	sed -i 's%#include *"\(AAFClass.h\)"%#include "aaf/\1"%' $file
	sed -i 's%#include *"\(\w*Parser.h\)"%#include "aaf/\1"%' $file
done

clang-format -i $ASRC/libs/aaf/*.c $ASRC/libs/aaf/aaf/*.h

#cd "$ASRC"
#patch -p1 < tools/aaf-patches/ardour_libaaf.diff
