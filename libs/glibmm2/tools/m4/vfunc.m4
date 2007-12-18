dnl
dnl _VFUNC_PH(gtkname, crettype, cargs and names)
dnl Create a callback and set it in our derived G*Class.
dnl
define(`_VFUNC_PH',`dnl
_PUSH(SECTION_PCC_CLASS_INIT_VFUNCS)
ifelse(`$4',,,`#ifdef $4'
)dnl
  klass->$1 = `&'$1_vfunc_callback;
ifelse(`$4',,,`#endif // $4
')dnl
_SECTION(SECTION_PH_VFUNCS)
ifelse(`$4',,,`#ifdef $4'
)dnl
  static $2 $1_vfunc_callback`'($3);
ifelse(`$4',,,`#endif // $4
')dnl
_POP()')


dnl               $1      $2       $3        $4
dnl _VFUNC_PCC(cppname,gtkname,cpprettype,crettype,
dnl                        $5                $6          $7            $8        $9					$10
dnl                  `<cargs and names>',`<cnames>',`<cpparg names>',firstarg, refreturn_ctype, ifdef)
dnl
dnl Note: _get_current_wrapper_inline() could be used throughout for performance instead of _get_current_wrapper(),
dnl and is_derived_() instead of is_derived_(),
dnl but it is not yet clear whether that would be a worthwhile performance optimization.
define(`_VFUNC_PCC',`dnl
_PUSH(SECTION_PCC_VFUNCS)
ifelse(`$10',,,`#ifdef $10'
)dnl
$4 __CPPNAME__`'_Class::$2_vfunc_callback`'($5)
{
dnl  First, do a simple cast to ObjectBase. We will have to do a dynamic_cast
dnl  eventually, but it is not necessary to check whether we need to call
dnl  the vfunc.
  Glib::ObjectBase *const obj_base = static_cast<Glib::ObjectBase*>(
      Glib::ObjectBase::_get_current_wrapper`'((GObject*)$8));

_IMPORT(SECTION_CHECK)
  // Non-gtkmmproc-generated custom classes implicitly call the default
  // Glib::ObjectBase constructor, which sets is_derived_. But gtkmmproc-
  // generated classes can use this optimisation, which avoids the unnecessary
  // parameter conversions if there is no possibility of the virtual function
  // being overridden:
  if(obj_base && obj_base->is_derived_())
  {
dnl  We need to do a dynamic cast to get the real object type, to call the
dnl  C++ vfunc on it.
    CppObjectType *const obj = dynamic_cast<CppObjectType* const>(obj_base);
    if(obj) // This can be NULL during destruction.
    {
      #ifdef GLIBMM_EXCEPTIONS_ENABLED
      try // Trap C++ exceptions which would normally be lost because this is a C callback.
      {
      #endif //GLIBMM_EXCEPTIONS_ENABLED
        // Call the virtual member method, which derived classes might override.
ifelse($4,void,`dnl
        obj->$1`'($7);
        return;
',`dnl
ifelse($9,refreturn_ctype,`dnl Assume Glib::unwrap_copy() is correct if refreturn_ctype is requested.
        return Glib::unwrap_copy`'(`obj->$1'($7));
',`dnl
        return _CONVERT($3,$4,`obj->$1`'($7)');
')dnl
')dnl
      #ifdef GLIBMM_EXCEPTIONS_ENABLED
      }
      catch(...)
      {
        Glib::exception_handlers_invoke`'();
      }
      #endif //GLIBMM_EXCEPTIONS_ENABLED
    }
  }
  
  BaseClassType *const base = static_cast<BaseClassType*>(
ifdef(`__BOOL_IS_INTERFACE__',`dnl
      _IFACE_PARENT_FROM_OBJECT($8)dnl
',`dnl
      _PARENT_GCLASS_FROM_OBJECT($8)dnl
')  );
dnl  g_assert(base != 0);

  // Call the original underlying C function:
  if(base && base->$2)
    ifelse($4,void,,`return ')(*base->$2)`'($6);

ifelse($4,void,,`dnl

  typedef $4 RType;
  return RType`'();
')dnl
}
ifelse(`$10',,,`#endif // $10
')dnl
_POP()')

#                $1        $2        $3           $4          $5            $6         $7          $8			$9
# _VFUNC_CC(vfunc_name, gtkname, cpp_rettype, c_rettype, `<cppargs>', `<carg_names>', is_const, refreturn, $ifdef)
#
define(`_VFUNC_CC',`dnl
_PUSH(SECTION_CC_VFUNCS)
ifelse(`$9',,,`#ifdef $9'
)dnl
$3 __NAMESPACE__::__CPPNAME__::$1`'($5) ifelse($7,1,const,)
{
  BaseClassType *const base = static_cast<BaseClassType*>(
ifdef(`__BOOL_IS_INTERFACE__',`dnl
      _IFACE_PARENT_FROM_OBJECT(gobject_)dnl
',`dnl
      _PARENT_GCLASS_FROM_OBJECT(gobject_)dnl
')  );
dnl  g_assert(base != 0);

  if(base && base->$2)
ifelse($3,void,`dnl
    (*base->$2)`'(ifelse(`$7',1,const_cast<__CNAME__*>(gobj()),gobj())`'_COMMA_PREFIX($6));
',`dnl
ifelse($8,refreturn,`dnl Assume Glib::wrap() is correct if refreturn is requested.
    return Glib::wrap((*base->$2)`'(ifelse(`$7',1,const_cast<__CNAME__*>(gobj()),gobj())`'_COMMA_PREFIX($6)), true);
',`dnl
    return _CONVERT($4,$3,`(*base->$2)`'(ifelse(`$7',1,const_cast<__CNAME__*>(gobj()),gobj())`'_COMMA_PREFIX($6))');
')dnl

  typedef $3 RType;
  return RType`'();
')dnl
}
ifelse(`$9',,,`#endif // $9
')dnl
_POP()')


#                $1      $2       $3           $4		$5
# _VFUNC_H(vfunc_name, rettype, `<cppargs>', is_const, ifndef)
# Only used for custom vfuncs.
#
define(`_VFUNC_H',`dnl
_PUSH(SECTION_H_VFUNCS)
ifelse(`$5',,,`#ifdef $5'
)dnl
ifelse($4,`1',`dnl
virtual $2 $1`'($3) const;
',`dnl
virtual $2 $1`'($3);
')
ifelse(`$5',,,`#endif // $5
')dnl
_POP()')


