dnl $Id: class_gobject.m4,v 1.6 2004/04/29 22:43:44 murrayc Exp $


define(`_CLASS_GOBJECT',`dnl
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


_POP()
_SECTION(SECTION_CLASS2)
') dnl end of _CLASS_GOBJECT

dnl Widget and Object, and some others, have custom-written destructor implementations:
define(`_CUSTOM_DTOR',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_CUSTOM_DTOR__',`$1')
_POP()
')

dnl Gdk::Pixmap_Class::wrap_new() needs a custom implementation, in order
dnl to create a Gdk::Bitmap object if appropriate.  See comments there.
define(`_CUSTOM_WRAP_NEW',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_CUSTOM_WRAP_NEW__',`1')
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

dnl Some of the Gdk types are actually direct typedefs of their base type.
dnl This means that 2 wrap functions would have the same argument.
dnl define(`_NO_WRAP_FUNCTION',`dnl
dnl _PUSH()
dnl Define this macro to be tested for later.
dnl define(`__BOOL_NO_WRAP_FUNCTION__',`$1')
dnl _POP()
dnl ')

dnl
dnl _CREATE_METHOD(args_type_and_name_hpp, args_type_and_name_cpp,args_name_only);
dnl
define(`_CREATE_METHOD',`
  static Glib::RefPtr<`'__CPPNAME__`'> create(`'$1`');
_PUSH(SECTION_CC)
Glib::RefPtr<`'__CPPNAME__`'> __CPPNAME__`'::create(`'$2`')
{
  return Glib::RefPtr<`'__CPPNAME__`'>( new __CPPNAME__`'(`'$3`') );
}
_POP()
')


dnl
dnl _END_CLASS_GOBJECT()
dnl   denotes the end of a class
dnl
define(`_END_CLASS_GOBJECT',`
_SECTION(SECTION_HEADER1)
ifdef(`__BOOL_NO_WRAP_FUNCTION__',`dnl
',`dnl
_STRUCT_PROTOTYPE()
')dnl

__NAMESPACE_BEGIN__ class __CPPNAME__`'_Class; __NAMESPACE_END__
_SECTION(SECTION_HEADER3)

ifdef(`__BOOL_NO_WRAP_FUNCTION__',`dnl
',`dnl
namespace Glib
{
  /** @relates __NAMESPACE__::__CPPNAME__
   * @param object The C instance
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   */
  Glib::RefPtr<__NAMESPACE__::__CPPNAME__> wrap(__REAL_CNAME__`'* object, bool take_copy = false);
}
')dnl


dnl
dnl
_SECTION(SECTION_PHEADER)

#include <glibmm/class.h>

__NAMESPACE_BEGIN__

_PH_CLASS_DECLARATION()

__NAMESPACE_END__

_SECTION(SECTION_SRC_GENERATED)

ifdef(`__BOOL_NO_WRAP_FUNCTION__',`dnl
',`dnl else
namespace Glib
{

Glib::RefPtr<__NAMESPACE__::__CPPNAME__> wrap(__REAL_CNAME__`'* object, bool take_copy)
{
  return Glib::RefPtr<__NAMESPACE__::__CPPNAME__>( dynamic_cast<__NAMESPACE__::__CPPNAME__*> (Glib::wrap_auto ((GObject*)(object), take_copy)) );
  //We use dynamic_cast<> in case of multiple inheritance.
}

} /* namespace Glib */
')dnl endif




__NAMESPACE_BEGIN__


/* The *_Class implementation: */

_PCC_CLASS_IMPLEMENTATION()

m4_ifdef(`__BOOL_CUSTOM_WRAP_NEW__',,`dnl else
Glib::ObjectBase* __CPPNAME__`'_Class::wrap_new(GObject* object)
{
  return new __CPPNAME__`'((__CNAME__*)`'object);
}

')dnl endif

/* The implementation: */

__CNAME__* __CPPNAME__::gobj_copy()
{
  reference();
  return gobj();
}

__CPPNAME__::__CPPNAME__`'(const Glib::ConstructParams& construct_params)
:
  __CPPPARENT__`'(construct_params)
{}

__CPPNAME__::__CPPNAME__`'(__CNAME__* castitem)
:
  __CPPPARENT__`'(__PCAST__`'(castitem))
{}

ifdef(`__BOOL_CUSTOM_DTOR__',`dnl
',`dnl
__CPPNAME__::~__CPPNAME__`'()
{}

')dnl


_CC_CLASS_IMPLEMENTATION()

__NAMESPACE_END__

dnl
dnl
dnl
dnl
_POP()
dnl
dnl The actual class, e.g. Gtk::Widget, declaration:
dnl _IMPORT(SECTION_H_SIGNALPROXIES_CUSTOM)

_IMPORT(SECTION_CLASS1)

#ifndef DOXYGEN_SHOULD_SKIP_THIS

public:
  typedef __CPPNAME__ CppObjectType;
  typedef __CPPNAME__`'_Class CppClassType;
  typedef __CNAME__ BaseObjectType;
  typedef __REAL_CNAME__`'Class BaseClassType;

m4_ifdef(`__BOOL_PROTECTED_GCLASS__',
`protected:',`dnl else
private:')dnl endif
  friend class __CPPNAME__`'_Class;
  static CppClassType `'__BASE__`'_class_;

private:
  // noncopyable
  __CPPNAME__`'(const __CPPNAME__&);
  __CPPNAME__& operator=(const __CPPNAME__&);

protected:
  explicit __CPPNAME__`'(const Glib::ConstructParams& construct_params);
  explicit __CPPNAME__`'(__CNAME__* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
  virtual ~__CPPNAME__`'();

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GObject.
  __CNAME__*       gobj()       { return reinterpret_cast<__CNAME__*>(gobject_); }

  ///Provides access to the underlying C GObject.
  const __CNAME__* gobj() const { return reinterpret_cast<__CNAME__*>(gobject_); }

  ///Provides access to the underlying C instance. The caller is responsible for unrefing it. Use when directly setting fields in structs.
  __CNAME__* gobj_copy();

private:
_IMPORT(SECTION_CLASS2)

public:
_H_VFUNCS_AND_SIGNALS()

')

