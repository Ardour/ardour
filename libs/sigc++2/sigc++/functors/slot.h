// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_FUNCTORS_MACROS_SLOTHM4_
#define _SIGC_FUNCTORS_MACROS_SLOTHM4_
#include <sigc++/trackable.h>
#include <sigc++/visit_each.h>
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/functors/slot_base.h>

namespace sigc {

namespace internal {

/** A typed slot_rep.
 * A typed slot_rep holds a functor that can be invoked from
 * slot::operator()(). visit_each() is used to visit the functor's
 * targets that inherit trackable recursively and register the
 * notification callback. Consequently the slot_rep object will be
 * notified when some referred object is destroyed or overwritten.
 */
template <class T_functor>
struct typed_slot_rep : public slot_rep
{
  typedef typed_slot_rep<T_functor> self;

  /* Use an adaptor type so that arguments can be passed as const references
   * through explicit template instantiation from slot_call#::call_it() */
  typedef typename adaptor_trait<T_functor>::adaptor_type adaptor_type;

  /** The functor contained by this slot_rep object. */
  adaptor_type functor_;

  /** Constructs an invalid typed slot_rep object.
   * The notification callback is registered using visit_each().
   * @param functor The functor contained by the new slot_rep object.
   */
  inline typed_slot_rep(const T_functor& functor)
    : slot_rep(0, &destroy, &dup), functor_(functor)
    { visit_each_type<trackable*>(slot_do_bind(this), functor_); }

  inline typed_slot_rep(const typed_slot_rep& cl)
    : slot_rep(cl.call_, &destroy, &dup), functor_(cl.functor_)
    { visit_each_type<trackable*>(slot_do_bind(this), functor_); }

  inline ~typed_slot_rep()
    {
      call_ = 0;
      destroy_ = 0;
      visit_each_type<trackable*>(slot_do_unbind(this), functor_);
    }

  /** Detaches the stored functor from the other referred trackables and destroys it.
   * This does not destroy the base slot_rep object.
   */
  static void* destroy(void* data)
    {
      self* self_ = static_cast<self*>(reinterpret_cast<slot_rep*>(data));
      self_->call_ = 0;
      self_->destroy_ = 0;
      visit_each_type<trackable*>(slot_do_unbind(self_), self_->functor_);
      self_->functor_.~adaptor_type();
      /* don't call disconnect() here: destroy() is either called
       * a) from the parent itself (in which case disconnect() leads to a segfault) or
       * b) from a parentless slot (in which case disconnect() does nothing)
       */
      return 0;
    }

  /** Makes a deep copy of the slot_rep object.
   * Deep copy means that the notification callback of the new
   * slot_rep object is registered in the referred trackables.
   * @return A deep copy of the slot_rep object.
   */
  static void* dup(void* data)
    {
      slot_rep* a_rep = reinterpret_cast<slot_rep*>(data);
      return static_cast<slot_rep*>(new self(*static_cast<self*>(a_rep)));
    }
};


/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 *
 */
template<class T_functor, class T_return>
struct slot_call0
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_)();
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1>
struct slot_call1
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take>
               (a_1);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 * - @e T_arg2 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1,class T_arg2>
struct slot_call2
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1,typename type_trait<T_arg2>::take a_2)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take,typename type_trait<T_arg2>::take>
               (a_1,a_2);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 * - @e T_arg2 Argument type used in the definition of call_it().
 * - @e T_arg3 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1,class T_arg2,class T_arg3>
struct slot_call3
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1,typename type_trait<T_arg2>::take a_2,typename type_trait<T_arg3>::take a_3)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take,typename type_trait<T_arg2>::take,typename type_trait<T_arg3>::take>
               (a_1,a_2,a_3);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 * - @e T_arg2 Argument type used in the definition of call_it().
 * - @e T_arg3 Argument type used in the definition of call_it().
 * - @e T_arg4 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
struct slot_call4
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1,typename type_trait<T_arg2>::take a_2,typename type_trait<T_arg3>::take a_3,typename type_trait<T_arg4>::take a_4)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take,typename type_trait<T_arg2>::take,typename type_trait<T_arg3>::take,typename type_trait<T_arg4>::take>
               (a_1,a_2,a_3,a_4);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 * - @e T_arg2 Argument type used in the definition of call_it().
 * - @e T_arg3 Argument type used in the definition of call_it().
 * - @e T_arg4 Argument type used in the definition of call_it().
 * - @e T_arg5 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
