// -*- c++ -*-
/* $Id$ */

/* stockid.cc
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

#include <gtkmm/stockid.h>
#include <gtkmm/stock.h>


namespace Gtk
{

StockID::StockID()
{}

StockID::StockID(const BuiltinStockID& id)
:
  id_ (id.id)
{}

StockID::StockID(const Glib::ustring& id)
:
  id_ (id)
{}

StockID::StockID(const char* id)
:
  id_ ((id) ? id : "")
{}

StockID::~StockID()
{}

StockID::StockID(const StockID& other)
:
  id_ (other.id_)
{}

StockID& StockID::operator=(const StockID& other)
{
  id_ = other.id_;
  return *this;
}

bool StockID::equal(const StockID& rhs) const
{
  // Do an exact per-byte comparison:
  // Locale-dependent collation is probably not appropriate for stock IDs.
  return (id_.raw() == rhs.id_.raw());
}

Glib::ustring StockID::get_string() const
{
  return id_;
}

const char* StockID::get_c_str() const
{
  return id_.c_str();
}

} // namespace Gtk

