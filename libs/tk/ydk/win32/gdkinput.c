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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* This file should really be one level up, in the backend-independent
 * GDK, and the x11/gdkinput.c could also be removed.
 * 
 * That stuff in x11/gdkinput.c which really *is* X11-dependent should
 * be in x11/gdkinput-x11.c.
 */

#include "config.h"

#include "gdkdisplay.h"
#include "gdkinput.h"

#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

static GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

/* Global variables  */

GList            *_gdk_input_devices;
GList            *_gdk_input_windows;
gboolean          _gdk_input_in_proximity = 0;
gboolean          _gdk_input_inside_input_window = 0;

void
_gdk_init_input_core (GdkDisplay *display)
{
  display->core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  
  display->core_pointer->name = "Core Pointer";
  display->core_pointer->source = GDK_SOURCE_MOUSE;
  display->core_pointer->mode = GDK_MODE_SCREEN;
  display->core_pointer->has_cursor = TRUE;
  display->core_pointer->num_axes = 2;
  display->core_pointer->axes = gdk_input_core_axes;
  display->core_pointer->num_keys = 0;
  display->core_pointer->keys = NULL;
}

GType
gdk_device_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
	{
	  sizeof (GdkDeviceClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) NULL,
	  NULL,			/* class_finalize */
	  NULL,			/* class_data */
	  sizeof (GdkDevicePrivate),
	  0,			/* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            g_intern_static_string ("GdkDevice"),
                                            &object_info, 0);
    }
  
  return object_type;
}

GList *
gdk_devices_list (void)
{
  return gdk_display_list_devices (_gdk_display);
}

GList *
gdk_display_list_devices (GdkDisplay *dpy)
{
  g_return_val_if_fail (dpy == _gdk_display, NULL);

  _gdk_input_wintab_init_check ();
  return _gdk_input_devices;
}

const gchar *
gdk_device_get_name (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->name;
}

GdkInputSource
gdk_device_get_source (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->source;
}

GdkInputMode
gdk_device_get_mode (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->mode;
}

gboolean
gdk_device_get_has_cursor (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  return device->has_cursor;
}

void
gdk_device_set_source (GdkDevice      *device,
		       GdkInputSource  source)
{
  g_return_if_fail (device != NULL);

  device->source = source;
}

void
gdk_device_get_key (GdkDevice       *device,
                    guint            index,
                    guint           *keyval,
                    GdkModifierType *modifiers)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index < device->num_keys);

  if (!device->keys[index].keyval &&
      !device->keys[index].modifiers)
    return;

  if (keyval)
    *keyval = device->keys[index].keyval;

  if (modifiers)
    *modifiers = device->keys[index].modifiers;
}

void
gdk_device_set_key (GdkDevice      *device,
		    guint           index,
		    guint           keyval,
		    GdkModifierType modifiers)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (index < device->num_keys);

  device->keys[index].keyval = keyval;
  device->keys[index].modifiers = modifiers;
}

GdkAxisUse
gdk_device_get_axis_use (GdkDevice *device,
                         guint      index)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_AXIS_IGNORE);
  g_return_val_if_fail (index < device->num_axes, GDK_AXIS_IGNORE);

  return device->axes[index].use;
}

gint
gdk_device_get_n_keys (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_keys;
}

gint
gdk_device_get_n_axes (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_axes;
}

void
gdk_device_set_axis_use (GdkDevice   *device,
			 guint        index,
			 GdkAxisUse   use)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (index < device->num_axes);

  device->axes[index].use = use;

  switch (use)
    {
    case GDK_AXIS_X:
    case GDK_AXIS_Y:
      device->axes[index].min = 0.;
      device->axes[index].max = 0.;
      break;
    case GDK_AXIS_XTILT:
    case GDK_AXIS_YTILT:
      device->axes[index].min = -1.;
      device->axes[index].max = 1;
      break;
    default:
      device->axes[index].min = 0.;
      device->axes[index].max = 1;
      break;
    }
}

gboolean
gdk_device_get_history  (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (events != NULL, FALSE);
  g_return_val_if_fail (n_events != NULL, FALSE);

  if (n_events)
    *n_events = 0;
  if (events)
    *events = NULL;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;
    
  if (GDK_IS_CORE (device))
    return FALSE;
  else
    return _gdk_device_get_history (device, window, start, stop, events, n_events);
}

GdkTimeCoord ** 
_gdk_device_allocate_history (GdkDevice *device,
			      gint       n_events)
{
  GdkTimeCoord **result = g_new (GdkTimeCoord *, n_events);
  gint i;

  for (i=0; i<n_events; i++)
    result[i] = g_malloc (sizeof (GdkTimeCoord) -
			  sizeof (double) * (GDK_MAX_TIMECOORD_AXES - device->num_axes));

  return result;
}

void 
gdk_device_free_history (GdkTimeCoord **events,
			 gint           n_events)
{
  gint i;
  
  for (i=0; i<n_events; i++)
    g_free (events[i]);

  g_free (events);
}

/* FIXME: this routine currently needs to be called between creation
   and the corresponding configure event (because it doesn't get the
   root_relative_geometry).  This should work with
   gtk_window_set_extension_events, but will likely fail in other
   cases */

static void
unset_extension_events (GdkWindow *window)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GdkInputWindow *iw;

  window_private = (GdkWindowObject*) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);
  iw = impl_window->input_window;

  if (window_private->extension_events != 0)
    {
      g_assert (iw != NULL);
      g_assert (g_list_find (iw->windows, window) != NULL);

      iw->windows = g_list_remove (iw->windows, window);
      if (iw->windows == NULL)
	{
	  impl_window->input_window = NULL;
	  _gdk_input_windows = g_list_remove(_gdk_input_windows,iw);
	  g_free (iw);
	}
    }

  window_private->extension_events = 0;
}

