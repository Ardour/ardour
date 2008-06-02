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
#ifndef _SIGC_REFERENCE_WRAPPER_H_
#define _SIGC_REFERENCE_WRAPPER_H_

namespace sigc {

/** Reference wrapper.
 * Use sigc::ref() to create a reference wrapper.
 */
template <class T_type>
struct reference_wrapper
{
  explicit reference_wrapper(T_type& v)
    : value_(v)  {}

  operator T_type& () const
    { return value_; }

  T_type& value_;
};

/** Const reference wrapper.
 * Use sigc::ref() to create a const reference wrapper.
 */
template <class T_type>
struct const_reference_wrapper
{
  explicit const_reference_wrapper(const T_type& v)
    : value_(v)  {}

  operator const T_type& () const
    { return value_; }

  const T_type& value_;
};

/** Creates a reference wrapper.
 * Passing an object throught sigc::ref() makes libsigc++ adaptors
 * like, e.g., sigc::bind store references to the object instead of copies.
 * If the object type inherits from sigc::trackable this will ensure
 * automatic invalidation of the adaptors when the object is deleted
 * or overwritten.
 *
 * @param v Reference to store.
 * @return A reference wrapper.
 */
template <class T_type>
reference_wrapper<T_type> ref(T_type& v)
{ return reference_wrapper<T_type>(v); }

/** Creates a const reference wrapper.
 * Passing an object throught sigc::ref() makes libsigc++ adaptors
 * like, e.g., sigc::bind store references to the object instead of copies.
 * If the object type inherits from sigc::trackable this will ensure
 * automatic invalidation of the adaptors when the object is deleted
 * or overwritten.
 *
 * @param v Reference to store.
 * @return A reference wrapper.
 */
template <class T_type>
const_reference_wrapper<T_type> ref(const T_type& v)
{ return const_reference_wrapper<T_type>(v); }

template <class T_type>
struct unwrap_reference
{
  typedef T_type type;
};

template <class T_type>
struct unwrap_reference<reference_wrapper<T_type> >
{
  typedef T_type& type;
};

template <class T_type>
struct unwrap_reference<const_reference_wrapper<T_type> >
{
  typedef const T_type& type;
};

template <class T_type>
T_type& unwrap(const reference_wrapper<T_type>& v)
{ return v; }

template <class T_type>
const T_type& unwrap(const const_reference_wrapper<T_type>& v)
{ return v; }

} /* namespace sigc */

#endif /* _SIGC_REFERENCE_WRAPPER_H_ */
