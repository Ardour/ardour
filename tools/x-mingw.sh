#!/bin/bash
# this script creates a windows32 version of ardour3
# cross-compiled on GNU/Linux
#
# It is intended to run in a pristine chroot or VM of a minimal
# debian system. see http://wiki.debian.org/cowbuilder
# but it can also be run as root on any system...
#
###############################################################################
### Quick start
### one-time cowbuilder/pbuilder setup on the build-host
#
# sudo apt-get install cowbuilder util-linux
# sudo mkdir -p /var/cache/pbuilder/jessie-amd64/aptcache
#
# sudo cowbuilder --create \
#     --basepath /var/cache/pbuilder/jessie-amd64/base.cow \
#     --distribution jessie \
#     --debootstrapopts --arch --debootstrapopts amd64
#
### 'interactive build'
#
# sudo cowbuilder --login --bindmounts /var/tmp \
#     --basepath /var/cache/pbuilder/jessie-amd64/base.cow
#
### now, inside cowbuilder (/var/tmp/ is shared with host, -> bindmounts)
#
# /var/tmp/this_script.sh
#
### go for a coffee and ~40min later find /var/tmp/ardour-{VERSION}-Setup.exe
###
### instead of cowbuilder --login, cowbuilder --execute /var/tmp/x-mingw.sh
### does it all by itself, a ~/.pbuilderrc or /etc//etc/pbuilderrc
### can be used to set bindmounts and basepath... last but not least
### ccache helps a lot to speed up recompiles. see also
### https://wiki.ubuntu.com/PbuilderHowto#Integration_with_ccache
###
###############################################################################

### influential environment variables

: ${XARCH=i686} # or x86_64
: ${ASIO=yes}   # [yes|no] build with ASIO/waves backend

: ${MAKEFLAGS=-j4}
: ${STACKCFLAGS="-O2 -g"}
: ${ARDOURCFG=--with-dummy --windows-vst}

: ${NOSTACK=}   # set to skip building the build-stack

: ${SRCDIR=/var/tmp/winsrc}  # source-code tgz cache
: ${TMPDIR=/var/tmp}         # package is built (and zipped) here.

: ${ROOT=/home/ardour} # everything happens below here :)
                       # src, build and stack-install

###############################################################################

if [ "$(id -u)" != "0" -a -z "$SUDO" ]; then
	echo "This script must be run as root in pbuilder" 1>&2
	echo "e.g sudo DIST=jessie cowbuilder --bindmounts /var/tmp --execute $0"
	exit 1
fi

###############################################################################
set -e

if test "$XARCH" = "x86_64" -o "$XARCH" = "amd64"; then
	echo "Target: 64bit Windows (x86_64)"
	XPREFIX=x86_64-w64-mingw32
	HPREFIX=x86_64
	WARCH=w64
	DEBIANPKGS="mingw-w64"
else
	echo "Target: 32 Windows (i686)"
	XPREFIX=i686-w64-mingw32
	HPREFIX=i386
	WARCH=w32
	DEBIANPKGS="gcc-mingw-w64-i686 g++-mingw-w64-i686 mingw-w64-tools mingw32"
fi

: ${PREFIX=${ROOT}/win-stack-$WARCH}
: ${BUILDD=${ROOT}/win-build-$WARCH}

apt-get -y install build-essential \
	${DEBIANPKGS} \
	git autoconf automake libtool pkg-config \
	curl unzip ed yasm cmake ca-certificates

#fixup mingw64 ccache for now
if test -d /usr/lib/ccache -a -f /usr/bin/ccache; then
	export PATH="/usr/lib/ccache:${PATH}"
	cd /usr/lib/ccache
	test -L ${XPREFIX}-gcc || ln -s ../../bin/ccache ${XPREFIX}-gcc
	test -L ${XPREFIX}-g++ || ln -s ../../bin/ccache ${XPREFIX}-g++
fi

###############################################################################

mkdir -p ${SRCDIR}
mkdir -p ${PREFIX}
mkdir -p ${BUILDD}

