#!/usr/bin/php
<?php
## USAGE
#
## generate doc/luadoc.json.gz (lua binding doc)
# ./waf configure --luadoc ....
# ./waf
# ./gtk2_ardour/arluadoc > doc/luadoc.json.gz
#
## generate doc/ardourapi.json.gz (ardour header doxygen doc)
# cd ../../tools/doxy2json
# ./ardourdoc.sh
# cd -
#
## format HTML (using this scripterl)
# php tools/fmt-luadoc.php > /tmp/luadoc.html
#

$options = getopt("m");
if (isset ($options['m'])) {
	$HTMLOUTPUT = false; ## set to false to output ardour-manual
} else {
	$HTMLOUTPUT = true; ## set to false to output ardour-manual
}

################################################################################
################################################################################

$json = gzdecode (file_get_contents (dirname (__FILE__).'/../doc/luadoc.json.gz'));
$doc = array ();
$ardourversion = '';
foreach (json_decode ($json, true) as $b) {
	if (!isset ($b['type'])) {
		if (isset ($b['version'])) { $ardourversion = $b['version']; }
		continue;
	}
	# reserved Lua words -> C++
	$b ['lua'] = preg_replace ('/:_end/', ':end', $b ['lua']);
	$b ['lua'] = preg_replace ('/:_type/', ':type', $b ['lua']);
	# resolves C++ ambiguities
	$b ['ldec'] = preg_replace ('/ const/', '', preg_replace ('/ const&/', '', $b['decl']));
	$b ['ldec'] = preg_replace ('/_VampHost::/', '', $b['ldec']);
	$b ['decl'] = preg_replace ('/_VampHost::/', '', $b['decl']);
	if (isset ($b['ret'])) {
		$b['ret'] = preg_replace ('/ const/', '', preg_replace ('/ const&/', '', $b['ret']));
		$b['ret'] = preg_replace ('/_VampHost::/', '', $b['ret']);
	}
	if (isset ($b['parent'])) {
		$b ['parent'] = preg_replace ('/_VampHost::/', '', $b['parent']);
	}
	$doc[] = $b;
}

if (count ($doc) == 0) {
	fwrite (STDERR, "Failed to read luadoc.json\n");
	exit (EXIT_FAILURE);
}

################################################################################
## Global result variables
################################################################################

$classlist = array ();
$constlist = array ();


################################################################################
## Pre-process the data, collect functions, parse arguments, cross reference
################################################################################


################################################################################
# some internal helper functions first

$funclist = array ();
$classes = array ();
$consts = array ();

function my_die ($msg) {
	fwrite (STDERR, $msg."\n");
	exit (1);
}

##function ptr_strip ($ctype) {
#	# boost::shared_ptr<std::list<boost::shared_ptr<ARDOUR::Route>> > >
#	# -> std::list<ARDOUR::Route>
#	$ctype = preg_replace ('/boost::shared_ptr<([^>]*)[ ]*>/', '$1', $ctype);
#	return preg_replace ('/boost::shared_ptr<([^>]*)[ ]*>/', '$1', $ctype);
#}

function arg2lua ($argtype, $flags = 0) {
	global $classes;
	global $consts;

	# LuaBridge abstracts C++ references
	$flags |= preg_match ('/&$/', $argtype);
	$arg = preg_replace ('/&$/', '', $argtype);
	$arg = preg_replace ('/ $/', '', $arg);

	# filter out basic types
	$builtin = array ('float', 'double', 'bool', 'std::string', 'int', 'short', 'long', 'unsigned int', 'unsigned short', 'unsigned long', 'unsigned char', 'char', 'void', 'char*', 'unsigned char*', 'void*');
	if (in_array ($arg, $builtin)) {
		return array ($arg => $flags);
	}

	if ($arg == 'luabridge::LuaRef') {
		return array ('Lua-Function' => $flags | 4);
	}
	if ($arg == 'LTC_TV_STANDARD') {
		$arg = 'ARDOUR::DSP::LTC_TV_STANDARD';
	}

	# check Class declarations first
	foreach (array_merge ($classes, $consts) as $b) {
		if ($b['ldec'] == $arg) {
			return array ($b['lua'] => $flags);
		}
	}

	# strip class pointers -- TODO Check C'tor for given class
	$arg = preg_replace ('/[&*]*$/', '', $argtype);
	foreach (array_merge ($classes, $consts) as $b) {
		if ($b['ldec'] == $arg) {
			return array ($b['lua'] => $flags);
		}
	}
	if ($flags & 2) {
		return array ($argtype => ($flags | 4));
	} else {
		return array ('--MISSING (' . $argtype . ')--' => ($flags | 4));
	}
}

function stripclass ($classname, $name) {
	$classname .= ':';
	if (strpos ($name, $classname) !== 0) {
		my_die ('invalid class prefix: ' .$classname. ' -- '. $name);
	}
	return substr ($name, strlen ($classname));
}

function datatype ($decl) {
	# TODO handle spaces in type. Works because
	# we don't yet have templated types (with_space <here >)
	return substr ($decl, 0, strrpos ($decl, ' '));
}

