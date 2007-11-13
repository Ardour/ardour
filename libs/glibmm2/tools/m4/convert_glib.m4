dnl 
dnl Glib C names have prefix 'G' but C++ namespace Glib
dnl 
define(`_CONV_GLIB_ENUM',`dnl
_CONVERSION(`G$1', `$1', (($1)(__ARG3__)))
_CONVERSION(`G$1', `Glib::$1', ((Glib::$1)(__ARG3__)))
_CONVERSION(`$1', `G$1', ((G$1)(__ARG3__)))
_CONVERSION(`Glib::$1', `G$1', ((G$1)(__ARG3__)))
')dnl

_EQUAL(gchar,char)
_EQUAL(gchar*,char*)
_EQUAL(gchar**,char**)
_EQUAL(gint**,int**)
_EQUAL(gchar**,char*[])
_EQUAL(const gchar*,const char*)
_EQUAL(const-gchar*,const char*)
_EQUAL(gpointer*,void**)

_CONV_GLIB_ENUM(IOStatus)
_CONV_GLIB_ENUM(IOFlags)
_CONV_GLIB_ENUM(IOCondition)
_CONV_GLIB_ENUM(SeekType)
_CONV_GLIB_ENUM(OptionArg)
_CONV_GLIB_ENUM(KeyFileFlags)

_CONVERSION(`gunichar&',`gunichar*',`&($3)')
_CONVERSION(`gsize&',`gsize*',`&($3)')


# Strings:
define(`__GCHARP_TO_USTRING',`Glib::convert_const_gchar_ptr_to_ustring($`'3)')
define(`__GCHARP_TO_STDSTRING',`Glib::convert_const_gchar_ptr_to_stdstring($`'3)')

_CONVERSION(`const Glib::ustring&',`const char*',`$3.c_str()')
_CONVERSION(`const std::string&',`const char*',`$3.c_str()')
_CONVERSION(`const Glib::ustring&',`gchar*',`const_cast<gchar*>($3.c_str())')
_CONVERSION(`gchar*',`Glib::ustring',__GCHARP_TO_USTRING)
_CONVERSION(`const-gchar*',`Glib::ustring',__GCHARP_TO_USTRING)
_CONVERSION(`const gchar*',`Glib::ustring',__GCHARP_TO_USTRING)
_CONVERSION(`const char*',`Glib::ustring',__GCHARP_TO_USTRING)
_CONVERSION(`const char*',`std::string',__GCHARP_TO_STDSTRING)
_CONVERSION(`const gchar*',`const Glib::ustring&',__GCHARP_TO_USTRING)
_CONVERSION(`const char*',`const-gchar*',`$3')
_CONVERSION(`const-gchar*',`const char*',`$3')

_CONVERSION(`return-gchar*',`Glib::ustring',`Glib::convert_return_gchar_ptr_to_ustring($3)')
_CONVERSION(`return-gchar*',`std::string',`Glib::convert_return_gchar_ptr_to_stdstring($3)')
_CONVERSION(`return-char*',`Glib::ustring',`Glib::convert_return_gchar_ptr_to_ustring($3)')

_CONVERSION(`const Glib::RefPtr<Glib::Object>&',`GObject*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<const Glib::Object>&',`GObject*',__CONVERT_CONST_REFPTR_TO_P_SUN(Glib::Object))
_CONVERSION(`GObject*',`Glib::RefPtr<Glib::Object>',`Glib::wrap($3)')
_CONVERSION(`GObject*',`Glib::RefPtr<const Glib::Object>',`Glib::wrap($3)')

_CONVERSION(`Glib::ValueBase&',`GValue*',`($3).gobj()')
_CONVERSION(`const Glib::ValueBase&',`const GValue*',`($3).gobj()')
_CONVERSION(`const Glib::ValueBase&',`GValue*',`const_cast<GValue*>(($3).gobj())')
_CONVERSION(`GValue*', `Glib::ValueBase&', `*reinterpret_cast<Glib::ValueBase*>($3)')
_CONVERSION(`const GValue*', `const Glib::ValueBase&', `*reinterpret_cast<const Glib::ValueBase*>($3)')

_CONVERSION(`OptionGroup&',`GOptionGroup*',`($3).gobj()')
#_CONVERSION(`GOptionGroup*',`OptionGroup',`Glib::wrap(($3), true /* take_copy */)')

