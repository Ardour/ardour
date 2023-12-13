/* gdkgeometry-quartz.c
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

#include "gdkprivate-quartz.h"

void
_gdk_quartz_window_queue_translation (GdkWindow *window,
				      GdkGC     *gc,
                                      GdkRegion *area,
                                      gint       dx,
                                      gint       dy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = (GdkWindowImplQuartz *)private->impl;

  int i, n_rects;
  GdkRegion *intersection;
  GdkRectangle *rects;

  /* We will intersect the known region that needs display with the given
   * area.  This intersection will be translated by dx, dy.  For the end
   * result, we will also set that it needs display.
   */

  if (!impl->needs_display_region)
    return;

  intersection = gdk_region_copy (impl->needs_display_region);
  gdk_region_intersect (intersection, area);
  gdk_region_offset (intersection, dx, dy);

  gdk_region_get_rectangles (intersection, &rects, &n_rects);

  for (i = 0; i < n_rects; i++)
    _gdk_quartz_window_set_needs_display_in_rect (window, &rects[i]);

  g_free (rects);
  gdk_region_destroy (intersection);
}

gboolean
_gdk_quartz_window_queue_antiexpose (GdkWindow *window,
                                     GdkRegion *area)
{
  return FALSE;
}
