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
#include <string.h>
#include "gdkinputprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* forward declarations */

static void gdk_input_check_proximity (GdkDisplay *display);

void
_gdk_input_init(GdkDisplay *display)
{
  _gdk_init_input_core (display);
  display->ignore_core_events = FALSE;
  _gdk_input_common_init (display, FALSE);
}

gboolean
gdk_device_set_mode (GdkDevice      *device,
		     GdkInputMode    mode)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;
  GdkInputWindow *input_window;
  GdkDisplayX11 *display_impl;

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

  display_impl = GDK_DISPLAY_X11 (gdkdev->display);
  for (tmp_list = display_impl->input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      _gdk_input_select_events (input_window->impl_window, gdkdev);
    }

  return TRUE;
}

static int
ignore_errors (Display *display, XErrorEvent *event)
{
  return True;
}

static void
gdk_input_check_proximity (GdkDisplay *display)
{
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);
  GList *tmp_list = display_impl->input_devices;
  gint new_proximity = 0;

  while (tmp_list && !new_proximity)
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev->info.mode != GDK_MODE_DISABLED
	  && !GDK_IS_CORE (gdkdev)
	  && gdkdev->xdevice)
	{
      int (*old_handler) (Display *, XErrorEvent *);
      XDeviceState *state = NULL;
      XInputClass *xic;
      int i;

      /* From X11 doc: "XQueryDeviceState can generate a BadDevice error."
       * This would occur in particular when a device is unplugged,
       * which would cause the program to crash (see bug 575767).
       *
       * To handle this case gracefully, we simply ignore the device.
       * GTK+ 3 handles this better with XInput 2's hotplugging support;
       * but this is better than a crash in GTK+ 2.
       */
      old_handler = XSetErrorHandler (ignore_errors);
      state = XQueryDeviceState(display_impl->xdisplay, gdkdev->xdevice);
      XSetErrorHandler (old_handler);

      if (! state)
        {
          /* Broken device. It may have been disconnected.
           * Ignore it.
           */
          tmp_list = tmp_list->next;
          continue;
        }

	  xic = state->data;
	  for (i=0; i<state->num_classes; i++)
	    {
	      if (xic->class == ValuatorClass)
		{
		  XValuatorState *xvs = (XValuatorState *)xic;
		  if ((xvs->mode & ProximityState) == InProximity)
		    {
		      new_proximity = TRUE;
		    }
		  break;
		}
	      xic = (XInputClass *)((char *)xic + xic->length);
	    }

	  XFreeDeviceState (state);
	}
      tmp_list = tmp_list->next;
    }

  display->ignore_core_events = new_proximity;
}

void
_gdk_input_configure_event (XConfigureEvent *xevent,
			    GdkWindow       *window)
{
  GdkWindowObject *priv = (GdkWindowObject *)window;
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = priv->input_window;
  if (input_window != NULL)
    {
      _gdk_input_get_root_relative_geometry (window, &root_x, &root_y);
      input_window->root_x = root_x;
      input_window->root_y = root_y;
    }
}

void
_gdk_input_crossing_event (GdkWindow *window,
			   gboolean enter)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);
  GdkWindowObject *priv = (GdkWindowObject *)window;
  GdkInputWindow *input_window;
  gint root_x, root_y;

  if (enter)
    {
      gdk_input_check_proximity(display);

      input_window = priv->input_window;
      if (input_window != NULL)
	{
	  _gdk_input_get_root_relative_geometry (window, &root_x, &root_y);
	  input_window->root_x = root_x;
	  input_window->root_y = root_y;
	}
    }
  else
    display->ignore_core_events = FALSE;
}

