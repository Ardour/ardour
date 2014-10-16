#!/bin/bash

. ./mingw-env.sh

. ./print-env.sh

if [ -z "$DLLS" ]; then
	echo "ERROR: DLLS variable is not defined..."
	exit 1
fi

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

echo "Copying config files to $PACKAGE_DIR ..."
mkdir -p $PACKAGE_DIR/etc
cp -RL $MINGW_ROOT/etc/fonts $PACKAGE_DIR/etc
cp -RL $MINGW_ROOT/etc/gtk-2.0 $PACKAGE_DIR/etc
cp -RL $MINGW_ROOT/etc/pango $PACKAGE_DIR/etc

cp -R $MINGW_ROOT/lib/gtk-2.0 $PACKAGE_DIR/lib
cp -R $MINGW_ROOT/lib/gdk-pixbuf-2.0 $PACKAGE_DIR/lib
cp $TOOLS_DIR/loaders.cache $PACKAGE_DIR/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache

mkdir -p $PACKAGE_DIR/lib/pango/1.8.0/modules
cp -r $MINGW_ROOT/lib/pango/1.8.0/modules/*.dll $PACKAGE_DIR/lib/pango/1.8.0/modules

cp $TOOLS_DIR/pango.modules $PACKAGE_DIR/etc/pango

cp $TOOLS_DIR/README $PACKAGE_DIR

echo "Copying mingw shared libraries to $PACKAGE_DIR ..."

for i in $DLLS;
do
	copydll "$i" "$PACKAGE_DIR" || exit 1
done

if test x$WITH_JACK != x; then
	echo "Copying JACK server and drivers to $PACKAGE_DIR ..."
	cp $MINGW_ROOT/bin/jackd.exe $PACKAGE_DIR
	cp -r $MINGW_ROOT/bin/jack $PACKAGE_DIR
fi

if test x$WITH_LV2 != x; then
	echo "Moving Bundled LV2 $PACKAGE_DIR ..."
	mv $PACKAGE_DIR/lib/lv2 $PACKAGE_DIR/lib/ardour3/LV2
fi

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

	if test x$WITH_JACK != x; then
		echo "Copying JACK utility programs to $PACKAGE_DIR ..."
		cp $MINGW_ROOT/bin/jack_*.exe $PACKAGE_DIR
	fi

	if test x$WITH_LV2 != x; then
		echo "Copying LV2 utility programs to $PACKAGE_DIR ..."
		cp $MINGW_ROOT/bin/lilv-bench.exe $PACKAGE_DIR
		cp $MINGW_ROOT/bin/lv2info.exe $PACKAGE_DIR
		cp $MINGW_ROOT/bin/lv2ls.exe $PACKAGE_DIR
	fi

	#echo "Copying any debug files to $PACKAGE_DIR ..."
	#cp $MINGW_ROOT/bin/*.debug $PACKAGE_DIR

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
	find $PACKAGE_DIR -type f -name "*.exe*" | xargs $STRIP
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
