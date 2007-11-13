dnl $Id: ctor.m4,v 1.1.1.1 2003/01/07 16:59:09 murrayc Exp $

dnl
dnl
dnl  Code generation sections for making a constructor.
dnl
dnl

dnl
dnl Declares and implements the default constructor
dnl
define(`_CTOR_DEFAULT',`dnl
__CPPNAME__`'();
_PUSH(SECTION_CC)
__CPPNAME__::__CPPNAME__`'()
:
  Glib::ObjectBase(0), //Mark this class as gtkmmproc-generated, rather than a custom class, to allow vfunc optimisations.
  __CPPPARENT__`'(Glib::ConstructParams(__BASE__`'_class_.init()))
{
  _IMPORT(SECTION_CC_INITIALIZE_CLASS_EXTRA)
}

_POP()')                 	


dnl
dnl Constructors for _new functions.
dnl               $1      $2    $3     $4
dnl  _CTOR_IMPL(cppname,cname,cppargs,c_varargs)
define(`_CTOR_IMPL',`dnl
_PUSH(SECTION_CC)
__CPPNAME__::$1`'($3)
:
  Glib::ObjectBase(0), //Mark this class as gtkmmproc-generated, rather than a custom class, to allow vfunc optimisations.
  __CPPPARENT__`'(Glib::ConstructParams(__BASE__`'_class_.init()`'ifelse(`$4',,,`, $4')`', (char*) 0))
{
  _IMPORT(SECTION_CC_INITIALIZE_CLASS_EXTRA)
}

_POP()')

define(`_CONSTRUCT',`dnl
Glib::ObjectBase(0), //Mark this class as gtkmmproc-generated, rather than a custom class, to allow vfunc optimisations.
  __CPPPARENT__`'(Glib::ConstructParams(__BASE__`'_class_.init()ifelse(`$1',,,`, $@'), (char*) 0))dnl
')dnl

dnl _CONSTRUCT() does not deal with multiple class definitions in one file.
dnl If necessary, _CONSTRUCT_SPECIFIC(BaseClass, Class) must be used instead.
dnl
define(`_CONSTRUCT_SPECIFIC',`dnl
Glib::ObjectBase(0), //Mark this class as gtkmmproc-generated, rather than a custom class, to allow vfunc optimisations.
  $1`'(Glib::ConstructParams(_LOWER($2)_class_.init()ifelse(`$3',,,`, shift(shift($@))'), (char*) 0))dnl
')dnl


dnl
dnl Extra code for initialize_class.
dnl Not commonly used.
dnl
define(`_INITIALIZE_CLASS_EXTRA',`dnl
_PUSH(SECTION_CC_INITIALIZE_CLASS_EXTRA)
$1
_POP()')

