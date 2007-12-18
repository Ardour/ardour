dnl $Id: class_opaque_refcounted.m4 413 2007-05-14 19:28:31Z murrayc $

dnl
dnl _CLASS_OPAQUE_REFCOUNTED(Coverage, PangoCoverage, pango_coverage_new, pango_coverage_ref, pango_coverage_unref)
dnl

define(`_CLASS_OPAQUE_REFCOUNTED',`dnl
_PUSH()
dnl
dnl  Define the args for later macros
define(`__CPPNAME__',`$1')
define(`__CNAME__',`$2')
define(`__OPAQUE_FUNC_NEW',`$3')
define(`__OPAQUE_FUNC_REF',`$4')
define(`__OPAQUE_FUNC_UNREF',`$5')

_POP()
_SECTION(SECTION_CLASS2)
')dnl End of _CLASS_OPAQUE_REFCOUNTED.


dnl
dnl _END_CLASS_OPAQUE_REFCOUNTED()
dnl   denotes the end of a class
dnl
define(`_END_CLASS_OPAQUE_REFCOUNTED',`

_SECTION(SECTION_HEADER3)

namespace Glib
{

  /** A Glib::wrap() method for this object.
   * 
   * @param object The C instance.
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   *
   * @relates __NAMESPACE__::__CPPNAME__
   */
  Glib::RefPtr<__NAMESPACE__::__CPPNAME__> wrap(__CNAME__* object, bool take_copy = false);

} // namespace Glib

_SECTION(SECTION_SRC_GENERATED)

/* Why reinterpret_cast<__CPPNAME__*>(gobject) is needed:
 *
 * A __CPPNAME__ instance is in fact always a __CNAME__ instance.
 * Unfortunately, __CNAME__ cannot be a member of __CPPNAME__,
 * because it is an opaque struct.  Also, the C interface does not provide
 * any hooks to install a destroy notification handler, thus we cannot
 * wrap it dynamically either.
 *
 * The cast works because __CPPNAME__ does not have any member data, and
 * it is impossible to derive from it.  This is ensured by not implementing
 * the (protected) default constructor.  The ctor is protected rather than
 * private just to avoid a compile warning.
 */

namespace Glib
{

Glib::RefPtr<__NAMESPACE__::__CPPNAME__> wrap(__CNAME__* object, bool take_copy)
{
  if(take_copy && object)
    __OPAQUE_FUNC_REF`'(object);

  // See the comment at the top of this file, if you want to know why the cast works.
  return Glib::RefPtr<__NAMESPACE__::__CPPNAME__>(reinterpret_cast<__NAMESPACE__::__CPPNAME__*>(object));
}

} // namespace Glib


__NAMESPACE_BEGIN__

dnl
dnl The implementation:
dnl

ifelse(__OPAQUE_FUNC_NEW,NONE,`dnl
',`dnl else
// static
Glib::RefPtr<__CPPNAME__> __CPPNAME__::create()
{
  // See the comment at the top of this file, if you want to know why the cast works.
  return Glib::RefPtr<__CPPNAME__>(reinterpret_cast<__CPPNAME__*>(__OPAQUE_FUNC_NEW`'()));
}
')dnl endif __OPAQUE_FUNC_NEW

void __CPPNAME__::reference() const
{
  // See the comment at the top of this file, if you want to know why the cast works.
  __OPAQUE_FUNC_REF`'(reinterpret_cast<__CNAME__*>(const_cast<__CPPNAME__*>(this)));
}

void __CPPNAME__::unreference() const
{
  // See the comment at the top of this file, if you want to know why the cast works.
  __OPAQUE_FUNC_UNREF`'(reinterpret_cast<__CNAME__*>(const_cast<__CPPNAME__*>(this)));
}

__CNAME__* __CPPNAME__::gobj()
{
  // See the comment at the top of this file, if you want to know why the cast works.
  return reinterpret_cast<__CNAME__*>(this);
}

const __CNAME__* __CPPNAME__::gobj() const
{
  // See the comment at the top of this file, if you want to know why the cast works.
  return reinterpret_cast<const __CNAME__*>(this);
}

__CNAME__* __CPPNAME__::gobj_copy() const
{
  // See the comment at the top of this file, if you want to know why the cast works.
  __CNAME__ *const gobject = reinterpret_cast<__CNAME__*>(const_cast<__CPPNAME__*>(this));
  __OPAQUE_FUNC_REF`'(gobject);
  return gobject;
}

_IMPORT(SECTION_CC)

__NAMESPACE_END__


dnl
dnl
dnl
dnl
_POP()
dnl
dnl
dnl The actual class, e.g. Pango::FontDescription, declaration:
dnl
_IMPORT(SECTION_CLASS1)
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef __CPPNAME__ CppObjectType;
  typedef __CNAME__ BaseObjectType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

ifelse(__OPAQUE_FUNC_NEW,NONE,`dnl
',`dnl else
  static Glib::RefPtr<__CPPNAME__> create();
')dnl endif __OPAQUE_FUNC_NEW

  // For use with Glib::RefPtr<> only.
  void reference()   const;
  void unreference() const;

  ///Provides access to the underlying C instance.
  __CNAME__*       gobj();

  ///Provides access to the underlying C instance.
  const __CNAME__* gobj() const;

  ///Provides access to the underlying C instance. The caller is responsible for unrefing it. Use when directly setting fields in structs.
  __CNAME__* gobj_copy() const;

protected:
  // Do not derive this.  __NAMESPACE__::__CPPNAME__ can neither be constructed nor deleted.
  __CPPNAME__`'();
  void operator delete(void*, size_t);

private:
  // noncopyable
  __CPPNAME__`'(const __CPPNAME__&);
  __CPPNAME__& operator=(const __CPPNAME__&);

_IMPORT(SECTION_CLASS2)
')

