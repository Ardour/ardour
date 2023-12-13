/* GDK - The GIMP Drawing Kit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gdkinputprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

void
_gdk_input_init (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  _gdk_init_input_core (display);
  
  display_x11->input_devices = g_list_append (NULL, display->core_pointer);
  display->ignore_core_events = FALSE;
}

void 
gdk_device_get_state (GdkDevice       *device,
		      GdkWindow       *window,
		      gdouble         *axes,
		      GdkModifierType *mask)
{
  gint x_int, y_int;

  g_return_if_fail (device != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

gboolean
_gdk_device_get_history (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  g_warning ("gdk_device_get_history() called for invalid device");
  return FALSE;
}

void
_gdk_input_select_events (GdkWindow        *impl_window,
			  GdkDevicePrivate *gdkdev)
{
}

gboolean
_gdk_input_other_event (GdkEvent *event, 
			XEvent *xevent, 
			GdkWindow *window)
{
  return FALSE;
}

void
_gdk_input_configure_event (XConfigureEvent *xevent,
			    GdkWindow       *window)
{
}

void 
_gdk_input_crossing_event (GdkWindow *window,
			   gboolean enter)
{
}

gint 
_gdk_input_grab_pointer (GdkWindow *     window,
			 GdkWindow      *native_window,
			 gint            owner_events,
			 GdkEventMask    event_mask,
			 GdkWindow *     confine_to,
			 guint32         time)
{
  return Success;
}

void
_gdk_input_ungrab_pointer (GdkDisplay *display,
			   guint32     time)
{
}

gboolean
gdk_device_set_mode (GdkDevice   *device,
		     GdkInputMode mode)
{
  return FALSE;
}

#define __GDK_INPUT_NONE_C__
#include "gdkaliasdef.c"
