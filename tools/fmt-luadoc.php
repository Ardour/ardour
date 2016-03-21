#!/usr/bin/php
<?php

$json = file_get_contents('../doc/luadoc.json');
$doc = array();
foreach (json_decode($json, true) as $b) {
	if (!isset ($b['type'])) { continue;}
	$doc[] = $b;
}

################################################################################

$classes = array();
$consts = array();
$constslist = array();
$funclist = array();
$classlist = array();

################################################################################

function my_die ($msg) {
	fwrite(STDERR,$msg."\n");
	exit(1);
}

##function ptr_strip ($ctype) {
#	# boost::shared_ptr<std::list<boost::shared_ptr<ARDOUR::Route>> > >
#	# -> std::list<ARDOUR::Route>
#	$ctype = preg_replace ('/boost::shared_ptr<([^>]*)[ ]*>/', '$1', $ctype);
#	return preg_replace ('/boost::shared_ptr<([^>]*)[ ]*>/', '$1', $ctype);
#}

function arg2lua ($argtype) {
	global $classes;
	global $consts;

	# LuaBridge abstracts C++ references
	$arg = preg_replace ('/&$/', '', $argtype);

	# filter out basic types
	$builtin = array ('float', 'double', 'bool', 'std::string', 'int', 'long', 'unsigned long', 'unsigned int', 'unsigned char', 'char', 'void', 'char*', 'unsigned char*', 'void*');
	if (in_array ($arg, $builtin)) return $argtype;

	# check Class declarations first
	foreach (array_merge ($classes, $consts) as $b) {
		if ($b['decl'] == $arg) {
			return $b['lua'];
		}
	}

	# strip class pointers -- TODO Check C'tor for given class
	$arg = preg_replace ('/[&*]*$/', '', $argtype);
	foreach (array_merge ($classes, $consts) as $b) {
		if ($b['decl'] == $arg) {
			return $b['lua'];
		}
	}
	return '--MISSING (' . $argtype . ')--';
}

function stripclass ($classname, $name) {
	$classname .= ':';
	if (strpos($name, $classname) !== 0) {
		my_die ('invalid class prefix' .$classname. ' -- '. $name);
	}
	return substr ($name, strlen ($classname));
}

function datatype ($decl) {
	# TODO handle spaces in type. Works because
	# we don't yet have templated types (with_space <here >)
	return substr($decl, 0, strpos ($decl, ' '));
}

function luafn2class ($lua) {
	return substr($lua, 0, strrpos ($lua, ':'));
}

function checkclass ($b) {
	global $classlist;
	if (!isset ($classlist[luafn2class($b['lua'])])) {
		my_die ('MISSING CLASS FOR '. print_r($b['lua'], true));
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
		$rv[] = arg2lua($a);
	}
	return $rv;
}

################################################################################
# step 1: build class indices

foreach ($doc as $b) {
	if (strpos ($b['type'], "[C] ") === 0) {
		$classes[] = $b;
		$classlist[$b['lua']] = array ();
		$classindex[$b['lua']] = $b;
		$classdecl[$b['decl']] = $b;
	}
}
foreach ($classes as $c) {
	if (isset ($c['parent'])) {
		if (isset ($classdecl[$c['parent']])) {
			$classindex[$c['lua']]['luaparent'] = $classdecl[$c['parent']]['lua'];
		} else {
			my_die ('unknown parent class: ' . print_r($c, true));
		}
	}
}

