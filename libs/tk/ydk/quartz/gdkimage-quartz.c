/* gdkimage-quartz.c
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
#include "gdkimage.h"
#include "gdkprivate-quartz.h"

static GObjectClass *parent_class;

GdkImage *
_gdk_quartz_image_copy_to_image (GdkDrawable *drawable,
				 GdkImage    *image,
				 gint         src_x,
				 gint         src_y,
				 gint         dest_x,
				 gint         dest_y,
				 gint         width,
				 gint         height)
{
  GdkScreen *screen;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_QUARTZ (drawable), NULL);
  g_return_val_if_fail (image != NULL || (dest_x == 0 && dest_y == 0), NULL);

  screen = gdk_drawable_get_screen (drawable);
  if (!image)
    image = _gdk_image_new_for_depth (screen, GDK_IMAGE_FASTEST, NULL, 
				      width, height,
				      gdk_drawable_get_depth (drawable));
  
  if (GDK_IS_PIXMAP_IMPL_QUARTZ (drawable))
    {
      GdkPixmapImplQuartz *pix_impl;
      gint bytes_per_row;
      guchar *data;
      int x, y;

      pix_impl = GDK_PIXMAP_IMPL_QUARTZ (drawable);
      data = (guchar *)(pix_impl->data);

      if (src_x + width > pix_impl->width || src_y + height > pix_impl->height)
      	{
          g_warning ("Out of bounds copy-area for pixmap -> image conversion\n");
          return image;
        }

      switch (gdk_drawable_get_depth (drawable))
        {
        case 24:
          bytes_per_row = pix_impl->width * 4;
          for (y = 0; y < height; y++)
            {
              guchar *src = data + ((y + src_y) * bytes_per_row) + (src_x * 4);

              for (x = 0; x < width; x++)
                {
                  gint32 pixel;
	  
                  /* RGB24, 4 bytes per pixel, skip first. */
                  pixel = src[0] << 16 | src[1] << 8 | src[2];
                  src += 4;

                  gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
                }
            }
          break;

        case 32:
          bytes_per_row = pix_impl->width * 4;
          for (y = 0; y < height; y++)
            {
              guchar *src = data + ((y + src_y) * bytes_per_row) + (src_x * 4);

              for (x = 0; x < width; x++)
                {
                  gint32 pixel;
	  
                  /* ARGB32, 4 bytes per pixel. */
                  pixel = src[0] << 24 | src[1] << 16 | src[2] << 8 | src[3];
                  src += 4;

                  gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
                }
            }
          break;

        case 1: /* TODO: optimize */
          bytes_per_row = pix_impl->width;
          for (y = 0; y < height; y++)
            {
              guchar *src = data + ((y + src_y) * bytes_per_row) + src_x;

              for (x = 0; x < width; x++)
                {
                  gint32 pixel;
	  
                  /* 8 bits */
                  pixel = src[0];
                  src++;

                  gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
                }
            }
          break;

        default:
          g_warning ("Unsupported bit depth %d\n", gdk_drawable_get_depth (drawable));
          return image;
        }
    }
  else if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
    {
      GdkQuartzView *view;
      NSBitmapImageRep *rep;
      guchar *data;
      int x, y;
      NSSize size;
      NSBitmapFormat format;
      gboolean has_alpha;
      gint bpp;
      gint r_byte = 0;
      gint g_byte = 1;
      gint b_byte = 2;
      gint a_byte = 3;
      gboolean le_image_data = FALSE;

      if (GDK_WINDOW_IMPL_QUARTZ (drawable) == GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (_gdk_root)->impl))
        {
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
         return image;
#else
          /* Special case for the root window. */
	  CGRect rect = CGRectMake (src_x, src_y, width, height);
          CGImageRef root_image_ref = CGWindowListCreateImage (rect,
                                                               kCGWindowListOptionOnScreenOnly,
                                                               kCGNullWindowID,
                                                               kCGWindowImageDefault);

          /* HACK: the NSBitmapImageRep does not copy and convert
           * CGImageRef's data so it matches what NSBitmapImageRep can
           * express in its API (which is RGBA and ARGB, premultiplied
           * and unpremultiplied), it only references the CGImageRef.
           * Therefore we need to do the host byte swapping ourselves.
           */
          if (CGImageGetBitmapInfo (root_image_ref) & kCGBitmapByteOrder32Little)
            {
              r_byte = 3;
              g_byte = 2;
              b_byte = 1;
              a_byte = 0;

              le_image_data = TRUE;
            }

          rep = [[NSBitmapImageRep alloc] initWithCGImage: root_image_ref];
          CGImageRelease (root_image_ref);
#endif
        }
      else
        {
	  NSRect rect = NSMakeRect (src_x, src_y, width, height);
          view = GDK_WINDOW_IMPL_QUARTZ (drawable)->view;

          /* We return the image even if we can't copy to it. */
          if (![view lockFocusIfCanDraw])
            return image;

          rep = [[NSBitmapImageRep alloc] initWithFocusedViewRect: rect];
          [view unlockFocus];
        }

      data = [rep bitmapData];
      size = [rep size];
      format = [rep bitmapFormat];
      has_alpha = [rep hasAlpha];
      bpp = [rep bitsPerPixel] / 8;

      /* MORE HACK: AlphaFirst seems set for le_image_data, which is
       * technically correct, but really apple, are you kidding, it's
       * in fact ABGR, not ARGB as promised in NSBitmapImageRep's API.
       */
      if (!le_image_data && (format & NSAlphaFirstBitmapFormat))
        {
          r_byte = 1;
          g_byte = 2;
          b_byte = 3;
          a_byte = 0;
        }

      for (y = 0; y < size.height; y++)
        {
          guchar *src = data + y * [rep bytesPerRow];

          for (x = 0; x < size.width; x++)
            {
              guchar r = src[r_byte];
              guchar g = src[g_byte];
              guchar b = src[b_byte];
              gint32 pixel;

              if (has_alpha)
                {
                  guchar alpha = src[a_byte];

                  /* unpremultiply if alpha > 0 */
                  if (! (format & NSAlphaNonpremultipliedBitmapFormat) && alpha)
                    {
                      r = r * 255 / alpha;
                      g = g * 255 / alpha;
                      b = b * 255 / alpha;
                    }

                  if (image->byte_order == GDK_MSB_FIRST)
                    pixel = alpha | b << 8 | g << 16 | r << 24;
                  else
                    pixel = alpha << 24 | b << 16 | g << 8 | r;
                }
              else
                {
                  if (image->byte_order == GDK_MSB_FIRST)
                    pixel = b | g << 8 | r << 16;
                  else
                    pixel = b << 16 | g << 8 | r;
                }

              src += bpp;

              gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
            }
        }

      [rep release];
    }

  return image;
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  g_free (image->mem);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = gdk_image_finalize;
}

