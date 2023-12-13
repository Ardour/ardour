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

#include "gtksocket.h"
#include "gtksocketprivate.h"
#include "gtkalias.h"

GdkNativeWindow
_gtk_socket_windowing_get_id (GtkSocket *socket)
{
  return 0;
}

void
_gtk_socket_windowing_realize_window (GtkSocket *socket)
{
}

void
_gtk_socket_windowing_end_embedding_toplevel (GtkSocket *socket)
{
}

void
_gtk_socket_windowing_size_request (GtkSocket *socket)
{
}

void
_gtk_socket_windowing_send_key_event (GtkSocket *socket,
				      GdkEvent  *gdk_event,
				      gboolean   mask_key_presses)
{
}

void
_gtk_socket_windowing_focus_change (GtkSocket *socket,
				    gboolean   focus_in)
{
}

void
_gtk_socket_windowing_update_active (GtkSocket *socket,
				     gboolean   active)
{
}

void
_gtk_socket_windowing_update_modality (GtkSocket *socket,
				       gboolean   modality)
{
}

void
_gtk_socket_windowing_focus (GtkSocket       *socket,
			     GtkDirectionType direction)
{
}

void
_gtk_socket_windowing_send_configure_event (GtkSocket *socket)
{
}

void
_gtk_socket_windowing_select_plug_window_input (GtkSocket *socket)
{
}

void
_gtk_socket_windowing_embed_get_info (GtkSocket *socket)
{
}

void
_gtk_socket_windowing_embed_notify (GtkSocket *socket)
{
}

gboolean
_gtk_socket_windowing_embed_get_focus_wrapped (void)
{
  return FALSE;
}

void
_gtk_socket_windowing_embed_set_focus_wrapped (void)
{
}

GdkFilterReturn
_gtk_socket_windowing_filter_func (GdkXEvent *gdk_xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  return GDK_FILTER_CONTINUE;
}
