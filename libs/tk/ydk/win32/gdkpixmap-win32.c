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
#include <stdio.h>
#include <string.h>

#include "gdkpixmap.h"
#include "gdkdisplay.h"
#include "gdkscreen.h"

#include "gdkprivate-win32.h"
#include <cairo-win32.h>

static void gdk_pixmap_impl_win32_get_size   (GdkDrawable        *drawable,
					      gint               *width,
					      gint               *height);

static void gdk_pixmap_impl_win32_init       (GdkPixmapImplWin32      *pixmap);
static void gdk_pixmap_impl_win32_class_init (GdkPixmapImplWin32Class *klass);
static void gdk_pixmap_impl_win32_finalize   (GObject                 *object);

static gpointer parent_class = NULL;

GType
_gdk_pixmap_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkPixmapImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_impl_win32_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_WIN32,
                                            "GdkPixmapImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

GType
_gdk_pixmap_impl_get_type (void)
{
  return _gdk_pixmap_impl_win32_get_type ();
}

static void
gdk_pixmap_impl_win32_init (GdkPixmapImplWin32 *impl)
{
  impl->width = 1;
  impl->height = 1;
}

static void
gdk_pixmap_impl_win32_class_init (GdkPixmapImplWin32Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_win32_finalize;

  drawable_class->get_size = gdk_pixmap_impl_win32_get_size;
}