function luafn2class ($lua) {
	return substr ($lua, 0, strrpos ($lua, ':'));
}

function luafn2name ($lua) {
	$fn = strrpos ($lua, ':');
	if ($fn !== 0 && strlen($lua) > $fn + 1) {
		return substr ($lua, $fn + 1);
	}
	my_die ('invalid class prefix: '. $name);
}


function checkclass ($b) {
	global $classlist;
	if (!isset ($classlist[luafn2class ($b['lua'])])) {
		my_die ('MISSING CLASS FOR '. print_r ($b['lua'], true));
	}
}

# parse functions argument list to lua-names
function decl2args ($decl) {
	$start = strrpos ($decl, '(');
	$end = strrpos ($decl, ')');
	$args = substr ($decl, $start + 1, $end - $start - 1);
	$arglist = preg_split ('/, */', $args);
	$rv = array ();
	foreach ($arglist as $a) {
		if (empty ($a)) { continue; }
		$rv[] = arg2lua ($a);
	}
	return $rv;
}

function canonical_ctor ($b) {
	$rv = '';
	if (preg_match('/[^(]*\(([^)*]*)\*\)(\(.*\))/', $b['decl'], $matches)) {
		$lc = luafn2class ($b['lua']);
		$cn = str_replace (':', '::', $lc);
		$fn = substr ($lc, 1 + strrpos ($lc, ':'));
		$rv = $cn . '::'. $fn . $matches[2];
	}
	return $rv;
}

function canonical_decl ($b) {
	$rv = '';
	$pfx = '';
	# match clang's declatation format
	if (preg_match('/[^(]*\(([^)*]*)\*\)\((.*)\)/', $b['decl'], $matches)) {
		if (strpos ($b['type'], 'Free Function') !== false) {
			$pfx = str_replace (':', '::', luafn2class ($b['lua'])) . '::';
		}
		$fn = substr ($b['lua'], 1 + strrpos ($b['lua'], ':'));
		$rv = $matches[1] . $fn . '(';
		$arglist = preg_split ('/, */', $matches[2]);
		$first = true;
		foreach ($arglist as $a) {
			if (!$first) { $rv .= ', '; }; $first = false;
			if (empty ($a)) { continue; }
			$a = preg_replace ('/([^>]) >/', '$1>', $a);
			$a = preg_replace ('/([^ ])&/', '$1 &', $a);
			$a = preg_replace ('/std::vector<([^>]*)> const/', 'const std::vector<$1>', $a);
			$a = str_replace ('std::vector', 'vector', $a);
			$a = str_replace ('vector', 'std::vector', $a);
			$a = str_replace ('std::string', 'string', $a);
			$a = str_replace ('string const', 'const string', $a);
			$a = str_replace ('string', 'std::string', $a);
			$rv .= $a;
		}
		$rv .= ')';
	}
	return $pfx . $rv;
}

################################################################################
# step 1: build class indices

foreach ($doc as $b) {
	if (strpos ($b['type'], "[C] ") === 0) {
		$classes[] = $b;
		$classlist[$b['lua']] = $b;
		if (strpos ($b['type'], 'Pointer Class') === false) {
			$classdecl[$b['ldec']] = $b;
		}
	}
}

foreach ($classes as $c) {
	if (strpos ($c['type'], 'Pointer Class') !== false) { continue; }
	if (isset ($c['parent'])) {
		if (isset ($classdecl[$c['parent']])) {
			$classlist[$c['lua']]['luaparent'][] = $classdecl[$c['parent']]['lua'];
		} else {
			my_die ('unknown parent class: ' . print_r ($c, true));
		}
	}
}

# step 2: extract constants/enum
foreach ($doc as $b) {
	switch ($b['type']) {
	case "Constant/Enum":
	case "Constant/Enum Member":
		if (strpos ($b['ldec'], '::') === false) {
			# for extern c enums, use the Lua Namespace
			$b['ldec'] = str_replace (':', '::', luafn2class ($b['lua']));
		}
		$ns = str_replace ('::', ':', $b['ldec']);
		$constlist[$ns][] = $b;
		# arg2lua lookup
		$b['lua'] = $ns;
		$consts[] = $b;
		break;
	default:
		break;
	}
}

