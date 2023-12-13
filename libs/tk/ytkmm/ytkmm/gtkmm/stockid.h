// -*- c++ -*-
#ifndef _GTKMM_STOCKID_H
#define _GTKMM_STOCKID_H

/* $Id$ */

/* stockid.h
 *
 * Copyright (C) 2002 The gtkmm Development Team
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <glibmm.h>
#pragma GCC diagnostic pop

namespace Gtk
{

struct BuiltinStockID;

/** See also Gtk::BuiltinStockID.
 */
class StockID
{
public:
  /** Create an empty StockID
   */
  StockID(); //TODO: This was added for Action::Action, but there might be a better way to do this.

  /** Create a StockID from one of the build-in stock ids.
   *
   * See also Gtk::BuildinStockID.
   */
  StockID(const BuiltinStockID& id);

  /** Create a StockID from its string representation.
   * @param id string representation of the stock id. Usually something like "gtk-about".
   */
  explicit StockID(const Glib::ustring& id);

  /** Create a StockID from its string representation.
   * @param id string representation of the stock id. Usually something like "gtk-about".
   *
   * If id is 0 an empty StockID will be created.
   */  
  explicit StockID(const char* id);
  ~StockID();

  /** Create a StockID as copy from another.
   * @param other: StockID to copy.
   */
  StockID(const StockID& other);

  /** Check if the StockIDs are equal.
   * @param other Another StockID.
   */
  StockID& operator=(const StockID& other);

  /** Tests whether the StockID is not empty.
   */
  operator bool() const;

  /** Check if two StockIDs are equal.
   * @param rhs Another StockID.
   *
   * @return <tt>true</tt> if both ids equal - <tt>false</tt> otherwise.
   */
  bool equal(const StockID& rhs) const;

  /** Get the string representation of the StockID.
   *
   * @return something like "gtk-about".
   */
  Glib::ustring get_string() const;

  /** Get the string representation as a const gchar*.
   *
   * @return string representation as const gchar*.
   */
  const char*   get_c_str()  const;

protected:
  Glib::ustring id_;
};

/** @relates Gtk::StockID */
inline bool operator==(const StockID& lhs, const StockID& rhs)
  { return lhs.equal(rhs); }

/** @relates Gtk::StockID */
inline bool operator!=(const StockID& lhs, const StockID& rhs)
  { return !lhs.equal(rhs); }


#ifndef DOXYGEN_SHOULD_SKIP_THIS
struct StockID_Traits : public Glib::Container_Helpers::TypeTraits<Glib::ustring>
{
  typedef Gtk::StockID CppType;

  static const char* to_c_type(const StockID& id) { return id.get_c_str(); }
  static StockID     to_cpp_type(const char* str) { return StockID(str);   }
};
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Gtk


#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Glib
{

template <>
class Value<Gtk::StockID> : public Glib::ValueBase_String
{
public:
  typedef Gtk::StockID CppType;

  void set(const Gtk::StockID& data) { set_cstring(data.get_c_str());      }
  Gtk::StockID get() const           { return Gtk::StockID(get_cstring()); }
};

} // namespace Glib
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


#endif /* _GTKMM_STOCKID_H */

