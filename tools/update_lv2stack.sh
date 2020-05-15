#!/bin/bash

set -e

cd /tmp
rm -rf lv2kit
git clone --recursive https://gitlab.com/lv2/lv2kit.git
cd lv2kit

DEFMOD=""
WINSRC=""

OUTDIR=/tmp/lv2-ardour/
PREFIX=/tmp/lv2-inst/

rm -rf $OUTDIR $PREFIX
mkdir -p $OUTDIR

function bundle ()
{
	set -e
	PROJ=$1
	SUF=$2
	cd libs/$PROJ
	git checkout master
	git pull --rebase
	rm -f *.tar.bz2
	PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig ./waf configure --prefix=$PREFIX
	./waf dist
	./waf install
	REV=`git rev-parse --short HEAD`
	REL=`ls ${PROJ}*.tar.bz2`
	VER=`basename $REL .tar.bz2 | sed 's/^[^-]*-//g'`
	OFN=`basename $REL .tar.bz2`-g${REV}${SUF}.tar.bz2

	mv "$REL" "${OUTDIR}${OFN}"
	DEFMOD="$DEFMOD\ndefmod $PROJ $PROJ ${VER}-g${REV}${SUF} http://ardour.org/files/deps/ bz2 ${PROJ}-${VER}"
	WINSRC="$WINSRC\nsrc $PROJ-${VER} tar.bz2 http://ardour.org/files/deps/${OFN} -g${REV}${SUF}"
	cd - &>/dev/null
}

bundle lv2
bundle serd
bundle sord
bundle sratom
bundle lilv
bundle suil

echo "--------------------------------------------"
ls -l $OUTDIR
echo "--------------------------------------------"
echo -e $DEFMOD
echo 
echo "--------------------------------------------"
echo -e $WINSRC
echo 
echo "--------------------------------------------"

echo 
echo "Upload ? {enter | ctrl+c}"
read -n 1

rsync -Pa ${OUTDIR} ardour.org:/persist/community.ardour.org/files/deps/
