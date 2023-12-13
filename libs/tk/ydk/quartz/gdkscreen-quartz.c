/* gdkscreen-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
 * Copyright (C) 2009  Kristian Rietveld  <kris@gtk.org>
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
#include "gdk.h"
#include "gdkscreen-quartz.h"
#include "gdkprivate-quartz.h"
 

/* A couple of notes about this file are in order.  In GDK, a
 * GdkScreen can contain multiple monitors.  A GdkScreen has an
 * associated root window, in which the monitors are placed.  The
 * root window "spans" all monitors.  The origin is at the top-left
 * corner of the root window.
 *
 * Cocoa works differently.  The system has a "screen" (NSScreen) for
 * each monitor that is connected (note the conflicting definitions
 * of screen).  The screen containing the menu bar is screen 0 and the
 * bottom-left corner of this screen is the origin of the "monitor
 * coordinate space".  All other screens are positioned according to this
 * origin.  If the menu bar is on a secondary screen (for example on
 * a monitor hooked up to a laptop), then this screen is screen 0 and
 * other monitors will be positioned according to the "secondary screen".
 * The main screen is the monitor that shows the window that is currently
 * active (has focus), the position of the menu bar does not have influence
 * on this!
 *
 * Upon start up and changes in the layout of screens, we calculate the
 * size of the GdkScreen root window that is needed to be able to place
 * all monitors in the root window.  Once that size is known, we iterate
 * over the monitors and translate their Cocoa position to a position
 * in the root window of the GdkScreen.  This happens below in the
 * function gdk_screen_quartz_calculate_layout().
 *
 * A Cocoa coordinate is always relative to the origin of the monitor
 * coordinate space.  Such coordinates are mapped to their respective
 * position in the GdkScreen root window (_gdk_quartz_window_xy_to_gdk_xy)
 * and vice versa (_gdk_quartz_window_gdk_xy_to_xy).  Both functions can
 * be found in gdkwindow-quartz.c.  Note that Cocoa coordinates can have
 * negative values (in case a monitor is located left or below of screen 0),
 * but GDK coordinates can *not*!
 */

static void  gdk_screen_quartz_dispose          (GObject         *object);
static void  gdk_screen_quartz_finalize         (GObject         *object);
static void  gdk_screen_quartz_calculate_layout (GdkScreenQuartz *screen);

static void display_reconfiguration_callback (CGDirectDisplayID            display,
                                              CGDisplayChangeSummaryFlags  flags,
                                              void                        *userInfo);

G_DEFINE_TYPE (GdkScreenQuartz, _gdk_screen_quartz, GDK_TYPE_SCREEN);

static void
_gdk_screen_quartz_class_init (GdkScreenQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_screen_quartz_dispose;
  object_class->finalize = gdk_screen_quartz_finalize;
}

static void
_gdk_screen_quartz_init (GdkScreenQuartz *screen_quartz)
{
  GdkScreen *screen = GDK_SCREEN (screen_quartz);
  NSScreen *nsscreen;

  gdk_screen_set_default_colormap (screen,
                                   gdk_screen_get_system_colormap (screen));

  nsscreen = [[NSScreen screens] objectAtIndex:0];
  gdk_screen_set_resolution (screen,
                             72.0 * [nsscreen userSpaceScaleFactor]);

  gdk_screen_quartz_calculate_layout (screen_quartz);

  CGDisplayRegisterReconfigurationCallback (display_reconfiguration_callback,
                                            screen);

  screen_quartz->emit_monitors_changed = FALSE;
}

static void
gdk_screen_quartz_dispose (GObject *object)
{
  GdkScreenQuartz *screen = GDK_SCREEN_QUARTZ (object);

  if (screen->default_colormap)
    {
      g_object_unref (screen->default_colormap);
      screen->default_colormap = NULL;
    }

  if (screen->screen_changed_id)
    {
      g_source_remove (screen->screen_changed_id);
      screen->screen_changed_id = 0;
    }

  CGDisplayRemoveReconfigurationCallback (display_reconfiguration_callback,
                                          screen);

  G_OBJECT_CLASS (_gdk_screen_quartz_parent_class)->dispose (object);
}

