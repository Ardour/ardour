#!/bin/bash
cd `dirname $0`
set -e
make
cd ../..
test -f libs/ardour/ardour/ardour.h
LLVMINCLUDE="-I `llvm-config --includedir` -I `llvm-config --libdir`/clang/`llvm-config --version`/include/"

TMPFILE=`mktemp`
trap 'rm -f $TMPFILE' exit SIGINT SIGTERM

echo "# analyzing source.. -> $TMPFILE"
time ./tools/doxy2json/doxy2json -j 4 \
	$LLVMINCLUDE \
	-D PACKAGE=\"doc\" \
	-D PROGRAM_NAME=\"Ardour\" -D PROGRAM_VERSION=\"6\" -D LOCALEDIR=\"/\" \
	-D ARCH_X86 -D CONFIG_ARCH=\"x86_64\" -D WAF_BUILD -D CANVAS_COMPATIBILITY=1 \
	-D HAVE_AUBIO=1 -D HAVE_ALSA=1 -D HAVE_GLIB=1 -D HAVE_LIBS_LUA=1 -D HAVE_XML=1 -D PTFORMAT=1 \
	-D HAVE_SAMPLERATE=1 -D HAVE_LV2=1 -D HAVE_LV2_1_10_0=1 -D HAVE_SERD=1 -D HAVE_SORD=1 -D HAVE_SRATOM=1 -D HAVE_LILV=1 -D HAVE_LV2_1_0_0=1 \
	-D HAVE_SUIL=1 -D LV2_SUPPORT=1 -D LV2_EXTENDED=1 -D HAVE_GTK=1 -D HAVE_LIBS_GTKMM2EXT=1 \
	-D HAVE_X11=1  -D LXVST_64BIT=1 -D LXVST_SUPPORT=1 -D HAVE_TAGLIB=1 -D HAVE_POSIX_MEMALIGN=1 -D HAVE_VAMPSDK=1 -D HAVE_VAMPHOSTSDK=1 -D HAVE_RUBBERBAND=1 -D ENABLE_NLS=1 \
	-D HAVE_CURL=1 -D HAVE_LO=1 -D HAVE_LRDF=1 -D _VAMP_NO_PLUGIN_NAMESPACE=1 -D _VAMP_NO_HOST_NAMESPACE=1 \
	-I libs/ardour -I libs/pbd -I libs/lua -I gtk2_ardour -I libs/temporal -I libs/audiographer -I libs/ptformat -I libs/fst \
	-I libs/libltc/ltc -I libs/evoral -I libs/canvas -I libs/gtkmm2ext -I libs/midi++2 -I libs/surfaces/control_protocol \
	-I libs/zita-resampler -I libs/fluidsynth/fluidsynth -I libs/waveview -I libs/widgets \
	-I libs -I build/libs/pbd -I build/libs/ardour -I build/gtk2_ardour \
	`pkg-config --cflags glib-2.0 glibmm-2.4 cairomm-1.0 gtkmm-2.4 libxml-2.0 lilv-0 suil-0 raptor2 | sed 's/-std=c++11 //;s/-pthread //'` \
	-X "_" -X "::" -X "sigc" -X "Atk::" -X "Gdk::" -X "Gtk::" -X "Gio::" \
	-X "Glib::" -X "Pango::" -X "luabridge::" \
	\
	libs/ardour/*.cc libs/pbd/*.cc \
	gtk2_ardour/*.cc \
	/usr/include/vamp-sdk/Plugin.h \
	~/gtk/inst/include/vamp-sdk/Plugin.h \
> $TMPFILE

ls -lh $TMPFILE

if test -z "$1"; then
	./tools/doxy2json/postproc.sh $TMPFILE
else
	cp -vi $TMPFILE doc/ardourapi-pre.json
fi
