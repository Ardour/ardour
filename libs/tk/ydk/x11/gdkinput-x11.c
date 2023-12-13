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
#include "gdkinputprivate.h"
#include "gdkinternals.h"
#include "gdkx.h"
#include "gdk.h"		/* For gdk_error_trap_push()/pop() */
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

#include <string.h>

/* Forward declarations */
static GdkDevicePrivate *gdk_input_device_new            (GdkDisplay       *display,
							  XDeviceInfo      *device,
							  gint              include_core);
static void              gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
							  GdkWindow        *window,
							  gint             *axis_data,
							  gdouble          *axis_out,
							  gdouble          *x_out,
							  gdouble          *y_out);
static void              gdk_input_update_axes           (GdkDevicePrivate *gdkdev,
							  gint             axes_count,
							  gint             first_axis,
							  gint             *axis_data);
static guint             gdk_input_translate_state       (guint             state,
							  guint             device_state);

/* A temporary error handler for ignoring device unplugging-related errors. */
static int
ignore_errors (Display *display, XErrorEvent *event)
{
  return True;
}

GdkDevicePrivate *
_gdk_input_find_device (GdkDisplay *display,
			guint32     id)
{
  GList *tmp_list = GDK_DISPLAY_X11 (display)->input_devices;
  GdkDevicePrivate *gdkdev;
  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->deviceid == id)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

void
_gdk_input_get_root_relative_geometry (GdkWindow *window,
				       int *x_ret, int *y_ret)
{
  Window child;
  gint x,y;

  XTranslateCoordinates (GDK_WINDOW_XDISPLAY (window),
			 GDK_WINDOW_XWINDOW (window),
			 GDK_WINDOW_XROOTWIN (window),
			 0, 0, &x, &y, &child);

  if (x_ret)
    *x_ret = x;
  if (y_ret)
    *y_ret = y;
}

