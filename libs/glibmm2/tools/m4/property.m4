dnl $Id: property.m4 291 2006-05-12 08:08:45Z murrayc $

dnl
dnl
dnl  Code generation sections for properties
dnl
dnl

dnl
dnl _PROPERTY_PROXY(name, name_underscored, cpp_type, proxy_suffix, docs)
dnl proxy_suffix could be "_WriteOnly" or "_ReadOnly"
dnl The method will be const if the propertyproxy is _ReadOnly.
dnl
define(`_PROPERTY_PROXY',`dnl
dnl
dnl Put spaces around the template parameter if necessary.
pushdef(`__PROXY_TYPE__',`dnl
Glib::PropertyProxy$4<'ifelse(regexp(_QUOTE($3),`>$'),`-1',_QUOTE($3),` '_QUOTE($3)` ')`>'dnl
)dnl
#ifdef GLIBMM_PROPERTIES_ENABLED
/** $5
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  __PROXY_TYPE__ property_$2`'() ifelse($4,_ReadOnly, const,);
#endif //#GLIBMM_PROPERTIES_ENABLED
_PUSH(SECTION_CC_PROPERTYPROXIES)
#ifdef GLIBMM_PROPERTIES_ENABLED
__PROXY_TYPE__ __CPPNAME__::property_$2`'() ifelse($4,_ReadOnly, const,)
{
  return __PROXY_TYPE__`'(this, "$1");
}
#endif //GLIBMM_PROPERTIES_ENABLED

_POP()
popdef(`__PROXY_TYPE__')dnl
')dnl

