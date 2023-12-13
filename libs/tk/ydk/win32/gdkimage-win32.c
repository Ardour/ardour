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
#include "gdkimage.h"
#include "gdkpixmap.h"
#include "gdkscreen.h" /* gdk_screen_get_default() */
#include "gdkprivate-win32.h"

static GList *image_list = NULL;
static gpointer parent_class = NULL;

static void gdk_win32_image_destroy (GdkImage      *image);
static void gdk_image_init          (GdkImage      *image);
static void gdk_image_class_init    (GdkImageClass *klass);
static void gdk_image_finalize      (GObject       *object);

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
        (GInstanceInitFunc) gdk_image_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkImage",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_image_init (GdkImage *image)
{
  image->windowing_data = NULL;
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_image_finalize;
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  gdk_win32_image_destroy (image);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
_gdk_image_exit (void)
{
  GdkImage *image;

  while (image_list)
    {
      image = image_list->data;
      gdk_win32_image_destroy (image);
    }
}

/*
 * Create a GdkImage _without_ an associated GdkPixmap. The caller is
 * responsible for creating a GdkPixmap object and making the association.
 */

static GdkImage *
_gdk_win32_new_image (GdkVisual *visual,
		      gint       width,
		      gint       height,
		      gint       depth,
		      guchar    *bits)
{
  GdkImage *image;

  image = g_object_new (gdk_image_get_type (), NULL);
  image->windowing_data = NULL;
  image->type = GDK_IMAGE_SHARED;
  image->visual = visual;
  image->byte_order = GDK_LSB_FIRST;
  image->width = width;
  image->height = height;
  image->depth = depth;
  image->bits_per_pixel = _gdk_windowing_get_bits_for_depth (gdk_display_get_default (), depth);
  switch (depth)
    {
    case 1:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
      image->bpp = 1;
      break;
    case 15:
    case 16:
      image->bpp = 2;
      break;
    case 24:
      image->bpp = image->bits_per_pixel / 8;
      break;
    case 32:
      image->bpp = 4;
      break;
    default:
      g_warning ("_gdk_win32_new_image: depth=%d", image->depth);
      g_assert_not_reached ();
    }
  if (depth == 1)
    image->bpl = ((width - 1)/32 + 1)*4;
  else if (depth == 4)
    image->bpl = ((width - 1)/8 + 1)*4;
  else
    image->bpl = ((width*image->bpp - 1)/4 + 1)*4;
  image->mem = bits;

  return image;
}

GdkImage *
gdk_image_new_bitmap (GdkVisual *visual,
		      gpointer   data,
		      gint       w,
		      gint       h)
{
  GdkPixmap *pixmap;
  GdkImage *image;
  guchar *bits;
  gint data_bpl = (w-1)/8 + 1;
  gint i;

  pixmap = gdk_pixmap_new (NULL, w, h, 1);

  if (pixmap == NULL)
    return NULL;

  GDK_NOTE (IMAGE, g_print ("gdk_image_new_bitmap: %dx%d=%p\n",
			    w, h, GDK_PIXMAP_HBITMAP (pixmap)));

  bits = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl)->bits;
  image = _gdk_win32_new_image (visual, w, h, 1, bits);
  image->windowing_data = pixmap;
  
  if (data_bpl != image->bpl)
    {
      for (i = 0; i < h; i++)
	memmove ((guchar *) image->mem + i*image->bpl, ((guchar *) data) + i*data_bpl, data_bpl);
    }
  else
    memmove (image->mem, data, data_bpl*h);

  return image;
}

void
_gdk_windowing_image_init (void)
{
  /* Nothing needed AFAIK */
}

GdkImage*
_gdk_image_new_for_depth (GdkScreen    *screen,
			  GdkImageType  type,
			  GdkVisual    *visual,
			  gint          width,
			  gint          height,
			  gint          depth)
{
  GdkPixmap *pixmap;
  GdkImage *image;
  guchar *bits;

  g_return_val_if_fail (!visual || GDK_IS_VISUAL (visual), NULL);
  g_return_val_if_fail (visual || depth != -1, NULL);
  g_return_val_if_fail (screen == gdk_screen_get_default (), NULL);
 
  if (visual)
    depth = visual->depth;

  pixmap = gdk_pixmap_new (NULL, width, height, depth);

  if (pixmap == NULL)
    return NULL;

  GDK_NOTE (IMAGE, g_print ("_gdk_image_new_for_depth: %dx%dx%d=%p\n",
			    width, height, depth, GDK_PIXMAP_HBITMAP (pixmap)));
  
  bits = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl)->bits;
  image = _gdk_win32_new_image (visual, width, height, depth, bits);
  image->windowing_data = pixmap;
  
  return image;
}

