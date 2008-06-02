dnl Copyright 2002, The libsigc++ Development Team 
dnl 
dnl This library is free software; you can redistribute it and/or 
dnl modify it under the terms of the GNU Lesser General Public 
dnl License as published by the Free Software Foundation; either 
dnl version 2.1 of the License, or (at your option) any later version. 
dnl 
dnl This library is distributed in the hope that it will be useful, 
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of 
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
dnl Lesser General Public License for more details. 
dnl 
dnl You should have received a copy of the GNU Lesser General Public 
dnl License along with this library; if not, write to the Free Software 
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
dnl
divert(-1)

include(template.macros.m4)

define([SIGNAL_EMIT_N],[dnl
/** Abstracts signal emission.
 * This template implements the emit() function of signal$1.
 * Template specializations are available to optimize signal
 * emission when no accumulator is used, i.e. the template
 * argument @e T_accumulator is @p nil.
 */
template <LIST(class T_return, LOOP(class T_arg%1, $1), class T_accumulator)>
struct signal_emit$1
{
  typedef signal_emit$1<LIST(T_return, LOOP(T_arg%1, $1), T_accumulator)> self_type;
  typedef typename T_accumulator::result_type result_type;
  typedef slot<LIST(T_return, LOOP(T_arg%1, $1))> slot_type;
  typedef internal::slot_iterator_buf<self_type> slot_iterator_buf_type;
  typedef internal::slot_reverse_iterator_buf<self_type> slot_reverse_iterator_buf_type;
  typedef signal_impl::const_iterator_type iterator_type;

ifelse($1,0,,[dnl
  /** Instantiates the class.
   * The parameters are stored in member variables. operator()() passes
   * the values on to some slot.
   */
])dnl
  signal_emit$1(LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)) ifelse($1,0,,[
    : LOOP(_A_a%1_(_A_a%1), $1)]) {}

ifelse($1,0,[dnl
  /** Invokes a slot.],[
  /** Invokes a slot using the buffered parameter values.])
   * @param _A_slot Some slot to invoke.
   * @return The slot's return value.
   */
  T_return operator()(const slot_type& _A_slot) const
    { return (reinterpret_cast<typename slot_type::call_type>(_A_slot.rep_->call_))(LIST(_A_slot.rep_, LOOP(_A_a%1_, $1))); }
dnl  T_return operator()(const slot_type& _A_slot) const
dnl    { return _A_slot(LOOP(_A_a%1_, $1)); }

  /** Executes a list of slots using an accumulator of type @e T_accumulator.dnl
ifelse($1,0,,[
   * The arguments are buffered in a temporary instance of signal_emit$1.])
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   * @return The accumulated return values of the slot invocations as processed by the accumulator.
   */
  static result_type emit(LIST(signal_impl* impl, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)))
    {
      T_accumulator accumulator;

      if (!impl)
        return accumulator(slot_iterator_buf_type(), slot_iterator_buf_type());

      signal_exec exec(impl);
      temp_slot_list slots(impl->slots_);

      self_type self ifelse($1,0,,[(LOOP(_A_a%1, $1))]);
      return accumulator(slot_iterator_buf_type(slots.begin(), &self),
                         slot_iterator_buf_type(slots.end(), &self));
    }

  /** Executes a list of slots using an accumulator of type @e T_accumulator in reverse order.dnl
ifelse($1,0,,[
   * The arguments are buffered in a temporary instance of signal_emit$1.])
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   * @return The accumulated return values of the slot invocations as processed by the accumulator.
   */
  static result_type emit_reverse(LIST(signal_impl* impl, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)))
    {
      T_accumulator accumulator;

      if (!impl)
        return accumulator(slot_iterator_buf_type(), slot_iterator_buf_type());

      signal_exec exec(impl);
      temp_slot_list slots(impl->slots_);

      self_type self ifelse($1,0,,[(LOOP(_A_a%1, $1))]);
      return accumulator(slot_reverse_iterator_buf_type(slots.end(), &self),
                         slot_reverse_iterator_buf_type(slots.begin(), &self));
    }
dnl
  FOR(1, $1,[
  typename type_trait<T_arg%1>::take _A_a%1_;])
};

/** Abstracts signal emission.
 * This template specialization implements an optimized emit()
 * function for the case that no accumulator is used.
 */
template <LIST(class T_return, LOOP(class T_arg%1, $1))>
struct signal_emit$1<LIST(T_return, LOOP(T_arg%1, $1), nil)>
{
  typedef signal_emit$1<LIST(T_return, LOOP(T_arg%1, $1), nil) > self_type;
  typedef T_return result_type;
  typedef slot<LIST(T_return, LOOP(T_arg%1, $1))> slot_type;
  typedef signal_impl::const_iterator_type iterator_type;
  typedef typename slot_type::call_type call_type;

  /** Executes a list of slots using an accumulator of type @e T_accumulator.dnl
ifelse($1,0,,[
   * The arguments are passed directly on to the slots.])
   * The return value of the last slot invoked is returned.
   * @param first An iterator pointing to the first slot in the list.
   * @param last An iterator pointing to the last slot in the list.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   * @return The return value of the last slot invoked.
   */
  static result_type emit(LIST(signal_impl* impl, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)))
    {
      if (!impl || impl->slots_.empty())
        return T_return();
        
      signal_exec exec(impl);
      T_return r_ = T_return(); 
      
      //Use this scope to make sure that "slots" is destroyed before "exec" is destroyed.
      //This avoids a leak on MSVC++ - see http://bugzilla.gnome.org/show_bug.cgi?id=306249
      { 
        temp_slot_list slots(impl->slots_);
        iterator_type it = slots.begin();
        for (; it != slots.end(); ++it)
          if (!it->empty() && !it->blocked()) break;
          
        if (it == slots.end())
          return T_return(); // note that 'T_return r_();' doesn't work => define 'r_' after this line and initialize as follows:
  
        r_ = (reinterpret_cast<call_type>(it->rep_->call_))(LIST(it->rep_, LOOP(_A_a%1, $1)));
        for (++it; it != slots.end(); ++it)
          {
            if (it->empty() || it->blocked())
              continue;
            r_ = (reinterpret_cast<call_type>(it->rep_->call_))(LIST(it->rep_, LOOP(_A_a%1, $1)));
          }
      }
      
      return r_;
    }

  /** Executes a list of slots using an accumulator of type @e T_accumulator in reverse order.dnl
ifelse($1,0,,[
   * The arguments are passed directly on to the slots.])
   * The return value of the last slot invoked is returned.
   * @param first An iterator pointing to the first slot in the list.
   * @param last An iterator pointing to the last slot in the list.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   * @return The return value of the last slot invoked.
   */
  static result_type emit_reverse(LIST(signal_impl* impl, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)))
    {
      if (!impl || impl->slots_.empty())
        return T_return();
        
      signal_exec exec(impl);
      T_return r_ = T_return(); 
      
      //Use this scope to make sure that "slots" is destroyed before "exec" is destroyed.
      //This avoids a leak on MSVC++ - see http://bugzilla.gnome.org/show_bug.cgi?id=306249
      { 
#ifndef SIGC_HAVE_SUN_REVERSE_ITERATOR
        typedef std::reverse_iterator<signal_impl::iterator_type> reverse_iterator_type;
#else
        typedef std::reverse_iterator<signal_impl::iterator_type, std::random_access_iterator_tag,
                                       slot_base, slot_base&, slot_base*, ptrdiff_t> reverse_iterator_type;
#endif

        temp_slot_list slots(impl->slots_);
        reverse_iterator_type it(slots.end());
        for (; it != reverse_iterator_type(slots.begin()); ++it)
          if (!it->empty() && !it->blocked()) break;
          
        if (it == reverse_iterator_type(slots.begin()))
          return T_return(); // note that 'T_return r_();' doesn't work => define 'r_' after this line and initialize as follows:
  
        r_ = (reinterpret_cast<call_type>(it->rep_->call_))(LIST(it->rep_, LOOP(_A_a%1, $1)));
        for (++it; it != reverse_iterator_type(slots.begin()); ++it)
          {
            if (it->empty() || it->blocked())
              continue;
            r_ = (reinterpret_cast<call_type>(it->rep_->call_))(LIST(it->rep_, LOOP(_A_a%1, $1)));
          }
      }
      
      return r_;
    }
};