static GdkDevicePrivate *
gdk_input_device_new (GdkDisplay  *display,
		      XDeviceInfo *device,
		      gint         include_core)
{
  GdkDevicePrivate *gdkdev;
  gchar *tmp_name;
  XAnyClassPtr class;
  gint i,j;

  gdkdev = g_object_new (GDK_TYPE_DEVICE, NULL);

  gdkdev->deviceid = device->id;
  gdkdev->display = display;

  if (device->name[0])
    gdkdev->info.name = g_strdup (device->name);
 else
   /* XFree86 3.2 gives an empty name to the default core devices,
      (fixed in 3.2A) */
   gdkdev->info.name = g_strdup ("pointer");

  gdkdev->info.mode = GDK_MODE_DISABLED;

  /* Try to figure out what kind of device this is by its name -
     could invite a very, very, long list... Lowercase name
     for comparison purposes */

  tmp_name = g_ascii_strdown (gdkdev->info.name, -1);

  if (strstr (tmp_name, "eraser"))
    gdkdev->info.source = GDK_SOURCE_ERASER;
  else if (strstr (tmp_name, "cursor"))
    gdkdev->info.source = GDK_SOURCE_CURSOR;
  else if (strstr (tmp_name, "wacom") ||
	   strstr (tmp_name, "pen"))
    gdkdev->info.source = GDK_SOURCE_PEN;
  else
    gdkdev->info.source = GDK_SOURCE_MOUSE;

  g_free(tmp_name);

  gdkdev->xdevice = NULL;

  /* step through the classes */

  gdkdev->info.num_axes = 0;
  gdkdev->info.num_keys = 0;
  gdkdev->info.axes = NULL;
  gdkdev->info.keys = NULL;
  gdkdev->axes = 0;
  gdkdev->info.has_cursor = 0;
  gdkdev->needs_update = FALSE;
  gdkdev->claimed = FALSE;
  memset(gdkdev->button_state, 0, sizeof (gdkdev->button_state));
  gdkdev->button_count = 0;

  class = device->inputclassinfo;
  for (i=0;i<device->num_classes;i++)
    {
      switch (class->class) {
      case ButtonClass:
	break;
      case KeyClass:
	{
	  XKeyInfo *xki = (XKeyInfo *)class;
	  /* Hack to catch XFree86 3.3.1 bug. Other devices better
	   * not have exactly 25 keys...
	   */
	  if ((xki->min_keycode == 8) && (xki->max_keycode == 32))
	    {
	      gdkdev->info.num_keys = 32;
	      gdkdev->min_keycode = 1;
	    }
	  else
	    {
	      gdkdev->info.num_keys = xki->max_keycode - xki->min_keycode + 1;
	      gdkdev->min_keycode = xki->min_keycode;
	    }
	  gdkdev->info.keys = g_new (GdkDeviceKey, gdkdev->info.num_keys);

	  for (j=0; j<gdkdev->info.num_keys; j++)
	    {
	      gdkdev->info.keys[j].keyval = 0;
	      gdkdev->info.keys[j].modifiers = 0;
	    }

	  break;
	}
      case ValuatorClass:
	{
	  XValuatorInfo *xvi = (XValuatorInfo *)class;
	  gdkdev->info.num_axes = xvi->num_axes;
	  gdkdev->axes = g_new (GdkAxisInfo, xvi->num_axes);
	  gdkdev->axis_data = g_new0 (gint, xvi->num_axes);
	  gdkdev->info.axes = g_new0 (GdkDeviceAxis, xvi->num_axes);
	  for (j=0;j<xvi->num_axes;j++)
	    {
	      gdkdev->axes[j].resolution =
		gdkdev->axes[j].xresolution = xvi->axes[j].resolution;
	      gdkdev->axes[j].min_value =
		gdkdev->axes[j].xmin_value = xvi->axes[j].min_value;
	      gdkdev->axes[j].max_value =
		gdkdev->axes[j].xmax_value = xvi->axes[j].max_value;
	      gdkdev->info.axes[j].use = GDK_AXIS_IGNORE;
	    }
	  j=0;
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_X);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_Y);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_PRESSURE);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_XTILT);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_YTILT);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_WHEEL);

	  break;
	}
      }
      class = (XAnyClassPtr)(((char *)class) + class->length);
    }
  /* return NULL if no axes */
  if (!gdkdev->info.num_axes || !gdkdev->axes ||
      (!include_core && device->use == IsXPointer))
    goto error;

  if (device->use != IsXPointer)
    {
      gdk_error_trap_push ();
      gdkdev->xdevice = XOpenDevice (GDK_DISPLAY_XDISPLAY (display),
				     gdkdev->deviceid);

      /* return NULL if device is not ready */
      if (gdk_error_trap_pop ())
	goto error;
    }

  gdkdev->buttonpress_type = 0;
  gdkdev->buttonrelease_type = 0;
  gdkdev->keypress_type = 0;
  gdkdev->keyrelease_type = 0;
  gdkdev->motionnotify_type = 0;
  gdkdev->proximityin_type = 0;
  gdkdev->proximityout_type = 0;
  gdkdev->changenotify_type = 0;

  return gdkdev;

 error:

  g_object_unref (gdkdev);

  return NULL;
}

