dnl $Id: ctor.m4 376 2007-01-28 22:22:16Z daniel $
dnl
dnl M4 macros for constructor generation.
dnl

dnl Declares and implements the default constructor
dnl
m4_define(`_CTOR_DEFAULT',`dnl
__CPPNAME__`'();
_PUSH(SECTION_CC)
__CPPNAME__::__CPPNAME__`'()
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  __CPPPARENT__`'(Glib::ConstructParams(__BASE__`'_class_.init()))
{
  _IMPORT(SECTION_CC_INITIALIZE_CLASS_EXTRA)
}

_POP()')

dnl Constructors with property initializations.
dnl
dnl _CTOR_IMPL(cppname, cname, cppargs, c_varargs)
dnl            $1       $2     $3       $4
dnl
m4_define(`_CTOR_IMPL',`dnl
_PUSH(SECTION_CC)
__CPPNAME__::$1`'($3)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  __CPPPARENT__`'(Glib::ConstructParams(__BASE__`'_class_.init()m4_ifelse(`$4',,,`, $4, static_cast<char*>(0)')))
{
  _IMPORT(SECTION_CC_INITIALIZE_CLASS_EXTRA)
}

_POP()')

m4_define(`_CONSTRUCT',
 `// Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  __CPPPARENT__`'(Glib::ConstructParams(__BASE__`'_class_.init()m4_ifelse(`$1',,,`, $@, static_cast<char*>(0)')))')

dnl _CONSTRUCT() does not deal with multiple class definitions in one file.
dnl If necessary, _CONSTRUCT_SPECIFIC(BaseClass, Class) must be used instead.
dnl
m4_define(`_CONSTRUCT_SPECIFIC',
 `// Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  $1`'(Glib::ConstructParams(_LOWER(`$2')_class_.init()m4_ifelse(`$3',,,`, m4_shift(m4_shift($@)), static_cast<char*>(0)')))')

dnl Extra code for initialize_class.
dnl Not commonly used.
dnl
m4_define(`_INITIALIZE_CLASS_EXTRA',`dnl
_PUSH(SECTION_CC_INITIALIZE_CLASS_EXTRA)
$1
_POP()')
