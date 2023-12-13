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

#include "config.h"
#include <stdlib.h>
#include <sys/types.h>

#include "gdk.h"		/* For gdk_flush() */
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkinternals.h"	/* For scratch_image code */
#include "gdkalias.h"

/**
 * gdk_image_ref:
 * @image: a #GdkImage
 *
 * Deprecated function; use g_object_ref() instead.
 * 
 * Return value: the image
 *
 * Deprecated: 2.0: Use g_object_ref() instead.
 **/
GdkImage *
gdk_image_ref (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  return g_object_ref (image);
}

/**
 * gdk_image_unref:
 * @image: a #GdkImage
 *
 * Deprecated function; use g_object_unref() instead.
 *
 * Deprecated: 2.0: Use g_object_unref() instead.
 **/
void
gdk_image_unref (GdkImage *image)
{
  g_return_if_fail (GDK_IS_IMAGE (image));

  g_object_unref (image);
}

/**
 * gdk_image_get:
 * @drawable: a #GdkDrawable
 * @x: x coordinate in @window
 * @y: y coordinate in @window
 * @width: width of area in @window
 * @height: height of area in @window
 * 
 * This is a deprecated wrapper for gdk_drawable_get_image();
 * gdk_drawable_get_image() should be used instead. Or even better: in
 * most cases gdk_pixbuf_get_from_drawable() is the most convenient
 * choice.
 * 
 * Return value: a new #GdkImage or %NULL
 **/
GdkImage*
gdk_image_get (GdkWindow *drawable,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (x >= 0, NULL);
  g_return_val_if_fail (y >= 0, NULL);
  g_return_val_if_fail (width >= 0, NULL);
  g_return_val_if_fail (height >= 0, NULL);
  
  return gdk_drawable_get_image (drawable, x, y, width, height);
}

/**
 * gdk_image_set_colormap:
 * @image: a #GdkImage
 * @colormap: a #GdkColormap
 * 
 * Sets the colormap for the image to the given colormap.  Normally
 * there's no need to use this function, images are created with the
 * correct colormap if you get the image from a drawable. If you
 * create the image from scratch, use the colormap of the drawable you
 * intend to render the image to.
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
void
gdk_image_set_colormap (GdkImage       *image,
                        GdkColormap    *colormap)
{
  g_return_if_fail (GDK_IS_IMAGE (image));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  if (image->colormap != colormap)
    {
      if (image->colormap)
	g_object_unref (image->colormap);

      image->colormap = colormap;
      g_object_ref (image->colormap);
    }
}

/**
 * gdk_image_get_colormap:
 * @image: a #GdkImage
 * 
 * Retrieves the colormap for a given image, if it exists.  An image
 * will have a colormap if the drawable from which it was created has
 * a colormap, or if a colormap was set explicitely with
 * gdk_image_set_colormap().
 * 
 * Return value: colormap for the image
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
GdkColormap *
gdk_image_get_colormap (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  return image->colormap;
}

/**
 * gdk_image_get_image_type:
 * @image: a #GdkImage
 *
 * Determines the type of a given image.
 *
 * Return value: the #GdkImageType of the image
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
GdkImageType
gdk_image_get_image_type (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->type;
}

/**
 * gdk_image_get_visual:
 * @image: a #GdkImage
 *
 * Determines the visual that was used to create the image.
 *
 * Return value: a #GdkVisual
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
GdkVisual *
gdk_image_get_visual (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  return image->visual;
}

/**
 * gdk_image_get_byte_order:
 * @image: a #GdkImage
 *
 * Determines the byte order of the image.
 *
 * Return value: a #GdkVisual
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
GdkByteOrder
gdk_image_get_byte_order (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->byte_order;
}

/**
 * gdk_image_get_width:
 * @image: a #GdkImage
 *
 * Determines the width of the image.
 *
 * Return value: the width
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
gint
gdk_image_get_width (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->width;
}

/**
 * gdk_image_get_height:
 * @image: a #GdkImage
 *
 * Determines the height of the image.
 *
 * Return value: the height
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
gint
gdk_image_get_height (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->height;
}

/**
 * gdk_image_get_depth:
 * @image: a #GdkImage
 *
 * Determines the depth of the image.
 *
 * Return value: the depth
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
guint16
gdk_image_get_depth (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->depth;
}

/**
 * gdk_image_get_bytes_per_pixel:
 * @image: a #GdkImage
 *
 * Determines the number of bytes per pixel of the image.
 *
 * Return value: the bytes per pixel
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
guint16
gdk_image_get_bytes_per_pixel (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->bpp;
}

/**
 * gdk_image_get_bytes_per_line:
 * @image: a #GdkImage
 *
 * Determines the number of bytes per line of the image.
 *
 * Return value: the bytes per line
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
guint16
gdk_image_get_bytes_per_line (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->bpl;
}

/**
 * gdk_image_get_bits_per_pixel:
 * @image: a #GdkImage
 *
 * Determines the number of bits per pixel of the image.
 *
 * Return value: the bits per pixel
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 **/
guint16
gdk_image_get_bits_per_pixel (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  return image->bits_per_pixel;
}

