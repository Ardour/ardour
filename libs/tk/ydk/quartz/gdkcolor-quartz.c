/* gdkcolor-quartz.c
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

#include "gdkcolor.h"
#include "gdkprivate-quartz.h"

GType
gdk_colormap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkColormapClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkColormap),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkColormap",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkColormap *
gdk_colormap_new (GdkVisual *visual,
		  gint       private_cmap)
{
  g_return_val_if_fail (visual != NULL, NULL);

  /* FIXME: Implement */
  return NULL;
}

GdkColormap *
gdk_screen_get_system_colormap (GdkScreen *screen)
{
  static GdkColormap *colormap = NULL;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (!colormap)
    {
      colormap = g_object_new (GDK_TYPE_COLORMAP, NULL);

      colormap->visual = gdk_visual_get_system ();
      colormap->size = colormap->visual->colormap_size;
    }

  return colormap;
}


GdkColormap *
gdk_screen_get_rgba_colormap (GdkScreen *screen)
{
  static GdkColormap *colormap = NULL;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (!colormap)
    {
      colormap = g_object_new (GDK_TYPE_COLORMAP, NULL);

      colormap->visual = gdk_screen_get_rgba_visual (screen);
      colormap->size = colormap->visual->colormap_size;
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  /* FIXME: Implement */
  return 0;
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
  /* FIXME: Implement */
}

gboolean
gdk_colors_alloc (GdkColormap   *colormap,
		  gboolean       contiguous,
		  gulong        *planes,
		  gint           nplanes,
		  gulong        *pixels,
		  gint           npixels)
{
  return TRUE;
}

void
gdk_colors_free (GdkColormap *colormap,
		 gulong      *pixels,
		 gint         npixels,
		 gulong       planes)
{
}

void
gdk_colormap_free_colors (GdkColormap    *colormap,
                          const GdkColor *colors,
                          gint            n_colors)
{
  /* This function shouldn't do anything since colors are never allocated. */
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
			   GdkColor    *colors,
			   gint         ncolors,
			   gboolean     writeable,
			   gboolean     best_match,
			   gboolean    *success)
{
  int i;
  int alpha;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), ncolors);
  g_return_val_if_fail (colors != NULL, ncolors);
  g_return_val_if_fail (success != NULL, ncolors);

  if (gdk_colormap_get_visual (colormap)->depth == 32)
    alpha = 0xff;
  else
    alpha = 0;

  for (i = 0; i < ncolors; i++)
    {
      colors[i].pixel = alpha << 24 |
        ((colors[i].red >> 8) & 0xff) << 16 |
        ((colors[i].green >> 8) & 0xff) << 8 |
        ((colors[i].blue >> 8) & 0xff);
    }

  *success = TRUE;

  return 0;
}

void
gdk_colormap_query_color (GdkColormap *colormap,
			  gulong       pixel,
			  GdkColor    *result)
{
  result->red = pixel >> 16 & 0xff;
  result->red += result->red << 8;

  result->green = pixel >> 8 & 0xff;
  result->green += result->green << 8;

  result->blue = pixel & 0xff;
  result->blue += result->blue << 8;
}

GdkScreen*
gdk_colormap_get_screen (GdkColormap *cmap)
{
  g_return_val_if_fail (cmap != NULL, NULL);

  return gdk_screen_get_default ();
}

CGColorRef
_gdk_quartz_colormap_get_cgcolor_from_pixel (GdkDrawable *drawable,
                                             guint32      pixel)
{
  CGFloat components[4] = { 0.0f, };
  CGColorRef color;
  CGColorSpaceRef colorspace;
  const GdkVisual *visual;
  GdkColormap *colormap;

  colormap = gdk_drawable_get_colormap (drawable);
  if (colormap)
    visual = gdk_colormap_get_visual (colormap);
  else
    visual = gdk_visual_get_best_with_depth (gdk_drawable_get_depth (drawable));

  switch (visual->type)
    {
      case GDK_VISUAL_STATIC_GRAY:
      case GDK_VISUAL_GRAYSCALE:
        components[0] = (pixel & 0xff) / 255.0f;

        if (visual->depth == 1)
          components[0] = components[0] == 0.0f ? 0.0f : 1.0f;
        components[1] = 1.0f;

        colorspace = CGColorSpaceCreateWithName (kCGColorSpaceGenericGray);
        color = CGColorCreate (colorspace, components);
        CGColorSpaceRelease (colorspace);
        break;

      default:
        components[0] = (pixel >> 16 & 0xff) / 255.0;
        components[1] = (pixel >> 8  & 0xff) / 255.0;
        components[2] = (pixel       & 0xff) / 255.0;

        if (visual->depth == 32)
          components[3] = (pixel >> 24 & 0xff) / 255.0;
        else
          components[3] = 1.0;

        colorspace = CGColorSpaceCreateDeviceRGB ();
        color = CGColorCreate (colorspace, components);
        CGColorSpaceRelease (colorspace);
        break;
    }

  return color;
}

gboolean
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
  if (color->pixel < 0 || color->pixel >= colormap->size)
    return FALSE;

  return TRUE;
}