static void
gdk_screen_quartz_screen_rects_free (GdkScreenQuartz *screen)
{
  screen->n_screens = 0;

  if (screen->screen_rects)
    {
      g_free (screen->screen_rects);
      screen->screen_rects = NULL;
    }
}

static void
gdk_screen_quartz_finalize (GObject *object)
{
  GdkScreenQuartz *screen = GDK_SCREEN_QUARTZ (object);

  gdk_screen_quartz_screen_rects_free (screen);
}


static void
gdk_screen_quartz_calculate_layout (GdkScreenQuartz *screen)
{
  NSArray *array;
  int i;
  int max_x, max_y;

  GDK_QUARTZ_ALLOC_POOL;

  gdk_screen_quartz_screen_rects_free (screen);

  array = [NSScreen screens];

  screen->width = 0;
  screen->height = 0;
  screen->min_x = 0;
  screen->min_y = 0;
  max_x = max_y = 0;

  /* We determine the minimum and maximum x and y coordinates
   * covered by the monitors.  From this we can deduce the width
   * and height of the root screen.
   */
  for (i = 0; i < [array count]; i++)
    {
      NSRect rect = [[array objectAtIndex:i] frame];

      screen->min_x = MIN (screen->min_x, rect.origin.x);
      max_x = MAX (max_x, rect.origin.x + rect.size.width);

      screen->min_y = MIN (screen->min_y, rect.origin.y);
      max_y = MAX (max_y, rect.origin.y + rect.size.height);
    }

  screen->width = max_x - screen->min_x;
  screen->height = max_y - screen->min_y;

  screen->n_screens = [array count];
  screen->screen_rects = g_new0 (GdkRectangle, screen->n_screens);

  for (i = 0; i < screen->n_screens; i++)
    {
      NSScreen *nsscreen;
      NSRect rect;

      nsscreen = [array objectAtIndex:i];
      rect = [nsscreen frame];

      screen->screen_rects[i].x = rect.origin.x - screen->min_x;
      screen->screen_rects[i].y
          = screen->height - (rect.origin.y + rect.size.height) + screen->min_y;
      screen->screen_rects[i].width = rect.size.width;
      screen->screen_rects[i].height = rect.size.height;
    }

  GDK_QUARTZ_RELEASE_POOL;
}


static void
process_display_reconfiguration (GdkScreenQuartz *screen)
{
  int width, height;

  width = gdk_screen_get_width (GDK_SCREEN (screen));
  height = gdk_screen_get_height (GDK_SCREEN (screen));

  gdk_screen_quartz_calculate_layout (GDK_SCREEN_QUARTZ (screen));

  _gdk_windowing_update_window_sizes (GDK_SCREEN (screen));

  if (screen->emit_monitors_changed)
    {
      g_signal_emit_by_name (screen, "monitors-changed");
      screen->emit_monitors_changed = FALSE;
    }

  if (width != gdk_screen_get_width (GDK_SCREEN (screen))
      || height != gdk_screen_get_height (GDK_SCREEN (screen)))
    g_signal_emit_by_name (screen, "size-changed");
}

static gboolean
screen_changed_idle (gpointer data)
{
  GdkScreenQuartz *screen = data;

  process_display_reconfiguration (data);

  screen->screen_changed_id = 0;

  return FALSE;
}

static void
display_reconfiguration_callback (CGDirectDisplayID            display,
                                  CGDisplayChangeSummaryFlags  flags,
                                  void                        *userInfo)
{
  GdkScreenQuartz *screen = userInfo;

  if (flags & kCGDisplayBeginConfigurationFlag)
    {
      /* Ignore the begin configuration signal. */
      return;
    }
  else
    {
      /* We save information about the changes, so we can emit
       * ::monitors-changed when appropriate.  This signal must be
       * emitted when the number, size of position of one of the
       * monitors changes.
       */
      if (flags & kCGDisplayMovedFlag
          || flags & kCGDisplayAddFlag
          || flags & kCGDisplayRemoveFlag
          || flags & kCGDisplayEnabledFlag
          || flags & kCGDisplayDisabledFlag)
        screen->emit_monitors_changed = TRUE;

      /* At this point Cocoa does not know about the new screen data
       * yet, so we delay our refresh into an idle handler.
       */
      if (!screen->screen_changed_id)
        screen->screen_changed_id = gdk_threads_add_idle (screen_changed_idle,
                                                          screen);
    }
}

