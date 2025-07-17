// -*- c++ -*-
#ifndef _GTKMM_BASE_H
#define _GTKMM_BASE_H

/* $Id$ */

/* base.h
 *
 * Copyright (C) 1998-2002 The gtkmm Development Team
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

#include <glibmm/utility.h>
#include <glibmm/ustring.h>

namespace Gtk
{
  using Glib::unconst;

}

/* This aint pretty, but I don't know a better way to make
   sure things work when the stl classes are defined in a namespace */

// Gtk-- Base and other utility classes
#define GTK_VERSION_GT(major,minor) ((GTK_MAJOR_VERSION>major)||(GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION>minor))
#define GTK_VERSION_GE(major,minor) ((GTK_MAJOR_VERSION>major)||(GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION>=minor))
#define GTK_VERSION_EQ(major,minor) ((GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION==minor))
#define GTK_VERSION_NE(major,minor) ((GTK_MAJOR_VERSION!=major)||(GTK_MINOR_VERSION!=minor))
#define GTK_VERSION_LE(major,minor) ((GTK_MAJOR_VERSION<major)||(GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION<=minor))
#define GTK_VERSION_LT(major,minor) ((GTK_MAJOR_VERSION<major)||(GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION<minor))

#define GTK_VERSION_GT_MICRO(major,minor,micro) \
  ((GTK_MAJOR_VERSION>major)|| \
  (GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION>minor)||\
  (GTK_MAJOR_VERSION==major)&&(GTK_MINOR_VERSION==minor)&&(GTK_MICRO_VERSION>minor))


#endif /* _GTKMM_BASE_H */