# step 3: process functions
foreach ($doc as $b) {
	switch ($b['type']) {
	case "Constructor":
	case "Weak/Shared Pointer Constructor":
		checkclass ($b);
		$classlist[luafn2class ($b['lua'])]['ctor'][] = array (
			'name' => luafn2class ($b['lua']),
			'args' => decl2args ($b['ldec']),
			'cand' => canonical_ctor ($b),
			'nil' => false
		);
		break;
	case "Weak/Shared Pointer NIL Constructor":
		checkclass ($b);
		$classlist[luafn2class ($b['lua'])]['ctor'][] = array (
			'name' => luafn2class ($b['lua']),
			'args' => decl2args ($b['ldec']),
			'cand' => canonical_ctor ($b),
			'nil' => true
		);
		break;
	case "Property":
		checkclass ($b);
		$classlist[luafn2class ($b['lua'])]['props'][] = array (
			'name' => $b['lua'],
			'ret'  => arg2lua (datatype ($b['ldec']))
		);
		break;
	case "Data Member":
		checkclass ($b);
		$classlist[luafn2class ($b['lua'])]['data'][] = array (
			'name' => $b['lua'],
			'ret'  => arg2lua (datatype ($b['ldec']))
		);
		break;
	case "Static C Function":
		checkclass ($b);
		if (strpos ($b['lua'], 'ARDOUR:DataType:') === 0) {
			# special case ARDOUR:DataType convenience c'tor
			$args = array ();
			$ret = array (luafn2class ($b['lua']) => 0);
			$canon = 'ARDOUR::LuaAPI::datatype_ctor_'.strtolower (luafn2name ($b['lua'])).'(lua_State*)';
		} else {
			my_die ('unhandled Static C: ' . print_r($b, true));
		}
		$classlist[luafn2class ($b['lua'])]['func'][] = array (
			'bind' => $b,
			'name' => $b['lua'],
			'args' => $args,
			'ret'  => $ret,
			'ref'  => false,
			'ext'  => false,
			'cand' => $canon
		);
		break;
	case "C Function":
		# we required C functions to be in a class namespace
	case "Ext C Function":
		checkclass ($b);
		$args = array (array ('--lua--' => 0));
		$ret = array ('...' => 0);
		$ns = luafn2class ($b['lua']);
		$cls = $classlist[$ns];
		if (preg_match ('/.*<([^>]*)[ ]*>/', $cls['ldec'], $templ)) {
			# std::vector, std::list types
			switch (stripclass($ns, $b['lua'])) {
			case 'add':
				#$args = array (array ('LuaTable {'.$templ[1].'}' => 0));
				$args = array (arg2lua ($templ[1], 2));
				$ret = array ('LuaTable' => 0);
				break;
			case 'iter':
				$args = array ();
				$ret = array ('LuaIter' => 0);
				break;
			case 'table':
				$args = array ();
				$ret = array ('LuaTable' => 0);
				break;
			default:
				break;
			}
		} else if (strpos ($cls['type'], ' Array') !== false) {
			# catches  C:FloatArray, C:IntArray
			$templ = preg_replace ('/[&*]*$/', '', $cls['ldec']);
			switch (stripclass($ns, $b['lua'])) {
			case 'array':
				$args = array ();
				$ret = array ('LuaMetaTable' => 0);
				break;
			case 'get_table':
				$args = array ();
				$ret = array ('LuaTable' => 0);
				break;
			case 'set_table':
				$args = array (array ('LuaTable {'.$templ.'}' => 0));
				$ret = array ('void' => 0);
				break;
			default:
				break;
			}
		}
		$classlist[luafn2class ($b['lua'])]['func'][] = array (
			'bind' => $b,
			'name' => $b['lua'],
			'args' => $args,
			'ret'  => $ret,
			'ref'  => true,
			'ext'  => true,
			'cand' => canonical_decl ($b)
		);
		break;
	case "Free C Function":
		$funclist[luafn2class ($b['lua'])][] = array (
			'bind' => $b,
			'name' => $b['lua'],
			'args' => $args,
			'ret'  => $ret,
			'ref'  => false,
			'ext'  => true,
			'cand' => str_replace (':', '::', $b['lua']).'(lua_State*)'
		);
		break;
	case "Free Function":
	case "Free Function RefReturn":
		$funclist[luafn2class ($b['lua'])][] = array (
			'bind' => $b,
			'name' => $b['lua'],
			'args' => decl2args ($b['ldec']),
			'ret'  => arg2lua ($b['ret']),
			'ref'  => (strpos ($b['type'], "RefReturn") !== false),
			'cand' => canonical_decl ($b)
		);
		break;
	case "Member Function":
	case "Member Function RefReturn":
	case "Member Pointer Function":
	case "Weak/Shared Pointer Function":
	case "Weak/Shared Pointer Function RefReturn":
	case "Weak/Shared Null Check":
	case "Static Member Function":
		checkclass ($b);
		$classlist[luafn2class ($b['lua'])]['func'][] = array (
			'bind' => $b,
			'name' => $b['lua'],
			'args' => decl2args ($b['ldec']),
			'ret'  => arg2lua ($b['ret']),
			'ref'  => (strpos ($b['type'], "RefReturn") !== false),
			'cand' => canonical_decl ($b)
		);
		break;
	case "Cast":
	case "Weak/Shared Pointer Cast":
		checkclass ($b);
		$classlist[luafn2class ($b['lua'])]['cast'][] = array (
			'bind' => $b,
			'name' => $b['lua'],
			'args' => decl2args ($b['ldec']),
			'ret'  => arg2lua ($b['ret']),
			'ref'  => (strpos ($b['type'], "RefReturn") !== false),
			'cand' => canonical_decl ($b)
		);
		break;
	case "Constant/Enum":
	case "Constant/Enum Member":
		# already handled -> $consts
		break;
	default:
		if (strpos ($b['type'], "[C] ") !== 0) {
			my_die ('unhandled type: ' . $b['type']);
		}
		break;
	}
}


# step 4: collect/group/sort

