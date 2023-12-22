#!/bin/sh

REF="${1:-HEAD}"

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

TMP=`mktemp -d`
test -d "$TMP"
echo $TMP

trap "rm -rf $TMP" EXIT

cd $TMP
git clone https://github.com/agfline/LibAAF.git aaf

cd aaf
git describe --tags
git checkout "$REF"
LIBAAF_VERSION=$(git describe --tags --dirty --match "v*")
cd $TMP

AAF=aaf/

rsync -auc --info=progress2 \
  --include='include/' \
  --include='include/**/' \
  --include='src/' \
  --include='src/**/' \
  --include='**/*.c' \
  --include='**/*.h' \
  --exclude='*' \
  ${AAF}/ \
  \
  "$ASRC/libs/aaf"

cat > "$ASRC/libs/aaf/include/libaaf/version.h" << EOF
#pragma once
#define LIBAAF_VERSION "${LIBAAF_VERSION}"
EOF

clang-format -i $(find "$ASRC/libs/aaf" -name \*.c -o -name \*.h)

#cd "$ASRC"
#patch -p1 < tools/aaf-patches/ardour_libaaf.diff
