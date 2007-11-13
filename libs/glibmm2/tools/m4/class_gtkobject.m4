dnl $Id: class_gtkobject.m4,v 1.6 2006/03/08 12:23:04 murrayc Exp $



define(`_CLASS_GTKOBJECT',`dnl
_PUSH()
dnl
dnl  Define the args for later macros
define(`__CPPNAME__',`$1')
define(`__CNAME__',`$2')
define(`__CCAST__',`$3')
define(`__BASE__',_LOWER(__CPPNAME__))
define(`__CPPPARENT__',`$4')
define(`__CPARENT__',`$5')
define(`__PCAST__',`($5*)')

dnl Some C types, e.g. GdkWindow or GdkPixmap, are a typedef to their base type,
dnl rather than the real instance type.  That is really ugly, yes.  We get around
dnl the problem by supporting optional __REAL_* arguments to this macro.
define(`__REAL_CNAME__',ifelse(`$6',,__CNAME__,`$6'))
define(`__REAL_CPARENT__',ifelse(`$7',,__CPARENT__,`$7'))


dnl
dnl ----------------------- Constructors -------------------------
dnl


_POP()
_SECTION(SECTION_CLASS2)
')dnl end of _CLASS_GTKOBJECT

dnl Widget and Object, and some others, have custom-written destructor implementations:
define(`_CUSTOM_DTOR',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_CUSTOM_DTOR__',`$1')
_POP()
')

dnl Gtk::Object has a custom-written cast implementation:
define(`_CUSTOM_CTOR_CAST',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_CUSTOM_CTOR_CAST__',`$1')
_POP()
')

dnl Top-level windows can not be manage()ed, so we should not use manage() in wrap_new().
define(`_UNMANAGEABLE',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_UNMANAGEABLE__',`$1')
_POP()
')

dnl Optionally ifdef-out the whole .h and .cc files:
define(`_DEPRECATED',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_DEPRECATED__',`$1')
_POP()
')

dnl Gnome::Canvas::CanvasAA::CanvasAA() needs access to the
dnl normally-private canvas_class_ member variable. See comments there.
define(`_GMMPROC_PROTECTED_GCLASS',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_PROTECTED_GCLASS__',`1')
_POP()
')


dnl
dnl _END_CLASS_GTKOBJECT()
dnl   denotes the end of a class
dnl
define(`_END_CLASS_GTKOBJECT',`

_SECTION(SECTION_HEADER1)
_STRUCT_PROTOTYPE()

__NAMESPACE_BEGIN__ class __CPPNAME__`'_Class; __NAMESPACE_END__
_SECTION(SECTION_HEADER3)

namespace Glib
{
  /** @relates __NAMESPACE__::__CPPNAME__
   * @param object The C instance
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   */
  __NAMESPACE__::__CPPNAME__`'* wrap(__CNAME__`'* object, bool take_copy = false);
} //namespace Glib

dnl
dnl
_SECTION(SECTION_PHEADER)

#include <glibmm/class.h>

__NAMESPACE_BEGIN__

_PH_CLASS_DECLARATION()

__NAMESPACE_END__

_SECTION(SECTION_SRC_GENERATED)

namespace Glib
{

__NAMESPACE__::__CPPNAME__`'* wrap(__CNAME__`'* object, bool take_copy)
{
  return dynamic_cast<__NAMESPACE__::__CPPNAME__ *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

__NAMESPACE_BEGIN__


/* The *_Class implementation: */

_PCC_CLASS_IMPLEMENTATION()

Glib::ObjectBase* __CPPNAME__`'_Class::wrap_new(GObject* o)
{
ifdef(`__BOOL_UNMANAGEABLE__',`dnl
  return new __CPPNAME__`'((__CNAME__*)`'(o)); //top-level windows can not be manage()ed.
',`dnl
  return manage(new __CPPNAME__`'((__CNAME__*)`'(o)));
')
}


/* The implementation: */

ifdef(`__BOOL_CUSTOM_CTOR_CAST__',`dnl necessary for Gtk::Object implementation
',`dnl
__CPPNAME__::__CPPNAME__`'(const Glib::ConstructParams& construct_params)
:
  __CPPPARENT__`'(construct_params)
{
  _IMPORT(SECTION_CC_INITIALIZE_CLASS_EXTRA) dnl Does not seem to work - custom implement it instead.
}

__CPPNAME__::__CPPNAME__`'(__CNAME__* castitem)
:
  __CPPPARENT__`'(__PCAST__`'(castitem))
{
  _IMPORT(SECTION_CC_INITIALIZE_CLASS_EXTRA)  dnl Does not seem to work - custom implement it instead.
}

')dnl
ifdef(`__BOOL_CUSTOM_DTOR__',`dnl
',`dnl
__CPPNAME__::~__CPPNAME__`'()
{
  destroy_();
}

')dnl
_CC_CLASS_IMPLEMENTATION()

__NAMESPACE_END__

dnl
dnl
dnl
dnl
_POP()
dnl The actual class, e.g. Gtk::Widget, declaration:
dnl _IMPORT(SECTION_H_SIGNALPROXIES_CUSTOM)

_IMPORT(SECTION_CLASS1)
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef __CPPNAME__ CppObjectType;
  typedef __CPPNAME__`'_Class CppClassType;
  typedef __CNAME__ BaseObjectType;
  typedef __REAL_CNAME__`'Class BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~__CPPNAME__`'();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

m4_ifdef(`__BOOL_PROTECTED_GCLASS__',
`protected:',`dnl else
private:')dnl endif

  friend class __CPPNAME__`'_Class;
  static CppClassType `'__BASE__`'_class_;

  // noncopyable
  __CPPNAME__`'(const __CPPNAME__&);
  __CPPNAME__& operator=(const __CPPNAME__&);

protected:
  explicit __CPPNAME__`'(const Glib::ConstructParams& construct_params);
  explicit __CPPNAME__`'(__CNAME__* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  __CNAME__*       gobj()       { return reinterpret_cast<__CNAME__*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const __CNAME__* gobj() const { return reinterpret_cast<__CNAME__*>(gobject_); }

_H_VFUNCS_AND_SIGNALS()

private:
_IMPORT(SECTION_CLASS2)

')

