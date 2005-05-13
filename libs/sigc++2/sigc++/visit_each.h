// -*- c++ -*- 
/*
 * Copyright 2002, The libsigc++ Development Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _SIGC_VISIT_EACH_HPP_
#define _SIGC_VISIT_EACH_HPP_

#include <sigc++/type_traits.h>

namespace sigc {

namespace internal {

/// Helper struct for visit_each_type().
template <class T_target, class T_action>
struct limit_derived_target
{
  typedef limit_derived_target<T_target, T_action> T_self;

  template <bool I_derived, class T_type> struct with_type;

  template <class T_type> struct with_type<false,T_type>
  { static void execute_(const T_type&, const T_self&) {} };

  template <class T_type> struct with_type<true,T_type>
  { static void execute_(const T_type& _A_type, const T_self& _A_action)
      { _A_action.action_(_A_type); }
  };

  template <class T_type>
  void operator()(const T_type& _A_type) const
  { with_type<is_base_and_derived<T_target,T_type>::value,T_type>::execute_(_A_type,*this); }

  limit_derived_target(const T_action& _A_action): action_(_A_action) {}

  T_action action_;
};

/// Helper struct for visit_each_type().
template <class T_target, class T_action>
struct limit_derived_target<T_target*, T_action>
{
  typedef limit_derived_target<T_target*, T_action> T_self;

  template <bool I_derived, class T_type> struct with_type;

  template <class T_type> struct with_type<false,T_type>
  { static void execute_(const T_type&, const T_self&) {} };

  template <class T_type> struct with_type<true,T_type>
  { static void execute_(const T_type& _A_type, const T_self& _A_action) 
     { _A_action.action_(&_A_type); }
  };

  template <class T_type>
  void operator()(const T_type& _A_type) const
  { with_type<is_base_and_derived<T_target,T_type>::value,T_type>::execute_(_A_type,*this); }

  limit_derived_target(const T_action& _A_action): action_(_A_action) {}

  T_action action_;
};

} /* namespace internal */


/** This function performs a functor on each of the targets of a functor.
 * All unknown types just call @e _A_action on them.
 * Add overloads that specialize the @e T_functor argument for your own
 * functor types, so that subobjects get visited. This is needed to enable
 * auto-disconnection support for your functor types.
 *
 * @par Example:
 *   @code
 *   struct some_functor
 *   {
 *     void operator()() {}
 *     some_possibly_sigc_trackable_derived_type some_data_member;
 *     some_other_functor_type some_other_functor;
 *   }
 *
 *   namespace sigc
 *   {
 *     template <class T_action>
 *     void visit_each(const T_action& _A_action,
 *                     const some_functor& _A_target)
 *     {
 *       visit_each(_A_action, _A_target.some_data_member);
 *       visit_each(_A_action, _A_target.some_other_functor);
 *     }
 *   }
 *   @endcode
 *
 * @ingroup functors
 */
template <class T_action, class T_functor>
void visit_each(const T_action& _A_action, const T_functor& _A_functor)
{ _A_action(_A_functor); }

/** This function performs a functor on each of the targets
 * of a functor limited to a restricted type.
 *
 * @ingroup functors
 */
template <class T_type, class T_action, class T_functor>
void visit_each_type(const T_action& _A_action, const T_functor& _A_functor)
{ 
  internal::limit_derived_target<T_type,T_action> limited_action(_A_action);
  visit_each(limited_action,_A_functor);
}

} /* namespace sigc */
#endif