GType
gdk_image_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkImageClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkImage),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkImage",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkImage *
gdk_image_new_bitmap (GdkVisual *visual, gpointer data, gint width, gint height)
{
  /* We don't implement this function because it's broken, deprecated and 
   * tricky to implement. */
  g_warning ("This function is unimplemented");

  return NULL;
}

GdkImage*
_gdk_image_new_for_depth (GdkScreen    *screen,
			  GdkImageType  type,
			  GdkVisual    *visual,
			  gint          width,
			  gint          height,
			  gint          depth)
{
  GdkImage *image;

  if (visual)
    depth = visual->depth;

  g_assert (depth == 24 || depth == 32);

  image = g_object_new (gdk_image_get_type (), NULL);
  image->type = type;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = depth;

  image->byte_order = (G_BYTE_ORDER == G_LITTLE_ENDIAN) ? GDK_LSB_FIRST : GDK_MSB_FIRST;

  /* We only support images with bpp 4 */
  image->bpp = 4;
  image->bpl = image->width * image->bpp;
  image->bits_per_pixel = image->bpp * 8;
  
  image->mem = g_malloc (image->bpl * image->height);
  memset (image->mem, 0x00, image->bpl * image->height);

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  guchar *ptr;

  g_return_val_if_fail (image != NULL, 0);
  g_return_val_if_fail (x >= 0 && x < image->width, 0);
  g_return_val_if_fail (y >= 0 && y < image->height, 0);

  ptr = image->mem + y * image->bpl + x * image->bpp;

  return *(guint32 *)ptr;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  guchar *ptr;

  ptr = image->mem + y * image->bpl + x * image->bpp;

  *(guint32 *)ptr = pixel;
}

gint
_gdk_windowing_get_bits_for_depth (GdkDisplay *display,
				   gint        depth)
{
  if (depth == 24 || depth == 32)
    return 32;
  else
    g_assert_not_reached ();

  return 0;
}
