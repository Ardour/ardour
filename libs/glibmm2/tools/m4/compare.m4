dnl $Id: compare.m4,v 1.2 2003/12/14 11:53:04 murrayc Exp $

define(`__OPERATOR_DECL',`dnl
/** @relates __NAMESPACE__::__CPPNAME__
 * @param lhs The left-hand side
 * @param rhs The right-hand side
 * @result The result
 */
bool operator`'$1`'(const __CPPNAME__& lhs, const __CPPNAME__& rhs);
')

define(`__OPERATOR_IMPL',`dnl
bool operator`'$1`'(const __CPPNAME__& lhs, const __CPPNAME__& rhs)
{'
ifelse`'(`__UNCONST__',`unconst',`dnl
  return ($2`'(const_cast<__CNAME__*>(lhs.gobj()), const_cast<__CNAME__*>(rhs.gobj())) $3);
',`dnl else
  return ($2`'(lhs.gobj(), rhs.gobj()) $3);
')`dnl endif
}
')


dnl
dnl _WRAP_EQUAL(gdk_region_equal, unconst)
dnl
define(`_WRAP_EQUAL',`dnl
pushdef(`__FUNC_EQUAL__',$1)dnl
pushdef(`__UNCONST__',$2)dnl
_PUSH(SECTION_HEADER3)

__NAMESPACE_BEGIN__

__OPERATOR_DECL(`==')
__OPERATOR_DECL(`!=')

__NAMESPACE_END__

_SECTION(SECTION_CC)

__OPERATOR_IMPL(`==', __FUNC_EQUAL__, `!= 0')
__OPERATOR_IMPL(`!=', __FUNC_EQUAL__, `== 0')

_POP()
popdef(`__UNCONST__')dnl
popdef(`__FUNC_EQUAL__')dnl
')dnl enddef _WRAP_EQUAL


dnl
dnl _WRAP_COMPARE(gtk_tree_path_compare)
dnl
define(`_WRAP_COMPARE',`dnl
pushdef(`__FUNC_COMPARE__',$1)dnl
pushdef(`__UNCONST__',$2)dnl
_PUSH(SECTION_HEADER3)

__NAMESPACE_BEGIN__

__OPERATOR_DECL(`==')
__OPERATOR_DECL(`!=')
__OPERATOR_DECL(`<')
__OPERATOR_DECL(`>')
__OPERATOR_DECL(`<=')
__OPERATOR_DECL(`>=')

__NAMESPACE_END__

_SECTION(SECTION_CC)

__OPERATOR_IMPL(`==', __FUNC_COMPARE__, `== 0')
__OPERATOR_IMPL(`!=', __FUNC_COMPARE__, `!= 0')
__OPERATOR_IMPL(`<', __FUNC_COMPARE__, `< 0')
__OPERATOR_IMPL(`>', __FUNC_COMPARE__, `> 0')
__OPERATOR_IMPL(`<=', __FUNC_COMPARE__, `<= 0')
__OPERATOR_IMPL(`>=', __FUNC_COMPARE__, `>= 0')

_POP()
popdef(`__UNCONST__')dnl
popdef(`__FUNC_COMPARE__')dnl
')dnl enddef _WRAP_COMPARE


dnl
dnl _WRAP_EQUAL_AND_COMPARE(gtk_text_iter_equal, gtk_text_iter_compare)
dnl
define(`_WRAP_EQUAL_AND_COMPARE',`dnl
pushdef(`__FUNC_EQUAL__',$1)dnl
pushdef(`__FUNC_COMPARE__',$2)dnl
pushdef(`__UNCONST__',$3)dnl
_PUSH(SECTION_HEADER3)

__NAMESPACE_BEGIN__

__OPERATOR_DECL(`==')
__OPERATOR_DECL(`!=')
__OPERATOR_DECL(`<')
__OPERATOR_DECL(`>')
__OPERATOR_DECL(`<=')
__OPERATOR_DECL(`>=')

__NAMESPACE_END__

_SECTION(SECTION_CC)

__OPERATOR_IMPL(`==', __FUNC_EQUAL__, `!= 0')
__OPERATOR_IMPL(`!=', __FUNC_EQUAL__, `== 0')
__OPERATOR_IMPL(`<', __FUNC_COMPARE__, `< 0')
__OPERATOR_IMPL(`>', __FUNC_COMPARE__, `> 0')
__OPERATOR_IMPL(`<=', __FUNC_COMPARE__, `<= 0')
__OPERATOR_IMPL(`>=', __FUNC_COMPARE__, `>= 0')

_POP()
popdef(`__UNCONST__')dnl
popdef(`__FUNC_COMPARE__')dnl
popdef(`__FUNC_EQUAL__')dnl
')dnl enddef _WRAP_EQUAL_AND_COMPARE

