/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

/* Stub implementation of backend-specific GtkPlug functions. */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkplug.h"
#include "gtkplugprivate.h"
#include "gtkalias.h"

GdkNativeWindow
_gtk_plug_windowing_get_id (GtkPlug *plug)
{
  return 0;
}

void
_gtk_plug_windowing_realize_toplevel (GtkPlug *plug)
{
}

void
_gtk_plug_windowing_map_toplevel (GtkPlug *plug)
{
}

void
_gtk_plug_windowing_unmap_toplevel (GtkPlug *plug)
{
}

void
_gtk_plug_windowing_set_focus (GtkPlug *plug)
{
}

void
_gtk_plug_windowing_add_grabbed_key (GtkPlug        *plug,
				     guint           accelerator_key,
				     GdkModifierType accelerator_mods)
{
}

void
_gtk_plug_windowing_remove_grabbed_key (GtkPlug        *plug,
					guint           accelerator_key,
					GdkModifierType accelerator_mods)
{
}

void
_gtk_plug_windowing_focus_to_parent (GtkPlug         *plug,
				     GtkDirectionType direction)
{
}

GdkFilterReturn
_gtk_plug_windowing_filter_func (GdkXEvent *gdk_xevent,
				 GdkEvent  *event,
				 gpointer   data)
{
  return GDK_FILTER_CONTINUE;
}