unset PKG_CONFIG_PATH
export XPREFIX
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export PREFIX
export SRCDIR

if test -n "$(which ${XPREFIX}-pkg-config)"; then
	export PKG_CONFIG=`which ${XPREFIX}-pkg-config`
fi

function download {
echo "--- Downloading.. $2"
test -f ${SRCDIR}/$1 || curl -k -L -o ${SRCDIR}/$1 $2
}

function src {
download ${1}.${2} $3
cd ${BUILDD}
rm -rf $1
tar xf ${SRCDIR}/${1}.${2}
cd $1
}

function autoconfconf {
set -e
echo "======= $(pwd) ======="
#CPPFLAGS="-I${PREFIX}/include -DDEBUG$CPPFLAGS" \
	CPPFLAGS="-I${PREFIX}/include$CPPFLAGS" \
	CFLAGS="-I${PREFIX}/include ${STACKCFLAGS} -mstackrealign$CFLAGS" \
	CXXFLAGS="-I${PREFIX}/include ${STACKCFLAGS} -mstackrealign$CXXFLAGS" \
	LDFLAGS="-L${PREFIX}/lib$LDFLAGS" \
	./configure --host=${XPREFIX} --build=${HPREFIX}-linux \
	--prefix=$PREFIX $@
}

function autoconfbuild {
set -e
autoconfconf $@
make $MAKEFLAGS && make install
}

function wafbuild {
set -e
echo "======= $(pwd) ======="
	CC=${XPREFIX}-gcc \
	CXX=${XPREFIX}-g++ \
	CPP=${XPREFIX}-cpp \
	AR=${XPREFIX}-ar \
	LD=${XPREFIX}-ld \
	NM=${XPREFIX}-nm \
	AS=${XPREFIX}-as \
	STRIP=${XPREFIX}-strip \
	RANLIB=${XPREFIX}-ranlib \
	DLLTOOL=${XPREFIX}-dlltool \
	CPPFLAGS="-I${PREFIX}/include$CPPFLAGS" \
	CFLAGS="-I${PREFIX}/include ${STACKCFLAGS} -mstackrealign$CFLAGS" \
	CXXFLAGS="-I${PREFIX}/include ${STACKCFLAGS} -mstackrealign$CXXFLAGS" \
	LDFLAGS="-L${PREFIX}/lib$LDFLAGS" \
	./waf configure --prefix=$PREFIX $@ \
	&& ./waf && ./waf install
}

################################################################################
if test -z "$NOSTACK"; then
################################################################################

### jack headers, .def, .lib, .dll and pkg-config file from jackd 1.9.10
### this is a re-zip of file extracted from official jack releases:
### https://dl.dropboxusercontent.com/u/28869550/Jack_v1.9.10_32_setup.exe
### https://dl.dropboxusercontent.com/u/28869550/Jack_v1.9.10_64_setup.exe

download jack_win3264.tar.xz http://robin.linuxaudio.org/jack_win3264.tar.xz
cd "$PREFIX"
tar xf ${SRCDIR}/jack_win3264.tar.xz
"$PREFIX"/update_pc_prefix.sh ${WARCH}


download pthreads-w32-2-9-1-release.tar.gz ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.tar.gz
cd ${BUILDD}
rm -rf pthreads-w32-2-9-1-release
tar xzf ${SRCDIR}/pthreads-w32-2-9-1-release.tar.gz
cd pthreads-w32-2-9-1-release
make clean GC CROSS=${XPREFIX}-
mkdir -p ${PREFIX}/bin
mkdir -p ${PREFIX}/lib
mkdir -p ${PREFIX}/include
cp -vf pthreadGC2.dll ${PREFIX}/bin/
cp -vf libpthreadGC2.a ${PREFIX}/lib/libpthread.a
cp -vf pthread.h sched.h ${PREFIX}/include

