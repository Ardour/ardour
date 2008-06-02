// -*- c++ -*-
#ifndef _GLIBMM_ARRAYHANDLE_H
#define _GLIBMM_ARRAYHANDLE_H

/* $Id: arrayhandle.h 32 2003-04-21 17:39:41Z murrayc $ */

/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/containerhandle_shared.h>


namespace Glib
{

namespace Container_Helpers
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* Count the number of elements in a 0-terminated sequence.
 */
template <class T> inline
size_t compute_array_size(const T* array)
{
  const T* pend = array;

  while(*pend)
    ++pend;

  return (pend - array);
}

/* Allocate and fill a 0-terminated array.  The size argument
 * specifies the number of elements in the input sequence.
 */
template <class For, class Tr>
typename Tr::CType* create_array(For pbegin, size_t size, Tr)
{
  typedef typename Tr::CType CType;

  CType *const array = static_cast<CType*>(g_malloc((size + 1) * sizeof(CType)));
  CType *const array_end = array + size;

  for(CType* pdest = array; pdest != array_end; ++pdest)
  {
    // Use & to force a warning if the iterator returns a temporary object.
    *pdest = Tr::to_c_type(*&*pbegin);
    ++pbegin;
  }

  *array_end = CType();
  return array;
}


/* Convert from any container that supports forward
 * iterators and has a size() method.
 */
template <class Tr, class Cont>
struct ArraySourceTraits
{
  typedef typename Tr::CType CType;

  static size_t get_size(const Cont& cont)
    { return cont.size(); }

  static const CType* get_data(const Cont& cont, size_t size)
    { return Glib::Container_Helpers::create_array(cont.begin(), size, Tr()); }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

/* Convert from a 0-terminated array.  The Cont argument must be a pointer
 * to the first element.  Note that only arrays of the C type are supported.
 */
template <class Tr, class Cont>
struct ArraySourceTraits<Tr,Cont*>
{
  typedef typename Tr::CType CType;

  static size_t get_size(const CType* array)
    { return (array) ? Glib::Container_Helpers::compute_array_size(array) : 0; }

  static const CType* get_data(const CType* array, size_t)
    { return array; }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_NONE;
};

template <class Tr, class Cont>
struct ArraySourceTraits<Tr,const Cont*> : ArraySourceTraits<Tr,Cont*>
{};

/* Convert from a 0-terminated array.  The Cont argument must be a pointer
 * to the first element.  Note that only arrays of the C type are supported.
 * For consistency, the array must be 0-terminated, even though the array
 * size is known at compile time.
 */
template <class Tr, class Cont, size_t N>
struct ArraySourceTraits<Tr,Cont[N]>
{
  typedef typename Tr::CType CType;

  static size_t get_size(const CType*)
    { return (N - 1); }

  static const CType* get_data(const CType* array, size_t)
    { return array; }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_NONE;
};

template <class Tr, class Cont, size_t N>
struct ArraySourceTraits<Tr,const Cont[N]> : ArraySourceTraits<Tr,Cont[N]>
{};

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


/**
 * @ingroup ContHelpers
 */
template <class Tr>
class ArrayHandleIterator
{
public:
  typedef typename Tr::CppType              CppType;
  typedef typename Tr::CType                CType;

  typedef std::random_access_iterator_tag   iterator_category;
  typedef CppType                           value_type;
  typedef ptrdiff_t                         difference_type;
  typedef value_type                        reference;
  typedef void                              pointer;

  explicit inline ArrayHandleIterator(const CType* pos);

  inline value_type operator*() const;
  inline value_type operator[](difference_type offset) const;

  inline ArrayHandleIterator<Tr> &     operator++();
  inline const ArrayHandleIterator<Tr> operator++(int);

  // All this random access stuff is only there because STL algorithms
  // usually have optimized specializations for random access iterators,
  // and we don't want to give away efficiency for nothing.
  //
  inline ArrayHandleIterator<Tr> &     operator+=(difference_type rhs);
  inline ArrayHandleIterator<Tr> &     operator-=(difference_type rhs);
  inline const ArrayHandleIterator<Tr> operator+ (difference_type rhs) const;
  inline const ArrayHandleIterator<Tr> operator- (difference_type rhs) const;
  inline difference_type operator-(const ArrayHandleIterator<Tr>& rhs) const;

  inline bool operator==(const ArrayHandleIterator<Tr>& rhs) const;
  inline bool operator!=(const ArrayHandleIterator<Tr>& rhs) const;
  inline bool operator< (const ArrayHandleIterator<Tr>& rhs) const;
  inline bool operator> (const ArrayHandleIterator<Tr>& rhs) const;
  inline bool operator<=(const ArrayHandleIterator<Tr>& rhs) const;
  inline bool operator>=(const ArrayHandleIterator<Tr>& rhs) const;

private:
  const CType* pos_;
};

} // namespace Container_Helpers


/** If a method takes this as an argument, or has this as a return type, then you can use a standard
 * container such as std::list or std::vector.
 * @ingroup ContHandles
 */
template < class T, class Tr = Glib::Container_Helpers::TypeTraits<T> >
class ArrayHandle
{
public:
  typedef typename Tr::CppType  CppType;
  typedef typename Tr::CType    CType;