GdkScreen *
_gdk_screen_quartz_new (void)
{
  return g_object_new (GDK_TYPE_SCREEN_QUARTZ, NULL);
}

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return _gdk_display;
}


GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return _gdk_root;
}

gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return 0;
}

gchar * 
_gdk_windowing_substitute_screen_number (const gchar *display_name,
					 int          screen_number)
{
  if (screen_number != 0)
    return NULL;

  return g_strdup (display_name);
}

GdkColormap*
gdk_screen_get_default_colormap (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_QUARTZ (screen)->default_colormap;
}

void
gdk_screen_set_default_colormap (GdkScreen   *screen,
				 GdkColormap *colormap)
{
  GdkColormap *old_colormap;
  
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  old_colormap = GDK_SCREEN_QUARTZ (screen)->default_colormap;

  GDK_SCREEN_QUARTZ (screen)->default_colormap = g_object_ref (colormap);
  
  if (old_colormap)
    g_object_unref (old_colormap);
}

gint
gdk_screen_get_width (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_QUARTZ (screen)->width;
}

gint
gdk_screen_get_height (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_QUARTZ (screen)->height;
}

static gint
get_mm_from_pixels (NSScreen *screen, int pixels)
{
  /* userSpaceScaleFactor is in "pixels per point", 
   * 72 is the number of points per inch, 
   * and 25.4 is the number of millimeters per inch.
   */
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
  float dpi = [screen userSpaceScaleFactor] * 72.0;
#else
  float dpi = 96.0 / 72.0;
#endif

  return (pixels / dpi) * 25.4;
}

static NSScreen *
get_nsscreen_for_monitor (gint monitor_num)
{
  NSArray *array;
  NSScreen *screen;

  GDK_QUARTZ_ALLOC_POOL;

  array = [NSScreen screens];
  screen = [array objectAtIndex:monitor_num];

  GDK_QUARTZ_RELEASE_POOL;

  return screen;
}

gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return get_mm_from_pixels (get_nsscreen_for_monitor (0),
                             GDK_SCREEN_QUARTZ (screen)->width);
}

gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return get_mm_from_pixels (get_nsscreen_for_monitor (0),
                             GDK_SCREEN_QUARTZ (screen)->height);
}

gint
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_QUARTZ (screen)->n_screens;
}

gint
gdk_screen_get_primary_monitor (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return 0;
}

gint
gdk_screen_get_monitor_width_mm	(GdkScreen *screen,
				 gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  g_return_val_if_fail (monitor_num < gdk_screen_get_n_monitors (screen), 0);
  g_return_val_if_fail (monitor_num >= 0, 0);

  return get_mm_from_pixels (get_nsscreen_for_monitor (monitor_num),
                             GDK_SCREEN_QUARTZ (screen)->screen_rects[monitor_num].width);
}

gint
gdk_screen_get_monitor_height_mm (GdkScreen *screen,
                                  gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  g_return_val_if_fail (monitor_num < gdk_screen_get_n_monitors (screen), 0);
  g_return_val_if_fail (monitor_num >= 0, 0);

  return get_mm_from_pixels (get_nsscreen_for_monitor (monitor_num),
                             GDK_SCREEN_QUARTZ (screen)->screen_rects[monitor_num].height);
}

gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen,
				  gint       monitor_num)
{
  /* FIXME: Is there some useful name we could use here? */
  return NULL;
}

void
gdk_screen_get_monitor_geometry (GdkScreen    *screen, 
				 gint          monitor_num,
				 GdkRectangle *dest)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (monitor_num < gdk_screen_get_n_monitors (screen));
  g_return_if_fail (monitor_num >= 0);

  *dest = GDK_SCREEN_QUARTZ (screen)->screen_rects[monitor_num];
}

gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup (gdk_display_get_name (_gdk_display));
}

GdkWindow *
gdk_screen_get_active_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

gboolean
gdk_screen_is_composited (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  return TRUE;
}