src zlib-1.2.7 tar.gz ftp://ftp.simplesystems.org/pub/libpng/png/src/history/zlib/zlib-1.2.7.tar.gz
make -fwin32/Makefile.gcc PREFIX=${XPREFIX}-
make install -fwin32/Makefile.gcc SHARED_MODE=1 \
	INCLUDE_PATH=${PREFIX}/include \
	LIBRARY_PATH=${PREFIX}/lib \
	BINARY_PATH=${PREFIX}/bin

src tiff-4.0.3 tar.gz ftp://ftp.remotesensing.org/pub/libtiff/tiff-4.0.3.tar.gz
autoconfbuild

download jpegsrc.v9a.tar.gz http://www.ijg.org/files/jpegsrc.v9a.tar.gz
cd ${BUILDD}
rm -rf jpeg-9a
tar xzf ${SRCDIR}/jpegsrc.v9a.tar.gz
cd jpeg-9a
autoconfbuild

src libogg-1.3.2 tar.gz http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.gz
autoconfbuild

src libvorbis-1.3.4 tar.gz http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.4.tar.gz
autoconfbuild --disable-examples --with-ogg=${PREFIX}

src flac-1.3.0 tar.xz http://downloads.xiph.org/releases/flac/flac-1.3.0.tar.xz
ed Makefile.in << EOF
%s/examples / /
wq
EOF
autoconfbuild

src libsndfile-1.0.25 tar.gz http://www.mega-nerd.com/libsndfile/files/libsndfile-1.0.25.tar.gz
ed Makefile.in << EOF
%s/ examples regtest tests programs//
wq
EOF
LDFLAGS=" -lFLAC -lwsock32 -lvorbis -logg -lwsock32" \
autoconfbuild
ed $PREFIX/lib/pkgconfig/sndfile.pc << EOF
%s/ -lsndfile/ -lsndfile -lvorbis -lvorbisenc -lFLAC -logg -lwsock32/
wq
EOF

src libsamplerate-0.1.8 tar.gz http://www.mega-nerd.com/SRC/libsamplerate-0.1.8.tar.gz
ed Makefile.in << EOF
%s/ examples tests//
wq
EOF
autoconfbuild

src expat-2.1.0 tar.gz http://prdownloads.sourceforge.net/expat/expat-2.1.0.tar.gz
autoconfbuild

src libiconv-1.14 tar.gz ftp://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz
autoconfbuild --with-included-gettext --with-libiconv-prefix=$PREFIX

src libxml2-2.9.1 tar.gz ftp://xmlsoft.org/libxslt/libxml2-2.9.1.tar.gz
CFLAGS=" -O0" CXXFLAGS=" -O0" \
autoconfbuild --with-threads=no --with-zlib=$PREFIX --without-python

src libpng-1.6.13 tar.xz https://downloads.sourceforge.net/project/libpng/libpng16/1.6.13/libpng-1.6.13.tar.xz
autoconfbuild

src freetype-2.5.3 tar.gz http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.gz
autoconfbuild -with-harfbuzz=no

src fontconfig-2.11.0 tar.bz2 http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.11.0.tar.bz2
ed Makefile.in << EOF
%s/conf.d test /conf.d /
wq
EOF
autoconfbuild --enable-libxml2

src pixman-0.32.2 tar.gz http://cgit.freedesktop.org/pixman/snapshot/pixman-0.32.2.tar.gz
./autogen.sh
autoconfbuild

src cairo-1.12.16 tar.xz http://cairographics.org/releases/cairo-1.12.16.tar.xz
autoconfbuild

src libffi-3.1 tar.gz ftp://sourceware.org/pub/libffi/libffi-3.1.tar.gz
autoconfbuild

src gettext-0.19.2 tar.gz http://ftp.gnu.org/pub/gnu/gettext/gettext-0.19.2.tar.gz
autoconfbuild

################################################################################
apt-get -y install python gettext libglib2.0-dev # /usr/bin/msgfmt , genmarshall
#NB. we could apt-get install wine instead and run the exe files in $PREFIX/bin
################################################################################

src glib-2.42.0 tar.xz  http://ftp.gnome.org/pub/gnome/sources/glib/2.42/glib-2.42.0.tar.xz
LIBS="-lpthread" \
autoconfbuild --with-pcre=internal --disable-silent-rules --with-libiconv=no

