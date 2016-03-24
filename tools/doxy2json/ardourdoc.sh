#!/bin/bash
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
	if (empty (\$a['decl'])) { continue; }
	if (\$a['decl'] == '::') { continue; }
	if (substr (\$a['decl'], 0, 1) == '_') { continue; }
	if (substr (\$a['decl'], 0, 2) == '::') { continue; }
	if (substr (\$a['decl'], 0, 4) == 'sigc') { continue; }
	if (substr (\$a['decl'], 0, 5) == 'Atk::') { continue; }
	if (substr (\$a['decl'], 0, 5) == 'Gdk::') { continue; }
	if (substr (\$a['decl'], 0, 5) == 'Gtk::') { continue; }
	if (substr (\$a['decl'], 0, 5) == 'Gio::') { continue; }
	if (substr (\$a['decl'], 0, 6) == 'Glib::') { continue; }
	if (substr (\$a['decl'], 0, 7) == 'Pango::') { continue; }
	if (substr (\$a['decl'], 0, 11) == 'luabridge::') { continue; }

	\$a['decl'] = str_replace ('size_t', 'unsigned long', \$a['decl']);
	\$canon = str_replace (' *', '*', \$a['decl']);
	\$api[\$canon] = \$a;
	}
\$jout = array ();
foreach (\$api as \$k => \$a) {
	\$jout[] = \$a;
}
file_put_contents('doc/ardourapi.json', json_encode (\$jout, JSON_PRETTY_PRINT));
EOF

ls -l doc/ardourapi.json
