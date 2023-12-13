/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_INPUT_WIN32_H__
#define __GDK_INPUT_WIN32_H__

#include <windows.h>
#include <wintab.h>

typedef struct _GdkAxisInfo    GdkAxisInfo;
typedef struct _GdkDevicePrivate GdkDevicePrivate;

/* information about a device axis */
struct _GdkAxisInfo
{
  /* calibrated resolution (for aspect ratio) - only relative values
     between axes used */
  gint resolution;
  
  /* calibrated minimum/maximum values */
  gint min_value, max_value;
};

struct _GdkDeviceClass
{
  GObjectClass parent_class;
};

struct _GdkDevicePrivate
{
  GdkDevice info;

  /* information about the axes */
  GdkAxisInfo *axes;

  gint button_state;

  gint *last_axis_data;

  /* WINTAB stuff: */
  HCTX hctx;
  /* Cursor number */
  UINT cursor;
  /* The cursor's CSR_PKTDATA */
  WTPKT pktdata;
  /* Azimuth and altitude axis */
  AXIS orientation_axes[2];
};

/* Addition used for extension_events mask */
#define GDK_ALL_DEVICES_MASK (1<<30)

struct _GdkInputWindow
{
  /* gdk window */
  GList *windows; /* GdkWindow:s with extension_events set */

  GdkWindow *impl_window; /* an impl window */

  /* position relative to root window */
  gint root_x;
  gint root_y;
};

/* Global data */

#define GDK_IS_CORE(d) (((GdkDevice *)(d)) == gdk_display_get_default ()->core_pointer)

extern GList *_gdk_input_devices;
extern GList *_gdk_input_windows;

extern gboolean _gdk_input_in_proximity;

/* Function declarations */
void             _gdk_init_input_core (GdkDisplay *display);

GdkTimeCoord ** _gdk_device_allocate_history (GdkDevice *device,
					      gint       n_events);

/* The following functions are provided by each implementation
 * (just wintab for now)
 */
void             _gdk_input_configure_event  (GdkWindow        *window);
gboolean         _gdk_input_other_event      (GdkEvent         *event,
					      MSG              *msg,
					      GdkWindow        *window);

void             _gdk_input_crossing_event   (GdkWindow        *window,
					      gboolean          enter);


/* These should be in gdkinternals.h */

GdkInputWindow  *_gdk_input_window_find      (GdkWindow        *window);

void             _gdk_input_window_destroy   (GdkWindow *window);

void             _gdk_input_select_events    (GdkWindow        *impl_window);
gint             _gdk_input_grab_pointer     (GdkWindow        *window,
					      gint              owner_events,
					      GdkEventMask      event_mask,
					      GdkWindow        *confine_to,
					      guint32           time);
void             _gdk_input_ungrab_pointer   (guint32           time);
gboolean         _gdk_device_get_history     (GdkDevice         *device,
					      GdkWindow         *window,
					      guint32            start,
					      guint32            stop,
					      GdkTimeCoord    ***events,
					      gint              *n_events);

void		_gdk_input_wintab_init_check (void);
void		_gdk_input_set_tablet_active (void);
void            _gdk_input_update_for_device_mode (GdkDevicePrivate *gdkdev);
void            _gdk_input_check_proximity (void);

#endif /* __GDK_INPUT_WIN32_H__ */
