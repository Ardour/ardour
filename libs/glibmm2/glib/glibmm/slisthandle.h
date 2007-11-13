// -*- c++ -*-
#ifndef _GLIBMM_SLISTHANDLE_H
#define _GLIBMM_SLISTHANDLE_H

/* $Id: slisthandle.h,v 1.3 2003/04/21 17:39:41 murrayc Exp $ */

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

#include <glib/gslist.h>
#include <glibmm/containerhandle_shared.h>


namespace Glib
{

namespace Container_Helpers
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* Create and fill a GSList as efficient as possible.
 * This requires bidirectional iterators.
 */
template <class Bi, class Tr>
GSList* create_slist(Bi pbegin, Bi pend, Tr)
{
  GSList* head = 0;

  while(pend != pbegin)
  {
    // Use & to force a warning if the iterator returns a temporary object.
    const void *const item = Tr::to_c_type(*&*--pend);
    head = g_slist_prepend(head, const_cast<void*>(item));
  }

  return head;
}

/* Create a GSList from a 0-terminated input sequence.
 * Build it in reverse order and reverse the whole list afterwards,
 * because appending to the list would be horribly inefficient.
 */
template <class For, class Tr>
GSList* create_slist(For pbegin, Tr)
{
  GSList* head = 0;

  while(*pbegin)
  {
    // Use & to force a warning if the iterator returns a temporary object.
    const void *const item = Tr::to_c_type(*&*pbegin);
    head = g_slist_prepend(head, const_cast<void*>(item));
    ++pbegin;
  }

  return g_slist_reverse(head);
}


/* Convert from any container that supports bidirectional iterators.
 */
template <class Tr, class Cont>
struct SListSourceTraits
{
  static GSList* get_data(const Cont& cont)
    { return Glib::Container_Helpers::create_slist(cont.begin(), cont.end(), Tr()); }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

/* Convert from a 0-terminated array.  The Cont
 * argument must be a pointer to the first element.
 */
template <class Tr, class Cont>
struct SListSourceTraits<Tr,Cont*>
{
  static GSList* get_data(const Cont* array)
    { return (array) ? Glib::Container_Helpers::create_slist(array, Tr()) : 0; }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

template <class Tr, class Cont>
struct SListSourceTraits<Tr,const Cont*> : SListSourceTraits<Tr,Cont*>
{};

/* Convert from a 0-terminated array.  The Cont argument must be a pointer
 * to the first element.  For consistency, the array must be 0-terminated,
 * even though the array size is known at compile time.
 */
template <class Tr, class Cont, size_t N>
struct SListSourceTraits<Tr,Cont[N]>
{
  static GSList* get_data(const Cont* array)
    { return Glib::Container_Helpers::create_slist(array, array + (N - 1), Tr()); }

  static const Glib::OwnershipType initial_ownership = Glib::OWNERSHIP_SHALLOW;
};

template <class Tr, class Cont, size_t N>
struct SListSourceTraits<Tr,const Cont[N]> : SListSourceTraits<Tr,Cont[N]>
{};

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


/**
 * @ingroup ContHelpers
 * If a method takes this as an argument, or has this as a return type, then you can use a standard
 * container such as std::list or std::vector.
 */
template <class Tr>
class SListHandleIterator
{
public:
  typedef typename Tr::CppType        CppType;
  typedef typename Tr::CType          CType;

  typedef std::forward_iterator_tag   iterator_category;
  typedef CppType                     value_type;
  typedef ptrdiff_t                   difference_type;
  typedef value_type                  reference;
  typedef void                        pointer;

  explicit inline SListHandleIterator(const GSList* node);

  inline value_type                    operator*() const;
  inline SListHandleIterator<Tr> &     operator++();
  inline const SListHandleIterator<Tr> operator++(int);

  inline bool operator==(const SListHandleIterator<Tr>& rhs) const;
  inline bool operator!=(const SListHandleIterator<Tr>& rhs) const;

private:
  const GSList* node_;
};

} // namespace Container_Helpers


/**
 * @ingroup ContHandles
 */
template < class T, class Tr = Glib::Container_Helpers::TypeTraits<T> >
class SListHandle
{
public:
  typedef typename Tr::CppType  CppType;
  typedef typename Tr::CType    CType;

  typedef CppType               value_type;
  typedef size_t                size_type;
  typedef ptrdiff_t             difference_type;

  typedef Glib::Container_Helpers::SListHandleIterator<Tr>   const_iterator;
  typedef Glib::Container_Helpers::SListHandleIterator<Tr>   iterator;

  template <class Cont> inline
    SListHandle(const Cont& container);

  // Take over ownership of a GSList created by GTK+ functions.
  inline SListHandle(GSList* glist, Glib::OwnershipType ownership);

  // Copying clears the ownership flag of the source handle.
  inline SListHandle(const SListHandle<T,Tr>& other);

