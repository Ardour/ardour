/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002 Hans Breuer
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
#include "gdkprivate-win32.h"

static GdkColormap *default_colormap = NULL;

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  return _gdk_root;
}

GdkColormap *
gdk_screen_get_default_colormap (GdkScreen *screen)
{
  return default_colormap;
}

void
gdk_screen_set_default_colormap (GdkScreen   *screen,
				 GdkColormap *colormap)
{
  GdkColormap *old_colormap;
  
  g_return_if_fail (screen == _gdk_screen);
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  old_colormap = default_colormap;

  default_colormap = g_object_ref (colormap);
  
  if (old_colormap)
    g_object_unref (old_colormap);
}

gint
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);

  return _gdk_num_monitors;
}

gint
gdk_screen_get_primary_monitor (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);

  return 0;
}

gint
gdk_screen_get_monitor_width_mm (GdkScreen *screen,
                                 gint       num_monitor)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);
  g_return_val_if_fail (num_monitor >= 0, 0);

  return _gdk_monitors[num_monitor].width_mm;
}

gint
gdk_screen_get_monitor_height_mm (GdkScreen *screen,
                                  gint       num_monitor)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);
  g_return_val_if_fail (num_monitor >= 0, 0);

  return _gdk_monitors[num_monitor].height_mm;
}

gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen,
                                  gint       num_monitor)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);
  g_return_val_if_fail (num_monitor >= 0, 0);

  return g_strdup (_gdk_monitors[num_monitor].name);
}

void
gdk_screen_get_monitor_geometry (GdkScreen    *screen, 
				 gint          num_monitor,
				 GdkRectangle *dest)
{
  g_return_if_fail (screen == _gdk_screen);
  g_return_if_fail (num_monitor < _gdk_num_monitors);
  g_return_if_fail (num_monitor >= 0);

  *dest = _gdk_monitors[num_monitor].rect;
}

GdkColormap *
gdk_screen_get_rgba_colormap (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, NULL);

  return NULL;
}
  
GdkVisual *
gdk_screen_get_rgba_visual (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, NULL);

  return NULL;
}
  
gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);  
  
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

  return FALSE;
}
