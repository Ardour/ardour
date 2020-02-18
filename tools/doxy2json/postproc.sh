#!/bin/bash
cd `dirname $0`
cd ../..

set -e

test -f "$1"

echo "# consolidating JSON $1"
php << EOF
<?php
\$json = file_get_contents ('$1');
\$api = array ();
foreach (json_decode (\$json, true) as \$a) {
	if (!isset (\$a['decl'])) { continue; }

	\$a['decl'] = str_replace ('__cxx11::', '', \$a['decl']);
	\$a['decl'] = str_replace ('size_t', 'unsigned long', \$a['decl']);
	\$a['decl'] = str_replace ('uint32_t', 'unsigned int', \$a['decl']);
	\$a['decl'] = str_replace ('int32_t', 'int', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::samplepos_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::samplecnt_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::sampleoffset_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::frameoffset_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::pframes_t', 'unsigned int', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::Sample', 'float', \$a['decl']);
	\$a['decl'] = str_replace ('ARDOUR::gain_t', 'float', \$a['decl']);
	\$a['decl'] = str_replace ('samplepos_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('samplecnt_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('sampleoffset_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('frameoffset_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('pframes_t', 'unsigned int', \$a['decl']);
	\$a['decl'] = preg_replace ('/\bSample\b/', 'float', \$a['decl']);
	\$a['decl'] = str_replace ('gain_t', 'float', \$a['decl']);
	\$a['decl'] = str_replace ('int64_t', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('uint8_t', 'unsigned char', \$a['decl']);
	\$a['decl'] = str_replace ('uint64_t', 'unsigned long', \$a['decl']);
	\$a['decl'] = str_replace ('const char', 'char', \$a['decl']);
	\$a['decl'] = str_replace ('const float', 'float', \$a['decl']);
	\$a['decl'] = str_replace ('const double', 'double', \$a['decl']);
	\$a['decl'] = str_replace ('const long', 'long', \$a['decl']);
	\$a['decl'] = str_replace ('const unsigned int', 'unsigned int', \$a['decl']);
	\$a['decl'] = str_replace ('const unsigned long', 'unsigned long', \$a['decl']);
	\$a['decl'] = str_replace (' ::Vamp::', ' Vamp::', \$a['decl']);
	\$a['decl'] = str_replace ('Cairo::Context::set_line_join(LineJoin)', 'Cairo::Context::set_line_join(Cairo::LineJoin)', \$a['decl']);
	\$a['decl'] = str_replace ('Cairo::Context::set_line_cap(LineCap)', 'Cairo::Context::set_line_cap(Cairo::LineCap)', \$a['decl']);
	\$a['decl'] = str_replace ('Cairo::Context::set_operator(Operator)', 'Cairo::Context::set_operator(Cairo::Operator)', \$a['decl']);
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
