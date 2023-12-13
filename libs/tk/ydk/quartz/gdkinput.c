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

#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include "gdkprivate-quartz.h"
#include "gdkscreen-quartz.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinputprivate.h"


#define N_CORE_POINTER_AXES 2
#define N_INPUT_DEVICE_AXES 5


static GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

static GdkDeviceAxis gdk_quartz_pen_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 },
  { GDK_AXIS_PRESSURE, 0, 1 },
  { GDK_AXIS_XTILT, -1, 1 },
  { GDK_AXIS_YTILT, -1, 1 }
};

static GdkDeviceAxis gdk_quartz_cursor_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 },
  { GDK_AXIS_PRESSURE, 0, 1 },
  { GDK_AXIS_XTILT, -1, 1 },
  { GDK_AXIS_YTILT, -1, 1 }
};

static GdkDeviceAxis gdk_quartz_eraser_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 },
  { GDK_AXIS_PRESSURE, 0, 1 },
  { GDK_AXIS_XTILT, -1, 1 },
  { GDK_AXIS_YTILT, -1, 1 }
};


/* Global variables  */
static GList     *_gdk_input_windows = NULL;
static GList     *_gdk_input_devices = NULL;
static GdkDevice *_gdk_core_pointer = NULL;
static GdkDevice *_gdk_quartz_pen = NULL;
static GdkDevice *_gdk_quartz_cursor = NULL;
static GdkDevice *_gdk_quartz_eraser = NULL;
static GdkDevice *active_device = NULL;

static void
gdk_device_finalize (GObject *object)
{
  g_error ("A GdkDevice object was finalized. This should not happen");
}

static void
gdk_device_class_init (GObjectClass *class)
{
  class->finalize = gdk_device_finalize;
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
        (GClassInitFunc) gdk_device_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDevicePrivate),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDevice",
                                            &object_info, 0);
    }
  
  return object_type;
}

GList *
gdk_devices_list (void)
{
  return _gdk_input_devices;
}

GList *
gdk_display_list_devices (GdkDisplay *dpy)
{
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
gdk_device_set_source (GdkDevice *device,
		       GdkInputSource source)
{
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
#if 0
  /* Remapping axes is unsupported for now */
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
#endif
}

/**
 * gdk_input_set_device_state:
 * @device: The devices to set
 * @mask: The new button mask
 * @axes: The new axes values
 *
 * Set the state of a device's inputs for later
 * retrieval by gdk_device_get_state.
 */
static void
gdk_input_set_device_state (GdkDevice *device,
                            GdkModifierType mask,
                            gdouble *axes)
{
  GdkDevicePrivate *priv;
  gint i;

  if (device != _gdk_core_pointer)
    {
      priv = (GdkDevicePrivate *)device;
      priv->last_state = mask;

      for (i = 0; i < device->num_axes; ++i)
        priv->last_axes_state[i] = axes[i];
    }
}

void
gdk_device_get_state (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  GdkDevicePrivate *priv;
  gint i;

  if (device == _gdk_core_pointer)
    {
      gint x_int, y_int;

      gdk_window_get_pointer (window, &x_int, &y_int, mask);

      if (axes)
        {
          axes[0] = x_int;
          axes[1] = y_int;
        }
    }
  else
    {
      priv = (GdkDevicePrivate *)device;

      if (mask)
        *mask = priv->last_state;

      if (axes)
        for (i = 0; i < device->num_axes; ++i)
          axes[i] = priv->last_axes_state[i];
    }
}

void 
gdk_device_free_history (GdkTimeCoord **events,
			 gint           n_events)
{
  gint i;
  
  for (i = 0; i < n_events; i++)
    g_free (events[i]);

  g_free (events);
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
  g_return_val_if_fail (GDK_WINDOW_IS_QUARTZ (window), FALSE);
  g_return_val_if_fail (events != NULL, FALSE);
  g_return_val_if_fail (n_events != NULL, FALSE);

  *n_events = 0;
  *events = NULL;
  return FALSE;
}

gboolean
gdk_device_set_mode (GdkDevice   *device,
                     GdkInputMode mode)
{
  /* FIXME: Window mode isn't supported yet */
  if (device != _gdk_core_pointer &&
      (mode == GDK_MODE_DISABLED || mode == GDK_MODE_SCREEN))
    {
      device->mode = mode;
      return TRUE;
    }

  return FALSE;
}

gint
_gdk_input_enable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}

