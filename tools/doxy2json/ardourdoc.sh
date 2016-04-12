#!/bin/bash
cd `dirname $0`
set -e
make
cd ../..
test -f libs/ardour/ardour/ardour.h
LLVMINCLUDE="-I /usr/lib/llvm-3.6/include -I /usr/lib/llvm-3.6/lib/clang/3.6.2/include/"

TMPFILE=`mktemp`
trap 'rm -f $TMPFILE' exit SIGINT SIGTERM

echo "# analyzing source.. -> $TMPFILE"
time ./tools/doxy2json/doxy2json -j 4 \
	$LLVMINCLUDE \
	-D PACKAGE=\"doc\" \
	-D PROGRAM_NAME=\"Ardour\" -D PROGRAM_VERSION=\"4\" -D LOCALEDIR=\"/\" \
	-D ARCH_X86 -D CONFIG_ARCH=\"x86_64\" -D WAF_BUILD \
	-D HAVE_AUBIO=1 -D HAVE_ALSA=1 -D HAVE_GLIB=1 -D HAVE_LIBS_LUA=1 -D HAVE_XML=1 -D PTFORMAT=1 \
	-D HAVE_SAMPLERATE=1 -D HAVE_LV2=1 -D HAVE_LV2_1_2_0=1 -D HAVE_LV2_1_10_0=1 -D HAVE_SERD=1 -D HAVE_SORD=1 -D HAVE_SRATOM=1 -D HAVE_LILV=1 -D HAVE_LV2_1_0_0=1 \
	-D HAVE_LILV_0_16_0=1 -D HAVE_LILV_0_19_2=1 -D HAVE_LILV_0_21_3=1 -D HAVE_SUIL=1 -D LV2_SUPPORT=1 -D LV2_EXTENDED=1 -D HAVE_GTK=1 -D HAVE_LIBS_GTKMM2EXT=1 \
	-D HAVE_X11=1  -D LXVST_64BIT=1 -D LXVST_SUPPORT=1 -D HAVE_TAGLIB=1 -D HAVE_POSIX_MEMALIGN=1 -D HAVE_VAMPSDK=1 -D HAVE_VAMPHOSTSDK=1 -D HAVE_RUBBERBAND=1 -D ENABLE_NLS=1 \
	-D HAVE_CURL=1 -D HAVE_LO=1 -D HAVE_LRDF=1 \
	-I libs/ardour -I libs/pbd -I libs/lua -I gtk2_ardour -I libs/timecode -I libs/audiographer -I libs/ptformat -I libs/fst \
	-I libs/ltc -I libs/evoral -I libs/canvas -I libs/gtkmm2ext -I libs/midi++2 -I libs/surfaces/control_protocol -I libs \
	-I build/libs/pbd -I build/libs/ardour -I build/gtk2_ardour \
	`pkg-config --cflags glib-2.0 glibmm-2.4 cairomm-1.0 gtkmm-2.4 libxml-2.0 lilv-0 suil-0 | sed 's/-std=c++11 //;s/-pthread //'` \
	-X "_" -X "::" -X "sigc" -X "Atk::" -X "Gdk::" -X "Gtk::" -X "Gio::" \
	-X "Glib::" -X "Pango::" -X "luabridge::" \
	\
	libs/ardour/*.cc libs/pbd/*.cc \
	gtk2_ardour/*.cc \
	/usr/include/cairomm-1.0/cairomm/context.h \
> $TMPFILE

ls -lh $TMPFILE

echo "# consolidating JSON"
php << EOF
<?php
\$json = file_get_contents ('$TMPFILE');
\$api = array ();
foreach (json_decode (\$json, true) as \$a) {
	if (!isset (\$a['decl'])) { continue; }

	\$a['decl'] = str_replace ('size_t', 'unsigned long', \$a['decl']);
	\$a['decl'] = str_replace ('uint32_t', 'unsigned int', \$a['decl']);
	\$a['decl'] = str_replace ('int32_t', 'int', \$a['decl']);
	\$a['decl'] = str_replace ('framepos_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('framecnt_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('frameoffset_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('int64_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('uint8_t', 'unsigned char', \$a['decl']);
	\$a['decl'] = str_replace ('pframes_t', 'unsigned int', \$a['decl']);
	\$a['decl'] = str_replace ('uint64_t', 'unsigned long', \$a['decl']);
	\$a['decl'] = str_replace ('const char', 'char', \$a['decl']);
	\$a['decl'] = str_replace ('const float', 'float', \$a['decl']);
	\$a['decl'] = str_replace ('const double', 'double', \$a['decl']);
	\$a['decl'] = str_replace ('const long', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('const unsigned int', 'unsigned int', \$a['decl']);
	\$a['decl'] = str_replace ('const unsigned long', 'unsigned long', \$a['decl']);
	\$canon = str_replace (' *', '*', \$a['decl']);
	\$api[\$canon] = \$a;
}
\$jout = array ();
foreach (\$api as \$k => \$a) {
	\$jout[] = \$a;
}
file_put_contents('doc/ardourapi.json.gz', gzencode (json_encode (\$jout, JSON_PRETTY_PRINT)));
EOF

ls -l doc/ardourapi.json.gz
