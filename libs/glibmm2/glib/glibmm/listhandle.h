// -*- c++ -*-
#ifndef _GLIBMM_LISTHANDLE_H
#define _GLIBMM_LISTHANDLE_H

/* $Id: listhandle.h 779 2009-01-19 17:58:50Z murrayc $ */

/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glib.h>
#include <glibmm/containerhandle_shared.h>


namespace Glib
{

namespace Container_Helpers
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* Create and fill a GList as efficient as possible.
 * This requires bidirectional iterators.
 */
template <class Bi, class Tr>
GList* create_list(Bi pbegin, Bi pend, Tr)
{
  GList* head = 0;

  while(pend != pbegin)
  {
    // Use & to force a warning if the iterator returns a temporary object.
    const void *const item = Tr::to_c_type(*&*--pend);
    head = g_list_prepend(head, const_cast<void*>(item));
  }

  return head;
}

/* Create a GList from a 0-terminated input sequence.
 * Build it in reverse order and reverse the whole list afterwards,
 * because appending to the list would be horribly inefficient.
 */
template <class For, class Tr>
GList* create_list(For pbegin, Tr)
{
  GList* head = 0;

  while(*pbegin)
  {
    // Use & to force a warning if the iterator returns a temporary object.
    const void *const item = Tr::to_c_type(*&*pbegin);
    head = g_list_prepend(head, const_cast<void*>(item));
    ++pbegin;
  }

  return g_list_reverse(head);
}


/* Convert from any container that supports bidirectional iterators.
 */
template <class Tr, class Cont>
struct ListSourceTraits
{
  static GList* get_data(const Cont& cont)
    { return Glib::Container_Helpers::create_list(cont.begin(), cont.end(), Tr()); }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

/* Convert from a 0-terminated array.  The Cont
 * argument must be a pointer to the first element.
 */
template <class Tr, class Cont>
struct ListSourceTraits<Tr,Cont*>
{
  static GList* get_data(const Cont* array)
    { return (array) ? Glib::Container_Helpers::create_list(array, Tr()) : 0; }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

template <class Tr, class Cont>
struct ListSourceTraits<Tr,const Cont*> : ListSourceTraits<Tr,Cont*>
{};

/* Convert from a 0-terminated array.  The Cont argument must be a pointer
 * to the first element.  For consistency, the array must be 0-terminated,
 * even though the array size is known at compile time.
 */
template <class Tr, class Cont, size_t N>
struct ListSourceTraits<Tr,Cont[N]>
{
  static GList* get_data(const Cont* array)
    { return Glib::Container_Helpers::create_list(array, array + (N - 1), Tr()); }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

template <class Tr, class Cont, size_t N>
struct ListSourceTraits<Tr,const Cont[N]> : ListSourceTraits<Tr,Cont[N]>
{};

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


/**
 * @ingroup ContHelpers
 * If a method takes this as an argument, or has this as a return type, then you can use a standard
 * container such as std::list or std::vector.
 */
template <class Tr>
class ListHandleIterator
{
public:
  typedef typename Tr::CppType        CppType;
  typedef typename Tr::CType          CType;

  typedef std::forward_iterator_tag   iterator_category;
  typedef CppType                     value_type;
  typedef ptrdiff_t                   difference_type;
  typedef value_type                  reference;
  typedef void                        pointer;

  explicit inline ListHandleIterator(const GList* node);

  inline value_type                   operator*() const;
  inline ListHandleIterator<Tr> &     operator++();
  inline const ListHandleIterator<Tr> operator++(int);

  inline bool operator==(const ListHandleIterator<Tr>& rhs) const;
  inline bool operator!=(const ListHandleIterator<Tr>& rhs) const;

private:
  const GList* node_;
};

} // namespace Container_Helpers


/**
 * @ingroup ContHandles
 */
template < class T, class Tr = Glib::Container_Helpers::TypeTraits<T> >
class ListHandle
{
public:
  typedef typename Tr::CppType  CppType;
  typedef typename Tr::CType    CType;

  typedef CppType               value_type;
  typedef size_t                size_type;
  typedef ptrdiff_t             difference_type;

  typedef Glib::Container_Helpers::ListHandleIterator<Tr>  const_iterator;
  typedef Glib::Container_Helpers::ListHandleIterator<Tr>  iterator;

  template <class Cont> inline
    ListHandle(const Cont& container);

  // Take over ownership of an array created by GTK+ functions.
  inline ListHandle(GList* glist, Glib::OwnershipType ownership);

  // Copying clears the ownership flag of the source handle.
  inline ListHandle(const ListHandle<T,Tr>& other);

  ~ListHandle();

