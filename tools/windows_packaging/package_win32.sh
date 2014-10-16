#!/bin/bash
function copydll () {
	if [ -f $GTK/bin/$1 ] ; then
		echo "cp $GTK/bin/$1 $2"
		cp $GTK/bin/$1 $2 || return 1
		return 0
	fi
	
	if [ -f $GTK/lib/$1 ] ; then
		echo "cp $GTK/lib/$1 $2"
		cp $GTK/lib/$1 $2 || return 1
		return 0
	fi
	
	if [ -f $A3/bin/$1 ] ; then
		echo "cp $A3/bin/$1 $2"
		cp $A3/bin/$1 $2 || return 1
		return 0
	fi

	if [ -f $A3/lib/$1 ] ; then
		echo "$A3/lib/$1 $2"
		cp $A3/lib/$1 $2 || return 1
		return 0
	fi
	if which $1 ; then	  
	  echo "cp `which $1` $2"
	  cp `which $1` $2 || return 1
	  return 0
	fi
	
	echo "there is no $1"
	return 1
}

# libcrypto-10.dll -- OOPS
# libgnomecanvasmm-2.6-1.dll -- OOPS
# iconv.dll == libiconv-2.dll
# libpng15-15.dll == libpng16-16.dll
# liblo-7.dll == liblo.dll

ABANDONEDDLLS='
jack-0.dll
jackserver-0.dll
libbz2-1.dll
libcppunit-1-12-1.dll
libexpat-1.dll
libgnurx-0.dll
libharfbuzz-0.dll
libFLAC-8.dll
libvorbis-0.dll
libvorbisenc-2.dll
libidn-11.dll
libssh2-1.dll
libssl-10.dll
pthreadGC2.dll
'

DLLS='
libiconv-2.dll
libpng16-16.dll
liblo.dll
libart_lgpl_2-2.dll
libatk-1.0-0.dll
libatkmm-1.6-1.dll
libcairo-2.dll
libcairo-gobject-2.dll
libcairomm-1.0-1.dll
libcairo-script-interpreter-2.dll
libcurl-4.dll
libfftw3-3.dll
libfftw3f-3.dll
libfontconfig-1.dll
libfreetype-6.dll
libgailutil-18.dll
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
libgobject-2.0-0.dll
libgthread-2.0-0.dll
libgtkmm-2.4-1.dll
libgtk-win32-2.0-0.dll
libiconv-2.dll
libogg-0.dll
libffi-6.dll
libintl-8.dll
libpango-1.0-0.dll
libpangocairo-1.0-0.dll
libpangoft2-1.0-0.dll
libpangomm-1.4-1.dll
libpangowin32-1.0-0.dll
libpixman-1-0.dll
libsamplerate-0.dll
libsigc-2.0-0.dll
libsndfile-1.dll
libxml2-2.dll
zlib1.dll
libstdc++-6.dll
libgcc_s_sjlj-1.dll
libwinpthread-1.dll
libeay32.dll
ssleay32.dll
libregex-1.dll
libportaudio-2.dll
'
. ./win32-env.sh
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
echo "./waf --destdir=$PACKAGE_DIR install"

./waf --destdir=$PACKAGE_DIR install || exit 1

