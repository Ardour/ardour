// -*- c++ -*-
#ifndef _GLIBMM_CONTAINERS_H
#define _GLIBMM_CONTAINERS_H

/* $Id: containers.h 749 2008-12-10 14:23:33Z jjongsma $ */

/* containers.h
 *
 * Copyright (C) 1998-2002 The gtkmm Development Team
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

#include <glib.h>
#include <glibmm/sarray.h> /* for backward compatibility */

#include <iterator>
#include <glibmmconfig.h>

GLIBMM_USING_STD(bidirectional_iterator_tag)
GLIBMM_USING_STD(forward_iterator_tag)


#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Glib
{

template <class T> class List_Iterator;
template <class T> class List_ConstIterator;
template <class T> class List_ReverseIterator;

// Most of these methods in the non-template classes needs to be moved
// to implementation.

//Daniel Elstner has ideas about generating these per-widget with m4. murrayc.


extern GLIBMM_API gpointer glibmm_null_pointer;

template <class T>
class List_Iterator_Base
{
public:
  typedef T  value_type;
  typedef T* pointer;
  typedef T& reference;
} ;

///For instance, List_Iterator< Gtk::Widget >
template <class T>
class List_Iterator : public List_Iterator_Base<T>
{
public:
  typedef std::bidirectional_iterator_tag iterator_category;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  
  typedef typename List_Iterator_Base<T>::pointer pointer;
  typedef typename List_Iterator_Base<T>::reference reference;

  GList* const* head_;
  GList* node_;

  typedef List_Iterator<T> Self;

  List_Iterator(GList* const& head, GList* node)
  : head_(&head), node_(node)
  {}

  List_Iterator()
   : head_(0), node_(0)
  {}

  List_Iterator(const Self& src)
  : head_(src.head_), node_(src.node_)
  {}

  bool operator==(const Self& src) const { return node_ == src.node_; }
  bool operator!=(const Self& src) const { return node_ != src.node_; }

  Self&  operator++()
  {
    if (!node_)
      node_ = g_list_first(*head_);
    else
      node_ = (GList*)g_list_next(node_);
    return *this;
  }

  Self operator++(int)
  {
    Self tmp = *this;
    ++*this;
    return tmp;
  }

  Self&  operator--()
  {
    if (!node_)
      node_ = g_list_last(*head_);
    else
      node_ = (GList*)g_list_previous(node_);

    return *this;
  }

  Self operator--(int)
  {
    Self tmp = *this;
    --*this;
    return tmp;
  }

  reference operator*()  const 
  {
    return *(pointer)( node_ ? node_->data : glibmm_null_pointer );
  }
  
  pointer operator -> () const { return &operator*(); }
};

///For instance, SList_Iterator< Gtk::Widget >
template <class T>
class SList_Iterator : public List_Iterator_Base<T>
{
public:
  typedef std::forward_iterator_tag iterator_category;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef typename List_Iterator_Base<T>::pointer pointer;
  typedef typename List_Iterator_Base<T>::reference reference;

  GSList* node_;
  typedef SList_Iterator<T> Self;

  SList_Iterator(GSList* node)
   : node_(node)
   {}

  SList_Iterator()
   : node_(0)
   {}

  SList_Iterator(const Self& src)
  : node_(src.node_)
  {}

  bool operator==(const Self& src) const { return node_ == src.node_; }
  bool operator!=(const Self& src) const { return node_ != src.node_; }

  Self&  operator++()
  {
    node_ = g_slist_next(node_);
    return *this;
  }

  Self operator++(int)
  {
    Self tmp = *this;
    ++*this;
    return tmp;
  }

  reference operator*()  const
  {
    //g++ complains that this statement has no effect: g_assert(node_);
    return reinterpret_cast<T&>( node_ ? node_->data : glibmm_null_pointer );
  }

  pointer operator -> () const { return &operator*(); }
};


// This iterator variation returns T_IFace (wrapped from T_Impl)
//  For instance,  List_Cpp_Iterator<GtkWidget, Gtk::Widget> is a little like std::list<Gtk::Widget>::iterator
template<class T_Impl, class T_IFace>
class List_Cpp_Iterator : public List_Iterator_Base<T_IFace>
{
public:
  typedef std::bidirectional_iterator_tag iterator_category;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef typename List_Iterator_Base<T_IFace>::pointer pointer;
  typedef typename List_Iterator_Base<T_IFace>::reference reference;

  typedef List_Cpp_Iterator<T_Impl, T_IFace> Self;

  GList** head_;
  GList* node_;

  bool operator==(const Self& src) const { return node_ == src.node_; }
  bool operator!=(const Self& src) const { return node_ != src.node_; }

  List_Cpp_Iterator(GList*& head, GList* node )
  : head_(&head), node_(node )
  {}

  List_Cpp_Iterator()
  : head_(0), node_(0)
  {}

  List_Cpp_Iterator(const Self& src)
  : head_(src.head_), node_(src.node_)
  {}

  reference operator*() const
  {
    if (node_ && node_->data)
    {
      //We copy/paste the widget wrap() implementation here,
      //because we can not use a specific Glib::wrap(T_Impl) overload here,
      //because that would be "dependent", and g++ 3.4 does not allow that.
      //The specific Glib::wrap() overloads don't do anything special anyway.
      GObject* cobj = static_cast<GObject*>( (*node_).data );
      
      #ifdef GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION
      return *(dynamic_cast<pointer>(Glib::wrap_auto(cobj, false /* take_copy */)));
      #else
      //We really do need to use dynamic_cast<>, so I expect problems if this code is used. murrayc.
      return *(static_cast<pointer>(Glib::wrap_auto(cobj, false /* take_copy */)));
      #endif
      
    }
    
    return *(pointer)glibmm_null_pointer;
  }

  pointer operator->() const { return &operator*(); }

  Self&  operator++()
  {
    if (!node_)
      node_ = g_list_first(*head_);
    else
      node_ = (GList *)g_list_next(node_);

    return *this;
  }

  Self operator++(int)
  {
    Self tmp = *this;
    ++*this;
    return tmp;
  }

  Self&  operator--()
  {
    if (!node_)
      node_ = g_list_last(*head_);
    else
      node_ = (GList *)g_list_previous(node_);

    return *this;
  }

  Self operator--(int)
  {
    Self tmp = *this;
    --*this;
    return tmp;
  }

};

template <class T_Base>
class List_ReverseIterator: private T_Base
{
public:
  typedef typename T_Base::iterator_category iterator_category;
  typedef typename T_Base::size_type         size_type;
  typedef typename T_Base::difference_type   difference_type;

  typedef typename T_Base::value_type        value_type;
  typedef typename T_Base::pointer           pointer;
  typedef typename T_Base::reference         reference;

  typedef List_ReverseIterator<T_Base>    Self;

  bool operator==(const Self& src) const { return T_Base::operator==(src); }
  bool operator!=(const Self& src) const { return T_Base::operator!=(src); }

  List_ReverseIterator(GList* const& head, GList* node)
   : T_Base(head, node)
  {}

  List_ReverseIterator()
   : T_Base()
  {}

  List_ReverseIterator(const Self& src)
  : T_Base(src)
  {}

  List_ReverseIterator(const T_Base& src)
  : T_Base(src)
  { ++(*this); }


  Self& operator++()   {T_Base::operator--(); return *this;}
  Self& operator--()   {T_Base::operator++(); return *this;}
  Self operator++(int) {Self src = *this; T_Base::operator--(); return src;}
  Self operator--(int) {Self src = *this; T_Base::operator++(); return src;}

  reference operator*() const { return T_Base::operator*(); }
  pointer operator->()  const { return T_Base::operator->(); }
};

template <class T_Base>
class List_ConstIterator: public T_Base
{
public:
  typedef typename T_Base::iterator_category iterator_category;
  typedef typename T_Base::size_type         size_type;
  typedef typename T_Base::difference_type   difference_type;

  typedef const typename T_Base::value_type  value_type;
  typedef const typename T_Base::pointer     pointer;
  typedef const typename T_Base::reference   reference;

  typedef List_ConstIterator<T_Base> Self;

  bool operator==(const Self& src) const { return T_Base::operator==(src); }
  bool operator!=(const Self& src) const { return T_Base::operator!=(src); }

  List_ConstIterator(GList* const& head, GList* node)
  : T_Base(head, node)
  {}

  List_ConstIterator()
  : T_Base()
  {}

  List_ConstIterator(const Self& src)
  : T_Base(src)
  {}

  List_ConstIterator(const T_Base& src)
  : T_Base(src)
  {}

  Self& operator++()   {T_Base::operator++(); return *this;}
  Self& operator--()   {T_Base::operator--(); return *this;}
  Self operator++(int) {Self src = *this; T_Base::operator++(); return src;}
  Self operator--(int) {Self src = *this; T_Base::operator--(); return src;}

  reference operator*() const { return T_Base::operator*(); }
  pointer operator->()   const { return T_Base::operator->(); }
};

} // namespace Glib

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* _GLIBMM_CONTAINERS_H */

