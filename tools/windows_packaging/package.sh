#!/bin/bash

. ./mingw-env.sh

. ./print-env.sh

cd $BASE || exit 1

if ! test -f $BUILD_CACHE_FILE; then
	echo "ERROR: $APPNAME is not configured and built yet..."
	exit 1
fi

if [ -d $PACKAGE_DIR ]; then
	echo "Removing old package directory structure ..."
	rm -rf $PACKAGE_DIR || exit 1
fi

./waf --destdir=$PACKAGE_DIR install || exit 1

echo "Moving Ardour dll's and executable to $PACKAGE_DIR ..."

mv $PACKAGE_DIR/lib/ardour3/*.dll $PACKAGE_DIR || exit 1
mv $PACKAGE_DIR/lib/ardour3/*.exe $PACKAGE_DIR || exit 1

echo "Deleting import libs ..."

rm $PACKAGE_DIR/lib/*dll.a

# delete sh script
rm $PACKAGE_DIR/ardour3

if test x$WITH_TESTS != x ; then
	echo "Copying tests and test data to $PACKAGE_DIR ..."
	cp $BUILD_DIR/libs/pbd/run-tests.exe $PACKAGE_DIR/pbd-run-tests.exe
	cp -r $BASE/libs/pbd/test $PACKAGE_DIR/pbd_testdata

	cp $BUILD_DIR/libs/evoral/run-tests.exe $PACKAGE_DIR/evoral-run-tests.exe
	mkdir -p $PACKAGE_DIR/test/testdata
	cp -r $BASE/libs/evoral/test/testdata/TakeFive.mid $PACKAGE_DIR/test/testdata

	cp -r $BASE/libs/ardour/test/data $PACKAGE_DIR/ardour_testdata
fi

echo "Copying mingw config files to $PACKAGE_DIR ..."
# just copy it all for now
cp -r $MINGW_ROOT/etc $PACKAGE_DIR

cp -r $MINGW_ROOT/lib/gtk-2.0 $PACKAGE_DIR/lib
cp -r $MINGW_ROOT/lib/gdk-pixbuf-2.0 $PACKAGE_DIR/lib
cp $TOOLS_DIR/loaders.cache $PACKAGE_DIR/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache

mkdir -p $PACKAGE_DIR/lib/pango/1.6.0/modules
cp -r $MINGW_ROOT/lib/pango/1.6.0/modules/*.dll $PACKAGE_DIR/lib/pango/1.6.0/modules
cp $TOOLS_DIR/pango.modules $PACKAGE_DIR/etc/pango

DLLS='
jack-0.dll
jackserver-0.dll
libart_lgpl_2-2.dll
libatk-1.0-0.dll
libatkmm-1.6-1.dll
libbz2-1.dll
libcairo-2.dll
libcairo-gobject-2.dll
libcairomm-1.0-1.dll
libcairo-script-interpreter-2.dll
libcppunit-1-12-1.dll
libcrypto-10.dll
libcurl-4.dll
libexpat-1.dll
libfftw3-3.dll
libfftw3f-3.dll
libfontconfig-1.dll
libfreetype-6.dll
libgailutil-18.dll
libgcc_s_sjlj-1.dll
libgdkmm-2.4-1.dll
libgdk_pixbuf-2.0-0.dll
libgdk-win32-2.0-0.dll
libgio-2.0-0.dll
libgiomm-2.4-1.dll
libglib-2.0-0.dll
libglibmm-2.4-1.dll
libglibmm_generate_extra_defs-2.4-1.dll
libgmodule-2.0-0.dll
libgnomecanvas-2-0.dll
libgnomecanvasmm-2.6-1.dll
libgnurx-0.dll
libgobject-2.0-0.dll
libgthread-2.0-0.dll
libgtkmm-2.4-1.dll
libgtk-win32-2.0-0.dll
libharfbuzz-0.dll
libiconv-2.dll
iconv.dll
libFLAC-8.dll
libogg-0.dll
libvorbis-0.dll
libvorbisenc-2.dll
libffi-6.dll
libidn-11.dll
libintl-8.dll
liblo-7.dll
libpango-1.0-0.dll
libpangocairo-1.0-0.dll
libpangoft2-1.0-0.dll
libpangomm-1.4-1.dll
libpangowin32-1.0-0.dll
libpixman-1-0.dll
libpng15-15.dll
libsamplerate-0.dll
libsigc-2.0-0.dll
libsndfile-1.dll
libssh2-1.dll
libssl-10.dll
libstdc++-6.dll
libxml2-2.dll
pthreadGC2.dll
zlib1.dll
'

echo "Copying mingw shared libraries to $PACKAGE_DIR ..."

for i in $DLLS;
do
cp $MINGW_ROOT/bin/$i $PACKAGE_DIR
done

echo "Copying JACK server and drivers to $PACKAGE_DIR ..."

cp $MINGW_ROOT/bin/jackd.exe $PACKAGE_DIR
cp -r $MINGW_ROOT/bin/jack $PACKAGE_DIR
cp $MINGW_ROOT/bin/libportaudio-2.dll $PACKAGE_DIR

SRC_DIRS='
libs/ardour
libs/pbd
libs/gtkmm2ext
libs/midi++2
libs/evoral
libs/panners
libs/timecode
libs/audiographer
'

if [ x$DEBUG = xT ]; then

	PACKAGE_SRC_DIR=$PACKAGE_DIR/src
	echo "Copying source files to $PACKAGE_SRC_DIR ..."
	mkdir -p $PACKAGE_SRC_DIR/libs
	cp -r $BASE/gtk2_ardour $PACKAGE_SRC_DIR
	for i in $SRC_DIRS;
	do
	cp -r -p $BASE/$i $PACKAGE_SRC_DIR/libs
	done
	
	echo "Copying JACK utility programs to $PACKAGE_DIR ..."
	cp $MINGW_ROOT/bin/jack_*.exe $PACKAGE_DIR

	echo "Copying any debug files to $PACKAGE_DIR ..."
	cp $MINGW_ROOT/bin/*.debug $PACKAGE_DIR

	echo "Copying gdb and config files to $PACKAGE_DIR ..."
	cp $MINGW_ROOT/bin/gdb.exe $PACKAGE_DIR
	cp $TOOLS_DIR/gdbinit $PACKAGE_DIR/.gdbinit
	cp $TOOLS_DIR/gdbinit_home $PACKAGE_DIR/gdbinit_home
	cp $TOOLS_DIR/gdb.bat $PACKAGE_DIR/gdb.bat
	cp $TOOLS_DIR/gdb-ardour.bat $PACKAGE_DIR/gdb-ardour.bat

	echo "Copying Gtk demo to $PACKAGE_DIR ..."
	cp $MINGW_ROOT/bin/gtk-demo.exe $PACKAGE_DIR
else
	echo "Optimized build Stripping executable ..."
	$STRIP $PACKAGE_DIR/ardour-3.0.exe
	echo "Stripping libraries ..."
	find $PACKAGE_DIR -type f -name "*.dll*" | xargs $STRIP
fi

if [ "$1" == "--tarball" ]; then
	echo "Creating tarball from $PACKAGE_DIR ..."
	cd $BASE || exit 1
	tar -cvJf $PACKAGE_DIR.tar.xz $PACKAGE_DIR
fi

if [ "$1" == "--zip" ]; then
	echo "Creating zip file from $PACKAGE_DIR ..."
	cd $BASE || exit 1
	zip -r $PACKAGE_DIR.zip $PACKAGE_DIR
fi
