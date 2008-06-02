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
#ifndef _SIGC_TYPE_TRAIT_H_
#define _SIGC_TYPE_TRAIT_H_

#include <sigc++config.h> //To get SIGC_SELF_REFERENCE_IN_MEMBER_INITIALIZATION


namespace sigc {

template <class T_type>
struct type_trait
{
  typedef T_type  type;
  typedef T_type& pass;
  typedef const T_type& take;
  typedef T_type* pointer;
};

template <class T_type, int N>
struct type_trait<T_type[N]>
{
  typedef T_type*  type;
  typedef T_type*& pass;
  typedef const T_type*& take;
  typedef T_type** pointer;
};

template <class T_type>
struct type_trait<T_type&>
{
  typedef T_type  type;
  typedef T_type& pass;
  typedef T_type& take;
  typedef T_type* pointer;
};

template <class T_type>
struct type_trait<const T_type&>
{
  typedef const T_type  type;
  typedef const T_type& pass;
  typedef const T_type& take;
  typedef const T_type* pointer;
};

template<>
struct type_trait<void>
{
  typedef void  type;
  typedef void  pass;
  typedef void  take;
  typedef void* pointer;
};


// From Esa Pulkkin:
/**
 * Compile-time determination of base-class relationship in C++
 * (adapted to match the syntax of boost's type_traits library).
 *
 * Use this to provide a template specialization for a set of types.
 * For instance,
 *
 * template < class T_thing, bool Tval_derives_from_something = sigc::is_base_and_derived<Something, T_thing>::value >
 * class TheTemplate
 * {
 *   //Standard implementation.
 * }
 *
 * //Specialization for T_things that derive from Something (Tval_derives_from_something is true)
 * template <class T_thing>
 * class TheTemplate<T_thing, true>
 * {
 *   T_thing thing;
     thing.method_that_is_in_something();
 * }
 */
template <class T_base, class T_derived>
struct is_base_and_derived
{
private:
  struct big {
    char memory[64];
  };

#ifndef SIGC_SELF_REFERENCE_IN_MEMBER_INITIALIZATION

  //Allow the internal inner class to access the other (big) inner
  //class.  The Tru64 compiler needs this. murrayc.
  friend struct internal_class;

  //Certain compilers, notably GCC 3.2, require these functions to be inside an inner class.
  struct internal_class
  {
    static big  is_base_class_(...);
    static char is_base_class_(typename type_trait<T_base>::pointer);
  };

public:
  static const bool value =
    sizeof(internal_class::is_base_class_(reinterpret_cast<typename type_trait<T_derived>::pointer>(0))) ==
    sizeof(char);

#else //SIGC_SELF_REFERENCE_IN_MEMBER_INITIALIZATION

  //The AIX xlC compiler does not like these 2 functions being in the inner class.
  //It says "The incomplete type "test" must not be used as a qualifier.
  //It does not seem necessary anyway. murrayc.
  static big  is_base_class_(...);
  static char is_base_class_(typename type_trait<T_base>::pointer);

public:
  static const bool value =
    sizeof(is_base_class_(reinterpret_cast<typename type_trait<T_derived>::pointer>(0))) ==
    sizeof(char);

#endif //SIGC_SELF_REFERENCE_IN_MEMBER_INITIALIZATION

  void avoid_gcc3_warning_(); //Not implemented. g++ 3.3.5 (but not 3.3.4, and not 3.4) warn that there are no public methods, even though there is a public variable.
};

template <class T_base>
struct is_base_and_derived<T_base, T_base>
{
  static const bool value = true;
};

} /* namespace sigc */

#endif /* _SIGC_TYPE_TRAIT_H_ */
