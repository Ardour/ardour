dnl
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

define([LIMIT_REFERENCE],[dnl
/** A [$1]limit_reference<Foo> object stores a reference (Foo&), but make sure that,
 * if Foo inherits from sigc::trackable, then visit_each<>() will "limit" itself to the
 * sigc::trackable reference instead of the derived reference. This avoids use of
 * a reference to the derived type when the derived destructor has run. That can be
 * a problem when using virtual inheritance.
 *
 * If Foo inherits from trackable then both the derived reference and the
 * sigc::trackable reference are stored, so we can later retrieve the sigc::trackable
 * reference without doing an implicit conversion. To retrieve the derived reference
 * (so that you invoke methods or members of it), use invoke(). To retrieve the trackable
 * reference (so that you can call visit_each() on it), you use visit().
 *
 * If Foo does not inherit from sigc::trackable then invoke() and visit() just return the
 * derived reference.
 *
 * This is used for bound (sigc::bind) slot parameters (via bound_argument), bound return values, 
 * and, with mem_fun(), the reference to the handling object.
 *
 * - @e T_type The type of the reference.
 */
template <class T_type,
          bool I_derives_trackable =
            is_base_and_derived<trackable, T_type>::value>
class [$1]limit_reference
{
public:
  /** Constructor.
   * @param _A_target The reference to limit.
   */
  [$1]limit_reference([$2]T_type& _A_target)
    : visited(_A_target)
    {}

  /** Retrieve the entity to visit for visit_each().
   * Depending on the template specialization, this is either a derived reference, or sigc::trackable& if T_type derives from sigc::trackable.
   * @return The reference.
   */
  inline const T_type& visit() const
    { return visited; }

  /** Retrieve the reference.
   * This is always a reference to the derived instance.
   * @return The reference.
   */
  inline [$3]T_type& invoke() const
    { return visited; }

private:
  /** The reference.
   */
  [$2]T_type& visited;
};

/** [$1]limit_reference object for a class that derives from trackable.
 * - @e T_type The type of the reference.
 */
template <class T_type>
class [$1]limit_reference<T_type, true>
{
public:
  /** Constructor.
   * @param _A_target The reference to limit.
   */
  [$1]limit_reference([$2]T_type& _A_target)
    : visited(_A_target),
      invoked(_A_target)
    {}

  /** Retrieve the entity to visit for visit_each().
   * Depending on the template specialization, this is either a derived reference, or sigc::trackable& if T_type derives from sigc::trackable.
   * @return The reference.
   */
  inline const trackable& visit() const
    { return visited; }

  /** Retrieve the reference.
   * This is always a reference to the derived instance.
   * @return The reference.
   */
  inline [$3]T_type& invoke() const
    { return invoked; }

private:
  /** The trackable reference.
   */
  [$2]trackable& visited;

  /** The reference.
   */
  [$2]T_type& invoked;
};

/** Implementation of visit_each() specialized for the [$1]limit_reference
 * class, to call visit_each() on the entity returned by the [$1]limit_reference's
 * visit() method.
 * - @e T_action The type of functor to invoke.
 * - @e T_type The type of the reference.
 * @param _A_action The functor to invoke.
 * @param _A_argument The visited instance.
 */
template <class T_action, class T_type, bool I_derives_trackable>
void
visit_each(const T_action& _A_action,
           const [$1]limit_reference<T_type, I_derives_trackable>& _A_target)
{
  visit_each(_A_action, _A_target.visit());
}
])

divert(0)

__FIREWALL__


#include <sigc++/type_traits.h>
#include <sigc++/trackable.h>


namespace sigc {


LIMIT_REFERENCE([],[],[])dnl


LIMIT_REFERENCE([const_],[const ],[const ])dnl


LIMIT_REFERENCE([volatile_],[],[volatile ])dnl


LIMIT_REFERENCE([const_volatile_],[const ],[const volatile ])dnl


} /* namespace sigc */