gint
_gdk_input_disable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}


GdkInputWindow *
_gdk_input_window_find(GdkWindow *window)
{
  GList *tmp_list;

  for (tmp_list=_gdk_input_windows; tmp_list; tmp_list=tmp_list->next)
    if (((GdkInputWindow *)(tmp_list->data))->window == window)
      return (GdkInputWindow *)(tmp_list->data);

  return NULL;      /* Not found */
}

/* FIXME: this routine currently needs to be called between creation
   and the corresponding configure event (because it doesn't get the
   root_relative_geometry).  This should work with
   gtk_window_set_extension_events, but will likely fail in other
   cases */

void
gdk_input_set_extension_events (GdkWindow *window, gint mask,
				GdkExtensionMode mode)
{
  GdkWindowObject *window_private;
  GList *tmp_list;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_WINDOW_IS_QUARTZ (window));

  window_private = (GdkWindowObject*) window;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  if (mask != 0)
    {
      iw = g_new(GdkInputWindow,1);

      iw->window = window;
      iw->mode = mode;

      iw->obscuring = NULL;
      iw->num_obscuring = 0;
      iw->grabbed = FALSE;

      _gdk_input_windows = g_list_append (_gdk_input_windows,iw);
      window_private->extension_events = mask;

      /* Add enter window events to the event mask */
      /* FIXME, this is not needed for XINPUT_NONE */
      gdk_window_set_events (window,
			     gdk_window_get_events (window) | 
			     GDK_ENTER_NOTIFY_MASK);
    }
  else
    {
      iw = _gdk_input_window_find (window);
      if (iw)
	{
	  _gdk_input_windows = g_list_remove (_gdk_input_windows,iw);
	  g_free (iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev != (GdkDevicePrivate *)_gdk_core_pointer)
	{
	  if (mask != 0 && gdkdev->info.mode != GDK_MODE_DISABLED
	      && (gdkdev->info.has_cursor || mode == GDK_EXTENSION_EVENTS_ALL))
	    _gdk_input_enable_window (window,gdkdev);
	  else
	    _gdk_input_disable_window (window,gdkdev);
	}
    }
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
  GdkInputWindow *input_window;

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  _gdk_input_windows = g_list_remove (_gdk_input_windows,input_window);
  g_free (input_window);
}

