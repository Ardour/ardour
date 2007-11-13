dnl $Id: base.m4,v 1.4 2006/03/08 12:23:04 murrayc Exp $
divert(-1)

dnl
dnl The general convention is 
dnl   _* are macros
dnl   __*__ are variables 

dnl
dnl  rename several m4 builtins to avoid name clashes
dnl

define(`_PREFIX_BUILTIN_ALIAS', `define(`m4_$1', defn(`$1'))')
define(`_PREFIX_BUILTIN', `_PREFIX_BUILTIN_ALIAS(`$1')`'undefine(`$1')')

_PREFIX_BUILTIN(`builtin')
_PREFIX_BUILTIN(`decr')
_PREFIX_BUILTIN(`errprint')
_PREFIX_BUILTIN(`esyscmd')
_PREFIX_BUILTIN(`eval')
_PREFIX_BUILTIN(`format')
_PREFIX_BUILTIN(`incr')
_PREFIX_BUILTIN(`index')
_PREFIX_BUILTIN(`indir')
_PREFIX_BUILTIN(`len')
_PREFIX_BUILTIN(`maketemp')
_PREFIX_BUILTIN(`syscmd')
_PREFIX_BUILTIN(`substr')
_PREFIX_BUILTIN(`sysval')

dnl
dnl  More alternative names for m4 macros, not undefined (yet!).
dnl

_PREFIX_BUILTIN_ALIAS(`changecom')
_PREFIX_BUILTIN_ALIAS(`changequote')
_PREFIX_BUILTIN_ALIAS(`define')
_PREFIX_BUILTIN_ALIAS(`divert')
_PREFIX_BUILTIN_ALIAS(`divnum')
_PREFIX_BUILTIN_ALIAS(`ifdef')
_PREFIX_BUILTIN_ALIAS(`ifelse')
_PREFIX_BUILTIN_ALIAS(`include')
_PREFIX_BUILTIN_ALIAS(`m4exit')
_PREFIX_BUILTIN_ALIAS(`m4wrap')
_PREFIX_BUILTIN_ALIAS(`patsubst')
_PREFIX_BUILTIN_ALIAS(`popdef')
_PREFIX_BUILTIN_ALIAS(`pushdef')
_PREFIX_BUILTIN_ALIAS(`shift')
_PREFIX_BUILTIN_ALIAS(`undefine')
_PREFIX_BUILTIN_ALIAS(`undivert')
_PREFIX_BUILTIN_ALIAS(`regexp')
_PREFIX_BUILTIN_ALIAS(`translit')

dnl
dnl  Type Conversion Macros
dnl

m4_include(convert.m4)

dnl
dnl ----------------------- Utility Macros -------------------------
dnl 

dnl
dnl Add a comma before the arg if any, do nothing otherwise
dnl _COMMA_PREFIX(a) -> ,a
dnl _COMMA_PREFIX() -> `'
dnl
define(`_COMMA_PREFIX', `m4_ifelse(m4_eval(m4_len(`$*') >= 1), 1, `,$*')')dnl

dnl
dnl Add a comma after the arg if any, do nothing otherwise
dnl _COMMA_SUFFIX(a) -> a,
dnl _COMMA_SUFFIX() -> `'
dnl
define(`_COMMA_SUFFIX', `m4_ifelse(m4_eval(m4_len(`$*') >= 1), 1, `$*,')')dnl


dnl
dnl  _UPPER(string)
dnl    uppercase a string
define(`_UPPER',`m4_translit(`$*',`abcdefghijklmnopqrstuvwxyz',`ABCDEFGHIJKLMNOPQRSTUVWXYZ')')

dnl
dnl  _LOWER(string)
dnl    lower a string
define(`_LOWER',`m4_translit(`$*',`ABCDEFGHIJKLMNOPQRSTUVWXYZ',`abcdefghijklmnopqrstuvwxyz')')

dnl 
dnl  _QUOTE(macro)  
dnl    If a macro generates an output with commas we need to protect it
dnl    from being broken down and interpreted
define(`_QUOTE',``$*'')

dnl
dnl  _NUM(arglist)   
dnl    count number of arguments
define(`_NUM',`m4_ifelse(m4_len(`$*'),0,0,`$#')')

dnl
dnl For handling of included macro files.
dnl

dnl _PUSH(section_name)
dnl Uses pushdef() to redefine the __DIV__ macro
dnl so that it diverts ouput to the section_name,
dnl or discards it (-1) if no section_name is given.
dnl TODO: However, as far as I can tell, __DIV__ is not used anywhere. murrayc.
define(`_PUSH',`pushdef(`__DIV__',divnum)m4_divert(m4_ifelse($1,,-1,__SEC_$1))dnl`'')

dnl _POP(section_name)
dnl Uses popdef() to go back to the previous definition of the __DIV__ macro.
define(`_POP',`m4_divert(__DIV__)popdef(`__DIV__')dnl`'')

dnl _SECTION(section_name):
dnl m4_divert() sends subsequent output to the specified file:
define(`_SECTION',`m4_divert(__SEC_$1)dnl')

dnl _IMPORT(section_name):
define(`_IMPORT',`m4_undivert(__SEC_$1)dnl')

dnl _GET_TYPE_FUNC(GtkWidget) -> gtk_widget_get_type()
dnl The funny `[A-Z]?' part of the regexp is to catch things like GdkGCFooBar.
define(`_GET_TYPE_FUNC',`dnl
m4_translit(m4_substr(m4_patsubst(`$1',`[A-Z]?[A-Z]',`_\&'),1),`[A-Z]',`[a-z]')_get_type()`'dnl
')

dnl Define a new diversion
dnl In m4, m4_divert() selects the output file to be used for subsequent text output.
dnl 0 is the normal output. We define extra output files with _NEW_SECTION().
dnl This macro seems to redefine __SEC_COUNT as __SEC_COUNT+1, and also
dnl define __SEC_<the macro argument> as __SEC_COUNT.
dnl So it just sets that section identifier to the next number.

define(`__SEC_COUNT__',0)

define(`_NEW_SECTION',`dnl
define(`__SEC_COUNT__',m4_eval(__SEC_COUNT__+1))dnl
define(`__SEC_$1',__SEC_COUNT__)dnl
')


changequote([,])
define([__BT__],[changequote(,)`changequote(`,')])
define([__FT__],[changequote(,)'changequote(`,')])
changequote(`,')

changecom()

dnl
dnl ----------------------- Main Headers -------------------------
dnl

_NEW_SECTION(SECTION_HEADER1)     dnl  header up to the first namespace
_NEW_SECTION(SECTION_HEADER2)     dnl  header after the first namespace
_NEW_SECTION(SECTION_HEADER3)     dnl  header after the first namespace
_NEW_SECTION(SECTION_PHEADER)     dnl  private header
_NEW_SECTION(SECTION_CC_INCLUDES) dnl  section for additional includes
_NEW_SECTION(SECTION_SRC_CUSTOM)        dnl  user supplied implementation
_NEW_SECTION(SECTION_ANONYMOUS_NAMESPACE)  dnl  built implementation in anonymous namespace
_NEW_SECTION(SECTION_SRC_GENERATED)        dnl  built implementation
_NEW_SECTION(SECTION_CLASS1)      dnl  decl to _CLASS
_NEW_SECTION(SECTION_CLASS2)      dnl  _CLASS to end of class
_NEW_SECTION(SECTION_CC)   dnl  section for methods (in current namespace)

_NEW_SECTION(SECTION_CC_IMPLEMENTS_INTERFACES)   dnl Calls SomeBaseInterface::add_interface(get_type()).

dnl Virtual Functions and Default Signal Handlers (Very similar)
_NEW_SECTION(SECTION_H_VFUNCS)      dnl Declaration of vfunc hooks.
_NEW_SECTION(SECTION_H_VFUNCS_CPPWRAPPER) dnl Convenience method, using C++ types, that just calls the vfunc.
_NEW_SECTION(SECTION_H_DEFAULT_SIGNAL_HANDLERS)      dnl Declaration of default signal handler' hooks.

_NEW_SECTION(SECTION_CC_DEFAULT_SIGNAL_HANDLERS)
_NEW_SECTION(SECTION_CC_VFUNCS)
_NEW_SECTION(SECTION_CC_VFUNCS_CPPWRAPPER) dnl Convenience method, using C++ types, that just calls the vfunc.

_NEW_SECTION(SECTION_PH_DEFAULT_SIGNAL_HANDLERS) dnl  private class declaration
_NEW_SECTION(SECTION_PH_VFUNCS) dnl  private class declaration

_NEW_SECTION(SECTION_PCC_DEFAULT_SIGNAL_HANDLERS) dnl  private class implementation
_NEW_SECTION(SECTION_PCC_VFUNCS) dnl  private class implementation

_NEW_SECTION(SECTION_PCC_CLASS_INIT_DEFAULT_SIGNAL_HANDLERS)  dnl  gtk+ class_init function
_NEW_SECTION(SECTION_PCC_CLASS_INIT_VFUNCS)  dnl  gtk+ class_init function


dnl Signal Proxies:
dnl _NEW_SECTION(SECTION_H_SIGNALPROXIES) dnl signal member objects
_NEW_SECTION(SECTION_CC_SIGNALPROXIES) dnl signal member objects

dnl Property Proxies:
dnl _NEW_SECTION(SECTION_H_PROPERTYPROXIES) 
_NEW_SECTION(SECTION_CC_PROPERTYPROXIES)

_NEW_SECTION(SECTION_CC_INITIALIZE_CLASS_EXTRA) dnl For instance, to initialize special member data from all constructors. Not commonly used.

dnl _NEW_SECTION(PROXY)
dnl _NEW_SECTION(SECTION_PCC_OBJECT_INIT) dnl  gtk+ object_init function


_NEW_SECTION(SECTION_CHECK)
_NEW_SECTION(SECTION_USR)

define(`_CHECK',`dnl
_PUSH(SECTION_CHECK)
    $*
_POP()
')

dnl Start of processing
dnl
dnl _START(filname, module,modulecanonical) .e.g _START(button, gtkmm, gtkmm)
define(`_START',`dnl
define(`__MODULE__',$2)dnl
define(`__MODULE_CANONICAL__',$3)dnl
define(`__HEADER_GUARD__',`_`'_UPPER(m4_translit(`$3`'_`'$1', `-', `_'))')dnl
define(`__FILE__',$1)dnl
define(`__DEPRECATION_GUARD__',`_UPPER($3)'`_DISABLE_DEPRECATED')dnl
_SECTION(SECTION_HEADER1)
')

dnl Start deprecation of individual methods:
define(`_DEPRECATE_IFDEF_START',`dnl
#ifndef __DEPRECATION_GUARD__'
)

dnl end deprecation of individual methods:
define(`_DEPRECATE_IFDEF_END',`dnl
#endif // __DEPRECATION_GUARD__'
)

dnl begin optional deprecation of whole class
define(`_DEPRECATE_IFDEF_CLASS_START',`dnl
ifdef(`__BOOL_DEPRECATED__',`dnl
_DEPRECATE_IFDEF_START',`dnl
')
')

dnl end optional deprecation of whole class
define(`_DEPRECATE_IFDEF_CLASS_END',`dnl
ifdef(`__BOOL_DEPRECATED__',`dnl
_DEPRECATE_IFDEF_END',`dnl
')
')

dnl This does all the work of assembling the final output
dnl
dnl _END()
dnl
define(`_END',`dnl
m4_divert(0)dnl
#S 0 dnl Marks the beginning of the header file.

// -*- c++ -*-
// Generated by gtkmmproc -- DO NOT MODIFY!
#ifndef __HEADER_GUARD__`'_H
#define __HEADER_GUARD__`'_H

_DEPRECATE_IFDEF_CLASS_START

m4_ifelse(__MODULE__,glibmm,,`dnl else
#include <glibmm.h>
')dnl
_IMPORT(SECTION_HEADER1)
_IMPORT(SECTION_HEADER2)
_IMPORT(SECTION_HEADER3)

_DEPRECATE_IFDEF_CLASS_END

#endif /* __HEADER_GUARD__`'_H */

#S 1 dnl Marks the beginning of the private header file.

// -*- c++ -*-
// Generated by gtkmmproc -- DO NOT MODIFY!
#ifndef __HEADER_GUARD__`'_P_H
#define __HEADER_GUARD__`'_P_H
_DEPRECATE_IFDEF_CLASS_START
_IMPORT(SECTION_PHEADER)
_DEPRECATE_IFDEF_CLASS_END
#endif /* __HEADER_GUARD__`'_P_H */

#S 2 dnl Marks the beginning of the source file.

// Generated by gtkmmproc -- DO NOT MODIFY!

_DEPRECATE_IFDEF_CLASS_START

#include <__MODULE__/__FILE__.h>
#include <__MODULE__/private/__FILE__`'_p.h>

_IMPORT(SECTION_CC_INCLUDES)
_IMPORT(SECTION_SRC_CUSTOM)

namespace
{
_IMPORT(SECTION_ANONYMOUS_NAMESPACE)
} // anonymous namespace

_IMPORT(SECTION_SRC_GENERATED)
_DEPRECATE_IFDEF_CLASS_END

m4_divert(-1)
m4_undivert()
')

define(`_NAMESPACE',`dnl
_PUSH()
m4_ifdef(`__NAMESPACE__',`dnl
pushdef(`__NAMESPACE__',__NAMESPACE__`::'$1)
pushdef(`__NAMESPACE_BEGIN__',__NAMESPACE_BEGIN__`

namespace '$1`
{')
pushdef(`__NAMESPACE_END__',`} // namespace '$1`

'__NAMESPACE_END__)
',`dnl else
pushdef(`__NAMESPACE__',$1)
pushdef(`__NAMESPACE_BEGIN__',`namespace '$1`
{')
pushdef(`__NAMESPACE_END__',`} // namespace '$1)
')dnl endif __NAMESPACE__
_POP()
')dnl enddef _NAMESPACE

define(`_END_NAMESPACE',`dnl
_PUSH()
popdef(`__NAMESPACE__')
popdef(`__NAMESPACE_BEGIN__')
popdef(`__NAMESPACE_END__')
_POP()
')dnl enddef _END_NAMESPACE

define(`_INCLUDE_FLAG',`__FLAG_$1_INCLUDE_`'_UPPER(m4_translit(`$2',`/.-',`___'))__')dnl

define(`_PH_INCLUDE',`dnl
m4_ifdef(_INCLUDE_FLAG(PH,`$*'),,`dnl else
define(_INCLUDE_FLAG(PH,`$*'))dnl
_PUSH(SECTION_PHEADER)
#include <$*>
_POP()
')dnl endif
')dnl

define(`_CC_INCLUDE',`dnl
m4_ifdef(_INCLUDE_FLAG(CC,`$*'),,`dnl else
define(_INCLUDE_FLAG(CC,`$*'))dnl
_PUSH(SECTION_CC_INCLUDES)
#include <$*>
_POP()
')dnl endif
')dnl

define(`_PINCLUDE', defn(`_PH_INCLUDE'))

# Put these, for instance, around gtkmmproc macros (_WRAP_SIGNAL) 
# to make the #ifndef appear around the generated code in both the .h 
# and .cc files.
# e.g.  _GTKMMPROC_H_AND_CC(#ifndef _SUN_CC_)
# e.g.  _GTKMMPROC_H_AND_CC(#endif //_SUN_CC_)
# _GTKMMPROC_H_AND_CC(code)
define(`_GTKMMPROC_H_AND_CC',`dnl
$1
_PUSH(SECTION_CC)
$1

_POP()
')dnl

# Same thing as _GTKMMPROC_H_AND_CC but for signals (_WRAP_SIGNAL)
define(`_GTKMMPROC_SIGNAL_H_AND_CC',`dnl
$1
_PUSH(SECTION_ANONYMOUS_NAMESPACE)
$1
_POP()

$1
_PUSH(SECTION_H_DEFAULT_SIGNAL_HANDLERS)
$1
_POP()

$1
_PUSH(SECTION_PCC_CLASS_INIT_DEFAULT_SIGNAL_HANDLERS)
$1
_POP()

$1
_PUSH(SECTION_CC_DEFAULT_SIGNAL_HANDLERS)
$1
_POP()

$1
_PUSH(SECTION_PCC_DEFAULT_SIGNAL_HANDLERS)
$1
_POP()

$1
_PUSH(SECTION_CC_SIGNALPROXIES)
$1
_POP()
')dnl

m4_include(class_shared.m4)
m4_include(class_generic.m4)
m4_include(class_gobject.m4)
m4_include(class_gtkobject.m4)
m4_include(class_boxedtype.m4)
m4_include(class_boxedtype_static.m4)
m4_include(class_interface.m4)
m4_include(class_opaque_copyable.m4)
m4_include(class_opaque_refcounted.m4)
m4_include(gerror.m4)
m4_include(signal.m4)
m4_include(vfunc.m4)
m4_include(method.m4)
m4_include(member.m4)
m4_include(compare.m4)
m4_include(ctor.m4)
m4_include(property.m4)
m4_include(enum.m4)

_SECTION(SECTION_HEADER1)