# step 4a: unify weak/shared Ptr classes
foreach ($classlist as $ns => $cl) {
	if (strpos ($cl['type'], ' Array') !== false) {
		$classlist[$ns]['arr'] = true;
		$classlist[$ns]['cdecl'] = $cl['decl'];
		continue;
	}
	foreach ($classes as $c) {
		if ($c['lua'] == $ns) {
			if (strpos ($c['type'], 'Pointer Class') !== false) {
				$classlist[$ns]['ptr'] = true;
				$classlist[$ns]['cdecl'] = 'boost::shared_ptr< '.$c['decl']. ' >, boost::weak_ptr< '.$c['decl']. ' >';
				break;
			} else {
				$classlist[$ns]['cdecl'] = $c['decl'];
			}
		}
	}
}

# step4b: sanity check
foreach ($classlist as $ns => $cl) {
	if (isset ($classes[$ns]['parent']) && !isset ($classlist[$ns]['luaparent'])) {
		my_die ('missing parent class: ' . print_r ($cl, true));
	}
}

# step 4c: merge free functions into classlist
foreach ($funclist as $ns => $fl) {
	if (isset ($classlist[$ns])) {
		my_die ('Free Funcion in existing namespace: '.$ns.' '. print_r ($ns, true));
	}
	$classlist[$ns]['func'] = $fl;
	$classlist[$ns]['free'] = true;
}

# step 4d: order to chaos
# no array_multisort() here, sub-types are sorted after merging parents
ksort ($classlist);


################################################################################
################################################################################
################################################################################


#### -- split here --  ####

# from here on, only $classlist and $constlist arrays are relevant.
# we also pull in C++ header annotation from doxygen to $api


# read documentation from doxygen
$json = gzdecode (file_get_contents (dirname (__FILE__).'/../doc/ardourapi.json.gz'));
$api = array ();
foreach (json_decode ($json, true) as $a) {
	if (!isset ($a['decl'])) { continue; }
	if (empty ($a['decl'])) { continue; }
	$canon = str_replace (' *', '*', $a['decl']);
	$api[$canon] = $a;
}

# keep track of found/missing doc
$dox_found = 0;
$dox_miss = 0;

# retrive a value from $api
function doxydoc ($canonical_declaration) {
	global $api;
	global $dox_found;
	global $dox_miss;
	if (isset ($api[$canonical_declaration])) {
		$dox_found++;
		return $api[$canonical_declaration]['doc'];
	}
	// remove template namespace e.g.
	//  "ARDOUR::Track::bounceable(boost::shared_ptr<ARDOUR::Processor>"
	//  "ARDOUR::Track::bounceable(boost::shared_ptr<Processor>"
	$cn = preg_replace ('/<[^>]*::([^>]*)>/', '<$1>', $canonical_declaration);
	if (isset ($api[$cn])) {
		$dox_found++;
		return $api[$cn]['doc'];
	}
	#fwrite (STDERR, $canonical_declaration."\n"); # XXX DEBUG

	$dox_miss++;
	return '';
}

################################################################################
# OUTPUT
################################################################################


################################################################################
# Helper functions
define ('NL', "\n");

# constructors, enums (constants) use a dot.  (e.g. "LuaOSC.Address" -> "LuaOSC.Address" )
function ctorname ($name) {
	return htmlentities (str_replace (':', '.', $name));
}

function luaname ($name) {
	# reserved Lua words
	$name = preg_replace ('/:end$/', ':_end', $name);
	$name = preg_replace ('/:type$/', ':_type', $name);
	return $name;
}

# strip class prefix (e.g "Evoral:MidiEvent:channel"  -> "channel")
function shortname ($name) {
	return htmlentities (substr ($name, strrpos ($name, ':') + 1));
}

# retrieve variable name from    array["VARNAME"] => FLAGS
function varname ($a) {
	return array_keys ($a)[0];
}

# recusively collect class parents (derived classes)
function traverse_parent ($ns, &$inherited) {
	global $classlist;
	$rv = '';
	if (isset ($classlist[$ns]['luaparent'])) {
		$parents = array_unique ($classlist[$ns]['luaparent']);
		asort ($parents);
		foreach ($parents as $p) {
			if (!empty ($rv)) { $rv .= ', '; }
			if ($p == $ns) { continue; }
			$rv .= typelink ($p);
			$inherited[$p] = $classlist[$p];
			traverse_parent ($p, $inherited);
		}
	}
	return $rv;
}

# create a cross-reference to a type (class or enum)
# *all* <a> links are generated here, currently anchors on a single page.
function typelink ($a, $short = false, $argcls = '', $linkcls = '', $suffix = '') {
	global $classlist;
	global $constlist;
	if (isset($classlist[$a]['free'])) {
		return '<a class="'.$linkcls.'" href="#'.htmlentities ($a).'">'.($short ? shortname($a) : ctorname($a)).$suffix.'</a>';
	} else if (in_array ($a, array_keys ($classlist))) {
		return '<a class="'.$linkcls.'" href="#'.htmlentities($a).'">'.($short ? shortname($a) : htmlentities($a)).$suffix.'</a>';
	} else if (in_array ($a, array_keys ($constlist))) {
		return '<a class="'.$linkcls.'" href="#'.ctorname ($a).'">'.($short ? shortname($a) : ctorname($a)).$suffix.'</a>';
	} else {
		return '<span class="'.$argcls.'">'.htmlentities($a).$suffix.'</span>';
	}
}