static void
gdk_input_get_root_relative_geometry (HWND w,
				      int  *x_ret,
				      int  *y_ret)
{
  POINT pt;

  pt.x = 0;
  pt.y = 0;
  ClientToScreen (w, &pt);

  if (x_ret)
    *x_ret = pt.x + _gdk_offset_x;
  if (y_ret)
    *y_ret = pt.y + _gdk_offset_y;
}

void
gdk_input_set_extension_events (GdkWindow *window, gint mask,
				GdkExtensionMode mode)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  window_private = (GdkWindowObject*) window;
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);

  if (mode == GDK_EXTENSION_EVENTS_ALL && mask != 0)
    mask |= GDK_ALL_DEVICES_MASK;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  iw = impl_window->input_window;

  if (mask != 0)
    {
      _gdk_input_wintab_init_check ();

      if (!iw)
	{
	  iw = g_new0 (GdkInputWindow,1);

	  iw->impl_window = (GdkWindow *)impl_window;

	  iw->windows = NULL;

	  _gdk_input_windows = g_list_append(_gdk_input_windows, iw);

	  gdk_input_get_root_relative_geometry (GDK_WINDOW_HWND (window), &iw->root_x, &iw->root_y);

	  impl_window->input_window = iw;
	}

      if (window_private->extension_events == 0)
	iw->windows = g_list_append (iw->windows, window);
      window_private->extension_events = mask;
    }
  else
    {
      unset_extension_events (window);
    }

  _gdk_input_select_events ((GdkWindow *)impl_window);
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
  unset_extension_events (window);
}

void
_gdk_input_check_proximity (void)
{
  GList *l;
  gboolean new_proximity = FALSE;

  if (!_gdk_input_inside_input_window)
    {
      _gdk_display->ignore_core_events = FALSE;
      return;
    }

  for (l = _gdk_input_devices; l != NULL; l = l->next)
    {
      GdkDevicePrivate *gdkdev = l->data;

      if (gdkdev->info.mode != GDK_MODE_DISABLED &&
	  !GDK_IS_CORE (gdkdev))
	{
	  if (_gdk_input_in_proximity)
	    {
	      new_proximity = TRUE;
	      break;
	    }
	}
    }

  _gdk_display->ignore_core_events = new_proximity;
}

void
_gdk_input_crossing_event (GdkWindow *window,
			   gboolean enter)
{
  if (enter)
    {
      GdkWindowObject *priv = (GdkWindowObject *)window;
      GdkInputWindow *input_window;
      gint root_x, root_y;

      _gdk_input_inside_input_window = TRUE;

      input_window = priv->input_window;
      if (input_window != NULL)
	{
	  gdk_input_get_root_relative_geometry (GDK_WINDOW_HWND (window), 
						&root_x, &root_y);
	  input_window->root_x = root_x;
	  input_window->root_y = root_y;
	}
    }
  else
    {
      _gdk_input_inside_input_window = FALSE;
    }

  _gdk_input_check_proximity ();
}

gboolean
gdk_device_get_axis (GdkDevice  *device,
		     gdouble    *axes,
		     GdkAxisUse  use,
		     gdouble    *value)
{
  gint i;
  
  g_return_val_if_fail (device != NULL, FALSE);

  if (axes == NULL)
    return FALSE;
  
  for (i=0; i<device->num_axes; i++)
    if (device->axes[i].use == use)
      {
	if (value)
	  *value = axes[i];
	return TRUE;
      }
  
  return FALSE;
}

gboolean
gdk_device_set_mode (GdkDevice   *device,
		     GdkInputMode mode)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;
  GdkInputWindow *input_window;

  if (GDK_IS_CORE (device))
    return FALSE;

  gdkdev = (GdkDevicePrivate *)device;

  if (device->mode == mode)
    return TRUE;

  device->mode = mode;

  if (mode == GDK_MODE_WINDOW)
    device->has_cursor = FALSE;
  else if (mode == GDK_MODE_SCREEN)
    device->has_cursor = TRUE;

  for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      _gdk_input_select_events (input_window->impl_window);
    }

  if (!GDK_IS_CORE (gdkdev))
    _gdk_input_update_for_device_mode (gdkdev);

  return TRUE;
}
