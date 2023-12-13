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

#include "config.h"

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "gdkx.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinputprivate.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

static GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

void
_gdk_init_input_core (GdkDisplay *display)
{
  GdkDevicePrivate *private;

  display->core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  private = (GdkDevicePrivate *)display->core_pointer;

  display->core_pointer->name = "Core Pointer";
  display->core_pointer->source = GDK_SOURCE_MOUSE;
  display->core_pointer->mode = GDK_MODE_SCREEN;
  display->core_pointer->has_cursor = TRUE;
  display->core_pointer->num_axes = 2;
  display->core_pointer->axes = gdk_input_core_axes;
  display->core_pointer->num_keys = 0;
  display->core_pointer->keys = NULL;

  private->display = display;
}

static void gdk_device_class_init (GdkDeviceClass *klass);
static void gdk_device_dispose    (GObject        *object);

static gpointer gdk_device_parent_class = NULL;

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
	  (GClassInitFunc) gdk_device_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkDevicePrivate),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};

      object_type = g_type_register_static (G_TYPE_OBJECT,
					    g_intern_static_string ("GdkDevice"),
					    &object_info, 0);
    }

  return object_type;
}

static void
gdk_device_class_init (GdkDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gdk_device_parent_class = g_type_class_peek_parent (klass);

  object_class->dispose  = gdk_device_dispose;
}

static void
gdk_device_dispose (GObject *object)
{
  GdkDevicePrivate *gdkdev = (GdkDevicePrivate *) object;

  if (gdkdev->display && !GDK_IS_CORE (gdkdev))
    {
#ifndef XINPUT_NONE
      if (gdkdev->xdevice)
	{
	  XCloseDevice (GDK_DISPLAY_XDISPLAY (gdkdev->display), gdkdev->xdevice);
	  gdkdev->xdevice = NULL;
	}
      g_free (gdkdev->axes);
      g_free (gdkdev->axis_data);
      gdkdev->axes = NULL;
      gdkdev->axis_data = NULL;
#endif /* !XINPUT_NONE */

      g_free (gdkdev->info.name);
      g_free (gdkdev->info.keys);
      g_free (gdkdev->info.axes);

      gdkdev->info.name = NULL;
      gdkdev->info.keys = NULL;
      gdkdev->info.axes = NULL;
    }

  G_OBJECT_CLASS (gdk_device_parent_class)->dispose (object);
}

/**
 * gdk_devices_list:
 *
 * Returns the list of available input devices for the default display.
 * The list is statically allocated and should not be freed.
 *
 * Return value: (transfer none) (element-type GdkDevice): a list of #GdkDevice
 **/
GList *
gdk_devices_list (void)
{
  return gdk_display_list_devices (gdk_display_get_default ());
}

/**
 * gdk_display_list_devices:
 * @display: a #GdkDisplay
 *
 * Returns the list of available input devices attached to @display.
 * The list is statically allocated and should not be freed.
 *
 * Return value: a list of #GdkDevice
 *
 * Since: 2.2
 **/
GList *
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_X11 (display)->input_devices;
}

/**
 * gdk_device_get_name:
 * @device: a #GdkDevice
 *
 * Determines the name of the device.
 *
 * Return value: a name
 *
 * Since: 2.22
 **/
const gchar *
gdk_device_get_name (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->name;
}

/**
 * gdk_device_get_source:
 * @device: a #GdkDevice
 *
 * Determines the type of the device.
 *
 * Return value: a #GdkInputSource
 *
 * Since: 2.22
 **/
GdkInputSource
gdk_device_get_source (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->source;
}

/**
 * gdk_device_get_mode:
 * @device: a #GdkDevice
 *
 * Determines the mode of the device.
 *
 * Return value: a #GdkInputSource
 *
 * Since: 2.22
 **/
GdkInputMode
gdk_device_get_mode (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->mode;
}

/**
 * gdk_device_get_has_cursor:
 * @device: a #GdkDevice
 *
 * Determines whether the pointer follows device motion.
 *
 * Return value: %TRUE if the pointer follows device motion
 *
 * Since: 2.22
 **/
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

/**
 * gdk_device_get_key:
 * @device: a #GdkDevice.
 * @index: the index of the macro button to get.
 * @keyval: return value for the keyval.
 * @modifiers: return value for modifiers.
 *
 * If @index has a valid keyval, this function will
 * fill in @keyval and @modifiers with the keyval settings.
 *
 * Since: 2.22
 **/
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

/**
 * gdk_device_get_axis_use:
 * @device: a #GdkDevice.
 * @index: the index of the axis.
 *
 * Returns the axis use for @index.
 *
 * Returns: a #GdkAxisUse specifying how the axis is used.
 *
 * Since: 2.22
 **/
GdkAxisUse
gdk_device_get_axis_use (GdkDevice *device,
                         guint      index)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_AXIS_IGNORE);
  g_return_val_if_fail (index < device->num_axes, GDK_AXIS_IGNORE);

  return device->axes[index].use;
}

/**
 * gdk_device_get_n_keys:
 * @device: a #GdkDevice.
 *
 * Gets the number of keys of a device.
 *
 * Returns: the number of keys of @device
 *
 * Since: 2.24
 **/
gint
gdk_device_get_n_keys (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_keys;
}

/**
 * gdk_device_get_n_axes:
 * @device: a #GdkDevice.
 *
 * Gets the number of axes of a device.
 *
 * Returns: the number of axes of @device
 *
 * Since: 2.22
 **/
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

static gboolean
impl_coord_in_window (GdkWindow *window,
		      int impl_x,
		      int impl_y)
{
  GdkWindowObject *priv = (GdkWindowObject *)window;

  if (impl_x < priv->abs_x ||
      impl_x > priv->abs_x + priv->width)
    return FALSE;
  if (impl_y < priv->abs_y ||
      impl_y > priv->abs_y + priv->height)
    return FALSE;
  return TRUE;
}

