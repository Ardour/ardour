dnl $Id: convert_base.m4,v 1.3 2006/05/16 19:42:36 murrayc Exp $

#
#  Define a hashing for names
#
define(`__HASH',`__`'m4_translit(`$*',`ABCDEFGHIJKLMNOPQRSTUVWXYZ<>[]&*, ',`abcdefghijklmnopqrstuvwxyzVBNMRSC_')`'')
define(`__EQUIV',`m4_ifdef(EV`'__HASH(`$1'),EV`'__HASH(`$1'),`$1')')

define(`__HASH2',`dnl
pushdef(`__E1',__EQUIV(`$1'))pushdef(`__E2',__EQUIV(`$2'))dnl
m4_ifelse(__E1,__E2,`__EQ',__HASH(__E1)`'__HASH(__E2))`'dnl
popdef(`__E1')popdef(`__E2')`'')

define(`CF__EQ',`$3')

#
#  _CONVERT(fromtype, totype, name, wrap_line)
#    Print the conversion from ctype to cpptype
define(`_CONVERT',`dnl
m4_ifelse(`$2',void,`$3',`dnl
pushdef(`__COV',`CF`'__HASH2(`$1',`$2')')dnl
m4_ifdef(__COV,`m4_indir(__COV,`$1',`$2',`$3')',`
m4_errprint(`No conversion from $1 to $2 defined (line: $4, parameter name: $3)
')
m4_m4exit(1)
')`'dnl
')`'dnl
')


#
#  Functions for populating the tables.
#
define(`_CONVERSION',`
m4_ifelse(`$3',,,`define(CF`'__HASH2(`$1',`$2'),`$3')')
')

define(`_EQUAL',`define(EV`'__HASH(`$1'),`$2')')

/*******************************************************************/


define(`__ARG3__',`$`'3')
define(`_CONV_ENUM',`dnl
_CONVERSION(`$1$2', `$2', (($2)(__ARG3__)))
_CONVERSION(`$1$2', `$1::$2', (($1::$2)(__ARG3__)))
_CONVERSION(`$2', `$1$2', (($1$2)(__ARG3__)))
_CONVERSION(`$1::$2', `$1$2', (($1$2)(__ARG3__)))
')dnl

# e.g. Glib::RefPtr<Gdk::Something> to GdkSomething*
define(`__CONVERT_REFPTR_TO_P',`Glib::unwrap($`'3)')

# e.g. Glib::RefPtr<const Gdk::Something> to GdkSomething*
#define(`__CONVERT_CONST_REFPTR_TO_P',`const_cast<$`'2>($`'3->gobj())')
define(`__CONVERT_CONST_REFPTR_TO_P',`const_cast<$`'2>(Glib::unwrap($`'3))')

# The Sun Forte compiler doesn't seem to be able to handle these, so we are using the altlernative,  __CONVERT_CONST_REFPTR_TO_P_SUN.
# The Sun compiler gives this error, for instance:
#  "widget.cc", line 4463: Error: Overloading ambiguity between "Glib::unwrap<Gdk::Window>(const Glib::RefPtr<const Gdk::Window>&)" and
# "Glib::unwrap<const Gdk::Window>(const Glib::RefPtr<const Gdk::Window>&)".
#
define(`__CONVERT_CONST_REFPTR_TO_P_SUN',`const_cast<$`'2>(Glib::unwrap<$1>($`'3))')


include(convert_gtk.m4)
include(convert_pango.m4)
include(convert_gdk.m4)
include(convert_atk.m4)
include(convert_glib.m4)