GdkImage*
_gdk_win32_copy_to_image (GdkDrawable    *drawable,
			  GdkImage       *image,
			  gint            src_x,
			  gint            src_y,
			  gint            dest_x,
			  gint            dest_y,
			  gint            width,
			  gint            height)
{
  GdkGC *gc;
  GdkScreen *screen = gdk_drawable_get_screen (drawable);
  
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable), NULL);
  g_return_val_if_fail (image != NULL || (dest_x == 0 && dest_y == 0), NULL);

  GDK_NOTE (IMAGE, g_print ("_gdk_win32_copy_to_image: %p\n",
			    GDK_DRAWABLE_HANDLE (drawable)));

  if (!image)
    image = _gdk_image_new_for_depth (screen, GDK_IMAGE_FASTEST, NULL, width, height,
				      gdk_drawable_get_depth (drawable));

  gc = gdk_gc_new ((GdkDrawable *) image->windowing_data);
  _gdk_win32_blit
    (FALSE,
     GDK_DRAWABLE_IMPL_WIN32 (GDK_PIXMAP_OBJECT (image->windowing_data)->impl),
     gc, drawable, src_x, src_y, dest_x, dest_y, width, height);
  g_object_unref (gc);

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint      x,
		     gint      y)
{
  guchar *pixelp;

  g_return_val_if_fail (image != NULL, 0);
  g_return_val_if_fail (x >= 0 && x < image->width, 0);
  g_return_val_if_fail (y >= 0 && y < image->height, 0);

  if (!(x >= 0 && x < image->width && y >= 0 && y < image->height))
      return 0;

  if (image->depth == 1)
    return (((guchar *) image->mem)[y * image->bpl + (x >> 3)] & (1 << (7 - (x & 0x7)))) != 0;

  if (image->depth == 4)
    {
      pixelp = (guchar *) image->mem + y * image->bpl + (x >> 1);
      if (x&1)
	return (*pixelp) & 0x0F;

      return (*pixelp) >> 4;
    }
    
  pixelp = (guchar *) image->mem + y * image->bpl + x * image->bpp;
      
  switch (image->bpp)
    {
    case 1:
      return *pixelp;
      
      /* Windows is always LSB, no need to check image->byte_order. */
    case 2:
      return pixelp[0] | (pixelp[1] << 8);
      
    case 3:
      return pixelp[0] | (pixelp[1] << 8) | (pixelp[2] << 16);

    case 4:
      return pixelp[0] | (pixelp[1] << 8) | (pixelp[2] << 16);
    }
  g_assert_not_reached ();
  return 0;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint       x,
		     gint       y,
		     guint32    pixel)
{
  guchar *pixelp;

  g_return_if_fail (image != NULL);
  g_return_if_fail (x >= 0 && x < image->width);
  g_return_if_fail (y >= 0 && y < image->height);

  if  (!(x >= 0 && x < image->width && y >= 0 && y < image->height))
    return;

  GdiFlush ();
  if (image->depth == 1)
    if (pixel & 1)
      ((guchar *) image->mem)[y * image->bpl + (x >> 3)] |= (1 << (7 - (x & 0x7)));
    else
      ((guchar *) image->mem)[y * image->bpl + (x >> 3)] &= ~(1 << (7 - (x & 0x7)));
  else if (image->depth == 4)
    {
      pixelp = (guchar *) image->mem + y * image->bpl + (x >> 1);

      if (x&1)
	{
	  *pixelp &= 0xF0;
	  *pixelp |= (pixel & 0x0F);
	}
      else
	{
	  *pixelp &= 0x0F;
	  *pixelp |= (pixel << 4);
	}
    }
  else
    {
      pixelp = (guchar *) image->mem + y * image->bpl + x * image->bpp;
      
      /* Windows is always LSB, no need to check image->byte_order. */
      switch (image->bpp)
	{
	case 4:
	  pixelp[3] = 0;
	case 3:
	  pixelp[2] = ((pixel >> 16) & 0xFF);
	case 2:
	  pixelp[1] = ((pixel >> 8) & 0xFF);
	case 1:
	  pixelp[0] = (pixel & 0xFF);
	}
    }
}

static void
gdk_win32_image_destroy (GdkImage *image)
{
  GdkPixmap *pixmap;

  g_return_if_fail (GDK_IS_IMAGE (image));

  pixmap = image->windowing_data;

  if (pixmap == NULL)		/* This means that _gdk_image_exit()
				 * destroyed the image already, and
				 * now we're called a second time from
				 * _finalize()
				 */
    return;
  
  GDK_NOTE (IMAGE, g_print ("gdk_win32_image_destroy: %p\n",
			    GDK_PIXMAP_HBITMAP (pixmap)));

  g_object_unref (pixmap);
  image->windowing_data = NULL;
}

gint
_gdk_windowing_get_bits_for_depth (GdkDisplay *display,
                                   gint        depth)
{
  g_return_val_if_fail (display == gdk_display_get_default (), 0);

  switch (depth)
    {
    case 1:
      return 1;

    case 2:
    case 3:
    case 4:
      return 4;

    case 5:
    case 6:
    case 7:
    case 8:
      return 8;

    case 15:
    case 16:
      return 16;

    case 24:
    case 32:
      return 32;
    }
  g_assert_not_reached ();
  return 0;
}