/**
 * gdk_image_get_pixels:
 * @image: a #GdkImage
 *
 * Returns a pointer to the pixel data of the image.
 *
 * Returns: the pixel data of the image
 *
 * Since: 2.22
 *
 * Deprecated: 2.22: #GdkImage should not be used anymore.
 */
gpointer
gdk_image_get_pixels (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  return image->mem;
}

/* We have N_REGION GDK_SCRATCH_IMAGE_WIDTH x GDK_SCRATCH_IMAGE_HEIGHT regions divided
 * up between n_images different images. possible_n_images gives
 * various divisors of N_REGIONS. The reason for allowing this
 * flexibility is that we want to create as few images as possible,
 * but we want to deal with the abberant systems that have a SHMMAX
 * limit less than
 *
 * GDK_SCRATCH_IMAGE_WIDTH * GDK_SCRATCH_IMAGE_HEIGHT * N_REGIONS * 4 (384k)
 *
 * (Are there any such?)
 */
#define N_REGIONS 6
static const int possible_n_images[] = { 1, 2, 3, 6 };

/* We allocate one GdkScratchImageInfo structure for each
 * depth where we are allocating scratch images. (Future: one
 * per depth, per display)
 */
typedef struct _GdkScratchImageInfo GdkScratchImageInfo;

struct _GdkScratchImageInfo {
  gint depth;
  
  gint n_images;
  GdkImage *static_image[N_REGIONS];
  gint static_image_idx;

  /* In order to optimize filling fractions, we simultaneously fill in up
   * to three regions of size GDK_SCRATCH_IMAGE_WIDTH * GDK_SCRATCH_IMAGE_HEIGHT: one
   * for images that are taller than GDK_SCRATCH_IMAGE_HEIGHT / 2, and must
   * be tiled horizontally. One for images that are wider than
   * GDK_SCRATCH_IMAGE_WIDTH / 2 and must be tiled vertically, and a third
   * for images smaller than GDK_SCRATCH_IMAGE_HEIGHT / 2 x GDK_SCRATCH_IMAGE_WIDTH x 2
   * that we tile in horizontal rows.
   */
  gint horiz_idx;
  gint horiz_y;
  gint vert_idx;
  gint vert_x;
  
  /* tile_y1 and tile_y2 define the horizontal band into
   * which we are tiling images. tile_x is the x extent to
   * which that is filled
   */
  gint tile_idx;
  gint tile_x;
  gint tile_y1;
  gint tile_y2;

  GdkScreen *screen;
};

static GSList *scratch_image_infos = NULL;

