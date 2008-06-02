dnl $Id: method.m4 320 2006-09-19 20:07:31Z murrayc $

dnl
dnl
dnl  Code generation sections for making a method.  
dnl
dnl


dnl
dnl method 
dnl $1      $2     $3         $4       $5    $6    $7     $8        $9        $10         $11        $12		$13				$14
dnl  _METHOD(cppname,cname,cpprettype,crettype,arglist,cargs,const,refreturn,errthrow,deprecated,constversion,ifdef, arglist_without_types)
define(`_METHOD',`dnl
_PUSH(SECTION_CC)
ifelse(`$10',,,`_DEPRECATE_IFDEF_START
')dnl
ifelse(`$13',,,`#ifdef $13'
)dnl
ifelse(`$9',,,`#ifdef GLIBMM_EXCEPTIONS_ENABLED'
)dnl
$3 __CPPNAME__::$1`'($5)ifelse(`$7',1,` const')
ifelse(`$9',,,`#else
$3 __CPPNAME__::$1`'(`'$5`'ifelse(($5),(),`',`, ')std::auto_ptr<Glib::Error>& error)ifelse(`$7',1,` const')
#endif //GLIBMM_EXCEPTIONS_ENABLED
')dnl
{
ifelse(`$11',,dnl
`ifelse(`$8'`$9',,dnl If it is not errthrow or refreturn
`ifelse(`$3',void,dnl If it returns voids:
`$2(ifelse(`$7',1,const_cast<__CNAME__*>(gobj()),gobj())`'ifelse(`$6',,,`, ')$6);' dnl It it returns non-void:
,`  return _CONVERT($4,$3,`$2`'(ifelse(`$7',1,const_cast<__CNAME__*>(gobj()),gobj())`'ifelse(`$6',,,`, ')$6)');')'dnl End if it returns voids.
,dnl If is errthrow or refreturn
`ifelse(`$9',,,`  GError* gerror = 0;')
  ifelse(`$3',void,,``$3' retvalue = ')_CONVERT($4,$3,`$2`'(ifelse(`$7',1,const_cast<__CNAME__*>(gobj()),gobj())`'ifelse(`$6',,,`, ')$6)');dnl
ifelse(`$9',,,`
#ifdef GLIBMM_EXCEPTIONS_ENABLED
  if(gerror)
    ::Glib::Error::throw_exception(gerror);
#else
  if(gerror)
    error = ::Glib::Error::throw_exception(gerror);
#endif //GLIBMM_EXCEPTIONS_ENABLED
')
ifelse(`$8',,,`dnl
  if(retvalue)
    retvalue->reference(); //The function does not do a ref for us.
')dnl
ifelse(`$3',void,,`  return retvalue;')
')dnl End errthrow/refreturn
',`  return const_cast<__CPPNAME__*>(this)->$1($12);')
}

ifelse(`$13',,,`
#endif // $13
')dnl
ifelse(`$10',,,`_DEPRECATE_IFDEF_END
')dnl
_POP()')

dnl
dnl static method
dnl                  $1       $2     $3         $4      $5     $6      $7      $8         $9		$10
dnl  _STATIC_METHOD(cppname,cname,cpprettype,crettype,arglist,cargs,refreturn,errthrow,deprecated,ifdef))
define(`_STATIC_METHOD',`dnl
_PUSH(SECTION_CC)
ifelse(`$9',,,`_DEPRECATE_IFDEF_START
')dnl
ifelse(`$10',,,`#ifdef $10'
)dnl
ifelse(`$8',,,`#ifdef GLIBMM_EXCEPTIONS_ENABLED
')dnl
$3 __CPPNAME__::$1($5)
ifelse(`$8',,,`#else
$3 __CPPNAME__::$1(`'$5`'ifelse(($5),(),`',`, ')std::auto_ptr<Glib::Error>& error)
#endif //GLIBMM_EXCEPTIONS_ENABLED
')dnl
{
ifelse(`$7'`$8',,dnl
`ifelse(`$3',void,,`  return ')_CONVERT($4,$3,`$2`'($6)');
',dnl
`ifelse(`$8',,,`  GError* gerror = 0;')
  ifelse(`$3',void,,``$3' retvalue = ')_CONVERT($4,$3,`$2`'($6)');
ifelse(`$8',,,`
#ifdef GLIBMM_EXCEPTIONS_ENABLED
  if(gerror)
    ::Glib::Error::throw_exception(gerror);
#else
  if(gerror)
    error = ::Glib::Error::throw_exception(gerror);
#endif //GLIBMM_EXCEPTIONS_ENABLED
')
ifelse(`$7',,,`dnl
  if(retvalue)
    retvalue->reference(); //The function does not do a ref for us.
')dnl
ifelse(`$3',void,,`  return retvalue;')
')dnl
}

ifelse(`$10',,,`
#endif // $10
')dnl
ifelse(`$9',,,`_DEPRECATE_IFDEF_END
')
_POP()')