void
_gdk_input_init (void)
{
  GdkDevicePrivate *priv;

  _gdk_core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  _gdk_core_pointer->name = "Core Pointer";
  _gdk_core_pointer->source = GDK_SOURCE_MOUSE;
  _gdk_core_pointer->mode = GDK_MODE_SCREEN;
  _gdk_core_pointer->has_cursor = TRUE;
  _gdk_core_pointer->num_axes = N_CORE_POINTER_AXES;
  _gdk_core_pointer->axes = gdk_input_core_axes;
  _gdk_core_pointer->num_keys = 0;
  _gdk_core_pointer->keys = NULL;

  _gdk_display->core_pointer = _gdk_core_pointer;
  _gdk_input_devices = g_list_append (NULL, _gdk_core_pointer);

  _gdk_quartz_pen = g_object_new (GDK_TYPE_DEVICE, NULL);
  _gdk_quartz_pen->name = "Quartz Pen";
  _gdk_quartz_pen->source = GDK_SOURCE_PEN;
  _gdk_quartz_pen->mode = GDK_MODE_SCREEN;
  _gdk_quartz_pen->has_cursor = TRUE;
  _gdk_quartz_pen->num_axes = N_INPUT_DEVICE_AXES;
  _gdk_quartz_pen->axes = gdk_quartz_pen_axes;
  _gdk_quartz_pen->num_keys = 0;
  _gdk_quartz_pen->keys = NULL;

  priv = (GdkDevicePrivate *)_gdk_quartz_pen;
  priv->last_axes_state = g_malloc_n (_gdk_quartz_pen->num_axes, sizeof (gdouble));

  _gdk_input_devices = g_list_append (_gdk_input_devices, _gdk_quartz_pen);

  _gdk_quartz_cursor = g_object_new (GDK_TYPE_DEVICE, NULL);
  _gdk_quartz_cursor->name = "Quartz Cursor";
  _gdk_quartz_cursor->source = GDK_SOURCE_CURSOR;
  _gdk_quartz_cursor->mode = GDK_MODE_SCREEN;
  _gdk_quartz_cursor->has_cursor = TRUE;
  _gdk_quartz_cursor->num_axes = N_INPUT_DEVICE_AXES;
  _gdk_quartz_cursor->axes = gdk_quartz_cursor_axes;
  _gdk_quartz_cursor->num_keys = 0;
  _gdk_quartz_cursor->keys = NULL;

  priv = (GdkDevicePrivate *)_gdk_quartz_cursor;
  priv->last_axes_state = g_malloc_n (_gdk_quartz_cursor->num_axes, sizeof (gdouble));

  _gdk_input_devices = g_list_append (_gdk_input_devices, _gdk_quartz_cursor);

  _gdk_quartz_eraser = g_object_new (GDK_TYPE_DEVICE, NULL);
  _gdk_quartz_eraser->name = "Quartz Eraser";
  _gdk_quartz_eraser->source = GDK_SOURCE_ERASER;
  _gdk_quartz_eraser->mode = GDK_MODE_SCREEN;
  _gdk_quartz_eraser->has_cursor = TRUE;
  _gdk_quartz_eraser->num_axes = N_INPUT_DEVICE_AXES;
  _gdk_quartz_eraser->axes = gdk_quartz_eraser_axes;
  _gdk_quartz_eraser->num_keys = 0;
  _gdk_quartz_eraser->keys = NULL;

  priv = (GdkDevicePrivate *)_gdk_quartz_eraser;
  priv->last_axes_state = g_malloc_n (_gdk_quartz_eraser->num_axes, sizeof (gdouble));

  _gdk_input_devices = g_list_append (_gdk_input_devices, _gdk_quartz_eraser);

  active_device = _gdk_core_pointer;
}

void
_gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev != (GdkDevicePrivate *)_gdk_core_pointer)
        {
          gdk_device_set_mode ((GdkDevice *)gdkdev, GDK_MODE_DISABLED);

          g_free (gdkdev->info.name);
          g_free (gdkdev->info.axes);
          g_free (gdkdev->info.keys);
          g_free (gdkdev->last_axes_state);
          g_free (gdkdev);
        }
    }

  g_list_free (_gdk_input_devices);

  for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      g_free (tmp_list->data);
    }
  g_list_free (_gdk_input_windows);
}

gboolean
gdk_device_get_axis (GdkDevice *device, gdouble *axes, GdkAxisUse use, gdouble *value)
{
  gint i;
  
  g_return_val_if_fail (device != NULL, FALSE);

  if (axes == NULL)
    return FALSE;
  
  for (i = 0; i < device->num_axes; i++)
    if (device->axes[i].use == use)
      {
	if (value)
	  *value = axes[i];
	return TRUE;
      }
  
  return FALSE;
}

void
_gdk_input_window_crossing (GdkWindow *window,
                            gboolean   enter)
{
}

/**
 * _gdk_input_quartz_tablet_proximity:
 * @deviceType: The result of [nsevent pointingDeviceType]
 *
 * Update the current active device based on a proximity event.
 */
void
_gdk_input_quartz_tablet_proximity (NSPointingDeviceType deviceType)
{
  if (deviceType == NSPenPointingDevice)
    active_device = _gdk_quartz_pen;
  else if (deviceType == NSCursorPointingDevice)
    active_device = _gdk_quartz_cursor;
  else if (deviceType == NSEraserPointingDevice)
    active_device = _gdk_quartz_eraser;
  else
    active_device = _gdk_core_pointer;
}

