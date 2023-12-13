/* gdkvisual-quartz.c
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

#include "gdkvisual.h"
#include "gdkprivate-quartz.h"

static GdkVisual *system_visual;
static GdkVisual *rgba_visual;
static GdkVisual *gray_visual;

static void
gdk_visual_finalize (GObject *object)
{
  g_error ("A GdkVisual object was finalized. This should not happen");
}

static void
gdk_visual_class_init (GObjectClass *class)
{
  class->finalize = gdk_visual_finalize;
}

GType
gdk_visual_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkVisualClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_visual_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkVisual),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkVisual",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_visual_decompose_mask (gulong  mask,
			   gint   *shift,
			   gint   *prec)
{
  *shift = 0;
  *prec = 0;

  while (!(mask & 0x1))
    {
      (*shift)++;
      mask >>= 1;
    }

  while (mask & 0x1)
    {
      (*prec)++;
      mask >>= 1;
    }
}

static GdkVisual *
create_standard_visual (gint depth)
{
  GdkVisual *visual = g_object_new (GDK_TYPE_VISUAL, NULL);

  visual->depth = depth;
  visual->byte_order = GDK_MSB_FIRST; /* FIXME: Should this be different on intel macs? */
  visual->colormap_size = 0;
  
  visual->type = GDK_VISUAL_TRUE_COLOR;

  visual->red_mask = 0xff0000;
  visual->green_mask = 0xff00;
  visual->blue_mask = 0xff;
  
  gdk_visual_decompose_mask (visual->red_mask,
			     &visual->red_shift,
			     &visual->red_prec);
  gdk_visual_decompose_mask (visual->green_mask,
			     &visual->green_shift,
			     &visual->green_prec);
  gdk_visual_decompose_mask (visual->blue_mask,
			     &visual->blue_shift,
			     &visual->blue_prec);

  return visual;
}

static GdkVisual *
create_gray_visual (void)
{
  GdkVisual *visual = g_object_new (GDK_TYPE_VISUAL, NULL);

  visual->depth = 1;
  visual->byte_order = GDK_MSB_FIRST;
  visual->colormap_size = 0;

  visual->type = GDK_VISUAL_STATIC_GRAY;

  return visual;
}

void
_gdk_visual_init (void)
{
  system_visual = create_standard_visual (24);
  rgba_visual = create_standard_visual (32);
  gray_visual = create_gray_visual ();
}

/* We prefer the system visual for now ... */
gint
gdk_visual_get_best_depth (void)
{
  return system_visual->depth;
}

GdkVisualType
gdk_visual_get_best_type (void)
{
  return system_visual->type;
}

GdkVisual *
gdk_screen_get_rgba_visual (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return rgba_visual;
}

GdkVisual*
gdk_screen_get_system_visual (GdkScreen *screen)
{
  return system_visual;
}

GdkVisual*
gdk_visual_get_best (void)
{
  return system_visual;
}

GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  GdkVisual *visual = NULL;

  switch (depth)
    {
      case 32:
        visual = rgba_visual;
        break;

      case 24:
        visual = system_visual;
        break;

      case 1:
        visual = gray_visual;
        break;

      default:
        visual = NULL;
    }

  return visual;
}

GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  if (system_visual->type == visual_type)
    return system_visual;
  else if (gray_visual->type == visual_type)
    return gray_visual;

  return NULL;
}

GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  if (system_visual->depth == depth
      && system_visual->type == visual_type)
    return system_visual;
  else if (rgba_visual->depth == depth
           && rgba_visual->type == visual_type)
    return rgba_visual;
  else if (gray_visual->depth == depth
           && gray_visual->type == visual_type)
    return gray_visual;

  return NULL;
}

/* For these, we also prefer the system visual */
void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  *count = 1;
  *depths = &system_visual->depth;
}

void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  *count = 1;
  *visual_types = &system_visual->type;
}

GList*
gdk_screen_list_visuals (GdkScreen *screen)
{
  GList *visuals = NULL;

  visuals = g_list_append (visuals, system_visual);
  visuals = g_list_append (visuals, rgba_visual);
  visuals = g_list_append (visuals, gray_visual);

  return visuals;
}

GdkScreen *
gdk_visual_get_screen (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return gdk_screen_get_default ();
}