# output format function arguments
function format_args ($args) {
	$rv = '<span class="functionargs"> (';
	$first = true;
	foreach ($args as $a) {
		if (!$first) { $rv .= ', '; }; $first = false;
		$flags = $a[varname ($a)];
		if ($flags & 4) {
			$rv .= '<span>'.varname ($a).'</span>';
		}
		else if ($flags & 2) {
			$rv .= '<em>LuaTable</em> {'.typelink (varname ($a), true, 'em').'}';
		}
		elseif ($flags & 1) {
			$rv .= typelink (varname ($a), true, 'em', '', '&amp;');
		}
		else {
			$rv .= typelink (varname ($a), true, 'em');
		}
	}
	$rv .= ')</span>';
	return $rv;
}

# format doxygen documentation for class-definition
function format_doxyclass ($cl) {
	$rv = '';
	if (isset ($cl['decl'])) {
		$doc = doxydoc ($cl['decl']);
		if (!empty ($doc)) {
			$rv.= '<div class="classdox">'.$doc.'</div>'.NL;
		}
	}
	return $rv;
}

# format doxygen documentation for class-members
function format_doxydoc ($f) {
	$rv = '';
	if (isset ($f['cand'])) {
		$doc = doxydoc ($f['cand']);
		if (!empty ($doc)) {
			$rv.= '<tr><td></td><td class="doc" colspan="2"><div class="dox">'.$doc;
			$rv.= '</div></td></tr>'.NL;
		} else if (0) { # debug
			$rv.= '<tr><td></td><td class="doc" colspan="2"><p>'.htmlentities($f['cand']).'</p>';
			$rv.= '</td></tr>'.NL;
		}
	}
	return $rv;
}

# usort() callback for class-members
function name_sort_cb ($a, $b) {
	return strcmp ($a['name'], $b['name']);
}

# main output function for every class
function format_class_members ($ns, $cl, &$dups) {
	$rv = '';
	# print contructor - if any
	if (isset ($cl['ctor'])) {
		usort ($cl['ctor'], 'name_sort_cb');
		$rv.= ' <tr><th colspan="3">Constructor</th></tr>'.NL;
		foreach ($cl['ctor'] as $f) {
			$rv.= ' <tr>';
			if ($f['nil']) {
				$rv.= '<td class="def"><abbr title="Nil Pointer Constructor">&alefsym;</abbr></td>';
			} else {
				$rv.= '<td class="def">&Copf;</td>';
			}
			$rv.= '<td class="decl">';
			$rv.= '<span class="functionname">'.ctorname (luaname ($f['name'])).'</span>';
			$rv.= format_args ($f['args']);
			$rv.= '</td><td class="fill"></td></tr>'.NL;
			# doxygen documentation (may be empty)
			$rv.= format_doxydoc($f);
		}
	}

	# strip duplicates (inherited or derived methods)
	# e.g  AudioTrack -> Track -> Route -> SessionObject -> Stateful
	# all 5 have "isnil()"
	$nondups = array ();
	if (isset ($cl['func'])) {
		foreach ($cl['func'] as $f) {
			if (in_array (stripclass ($ns, $f['name']), $dups)) { continue; }
			$nondups[] = $f;
		}
	}

	# print methods - if any
	if (count ($nondups) > 0) {
		usort ($nondups, 'name_sort_cb');
		$rv.= ' <tr><th colspan="3">Methods</th></tr>'.NL;
		foreach ($nondups as $f) {
			$dups[] = stripclass ($ns, $f['name']);
			# return value/type
			$rv.= ' <tr><td class="def">';
			if ($f['ref'] && isset ($f['ext'])) {
				# external C functions
				$rv.= '<em>'.varname ($f['ret']).'</em>';
			} elseif ($f['ref'] && varname ($f['ret']) == 'void') {
				# void functions with reference args
				$rv.= '<em>LuaTable</em>(...)';
			} elseif ($f['ref']) {
				# functions with reference args and return value
				$rv.= '<em>LuaTable</em>('.typelink (varname ($f['ret']), true, 'em').', ...)';
			} else {
				# normal class members
				$rv.= typelink (varname ($f['ret']), true, 'em');
			}
			# function declaration and arguments
			$rv.= '</td><td class="decl">';
			$rv.= '<span class="functionname"><abbr title="'.htmlentities($f['bind']['decl']).'">'.stripclass ($ns, luaname ($f['name'])).'</abbr></span>';
			$rv.= format_args ($f['args']);
			$rv.= '</td><td class="fill"></td></tr>'.NL;
			# doxygen documentation (may be empty)
			$rv.= format_doxydoc($f);
		}
	}
	# print cast - if any
	if (isset ($cl['cast'])) {
		usort ($cl['cast'], 'name_sort_cb');
		$rv.= ' <tr><th colspan="3">Cast</th></tr>'.NL;
		foreach ($cl['cast'] as $f) {
			$rv.= ' <tr><td class="def">';
			$rv.= typelink (varname ($f['ret']), true, 'em');
			# function declaration and arguments
			$rv.= '</td><td class="decl">';
			$rv.= '<span class="functionname"><abbr title="'.htmlentities($f['bind']['decl']).'">'.stripclass ($ns, luaname ($f['name'])).'</abbr></span>';
			$rv.= format_args ($f['args']);
			$rv.= '</td><td class="fill"></td></tr>'.NL;
			# doxygen documentation (may be empty)
			$rv.= format_doxydoc($f);
		}
	}

	# print properties - if any
	if (isset ($cl['props'])) {
		usort ($cl['props'], 'name_sort_cb');
		$rv.= ' <tr><th colspan="3">Properties</th></tr>'.NL;
		foreach ($cl['props'] as $f) {
			$rv.= ' <tr><td class="def">'.typelink (array_keys ($f['ret'])[0], false, 'em').'</td><td class="decl">';
			$rv.= '<span class="functionname">'.stripclass ($ns, luaname ($f['name'])).'</span>';
			$rv.= '</td><td class="fill"></td></tr>'.NL;
		}
	}

	# print data members - if any
	if (isset ($cl['data'])) {
		usort ($cl['data'], 'name_sort_cb');
		$rv.= ' <tr><th colspan="3">Data Members</th></tr>'.NL;
		foreach ($cl['data'] as $f) {
			$rv.= ' <tr><td class="def">'.typelink (array_keys ($f['ret'])[0], false, 'em').'</td><td class="decl">';
			$rv.= '<span class="functionname">'.stripclass ($ns, luaname ($f['name'])).'</span>';
			$rv.= '</td><td class="fill"></td></tr>'.NL;
			$f['cand'] = str_replace (':', '::', $f['name']);
			$rv.= format_doxydoc($f);
		}
	}
	return $rv;
}