static gboolean
allocate_scratch_images (GdkScratchImageInfo *info,
			 gint                 n_images,
			 gboolean             shared)
{
  gint i;
  
  for (i = 0; i < n_images; i++)
    {
      info->static_image[i] = _gdk_image_new_for_depth (info->screen,
							shared ? GDK_IMAGE_SHARED : GDK_IMAGE_NORMAL,
							NULL,
							GDK_SCRATCH_IMAGE_WIDTH * (N_REGIONS / n_images), 
							GDK_SCRATCH_IMAGE_HEIGHT,
							info->depth);
      
      if (!info->static_image[i])
	{
	  gint j;
	  
	  for (j = 0; j < i; j++)
	    g_object_unref (info->static_image[j]);
	  
	  return FALSE;
	}
    }
  
  return TRUE;
}

static void
scratch_image_info_display_closed (GdkDisplay          *display,
                                   gboolean             is_error,
                                   GdkScratchImageInfo *image_info)
{
  gint i;

  g_signal_handlers_disconnect_by_func (display,
                                        scratch_image_info_display_closed,
                                        image_info);

  scratch_image_infos = g_slist_remove (scratch_image_infos, image_info);

  for (i = 0; i < image_info->n_images; i++)
    g_object_unref (image_info->static_image[i]);

  g_free (image_info);
}

static GdkScratchImageInfo *
scratch_image_info_for_depth (GdkScreen *screen,
			      gint       depth)
{
  GSList *tmp_list;
  GdkScratchImageInfo *image_info;
  gint i;

  tmp_list = scratch_image_infos;
  while (tmp_list)
    {
      image_info = tmp_list->data;
      if (image_info->depth == depth && image_info->screen == screen)
	return image_info;
      
      tmp_list = tmp_list->next;
    }

  image_info = g_new (GdkScratchImageInfo, 1);

  image_info->depth = depth;
  image_info->screen = screen;

  g_signal_connect (gdk_screen_get_display (screen), "closed",
                    G_CALLBACK (scratch_image_info_display_closed),
                    image_info);

  /* Try to allocate as few possible shared images */
  for (i=0; i < G_N_ELEMENTS (possible_n_images); i++)
    {
      if (allocate_scratch_images (image_info, possible_n_images[i], TRUE))
	{
	  image_info->n_images = possible_n_images[i];
	  break;
	}
    }

  /* If that fails, just allocate N_REGIONS normal images */
  if (i == G_N_ELEMENTS (possible_n_images))
    {
      allocate_scratch_images (image_info, N_REGIONS, FALSE);
      image_info->n_images = N_REGIONS;
    }

  image_info->static_image_idx = 0;

  image_info->horiz_y = GDK_SCRATCH_IMAGE_HEIGHT;
  image_info->vert_x = GDK_SCRATCH_IMAGE_WIDTH;
  image_info->tile_x = GDK_SCRATCH_IMAGE_WIDTH;
  image_info->tile_y1 = image_info->tile_y2 = GDK_SCRATCH_IMAGE_HEIGHT;

  scratch_image_infos = g_slist_prepend (scratch_image_infos, image_info);

  return image_info;
}

/* Defining NO_FLUSH can cause inconsistent screen updates, but is useful
   for performance evaluation. */

#undef NO_FLUSH

#ifdef VERBOSE
static gint sincelast;
#endif

static gint
alloc_scratch_image (GdkScratchImageInfo *image_info)
{
  if (image_info->static_image_idx == N_REGIONS)
    {
#ifndef NO_FLUSH
      gdk_flush ();
#endif
#ifdef VERBOSE
      g_print ("flush, %d puts since last flush\n", sincelast);
      sincelast = 0;
#endif
      image_info->static_image_idx = 0;

      /* Mark all regions that we might be filling in as completely
       * full, to force new tiles to be allocated for subsequent
       * images
       */
      image_info->horiz_y = GDK_SCRATCH_IMAGE_HEIGHT;
      image_info->vert_x = GDK_SCRATCH_IMAGE_WIDTH;
      image_info->tile_x = GDK_SCRATCH_IMAGE_WIDTH;
      image_info->tile_y1 = image_info->tile_y2 = GDK_SCRATCH_IMAGE_HEIGHT;
    }
  return image_info->static_image_idx++;
}

