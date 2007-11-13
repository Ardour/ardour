dnl $Id: class_shared.m4,v 1.6 2006/09/19 20:07:30 murrayc Exp $

define(`_CLASS_START',`dnl
_PUSH(SECTION_CLASS1)
')

dnl
dnl
dnl
define(`_H_VFUNCS_AND_SIGNALS',`dnl

public:
  //C++ methods used to invoke GTK+ virtual functions:
#ifdef GLIBMM_VFUNCS_ENABLED
_IMPORT(SECTION_H_VFUNCS_CPPWRAPPER)
#endif //GLIBMM_VFUNCS_ENABLED

protected:
  //GTK+ Virtual Functions (override these to change behaviour):
#ifdef GLIBMM_VFUNCS_ENABLED
_IMPORT(SECTION_H_VFUNCS)
#endif //GLIBMM_VFUNCS_ENABLED

  //Default Signal Handlers::
#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
_IMPORT(SECTION_H_DEFAULT_SIGNAL_HANDLERS)
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
')


dnl
dnl
dnl
define(`_IMPLEMENTS_INTERFACE_CC',`dnl
_PUSH(SECTION_CC_IMPLEMENTS_INTERFACES)
ifelse(`$2',,,`#ifdef $2'
)dnl
  $1`'::add_interface(get_type());
ifelse(`$2',,,`
#endif // $2
')dnl
_POP()
')



dnl
dnl
dnl
define(`_PH_CLASS_DECLARATION',`dnl
class __CPPNAME__`'_Class : public Glib::Class
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef __CPPNAME__ CppObjectType;
  typedef __REAL_CNAME__ BaseObjectType;
ifdef(`__BOOL_NO_DERIVED_CLASS__',`dnl
',`dnl
  typedef __REAL_CNAME__`'Class BaseClassType;
  typedef __CPPPARENT__`'_Class CppClassParent;
  typedef __REAL_CPARENT__`'Class BaseClassParent;
')dnl

  friend class __CPPNAME__;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  const Glib::Class& init();

ifdef(`__BOOL_NO_DERIVED_CLASS__',`dnl
',`dnl
  static void class_init_function(void* g_class, void* class_data);
')dnl

  static Glib::ObjectBase* wrap_new(GObject*);

protected:

#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Callbacks (default signal handlers):
  //These will call the *_impl member methods, which will then call the existing default signal callbacks, if any.
  //You could prevent the original default signal handlers being called by overriding the *_impl method.
_IMPORT(SECTION_PH_DEFAULT_SIGNAL_HANDLERS)
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED

  //Callbacks (virtual functions):
#ifdef GLIBMM_VFUNCS_ENABLED
_IMPORT(SECTION_PH_VFUNCS)
#endif //GLIBMM_VFUNCS_ENABLED
};
')


dnl
dnl
dnl
define(`_PCC_CLASS_IMPLEMENTATION',`dnl
const Glib::Class& __CPPNAME__`'_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &__CPPNAME__`'_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(_LOWER(__CCAST__)_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:
_IMPORT(SECTION_CC_IMPLEMENTS_INTERFACES)
  }

  return *this;
}
ifdef(`__BOOL_NO_DERIVED_CLASS__',`dnl
',`dnl

void __CPPNAME__`'_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);

#ifdef GLIBMM_VFUNCS_ENABLED
_IMPORT(SECTION_PCC_CLASS_INIT_VFUNCS)
#endif //GLIBMM_VFUNCS_ENABLED

#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
_IMPORT(SECTION_PCC_CLASS_INIT_DEFAULT_SIGNAL_HANDLERS)
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
}
')dnl

#ifdef GLIBMM_VFUNCS_ENABLED
_IMPORT(SECTION_PCC_VFUNCS)
#endif //GLIBMM_VFUNCS_ENABLED

#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
_IMPORT(SECTION_PCC_DEFAULT_SIGNAL_HANDLERS)
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
')



dnl
dnl
dnl
define(`_CC_CLASS_IMPLEMENTATION',`dnl
__CPPNAME__::CppClassType __CPPNAME__::`'__BASE__`'_class_; // initialize static member

GType __CPPNAME__::get_type()
{
  return __BASE__`'_class_.init().get_type();
}

GType __CPPNAME__::get_base_type()
{
  return _LOWER(__CCAST__)_get_type();
}

_IMPORT(SECTION_CC)

dnl _IMPORT(SECTION_CC_SIGNALPROXIES_CUSTOM)

_IMPORT(SECTION_CC_SIGNALPROXIES)

_IMPORT(SECTION_CC_PROPERTYPROXIES)

#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
_IMPORT(SECTION_CC_DEFAULT_SIGNAL_HANDLERS)
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED

#ifdef GLIBMM_VFUNCS_ENABLED
_IMPORT(SECTION_CC_VFUNCS)
_IMPORT(SECTION_CC_VFUNCS_CPPWRAPPER)
#endif //GLIBMM_VFUNCS_ENABLED
')

dnl _PARENT_GCLASS_FROM_OBJECT(object_instance_name)
define(`_PARENT_GCLASS_FROM_OBJECT',`dnl
g_type_class_peek_parent`'(G_OBJECT_GET_CLASS`'($1)) // Get the parent class of the object class (The original underlying C class).
')

dnl _IFACE_PARENT_FROM_OBJECT(object_instance_name)
define(`_IFACE_PARENT_FROM_OBJECT',`dnl
g_type_interface_peek_parent`'( // Get the parent interface of the interface (The original underlying C interface).
g_type_interface_peek`'(G_OBJECT_GET_CLASS`'($1), CppObjectType::get_type`'()) // Get the interface.
)dnl
')

dnl Bonobo doesn't use the "typedef struct _somestruct struct" system.
define(`_STRUCT_NOT_HIDDEN',`dnl
_PUSH()
dnl Define this macro to be tested for later.
define(`__BOOL_STRUCT_NOT_HIDDEN__',`$1')
_POP()
')

dnl _STRUCT_PROTOTYPE()
define(`_STRUCT_PROTOTYPE',`dnl
#ifndef DOXYGEN_SHOULD_SKIP_THIS
ifdef(`__BOOL_STRUCT_NOT_HIDDEN__',`dnl
',`dnl
typedef struct _`'__CNAME__ __CNAME__;
typedef struct _`'__CNAME__`'Class __CNAME__`'Class;
')dnl
#endif /* DOXYGEN_SHOULD_SKIP_THIS */
')

dnl _GTKMMPROC_WIN32_NO_WRAP
dnl Just process it to remove it from the generated file.
dnl generate_wrap_init.pl will look for this in the original .hg file.
dnl
define(`_GTKMMPROC_WIN32_NO_WRAP', dnl
`//This is not available in on Win32.
//This source file will not be compiled,
//and the class will not be registered in wrap_init.h or wrap_init.cc
')dnl