# step 2: extract constants/enum
foreach ($doc as $b) {
	switch ($b['type']) {
	case "Constant/Enum":
	case "Constant/Enum Member":
		$ns = luafn2class($b['lua']);
		$constlist[$ns][] = $b;
		if (strpos ($b['decl'], '::') === false) {
			# for extern c enums, use the Lua Namespace
			$b['decl'] = str_replace (':', '::', luafn2class($b['lua']));
		}
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
		$classlist[luafn2class($b['lua'])]['ctor'][] = array (
			'name' => luafn2class($b['lua']),
			'args' => decl2args ($b['decl']),
		);
		break;
	case "Data Member":
		checkclass ($b);
		$classlist[luafn2class($b['lua'])]['data'][] = array (
			'name' => $b['lua'],
			'ret'  => arg2lua (datatype($b['decl']))
		);
		break;
	case "C Function":
		# we required C functions to be in a class namespace
	case "Ext C Function":
		checkclass ($b);
		$classlist[luafn2class($b['lua'])]['func'][] = array (
			'name' => $b['lua'],
			'args' => array ('--custom--'), // XXX
			'ret' => '(LUA)', // XXX
			'ext'  => true
		);
		break;
	case "Free Function":
	case "Free Function RefReturn":
		$funclist[luafn2class($b['lua'])][] = array (
			'name' => $b['lua'],
			'args' => decl2args ($b['decl']),
			'ret'  => arg2lua ($b['ret']),
			'ref'  => (strpos($b['type'], "RefReturn") !== false)
		);
		break;
	case "Member Function":
	case "Member Pointer Function":
	case "Weak/Shared Pointer Function":
	case "Weak/Shared Pointer Function RefReturn":
	case "Weak/Shared Null Check":
	case "Weak/Shared Pointer Cast":
	case "Static Member Function":
		checkclass ($b);
		$classlist[luafn2class($b['lua'])]['func'][] = array (
			'name' => $b['lua'],
			'args' => decl2args ($b['decl']),
			'ret'  => arg2lua ($b['ret']),
			'ref'  => (strpos($b['type'], "RefReturn") !== false)
		);
		#print_r(decl2args ($b['decl']));
		#echo arg2lua ($b['ret'])."\n";
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


## TODO...

# step 4a: unify weak/shared Ptr classes
# step 4b: group namespaces (class, enums)

## list of possible functions producing the given type
## -> see also arg2lua()
#function arg2src ($argtype) {
#	global $classes;
#	global $consts;
#	$rv = array ();
#	# filter out basic types
#	$builtin = array ('float', 'double', 'bool', 'std::string', 'int');
#	if (in_array ($builtin)) return $rv;
#
#	# check class c'tors end enums
#	foreach (array_merge ($classes, $consts) as $b) {
#		if (strpos ($b['decl'], $argtype) === 0) {
#			$rv[$b['lua']] = $b;
#		}
#	}
#	# check C++ declarations next
#	foreach ($doc as $b) {
#		if (isset($b['ret']) && $b['ret'] == $argtype) {
#			$rv[$b['lua']] = $b;
#		}
#		if (strpos ($b['decl'], $argtype) === 0) {
#			$rv[$b['lua']] = $b;
#		}
#	}
#	# check lua-name for extern c enums
#	$argtype = str_replace ('::', ':', $argtype);
#	foreach ($doc as $b) {
#		if (strpos ($b['lua'], $argtype) === 0) {
#			$rv[$b['lua']] = $b;
#		}
#	}
#	return $rv;
#}

# step 5: output
define ('NL', "\n");

echo '<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">'.NL;
echo '<body>';

function format_args ($args) {
	$rv = ' (';
	$first = true;
	foreach ($args as $a) {
		if (!$first) { $rv .= ', '; }
		$rv .= '<em>'.$a.'</em>';
		$first = false;
	}
	$rv .= ')';
	return $rv;
}

function ctorname ($name) {
	return str_replace (':', '.', $name);
}

foreach ($classlist as $ns => $cl) {
	echo '<h2 id="'.$ns.'">'.$ns.'</h2>'.NL;
	if (isset ($classindex[$ns]['luaparent'])) {
		echo ' <p>is-a <a href="#'.$classindex[$ns]['luaparent'].'">'.$classindex[$ns]['luaparent'].'</a></p>'.NL;
	}
	// TODO highlight Pointer Classes

	// TODO optionally traverse all parent classes..
	// function format_class_members()
	if (isset ($cl['ctor'])) {
		echo ' <h3>Constructor</h3>'.NL;
		echo ' <ul>'.NL;
		foreach ($cl['ctor'] as $f) {
			echo '  <li>'.ctorname($f['name']).format_args($f['args']).'</li>'.NL;
		}
		echo ' </ul>'.NL;
	}
	if (isset ($cl['func'])) {
		echo ' <h3>Methods</h3>'.NL;
		echo ' <ul>'.NL;
		foreach ($cl['func'] as $f) {
			echo '  <li>'.$f['ret'].' '.stripclass($ns, $f['name']).format_args($f['args']).'</li>'.NL;
		}
		echo ' </ul>'.NL;
	}
	if (isset ($cl['data'])) {
		echo ' <h3>Data</h3>'.NL;
		echo ' <ul>'.NL;
		foreach ($cl['data'] as $f) {
			echo '  <li>'.stripclass($ns, $f['name']).'</li>'.NL;
		}
		echo ' </ul>'.NL;
	}
}

foreach ($funclist as $ns => $fl) {
	echo '<h2>'.$ns.'</h2>'.NL;
	echo ' <ul>'.NL;
	foreach ($fl as $f) {
		echo '  <li>'.$f['ret'].' '.stripclass($ns, $f['name']).format_args($f['args']).'</li>'.NL;
	}
	echo ' </ul>'.NL;
}

echo "</body>\n</html>\n";