/**
 * _gdk_image_get_scratch:
 * @screen: a #GdkScreen
 * @width: desired width
 * @height: desired height
 * @depth: depth of image 
 * @x: X location within returned image of scratch image
 * @y: Y location within returned image of scratch image
 * 
 * Allocates an image of size width/height, up to a maximum
 * of GDK_SCRATCH_IMAGE_WIDTHxGDK_SCRATCH_IMAGE_HEIGHT that is
 * suitable to use on @screen.
 * 
 * Return value: a scratch image. This must be used by a
 *  call to gdk_image_put() before any other calls to
 *  _gdk_image_get_scratch()
 **/
GdkImage *
_gdk_image_get_scratch (GdkScreen   *screen,
			gint	     width,			
			gint	     height,
			gint	     depth,
			gint	    *x,
			gint	    *y)
{
  GdkScratchImageInfo *image_info;
  GdkImage *image;
  gint idx;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  image_info = scratch_image_info_for_depth (screen, depth);

  if (width >= (GDK_SCRATCH_IMAGE_WIDTH >> 1))
    {
      if (height >= (GDK_SCRATCH_IMAGE_HEIGHT >> 1))
	{
	  idx = alloc_scratch_image (image_info);
	  *x = 0;
	  *y = 0;
	}
      else
	{
	  if (height + image_info->horiz_y > GDK_SCRATCH_IMAGE_HEIGHT)
	    {
	      image_info->horiz_idx = alloc_scratch_image (image_info);
	      image_info->horiz_y = 0;
	    }
	  idx = image_info->horiz_idx;
	  *x = 0;
	  *y = image_info->horiz_y;
	  image_info->horiz_y += height;
	}
    }
  else
    {
      if (height >= (GDK_SCRATCH_IMAGE_HEIGHT >> 1))
	{
	  if (width + image_info->vert_x > GDK_SCRATCH_IMAGE_WIDTH)
	    {
	      image_info->vert_idx = alloc_scratch_image (image_info);
	      image_info->vert_x = 0;
	    }
	  idx = image_info->vert_idx;
	  *x = image_info->vert_x;
	  *y = 0;
	  /* using 3 and -4 would be slightly more efficient on 32-bit machines
	     with > 1bpp displays */
	  image_info->vert_x += (width + 7) & -8;
	}
      else
	{
	  if (width + image_info->tile_x > GDK_SCRATCH_IMAGE_WIDTH)
	    {
	      image_info->tile_y1 = image_info->tile_y2;
	      image_info->tile_x = 0;
	    }
	  if (height + image_info->tile_y1 > GDK_SCRATCH_IMAGE_HEIGHT)
	    {
	      image_info->tile_idx = alloc_scratch_image (image_info);
	      image_info->tile_x = 0;
	      image_info->tile_y1 = 0;
	      image_info->tile_y2 = 0;
	    }
	  if (height + image_info->tile_y1 > image_info->tile_y2)
	    image_info->tile_y2 = height + image_info->tile_y1;
	  idx = image_info->tile_idx;
	  *x = image_info->tile_x;
	  *y = image_info->tile_y1;
	  image_info->tile_x += (width + 7) & -8;
	}
    }
  image = image_info->static_image[idx * image_info->n_images / N_REGIONS];
  *x += GDK_SCRATCH_IMAGE_WIDTH * (idx % (N_REGIONS / image_info->n_images));
#ifdef VERBOSE
  g_print ("index %d, x %d, y %d (%d x %d)\n", idx, *x, *y, width, height);
  sincelast++;
#endif
  return image;
}

GdkImage*
gdk_image_new (GdkImageType  type,
	       GdkVisual    *visual,
	       gint          width,
	       gint          height)
{
  return _gdk_image_new_for_depth (gdk_visual_get_screen (visual), type,
				   visual, width, height, -1);
}

#define __GDK_IMAGE_C__
#include "gdkaliasdef.c"