/**
 * _gdk_input_fill_quartz_input_event:
 * @event: The GDK mouse event.
 * @nsevent: The NSEvent that generated the mouse event.
 * @input_event: (out): Return location for the input event.
 *
 * Handle extended input for the passed event, the GdkEvent object
 * passed in should be a filled mouse button or motion event.
 *
 * Return value: %TRUE if an extended input event was generated.
 */
gboolean
_gdk_input_fill_quartz_input_event (GdkEvent *event,
                                    NSEvent  *nsevent,
                                    GdkEvent *input_event)
{
  gdouble *axes;
  gint x, y;
  gint x_target, y_target;
  gdouble x_root, y_root;
  gint state;
  GdkInputWindow *iw;
  GdkWindow *target_window;
  GdkScreenQuartz *screen_quartz;

  if ([nsevent subtype] == NSTabletProximityEventSubtype)
    {
      _gdk_input_quartz_tablet_proximity ([nsevent pointingDeviceType]);
    }
  else if (([nsevent subtype] != NSTabletPointEventSubtype) ||
           (active_device == _gdk_core_pointer) ||
           (active_device->mode == GDK_MODE_DISABLED))
    {
      _gdk_display->ignore_core_events = FALSE;
      return FALSE;
    }

  switch (event->any.type)
    {
      case GDK_MOTION_NOTIFY:
        x = event->motion.x;
        y = event->motion.y;
        state = event->motion.state;
        break;
      case GDK_BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        x = event->button.x;
        y = event->button.y;
        state = event->button.state;
        break;
      default:
        /* Not an input related event */
        return FALSE;
        break;
    }

  /* Input events won't be propagated through windows that aren't listening
   * for input events, so _gdk_window_get_input_window_for_event finds the
   * window to directly send the event to.
   */
  target_window = _gdk_window_get_input_window_for_event (event->any.window,
                                                          event->any.type,
                                                          0, x, y, 0);

  iw = _gdk_input_window_find (target_window);

  if (!iw)
    {
      /* Return if the target window doesn't have extended events enabled or
       * hasn't asked for this type of event.
       */
      _gdk_display->ignore_core_events = FALSE;
      return FALSE;
    }

  /* The cursor is inside an extended events window, block propagation of the
   * core motion / button events
   */
  _gdk_display->ignore_core_events = TRUE;

  axes = g_malloc_n (N_INPUT_DEVICE_AXES, sizeof (gdouble));

  gdk_window_get_origin (target_window, &x_target, &y_target);

  /* Equation for root x & y taken from _gdk_quartz_window_xy_to_gdk_xy
   * recalculated here to get doubles instead of ints.
   */
  screen_quartz = GDK_SCREEN_QUARTZ (_gdk_screen);
  x_root = [NSEvent mouseLocation].x - screen_quartz->min_x;
  y_root = screen_quartz->height - [NSEvent mouseLocation].y + screen_quartz->min_y;

  axes[0] = x_root - x_target;
  axes[1] = y_root - y_target;
  axes[2] = [nsevent pressure];
  axes[3] = [nsevent tilt].x;
  axes[4] = [nsevent tilt].y;

  gdk_input_set_device_state (active_device, state, axes);

  input_event->any.window     = target_window;
  input_event->any.type       = event->any.type;
  input_event->any.send_event = event->any.send_event;

  switch (event->any.type)
    {
      case GDK_MOTION_NOTIFY:
        input_event->motion.device = active_device;
        input_event->motion.x      = axes[0];
        input_event->motion.y      = axes[1];
        input_event->motion.axes   = axes;
        input_event->motion.x_root = x_root;
        input_event->motion.y_root = y_root;

        input_event->motion.time    = event->motion.time;
        input_event->motion.state   = event->motion.state;
        input_event->motion.is_hint = event->motion.is_hint;
        break;
      case GDK_BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        input_event->button.device = active_device;
        input_event->button.x      = axes[0];
        input_event->button.y      = axes[1];
        input_event->button.axes   = axes;
        input_event->button.x_root = x_root;
        input_event->button.y_root = y_root;

        input_event->button.time   = event->button.time;
        input_event->button.state  = event->button.state;
        input_event->button.button = event->button.button;
        break;
      default:
        return FALSE;
        break;
    }

  return TRUE;
}
