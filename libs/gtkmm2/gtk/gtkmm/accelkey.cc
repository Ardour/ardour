// -*- c++ -*-
/* $Id$ */

/* 
 *
 * Copyright 1998-2002 The gtkmm Development Team
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

#include <gtkmm/accelkey.h>
#include <gtkmm/accelgroup.h>

namespace Gtk
{

AccelKey::AccelKey()
: key_(GDK_VoidSymbol),
  mod_((Gdk::ModifierType)0)
{
}

AccelKey::AccelKey(guint accel_key, Gdk::ModifierType accel_mods,
                   const Glib::ustring& accel_path)
: key_(accel_key),
  mod_(accel_mods),
  path_(accel_path)
{
}

AccelKey::AccelKey(const Glib::ustring& accelerator, const Glib::ustring& accel_path)
: path_(accel_path)
{
  //Get the key and mod by parsing the accelerator string:
  AccelGroup::parse (accelerator, key_, mod_);
}

AccelKey::AccelKey(const AccelKey& src)
{
  key_ = src.key_;
  mod_ = src.mod_;
  path_ = src.path_;
}

AccelKey& AccelKey::operator=(const AccelKey& src)
{
  key_ = src.key_;
  mod_ = src.mod_;
  path_ = src.path_;

  return *this;
}

guint AccelKey::get_key() const
{
  return key_;
}

Gdk::ModifierType AccelKey::get_mod() const
{
  return mod_;
}

Glib::ustring AccelKey::get_path() const
{
  return path_;
}

bool AccelKey::is_null() const
{
  return ( (key_ == GDK_VoidSymbol) || !(get_key() > 0) ); //both seem to be invalid.
}

Glib::ustring AccelKey::get_abbrev() const
{
  return AccelGroup::name (key_, mod_);
}


} // namespace

