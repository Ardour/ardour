/* gdkdisplay-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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
#include "gdkprivate-quartz.h"
#include "gdkscreen-quartz.h"

GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* FIXME: Implement */

  return NULL;
}

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  g_assert (display == NULL || _gdk_display == display);
}

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL;

  /* Initialize application */
  [NSApplication sharedApplication];

  _gdk_display = g_object_new (GDK_TYPE_DISPLAY, NULL);

  _gdk_visual_init ();

  _gdk_screen = _gdk_screen_quartz_new ();

  _gdk_windowing_window_init ();

  _gdk_events_init ();
  _gdk_input_init ();

#if 0
  /* FIXME: Remove the #if 0 when we have these functions */
  _gdk_dnd_init ();
#endif

  g_signal_emit_by_name (gdk_display_manager_get (),
			 "display_opened", _gdk_display);

  return _gdk_display;
}

const gchar *
gdk_display_get_name (GdkDisplay *display)
{
  static gchar *display_name = NULL;

  if (!display_name)
    {
      GDK_QUARTZ_ALLOC_POOL;
      display_name = g_strdup ([[[NSHost currentHost] name] UTF8String]);
      GDK_QUARTZ_RELEASE_POOL;
    }

  return display_name;
}

int
gdk_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return _gdk_screen;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
}

void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  NSBeep();
}

gboolean 
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* FIXME: Implement */
  return FALSE;
}

gboolean 
gdk_display_request_selection_notification (GdkDisplay *display,
                                            GdkAtom     selection)

{
  /* FIXME: Implement */
  return FALSE;
}

gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

gboolean 
gdk_display_supports_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

gboolean 
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

void
gdk_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
  /* FIXME: Implement */
}


gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

gulong
_gdk_windowing_window_get_next_serial (GdkDisplay *display)
{
  return 0;
}