  inline const_iterator begin() const;
  inline const_iterator end()   const;

  template <class U> inline operator std::vector<U>() const;
  template <class U> inline operator std::deque<U>()  const;
  template <class U> inline operator std::list<U>()   const;

  template <class Cont> inline
    void assign_to(Cont& container) const;

  template <class Out> inline
    void copy(Out pdest) const;

  inline GList* data()  const;
  inline size_t size()  const;
  inline bool   empty() const;

private:
  GList *                     plist_;
  mutable Glib::OwnershipType ownership_;

  // No copy assignment.
  ListHandle<T,Tr>& operator=(const ListHandle<T,Tr>&);
};


/***************************************************************************/
/*  Inline implementation                                                  */
/***************************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Container_Helpers
{

/**** Glib::Container_Helpers::ListHandleIterator<> ************************/

template <class Tr> inline
ListHandleIterator<Tr>::ListHandleIterator(const GList* node)
:
  node_ (node)
{}

template <class Tr> inline
typename ListHandleIterator<Tr>::value_type ListHandleIterator<Tr>::operator*() const
{
  return Tr::to_cpp_type(static_cast<typename Tr::CTypeNonConst>(node_->data));
}

template <class Tr> inline
ListHandleIterator<Tr>& ListHandleIterator<Tr>::operator++()
{
  node_ = node_->next;
  return *this;
}

template <class Tr> inline
const ListHandleIterator<Tr> ListHandleIterator<Tr>::operator++(int)
{
  const ListHandleIterator<Tr> tmp (*this);
  node_ = node_->next;
  return tmp;
}

template <class Tr> inline
bool ListHandleIterator<Tr>::operator==(const ListHandleIterator<Tr>& rhs) const
{
  return (node_ == rhs.node_);
}

template <class Tr> inline
bool ListHandleIterator<Tr>::operator!=(const ListHandleIterator<Tr>& rhs) const
{
  return (node_ != rhs.node_);
}

} // namespace Container_Helpers


/**** Glib::ListHandle<> ***************************************************/

template <class T, class Tr>
  template <class Cont>
inline
ListHandle<T,Tr>::ListHandle(const Cont& container)
:
  plist_     (Glib::Container_Helpers::ListSourceTraits<Tr,Cont>::get_data(container)),
  ownership_ (Glib::Container_Helpers::ListSourceTraits<Tr,Cont>::initial_ownership)
{}

template <class T, class Tr> inline
ListHandle<T,Tr>::ListHandle(GList* glist, Glib::OwnershipType ownership)
:
  plist_     (glist),
  ownership_ (ownership)
{}

template <class T, class Tr> inline
ListHandle<T,Tr>::ListHandle(const ListHandle<T,Tr>& other)
:
  plist_     (other.plist_),
  ownership_ (other.ownership_)
{
  other.ownership_ = Glib::OWNERSHIP_NONE;
}

template <class T, class Tr>
ListHandle<T,Tr>::~ListHandle()
{
  if(ownership_ != Glib::OWNERSHIP_NONE)
  {
    if(ownership_ != Glib::OWNERSHIP_SHALLOW)
    {
      // Deep ownership: release each container element.
      for(GList* node = plist_; node != 0; node = node->next)
        Tr::release_c_type(static_cast<typename Tr::CTypeNonConst>(node->data));
    }
    g_list_free(plist_);
  }
}

template <class T, class Tr> inline
typename ListHandle<T,Tr>::const_iterator ListHandle<T,Tr>::begin() const
{
  return Glib::Container_Helpers::ListHandleIterator<Tr>(plist_);
}

template <class T, class Tr> inline
typename ListHandle<T,Tr>::const_iterator ListHandle<T,Tr>::end() const
{
  return Glib::Container_Helpers::ListHandleIterator<Tr>(0);
}

template <class T, class Tr>
  template <class U>
inline
ListHandle<T,Tr>::operator std::vector<U>() const
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
ListHandle<T,Tr>::operator std::deque<U>() const
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
ListHandle<T,Tr>::operator std::list<U>() const
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
void ListHandle<T,Tr>::assign_to(Cont& container) const
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
void ListHandle<T,Tr>::copy(Out pdest) const
{
  std::copy(this->begin(), this->end(), pdest);
}

template <class T, class Tr> inline
GList* ListHandle<T,Tr>::data() const
{
  return plist_;
}

template <class T, class Tr> inline
size_t ListHandle<T,Tr>::size() const
{
  return g_list_length(plist_);
}

template <class T, class Tr> inline
bool ListHandle<T,Tr>::empty() const
{
  return (plist_ == 0);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Glib


#endif /* _GLIBMM_LISTHANDLE_H */