void
_gdk_input_common_find_events (GdkDevicePrivate *gdkdev,
			       gint mask,
			       XEventClass *classes,
			       int *num_classes)
{
  gint i;
  XEventClass class;

  i = 0;
  if (mask & GDK_BUTTON_PRESS_MASK)
    {
      DeviceButtonPress (gdkdev->xdevice, gdkdev->buttonpress_type,
			     class);
      if (class != 0)
	  classes[i++] = class;
      DeviceButtonPressGrab (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_BUTTON_RELEASE_MASK)
    {
      DeviceButtonRelease (gdkdev->xdevice, gdkdev->buttonrelease_type,
			   class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & (GDK_POINTER_MOTION_MASK |
	      GDK_BUTTON1_MOTION_MASK | GDK_BUTTON2_MOTION_MASK |
	      GDK_BUTTON3_MOTION_MASK | GDK_BUTTON_MOTION_MASK))
    {
      DeviceMotionNotify (gdkdev->xdevice, gdkdev->motionnotify_type, class);
      if (class != 0)
	  classes[i++] = class;
      DeviceStateNotify (gdkdev->xdevice, gdkdev->devicestatenotify_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_KEY_PRESS_MASK)
    {
      DeviceKeyPress (gdkdev->xdevice, gdkdev->keypress_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_KEY_RELEASE_MASK)
    {
      DeviceKeyRelease (gdkdev->xdevice, gdkdev->keyrelease_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_PROXIMITY_IN_MASK)
    {
      ProximityIn   (gdkdev->xdevice, gdkdev->proximityin_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_PROXIMITY_OUT_MASK)
    {
      ProximityOut  (gdkdev->xdevice, gdkdev->proximityout_type, class);
      if (class != 0)
	  classes[i++] = class;
    }

  *num_classes = i;
}

void
_gdk_input_select_events (GdkWindow *impl_window,
			  GdkDevicePrivate *gdkdev)
{
  int (*old_handler) (Display *, XErrorEvent *);
  XEventClass classes[GDK_MAX_DEVICE_CLASSES];
  gint num_classes;
  guint event_mask;
  GdkWindowObject *w;
  GdkInputWindow *iw;
  GList *l;

  event_mask = 0;
  iw = ((GdkWindowObject *)impl_window)->input_window;

  if (gdkdev->info.mode != GDK_MODE_DISABLED &&
      iw != NULL)
    {
      for (l = iw->windows; l != NULL; l = l->next)
	{
	  w = l->data;
	  if (gdkdev->info.has_cursor || (w->extension_events & GDK_ALL_DEVICES_MASK))
	    event_mask |= w->extension_events;
	}
    }
  event_mask &= ~GDK_ALL_DEVICES_MASK;

  if (event_mask)
    event_mask |= GDK_PROXIMITY_OUT_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

  _gdk_input_common_find_events (gdkdev, event_mask,
				 classes, &num_classes);

  /* From X11 doc:
   * "XSelectExtensionEvent can generate a BadWindow or BadClass error."
   * In particular when a device is unplugged, a requested event class
   * could no longer be valid and raise a BadClass, which would cause
   * the program to crash.
   *
   * To handle this case gracefully, we simply ignore XSelectExtensionEvent() errors.
   * This is OK since there is no events to report for the unplugged device anyway.
   * So simply the device remains "silent".
   */
  old_handler = XSetErrorHandler (ignore_errors);
  XSelectExtensionEvent (GDK_WINDOW_XDISPLAY (impl_window),
			 GDK_WINDOW_XWINDOW (impl_window),
			 classes, num_classes);
  XSetErrorHandler (old_handler);
}

gint
_gdk_input_common_init (GdkDisplay *display,
			gint        include_core)
{
  XDeviceInfo   *devices;
  int num_devices, loop;
  int ignore, event_base;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  /* Init XInput extension */

  display_x11->input_devices = NULL;
  if (XQueryExtension (display_x11->xdisplay, "XInputExtension",
		       &ignore, &event_base, &ignore))
    {
      gdk_x11_register_standard_event_type (display,
					    event_base, 15 /* Number of events */);

      devices = XListInputDevices(display_x11->xdisplay, &num_devices);
      if (devices)
	{
	  for(loop=0; loop<num_devices; loop++)
	    {
	      GdkDevicePrivate *gdkdev = gdk_input_device_new(display,
							      &devices[loop],
							      include_core);
	      if (gdkdev)
		display_x11->input_devices = g_list_append(display_x11->input_devices, gdkdev);
	    }
	  XFreeDeviceList(devices);
	}
    }

  display_x11->input_devices = g_list_append (display_x11->input_devices, display->core_pointer);

  return TRUE;
}

static void
gdk_input_update_axes (GdkDevicePrivate *gdkdev,
		       gint             axes_count,
		       gint             first_axis,
		       gint             *axis_data)
{
  int i;
  g_return_if_fail (first_axis >= 0 && first_axis + axes_count <= gdkdev->info.num_axes);

  for (i = 0; i < axes_count; i++)
    gdkdev->axis_data[first_axis + i] = axis_data[i];
}

static void
gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
				 GdkWindow        *window,
				 gint             *axis_data,
				 gdouble          *axis_out,
				 gdouble          *x_out,
				 gdouble          *y_out)
{
  GdkWindowObject *priv, *impl_window;

  int i;
  int x_axis = 0;
  int y_axis = 0;

  double device_width, device_height, x_min, y_min;
  double x_offset, y_offset, x_scale, y_scale;

  priv = (GdkWindowObject *) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  x_axis = i;
	  break;
	case GDK_AXIS_Y:
	  y_axis = i;
	  break;
	default:
	  break;
	}
    }

  device_width = gdkdev->axes[x_axis].max_value - gdkdev->axes[x_axis].min_value;
  if (device_width > 0)
    {
      x_min = gdkdev->axes[x_axis].min_value;
    }
  else
    {
      device_width = gdk_screen_get_width (gdk_drawable_get_screen (window));
      x_min = 0;
    }

  device_height = gdkdev->axes[y_axis].max_value - gdkdev->axes[y_axis].min_value;
  if (device_height > 0)
    {
      y_min = gdkdev->axes[y_axis].min_value;
    }
  else
    {
      device_height = gdk_screen_get_height (gdk_drawable_get_screen (window));
      y_min = 0;
    }

  if (gdkdev->info.mode == GDK_MODE_SCREEN)
    {
      x_scale = gdk_screen_get_width (gdk_drawable_get_screen (window)) / device_width;
      y_scale = gdk_screen_get_height (gdk_drawable_get_screen (window)) / device_height;

      x_offset = - impl_window->input_window->root_x - priv->abs_x;
      y_offset = - impl_window->input_window->root_y - priv->abs_y;
    }
  else				/* GDK_MODE_WINDOW */
    {
      double x_resolution = gdkdev->axes[x_axis].resolution;
      double y_resolution = gdkdev->axes[y_axis].resolution;
      double device_aspect;
      /*
       * Some drivers incorrectly report the resolution of the device
       * as zero (in partiular linuxwacom < 0.5.3 with usb tablets).
       * This causes the device_aspect to become NaN and totally
       * breaks windowed mode.  If this is the case, the best we can
       * do is to assume the resolution is non-zero is equal in both
       * directions (which is true for many devices).  The absolute
       * value of the resolution doesn't matter since we only use the
       * ratio.
       */
      if ((x_resolution == 0) || (y_resolution == 0))
	{
	  x_resolution = 1;
	  y_resolution = 1;
	}
      device_aspect = (device_height*y_resolution) /
	(device_width*x_resolution);
      if (device_aspect * priv->width >= priv->height)
	{
	  /* device taller than window */
	  x_scale = priv->width / device_width;
	  y_scale = (x_scale * x_resolution) / y_resolution;

	  x_offset = 0;
	  y_offset = -(device_height * y_scale -  priv->height)/2;
	}
      else
	{
	  /* window taller than device */
	  y_scale = priv->height / device_height;
	  x_scale = (y_scale * y_resolution) / x_resolution;

	  y_offset = 0;
	  x_offset = - (device_width * x_scale - priv->width)/2;
	}
    }

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  axis_out[i] = x_offset + x_scale * (axis_data[x_axis] - x_min);
	  if (x_out)
	    *x_out = axis_out[i];
	  break;
	case GDK_AXIS_Y:
	  axis_out[i] = y_offset + y_scale * (axis_data[y_axis] - y_min);
	  if (y_out)
	    *y_out = axis_out[i];
	  break;
	default:
	  axis_out[i] =
	    (gdkdev->info.axes[i].max * (axis_data[i] - gdkdev->axes[i].min_value) +
	     gdkdev->info.axes[i].min * (gdkdev->axes[i].max_value - axis_data[i])) /
	    (gdkdev->axes[i].max_value - gdkdev->axes[i].min_value);
	  break;
	}
    }
}

/* combine the state of the core device and the device state
 * into one - for now we do this in a simple-minded manner -
 * we just take the keyboard portion of the core device and
 * the button portion (all of?) the device state.
 * Any button remapping should go on here.
 */
static guint
gdk_input_translate_state(guint state, guint device_state)
{
  return device_state | (state & 0xFF);
}


gboolean
_gdk_input_common_other_event (GdkEvent         *event,
			       XEvent           *xevent,
			       GdkWindow        *window,
			       GdkDevicePrivate *gdkdev)
{
  GdkWindowObject *priv, *impl_window;
  GdkInputWindow *input_window;

  priv = (GdkWindowObject *) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);
  input_window = impl_window->input_window;

  if ((xevent->type == gdkdev->buttonpress_type) ||
      (xevent->type == gdkdev->buttonrelease_type))
    {
      XDeviceButtonEvent *xdbe = (XDeviceButtonEvent *)(xevent);

      g_return_val_if_fail (xdbe->button < 256, FALSE);
      if (xdbe->type == gdkdev->buttonpress_type)
	{
	  event->button.type = GDK_BUTTON_PRESS;
	  if (!(gdkdev->button_state[xdbe->button/8] & 1 << (xdbe->button%8)))
	    {
	      gdkdev->button_state[xdbe->button/8] |= 1 << (xdbe->button%8);
	      gdkdev->button_count++;
	    }
	}
      else
	{
	  event->button.type = GDK_BUTTON_RELEASE;
	  if (gdkdev->button_state[xdbe->button/8] & 1 << (xdbe->button%8))
	    {
	      gdkdev->button_state[xdbe->button/8] &= ~(1 << (xdbe->button%8));
	      gdkdev->button_count--;
	    }
	}
      event->button.device = &gdkdev->info;
      event->button.window = window;
      event->button.time = xdbe->time;

      event->button.axes = g_new (gdouble, gdkdev->info.num_axes);
      gdk_input_update_axes (gdkdev, xdbe->axes_count, xdbe->first_axis,
			     xdbe->axis_data);
      gdk_input_translate_coordinates (gdkdev, window, gdkdev->axis_data,
				       event->button.axes,
				       &event->button.x, &event->button.y);
      event->button.x_root = event->button.x + priv->abs_x + input_window->root_x;
      event->button.y_root = event->button.y + priv->abs_y + input_window->root_y;
      event->button.state = gdk_input_translate_state (xdbe->state,xdbe->device_state);
      event->button.button = xdbe->button;

      if (event->button.type == GDK_BUTTON_PRESS)
	_gdk_event_button_generate (gdk_drawable_get_display (event->button.window),
				    event);

      GDK_NOTE (EVENTS,
	g_print ("button %s:\t\twindow: %ld  device: %ld  x,y: %f %f  button: %d\n",
		 (event->button.type == GDK_BUTTON_PRESS) ? "press" : "release",
		 xdbe->window,
		 xdbe->deviceid,
		 event->button.x, event->button.y,
		 xdbe->button));

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
  }

  if ((xevent->type == gdkdev->keypress_type) ||
      (xevent->type == gdkdev->keyrelease_type))
    {
      XDeviceKeyEvent *xdke = (XDeviceKeyEvent *)(xevent);

      GDK_NOTE (EVENTS,
	g_print ("device key %s:\twindow: %ld  device: %ld  keycode: %d\n",
		 (event->key.type == GDK_KEY_PRESS) ? "press" : "release",
		 xdke->window,
		 xdke->deviceid,
		 xdke->keycode));

      if (xdke->keycode < gdkdev->min_keycode ||
	  xdke->keycode >= gdkdev->min_keycode + gdkdev->info.num_keys)
	{
	  g_warning ("Invalid device key code received");
	  return FALSE;
	}

      event->key.keyval = gdkdev->info.keys[xdke->keycode - gdkdev->min_keycode].keyval;

      if (event->key.keyval == 0)
	{
	  GDK_NOTE (EVENTS,
	    g_print ("\t\ttranslation - NONE\n"));

	  return FALSE;
	}

      event->key.type = (xdke->type == gdkdev->keypress_type) ?
	GDK_KEY_PRESS : GDK_KEY_RELEASE;

      event->key.window = window;
      event->key.time = xdke->time;

      event->key.state = gdk_input_translate_state(xdke->state, xdke->device_state)
	| gdkdev->info.keys[xdke->keycode - gdkdev->min_keycode].modifiers;

      /* Add a string translation for the key event */
      if ((event->key.keyval >= 0x20) && (event->key.keyval <= 0xFF))
	{
	  event->key.length = 1;
	  event->key.string = g_new (gchar, 2);
	  event->key.string[0] = (gchar)event->key.keyval;
	  event->key.string[1] = 0;
	}
      else
	{
	  event->key.length = 0;
	  event->key.string = g_new0 (gchar, 1);
	}

      GDK_NOTE (EVENTS,
	g_print ("\t\ttranslation - keyval: %d modifiers: %#x\n",
		 event->key.keyval,
		 event->key.state));

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
    }

  if (xevent->type == gdkdev->motionnotify_type)
    {
      XDeviceMotionEvent *xdme = (XDeviceMotionEvent *)(xevent);

      event->motion.device = &gdkdev->info;

      event->motion.axes = g_new (gdouble, gdkdev->info.num_axes);
      gdk_input_update_axes (gdkdev, xdme->axes_count, xdme->first_axis, xdme->axis_data);
      gdk_input_translate_coordinates(gdkdev, window, gdkdev->axis_data,
				      event->motion.axes,
				      &event->motion.x,&event->motion.y);
      event->motion.x_root = event->motion.x + priv->abs_x + input_window->root_x;
      event->motion.y_root = event->motion.y + priv->abs_y + input_window->root_y;

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xdme->time;
      event->motion.state = gdk_input_translate_state(xdme->state,
						      xdme->device_state);
      event->motion.is_hint = xdme->is_hint;

      GDK_NOTE (EVENTS,
	g_print ("motion notify:\t\twindow: %ld  device: %ld  x,y: %f %f  state %#4x  hint: %s\n",
		 xdme->window,
		 xdme->deviceid,
		 event->motion.x, event->motion.y,
		 event->motion.state,
		 (xdme->is_hint) ? "true" : "false"));


      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
    }

  if (xevent->type == gdkdev->devicestatenotify_type)
    {
      int i;
      XDeviceStateNotifyEvent *xdse = (XDeviceStateNotifyEvent *)(xevent);
      XInputClass *input_class = (XInputClass *)xdse->data;
      for (i=0; i<xdse->num_classes; i++)
	{
	  if (input_class->class == ValuatorClass)
	    gdk_input_update_axes (gdkdev, gdkdev->info.num_axes, 0,
				   ((XValuatorState *)input_class)->valuators);
	  input_class = (XInputClass *)(((char *)input_class)+input_class->length);
	}

      GDK_NOTE (EVENTS,
	g_print ("device state notify:\t\twindow: %ld  device: %ld\n",
		 xdse->window,
		 xdse->deviceid));

      return FALSE;
    }
  if (xevent->type == gdkdev->proximityin_type ||
      xevent->type == gdkdev->proximityout_type)
    {
      XProximityNotifyEvent *xpne = (XProximityNotifyEvent *)(xevent);

      event->proximity.device = &gdkdev->info;
      event->proximity.type = (xevent->type == gdkdev->proximityin_type)?
	GDK_PROXIMITY_IN:GDK_PROXIMITY_OUT;
      event->proximity.window = window;
      event->proximity.time = xpne->time;

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
  }

  return FALSE;			/* wasn't one of our event types */
}