static GdkEventType
get_input_event_type (GdkDevicePrivate *gdkdev,
		      XEvent *xevent,
		      int *core_x, int *core_y)
{
  if (xevent->type == gdkdev->buttonpress_type)
    {
      XDeviceButtonEvent *xie = (XDeviceButtonEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_BUTTON_PRESS;
    }

  if (xevent->type == gdkdev->buttonrelease_type)
    {
      XDeviceButtonEvent *xie = (XDeviceButtonEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_BUTTON_RELEASE;
    }

  if (xevent->type == gdkdev->keypress_type)
    {
      XDeviceKeyEvent *xie = (XDeviceKeyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_KEY_PRESS;
    }

  if (xevent->type == gdkdev->keyrelease_type)
    {
      XDeviceKeyEvent *xie = (XDeviceKeyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_KEY_RELEASE;
    }

  if (xevent->type == gdkdev->motionnotify_type)
    {
      XDeviceMotionEvent *xie = (XDeviceMotionEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_MOTION_NOTIFY;
    }

  if (xevent->type == gdkdev->proximityin_type)
    {
      XProximityNotifyEvent *xie = (XProximityNotifyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_PROXIMITY_IN;
    }

  if (xevent->type == gdkdev->proximityout_type)
    {
      XProximityNotifyEvent *xie = (XProximityNotifyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_PROXIMITY_OUT;
    }

  *core_x = 0;
  *core_y = 0;
  return GDK_NOTHING;
}


gboolean
_gdk_input_other_event (GdkEvent *event,
			XEvent *xevent,
			GdkWindow *event_window)
{
  GdkWindow *window;
  GdkWindowObject *priv;
  GdkInputWindow *iw;
  GdkDevicePrivate *gdkdev;
  GdkEventType event_type;
  int x, y;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (event_window);
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);

  /* This is a sort of a hack, as there isn't any XDeviceAnyEvent -
     but it's potentially faster than scanning through the types of
     every device. If we were deceived, then it won't match any of
     the types for the device anyways */
  gdkdev = _gdk_input_find_device (display,
				   ((XDeviceButtonEvent *)xevent)->deviceid);
  if (!gdkdev)
    return FALSE;			/* we don't handle it - not an XInput event */

  event_type = get_input_event_type (gdkdev, xevent, &x, &y);
  if (event_type == GDK_NOTHING)
    return FALSE;

  /* If we're not getting any event window its likely because we're outside the
     window and there is no grab. We should still report according to the
     implicit grab though. */
  iw = ((GdkWindowObject *)event_window)->input_window;

  if (iw->button_down_window)
    window = iw->button_down_window;
  else
    window = _gdk_window_get_input_window_for_event (event_window,
                                                     event_type,
						     /* TODO: Seems wrong, but the code used to ignore button motion handling here... */
						     0, 
                                                     x, y,
                                                     xevent->xany.serial);
  priv = (GdkWindowObject *)window;
  if (window == NULL)
    return FALSE;

  if (gdkdev->info.mode == GDK_MODE_DISABLED ||
      priv->extension_events == 0 ||
      !(gdkdev->info.has_cursor || (priv->extension_events & GDK_ALL_DEVICES_MASK)))
    return FALSE;

  if (!display->ignore_core_events && priv->extension_events != 0)
    gdk_input_check_proximity (GDK_WINDOW_DISPLAY (window));

  if (!_gdk_input_common_other_event (event, xevent, window, gdkdev))
    return FALSE;

  if (event->type == GDK_BUTTON_PRESS)
    iw->button_down_window = window;
  if (event->type == GDK_BUTTON_RELEASE && !gdkdev->button_count)
    iw->button_down_window = NULL;

  if (event->type == GDK_PROXIMITY_OUT &&
      display->ignore_core_events)
    gdk_input_check_proximity (GDK_WINDOW_DISPLAY (window));

  return _gdk_input_common_event_selected(event, window, gdkdev);
}

gint
_gdk_input_grab_pointer (GdkWindow      *window,
			 GdkWindow      *native_window, /* This is the toplevel */
			 gint            owner_events,
			 GdkEventMask    event_mask,
			 GdkWindow *     confine_to,
			 guint32         time)
{
  GdkInputWindow *input_window;
  GdkWindowObject *priv, *impl_window;
  gboolean need_ungrab;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;
  XEventClass event_classes[GDK_MAX_DEVICE_CLASSES];
  gint num_classes;
  gint result;
  GdkDisplayX11 *display_impl  = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  tmp_list = display_impl->input_windows;
  need_ungrab = FALSE;

  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;

      if (input_window->grabbed)
	{
	  input_window->grabbed = FALSE;
	  need_ungrab = TRUE;
	  break;
	}
      tmp_list = tmp_list->next;
    }

  priv = (GdkWindowObject *)window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);
  input_window = impl_window->input_window;
  if (priv->extension_events)
    {
      g_assert (input_window != NULL);
      input_window->grabbed = TRUE;

      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice)
	    {
	      _gdk_input_common_find_events (gdkdev, event_mask,
					     event_classes, &num_classes);
#ifdef G_ENABLE_DEBUG
	      if (_gdk_debug_flags & GDK_DEBUG_NOGRABS)
		result = GrabSuccess;
	      else
#endif
		result = XGrabDevice (display_impl->xdisplay, gdkdev->xdevice,
				      GDK_WINDOW_XWINDOW (native_window),
				      owner_events, num_classes, event_classes,
				      GrabModeAsync, GrabModeAsync, time);

	      /* FIXME: if failure occurs on something other than the first
		 device, things will be badly inconsistent */
	      if (result != Success)
		return result;
	    }
	  tmp_list = tmp_list->next;
	}
    }
  else
    {
      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice &&
	      ((gdkdev->button_count != 0) || need_ungrab))
	    {
	      XUngrabDevice (display_impl->xdisplay, gdkdev->xdevice, time);
	      memset (gdkdev->button_state, 0, sizeof (gdkdev->button_state));
	      gdkdev->button_count = 0;
	    }

	  tmp_list = tmp_list->next;
	}
    }

  return Success;
}

void
_gdk_input_ungrab_pointer (GdkDisplay *display,
			   guint32 time)
{
  GdkInputWindow *input_window = NULL; /* Quiet GCC */
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);

  tmp_list = display_impl->input_windows;
  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      if (input_window->grabbed)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)			/* we found a grabbed window */
    {
      input_window->grabbed = FALSE;

      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice)
	    XUngrabDevice( display_impl->xdisplay, gdkdev->xdevice, time);

	  tmp_list = tmp_list->next;
	}
    }
}

#define __GDK_INPUT_XFREE_C__
#include "gdkaliasdef.c"
