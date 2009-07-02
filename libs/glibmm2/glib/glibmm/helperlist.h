// -*- c++ -*-
#ifndef _GLIBMM_HELPERLIST_H
#define _GLIBMM_HELPERLIST_H
/* $Id: helperlist.h 648 2008-03-29 18:53:44Z jjongsma $ */

/* helperlist.h
 *
 * Copyright 2002 The gtkmm Development Team
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

#include <glibmm/containers.h>

namespace Glib
{

// This class has some pure virtual methods which need to be implemented by derived classes.
template< typename T_Child, typename T_CppElement, typename T_Iterator >
class HelperList
{
public:
  HelperList()
  : gparent_(0)
  {}

  HelperList(GObject* gp) //We use gp instead of gparent because that can cause warnings about a shadowed member.
  : gparent_(gp)
  {}

  virtual ~HelperList()
  {}

  typedef T_Child value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;

  typedef T_Iterator iterator;
  typedef List_ConstIterator<iterator> const_iterator;
  typedef List_ReverseIterator<iterator> reverse_iterator;
  typedef List_ConstIterator<reverse_iterator> const_reverse_iterator;

  typedef T_CppElement element_type;

  typedef size_t difference_type;
  typedef size_t size_type;

  //These are implemented differently for each Helper List.
  virtual iterator erase(iterator) = 0;

  virtual void erase(iterator start, iterator stop)
  {
    while(start != stop)
      start = erase(start); //Implemented in derived class.
  }

  virtual void remove(const_reference) = 0;

  size_type size() const
  {
    return g_list_length(glist());
  }

  inline size_type max_size() { return size_type(-1); }
  inline bool empty() { return glist() == 0; }

  inline iterator begin()
    {return begin_();}
  inline iterator end()
    {return end_();}

  inline const_iterator begin() const
    { return const_iterator(begin_()); }
  inline const_iterator end() const
    { return const_iterator(end_()); }

  inline reverse_iterator rbegin()
    { return reverse_iterator(end_()); }
  inline reverse_iterator rend()
    { return reverse_iterator(begin_()); }

  inline const_reverse_iterator rbegin() const
    { return const_reverse_iterator(reverse_iterator(end_())); }
  inline const_reverse_iterator rend() const
    { return const_reverse_iterator(reverse_iterator(begin_())); }

  reference front() const
  {
    return *begin();
  }

  reference back() const
  {
    return *(--end());
  }

  reference operator[](size_type l) const
  {
    size_type j = 0;
    iterator i;
    for(i = begin(), j = 0; i != end(), j < l; ++i, ++j)
      ;
    return (*i);
  }

//  iterator find(const_reference w)
//  {
//    iterator i = begin();
//    for(i = begin(); i != end() && (*i != w); i++);
//    return i;
//  }
//
//  iterator find(Widget& w)
//  {
//    iterator i;
//    for (i = begin(); i != end() && ((*i)->$1() != &w); i++);
//    return i;
//  }

  //Derived classes might choose to reimplement these as public:
  inline void pop_front()
    { erase(begin()); }
  inline void pop_back()
    { erase(--end()); }

  void clear()
    { erase(begin(), end()); }

  GObject* gparent()
    { return gparent_; };
  const GObject* gparent() const
    { return gparent_; };

protected:
  virtual GList*& glist() const = 0;      // front of list

  iterator begin_() const
  {
    return iterator(glist(), glist());
  }

  iterator end_() const
  {
    return iterator(glist(), (GList*)0);
  }

  GObject* gparent_;
};


} /* namespace Glib */

#endif /* _GLIBMM_HELPERLIST_H */

