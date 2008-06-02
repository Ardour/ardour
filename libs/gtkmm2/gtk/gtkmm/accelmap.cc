// -*- c++ -*-
/* $Id$ */

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

#include <gtkmm/accelmap.h>
#include <gtk/gtkaccelmap.h>

namespace Gtk
{

namespace AccelMap
{

void add_entry(const std::string& accel_path, 
               guint accel_key, 
               Gdk::ModifierType accel_mods)
{
    gtk_accel_map_add_entry(accel_path.c_str(), accel_key,
                            (GdkModifierType)accel_mods);
}

bool change_entry(const std::string& accel_path, 
                  guint accel_key, 
                  Gdk::ModifierType accel_mods,
                  bool replace)
{
    return gtk_accel_map_change_entry(accel_path.c_str(), accel_key,
                                      (GdkModifierType)accel_mods, replace);
}

void load(const std::string& filename)
{
  gtk_accel_map_load(filename.c_str());
}

void save(const std::string& filename)
{
  gtk_accel_map_save(filename.c_str());
}

void lock_path(const std::string& accel_path)
{
  gtk_accel_map_lock_path(accel_path.c_str());
}

void unlock_path(const std::string& accel_path)
{
  gtk_accel_map_unlock_path(accel_path.c_str());
}

bool lookup_entry(const Glib::ustring& accel_path, Gtk::AccelKey& key)
{
  GtkAccelKey gkey = {GDK_VoidSymbol, GdkModifierType (0), 0};
  const bool known = gtk_accel_map_lookup_entry(accel_path.c_str(), &gkey);

  if(known)
    key = AccelKey(gkey.accel_key, Gdk::ModifierType (gkey.accel_mods));
  else
    key = AccelKey(GDK_VoidSymbol, Gdk::ModifierType (0));

  return known;
}

bool lookup_entry(const Glib::ustring& accel_path)
{
  return gtk_accel_map_lookup_entry(accel_path.c_str(), 0 /* "optional", according to the C docs. */);
}

} // namespace AccelMap

} // namespace Gtk