  typedef CppType               value_type;
  typedef size_t                size_type;
  typedef ptrdiff_t             difference_type;

  typedef Glib::Container_Helpers::ArrayHandleIterator<Tr>   const_iterator;
  typedef Glib::Container_Helpers::ArrayHandleIterator<Tr>   iterator;

  template <class Cont> inline
    ArrayHandle(const Cont& container);

  // Take over ownership of an array created by GTK+ functions.
  inline ArrayHandle(const CType* array, size_t array_size, Glib::OwnershipType ownership);
  inline ArrayHandle(const CType* array, Glib::OwnershipType ownership);

  // Copying clears the ownership flag of the source handle.
  inline ArrayHandle(const ArrayHandle<T,Tr>& other);

  ~ArrayHandle();

  inline const_iterator begin() const;
  inline const_iterator end()   const;

  template <class U> inline operator std::vector<U>() const;
  template <class U> inline operator std::deque<U>()  const;
  template <class U> inline operator std::list<U>()   const;

  template <class Cont> inline
    void assign_to(Cont& container) const;

  template <class Out> inline
    void copy(Out pdest) const;

  inline const CType* data()  const;
  inline size_t       size()  const;
  inline bool         empty() const;

private:
  size_t                      size_;
  const CType*                parray_;
  mutable Glib::OwnershipType ownership_;

  // No copy assignment.
  ArrayHandle<T, Tr>& operator=(const ArrayHandle<T,Tr>&);
};

/** If a method takes this as an argument, or has this as a return type, then you can use a standard
 * container such as std::list<Glib::ustring> or std::vector<Glib::ustring>.
 * @ingroup ContHandles
 */
typedef ArrayHandle<Glib::ustring> StringArrayHandle;


/***************************************************************************/
/*  Inline implementation                                                  */
/***************************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Container_Helpers
{

/**** Glib::Container_Helpers::ArrayHandleIterator<> ***********************/

template <class Tr> inline
ArrayHandleIterator<Tr>::ArrayHandleIterator(const CType* pos)
:
  pos_ (pos)
{}

template <class Tr> inline
typename ArrayHandleIterator<Tr>::value_type ArrayHandleIterator<Tr>::operator*() const
{
  return Tr::to_cpp_type(*pos_);
}

template <class Tr> inline
typename ArrayHandleIterator<Tr>::value_type
ArrayHandleIterator<Tr>::operator[](difference_type offset) const
{
  return Tr::to_cpp_type(pos_[offset]);
}

template <class Tr> inline
ArrayHandleIterator<Tr>& ArrayHandleIterator<Tr>::operator++()
{
  ++pos_;
  return *this;
}

template <class Tr> inline
const ArrayHandleIterator<Tr> ArrayHandleIterator<Tr>::operator++(int)
{
  return ArrayHandleIterator<Tr>(pos_++);
}

template <class Tr> inline
ArrayHandleIterator<Tr>&
ArrayHandleIterator<Tr>::operator+=(typename ArrayHandleIterator<Tr>::difference_type rhs)
{
  pos_ += rhs;
  return *this;
}

template <class Tr> inline
ArrayHandleIterator<Tr>&
ArrayHandleIterator<Tr>::operator-=(typename ArrayHandleIterator<Tr>::difference_type rhs)
{
  pos_ -= rhs;
  return *this;
}

template <class Tr> inline
const ArrayHandleIterator<Tr>
ArrayHandleIterator<Tr>::operator+(typename ArrayHandleIterator<Tr>::difference_type rhs) const
{
  return ArrayHandleIterator<Tr>(pos_ + rhs);
}

template <class Tr> inline
const ArrayHandleIterator<Tr>
ArrayHandleIterator<Tr>::operator-(typename ArrayHandleIterator<Tr>::difference_type rhs) const
{
  return ArrayHandleIterator<Tr>(pos_ - rhs);
}

template <class Tr> inline
typename ArrayHandleIterator<Tr>::difference_type
ArrayHandleIterator<Tr>::operator-(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ - rhs.pos_);
}

template <class Tr> inline
bool ArrayHandleIterator<Tr>::operator==(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ == rhs.pos_);
}

template <class Tr> inline
bool ArrayHandleIterator<Tr>::operator!=(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ != rhs.pos_);
}

template <class Tr> inline
bool ArrayHandleIterator<Tr>::operator<(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ < rhs.pos_);
}

template <class Tr> inline
bool ArrayHandleIterator<Tr>::operator>(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ > rhs.pos_);
}