/**
 * gdk_device_get_history:
 * @device: a #GdkDevice
 * @window: the window with respect to which which the event coordinates will be reported
 * @start: starting timestamp for range of events to return
 * @stop: ending timestamp for the range of events to return
 * @events: (array length=n_events) (out) (transfer none): location to store a newly-allocated array of #GdkTimeCoord, or %NULL
 * @n_events: location to store the length of @events, or %NULL
 *
 * Obtains the motion history for a device; given a starting and
 * ending timestamp, return all events in the motion history for
 * the device in the given range of time. Some windowing systems
 * do not support motion history, in which case, %FALSE will
 * be returned. (This is not distinguishable from the case where
 * motion history is supported and no events were found.)
 *
 * Return value: %TRUE if the windowing system supports motion history and
 *  at least one event was found.
 **/
gboolean
gdk_device_get_history  (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  GdkTimeCoord **coords = NULL;
  GdkWindow *impl_window;
  gboolean result = FALSE;
  int tmp_n_events = 0;

  g_return_val_if_fail (GDK_WINDOW_IS_X11 (window), FALSE);

  impl_window = _gdk_window_get_impl_window (window);

  if (GDK_WINDOW_DESTROYED (window))
    /* Nothing */ ;
  else if (GDK_IS_CORE (device))
    {
      XTimeCoord *xcoords;

      xcoords = XGetMotionEvents (GDK_DRAWABLE_XDISPLAY (window),
				  GDK_DRAWABLE_XID (impl_window),
				  start, stop, &tmp_n_events);
      if (xcoords)
	{
	  GdkWindowObject *priv = (GdkWindowObject *)window;
          int i, j;

	  coords = _gdk_device_allocate_history (device, tmp_n_events);
	  j = 0;

	  for (i = 0; i < tmp_n_events; i++)
	    {
	      if (impl_coord_in_window (window, xcoords[i].x, xcoords[i].y))
		{
		  coords[j]->time = xcoords[i].time;
		  coords[j]->axes[0] = xcoords[i].x - priv->abs_x;
		  coords[j]->axes[1] = xcoords[i].y - priv->abs_y;
		  j++;
		}
	    }

	  XFree (xcoords);

          /* free the events we allocated too much */
          for (i = j; i < tmp_n_events; i++)
            {
              g_free (coords[i]);
              coords[i] = NULL;
            }

          tmp_n_events = j;

          if (tmp_n_events > 0)
            {
              result = TRUE;
            }
          else
            {
              gdk_device_free_history (coords, tmp_n_events);
              coords = NULL;
            }
	}
    }
  else
    result = _gdk_device_get_history (device, window, start, stop, &coords, &tmp_n_events);

  if (n_events)
    *n_events = tmp_n_events;

  if (events)
    *events = coords;
  else if (coords)
    gdk_device_free_history (coords, tmp_n_events);

  return result;
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

/**
 * gdk_device_free_history:
 * @events: (inout) (transfer none): an array of #GdkTimeCoord.
 * @n_events: the length of the array.
 *
 * Frees an array of #GdkTimeCoord that was returned by gdk_device_get_history().
 */
void
gdk_device_free_history (GdkTimeCoord **events,
			 gint           n_events)
{
  gint i;

  for (i=0; i<n_events; i++)
    g_free (events[i]);

  g_free (events);
}

static void
unset_extension_events (GdkWindow *window)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GdkDisplayX11 *display_x11;
  GdkInputWindow *iw;

  window_private = (GdkWindowObject*) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);
  iw = impl_window->input_window;

  display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  if (window_private->extension_events != 0)
    {
      g_assert (iw != NULL);
      g_assert (g_list_find (iw->windows, window) != NULL);

      iw->windows = g_list_remove (iw->windows, window);
      if (iw->windows == NULL)
	{
	  impl_window->input_window = NULL;
	  display_x11->input_windows = g_list_remove (display_x11->input_windows, iw);
	  g_free (iw);
	}
    }

  window_private->extension_events = 0;
}

void
gdk_input_set_extension_events (GdkWindow *window, gint mask,
				GdkExtensionMode mode)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GdkInputWindow *iw;
  GdkDisplayX11 *display_x11;
#ifndef XINPUT_NONE
  GList *tmp_list;
#endif

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_WINDOW_IS_X11 (window));

  window_private = (GdkWindowObject*) window;
  display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
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
      if (!iw)
	{
	  iw = g_new0 (GdkInputWindow,1);

	  iw->impl_window = (GdkWindow *)impl_window;

	  iw->windows = NULL;
	  iw->grabbed = FALSE;

	  display_x11->input_windows = g_list_append (display_x11->input_windows, iw);

#ifndef XINPUT_NONE
	  /* we might not receive ConfigureNotify so get the root_relative_geometry
	   * now, just in case */
	  _gdk_input_get_root_relative_geometry (window, &iw->root_x, &iw->root_y);
#endif /* !XINPUT_NONE */
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

#ifndef XINPUT_NONE
  for (tmp_list = display_x11->input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = tmp_list->data;

      if (!GDK_IS_CORE (gdkdev))
	_gdk_input_select_events ((GdkWindow *)impl_window, gdkdev);
    }
#endif /* !XINPUT_NONE */
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
  unset_extension_events (window);
}

/**
 * gdk_device_get_axis:
 * @device: a #GdkDevice
 * @axes: pointer to an array of axes
 * @use: the use to look for
 * @value: location to store the found value.
 *
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis use.
 *
 * Return value: %TRUE if the given axis use was found, otherwise %FALSE
 **/
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

#define __GDK_INPUT_C__
#include "gdkaliasdef.c"
