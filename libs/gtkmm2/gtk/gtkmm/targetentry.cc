// -*- c++ -*-
/* $Id$ */

/* targetentry.cc
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

#include <gtkmm/targetentry.h>
#include <cstring>

namespace Gtk
{

TargetEntry::TargetEntry()
{
  memset(&gobject_, 0, sizeof(gobject_));
}

TargetEntry::TargetEntry(const Glib::ustring& target, Gtk::TargetFlags flags, guint info)
{
  set_target(target);
  set_flags(flags);
  set_info(info);
}

TargetEntry::TargetEntry(const GtkTargetEntry& gobject)
{
  set_target(gobject.target);
  set_info(gobject.info);
  set_flags(TargetFlags(gobject.flags));
}

TargetEntry::TargetEntry(const TargetEntry& src)
{
  set_target(src.get_target());
  set_info(src.get_info());
  set_flags(src.get_flags());
}

TargetEntry::~TargetEntry()
{
  g_free(gobject_.target);
}

TargetEntry& TargetEntry::operator=(const TargetEntry& src)
{
  if(&src != this)
  {
    set_target(src.get_target());
    set_info(src.get_info());
    set_flags(src.get_flags());
  }

  return *this;
}

Glib::ustring TargetEntry::get_target() const
{
  return gobject_.target;
}

void TargetEntry::set_target(const Glib::ustring& target)
{
  gobject_.target = g_strdup(target.c_str());
}

Gtk::TargetFlags TargetEntry::get_flags() const
{
  return (Gtk::TargetFlags)(gobject_.flags);
}

void TargetEntry::set_flags(Gtk::TargetFlags flags)
{
  gobject_.flags = (guint)(flags);
}

guint TargetEntry::get_info() const
{
  return gobject_.info;
}

void TargetEntry::set_info(guint info)
{
  gobject_.info = info;
}

GtkTargetEntry* TargetEntry::gobj()
{
  return &gobject_;
}

const GtkTargetEntry* TargetEntry::gobj() const
{
  return &gobject_;
}

} /* namespace Gtk */