################################################################################
dpkg -P gettext python || true
################################################################################

src harfbuzz-0.9.35 tar.bz2 http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.35.tar.bz2
autoconfbuild

src pango-1.36.8 tar.xz http://ftp.gnome.org/pub/GNOME/sources/pango/1.36/pango-1.36.8.tar.xz
autoconfbuild --without-x --with-included-modules=yes

src atk-2.14.0 tar.bz2 http://ftp.gnome.org/pub/GNOME/sources/atk/2.14/atk-2.14.0.tar.xz
autoconfbuild --disable-rebuilds

src gdk-pixbuf-2.31.1 tar.xz http://ftp.acc.umu.se/pub/GNOME/sources/gdk-pixbuf/2.31/gdk-pixbuf-2.31.1.tar.xz
autoconfbuild --disable-modules --without-gdiplus --with-included-loaders=yes

src gtk+-2.24.24 tar.xz http://ftp.gnome.org/pub/gnome/sources/gtk+/2.24/gtk+-2.24.24.tar.xz
ed Makefile.in << EOF
%s/demos / /
wq
EOF
autoconfconf --disable-rebuilds # --disable-modules
if test "$WARCH" = "w64"; then
make -n || true
rm gtk/gtk.def # workaround disable-rebuilds
fi
make && make install

################################################################################
dpkg -P libglib2.0-dev libpcre3-dev || true
################################################################################

src lv2-1.10.0 tar.bz2 http://lv2plug.in/spec/lv2-1.10.0.tar.bz2
wafbuild --no-plugins

src serd-0.20.0 tar.bz2 http://download.drobilla.net/serd-0.20.0.tar.bz2
wafbuild

src sord-0.12.2 tar.bz2 http://download.drobilla.net/sord-0.12.2.tar.bz2
wafbuild --no-utils

src sratom-0.4.6 tar.bz2 http://download.drobilla.net/sratom-0.4.6.tar.bz2
wafbuild

# http://dev.drobilla.net/ticket/986
src lilv-0.20.0 tar.bz2 http://download.drobilla.net/lilv-0.20.0.tar.bz2
ed wscript << EOF
/sys.platform.*win32
.s/win32/linux2/
/sys.platform.*win32
.s/win32/linux2/
%s/win32/linux/
wq
EOF
wafbuild
ed $PREFIX/lib/pkgconfig/lilv-0.pc << EOF
%s/-ldl//
wq
EOF

src suil-0.8.2 tar.bz2 http://download.drobilla.net/suil-0.8.2.tar.bz2
wafbuild

src curl-7.35.0 tar.bz2 http://curl.haxx.se/download/curl-7.35.0.tar.bz2
autoconfbuild

src libsigc++-2.4.0 tar.xz http://ftp.gnome.org/pub/GNOME/sources/libsigc++/2.4/libsigc++-2.4.0.tar.xz
autoconfbuild

src glibmm-2.42.0 tar.xz http://ftp.gnome.org/pub/GNOME/sources/glibmm/2.42/glibmm-2.42.0.tar.xz
autoconfbuild

src cairomm-1.11.2 tar.gz http://cairographics.org/releases/cairomm-1.11.2.tar.gz
autoconfbuild

src pangomm-2.34.0 tar.xz http://ftp.acc.umu.se/pub/gnome/sources/pangomm/2.34/pangomm-2.34.0.tar.xz
autoconfbuild

src atkmm-2.22.7 tar.xz http://ftp.gnome.org/pub/GNOME/sources/atkmm/2.22/atkmm-2.22.7.tar.xz
autoconfbuild

src gtkmm-2.24.4 tar.xz http://ftp.acc.umu.se/pub/GNOME/sources/gtkmm/2.24/gtkmm-2.24.4.tar.xz
autoconfbuild