gboolean
_gdk_input_common_event_selected (GdkEvent         *event,
				  GdkWindow        *window,
				  GdkDevicePrivate *gdkdev)
{
  GdkWindowObject *priv = (GdkWindowObject *) window;

  switch (event->type) {
    case GDK_BUTTON_PRESS:
      return priv->extension_events & GDK_BUTTON_PRESS_MASK;

    case GDK_BUTTON_RELEASE:
      return priv->extension_events & GDK_BUTTON_RELEASE_MASK;

    case GDK_KEY_PRESS:
      return priv->extension_events & GDK_KEY_PRESS_MASK;

    case GDK_KEY_RELEASE:
      return priv->extension_events & GDK_KEY_RELEASE_MASK;

    case GDK_MOTION_NOTIFY:
      if (priv->extension_events & GDK_POINTER_MOTION_MASK)
	return TRUE;
      if (gdkdev->button_count && (priv->extension_events & GDK_BUTTON_MOTION_MASK))
	return TRUE;

      if ((gdkdev->button_state[0] & 1 << 1) && (priv->extension_events & GDK_BUTTON1_MOTION_MASK))
	return TRUE;
      if ((gdkdev->button_state[0] & 1 << 2) && (priv->extension_events & GDK_BUTTON2_MOTION_MASK))
	return TRUE;
      if ((gdkdev->button_state[0] & 1 << 3) && (priv->extension_events & GDK_BUTTON3_MOTION_MASK))
	return TRUE;

      return FALSE;

    case GDK_PROXIMITY_IN:
	  return priv->extension_events & GDK_PROXIMITY_IN_MASK;

    case GDK_PROXIMITY_OUT:
	  return priv->extension_events & GDK_PROXIMITY_OUT_MASK;

    default:
      return FALSE;
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
  GdkTimeCoord **coords;
  XDeviceTimeCoord *device_coords;
  GdkWindow *impl_window;
  GdkDevicePrivate *gdkdev;
  gint mode_return;
  gint axis_count_return;
  gint i;

  gdkdev = (GdkDevicePrivate *)device;

  impl_window = _gdk_window_get_impl_window (window);

  device_coords = XGetDeviceMotionEvents (GDK_WINDOW_XDISPLAY (impl_window),
					  gdkdev->xdevice,
					  start, stop,
					  n_events, &mode_return,
					  &axis_count_return);

  if (device_coords)
    {
      coords = _gdk_device_allocate_history (device, *n_events);

      for (i = 0; i < *n_events; i++)
	{
	  coords[i]->time = device_coords[i].time;

	  gdk_input_translate_coordinates (gdkdev, window,
					   device_coords[i].data,
					   coords[i]->axes, NULL, NULL);
	}

      XFreeDeviceMotionEvents (device_coords);

      *events = coords;

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gdk_device_get_state:
 * @device: a #GdkDevice.
 * @window: a #GdkWindow.
 * @axes: an array of doubles to store the values of the axes of @device in,
 * or %NULL.
 * @mask: location to store the modifiers, or %NULL.
 *
 * Gets the current state of a device.
 */
void
gdk_device_get_state (GdkDevice       *device,
		      GdkWindow       *window,
		      gdouble         *axes,
		      GdkModifierType *mask)
{
  gint i;

  g_return_if_fail (device != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_IS_CORE (device))
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
      int (*old_handler) (Display *, XErrorEvent *);
      GdkDevicePrivate *gdkdev;
      XDeviceState *state = NULL;
      XInputClass *input_class;

      if (mask)
	gdk_window_get_pointer (window, NULL, NULL, mask);

      gdkdev = (GdkDevicePrivate *)device;

      /* From X11 doc: "XQueryDeviceState can generate a BadDevice error."
       * This would occur in particular when a device is unplugged,
       * which would cause the program to crash (see bug 575767).
       *
       * To handle this case gracefully, we simply ignore the device.
       * GTK+ 3 handles this better with XInput 2's hotplugging support;
       * but this is better than a crash in GTK+ 2.
       */
      old_handler = XSetErrorHandler (ignore_errors);
      state = XQueryDeviceState (GDK_WINDOW_XDISPLAY (window),
				 gdkdev->xdevice);
      XSetErrorHandler (old_handler);

      if (! state)
        return;

      input_class = state->data;
      for (i=0; i<state->num_classes; i++)
	{
	  switch (input_class->class)
	    {
	    case ValuatorClass:
	      if (axes)
		gdk_input_translate_coordinates (gdkdev, window,
						 ((XValuatorState *)input_class)->valuators,
						 axes, NULL, NULL);
	      break;

	    case ButtonClass:
	      if (mask)
		{
		  *mask &= 0xFF;
		  if (((XButtonState *)input_class)->num_buttons > 0)
		    *mask |= ((XButtonState *)input_class)->buttons[0] << 7;
		  /* GDK_BUTTON1_MASK = 1 << 8, and button n is stored
		   * in bit 1<<(n%8) in byte n/8. n = 1,2,... */
		}
	      break;
	    }
	  input_class = (XInputClass *)(((char *)input_class)+input_class->length);
	}
      XFreeDeviceState (state);
    }
}

#define __GDK_INPUT_X11_C__
#include "gdkaliasdef.c"
