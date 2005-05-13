/* init.cc
 *
 * Copyright (C) 2001 The libgnomeuimm Development Team
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

#include <libgnomecanvasmm/init.h>
#include <gtkmm/main.h>
#include <libgnomecanvasmm/wrap_init.h>

namespace Gnome
{

namespace Canvas
{

void init()
{
  Gtk::Main::init_gtkmm_internals(); //Sets up the g type system and the Glib::wrap() table.
  wrap_init(); //Tells the Glib::wrap() table about the libgnomecanvasmm classes.
}

} /* namespace Canvas */
} /* namespace Gnome */