template <class Tr> inline
bool ArrayHandleIterator<Tr>::operator<=(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ <= rhs.pos_);
}

template <class Tr> inline
bool ArrayHandleIterator<Tr>::operator>=(const ArrayHandleIterator<Tr>& rhs) const
{
  return (pos_ >= rhs.pos_);
}

} // namespace Container_Helpers


/**** Glib::ArrayHandle<> **************************************************/

template <class T, class Tr>
  template <class Cont>
inline
ArrayHandle<T,Tr>::ArrayHandle(const Cont& container)
:
  size_      (Glib::Container_Helpers::ArraySourceTraits<Tr,Cont>::get_size(container)),
  parray_    (Glib::Container_Helpers::ArraySourceTraits<Tr,Cont>::get_data(container, size_)),
  ownership_ (Glib::Container_Helpers::ArraySourceTraits<Tr,Cont>::initial_ownership)
{}

template <class T, class Tr> inline
ArrayHandle<T,Tr>::ArrayHandle(const typename ArrayHandle<T,Tr>::CType* array, size_t array_size,
                               Glib::OwnershipType ownership)
:
  size_      (array_size),
  parray_    (array),
  ownership_ (ownership)
{}

template <class T, class Tr> inline
ArrayHandle<T,Tr>::ArrayHandle(const typename ArrayHandle<T,Tr>::CType* array,
                               Glib::OwnershipType ownership)
:
  size_      ((array) ? Glib::Container_Helpers::compute_array_size(array) : 0),
  parray_    (array),
  ownership_ (ownership)
{}

template <class T, class Tr> inline
ArrayHandle<T,Tr>::ArrayHandle(const ArrayHandle<T,Tr>& other)
:
  size_      (other.size_),
  parray_    (other.parray_),
  ownership_ (other.ownership_)
{
  other.ownership_ = Glib::OWNERSHIP_NONE;
}

template <class T, class Tr>
ArrayHandle<T,Tr>::~ArrayHandle()
{
  if(ownership_ != Glib::OWNERSHIP_NONE)
  {
    if(ownership_ != Glib::OWNERSHIP_SHALLOW)
    {
      // Deep ownership: release each container element.
      const CType *const pend = parray_ + size_;
      for(const CType* p = parray_; p != pend; ++p)
        Tr::release_c_type(*p);
    }
    g_free(const_cast<CType*>(parray_));
  }
}

template <class T, class Tr> inline
typename ArrayHandle<T,Tr>::const_iterator ArrayHandle<T,Tr>::begin() const
{
  return Glib::Container_Helpers::ArrayHandleIterator<Tr>(parray_);
}

template <class T, class Tr> inline
typename ArrayHandle<T,Tr>::const_iterator ArrayHandle<T,Tr>::end() const
{
  return Glib::Container_Helpers::ArrayHandleIterator<Tr>(parray_ + size_);
}

template <class T, class Tr>
  template <class U>
inline
ArrayHandle<T,Tr>::operator std::vector<U>() const
{
#ifdef GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS
  return std::vector<U>(this->begin(), this->end());
#else
  std::vector<U> temp;
  temp.reserve(this->size());
  Glib::Container_Helpers::fill_container(temp, this->begin(), this->end());
  return temp;
#endif
}

template <class T, class Tr>
  template <class U>
inline
ArrayHandle<T,Tr>::operator std::deque<U>() const
{
#ifdef GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS
  return std::deque<U>(this->begin(), this->end());
#else
  std::deque<U> temp;
  Glib::Container_Helpers::fill_container(temp, this->begin(), this->end());
  return temp;
#endif
}

template <class T, class Tr>
  template <class U>
inline
ArrayHandle<T,Tr>::operator std::list<U>() const
{
#ifdef GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS
  return std::list<U>(this->begin(), this->end());
#else
  std::list<U> temp;
  Glib::Container_Helpers::fill_container(temp, this->begin(), this->end());
  return temp;
#endif
}

template <class T, class Tr>
  template <class Cont>
inline
void ArrayHandle<T,Tr>::assign_to(Cont& container) const
{
#ifdef GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS
  container.assign(this->begin(), this->end());
#else
  Cont temp;
  Glib::Container_Helpers::fill_container(temp, this->begin(), this->end());
  container.swap(temp);
#endif
}

template <class T, class Tr>
  template <class Out>
inline
void ArrayHandle<T,Tr>::copy(Out pdest) const
{
  std::copy(this->begin(), this->end(), pdest);
}

template <class T, class Tr> inline
const typename ArrayHandle<T,Tr>::CType* ArrayHandle<T,Tr>::data() const
{
  return parray_;
}

template <class T, class Tr> inline
size_t ArrayHandle<T,Tr>::size() const
{
  return size_;
}

template <class T, class Tr> inline
bool ArrayHandle<T,Tr>::empty() const
{
  return (size_ == 0);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Glib


#endif /* _GLIBMM_ARRAYHANDLE_H */