struct slot_call5
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @param _A_a5 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1,typename type_trait<T_arg2>::take a_2,typename type_trait<T_arg3>::take a_3,typename type_trait<T_arg4>::take a_4,typename type_trait<T_arg5>::take a_5)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take,typename type_trait<T_arg2>::take,typename type_trait<T_arg3>::take,typename type_trait<T_arg4>::take,typename type_trait<T_arg5>::take>
               (a_1,a_2,a_3,a_4,a_5);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 * - @e T_arg2 Argument type used in the definition of call_it().
 * - @e T_arg3 Argument type used in the definition of call_it().
 * - @e T_arg4 Argument type used in the definition of call_it().
 * - @e T_arg5 Argument type used in the definition of call_it().
 * - @e T_arg6 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
struct slot_call6
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @param _A_a5 Argument to be passed on to the functor.
   * @param _A_a6 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1,typename type_trait<T_arg2>::take a_2,typename type_trait<T_arg3>::take a_3,typename type_trait<T_arg4>::take a_4,typename type_trait<T_arg5>::take a_5,typename type_trait<T_arg6>::take a_6)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take,typename type_trait<T_arg2>::take,typename type_trait<T_arg3>::take,typename type_trait<T_arg4>::take,typename type_trait<T_arg5>::take,typename type_trait<T_arg6>::take>
               (a_1,a_2,a_3,a_4,a_5,a_6);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

/** Abstracts functor execution.
 * call_it() invokes a functor of type @e T_functor with a list of
 * parameters whose types are given by the template arguments.
 * address() forms a function pointer from call_it().
 *
 * The following template arguments are used:
 * - @e T_functor The functor type.
 * - @e T_return The return type of call_it().
 * - @e T_arg1 Argument type used in the definition of call_it().
 * - @e T_arg2 Argument type used in the definition of call_it().
 * - @e T_arg3 Argument type used in the definition of call_it().
 * - @e T_arg4 Argument type used in the definition of call_it().
 * - @e T_arg5 Argument type used in the definition of call_it().
 * - @e T_arg6 Argument type used in the definition of call_it().
 * - @e T_arg7 Argument type used in the definition of call_it().
 *
 */
template<class T_functor, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
struct slot_call7
{
  /** Invokes a functor of type @p T_functor.
   * @param rep slot_rep object that holds a functor of type @p T_functor.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @param _A_a5 Argument to be passed on to the functor.
   * @param _A_a6 Argument to be passed on to the functor.
   * @param _A_a7 Argument to be passed on to the functor.
   * @return The return values of the functor invocation.
   */
  static T_return call_it(slot_rep* rep, typename type_trait<T_arg1>::take a_1,typename type_trait<T_arg2>::take a_2,typename type_trait<T_arg3>::take a_3,typename type_trait<T_arg4>::take a_4,typename type_trait<T_arg5>::take a_5,typename type_trait<T_arg6>::take a_6,typename type_trait<T_arg7>::take a_7)
    {
      typedef typed_slot_rep<T_functor> typed_slot;
      typed_slot *typed_rep = static_cast<typed_slot*>(rep);
      return (typed_rep->functor_).SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::take,typename type_trait<T_arg2>::take,typename type_trait<T_arg3>::take,typename type_trait<T_arg4>::take,typename type_trait<T_arg5>::take,typename type_trait<T_arg6>::take,typename type_trait<T_arg7>::take>
               (a_1,a_2,a_3,a_4,a_5,a_6,a_7);
    }

  /** Forms a function pointer from call_it().
   * @return A function pointer formed from call_it().
   */
  static hook address() 
    { return reinterpret_cast<hook>(&call_it); }
};

} /* namespace internal */


