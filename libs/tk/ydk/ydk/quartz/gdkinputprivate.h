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

#ifndef __GDK_INPUTPRIVATE_H__
#define __GDK_INPUTPRIVATE_H__

#include "config.h"
#include "gdkinput.h"
#include "gdkevents.h"
#include "gdkquartz.h"

typedef struct _GdkAxisInfo    GdkAxisInfo;
typedef struct _GdkInputVTable GdkInputVTable;
typedef struct _GdkDevicePrivate GdkDevicePrivate;

struct _GdkInputVTable {
  gint (*set_mode) (guint32 deviceid, GdkInputMode mode);
  void (*set_axes) (guint32 deviceid, GdkAxisUse *axes);
  void (*set_key)  (guint32 deviceid,
		    guint   index,
		    guint   keyval,
		    GdkModifierType modifiers);
	
  GdkTimeCoord* (*motion_events) (GdkWindow *window,
				  guint32 deviceid,
				  guint32 start,
				  guint32 stop,
				  gint *nevents_return);
  void (*get_pointer)   (GdkWindow       *window,
			 guint32	  deviceid,
			 gdouble         *x,
			 gdouble         *y,
			 gdouble         *pressure,
			 gdouble         *xtilt,
			 gdouble         *ytilt,
			 GdkModifierType *mask);
  gint (*grab_pointer) (GdkWindow *     window,
			gint            owner_events,
			GdkEventMask    event_mask,
			GdkWindow *     confine_to,
			guint32         time);
  void (*ungrab_pointer) (guint32 time);

  void (*configure_event) (GdkEventConfigure *event, GdkWindow *window);
  void (*enter_event) (GdkEventCrossing *event, GdkWindow *window);
  gint (*other_event) (GdkEvent *event, GdkWindow *window);
  /* Handle an unidentified event. Returns TRUE if handled, FALSE
     otherwise */
  gint (*window_none_event) (GdkEvent *event);
  gint (*enable_window) (GdkWindow *window, GdkDevicePrivate *gdkdev);
  gint (*disable_window) (GdkWindow *window, GdkDevicePrivate *gdkdev);
};

/* information about a device axis */
struct _GdkAxisInfo
{
  /* reported x resolution */
  gint xresolution;

  /* reported x minimum/maximum values */
  gint xmin_value, xmax_value;

  /* calibrated resolution (for aspect ration) - only relative values
     between axes used */
  gint resolution;
  
  /* calibrated minimum/maximum values */
  gint min_value, max_value;
};

#define GDK_INPUT_NUM_EVENTC 6

struct _GdkDevicePrivate {
  GdkDevice  info;

  gint last_state;
  gdouble *last_axes_state;
};

struct _GdkDeviceClass
{
  GObjectClass parent_class;
};

struct _GdkInputWindow
{
  /* gdk window */
  GdkWindow *window;

  /* Extension mode (GDK_EXTENSION_EVENTS_ALL/CURSOR) */
  GdkExtensionMode mode;

  /* position relative to root window */
  gint root_x;
  gint root_y;

  /* rectangles relative to window of windows obscuring this one */
  GdkRectangle *obscuring;
  gint num_obscuring;

  /* Is there a pointer grab for this window ? */
  gint grabbed;
};

/* Global data */

extern const GdkDevice gdk_input_core_info;

extern GdkInputVTable gdk_input_vtable;
/* information about network port and host for gxid daemon */
extern gchar           *_gdk_input_gxid_host;
extern gint             _gdk_input_gxid_port;
extern gint             _gdk_input_ignore_core;

/* Function declarations */

GdkInputWindow *   _gdk_input_window_find    (GdkWindow        *window);
void               _gdk_input_window_destroy (GdkWindow        *window);
void               _gdk_input_init           (void);
void               _gdk_input_exit           (void);
gint               _gdk_input_enable_window  (GdkWindow        *window,
					      GdkDevicePrivate *gdkdev);
gint               _gdk_input_disable_window (GdkWindow        *window,
					      GdkDevicePrivate *gdkdev);
void               _gdk_init_input_core      (void);

void               _gdk_input_window_crossing (GdkWindow       *window,
                                               gboolean         enter);

void               _gdk_input_quartz_tablet_proximity (NSPointingDeviceType deviceType);
gboolean           _gdk_input_fill_quartz_input_event (GdkEvent  *event,
                                                       NSEvent   *nsevent,
                                                       GdkEvent  *input_event);

void _gdk_input_exit           (void);

#endif /* __GDK_INPUTPRIVATE_H__ */
