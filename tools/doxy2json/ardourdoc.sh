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
./tools/doxy2json/doxy2json \
	`pkg-config --cflags glib-2.0 glibmm-2.4 cairomm-1.0 gtkmm-2.4 | sed 's/-std=c++11 //;s/-pthread //'` \
	$LLVMINCLUDE -I /usr/include/linux \
	-I libs/ardour -I libs/pbd -I libs/lua -I gtk2_ardour -I libs/timecode \
	-I libs/ltc -I libs/evoral \
	-X "_" -X "::" -X "sigc" -X "Atk::" -X "Gdk::" -X "Gtk::" -X "Gio::" \
	-X "Glib::" -X "Pango::" -X "luabridge::" \
	libs/ardour/ardour/* libs/pbd/pbd/* \
	gtk2_ardour/*.h \
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