src fftw-3.3.4 tar.gz http://www.fftw.org/fftw-3.3.4.tar.gz
autoconfbuild --enable-single --enable-float --enable-type-prefix --enable-sse --with-our-malloc --enable-avx --disable-mpi
make clean
autoconfbuild --enable-type-prefix --with-our-malloc --enable-avx --disable-mpi

################################################################################
src taglib-1.9.1 tar.gz http://taglib.github.io/releases/taglib-1.9.1.tar.gz
ed CMakeLists.txt << EOF
0i
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER ${XPREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${XPREFIX}-c++)
set(CMAKE_RC_COMPILER ${XPREFIX}-windres)
.
wq
EOF
rm -rf build/
mkdir build && cd build
	cmake \
		-DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_RELEASE_TYPE=Release \
		-DCMAKE_SYSTEM_NAME=Windows -DZLIB_ROOT=$PREFIX \
		..
make $MAKEFLAGS && make install

# windows target does not create .pc file...
cat > $PREFIX/lib/pkgconfig/taglib.pc << EOF
prefix=$PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: TagLib
Description: Audio meta-data library
Requires:
Version: 1.9.1
Libs: -L\${libdir}/lib -ltag
Cflags: -I\${includedir}/include/taglib
EOF

################################################################################
#git://liblo.git.sourceforge.net/gitroot/liblo/liblo
src liblo-0.28 tar.gz http://downloads.sourceforge.net/liblo/liblo-0.28.tar.gz
autoconfconf --enable-shared
ed src/Makefile << EOF
/noinst_PROGRAMS
.,+3d
wq
EOF
ed Makefile << EOF
%s/examples//
wq
EOF
make $MAKEFLAGS && make install

################################################################################
src boost_1_56_0 tar.bz2 http://sourceforge.net/projects/boost/files/boost/1.56.0/boost_1_56_0.tar.bz2
./bootstrap.sh --prefix=$PREFIX
echo "using gcc : 4.7 : ${XPREFIX}-g++ :
<rc>${XPREFIX}-windres
<archiver>${XPREFIX}-ar
;" > user-config.jam
#	PTW32_INCLUDE=${PREFIX}/include \
#	PTW32_LIB=${PREFIX}/lib  \
	./b2 --prefix=$PREFIX \
	toolset=gcc \
	target-os=windows \
	variant=release \
	threading=multi \
	threadapi=win32 \
	link=shared \
	runtime-link=shared \
	--with-exception \
	--with-regex \
	--layout=tagged \
	--user-config=user-config.jam \
	$MAKEFLAGS install

################################################################################
download ladspa.h http://www.ladspa.org/ladspa_sdk/ladspa.h.txt
cp ${SRCDIR}/ladspa.h $PREFIX/include/ladspa.h
################################################################################

src vamp-plugin-sdk-2.5 tar.gz http://code.soundsoftware.ac.uk/attachments/download/690/vamp-plugin-sdk-2.5.tar.gz
ed Makefile.in << EOF
%s/= ar/= ${XPREFIX}-ar/
%s/= ranlib/= ${XPREFIX}-ranlib/
wq
EOF
MAKEFLAGS="sdk -j4" autoconfbuild
ed $PREFIX/lib/pkgconfig/vamp-hostsdk.pc << EOF
%s/-ldl//
wq
EOF

src rubberband-1.8.1 tar.bz2 http://code.breakfastquay.com/attachments/download/34/rubberband-1.8.1.tar.bz2
ed Makefile.in << EOF
%s/= ar/= ${XPREFIX}-ar/
wq
EOF
autoconfbuild
ed $PREFIX/lib/pkgconfig/rubberband.pc << EOF
%s/ -lrubberband/ -lrubberband -lfftw3/
wq
EOF

src mingw-libgnurx-2.5.1 tar.gz http://sourceforge.net/projects/mingw/files/Other/UserContributed/regex/mingw-regex-2.5.1/mingw-libgnurx-2.5.1-src.tar.gz
autoconfbuild

src aubio-0.3.2 tar.gz http://aubio.org/pub/aubio-0.3.2 tar.gz
ed Makefile.in << EOF
%s/examples / /
wq
EOF
autoconfbuild
ed $PREFIX/lib/pkgconfig/aubio.pc << EOF
%s/ -laubio/ -laubio -lfftw3f/
wq
EOF