################################################################################
# Start Output

if ($HTMLOUTPUT) {

?><!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
<title>Ardour Lua Bindings</title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<style type="text/css">
div.header         { text-align:center; }
div.header h2      { margin:0; }
div.header p       { margin:.25em; text-align:center; }
div.luafooter      { text-align:center; font-size:80%; color: #888; margin: 2em 0; }
#luaref            { max-width:60em; margin: 1em auto; }

#luaref h2                 { margin:2em 0 0 0; padding:0em; border-bottom: 1px solid black; }
#luaref h3.cls             { margin:2em 0 0 0; padding: 0 0 0 1em; border: 1px dashed #6666ee; }
#luaref h3.cls abbr        { text-decoration:none; cursor:default; }
#luaref h4.cls             { margin:1em 0 0 0; }
#luaref h3.class           { background-color: #aaee66; }
#luaref h3.enum            { background-color: #aaaaaa; }
#luaref h3.pointerclass    { background-color: #eeaa66; }
#luaref h3.array           { background-color: #66aaee; }
#luaref h3.opaque          { background-color: #6666aa; }
#luaref p                  { text-align: justify; }
#luaref p.cdecl            { text-align: right; float:right; font-size:90%; margin:0; padding: 0 0 0 1em; }
#luaref ul.classindex      { columns: 2; -webkit-columns: 2; -moz-columns: 2; }
#luaref div.clear          { clear:both; }
#luaref p.classinfo        { margin: .25em 0; }
#luaref div.code           { width:80%; margin:.5em auto; }
#luaref div.code div       { width:45%; }
#luaref div.code pre       { line-height: 1.2em; margin: .25em 0; }
#luaref div.code samp      { color: green; font-weight: bold; background-color: #eee; }
#luaref div.classdox       { padding: .1em 1em; }
#luaref div.classdox p     { margin: .5em 0 .5em .6em; }
#luaref div.classdox p     { margin: .5em 0 .5em .6em; }
#luaref div.classdox       { padding: .1em 1em; }
#luaref div.classdox p     { margin: .5em 0 .5em .6em; }
#luaref table.classmembers { width: 100%; }
#luaref table.classmembers th      { text-align:left; border-bottom:1px solid black; padding-top:1em; }
#luaref table.classmembers td.def  { text-align:right; padding-right:.5em;  white-space: nowrap; }
#luaref table.classmembers td.decl { text-align:left; padding-left:.5em; white-space: nowrap; }
#luaref table.classmembers td.doc  { text-align:left; padding-left:.6em; line-height: 1.2em; font-size:80%; }
#luaref table.classmembers td.doc div.dox {background-color:#eee; padding: .1em 1em; }
#luaref table.classmembers td.doc p { margin: .5em 0; }
#luaref table.classmembers td.doc p.para-brief { font-size:120%; }
#luaref table.classmembers td.doc p.para-returns { font-size:120%; }
#luaref table.classmembers td.doc dl { font-size:120%; line-height: 1.3em; }
#luaref table.classmembers td.doc dt { font-style: italic; }
#luaref table.classmembers td.fill { width: 99%; }
#luaref table.classmembers span.em { font-style: italic; }
#luaref span.functionname abbr     { text-decoration:none; cursor:default; }
#luaref table.classmembers td.def abbr { text-decoration:none; cursor:default; }
</style>
</head>
<body>
<div class="header">
<h2>Ardour Lua Bindings</h2>
<p>
<a href="#h_classes">Class Documentation</a>
&nbsp;|&nbsp;
<a href="#h_enum">Enum/Constants</a>
&nbsp;|&nbsp;
<a href="#h_index">Index</a>
</p>
</div>

<!-- #### SNIP #### !-->

<?php

} else {

?>

<p class="warning">
This documentation is far from complete may be inaccurate and subject to change.
</p>

<?php
}
?>

<div id="luaref">

<?php

################################################################################
# some general documentation -- should really go elsehere

?>

<h2 id="h_intro">Overview</h2>
<p>
The top-level entry point are <?=typelink('ARDOUR:Session')?> and <?=typelink('ArdourUI:Editor')?>.
Most other Classes are used indirectly starting with a Session function. e.g. Session:get_routes().
</p>
<p>
A few classes are dedicated to certain script types, e.g. Lua DSP processors have exclusive access to
<?=typelink('ARDOUR:DSP')?> and <?=typelink('ARDOUR:ChanMapping')?>. Action Hooks Scripts to
<?=typelink('LuaSignal:Set')?> etc.
</p>
<p>
Detailed documentation (parameter names, method description) is not yet available. Please stay tuned.
</p>
<h3>Short introduction to Ardour classes</h3>
<p>
Ardour's structure is object oriented. The main object is the Session. A Session contains Audio Tracks, Midi Tracks and Busses.
Audio and Midi tracks are derived from a more general "Track" Object,  which in turn is derived from a "Route" (aka Bus).
(We say "An Audio Track <em>is-a</em> Track <em>is-a</em> Route").
Tracks contain specifics. For Example a track <em>has-a</em> diskstream (for file i/o).
</p>
<p>
Operations are performed on objects. One gets a reference to an object and then calls a method.
e.g <code>obj = Session:route_by_name("Audio")   obj:set_name("Guitar")</code>.
</p>
<p>
Lua automatically follows C++ class inheritance. e.g one can directly call all SessionObject and Route methods on Track object. However lua does not automatically promote objects. A Route object which just happens to be a Track needs to be explicitly cast to a Track. Methods for casts are provided with each class. Note that the cast may fail and return a <em>nil</em> reference.
</p>
<p>
Likewise multiple inheritance is a <a href="http://www.lua.org/pil/16.3.html">non-trivial issue</a> in Lua. To avoid performance penalties involved with lookups, explicit casts are required in this case. One example is <?=typelink('ARDOUR:SessionObject')?> which is-a StatefulDestructible which inherits from both Stateful and Destructible.
</p>
<p>
Object lifetimes are managed by the Session. Most Objects cannot be directly created, but one asks the Session to create or destroy them. This is mainly due to realtime constrains:
you cannot simply remove a track that is currently processing audio. There are various <em>factory</em> methods for object creation or removal.
</p>
<h3>Pass by Reference</h3>
<p>
Since Lua functions are closures, C++ methods that pass arguments by reference cannot be used as-is.
All parameters passed to a C++ method which uses references are returned as Lua Table.
If the C++ method also returns a value it is prefixed. Two parameters are returned: the value and a Lua Table holding the parameters.
</p>

<div class="code">
	<div style="float:left;">C++

<pre><code class="cxx">void set_ref (int&amp; var, long&amp; val)
{
	printf ("%d %ld\n", var, val);
	var = 5;
	val = 7;
}
</code></pre>

	</div>
	<div style="float:right;">Lua

<pre><code class="lua">local var = 0;
ref = set_ref (var, 2);
-- output from C++ printf()
</code><samp class="lua">0 2</samp><code>
-- var is still 0 here
print (ref[1], ref[2])
</code><samp class="lua">5 7</samp></pre>

	</div>
</div>
<div class="clear"></div>
<div class="code">
	<div style="float:left;">

<pre><code class="cxx">int set_ref2 (int &amp;var, std::string unused)
{
	var = 5;
	return 3;
}
</code></pre>

	</div>
	<div style="float:right;">
<pre><code class="lua">rv, ref = set_ref2 (0, "hello");
print (rv, ref[1], ref[2])
</code><samp class="lua">3 5 hello</samp></pre>
	</div>
</div>
<div class="clear"></div>

<h3>Pointer Classes</h3>
<p>
Libardour makes extensive use of reference counted <code>boost::shared_ptr</code> to manage lifetimes.
The Lua bindings provide a complete abstraction of this. There are no pointers in Lua.
For example a <?=typelink('ARDOUR:Route')?> is a pointer in C++, but Lua functions operate on it like it was a class instance.
</p>
<p>
<code>shared_ptr</code> are reference counted. Once assigned to a Lua variable, the C++ object will be kept and remains valid.
It is good practice to assign references to Lua <code>local</code> variables or reset the variable to <code>nil</code> to drop the ref.
</p>
<p>
All pointer classes have a <code>isnil ()</code> method. This is for two cases:
Construction may fail. e.g. <code><?=typelink('ARDOUR:LuaAPI')?>.newplugin()</code>
may not be able to find the given plugin and hence cannot create an object.
</p>
<p>
The second case if for <code>boost::weak_ptr</code>. As opposed to <code>boost::shared_ptr</code> weak-pointers are not reference counted.
The object may vanish at any time.
If Lua code calls a method on a nil object, the interpreter will raise an exception and the script will not continue.
This is not unlike <code>a = nil a:test()</code> which results in en error "<em>attempt to index a nil value</em>".
</p>
<p>
From the Lua side of things there is no distinction between weak and shared pointers. They behave identically.
Below they're indicated in orange and have an arrow to indicate the pointer type.
Pointer Classes cannot be created in Lua scripts. It always requires a call to C++ to create the Object and obtain a reference to it.
</p>


<?php

#################################
# Main output function -- Classes

echo '<h2 id="h_classes">Class Documentation</h2>'.NL;
foreach ($classlist as $ns => $cl) {
	$dups = array ();
	$tbl =  format_class_members ($ns, $cl, $dups);

	# format class title - depending on type
	if (empty ($tbl)) {
		# classes with no members (no ctor, no methods, no data)
		echo '<h3 id="'.htmlentities ($ns).'" class="cls opaque"><abbr title="Opaque Object">&empty;</abbr>&nbsp;'.htmlentities ($ns).'</h3>'.NL;
	}
	else if (isset ($classlist[$ns]['free'])) {
		# free functions (no class)
		echo '<h3 id="'.htmlentities ($ns).'" class="cls freeclass"><abbr title="Namespace">&Nopf;</abbr>&nbsp;'.ctorname($ns).'</h3>'.NL;
	}
	else if (isset ($classlist[$ns]['arr'])) {
		# C Arrays
		echo '<h3 id="'.htmlentities ($ns).'" class="cls array"><abbr title="C Array">&ctdot;</abbr>&nbsp;'.htmlentities ($ns).'</h3>'.NL;
	}
	else if (isset ($classlist[$ns]['ptr'])) {
		# Pointer Classes
		echo '<h3 id="'.htmlentities ($ns).'" class="cls pointerclass"><abbr title="Pointer Class">&Rarr;</abbr>&nbsp;'. htmlentities ($ns).'</h3>'.NL;
	}
	else {
		# Normal Class
		echo '<h3 id="'.htmlentities ($ns).'" class="cls class"><abbr title="Class">&comp;</abbr>&nbsp;'.htmlentities ($ns).'</h3>'.NL;
	}

	# show original C++ declaration
	if (isset ($cl['cdecl'])) {
		echo '<p class="cdecl"><em>C&#8225;</em>: '.htmlentities ($cl['cdecl']).'</p>'.NL;
	}

	# print class inheritance (direct parent *name* only)
	$inherited = array ();
	$isa = traverse_parent ($ns, $inherited);
	if (!empty ($isa)) {
		echo ' <p class="classinfo">is-a: '.$isa.'</p>'.NL;
	}
	echo '<div class="clear"></div>'.NL;


	# class documentation (if any)
	echo format_doxyclass ($cl);

	# member documentation
	if (empty ($tbl)) {
		echo '<p class="classinfo">This class object is only used indirectly as return-value and function-parameter. It provides no methods by itself.</p>'.NL;
	} else {
		echo '<table class="classmembers">'.NL;
		echo $tbl;
		echo ' </table>'.NL;
	}

	# traverse parent classes (all inherited members)
	foreach ($inherited as $pns => $pcl) {
		$tbl = format_class_members ($pns, $pcl, $dups);
		if (!empty ($tbl)) {
			echo '<h4 class="cls">Inherited from '.$pns.'</h4>'.NL;
			echo '<table class="classmembers">'.NL;
			echo $tbl;
			echo '</table>'.NL;
		}
	}
}

####################
# Enum and Constants

echo '<h2 id="h_enum">Enum/Constants</h2>'.NL;
foreach ($constlist as $ns => $cs) {
	echo '<h3 id="'.ctorname ($ns).'" class="cls enum"><abbr title="Enum">&isin;</abbr>&nbsp;'.ctorname ($ns).'</h3>'.NL;
	echo '<ul class="enum">'.NL;
	foreach ($cs as $c) {
		echo '<li class="const">'.ctorname ($c['lua']).'</li>'.NL;
	}
	echo '</ul>'.NL;
}

######################
# Index of all classes

echo '<h2 id="h_index" >Class Index</h2>'.NL;
echo '<ul class="classindex">'.NL;
foreach ($classlist as $ns => $cl) {
	echo '<li>'.typelink($ns).'</li>'.NL;
}
echo '</ul>'.NL;


# see how far there is still to go...
fwrite (STDERR, "Found $dox_found annotations. missing: $dox_miss\n");
echo '<!-- '.$dox_found.' / '.$dox_miss.' !-->'.NL;

?>
</div>
<div class="luafooter">Ardour <?=$ardourversion?> &nbsp;-&nbsp; <?=date('r')?></div>
<?php

if ($HTMLOUTPUT) {
	echo '<!-- #### SNIP #### !-->'.NL;
	echo '</body>'.NL;
	echo '</html>'.NL;
}
