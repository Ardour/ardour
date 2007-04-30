// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_ADAPTORS_MACROS_ADAPTOR_TRAITHM4_
#define _SIGC_ADAPTORS_MACROS_ADAPTOR_TRAITHM4_
#include <sigc++config.h> //To get SIGC_TEMPLATE_KEYWORD_OPERATOR_OVERLOAD
#include <sigc++/visit_each.h>
#include <sigc++/functors/functor_trait.h>
#include <sigc++/functors/ptr_fun.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/adaptors/deduce_result_type.h>

namespace sigc {

// Call either operator()<>() or sun_forte_workaround<>(),
// depending on the compiler:
#ifdef SIGC_GCC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  #define SIGC_WORKAROUND_OPERATOR_PARENTHESES template operator()
  #define SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
#else
  #ifdef SIGC_MSVC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
    #define SIGC_WORKAROUND_OPERATOR_PARENTHESES operator()
    #define SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  #else
    #define SIGC_WORKAROUND_OPERATOR_PARENTHESES sun_forte_workaround
  #endif
#endif


template <class T_functor> struct adapts;

/** @defgroup adaptors Adaptors
 * Adaptors are functors that alter the signature of a functor's
 * operator()().
 *
 * The adaptor types libsigc++ provides
 * are created with bind(), bind_return(), hide(), hide_return(),
 * retype_return(), retype(), compose(), exception_catch() and group().
 *
 * You can easily derive your own adaptor type from sigc::adapts.
 */

/** Converts an arbitrary functor into an adaptor type.
 * All adaptor tyes in libsigc++ are unnumbered and have
 * a <tt>template operator()</tt> member of every argument count
 * they support. These functions in turn invoke a stored adaptor's
 * <tt>template operator()</tt> processing the arguments and return
 * value in a characteristic manner. Explicit function template
 * instantiation is used to pass type hints thus saving copy costs.
 *
 * adaptor_functor is a glue between adaptors and arbitrary functors
 * that just passes on the arguments. You won't use this type directly.
 *
 * The template argument @e T_functor determines the type of stored
 * functor.
 *
 * @ingroup adaptors
 */
template <class T_functor>
struct adaptor_functor : public adaptor_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename sigc::deduce_result_type<T_functor, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type type; };
  typedef typename functor_trait<T_functor>::result_type result_type;

  /** Invokes the wrapped functor passing on the arguments.
   * @return The return value of the functor invocation.
   */
  result_type
  operator()() const;

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  result_type sun_forte_workaround() const
    { return operator(); }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator()(T_arg1 _A_arg1) const
    { return functor_(_A_arg1); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround(T_arg1 _A_arg1) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1);
    }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @param _A_arg2 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator()(T_arg1 _A_arg1,T_arg2 _A_arg2) const
    { return functor_(_A_arg1,_A_arg2); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround(T_arg1 _A_arg1,T_arg2 _A_arg2) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1,_A_arg2);
    }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @param _A_arg2 Argument to be passed on to the functor.
   * @param _A_arg3 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator()(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3) const
    { return functor_(_A_arg1,_A_arg2,_A_arg3); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1,_A_arg2,_A_arg3);
    }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @param _A_arg2 Argument to be passed on to the functor.
   * @param _A_arg3 Argument to be passed on to the functor.
   * @param _A_arg4 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator()(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4) const
    { return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4);
    }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @param _A_arg2 Argument to be passed on to the functor.
   * @param _A_arg3 Argument to be passed on to the functor.
   * @param _A_arg4 Argument to be passed on to the functor.
   * @param _A_arg5 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator()(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4,T_arg5 _A_arg5) const
    { return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4,_A_arg5); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4,T_arg5 _A_arg5) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4,_A_arg5);
    }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @param _A_arg2 Argument to be passed on to the functor.
   * @param _A_arg3 Argument to be passed on to the functor.
   * @param _A_arg4 Argument to be passed on to the functor.
   * @param _A_arg5 Argument to be passed on to the functor.
   * @param _A_arg6 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator()(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4,T_arg5 _A_arg5,T_arg6 _A_arg6) const
    { return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4,_A_arg5,_A_arg6); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4,T_arg5 _A_arg5,T_arg6 _A_arg6) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4,_A_arg5,_A_arg6);
    }
  #endif
  
  /** Invokes the wrapped functor passing on the arguments.
   * @param _A_arg1 Argument to be passed on to the functor.
   * @param _A_arg2 Argument to be passed on to the functor.
   * @param _A_arg3 Argument to be passed on to the functor.
   * @param _A_arg4 Argument to be passed on to the functor.
   * @param _A_arg5 Argument to be passed on to the functor.
   * @param _A_arg6 Argument to be passed on to the functor.
   * @param _A_arg7 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator()(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4,T_arg5 _A_arg5,T_arg6 _A_arg6,T_arg7 _A_arg7) const
    { return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4,_A_arg5,_A_arg6,_A_arg7); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround(T_arg1 _A_arg1,T_arg2 _A_arg2,T_arg3 _A_arg3,T_arg4 _A_arg4,T_arg5 _A_arg5,T_arg6 _A_arg6,T_arg7 _A_arg7) const
    { //Just calling operator() tries to copy the argument:
      return functor_(_A_arg1,_A_arg2,_A_arg3,_A_arg4,_A_arg5,_A_arg6,_A_arg7);
    }
  #endif
  
  /// Constructs an invalid functor.
  adaptor_functor()
    {}

  /** Constructs an adaptor_functor object that wraps the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit adaptor_functor(const T_functor& _A_functor)
    : functor_(_A_functor)
    {}

  /** Constructs an adaptor_functor object that wraps the passed (member)
   * function pointer.
   * @param _A_type Pointer to function or class method to invoke from operator()().
   */
  template <class T_type>
  explicit adaptor_functor(const T_type& _A_type)
    : functor_(_A_type)
    {}

  /// Functor that is invoked from operator()().
  mutable T_functor functor_;
};