rm -f ${PREFIX}/include/pa_asio.h ${PREFIX}/include/portaudio.h ${PREFIX}/include/asio.h
if test "$ASIO" != "no"; then
	if test ! -d ${SRCDIR}/soundfind.git.reference; then
		git clone --mirror git://github.com/aardvarkk/soundfind.git ${SRCDIR}/soundfind.git.reference
	fi
	cd ${BUILDD}
	git clone --reference ${SRCDIR}/soundfind.git.reference --depth 1 git://github.com/aardvarkk/soundfind.git || true

	download pa_waves2.diff http://robin.linuxaudio.org/tmp/pa_waves2.diff
	src portaudio tgz http://portaudio.com/archives/pa_stable_v19_20140130.tgz
	patch -p1 < ${SRCDIR}/pa_waves2.diff
	autoconfconf --with-asiodir=${BUILDD}/soundfind/ASIOSDK2/ --with-winapi=asio,wmme --without-jack
	ed Makefile << EOF
%s/-luuid//g
wq
EOF
	make $MAKEFLAGS && make install
	cp include/pa_asio.h ${PREFIX}/include/
	cp ${BUILDD}/soundfind/ASIOSDK2/common/asio.h ${PREFIX}/include/
else
	src portaudio tgz http://portaudio.com/archives/pa_stable_v19_20140130.tgz
	autoconfbuild
fi

################################################################################
fi  # $NOSTACK
################################################################################

if test "$ASIO" != "no"; then
	ARDOURCFG="$ARDOURCFG --with-wavesbackend"
fi

################################################################################

cd ${ROOT}
ARDOURSRC=ardour-${WARCH}
# create a git cache to speed up future clones
if test ! -d ${SRCDIR}/ardour.git.reference; then
	git clone --mirror git://git.ardour.org/ardour/ardour.git ${SRCDIR}/ardour.git.reference
fi
git clone --reference ${SRCDIR}/ardour.git.reference -b cairocanvas git://git.ardour.org/ardour/ardour.git $ARDOURSRC || true
cd ${ARDOURSRC}
#if git diff-files --quiet --ignore-submodules -- && git diff-index --cached --quiet HEAD --ignore-submodules --; then
#	git pull
#fi

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

################################################################################
if test -n "$NOBUNDLE"; then
	echo "Done. (NOBUNDLE)"
	exit
fi

ARDOURVERSION=$(git describe | sed 's/-g.*$//')
ARDOURDATE=$(date -R)
./waf install

################################################################################
################################################################################
################################################################################

if test -z "$DESTDIR"; then
	DESTDIR=`mktemp -d`
	trap 'rm -rf $DESTDIR' exit SIGINT SIGTERM
fi

echo " === bundle to $DESTDIR"

ALIBDIR=$DESTDIR/lib/ardour3

rm -rf $DESTDIR
mkdir -p $DESTDIR/bin
mkdir -p $DESTDIR/share/
mkdir -p $ALIBDIR/surfaces
mkdir -p $ALIBDIR/backends
mkdir -p $ALIBDIR/panners
mkdir -p $ALIBDIR/vamp

cp build/libs/gtkmm2ext/gtkmm2ext-*.dll $DESTDIR/bin/
cp build/libs/midi++2/midipp-*.dll $DESTDIR/bin/
cp build/libs/evoral/evoral-*.dll $DESTDIR/bin/
cp build/libs/ardour/ardour-*.dll $DESTDIR/bin/
cp build/libs/timecode/timecode.dll $DESTDIR/bin/
cp build/libs/qm-dsp/qmdsp-*.dll $DESTDIR/bin/
cp build/libs/canvas/canvas-*.dll $DESTDIR/bin/
cp build/libs/pbd/pbd-*.dll $DESTDIR/bin/
cp build/libs/audiographer/audiographer-*.dll $DESTDIR/bin/
cp build/libs/fst/ardour-vst-scanner.exe $DESTDIR/bin/ || true
cp `ls -t build/gtk2_ardour/ardour-*.exe | head -n1` $DESTDIR/bin/ardour.exe