  ~SListHandle();

  inline const_iterator begin() const;
  inline const_iterator end()   const;

  template <class U> inline operator std::vector<U>() const;
  template <class U> inline operator std::deque<U>()  const;
  template <class U> inline operator std::list<U>()   const;

  template <class Cont> inline
    void assign_to(Cont& container) const;
  template <class Out> inline
    void copy(Out pdest) const;

  inline GSList* data()  const;
  inline size_t  size()  const;
  inline bool    empty() const;

private:
  GSList *                    pslist_;
  mutable Glib::OwnershipType ownership_;

  // No copy assignment.
  SListHandle<T,Tr>& operator=(const SListHandle<T,Tr>&);
};


/***************************************************************************/
/*  Inline implementation                                                  */
/***************************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Container_Helpers
{

/**** Glib::Container_Helpers::SListHandleIterator<> ***********************/

template <class Tr> inline
SListHandleIterator<Tr>::SListHandleIterator(const GSList* node)
:
  node_ (node)
{}

template <class Tr> inline
typename SListHandleIterator<Tr>::value_type SListHandleIterator<Tr>::operator*() const
{
  return Tr::to_cpp_type(static_cast<typename Tr::CTypeNonConst>(node_->data));
}

template <class Tr> inline
SListHandleIterator<Tr>& SListHandleIterator<Tr>::operator++()
{
  node_ = node_->next;
  return *this;
}

template <class Tr> inline
const SListHandleIterator<Tr> SListHandleIterator<Tr>::operator++(int)
{
  const SListHandleIterator<Tr> tmp (*this);
  node_ = node_->next;
  return tmp;
}

template <class Tr> inline
bool SListHandleIterator<Tr>::operator==(const SListHandleIterator<Tr>& rhs) const
{
  return (node_ == rhs.node_);
}

template <class Tr> inline
bool SListHandleIterator<Tr>::operator!=(const SListHandleIterator<Tr>& rhs) const
{
  return (node_ != rhs.node_);
}

} // namespace Container_Helpers


/**** Glib::SListHandle<> **************************************************/

template <class T, class Tr>
  template <class Cont>
inline
SListHandle<T,Tr>::SListHandle(const Cont& container)
:
  pslist_    (Glib::Container_Helpers::SListSourceTraits<Tr,Cont>::get_data(container)),
  ownership_ (Glib::Container_Helpers::SListSourceTraits<Tr,Cont>::initial_ownership)
{}

template <class T, class Tr> inline
SListHandle<T,Tr>::SListHandle(GSList* gslist, Glib::OwnershipType ownership)
:
  pslist_    (gslist),
  ownership_ (ownership)
{}

template <class T, class Tr> inline
SListHandle<T,Tr>::SListHandle(const SListHandle<T,Tr>& other)
:
  pslist_    (other.pslist_),
  ownership_ (other.ownership_)
{
  other.ownership_ = Glib::OWNERSHIP_NONE;
}

template <class T, class Tr>
SListHandle<T,Tr>::~SListHandle()
{
  if(ownership_ != Glib::OWNERSHIP_NONE)
  {
    if(ownership_ != Glib::OWNERSHIP_SHALLOW)
    {
      // Deep ownership: release each container element.
      for(GSList* node = pslist_; node != 0; node = node->next)
        Tr::release_c_type(static_cast<typename Tr::CTypeNonConst>(node->data));
    }
    g_slist_free(pslist_);
  }
}

template <class T, class Tr> inline
typename SListHandle<T,Tr>::const_iterator SListHandle<T,Tr>::begin() const
{
  return Glib::Container_Helpers::SListHandleIterator<Tr>(pslist_);
}

template <class T, class Tr> inline
typename SListHandle<T,Tr>::const_iterator SListHandle<T,Tr>::end() const
{
  return Glib::Container_Helpers::SListHandleIterator<Tr>(0);
}

template <class T, class Tr>
  template <class U>
inline
SListHandle<T,Tr>::operator std::vector<U>() const
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
SListHandle<T,Tr>::operator std::deque<U>() const
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
SListHandle<T,Tr>::operator std::list<U>() const
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
void SListHandle<T,Tr>::assign_to(Cont& container) const
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
void SListHandle<T,Tr>::copy(Out pdest) const
{
  std::copy(this->begin(), this->end(), pdest);
}

template <class T, class Tr> inline
GSList* SListHandle<T,Tr>::data() const
{
  return pslist_;
}

template <class T, class Tr> inline
size_t SListHandle<T,Tr>::size() const
{
  return g_slist_length(pslist_);
}

template <class T, class Tr> inline
bool SListHandle<T,Tr>::empty() const
{
  return (pslist_ == 0);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Glib


#endif /* _GLIBMM_SLISTHANDLE_H */

