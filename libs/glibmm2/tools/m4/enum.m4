
dnl
dnl _ENUM(cpp_type, c_type, value_suffix, `element_list', `flags', `optional_refdoc_comment', 'get_type_function_name')
dnl
m4_define(`_ENUM',`dnl
_PUSH()

m4_define(`__ENUM_CPPNAME__',`$1')
m4_define(`__ENUM_CNAME__',`$2')
m4_define(`__ENUM_VALUE_BASE__',`Glib::Value_$3<__NAMESPACE__::__ENUM_CPPNAME__>')

_POP()
dnl
dnl // Define a new Doxygen group if this is the first enum in the file.
dnl
m4_ifdef(`__DOCGROUP_'__MODULE_CANONICAL__`_ENUMS__',,`dnl else
m4_define(`__DOCGROUP_'__MODULE_CANONICAL__`_ENUMS__')dnl
/** @addtogroup '__MODULE_CANONICAL__`Enums Enums and Flags */

')dnl endif
dnl
dnl
/**$6
 * @ingroup __MODULE_CANONICAL__`'Enums
m4_ifelse($3,Flags,`dnl
 * @par Bitwise operators:
 * <tt>%__ENUM_CPPNAME__ operator|(__ENUM_CPPNAME__, __ENUM_CPPNAME__)</tt><br>
 * <tt>%__ENUM_CPPNAME__ operator&(__ENUM_CPPNAME__, __ENUM_CPPNAME__)</tt><br>
 * <tt>%__ENUM_CPPNAME__ operator^(__ENUM_CPPNAME__, __ENUM_CPPNAME__)</tt><br>
 * <tt>%__ENUM_CPPNAME__ operator~(__ENUM_CPPNAME__)</tt><br>
 * <tt>%__ENUM_CPPNAME__& operator|=(__ENUM_CPPNAME__&, __ENUM_CPPNAME__)</tt><br>
 * <tt>%__ENUM_CPPNAME__& operator&=(__ENUM_CPPNAME__&, __ENUM_CPPNAME__)</tt><br>
 * <tt>%__ENUM_CPPNAME__& operator^=(__ENUM_CPPNAME__&, __ENUM_CPPNAME__)</tt><br>
')dnl endif
 */
enum __ENUM_CPPNAME__
{
$4
};
m4_ifelse($3,Flags,`dnl

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__ operator|(__ENUM_CPPNAME__ lhs, __ENUM_CPPNAME__ rhs)
  { return static_cast<__ENUM_CPPNAME__>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs)); }

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__ operator&(__ENUM_CPPNAME__ lhs, __ENUM_CPPNAME__ rhs)
  { return static_cast<__ENUM_CPPNAME__>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs)); }

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__ operator^(__ENUM_CPPNAME__ lhs, __ENUM_CPPNAME__ rhs)
  { return static_cast<__ENUM_CPPNAME__>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs)); }

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__ operator~(__ENUM_CPPNAME__ flags)
  { return static_cast<__ENUM_CPPNAME__>(~static_cast<unsigned>(flags)); }

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__& operator|=(__ENUM_CPPNAME__& lhs, __ENUM_CPPNAME__ rhs)
  { return (lhs = static_cast<__ENUM_CPPNAME__>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs))); }

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__& operator&=(__ENUM_CPPNAME__& lhs, __ENUM_CPPNAME__ rhs)
  { return (lhs = static_cast<__ENUM_CPPNAME__>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs))); }

/** @ingroup __MODULE_CANONICAL__`'Enums */
inline __ENUM_CPPNAME__& operator^=(__ENUM_CPPNAME__& lhs, __ENUM_CPPNAME__ rhs)
  { return (lhs = static_cast<__ENUM_CPPNAME__>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs))); }
')dnl endif Flags

m4_ifelse($5,`NO_GTYPE',,`dnl else
__NAMESPACE_END__


#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Glib
{

template <>
class Value<__NAMESPACE__::__ENUM_CPPNAME__> : public __ENUM_VALUE_BASE__
{
public:
  static GType value_type() G_GNUC_CONST;
};

} // namespace Glib
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


__NAMESPACE_BEGIN__
_PUSH(SECTION_SRC_GENERATED)
// static
GType Glib::Value<__NAMESPACE__::__ENUM_CPPNAME__>::value_type()
{
  return _GET_TYPE_FUNC(__ENUM_CNAME__);
}

_POP()
')dnl endif !NO_GTYPE
')dnl enddef _ENUM

