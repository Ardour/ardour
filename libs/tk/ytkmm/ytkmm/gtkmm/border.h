// -*- c++ -*-
#ifndef _GTKMM_BORDER_H
#define _GTKMM_BORDER_H

/* border.h
 *
 * Copyright (C) 2009 The gtkmm Development Team
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

#include <glibmm/value.h>
#include <gtk/gtk.h>  /* For GtkBorder */

namespace Gtk
{

typedef GtkBorder Border;

} /* namespace Gtk */


#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Glib
{

template <>
class Value<Gtk::Border> : public ValueBase_Boxed
{
public:
  typedef Gtk::Border CppType;

  static GType value_type() { return gtk_border_get_type(); }

  void set(const Gtk::Border& data) { set_boxed(&data); }
  Gtk::Border get() const
  {
    GtkBorder* cobj = static_cast<GtkBorder*>(get_boxed());
    Gtk::Border obj = {0, 0, 0, 0};
    return ((cobj) ? (static_cast<Gtk::Border>(*cobj)) : (obj));
  }
};

} /* namespace Glib */

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* _GTKMM_BORDER_H */