/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return>
class slot0
  : public slot_base
{
public:
  typedef T_return result_type;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()() const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_);
      return T_return();
    }

  inline slot0() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot0(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call0<T_functor, T_return>::address();
    }

  slot0(const slot0& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot0& operator=(const slot0& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1>
class slot1
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1);
      return T_return();
    }

  inline slot1() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot1(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call1<T_functor, T_return, T_arg1>::address();
    }

  slot1(const slot1& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot1& operator=(const slot1& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1,class T_arg2>
class slot2
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;
  typedef typename type_trait<T_arg2>::take arg2_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_,arg2_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1,arg2_type_ _A_a2) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1,_A_a2);
      return T_return();
    }

  inline slot2() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot2(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call2<T_functor, T_return, T_arg1,T_arg2>::address();
    }

  slot2(const slot2& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot2& operator=(const slot2& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg3 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3>
class slot3
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;
  typedef typename type_trait<T_arg2>::take arg2_type_;
  typedef typename type_trait<T_arg3>::take arg3_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_,arg2_type_,arg3_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1,arg2_type_ _A_a2,arg3_type_ _A_a3) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1,_A_a2,_A_a3);
      return T_return();
    }

  inline slot3() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot3(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call3<T_functor, T_return, T_arg1,T_arg2,T_arg3>::address();
    }

  slot3(const slot3& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot3& operator=(const slot3& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg3 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg4 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class slot4
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;
  typedef typename type_trait<T_arg2>::take arg2_type_;
  typedef typename type_trait<T_arg3>::take arg3_type_;
  typedef typename type_trait<T_arg4>::take arg4_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_,arg2_type_,arg3_type_,arg4_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1,arg2_type_ _A_a2,arg3_type_ _A_a3,arg4_type_ _A_a4) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1,_A_a2,_A_a3,_A_a4);
      return T_return();
    }

  inline slot4() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot4(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call4<T_functor, T_return, T_arg1,T_arg2,T_arg3,T_arg4>::address();
    }

  slot4(const slot4& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot4& operator=(const slot4& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg3 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg4 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg5 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class slot5
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;
  typedef typename type_trait<T_arg2>::take arg2_type_;
  typedef typename type_trait<T_arg3>::take arg3_type_;
  typedef typename type_trait<T_arg4>::take arg4_type_;
  typedef typename type_trait<T_arg5>::take arg5_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_,arg2_type_,arg3_type_,arg4_type_,arg5_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @param _A_a5 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1,arg2_type_ _A_a2,arg3_type_ _A_a3,arg4_type_ _A_a4,arg5_type_ _A_a5) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1,_A_a2,_A_a3,_A_a4,_A_a5);
      return T_return();
    }

  inline slot5() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot5(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call5<T_functor, T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::address();
    }

  slot5(const slot5& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot5& operator=(const slot5& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg3 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg4 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg5 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg6 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class slot6
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;
  typedef typename type_trait<T_arg2>::take arg2_type_;
  typedef typename type_trait<T_arg3>::take arg3_type_;
  typedef typename type_trait<T_arg4>::take arg4_type_;
  typedef typename type_trait<T_arg5>::take arg5_type_;
  typedef typename type_trait<T_arg6>::take arg6_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_,arg2_type_,arg3_type_,arg4_type_,arg5_type_,arg6_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @param _A_a5 Argument to be passed on to the functor.
   * @param _A_a6 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1,arg2_type_ _A_a2,arg3_type_ _A_a3,arg4_type_ _A_a4,arg5_type_ _A_a5,arg6_type_ _A_a6) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6);
      return T_return();
    }

  inline slot6() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot6(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call6<T_functor, T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::address();
    }

  slot6(const slot6& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot6& operator=(const slot6& src)
    { slot_base::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg3 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg4 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg5 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg6 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg7 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * You should use the more convenient unnumbered sigc::slot template.
 *
 * @ingroup slot
 */
/* TODO: Where put the following bit of information? I can't make any
 *       sense of the "because", by the way!
 *
 * Because slot is opaque, visit_each() will not visit its internal members.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class slot7
  : public slot_base
{
public:
  typedef T_return result_type;
  typedef typename type_trait<T_arg1>::take arg1_type_;
  typedef typename type_trait<T_arg2>::take arg2_type_;
  typedef typename type_trait<T_arg3>::take arg3_type_;
  typedef typename type_trait<T_arg4>::take arg4_type_;
  typedef typename type_trait<T_arg5>::take arg5_type_;
  typedef typename type_trait<T_arg6>::take arg6_type_;
  typedef typename type_trait<T_arg7>::take arg7_type_;


#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
  typedef internal::slot_rep rep_type;
public:
  typedef T_return (*call_type)(rep_type*, arg1_type_,arg2_type_,arg3_type_,arg4_type_,arg5_type_,arg6_type_,arg7_type_);
#endif

  /** Invoke the contained functor unless slot is in blocking state.
   * @param _A_a1 Argument to be passed on to the functor.
   * @param _A_a2 Argument to be passed on to the functor.
   * @param _A_a3 Argument to be passed on to the functor.
   * @param _A_a4 Argument to be passed on to the functor.
   * @param _A_a5 Argument to be passed on to the functor.
   * @param _A_a6 Argument to be passed on to the functor.
   * @param _A_a7 Argument to be passed on to the functor.
   * @return The return value of the functor invocation.
   */
  inline T_return operator()(arg1_type_ _A_a1,arg2_type_ _A_a2,arg3_type_ _A_a3,arg4_type_ _A_a4,arg5_type_ _A_a5,arg6_type_ _A_a6,arg7_type_ _A_a7) const
    {
      if (!empty() && !blocked())
        return (reinterpret_cast<call_type>(slot_base::rep_->call_))(slot_base::rep_, _A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7);
      return T_return();
    }

  inline slot7() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot7(const T_functor& _A_func)
    : slot_base(new internal::typed_slot_rep<T_functor>(_A_func))
    {
      //The slot_base:: is necessary to stop the HP-UX aCC compiler from being confused. murrayc.
      slot_base::rep_->call_ = internal::slot_call7<T_functor, T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::address();
    }

  slot7(const slot7& src)
    : slot_base(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  slot7& operator=(const slot7& src)
    { slot_base::operator=(src); return *this; }
};


/** Convenience wrapper for the numbered sigc::slot# templates.
 * Slots convert arbitrary functors to unified types which are opaque.
 * sigc::slot itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg2 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg3 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg4 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg5 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg6 Argument type used in the definition of operator()(). The default @p nil means no argument.
 * - @e T_arg7 Argument type used in the definition of operator()(). The default @p nil means no argument.
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   void foo(int) {}
 *   sigc::slot<void, long> s = sigc::ptr_fun(&foo);
 *   s(19);
 *   @endcode
 *
 * @ingroup slot
 */
template <class T_return, class T_arg1 = nil,class T_arg2 = nil,class T_arg3 = nil,class T_arg4 = nil,class T_arg5 = nil,class T_arg6 = nil,class T_arg7 = nil>
class slot 
  : public slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
{
public:
  typedef slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};



/** Convenience wrapper for the numbered sigc::slot0 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 0 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return>
class slot <T_return, nil, nil, nil, nil, nil, nil, nil>
  : public slot0<T_return>
{
public:
  typedef slot0<T_return> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};


/** Convenience wrapper for the numbered sigc::slot1 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 1 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return, class T_arg1>
class slot <T_return, T_arg1, nil, nil, nil, nil, nil, nil>
  : public slot1<T_return, T_arg1>
{
public:
  typedef slot1<T_return, T_arg1> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};


/** Convenience wrapper for the numbered sigc::slot2 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 2 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return, class T_arg1,class T_arg2>
class slot <T_return, T_arg1, T_arg2, nil, nil, nil, nil, nil>
  : public slot2<T_return, T_arg1,T_arg2>
{
public:
  typedef slot2<T_return, T_arg1,T_arg2> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};


/** Convenience wrapper for the numbered sigc::slot3 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 3 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3>
class slot <T_return, T_arg1, T_arg2, T_arg3, nil, nil, nil, nil>
  : public slot3<T_return, T_arg1,T_arg2,T_arg3>
{
public:
  typedef slot3<T_return, T_arg1,T_arg2,T_arg3> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};


/** Convenience wrapper for the numbered sigc::slot4 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 4 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class slot <T_return, T_arg1, T_arg2, T_arg3, T_arg4, nil, nil, nil>
  : public slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
{
public:
  typedef slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};


/** Convenience wrapper for the numbered sigc::slot5 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 5 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class slot <T_return, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, nil, nil>
  : public slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
{
public:
  typedef slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};


/** Convenience wrapper for the numbered sigc::slot6 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::slot
 * template for 6 argument(s), specialized for different numbers of arguments
 * This is possible because the template has default (nil) template types.
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class slot <T_return, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, nil>
  : public slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
{
public:
  typedef slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> parent_type;

  inline slot() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desirer functor the new slot should be assigned to.
   */
  template <class T_functor>
  slot(const T_functor& _A_func)
    : parent_type(_A_func) {}

  slot(const slot& src)
    : parent_type(reinterpret_cast<const parent_type&>(src)) {}
};



} /* namespace sigc */
#endif /* _SIGC_FUNCTORS_MACROS_SLOTHM4_ */
