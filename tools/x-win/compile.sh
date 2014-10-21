#!/bin/bash

# we assuem this script is <ardour-src>/tools/x-win/compile.sh
pushd "`/usr/bin/dirname \"$0\"`" > /dev/null; this_script_dir="`pwd`"; popd > /dev/null
cd $this_script_dir/../..
test -f gtk2_ardour/wscript || exit 1

: ${XARCH=i686} # or x86_64
: ${ROOT=/home/ardour}
: ${MAKEFLAGS=-j4}

if test "$XARCH" = "x86_64" -o "$XARCH" = "amd64"; then
	echo "Target: 64bit Windows (x86_64)"
	XPREFIX=x86_64-w64-mingw32
	WARCH=w64
	DEBIANPKGS="mingw-w64"
else
	echo "Target: 32 Windows (i686)"
	XPREFIX=i686-w64-mingw32
	WARCH=w32
	DEBIANPKGS="gcc-mingw-w64-i686 g++-mingw-w64-i686 mingw-w64-tools mingw32"
fi

: ${PREFIX=${ROOT}/win-stack-$WARCH}

if test -z "${ARDOURCFG}"; then
	ARDOURCFG="--with-dummy --windows-vst"
	if test -f ${PREFIX}/include/pa_asio.h; then
		ARDOURCFG="$ARDOURCFG --with-wavesbackend"
	fi
fi

if [ "$(id -u)" = "0" ]; then
	apt-get -y install build-essential \
		${DEBIANPKGS} \
		git autoconf automake libtool pkg-config yasm python

	#fixup mingw64 ccache for now
	if test -d /usr/lib/ccache -a -f /usr/bin/ccache; then
		export PATH="/usr/lib/ccache:${PATH}"
		cd /usr/lib/ccache
		test -L ${XPREFIX}-gcc || ln -s ../../bin/ccache ${XPREFIX}-gcc
		test -L ${XPREFIX}-g++ || ln -s ../../bin/ccache ${XPREFIX}-g++
		cd - > /dev/null
	fi
fi

################################################################################
set -e
unset PKG_CONFIG_PATH
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig

export CC=${XPREFIX}-gcc
export CXX=${XPREFIX}-g++
export CPP=${XPREFIX}-cpp
export AR=${XPREFIX}-ar
export LD=${XPREFIX}-ld
export NM=${XPREFIX}-nm
export AS=${XPREFIX}-as
export STRIP=${XPREFIX}-strip
export WINRC=${XPREFIX}-windres
export RANLIB=${XPREFIX}-ranlib
export DLLTOOL=${XPREFIX}-dlltool


CFLAGS="-mstackrealign" \
CXXFLAGS="-mstackrealign" \
LDFLAGS="-L${PREFIX}/lib" ./waf configure \
	--dist-target=mingw \
	--also-include=${PREFIX}/include \
	$ARDOURCFG \
	--prefix=${PREFIX}
./waf
