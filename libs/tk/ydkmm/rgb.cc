// -*- c++ -*-
/* $Id$ */

/* Copyright 2004      The gtkmm Development Team
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

#include <glibmm/listhandle.h>
#include <ydkmm/rgb.h>
#include <ydk/gdk.h>

namespace Gdk
{

Glib::RefPtr<Colormap> rgb_get_colormap()
{
  return Glib::wrap( gdk_rgb_get_colormap() );
}

Glib::RefPtr<Visual> rgb_get_visual()
{
  return Glib::wrap( gdk_rgb_get_visual() );
}

bool rgb_ditherable()
{
  return gdk_rgb_ditherable();
}

} //namespace Gdk