echo "Moving everything from $PACKAGE_DIR/msys to $PACKAGE_DIR ..."
mv $PACKAGE_DIR/msys/* $PACKAGE_DIR || exit 1
rmdir $PACKAGE_DIR/msys || exit 1


echo "Moving Ardour dll's and executable to $PACKAGE_DIR ..."

echo "mv $PACKAGE_DIR/lib/ardour3/*.dll $PACKAGE_DIR"
echo "mv $PACKAGE_DIR/lib/ardour3/*.exe $PACKAGE_DIR"

mv $PACKAGE_DIR/lib/ardour3/*.dll $PACKAGE_DIR || exit 1
mv $PACKAGE_DIR/lib/ardour3/*.exe $PACKAGE_DIR || exit 1

echo "Deleting import libs ..."

rm $PACKAGE_DIR/lib/*dll.a || exit 1

# delete sh script
rm $PACKAGE_DIR/ardour3 || exit 1

if test x$WITH_TESTS != x ; then
	echo "Copying tests and test data to $PACKAGE_DIR ..."
	cp $BUILD_DIR/libs/pbd/run-tests.exe $PACKAGE_DIR/pbd-run-tests.exe || exit 1
	cp -r $BASE/libs/pbd/test $PACKAGE_DIR/pbd_testdata || exit 1

	cp $BUILD_DIR/libs/evoral/run-tests.exe $PACKAGE_DIR/evoral-run-tests.exe || exit 1
	mkdir -p $PACKAGE_DIR/test/testdata || exit 1
	cp -r $BASE/libs/evoral/test/testdata/TakeFive.mid $PACKAGE_DIR/test/testdata || exit 1

	cp -r $BASE/libs/ardour/test/data $PACKAGE_DIR/ardour_testdata || exit 1
fi

echo "Copying mingw config files to $PACKAGE_DIR ..."
# just copy it all for now
cp -r $MINGW_ROOT/etc $PACKAGE_DIR || exit 1

cp -r $GTK/lib/gtk-2.0 $PACKAGE_DIR/lib || exit 1
cp -r $GTK/lib/gdk-pixbuf-2.0 $PACKAGE_DIR/lib || exit 1
cp $TOOLS_DIR/loaders.cache $PACKAGE_DIR/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache || exit 1

mkdir -p $PACKAGE_DIR/lib/pango/1.6.0/modules || exit 1
cp -r $GTK/lib/pango/1.6.0/modules/*.dll $PACKAGE_DIR/lib/pango/1.6.0/modules || exit 1

cp -r $TOOLS_DIR/mingw64/* $PACKAGE_DIR/etc || exit 1

echo "Copying mingw shared libraries to $PACKAGE_DIR ..."

for i in $DLLS;
do
copydll "$i" "$PACKAGE_DIR" || exit 1
done

echo "Copying JACK server and drivers to $PACKAGE_DIR ..."

# VK: -- FIXIT cp $MINGW_ROOT/bin/jackd.exe $PACKAGE_DIR || exit 1
# VK: -- FIXIT cp -r $MINGW_ROOT/bin/jack $PACKAGE_DIR || exit 1
# VK: -- FIXIT cp $MINGW_ROOT/bin/libportaudio-2.dll $PACKAGE_DIR || exit 1

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

if test x$DEBUG != x ; then

	PACKAGE_SRC_DIR=$PACKAGE_DIR/src
	echo "Copying source files to $PACKAGE_SRC_DIR ..."
	mkdir -p $PACKAGE_SRC_DIR/libs || exit 1
	cp -r $BASE/gtk2_ardour $PACKAGE_SRC_DIR || exit 1
	for i in $SRC_DIRS;
	do
	cp -r -p $BASE/$i $PACKAGE_SRC_DIR/libs || exit 1
	done
	
	echo "Copying JACK utility programs to $PACKAGE_DIR ..."
	# VK: -- FIXIT cp $MINGW_ROOT/bin/jack_*.exe $PACKAGE_DIR || exit 1

	echo "Copying any debug files to $PACKAGE_DIR ..."
	# VK: -- FIXIT cp $MINGW_ROOT/bin/*.debug $PACKAGE_DIR || exit 1

	echo "Copying gdb to $PACKAGE_DIR ..."
	cp $MINGW_ROOT/bin/gdb.exe $PACKAGE_DIR || exit 1

	echo "Copying .gdbinit to $PACKAGE_DIR ..."
	cp $TOOLS_DIR/gdbinit $PACKAGE_DIR/.gdbinit || exit 1

	echo "Copying Gtk demo to $PACKAGE_DIR ..."
	cp $GTK/bin/gtk-demo.exe $PACKAGE_DIR || exit 1
else
	echo "Optimized build Stripping executable ..."
	$STRIP $PACKAGE_DIR/ardour-3.0.exe || exit 1
	echo "Stripping libraries ..." || exit 1
	find $PACKAGE_DIR -type f -name "*.dll*" | xargs $STRIP
fi

if [ "$1" == "--tarball" ]; then
	echo "Creating tarball from $PACKAGE_DIR ..."
	cd $BASE || exit 1
	tar -cvJf $PACKAGE_DIR.tar.xz $PACKAGE_DIR || exit 1
fi

if [ "$1" == "--zip" ]; then
	echo "Creating zip file from $PACKAGE_DIR ..."
	cd $BASE || exit 1
	zip -r $PACKAGE_DIR.zip $PACKAGE_DIR || exit 1
fi
