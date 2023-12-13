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

/* gdkgeometry-win32.c: emulation of 32 bit coordinates within the
 * limits of Win32 GDI. The idea of big window emulation is more or less
 * a copy of the X11 version, and the equvalent of guffaw scrolling
 * is ScrollWindowEx(). While we determine the invalidated region
 * ourself during scrolling, we do not pass SW_INVALIDATE to
 * ScrollWindowEx() to avoid a unnecessary WM_PAINT.
 *
 * Bits are always scrolled correctly by ScrollWindowEx(), but
 * some big children may hit the coordinate boundary (i.e.
 * win32_x/win32_y < -16383) after scrolling. They needed to be moved
 * back to the real position determined by gdk_window_compute_position().
 * This is handled in gdk_window_postmove().
 * 
 * The X11 version by Owen Taylor <otaylor@redhat.com>
 * Copyright Red Hat, Inc. 2000
 * Win32 hack by Tor Lillqvist <tml@iki.fi>
 * and Hans Breuer <hans@breuer.org>
 * Modified by Ivan, Wong Yat Cheung <email@ivanwong.info>
 * so that big window emulation finally works.
 */

#include "config.h"
#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkregion.h"
#include "gdkregion-generic.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

#define SIZE_LIMIT 32767

typedef struct _GdkWindowParentPos GdkWindowParentPos;

static void tmp_unset_bg (GdkWindow *window);
static void tmp_reset_bg (GdkWindow *window);

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  GDK_NOTE (MISC, g_print ("_gdk_window_move_resize_child: %s@%+d%+d %dx%d@%+d%+d\n",
			   _gdk_win32_drawable_description (window),
			   obj->x, obj->y, width, height, x, y));

  if (width > 65535 || height > 65535)
  {
    g_warning ("Native children wider or taller than 65535 pixels are not supported.");

    if (width > 65535)
      width = 65535;
    if (height > 65535)
      height = 65535;
  }

  obj->x = x;
  obj->y = y;
  obj->width = width;
  obj->height = height;

  _gdk_win32_window_tmp_unset_parent_bg (window);
  _gdk_win32_window_tmp_unset_bg (window, TRUE);
  
  GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%d,%d,"
			   "NOACTIVATE|NOZORDER)\n",
			   GDK_WINDOW_HWND (window),
			   obj->x + obj->parent->abs_x, obj->y + obj->parent->abs_y, 
			   width, height));

  API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			   obj->x + obj->parent->abs_x, obj->y + obj->parent->abs_y, 
			   width, height,
			   SWP_NOACTIVATE | SWP_NOZORDER));

  //_gdk_win32_window_tmp_reset_parent_bg (window);
  _gdk_win32_window_tmp_reset_bg (window, TRUE);
}

void
_gdk_win32_window_tmp_unset_bg (GdkWindow *window,
				gboolean recurse)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;

  if (private->input_only || private->destroyed ||
      (private->window_type != GDK_WINDOW_ROOT &&
       !GDK_WINDOW_IS_MAPPED (window)))
    return;

  if (_gdk_window_has_impl (window) &&
      GDK_WINDOW_IS_WIN32 (window) &&
      private->window_type != GDK_WINDOW_ROOT &&
      private->window_type != GDK_WINDOW_FOREIGN)
    tmp_unset_bg (window);

  if (recurse)
    {
      GList *l;

      for (l = private->children; l != NULL; l = l->next)
	_gdk_win32_window_tmp_unset_bg (l->data, TRUE);
    }
}

static void
tmp_unset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  impl->no_bg = TRUE;

  /*
   * The X version sets background = None to avoid updateing for a moment.
   * Not sure if this could really emulate it.
   */
  if (obj->bg_pixmap != GDK_NO_BG)
    {
      ///* handled in WM_ERASEBKGRND proceesing */;

      //HDC hdc = GetDC (GDK_WINDOW_HWND (window));
      //erase_background (window, hdc);
    }
}

static void
tmp_reset_bg (GdkWindow *window)
{
  GdkWindowObject *obj;
  GdkWindowImplWin32 *impl;

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  impl->no_bg = FALSE;
}

void
_gdk_win32_window_tmp_unset_parent_bg (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject*)window;

  if (GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT)
    return;

  window = _gdk_window_get_impl_window ((GdkWindow*)private->parent);
  _gdk_win32_window_tmp_unset_bg (window, FALSE);
}

void
_gdk_win32_window_tmp_reset_bg (GdkWindow *window,
				gboolean   recurse)
{
  GdkWindowObject *private = (GdkWindowObject*)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->input_only || private->destroyed ||
      (private->window_type != GDK_WINDOW_ROOT && !GDK_WINDOW_IS_MAPPED (window)))
    return;

  if (_gdk_window_has_impl (window) &&
      GDK_WINDOW_IS_WIN32 (window) &&
      private->window_type != GDK_WINDOW_ROOT &&
      private->window_type != GDK_WINDOW_FOREIGN)
    {
      tmp_reset_bg (window);
    }

  if (recurse)
    {
      GList *l;

      for (l = private->children; l != NULL; l = l->next)
	_gdk_win32_window_tmp_reset_bg (l->data, TRUE);
    }
}

/*
void
_gdk_win32_window_tmp_reset_bg (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  impl->no_bg = FALSE;
}
*/

#if 0
static GdkRegion *
gdk_window_clip_changed (GdkWindow    *window,
			 GdkRectangle *old_clip,
			 GdkRectangle *new_clip)
{
  GdkWindowImplWin32 *impl;
  GdkWindowObject *obj;
  GdkRegion *old_clip_region;
  GdkRegion *new_clip_region;
  
  if (((GdkWindowObject *)window)->input_only)
    return NULL;

  obj = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);
  
  old_clip_region = gdk_region_rectangle (old_clip);
  new_clip_region = gdk_region_rectangle (new_clip);

  /* Trim invalid region of window to new clip rectangle
   */
  if (obj->update_area)
    gdk_region_intersect (obj->update_area, new_clip_region);

  /* Invalidate newly exposed portion of window
   */
  gdk_region_subtract (new_clip_region, old_clip_region);
  if (!gdk_region_empty (new_clip_region))
    gdk_window_tmp_unset_bg (window);
  else
    {
      gdk_region_destroy (new_clip_region);
      new_clip_region = NULL;
    }

  gdk_region_destroy (old_clip_region);

  return new_clip_region;
}
#endif

#if 0
static void
gdk_window_post_scroll (GdkWindow    *window,
			GdkRegion    *new_clip_region)
{
  GDK_NOTE (EVENTS,
	    g_print ("gdk_window_clip_changed: invalidating region: %s\n",
		     _gdk_win32_gdkregion_to_string (new_clip_region)));

  gdk_window_invalidate_region (window, new_clip_region, FALSE);
  g_print ("gdk_window_post_scroll\n");
  gdk_region_destroy (new_clip_region);
}

#endif