mkdir -p $DESTDIR/lib/gtk-2.0/engines
cp build/libs/clearlooks-newer/clearlooks.dll $DESTDIR/lib/gtk-2.0/engines/libclearlooks.la

cp $PREFIX/bin/*dll $DESTDIR/bin/
cp $PREFIX/lib/*dll $DESTDIR/bin/
rm -rf $DESTDIR/bin/libjack*.dll

cp `find build/libs/surfaces/ -iname "*.dll"` $ALIBDIR/surfaces/
cp `find build/libs/backends/ -iname "*.dll"` $ALIBDIR/backends/
cp `find build/libs/panners/ -iname "*.dll"` $ALIBDIR/panners/

cp -r build/libs/LV2 $ALIBDIR/
cp -r build/libs/vamp-plugins/*ardourvampplugins*.dll $ALIBDIR/vamp/libardourvampplugins.dll

mv $ALIBDIR/surfaces/ardourcp-*.dll $DESTDIR/bin/

# TODO use -static-libgcc -static-libstdc++ -- but for .exe files only
if update-alternatives --query ${XPREFIX}-gcc | grep Value: | grep -q win32; then
	cp /usr/lib/gcc/${XPREFIX}/*-win32/libgcc_s_*.dll $DESTDIR/bin/
	cp /usr/lib/gcc/${XPREFIX}/*-win32/libstdc++-6.dll $DESTDIR/bin/
elif update-alternatives --query ${XPREFIX}-gcc | grep Value: | grep -q posix; then
	cp /usr/lib/gcc/${XPREFIX}/*-posix/libgcc_s_*.dll $DESTDIR/bin/
	cp /usr/lib/gcc/${XPREFIX}/*-posix/libstdc++-6.dll $DESTDIR/bin/
else
	cp /usr/lib/gcc/${XPREFIX}/*/libgcc_s_sjlj-1.dll $DESTDIR/bin/
	cp /usr/lib/gcc/${XPREFIX}/*/libstdc++-6.dll $DESTDIR/bin/
fi

cp -r $PREFIX/share/ardour3 $DESTDIR/share/
cp -r $PREFIX/etc/ardour3/* $DESTDIR/share/ardour3/

cp COPYING $DESTDIR/share/
cp gtk2_ardour/icons/ardour.ico $DESTDIR/share/

# clean stack-dir after install
./waf uninstall
echo " === complete"
du -sh $DESTDIR

################################################################################
### include static gdb - re-zipped binaries from
### http://sourceforge.net/projects/mingw/files/MinGW/Extension/gdb/gdb-7.6.1-1/gdb-7.6.1-1-mingw32-bin.tar.lzma
### http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/4.9.1/threads-win32/sjlj/x86_64-4.9.1-release-win32-sjlj-rt_v3-rev1.7z
if ! echo "$ARDOURCFG" | grep -q -- "--optimize"; then
	download gdb-static-win3264.tar.xz http://robin.linuxaudio.org/gdb-static-win3264.tar.xz
	cd ${SRCDIR}
	tar xf gdb-static-win3264.tar.xz
	cd ${ROOT}/${ARDOURSRC}

	echo " === Creating ardbg.bat"
	cp -r ${SRCDIR}/gdb_$WARCH $DESTDIR/gdb
	cat > $DESTDIR/ardbg.bat << EOF
cd bin
START ..\\gdb\\bin\\gdb.exe ardour.exe
EOF
fi

################################################################################
echo " === Preparing Windows Installer"
NSISFILE=$DESTDIR/a3.nsis
OUTFILE="${TMPDIR}/ardour-${ARDOURVERSION}-${WARCH}-Setup.exe"

if test "$WARCH" = "w64"; then
	PGF=PROGRAMFILES64
else
	PGF=PROGRAMFILES
fi

cat > $NSISFILE << EOF
SetCompressor /SOLID lzma
SetCompressorDictSize 32

!include MUI2.nsh
Name "Ardour3"
OutFile "${OUTFILE}"
RequestExecutionLevel admin
InstallDir "\$${PGF}\\ardour3"
InstallDirRegKey HKLM "Software\\Ardour\\ardour3\\$WARCH" "Install_Dir"

!define MUI_ICON "share\\ardour.ico"
!define MUI_FINISHPAGE_TITLE "Welcome to Ardour"
!define MUI_FINISHPAGE_TEXT "This windows versions or Ardour is provided as-is.\$\\r\$\\nThe ardour community currently has no expertise in supporting windows users, and there are no developers focusing on windows specific issues either.\$\\r\$\\nIf you like Ardour, please consider helping out."
!define MUI_FINISHPAGE_LINK "Ardour Manual"
!define MUI_FINISHPAGE_LINK_LOCATION "http://manual.ardour.org"
#this would run as admin - see http://forums.winamp.com/showthread.php?t=353366
#!define MUI_FINISHPAGE_RUN "\$INSTDIR\\bin\\ardour.exe"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE "share\\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Ardour3 (required)" SecArdour
  SectionIn RO
  SetOutPath \$INSTDIR
  File /r bin
  File /r lib
  File /r share
  File /nonfatal ardbg.bat
  File /nonfatal /r gdb
  WriteRegStr HKLM SOFTWARE\\Ardour\\ardour3\\$WARCH "Install_Dir" "\$INSTDIR"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "DisplayName" "Ardour3"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "UninstallString" '"\$INSTDIR\\uninstall.exe"'
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "NoModify" 1
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour3" "NoRepair" 1
  WriteUninstaller "\$INSTDIR\uninstall.exe"
  CreateShortCut "\$INSTDIR\\Ardour3.lnk" "\$INSTDIR\\bin\\ardour.exe" "" "\$INSTDIR\\bin\\ardour.exe" 0
SectionEnd
Section "Start Menu Shortcuts" SecMenu
  SetShellVarContext all
  CreateDirectory "\$SMPROGRAMS\\ardour3"
  CreateShortCut "\$SMPROGRAMS\\ardour3\\Ardour3.lnk" "\$INSTDIR\\bin\\ardour.exe" "" "\$INSTDIR\\bin\\ardour.exe" 0
  CreateShortCut "\$SMPROGRAMS\\ardour3\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe" "" "\$INSTDIR\\uninstall.exe" 0
SectionEnd
LangString DESC_SecArdour \${LANG_ENGLISH} "Ardour ${ARDOURVERSION}\$\\r\$\\nDebug Version.\$\\r\$\\n${ARDOURDATE}"
LangString DESC_SecMenu \${LANG_ENGLISH} "Create Start-Menu Shortcuts (recommended)."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT \${SecArdour} \$(DESC_SecArdour)
!insertmacro MUI_DESCRIPTION_TEXT \${SecMenu} \$(DESC_SecMenu)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
Section "Uninstall"
  SetShellVarContext all
  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ardour"
  DeleteRegKey HKLM SOFTWARE\\Ardour\\ardour3
  RMDir /r "\$INSTDIR\\bin"
  RMDir /r "\$INSTDIR\\lib"
  RMDir /r "\$INSTDIR\\share"
  RMDir /r "\$INSTDIR\\gdb"
  Delete "\$INSTDIR\\ardbg.bat"
  Delete "\$INSTDIR\\uninstall.exe"
  Delete "\$INSTDIR\\Ardour3.lnk"
  RMDir "\$INSTDIR"
  Delete "\$SMPROGRAMS\\ardour3\\*.*"
  RMDir "\$SMPROGRAMS\\ardour3"
SectionEnd
EOF

apt-get -y install nsis

rm -f ${OUTFILE}
echo " === OutFile: $OUTFILE"
echo " === Building Windows Installer (lzma compression takes ages)"
makensis -V2 $NSISFILE
rm -rf $DESTDIR
ls -lh "$OUTFILE"
