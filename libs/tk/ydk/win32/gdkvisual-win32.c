/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include <stdlib.h>

#include "gdkvisual.h"
#include "gdkscreen.h" /* gdk_screen_get_default() */
#include "gdkprivate-win32.h"

static void  gdk_visual_decompose_mask (gulong     mask,
					gint      *shift,
					gint      *prec);

static GdkVisual *system_visual = NULL;

static gint available_depths[1];

static GdkVisualType available_types[1];

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

void
_gdk_visual_init (void)
{
  struct
  {
    BITMAPINFOHEADER bi;
    union
    {
      RGBQUAD colors[256];
      DWORD fields[256];
    } u;
  } bmi;
  HBITMAP hbm;

  const gint rastercaps = GetDeviceCaps (_gdk_display_hdc, RASTERCAPS);
  const int numcolors = GetDeviceCaps (_gdk_display_hdc, NUMCOLORS);
  gint bitspixel = GetDeviceCaps (_gdk_display_hdc, BITSPIXEL);
  gint map_entries = 0;

  system_visual = g_object_new (GDK_TYPE_VISUAL, NULL);

  GDK_NOTE (COLORMAP, g_print ("BITSPIXEL=%d NUMCOLORS=%d\n",
			       bitspixel, numcolors));

  if (rastercaps & RC_PALETTE)
    {
      const int sizepalette = GetDeviceCaps (_gdk_display_hdc, SIZEPALETTE);
      gchar *max_colors = getenv ("GDK_WIN32_MAX_COLORS");
      system_visual->type = GDK_VISUAL_PSEUDO_COLOR;

      GDK_NOTE (COLORMAP, g_print ("SIZEPALETTE=%d\n", sizepalette));
      g_assert (sizepalette == 256);

      if (max_colors != NULL)
	_gdk_max_colors = atoi (max_colors);
      
      map_entries = _gdk_max_colors;

      if (map_entries >= 16 && map_entries < sizepalette)
	{
	  /* The calls to gdk_rgb_set_min_colors() here have knowledge
	   * of what color cubes gdk_rgb_do_colormaps() will try, and
	   * of the static system palette colors... XXX
	   */
	  if (map_entries < 32)
	    {
	      map_entries = 16;
	      system_visual->type = GDK_VISUAL_STATIC_COLOR;
	      bitspixel = 4;
	      gdk_rgb_set_min_colors (2*2*2);
	    }
	  else if (map_entries < 64)
	    {
	      map_entries = 32;
	      bitspixel = 5;
	      gdk_rgb_set_min_colors (3*3*3);
	    }
	  else if (map_entries < 128)
	    {
	      map_entries = 64;
	      bitspixel = 6;
	      gdk_rgb_set_min_colors (3*3*3);
	    }
	  else if (map_entries < 256)
	    {
	      map_entries = 128;
	      bitspixel = 7;
	      gdk_rgb_set_min_colors (5*5*4);
	    }
	  else
	    g_assert_not_reached ();
	}
      else
	map_entries = sizepalette;
    }
  else if (bitspixel == 1 && numcolors == 16)
    {
      bitspixel = 4;
      system_visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 16;
    }
  else if (bitspixel == 1)
    {
      system_visual->type = GDK_VISUAL_STATIC_GRAY;
      map_entries = 2;
    }
  else if (bitspixel == 4)
    {
      system_visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 16;
    }
  else if (bitspixel == 8)
    {
      system_visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 256;
    }
  else if (bitspixel == 16)
    {
      system_visual->type = GDK_VISUAL_TRUE_COLOR;
#if 1
      /* This code by Mike Enright,
       * see http://www.users.cts.com/sd/m/menright/display.html
       */
      memset (&bmi, 0, sizeof (bmi));
      bmi.bi.biSize = sizeof (bmi.bi);

      hbm = CreateCompatibleBitmap (_gdk_display_hdc, 1, 1);
      GetDIBits (_gdk_display_hdc, hbm, 0, 1, NULL,
		 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
      GetDIBits (_gdk_display_hdc, hbm, 0, 1, NULL,
		 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
      DeleteObject (hbm);

      if (bmi.bi.biCompression != BI_BITFIELDS)
	{
	  /* Either BI_RGB or BI_RLE_something
	   * .... or perhaps (!!) something else.
	   * Theoretically biCompression might be
	   * mmioFourCC('c','v','i','d') but I doubt it.
	   */
	  if (bmi.bi.biCompression == BI_RGB)
	    {
	      /* It's 555 */
	      bitspixel = 15;
	      system_visual->red_mask   = 0x00007C00;
	      system_visual->green_mask = 0x000003E0;
	      system_visual->blue_mask  = 0x0000001F;
	    }
	  else
	    {
	      g_assert_not_reached ();
	    }
	}
      else
	{
	  DWORD allmasks =
	    bmi.u.fields[0] | bmi.u.fields[1] | bmi.u.fields[2];
	  int k = 0;
	  while (allmasks)
	    {
	      if (allmasks&1)
		k++;
	      allmasks/=2;
	    }
	  bitspixel = k;
	  system_visual->red_mask = bmi.u.fields[0];
	  system_visual->green_mask = bmi.u.fields[1];
	  system_visual->blue_mask  = bmi.u.fields[2];
	}
#else
      /* Old, incorrect (but still working) code. */
#if 0
      system_visual->red_mask   = 0x0000F800;
      system_visual->green_mask = 0x000007E0;
      system_visual->blue_mask  = 0x0000001F;
#else
      system_visual->red_mask   = 0x00007C00;
      system_visual->green_mask = 0x000003E0;
      system_visual->blue_mask  = 0x0000001F;
#endif
#endif
    }
  else if (bitspixel == 24 || bitspixel == 32)
    {
      bitspixel = 24;
      system_visual->type = GDK_VISUAL_TRUE_COLOR;
      system_visual->red_mask   = 0x00FF0000;
      system_visual->green_mask = 0x0000FF00;
      system_visual->blue_mask  = 0x000000FF;
    }
  else
    g_error ("_gdk_visual_init: unsupported BITSPIXEL: %d\n", bitspixel);

  system_visual->depth = bitspixel;
  system_visual->byte_order = GDK_LSB_FIRST;
  system_visual->bits_per_rgb = 42; /* Not used? */

  if ((system_visual->type == GDK_VISUAL_TRUE_COLOR) ||
      (system_visual->type == GDK_VISUAL_DIRECT_COLOR))
    {
      gdk_visual_decompose_mask (system_visual->red_mask,
				 &system_visual->red_shift,
				 &system_visual->red_prec);

      gdk_visual_decompose_mask (system_visual->green_mask,
				 &system_visual->green_shift,
				 &system_visual->green_prec);

      gdk_visual_decompose_mask (system_visual->blue_mask,
				 &system_visual->blue_shift,
				 &system_visual->blue_prec);
      map_entries = 1 << (MAX (system_visual->red_prec,
			       MAX (system_visual->green_prec,
				    system_visual->blue_prec)));
    }
  else
    {
      system_visual->red_mask = 0;
      system_visual->red_shift = 0;
      system_visual->red_prec = 0;

      system_visual->green_mask = 0;
      system_visual->green_shift = 0;
      system_visual->green_prec = 0;

      system_visual->blue_mask = 0;
      system_visual->blue_shift = 0;
      system_visual->blue_prec = 0;
    }
  system_visual->colormap_size = map_entries;

  available_depths[0] = system_visual->depth;
  available_types[0] = system_visual->type;
}

gint
gdk_visual_get_best_depth (void)
{
  return available_depths[0];
}

GdkVisualType
gdk_visual_get_best_type (void)
{
  return available_types[0];
}

GdkVisual*
gdk_screen_get_system_visual (GdkScreen *screen)
{
  return system_visual;
}

GdkVisual*
gdk_visual_get_best (void)
{
  return ((GdkVisual*) system_visual);
}

GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  if (depth == system_visual->depth)
    return (GdkVisual*) system_visual;
  else
    return NULL;
}

GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  if (visual_type == system_visual->type)
    return system_visual;
  else
    return NULL;
}

GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  if ((depth == system_visual->depth) && (visual_type == system_visual->type))
    return system_visual;
  else
    return NULL;
}

void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  *count = 1;
  *depths = available_depths;
}

void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  *count = 1;
  *visual_types = available_types;
}

GList*
gdk_screen_list_visuals (GdkScreen *screen)
{
  return g_list_append (NULL, (gpointer) system_visual);
}

GdkScreen *
gdk_visual_get_screen (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return gdk_screen_get_default ();
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