template <class T_functor>
typename adaptor_functor<T_functor>::result_type
adaptor_functor<T_functor>::operator()() const
  { return functor_(); }


//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::adaptor_functor performs a functor
 * on the functor stored in the sigc::adaptor_functor object.
 *
 * @ingroup adaptors
 */
template <class T_action, class T_functor>
void visit_each(const T_action& _A_action,
                const adaptor_functor<T_functor>& _A_target)
{
  //The extra sigc:: prefix avoids ambiguity in some strange
  //situations.
  sigc::visit_each(_A_action, _A_target.functor_);
}


/** Trait that specifies what is the adaptor version of a functor type.
 * Template specializations for sigc::adaptor_base derived functors,
 * for function pointers and for class methods are provided.
 *
 * The template argument @e T_functor is the functor type to convert.
 * @e I_isadaptor indicates whether @e T_functor inherits from sigc::adaptor_base.
 *
 * @ingroup adaptors
 */
template <class T_functor, bool I_isadaptor = is_base_and_derived<adaptor_base, T_functor>::value> struct adaptor_trait;

/** Trait that specifies what is the adaptor version of a functor type.
 * This template specialization is used for types that inherit from adaptor_base.
 * adaptor_type is equal to @p T_functor in this case.
 */
template <class T_functor> 
struct adaptor_trait<T_functor, true>
{
  typedef typename T_functor::result_type result_type;
  typedef T_functor functor_type;
  typedef T_functor adaptor_type;
};

/** Trait that specifies what is the adaptor version of a functor type.
 * This template specialization is used for arbitrary functors,
 * for function pointers and for class methods are provided.
 * The latter are converted into @p pointer_functor or @p mem_functor types.
 * adaptor_type is equal to @p adaptor_functor<functor_type>.
 */
template <class T_functor>
struct adaptor_trait<T_functor, false>
{
  typedef typename functor_trait<T_functor>::result_type result_type;
  typedef typename functor_trait<T_functor>::functor_type functor_type;
  typedef adaptor_functor<functor_type> adaptor_type;
};


/** Base type for adaptors.
 * adapts wraps adaptors, functors, function pointers and class methods.
 * It contains a single member functor which is always a sigc::adaptor_base.
 * The typedef adaptor_type defines the exact type that is used
 * to store the adaptor, functor, function pointer or class method passed
 * into the constructor. It differs from @e T_functor unless @e T_functor
 * inherits from sigc::adaptor_base.
 *
 * @par Example of a simple adaptor:
 *   @code
 *   template <T_functor>
 *   struct my_adpator : public sigc::adapts<T_functor>
 *   {
 *     template <class T_arg1=void, class T_arg2=void>
 *     struct deduce_result_type
 *     { typedef typename sigc::deduce_result_type<T_functor, T_arg1, T_arg2>::type type; };
 *     typedef typename sigc::functor_trait<T_functor>::result_type result_type;
 *
 *     result_type
 *     operator()() const;
 *
 *     template <class T_arg1>
 *     typename deduce_result_type<T_arg1>::type
 *     operator()(T_arg1 _A_arg1) const;
 *
 *     template <class T_arg1, class T_arg2>
 *     typename deduce_result_type<T_arg1, T_arg2>::type
 *     operator()(T_arg1 _A_arg1, class T_arg2) const;
 *
 *     explicit adaptor_functor(const T_functor& _A_functor) // Constructs a my_functor object that wraps the passed functor.
 *       : sigc::adapts<T_functor>(_A_functor) {}
 *
 *     mutable T_functor functor_; // Functor that is invoked from operator()().
 *   };
 *   @endcode
 *
 * @ingroup adaptors
 */
template <class T_functor>
struct adapts : public adaptor_base
{
  typedef typename adaptor_trait<T_functor>::result_type  result_type;
  typedef typename adaptor_trait<T_functor>::adaptor_type adaptor_type;

  /** Constructs an adaptor that wraps the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit adapts(const T_functor& _A_functor)
    : functor_(_A_functor)
    {}

  /// Adaptor that is invoked from operator()().
  mutable adaptor_type functor_;
};

} /* namespace sigc */
#endif /* _SIGC_ADAPTORS_MACROS_ADAPTOR_TRAITHM4_ */
