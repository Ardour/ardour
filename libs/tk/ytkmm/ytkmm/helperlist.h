/* helperlist.h
 *
 * Copyright 2002 The gtkmm Development Team
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
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <glibmm/containers.h>

namespace Ytkmm
{

// This class has some pure virtual methods which need to be implemented by derived classes.

template <typename T_Child, typename T_CppElement, typename T_Iterator>
class HelperList
{
public:
  HelperList() : gparent_(nullptr) {}

  HelperList(GObject*
      gp) // We use gp instead of gparent because that can cause warnings about a shadowed member.
    : gparent_(gp)
  {
  }

  virtual ~HelperList() noexcept {}

  using value_type = T_Child;
  using reference = value_type&;
  using const_reference = const value_type&;

  using iterator = T_Iterator;
  using const_iterator = Glib::List_ConstIterator<iterator>;
  using reverse_iterator = Glib::List_ReverseIterator<iterator>;
  using const_reverse_iterator = Glib::List_ConstIterator<reverse_iterator>;

  using element_type = T_CppElement;

  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

  // These are implemented differently for each Helper List.
  virtual iterator erase(iterator) = 0;

  virtual void erase(iterator start, iterator stop)
  {
    while (start != stop)
      start = erase(start); // Implemented in derived class.
  }

  virtual void remove(const_reference) = 0;

  size_type size() const { return g_list_length(glist()); }

  inline size_type max_size() { return size_type(-1); }
  inline bool empty() { return glist() == nullptr; }

  inline iterator begin() { return begin_(); }
  inline iterator end() { return end_(); }

  inline const_iterator begin() const { return const_iterator(begin_()); }
  inline const_iterator end() const { return const_iterator(end_()); }

  inline reverse_iterator rbegin() { return reverse_iterator(end_()); }
  inline reverse_iterator rend() { return reverse_iterator(begin_()); }

  inline const_reverse_iterator rbegin() const
  {
    return const_reverse_iterator(reverse_iterator(end_()));
  }
  inline const_reverse_iterator rend() const
  {
    return const_reverse_iterator(reverse_iterator(begin_()));
  }

  reference front() const { return *begin(); }

  reference back() const { return *(--end()); }

  reference operator[](size_type l) const
  {
    size_type j = 0;
    iterator i;
    for (i = begin(), j = 0; i != end() && j < l; ++i, ++j)
      ;
    return (*i);
  }

  // Derived classes might choose to reimplement these as public:
  inline void pop_front() { erase(begin()); }
  inline void pop_back() { erase(--end()); }

  void clear() { erase(begin(), end()); }

  GObject* gparent() { return gparent_; };
  const GObject* gparent() const { return gparent_; };

protected:
  virtual GList*& glist() const = 0; // front of list

  iterator begin_() const { return iterator(glist(), glist()); }

  iterator end_() const { return iterator(glist(), (GList*)nullptr); }

  GObject* gparent_;
};

} /* namespace Ytkmm */
