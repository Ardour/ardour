
#
# --------------------------- Signal Decl----------------------------
#

dnl _SIGNAL_PROXY($1 = c_signal_name,
dnl               $2 = c_return_type,
dnl               $3 = `<c_arg_types_and_names>',
dnl               $4 = cpp_signal_name,
dnl               $5 = cpp_return_type,
dnl               $6 = `<cpp_arg_types>',
dnl               $7 = `<c_args_to_cpp>',
dnl               $8 = `custom_c_callback (boolean)',
dnl               $9 = `refdoc_comment',
dnl				  $10 = ifdef)

define(`_SIGNAL_PROXY',`
$9

ifelse(`$10',,,`#ifdef $10'
)dnl
  Glib::SignalProxy`'_NUM($6)< $5`'_COMMA_PREFIX($6) > signal_$4`'();
ifelse(`$10',,,`#endif // $10
')dnl
dnl
_PUSH(SECTION_ANONYMOUS_NAMESPACE)

ifelse(`$10',,,`#ifdef $10'
)dnl
dnl
ifelse($2`'_NUM($3)`'$5`'_NUM($6),`void0void0',`dnl
dnl
dnl Use predefined callback for SignalProxy0<void>, to reduce code size.

static const Glib::SignalProxyInfo __CPPNAME__`'_signal_$4_info =
{
  "$1",
  (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
  (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};
',`dnl else

ifelse($8,`1',,`dnl Do not generate the implementation if it should be custom:
static $2 __CPPNAME__`'_signal_$4_callback`'(__CNAME__`'* self, _COMMA_SUFFIX($3)`'void* data)
{
  using namespace __NAMESPACE__;
  typedef sigc::slot< $5`'_COMMA_PREFIX($6) > SlotType;

  // Do not try to call a signal on a disassociated wrapper.
  if(Glib::ObjectBase::_get_current_wrapper((GObject*) self))
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    try
    {
    #endif //GLIBMM_EXCEPTIONS_ENABLED
      if(sigc::slot_base *const slot = Glib::SignalProxyNormal::data_to_slot`'(data))
ifelse(`$2',void,`dnl
        (*static_cast<SlotType*>(slot))($7);
',`dnl else
        return _CONVERT($5,$2,`(*static_cast<SlotType*>(slot))($7)');
')dnl endif
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    }
    catch(...)
    {
      Glib::exception_handlers_invoke();
    }
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }
ifelse($2,void,,`dnl else

  typedef $2 RType;
  return RType`'();
')dnl
}
ifelse($2,void,,`dnl else

static $2 __CPPNAME__`'_signal_$4_notify_callback`'(__CNAME__`'* self, _COMMA_SUFFIX($3)`' void* data)
{
  using namespace __NAMESPACE__;
  typedef sigc::slot< void`'_COMMA_PREFIX($6) > SlotType;

  // Do not try to call a signal on a disassociated wrapper.
  if(Glib::ObjectBase::_get_current_wrapper((GObject*) self))
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    try
    {
    #endif //GLIBMM_EXCEPTIONS_ENABLED
      if(sigc::slot_base *const slot = Glib::SignalProxyNormal::data_to_slot`'(data))
        (*static_cast<SlotType*>(slot))($7);
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    }
    catch(...)
    {
      Glib::exception_handlers_invoke();
    }
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }

  typedef $2 RType;
  return RType`'();
}
')dnl endif
')dnl endif

static const Glib::SignalProxyInfo __CPPNAME__`'_signal_$4_info =
{
  "$1",
  (GCallback) &__CPPNAME__`'_signal_$4_callback,
  (GCallback) &__CPPNAME__`'_signal_$4_`'ifelse($2,void,,notify_)`'callback
};
')dnl endif

ifelse(`$10',,,`#endif // $10
')dnl

_SECTION(SECTION_CC_SIGNALPROXIES)

ifelse(`$10',,,`#ifdef $10'
)dnl
Glib::SignalProxy`'_NUM($6)< $5`'_COMMA_PREFIX($6) > __CPPNAME__::signal_$4`'()
{
  return Glib::SignalProxy`'_NUM($6)< $5`'_COMMA_PREFIX($6) >(this, &__CPPNAME__`'_signal_$4_info);
}
ifelse(`$10',,,`#endif // $10
')dnl

_POP()')


dnl
dnl _SIGNAL_PH(gname, crettype, cargs and names)
dnl Create a callback and set it in our derived G*Class.
dnl
define(`_SIGNAL_PH',`dnl
_PUSH(SECTION_PCC_CLASS_INIT_DEFAULT_SIGNAL_HANDLERS)
ifelse(`$4',,,`#ifdef $4'
)dnl
  klass->$1 = `&'$1_callback;
ifelse(`$4',,,`#endif // $4
')dnl
_SECTION(SECTION_PH_DEFAULT_SIGNAL_HANDLERS)
ifelse(`$4',,,`#ifdef $4'
)dnl
  static $2 $1_callback`'($3);
ifelse(`$4',,,`#endif // $4
')dnl
_POP()')



dnl                $1      $2       $3        $4
dnl _SIGNAL_PCC(cppname,gname,cpprettype,crettype,
dnl                        $5                $6          $7            $8			$9
dnl                  `<cargs and names>',`<cnames>',`<cpparg names>', firstarg, <ifndef>)
dnl
define(`_SIGNAL_PCC',`dnl
_PUSH(SECTION_PCC_DEFAULT_SIGNAL_HANDLERS)
ifelse(`$9',,,`#ifdef $9'
)dnl
$4 __CPPNAME__`'_Class::$2_callback`'($5)
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
        obj->on_$1`'($7);
        return;
',`dnl
        return _CONVERT($3,$4,`obj->on_$1`'($7)');
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
')    );
dnl    g_assert(base != 0);

  // Call the original underlying C function:
  if(base && base->$2)
    ifelse($4,void,,`return ')(*base->$2)`'($6);
ifelse($4,void,,`dnl

  typedef $4 RType;
  return RType`'();
')dnl
}
ifelse(`$9',,,`#endif // $9
')dnl
_POP()')


dnl               $1      $2       $3 		$4
dnl _SIGNAL_H(signame, rettype, `<cppargs>', <ifdef>)
dnl
define(`_SIGNAL_H',`dnl
_PUSH(SECTION_H_DEFAULT_SIGNAL_HANDLERS)
ifelse(`$4',,,`#ifdef $4'
)dnl
  virtual $2 on_$1`'($3);
ifelse(`$4',,,`#endif // $4
')dnl
_POP()')

dnl              $1      $2     $3     $4         $5          $6            $7      $8			$9
dnl _SIGNAL_CC(signame,gname,rettype,crettype,`<cppargs>',`<carg_names>', const, refreturn, <ifdef>)
dnl
define(`_SIGNAL_CC',`dnl
_PUSH(SECTION_CC_DEFAULT_SIGNAL_HANDLERS)
ifelse(`$9',,,`#ifdef $9'
)dnl
$3 __NAMESPACE__::__CPPNAME__::on_$1`'($5)
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
    (*base->$2)`'(gobj`'()`'_COMMA_PREFIX($6));
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

