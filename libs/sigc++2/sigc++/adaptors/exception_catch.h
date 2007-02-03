// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_ADAPTORS_MACROS_EXCEPTION_CATCHHM4_
#define _SIGC_ADAPTORS_MACROS_EXCEPTION_CATCHHM4_
#include <sigc++/adaptors/adaptor_trait.h>

namespace sigc {

/*
   functor adaptor:  exception_catch(functor, catcher)

   usage:


   Future directions:
     The catcher should be told what type of return it needs to
   return for multiple type functors,  to do this the user
   will need to derive from catcher_base.
*/
/** @defgroup exception_catch exception_catch()
 * sigc::exception_catch() catches an exception thrown from within 
 * the wrapped functor and directs it to a catcher functor.
 * This catcher can then rethrow the exception and catch it with the proper type.
 *
 * Note that the catcher is expected to return the same type
 * as the wrapped functor so that normal flow can continue.
 *
 * Catchers can be cascaded to catch multiple types because uncaught
 * rethrown exceptions proceed to the next catcher adaptor.
 *
 * @par Examples:
 *   @code
 *   struct my_catch
 *   {
 *     int operator()()
 *     {
 *       try { throw; }
 *       catch (std::range_error e) // catch what types we know
 *         { std::cerr << "caught " << e.what() << std::endl; }
 *       return 1;
 *     }
 *   }
 *   int foo(); // throws std::range_error
 *   sigc::exception_catch(&foo, my_catch())();
 *   @endcode
 *
 * The functor sigc::execption_catch() returns can be passed into
 * sigc::signal::connect() directly.
 *
 * @par Example:
 *   @code
 *   sigc::signal<int> some_signal;
 *   some_signal.connect(sigc::exception_catch(&foo, my_catch));
 *   @endcode
 *
 * @ingroup adaptors
 */

template <class T_functor, class T_catcher, class T_return = typename adapts<T_functor>::result_type>
struct exception_catch_functor : public adapts<T_functor>
{
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type type; };
  typedef T_return result_type;

  result_type
  operator()();

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator()(T_arg1 _A_a1)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_a1);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_a1,_A_a2);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_a1,_A_a2,_A_a3);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7);
        } 
      catch (...)
        { return catcher_(); }
    }

  exception_catch_functor(const T_functor& _A_func,
                          const T_catcher& _A_catcher)
    : adapts<T_functor>(_A_func), catcher_(_A_catcher)
    {}

  T_catcher catcher_; 
};

template <class T_functor, class T_catcher, class T_return>
typename exception_catch_functor<T_functor, T_catcher, T_return>::result_type
exception_catch_functor<T_functor, T_catcher, T_return>::operator()()
  { 
    try
      { return this->functor_(); }
    catch (...)
      { return catcher_(); }
  }

// void specialization
template <class T_functor, class T_catcher>
struct exception_catch_functor<T_functor, T_catcher, void> : public adapts<T_functor>
{
  typedef void result_type;
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  void
  operator()();

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator()(T_arg1 _A_a1)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_a1);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_a1,_A_a2);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_a1,_A_a2,_A_a3);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6);
        } 
      catch (...)
        { return catcher_(); }
    }

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7);
        } 
      catch (...)
        { return catcher_(); }
    }

  exception_catch_functor() {}
  exception_catch_functor(const T_functor& _A_func,
                          const T_catcher& _A_catcher)
    : adapts<T_functor>(_A_func), catcher_(_A_catcher)
    {}
  ~exception_catch_functor() {}

    T_catcher catcher_; 
};

template <class T_functor, class T_catcher>
void exception_catch_functor<T_functor, T_catcher, void>::operator()()
  { 
    try
      { this->functor_(); } // I don't understand why void return doesn't work here (Martin)
    catch (...)
      { this->catcher_(); }
  }

  
//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, class T_catcher, class T_return>
void visit_each(const T_action& _A_action,
                const exception_catch_functor<T_functor, T_catcher, T_return>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);
  visit_each(_A_action, _A_target.catcher_);
}


template <class T_functor, class T_catcher>
inline exception_catch_functor<T_functor, T_catcher>
exception_catch(const T_functor& _A_func, const T_catcher& _A_catcher)
  { return exception_catch_functor<T_functor, T_catcher>(_A_func, _A_catcher); }

} /* namespace sigc */
#endif /* _SIGC_ADAPTORS_MACROS_EXCEPTION_CATCHHM4_ */
