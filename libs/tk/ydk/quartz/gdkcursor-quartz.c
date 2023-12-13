/* gdkcursor-quartz.c
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#include "gdkdisplay.h"
#include "gdkcursor.h"
#include "gdkprivate-quartz.h"

#include "xcursors.h"

static GdkCursor *cached_xcursors[G_N_ELEMENTS (xcursors)];

static GdkCursor *
gdk_quartz_cursor_new_from_nscursor (NSCursor      *nscursor,
                                     GdkCursorType  cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  private = g_new (GdkCursorPrivate, 1);
  private->nscursor = nscursor;

  cursor = (GdkCursor *)private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;

  return cursor;
}

static GdkCursor *
create_blank_cursor (void)
{
  NSCursor *nscursor;
  NSImage *nsimage;
  NSSize size = { 1.0, 1.0 };

  nsimage = [[NSImage alloc] initWithSize:size];
  nscursor = [[NSCursor alloc] initWithImage:nsimage
                               hotSpot:NSMakePoint(0.0, 0.0)];
  [nsimage release];

  return gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_BLANK_CURSOR);
}

static gboolean
get_bit (const guchar *data,
         gint          width,
         gint          height,
         gint          x,
         gint          y)
{
  gint bytes_per_line;
  const guchar *src;

  if (x < 0 || y < 0 || x >= width || y >= height)
    return FALSE;

  bytes_per_line = (width + 7) / 8;

  src = &data[y * bytes_per_line];
  return ((src[x / 8] >> x % 8) & 1);
}

static GdkCursor *
create_builtin_cursor (GdkCursorType cursor_type)
{
  GdkCursor *cursor;
  NSBitmapImageRep *bitmap_rep;
  NSInteger mask_width, mask_height;
  gint src_width, src_height;
  gint dst_stride;
  const guchar *mask_start, *src_start;
  gint dx, dy;
  gint x, y;
  NSPoint hotspot;
  NSImage *image;
  NSCursor *nscursor;

  if (cursor_type >= G_N_ELEMENTS (xcursors) || cursor_type < 0)
    return NULL;

  cursor = cached_xcursors[cursor_type];
  if (cursor)
    return cursor;

  GDK_QUARTZ_ALLOC_POOL;

  src_width = xcursors[cursor_type].width;
  src_height = xcursors[cursor_type].height;
  mask_width = xcursors[cursor_type+1].width;
  mask_height = xcursors[cursor_type+1].height;

  bitmap_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
		pixelsWide:mask_width pixelsHigh:mask_height
		bitsPerSample:8 samplesPerPixel:4
		hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:0 bitsPerPixel:0];

  dst_stride = [bitmap_rep bytesPerRow];

  src_start = xcursors[cursor_type].bits;
  mask_start = xcursors[cursor_type+1].bits;

  dx = xcursors[cursor_type+1].hotx - xcursors[cursor_type].hotx;
  dy = xcursors[cursor_type+1].hoty - xcursors[cursor_type].hoty;

  for (y = 0; y < mask_height; y++)
    {
      guchar *dst = [bitmap_rep bitmapData] + y * dst_stride;

      for (x = 0; x < mask_width; x++)
	{
	  if (get_bit (mask_start, mask_width, mask_height, x, y))
            {
              if (get_bit (src_start, src_width, src_height, x - dx, y - dy))
                {
                  *dst++ = 0;
                  *dst++ = 0;
                  *dst++ = 0;
                }
              else
                {
                  *dst++ = 0xff;
                  *dst++ = 0xff;
                  *dst++ = 0xff;
                }

              *dst++ = 0xff;
            }
	  else
            {
              *dst++ = 0;
              *dst++ = 0;
              *dst++ = 0;
              *dst++ = 0;
            }
        }
    }

  image = [[NSImage alloc] init];
  [image addRepresentation:bitmap_rep];
  [bitmap_rep release];

  hotspot = NSMakePoint (xcursors[cursor_type+1].hotx,
                         xcursors[cursor_type+1].hoty);

  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:hotspot];
  [image release];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);

  cached_xcursors[cursor_type] = gdk_cursor_ref (cursor);

  GDK_QUARTZ_RELEASE_POOL;

  return cursor;
}

GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
			    GdkCursorType  cursor_type)
{
  NSCursor *nscursor;

  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  switch (cursor_type) 
    {
    case GDK_XTERM:
      nscursor = [NSCursor IBeamCursor];
      break;
    case GDK_SB_H_DOUBLE_ARROW:
      nscursor = [NSCursor resizeLeftRightCursor];
      break;
    case GDK_SB_V_DOUBLE_ARROW:
      nscursor = [NSCursor resizeUpDownCursor];
      break;
    case GDK_SB_UP_ARROW:
    case GDK_BASED_ARROW_UP:
    case GDK_BOTTOM_TEE:
    case GDK_TOP_SIDE:
      nscursor = [NSCursor resizeUpCursor];
      break;
    case GDK_SB_DOWN_ARROW:
    case GDK_BASED_ARROW_DOWN:
    case GDK_TOP_TEE:
    case GDK_BOTTOM_SIDE:
      nscursor = [NSCursor resizeDownCursor];
      break;
    case GDK_SB_LEFT_ARROW:
    case GDK_RIGHT_TEE:
    case GDK_LEFT_SIDE:
      nscursor = [NSCursor resizeLeftCursor];
      break;
    case GDK_SB_RIGHT_ARROW:
    case GDK_LEFT_TEE:
    case GDK_RIGHT_SIDE:
      nscursor = [NSCursor resizeRightCursor];
      break;
    case GDK_TCROSS:
    case GDK_CROSS:
    case GDK_CROSSHAIR:
    case GDK_DIAMOND_CROSS:
      nscursor = [NSCursor crosshairCursor];
      break;
    case GDK_HAND1:
    case GDK_HAND2:
      nscursor = [NSCursor pointingHandCursor];
      break;
    case GDK_CURSOR_IS_PIXMAP:
      return NULL;
    case GDK_BLANK_CURSOR:
      return create_blank_cursor ();
    default:
      return gdk_cursor_ref (create_builtin_cursor (cursor_type));
    }

  [nscursor retain];
  return gdk_quartz_cursor_new_from_nscursor (nscursor, cursor_type);
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap      *source,
			    GdkPixmap      *mask,
			    const GdkColor *fg,
			    const GdkColor *bg,
			    gint            x,
			    gint            y)
{
  NSBitmapImageRep *bitmap_rep;
  NSImage *image;
  NSCursor *nscursor;
  GdkCursor *cursor;
  int width, height;
  gint tmp_x, tmp_y;
  guchar *dst_data, *mask_data, *src_data;
  guchar *mask_start, *src_start;
  int dst_stride;

  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  GDK_QUARTZ_ALLOC_POOL;

  gdk_drawable_get_size (source, &width, &height);

  bitmap_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
		pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height
		bitsPerSample:8 samplesPerPixel:4
		hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:0 bitsPerPixel:0];

  dst_stride = [bitmap_rep bytesPerRow];
  mask_start = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (mask)->impl)->data;
  src_start = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (source)->impl)->data;

  for (tmp_y = 0; tmp_y < height; tmp_y++)
    {
      dst_data = [bitmap_rep bitmapData] + tmp_y * dst_stride;
      mask_data = mask_start + tmp_y * width;
      src_data = src_start + tmp_y * width;

      for (tmp_x = 0; tmp_x < width; tmp_x++)
	{
	  if (*mask_data++)
	    {
	      const GdkColor *color;

	      if (*src_data++)
		color = fg;
	      else
		color = bg;

	      *dst_data++ = (color->red >> 8) & 0xff;
	      *dst_data++ = (color->green >> 8) & 0xff;
	      *dst_data++ = (color->blue >> 8) & 0xff;
	      *dst_data++ = 0xff;

	    }
	  else
	    {
	      *dst_data++ = 0x00;
	      *dst_data++ = 0x00;
	      *dst_data++ = 0x00;
	      *dst_data++ = 0x00;

	      src_data++;
	    }
	}
    }
  image = [[NSImage alloc] init];
  [image addRepresentation:bitmap_rep];
  [bitmap_rep release];

  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(x, y)];
  [image release];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);

  GDK_QUARTZ_RELEASE_POOL;

  return cursor;
}

static NSImage *
_gdk_quartz_pixbuf_to_ns_image (GdkPixbuf *pixbuf)
{
  NSBitmapImageRep  *bitmap_rep;
  NSImage           *image;
  gboolean           has_alpha;
  
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  
  /* Create a bitmap image rep */
  bitmap_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL 
                                         pixelsWide:gdk_pixbuf_get_width (pixbuf)
					 pixelsHigh:gdk_pixbuf_get_height (pixbuf)
					 bitsPerSample:8 samplesPerPixel:has_alpha ? 4 : 3
					 hasAlpha:has_alpha isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
					 bytesPerRow:0 bitsPerPixel:0];
	
  {
    /* Add pixel data to bitmap rep */
    guchar *src, *dst;
    int src_stride, dst_stride;
    int x, y;
		
    src_stride = gdk_pixbuf_get_rowstride (pixbuf);
    dst_stride = [bitmap_rep bytesPerRow];
		
    for (y = 0; y < gdk_pixbuf_get_height (pixbuf); y++) 
      {
	src = gdk_pixbuf_get_pixels (pixbuf) + y * src_stride;
	dst = [bitmap_rep bitmapData] + y * dst_stride;
	
	for (x = 0; x < gdk_pixbuf_get_width (pixbuf); x++)
	  {
	    if (has_alpha)
	      {
		guchar red, green, blue, alpha;
		
		red = *src++;
		green = *src++;
		blue = *src++;
		alpha = *src++;
		
		*dst++ = (red * alpha) / 255;
		*dst++ = (green * alpha) / 255;
		*dst++ = (blue * alpha) / 255;
		*dst++ = alpha;
	      }
	    else
	     {
	       *dst++ = *src++;
	       *dst++ = *src++;
	       *dst++ = *src++;
	     }
	  }
      }	
  }
	
  image = [[NSImage alloc] init];
  [image addRepresentation:bitmap_rep];
  [bitmap_rep release];
  [image autorelease];
	
  return image;
}

GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display, 
			    GdkPixbuf  *pixbuf,
			    gint        x,
			    gint        y)
{
  NSImage *image;
  NSCursor *nscursor;
  GdkCursor *cursor;
  gboolean has_alpha;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  GDK_QUARTZ_ALLOC_POOL;

  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  image = _gdk_quartz_pixbuf_to_ns_image (pixbuf);
  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(x, y)];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);

  GDK_QUARTZ_RELEASE_POOL;

  return cursor;
}

GdkCursor*  
gdk_cursor_new_from_name (GdkDisplay  *display,  
			  const gchar *name)
{
  /* FIXME: Implement */
  return NULL;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivate *)cursor;
  [private->nscursor release];
  
  g_free (private);
}

gboolean 
gdk_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

gboolean 
gdk_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

guint     
gdk_display_get_default_cursor_size (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  /* Mac OS X doesn't have the notion of a default size */
  return 32;
}

void     
gdk_display_get_maximal_cursor_size (GdkDisplay *display,
				     guint       *width,
				     guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  /* Cursor sizes in Mac OS X can be arbitrarily large */
  *width = 65536;
  *height = 65536;
}

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return gdk_display_get_default ();
}

GdkPixbuf *
gdk_cursor_get_image (GdkCursor *cursor)
{
  /* FIXME: Implement */
  return NULL;
}

NSImage *
gdk_quartz_pixbuf_to_ns_image_libgtk_only (GdkPixbuf *pixbuf)
{
  return _gdk_quartz_pixbuf_to_ns_image (pixbuf);
}