/** Abstracts signal emission.
 * This template specialization implements an optimized emit()
 * function for the case that no accumulator is used and the
 * return type is @p void.
 */
template <LOOP(class T_arg%1, $1)>
struct signal_emit$1<LIST(void, LOOP(T_arg%1, $1), nil)>
{
  typedef signal_emit$1<LIST(void, LOOP(T_arg%1, $1), nil)> self_type;
  typedef void result_type;
  typedef slot<LIST(void, LOOP(T_arg%1, $1))> slot_type;
  typedef signal_impl::const_iterator_type iterator_type;
  typedef ifelse($1,0,void (*call_type)(slot_rep*),typename slot_type::call_type call_type);

  /** Executes a list of slots using an accumulator of type @e T_accumulator.dnl
ifelse($1,0,,[
   * The arguments are passed directly on to the slots.])
   * @param first An iterator pointing to the first slot in the list.
   * @param last An iterator pointing to the last slot in the list.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   */
  static result_type emit(LIST(signal_impl* impl, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)))
    {
      if (!impl || impl->slots_.empty()) return;
      signal_exec exec(impl);
      temp_slot_list slots(impl->slots_);

      for (iterator_type it = slots.begin(); it != slots.end(); ++it)
        {
          if (it->empty() || it->blocked())
            continue;
          (reinterpret_cast<call_type>(it->rep_->call_))(LIST(it->rep_, LOOP(_A_a%1, $1)));
        }
    }

  /** Executes a list of slots using an accumulator of type @e T_accumulator in reverse order.dnl
ifelse($1,0,,[
   * The arguments are passed directly on to the slots.])
   * @param first An iterator pointing to the first slot in the list.
   * @param last An iterator pointing to the last slot in the list.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   */
  static result_type emit_reverse(LIST(signal_impl* impl, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)))
    {
      if (!impl || impl->slots_.empty()) return;
      signal_exec exec(impl);
      temp_slot_list slots(impl->slots_);

#ifndef SIGC_HAVE_SUN_REVERSE_ITERATOR
      typedef std::reverse_iterator<signal_impl::iterator_type> reverse_iterator_type;
#else
      typedef std::reverse_iterator<signal_impl::iterator_type, std::random_access_iterator_tag,
                                     slot_base, slot_base&, slot_base*, ptrdiff_t> reverse_iterator_type;
#endif
      for (reverse_iterator_type it = reverse_iterator_type(slots.end()); it != reverse_iterator_type(slots.begin()); ++it)
        {
          if (it->empty() || it->blocked())
            continue;
          (reinterpret_cast<call_type>(it->rep_->call_))(LIST(it->rep_, LOOP(_A_a%1, $1)));
        }
    }
};

])
define([SIGNAL_N],[dnl
/** Signal declaration.
 * signal$1 can be used to connect() slots that are invoked
 * during subsequent calls to emit(). Any functor or slot
 * can be passed into connect(). It is converted into a slot
 * implicitely.
 *
 * If you want to connect one signal to another, use make_slot()
 * to retrieve a functor that emits the signal when invoked.
 *
 * Be careful if you directly pass one signal into the connect()
 * method of another: a shallow copy of the signal is made and
 * the signal's slots are not disconnected until both the signal
 * and its clone are destroyed which is probably not what you want!
 *
 * An STL-style list interface for the signal's list of slots
 * can be retrieved with slots(). This interface supports
 * iteration, insertion and removal of slots.
 *
 * The following template arguments are used:
 * - @e T_return The desired return type for the emit() function (may be overridden by the accumulator).dnl
FOR(1,$1,[
 * - @e T_arg%1 Argument type used in the definition of emit().])
 * - @e T_accumulator The accumulator type used for emission. The default @p nil means that no accumulator should be used, i.e. signal emission returns the return value of the last slot invoked.
 *
 * You should use the more convenient unnumbered sigc::signal template.
 *
 * @ingroup signal
 */
template <LIST(class T_return, LOOP(class T_arg%1, $1), class T_accumulator=nil)>
class signal$1
  : public signal_base
{
public:
  typedef internal::signal_emit$1<LIST(T_return, LOOP(T_arg%1, $1), T_accumulator)> emitter_type;
  typedef typename emitter_type::result_type         result_type;
  typedef slot<LIST(T_return, LOOP(T_arg%1, $1))>    slot_type;
  typedef slot_list<slot_type>                       slot_list_type;
  typedef typename slot_list_type::iterator               iterator;
  typedef typename slot_list_type::const_iterator         const_iterator;
  typedef typename slot_list_type::reverse_iterator       reverse_iterator;
  typedef typename slot_list_type::const_reverse_iterator const_reverse_iterator;

  /** Add a slot to the list of slots.
   * Any functor or slot may be passed into connect().
   * It will be converted into a slot implicitely.
   * The returned iterator may be stored for disconnection
   * of the slot at some later point. It stays valid until
   * the slot is removed from the list of slots. The iterator
   * can also be implicitely converted into a sigc::connection object
   * that may be used safely beyond the life time of the slot.
   * @param slot_ The slot to add to the list of slots.
   * @return An iterator pointing to the new slot in the list.
   */
  iterator connect(const slot_type& slot_)
    { return iterator(signal_base::connect(static_cast<const slot_base&>(slot_))); }

  /** Triggers the emission of the signal.
   * During signal emission all slots that have been connected
   * to the signal are invoked unless they are manually set into
   * a blocking state. The parameters are passed on to the slots.
   * If @e T_accumulated is not @p nil, an accumulator of this type
   * is used to process the return values of the slot invocations.
   * Otherwise, the return value of the last slot invoked is returned.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the slots.])
   * @return The accumulated return values of the slot invocations.
   */
  result_type emit(LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)) const
    { return emitter_type::emit(LIST(impl_, LOOP(_A_a%1, $1))); }

  /** Triggers the emission of the signal in reverse order (see emit()). */
  result_type emit_reverse(LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)) const
    { return emitter_type::emit_reverse(LIST(impl_, LOOP(_A_a%1, $1))); }

  /** Triggers the emission of the signal (see emit()). */
  result_type operator()(LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)) const
    { return emit(LOOP(_A_a%1, $1)); }

  /** Creates a functor that calls emit() on this signal.
   * @code
   * sigc::mem_fun(mysignal, &sigc::signal$1::emit)
   * @endcode
   * yields the same result.
   * @return A functor that calls emit() on this signal.
   */
  bound_const_mem_functor$1<LIST(result_type, signal$1, LOOP(typename type_trait<T_arg%1>::take, $1))> make_slot() const
    { return bound_const_mem_functor$1<LIST(result_type, signal$1, LOOP(typename type_trait<T_arg%1>::take, $1))>(this, &signal$1::emit); }

  /** Creates an STL-style interface for the signal's list of slots.
   * This interface supports iteration, insertion and removal of slots.
   * @return An STL-style interface for the signal's list of slots.
   */
  slot_list_type slots()
    { return slot_list_type(impl()); }

  /** Creates an STL-style interface for the signal's list of slots.
   * This interface supports iteration, insertion and removal of slots.
   * @return An STL-style interface for the signal's list of slots.
   */
  const slot_list_type slots() const
    { return slot_list_type(const_cast<signal$1*>(this)->impl()); }

  signal$1() {}

  signal$1(const signal$1& src)
    : signal_base(src) {}
};

])
define([SIGNAL],[dnl
ifelse($1, $2,[dnl
/** Convenience wrapper for the numbered sigc::signal# templates.
 * signal can be used to connect() slots that are invoked
 * during subsequent calls to emit(). Any functor or slot
 * can be passed into connect(). It is converted into a slot
 * implicitly.
 *
 * If you want to connect one signal to another, use make_slot()
 * to retrieve a functor that emits the signal when invoked.
 *
 * Be careful if you directly pass one signal into the connect()
 * method of another: a shallow copy of the signal is made and
 * the signal's slots are not disconnected until both the signal
 * and its clone are destroyed which is probably not what you want!
 *
 * An STL-style list interface for the signal's list of slots
 * can be retrieved with slots(). This interface supports
 * iteration, insertion and removal of slots.
 *
 * The template arguments determine the function signature of
 * the emit() function:
 * - @e T_return The desired return type of the emit() function.dnl
FOR(1,$1,[
 * - @e T_arg%1 Argument type used in the definition of emit(). The default @p nil means no argument.])
 *
 * To specify an accumulator type the nested class signal::accumulated can be used.
 *
 * @par Example:
 *   @code
 *   void foo(int) {}
 *   sigc::signal<void, long> sig;
 *   sig.connect(sigc::ptr_fun(&foo));
 *   sig.emit(19);
 *   @endcode
 *
 * @ingroup signal
 */
template <LIST(class T_return, LOOP(class T_arg%1 = nil, $1))>],[dnl

/** Convenience wrapper for the numbered sigc::signal$1 template.
 * See the base class for useful methods.
 * This is the template specialization of the unnumbered sigc::signal
 * template for $1 argument(s).
ifelse($1, $2,[dnl
 *
 * @ingroup signal
])dnl
 */
template <LIST(class T_return, LOOP(class T_arg%1, $1))>])
class signal ifelse($1, $2,,[<LIST(T_return, LOOP(T_arg%1,$1), LOOP(nil, CALL_SIZE - $1))>])
  : public signal$1<LIST(T_return, LOOP(T_arg%1, $1),nil)>
{
public:
ifelse($1, $2,[dnl
  /** Convenience wrapper for the numbered sigc::signal# templates.
   * Like sigc::signal but the additional template parameter @e T_accumulator
   * defines the accumulator type that should be used.
   *
   * An accumulator is a functor that uses a pair of special iterators
   * to step through a list of slots and calculate a return value
   * from the results of the slot invokations. The iterators' operator*()
   * executes the slot. The return value is buffered, so that in an expression
   * like @code a = (*i) * (*i); @endcode the slot is executed only once.
   * The accumulator must define its return value as @p result_type.
   * 
   * @par Example 1:
   *   This accumulator calculates the arithmetic mean value:
   *   @code
   *   struct arithmetic_mean_accumulator
   *   {
   *     typedef double result_type;
   *     template<typename T_iterator>
   *     result_type operator()(T_iterator first, T_iterator last) const
   *     {
   *       result_type value_ = 0;
   *       int n_ = 0;
   *       for (; first != last; ++first, ++n_)
   *         value_ += *first;
   *       return value_ / n_;
   *     }
   *   };
   *   @endcode
   *
   * @par Example 2:
   *   This accumulator stops signal emission when a slot returns zero:
   *   @code
   *   struct interruptable_accumulator
   *   {
   *     typedef bool result_type;
   *     template<typename T_iterator>
   *     result_type operator()(T_iterator first, T_iterator last) const
   *     {
   *       for (; first != last; ++first, ++n_)
   *         if (!*first) return false;
   *       return true;
   *     }
   *   };
   *   @endcode
   *
   * @ingroup signal
],[
  /** Convenience wrapper for the numbered sigc::signal$1 template.
   * Like sigc::signal but the additional template parameter @e T_accumulator
   * defines the accumulator type that should be used.
])dnl
   */
  template <class T_accumulator>
  class accumulated
    : public signal$1<LIST(T_return, LOOP(T_arg%1, $1), T_accumulator)>
  {
  public:
    accumulated() {}
    accumulated(const accumulated& src)
      : signal$1<LIST(T_return, LOOP(T_arg%1, $1), T_accumulator)>(src) {}
  };

  signal() {}
  signal(const signal& src)
    : signal$1<LIST(T_return, LOOP(T_arg%1, $1),nil)>(src) {}
};

])

divert(0)
#ifndef _SIGC_SIGNAL_H_
#define _SIGC_SIGNAL_H_

#include <list>
#include <sigc++/signal_base.h>
#include <sigc++/type_traits.h>
#include <sigc++/trackable.h>
#include <sigc++/functors/slot.h>
#include <sigc++/functors/mem_fun.h>

//SIGC_TYPEDEF_REDEFINE_ALLOWED:
// TODO: This should have its own test, but I can not create one that gives the error instead of just a warning. murrayc.
// I have just used this because there is a correlation between these two problems.
#ifdef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  //Compilers, such as older versions of SUN Forte C++, that do not allow this also often
  //do not allow a typedef to have the same name as a class in the typedef's definition.
  //For Sun Forte CC 5.7 (SUN Workshop 10), comment this out to fix the build.
  #define SIGC_TYPEDEF_REDEFINE_ALLOWED 1
#endif

namespace sigc {

/** STL-style iterator for slot_list.
 *
 * @ingroup signal
 */
template <typename T_slot>
struct slot_iterator
{
  typedef size_t                          size_type;
  typedef ptrdiff_t                       difference_type;
  typedef std::bidirectional_iterator_tag iterator_category;

  typedef T_slot  slot_type;

  typedef T_slot  value_type;
  typedef T_slot* pointer;
  typedef T_slot& reference;

  typedef typename internal::signal_impl::iterator_type iterator_type;

  slot_iterator()
    {}

  explicit slot_iterator(const iterator_type& i)
    : i_(i) {}

  reference operator*() const
    { return static_cast<reference>(*i_); }

  pointer operator->() const
    { return &(operator*()); }

  slot_iterator& operator++()
    {
      ++i_;
      return *this;
    }

  slot_iterator operator++(int)
    { 
      slot_iterator __tmp(*this);
      ++i_;
      return __tmp;
    }

  slot_iterator& operator--()
    {
      --i_;
      return *this;
    }

  slot_iterator operator--(int)
    {
      slot_iterator __tmp(*this);
      --i_;
      return __tmp;
    }

  bool operator == (const slot_iterator& other) const
    { return i_ == other.i_; }

  bool operator != (const slot_iterator& other) const
    { return i_ != other.i_; }

  iterator_type i_;
};

/** STL-style const iterator for slot_list.
 *
 * @ingroup signal
 */
template <typename T_slot>
struct slot_const_iterator
{
  typedef size_t                          size_type;
  typedef ptrdiff_t                       difference_type;
  typedef std::bidirectional_iterator_tag iterator_category;

  typedef T_slot        slot_type;

  typedef T_slot        value_type;
  typedef const T_slot* pointer;
  typedef const T_slot& reference;

  typedef typename internal::signal_impl::const_iterator_type iterator_type;

  slot_const_iterator()
    {}

  explicit slot_const_iterator(const iterator_type& i)
    : i_(i) {}

  reference operator*() const
    { return static_cast<reference>(*i_); }

  pointer operator->() const
    { return &(operator*()); }

  slot_const_iterator& operator++()
    {
      ++i_;
      return *this;
    }

  slot_const_iterator operator++(int)
    { 
      slot_const_iterator __tmp(*this);
      ++i_;
      return __tmp;
    }

  slot_const_iterator& operator--()
    {
      --i_;
      return *this;
    }

  slot_const_iterator operator--(int)
    {
      slot_const_iterator __tmp(*this);
      --i_;
      return __tmp;
    }

  bool operator == (const slot_const_iterator& other) const
    { return i_ == other.i_; }

  bool operator != (const slot_const_iterator& other) const
    { return i_ != other.i_; }

  iterator_type i_;
};

/** STL-style list interface for sigc::signal#.
 * slot_list can be used to iterate over the list of slots that
 * is managed by a signal. Slots can be added or removed from
 * the list while existing iterators stay valid. A slot_list
 * object can be retrieved from the signal's slots() function.
 *
 * @ingroup signal
 */
template <class T_slot>
struct slot_list
{
  typedef T_slot slot_type;

  typedef slot_type&       reference;
  typedef const slot_type& const_reference;

  typedef slot_iterator<slot_type>              iterator;
  typedef slot_const_iterator<slot_type>        const_iterator;
  
  #ifndef SIGC_HAVE_SUN_REVERSE_ITERATOR
  typedef std::reverse_iterator<iterator>       reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  #else
  typedef std::reverse_iterator<iterator, std::random_access_iterator_tag,
                                int, int&, int*, ptrdiff_t> reverse_iterator;

  typedef std::reverse_iterator<const_iterator, std::random_access_iterator_tag,
                                int, const int&, const int*, ptrdiff_t> const_reverse_iterator;
  #endif /* SIGC_HAVE_SUN_REVERSE_ITERATOR */




  slot_list()
    : list_(0) {}

  explicit slot_list(internal::signal_impl* __list)
    : list_(__list) {}

  iterator begin()
    { return iterator(list_->slots_.begin()); }

  const_iterator begin() const
    { return const_iterator(list_->slots_.begin()); }

  iterator end()
    { return iterator(list_->slots_.end()); }

  const_iterator end() const
    { return const_iterator(list_->slots_.end()); }

  reverse_iterator rbegin() 
    { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const 
    { return const_reverse_iterator(end()); }

  reverse_iterator rend()
    { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const
    { return const_reverse_iterator(begin()); }

  reference front()
    { return *begin(); }

  const_reference front() const
    { return *begin(); }

  reference back()
    { return *(--end()); }

  const_reference back() const
    { return *(--end()); }

  iterator insert(iterator i, const slot_type& slot_)
    { return iterator(list_->insert(i.i_, static_cast<const slot_base&>(slot_))); }

  void push_front(const slot_type& c)
    { insert(begin(), c); }

  void push_back(const slot_type& c)
    { insert(end(), c); }

  iterator erase(iterator i)
    { return iterator(list_->erase(i.i_)); }

  iterator erase(iterator first_, iterator last_)
    {
      while (first_ != last_)
        first_ = erase(first_);
      return last_;
    }

  void pop_front()
    { erase(begin()); }

  void pop_back()
    { 
      iterator tmp_ = end();
      erase(--tmp_);
    }

protected:
  internal::signal_impl* list_;
};


namespace internal {

/** Special iterator over sigc::internal::signal_impl's slot list that holds extra data.
 * This iterators is for use in accumulators. operator*() executes
 * the slot. The return value is buffered, so that in an expression
 * like @code a = (*i) * (*i); @endcode the slot is executed only once.
 */
template <class T_emitter, class T_result = typename T_emitter::result_type>
struct slot_iterator_buf
{
  typedef size_t                           size_type;
  typedef ptrdiff_t                        difference_type;
  typedef std::bidirectional_iterator_tag  iterator_category;

  //These are needed just to make this a proper C++ iterator, 
  //that can be used with standard C++ algorithms.
  typedef T_result                         value_type;
  typedef T_result&                        reference;
  typedef T_result*                        pointer;

  typedef T_emitter                        emitter_type;
  typedef T_result                         result_type;
  typedef typename T_emitter::slot_type    slot_type;

  typedef signal_impl::const_iterator_type iterator_type;

  slot_iterator_buf()
    : c_(0), invoked_(false) {}

  slot_iterator_buf(const iterator_type& i, const emitter_type* c)
    : i_(i), c_(c), invoked_(false) {}

  result_type operator*() const
    {
      if (!i_->empty() && !i_->blocked() && !invoked_)
        {
          r_ = (*c_)(static_cast<const slot_type&>(*i_));
          invoked_ = true;
        }
      return r_;
    }

  slot_iterator_buf& operator++()
    {
      ++i_;
      invoked_ = false;
      return *this;
    }

  slot_iterator_buf operator++(int)
    { 
      slot_iterator_buf __tmp(*this);
      ++i_;
      invoked_ = false;
      return __tmp;
    }

  slot_iterator_buf& operator--()
    {
      --i_;
      invoked_ = false;
      return *this;
    }

  slot_iterator_buf operator--(int)
    {
      slot_iterator_buf __tmp(*this);
      --i_;
      invoked_ = false;
      return __tmp;
    }

  bool operator == (const slot_iterator_buf& other) const
    { return (!c_ || (i_ == other.i_)); } /* If '!c_' the iterators are empty.
                                           * Unfortunately, empty stl iterators are not equal.
                                           * We are forcing equality so that 'first==last'
                                           * in the accumulator's emit function yields true. */

  bool operator != (const slot_iterator_buf& other) const
    { return (c_ && (i_ != other.i_)); }

private:
  iterator_type i_;
  const emitter_type* c_;
  mutable result_type r_;
  mutable bool invoked_;
};

/** Template specialization of slot_iterator_buf for void return signals.
 */
template <class T_emitter>
struct slot_iterator_buf<T_emitter, void>
{
  typedef size_t                           size_type;
  typedef ptrdiff_t                        difference_type;
  typedef std::bidirectional_iterator_tag  iterator_category;

  typedef T_emitter                        emitter_type;
  typedef void                             result_type;
  typedef typename T_emitter::slot_type    slot_type;

  typedef signal_impl::const_iterator_type iterator_type;

  slot_iterator_buf()
    : c_(0), invoked_(false) {}

  slot_iterator_buf(const iterator_type& i, const emitter_type* c)
    : i_(i), c_(c), invoked_(false) {}

  void operator*() const
    {
      if (!i_->empty() && !i_->blocked() && !invoked_)
        {
          (*c_)(static_cast<const slot_type&>(*i_));
          invoked_ = true;
        }
    }

  slot_iterator_buf& operator++()
    {
      ++i_;
      invoked_ = false;
      return *this;
    }

  slot_iterator_buf operator++(int)
    { 
      slot_iterator_buf __tmp(*this);
      ++i_;
      invoked_ = false;
      return __tmp;
    }

  slot_iterator_buf& operator--()
    {
      --i_;
      invoked_ = false;
      return *this;
    }

  slot_iterator_buf operator--(int)
    {
      slot_iterator_buf __tmp(*this);
      --i_;
      invoked_ = false;
      return __tmp;
    }

  bool operator == (const slot_iterator_buf& other) const
    { return i_ == other.i_; }

  bool operator != (const slot_iterator_buf& other) const
    { return i_ != other.i_; }

private:
  iterator_type i_;
  const emitter_type* c_;
  mutable bool invoked_;
};

/** Reverse version of sigc::internal::slot_iterator_buf. */
template <class T_emitter, class T_result = typename T_emitter::result_type>
struct slot_reverse_iterator_buf
{
  typedef size_t                           size_type;
  typedef ptrdiff_t                        difference_type;
  typedef std::bidirectional_iterator_tag  iterator_category;

  //These are needed just to make this a proper C++ iterator, 
  //that can be used with standard C++ algorithms.
  typedef T_result                         value_type;
  typedef T_result&                        reference;
  typedef T_result*                        pointer;

  typedef T_emitter                        emitter_type;
  typedef T_result                         result_type;
  typedef typename T_emitter::slot_type    slot_type;

  typedef signal_impl::const_iterator_type iterator_type;

  slot_reverse_iterator_buf()
    : c_(0), invoked_(false) {}

  slot_reverse_iterator_buf(const iterator_type& i, const emitter_type* c)
    : i_(i), c_(c), invoked_(false) {}

  result_type operator*() const
    {
      iterator_type __tmp(i_);
	  --__tmp;
      if (!__tmp->empty() && !__tmp->blocked() && !invoked_)
        {
          r_ = (*c_)(static_cast<const slot_type&>(*__tmp));
          invoked_ = true;
        }
      return r_;
    }

  slot_reverse_iterator_buf& operator++()
    {
      --i_;
      invoked_ = false;
      return *this;
    }

  slot_reverse_iterator_buf operator++(int)
    { 
      slot_reverse_iterator_buf __tmp(*this);
      --i_;
      invoked_ = false;
      return __tmp;
    }

  slot_reverse_iterator_buf& operator--()
    {
      ++i_;
      invoked_ = false;
      return *this;
    }

  slot_reverse_iterator_buf operator--(int)
    {
      slot_reverse_iterator_buf __tmp(*this);
      ++i_;
      invoked_ = false;
      return __tmp;
    }

  bool operator == (const slot_reverse_iterator_buf& other) const
    { return (!c_ || (i_ == other.i_)); } /* If '!c_' the iterators are empty.
                                           * Unfortunately, empty stl iterators are not equal.
                                           * We are forcing equality so that 'first==last'
                                           * in the accumulator's emit function yields true. */

  bool operator != (const slot_reverse_iterator_buf& other) const
    { return (c_ && (i_ != other.i_)); }

private:
  iterator_type i_;
  const emitter_type* c_;
  mutable result_type r_;
  mutable bool invoked_;
};

/** Template specialization of slot_reverse_iterator_buf for void return signals.
 */
template <class T_emitter>
struct slot_reverse_iterator_buf<T_emitter, void>
{
  typedef size_t                           size_type;
  typedef ptrdiff_t                        difference_type;
  typedef std::bidirectional_iterator_tag  iterator_category;

  typedef T_emitter                        emitter_type;
  typedef void                             result_type;
  typedef typename T_emitter::slot_type    slot_type;

  typedef signal_impl::const_iterator_type iterator_type;

  slot_reverse_iterator_buf()
    : c_(0), invoked_(false) {}

  slot_reverse_iterator_buf(const iterator_type& i, const emitter_type* c)
    : i_(i), c_(c), invoked_(false) {}

  void operator*() const
    {
      iterator_type __tmp(i_);
	  --__tmp;
	  if (!__tmp->empty() && !__tmp->blocked() && !invoked_)
        {
          (*c_)(static_cast<const slot_type&>(*__tmp));
          invoked_ = true;
        }
    }

  slot_reverse_iterator_buf& operator++()
    {
      --i_;
      invoked_ = false;
      return *this;
    }

  slot_reverse_iterator_buf operator++(int)
    { 
      slot_reverse_iterator_buf __tmp(*this);
      --i_;
      invoked_ = false;
      return __tmp;
    }

  slot_reverse_iterator_buf& operator--()
    {
      ++i_;
      invoked_ = false;
      return *this;
    }

  slot_reverse_iterator_buf operator--(int)
    {
      slot_reverse_iterator_buf __tmp(*this);
      ++i_;
      invoked_ = false;
      return __tmp;
    }

  bool operator == (const slot_reverse_iterator_buf& other) const
    { return i_ == other.i_; }

  bool operator != (const slot_reverse_iterator_buf& other) const
    { return i_ != other.i_; }

private:
  iterator_type i_;
  const emitter_type* c_;
  mutable bool invoked_;
};

FOR(0,CALL_SIZE,[[SIGNAL_EMIT_N(%1)]])
} /* namespace internal */

FOR(0,CALL_SIZE,[[SIGNAL_N(%1)]])

SIGNAL(CALL_SIZE,CALL_SIZE)
FOR(0,eval(CALL_SIZE-1),[[SIGNAL(%1)]])

} /* namespace sigc */

#endif /* _SIGC_SIGNAL_H_ */