static void
gdk_pixmap_impl_win32_finalize (GObject *object)
{
  GdkPixmapImplWin32 *impl = GDK_PIXMAP_IMPL_WIN32 (object);
  GdkDrawableImplWin32 *drawable_impl = GDK_DRAWABLE_IMPL_WIN32 (impl);
  GdkPixmap *wrapper = GDK_PIXMAP (drawable_impl->wrapper);

  GDK_NOTE (PIXMAP, g_print ("gdk_pixmap_impl_win32_finalize: %p\n",
			     GDK_PIXMAP_HBITMAP (wrapper)));

  if (!impl->is_foreign)
    {
      /* Only decrement count if we did set the hdc */
      if (drawable_impl->hdc)
	drawable_impl->hdc_count--;

      if (drawable_impl->cairo_surface)
	{
	  /* Tell outstanding owners that the surface is useless */
	  cairo_surface_finish (drawable_impl->cairo_surface);

	  /* Drop our reference */
	  cairo_surface_destroy (drawable_impl->cairo_surface);
	  drawable_impl->cairo_surface = NULL;
	  if (impl->is_allocated)
	    {
	      GDI_CALL (DeleteDC, (drawable_impl->hdc));
	      GDI_CALL (DeleteObject, (GDK_PIXMAP_HBITMAP (wrapper)));
	    }
	}
    }

  _gdk_win32_drawable_finish (GDK_DRAWABLE (object));

  gdk_win32_handle_table_remove (GDK_PIXMAP_HBITMAP (wrapper));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_win32_get_size (GdkDrawable *drawable,
				gint        *width,
				gint        *height)
{
  if (width)
    *width = GDK_PIXMAP_IMPL_WIN32 (drawable)->width;
  if (height)
    *height = GDK_PIXMAP_IMPL_WIN32 (drawable)->height;
}

GdkPixmap*
_gdk_pixmap_new (GdkDrawable *drawable,
		gint         width,
		gint         height,
		gint         depth)
{
  HDC hdc;
  HBITMAP hbitmap;
  GdkPixmap *pixmap;
  GdkDrawableImplWin32 *drawable_impl;
  GdkPixmapImplWin32 *pixmap_impl;
  GdkColormap *cmap;
  gint window_depth;
  cairo_surface_t *dib_surface, *image_surface;
  cairo_format_t format;
  guchar *bits;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail ((drawable != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!drawable)
    drawable = _gdk_root;

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  window_depth = gdk_drawable_get_depth (GDK_DRAWABLE (drawable));
  if (depth == -1)
    depth = window_depth;

  GDK_NOTE (PIXMAP, g_print ("gdk_pixmap_new: %dx%dx%d drawable=%p\n",
			     width, height, depth, drawable));

  switch (depth)
    {
    case 1:
      format = CAIRO_FORMAT_A1;
      break;

    case 8:
      format = CAIRO_FORMAT_A8;
      break;

    case 15:
    case 16:
      format = CAIRO_FORMAT_RGB16_565;
      break;

    case 24:
    case 32:
      format = CAIRO_FORMAT_RGB24;
      break;

    default:
      g_warning ("gdk_win32_pixmap_new: depth = %d not supported", depth);
      return NULL;
      break;
    }

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  drawable_impl = GDK_DRAWABLE_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pixmap_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  drawable_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  pixmap_impl->is_foreign = FALSE;
  pixmap_impl->width = width;
  pixmap_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  if (depth == window_depth)
    {
      cmap = gdk_drawable_get_colormap (drawable);
      if (cmap)
        gdk_drawable_set_colormap (pixmap, cmap);
    }

  if (depth != 15 && depth != 16)
    {
      dib_surface = cairo_win32_surface_create_with_dib (format, width, height);
      if (dib_surface == NULL ||
	  cairo_surface_status (dib_surface) != CAIRO_STATUS_SUCCESS)
	{
	  g_object_unref ((GObject *) pixmap);
	  return NULL;
	}

      /* We need to have cairo create the dibsection for us, because
	 creating a cairo surface from a hdc only works for rgb24 format */
      hdc = cairo_win32_surface_get_dc (dib_surface);

      /* Get the bitmap from the cairo hdc */
      hbitmap = GetCurrentObject (hdc, OBJ_BITMAP);

      /* Cairo_win32_surface_get_image() returns NULL on failure, but
	 this is likely an oversight and future versions will return a
	 "nil" surface.
       */
      image_surface = cairo_win32_surface_get_image (dib_surface);
      if (image_surface == NULL ||
	  cairo_surface_status (image_surface) != CAIRO_STATUS_SUCCESS)
      {
	cairo_surface_destroy (dib_surface);
	g_object_unref ((GObject*) pixmap);
	return NULL;
      }
      bits = cairo_image_surface_get_data (image_surface);
    }
  else
    {
      /* 16 bpp not supported by win32 cairo surface */
      struct {
	BITMAPINFOHEADER bmiHeader;
	union {
	  WORD bmiIndices[256];
	  DWORD bmiMasks[3];
	  RGBQUAD bmiColors[256];
	} u;
      } bmi;
      UINT iUsage;
      HWND hwnd;
      GdkVisual *visual;

      if (GDK_IS_WINDOW (drawable))
	hwnd = GDK_WINDOW_HWND (drawable);
      else
	hwnd = GetDesktopWindow ();
      if ((hdc = GetDC (hwnd)) == NULL)
	{
	  WIN32_GDI_FAILED ("GetDC");
	  g_object_unref ((GObject *) pixmap);
	  return NULL;
	}

      bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = width;
      bmi.bmiHeader.biHeight = -height;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 16;
      bmi.bmiHeader.biCompression = BI_BITFIELDS;
      bmi.bmiHeader.biSizeImage = 0;
      bmi.bmiHeader.biXPelsPerMeter =
	bmi.bmiHeader.biYPelsPerMeter = 0;
      bmi.bmiHeader.biClrUsed = 0;
      bmi.bmiHeader.biClrImportant = 0;

      iUsage = DIB_RGB_COLORS;
      visual = gdk_visual_get_system ();
      bmi.u.bmiMasks[0] = visual->red_mask;
      bmi.u.bmiMasks[1] = visual->green_mask;
      bmi.u.bmiMasks[2] = visual->blue_mask;

      if ((hbitmap = CreateDIBSection (hdc, (BITMAPINFO *) &bmi,
				       iUsage, (PVOID *) &bits, NULL, 0)) == NULL)
	{
	  WIN32_GDI_FAILED ("CreateDIBSection");
	  GDI_CALL (ReleaseDC, (hwnd, hdc));
	  g_object_unref ((GObject *) pixmap);
	  return NULL;
	}
      GDI_CALL (ReleaseDC, (hwnd, hdc));

      dib_surface = cairo_image_surface_create_for_data (bits,
							 format, width, height,
							 (width * 2 + 3) & ~3);

      hdc = CreateCompatibleDC (NULL);
      if (!hdc)
	{
	  WIN32_GDI_FAILED ("CreateCompatibleDC");
	  g_object_unref ((GObject *) pixmap);
	  return NULL;
	}

      SelectObject (hdc, hbitmap);
      pixmap_impl->is_allocated = TRUE;
    }

  /* We need to use the same hdc, because only one hdc
     can render to the same bitmap */
  drawable_impl->hdc = hdc;
  drawable_impl->hdc_count = 1; /* Ensure we never free the cairo surface HDC */

  /* No need to create a new surface when needed, as we have one already */
  drawable_impl->cairo_surface = dib_surface;
  drawable_impl->handle = hbitmap;
  pixmap_impl->bits = bits;

  gdk_win32_handle_table_insert (&GDK_PIXMAP_HBITMAP (pixmap), pixmap);

  return pixmap;
}

static const unsigned char mirror[256] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
  0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
  0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
  0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
  0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
  0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
  0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
  0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
  0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
  0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
  0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
  0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
  0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
  0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
  0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
  0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
  0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
  0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
  0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
  0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
  0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
  0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
  0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
  0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
  0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
  0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
  0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
  0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
  0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
  0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
  0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
  0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

GdkPixmap *
_gdk_bitmap_create_from_data (GdkDrawable *drawable,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
  GdkPixmap *pixmap;
  GdkPixmapImplWin32 *pixmap_impl;
  gint i, j, data_bpl, pixmap_bpl;
  guchar *bits;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);

  if (!drawable)
    drawable = _gdk_root;
  else if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  pixmap = gdk_pixmap_new (drawable, width, height, 1);

  if (pixmap == NULL)
    return NULL;

  pixmap_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  bits = pixmap_impl->bits;
  data_bpl = ((width - 1) / 8 + 1);
  pixmap_bpl = ((width - 1)/32 + 1)*4;

  for (i = 0; i < height; i++)
    for (j = 0; j < data_bpl; j++)
      bits[i*pixmap_bpl + j] = mirror[(guchar) data[i*data_bpl + j]];

  GDK_NOTE (PIXMAP, g_print ("gdk_bitmap_create_from_data: %dx%d=%p\n",
			     width, height, GDK_PIXMAP_HBITMAP (pixmap)));

  return pixmap;
}

GdkPixmap*
_gdk_pixmap_create_from_data (GdkDrawable    *drawable,
			     const gchar    *data,
			     gint            width,
			     gint            height,
			     gint            depth,
			     const GdkColor *fg,
			     const GdkColor *bg)
{
  /* Oh wow. I struggled with dozens of lines of code trying to get
   * this right using a monochrome Win32 bitmap created from data, and
   * a colour DIB section as the result, trying setting pens,
   * background colors, whatnot and BitBlt:ing.  Nope. Then finally I
   * realized it's much easier to do it using gdk...:
   */

  GdkPixmap *result;
  GdkPixmap *source;
  GdkGC *gc;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);
  g_return_val_if_fail ((drawable != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  result = gdk_pixmap_new (drawable, width, height, depth);
  source = gdk_bitmap_create_from_data (drawable, data, width, height);
  gc = gdk_gc_new (result);

  gdk_gc_set_foreground (gc, fg);
  gdk_gc_set_background (gc, bg);
  _gdk_win32_blit
    (TRUE,
     GDK_DRAWABLE_IMPL_WIN32 (GDK_PIXMAP_OBJECT (result)->impl),
     gc, source, 0, 0, 0, 0, width, height);
  g_object_unref (source);
  g_object_unref (gc);

  GDK_NOTE (PIXMAP, g_print ("gdk_pixmap_create_from_data: %dx%dx%d=%p\n",
			     width, height, depth,
			     GDK_PIXMAP_HBITMAP (result)));

  return result;
}

GdkPixmap *
gdk_pixmap_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (display == _gdk_display, NULL);

  return gdk_pixmap_foreign_new (anid);
}

GdkPixmap *
gdk_pixmap_foreign_new_for_screen (GdkScreen       *screen,
				   GdkNativeWindow  anid,
				   gint             width,
				   gint             height,
				   gint             depth)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return gdk_pixmap_foreign_new (anid);
}

GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
  GdkPixmap *pixmap;
  GdkDrawableImplWin32 *draw_impl;
  GdkPixmapImplWin32 *pix_impl;
  HBITMAP hbitmap;
  SIZE size;

  /* Check to make sure we were passed a HBITMAP */
  g_return_val_if_fail (GetObjectType ((HGDIOBJ) anid) == OBJ_BITMAP, NULL);

  hbitmap = (HBITMAP) anid;

  /* Get information about the bitmap to fill in the structure for the
   * GDK window.
   */
  GetBitmapDimensionEx (hbitmap, &size);

  /* Allocate a new GDK pixmap */
  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  draw_impl->handle = hbitmap;
  draw_impl->colormap = NULL;
  pix_impl->is_foreign = TRUE;
  pix_impl->width = size.cx;
  pix_impl->height = size.cy;
  pix_impl->bits = NULL;

  gdk_win32_handle_table_insert (&GDK_PIXMAP_HBITMAP (pixmap), pixmap);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  return (GdkPixmap*) gdk_win32_handle_table_lookup (anid);
}

GdkPixmap*
gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (display == _gdk_display, NULL);

  return gdk_pixmap_lookup (anid);
}
