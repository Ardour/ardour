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

#define GDK_PIXBUF_ENABLE_BACKEND

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#include <string.h>
#include <errno.h>

#include "gdkprivate-x11.h"
#include "gdkcursor.h"
#include "gdkdisplay-x11.h"
#include "gdkpixmap-x11.h"
#include "gdkx.h"
#include <gdk/gdkpixmap.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gdkalias.h"

static guint theme_serial = 0;

/* cursor_cache holds a cache of non-pixmap cursors to avoid expensive 
 * libXcursor searches, cursors are added to it but only removed when
 * their display is closed. We make the assumption that since there are 
 * a small number of display's and a small number of cursor's that this 
 * list will stay small enough not to be a problem.
 */
static GSList* cursor_cache = NULL;

struct cursor_cache_key
{
  GdkDisplay* display;
  GdkCursorType type;
  const char* name;
};

/* Caller should check if there is already a match first.
 * Cursor MUST be either a typed cursor or a pixmap with 
 * a non-NULL name.
 */
static void
add_to_cache (GdkCursorPrivate* cursor)
{
  cursor_cache = g_slist_prepend (cursor_cache, cursor);

  /* Take a ref so that if the caller frees it we still have it */
  gdk_cursor_ref ((GdkCursor*) cursor);
}

/* Returns 0 on a match
 */
static gint
cache_compare_func (gconstpointer listelem, 
                    gconstpointer target)
{
  GdkCursorPrivate* cursor = (GdkCursorPrivate*)listelem;
  struct cursor_cache_key* key = (struct cursor_cache_key*)target;

  if ((cursor->cursor.type != key->type) ||
      (cursor->display != key->display))
    return 1; /* No match */
  
  /* Elements marked as pixmap must be named cursors 
   * (since we don't store normal pixmap cursors 
   */
  if (key->type == GDK_CURSOR_IS_PIXMAP)
    return strcmp (key->name, cursor->name);

  return 0; /* Match */
}

/* Returns the cursor if there is a match, NULL if not
 * For named cursors type shall be GDK_CURSOR_IS_PIXMAP
 * For unnamed, typed cursors, name shall be NULL
 */
static GdkCursorPrivate*
find_in_cache (GdkDisplay    *display, 
               GdkCursorType  type,
               const char    *name)
{
  GSList* res;
  struct cursor_cache_key key;

  key.display = display;
  key.type = type;
  key.name = name;

  res = g_slist_find_custom (cursor_cache, &key, cache_compare_func);

  if (res)
    return (GdkCursorPrivate *) res->data;

  return NULL;
}

/* Called by gdk_display_x11_finalize to flush any cached cursors
 * for a dead display.
 */
void 
_gdk_x11_cursor_display_finalize (GdkDisplay *display)
{
  GSList* item;
  GSList** itemp; /* Pointer to the thing to fix when we delete an item */
  item = cursor_cache;
  itemp = &cursor_cache;
  while (item)
    {
      GdkCursorPrivate* cursor = (GdkCursorPrivate*)(item->data);
      if (cursor->display == display)
        {
	  GSList* olditem;
          gdk_cursor_unref ((GdkCursor*) cursor);
	  /* Remove this item from the list */
	  *(itemp) = item->next;
	  olditem = item;
	  item = g_slist_next (item);
	  g_slist_free_1 (olditem);
        } 
      else 
        {
	  itemp = &(item->next);
	  item = g_slist_next (item);
	}
    }
}

static Cursor
get_blank_cursor (GdkDisplay *display)
{
  GdkScreen *screen;
  GdkPixmap *pixmap;
  Pixmap source_pixmap;
  XColor color;
  Cursor cursor;

  screen = gdk_display_get_default_screen (display);
  pixmap = gdk_bitmap_create_from_data (gdk_screen_get_root_window (screen), 
					"\0\0\0\0\0\0\0\0", 1, 1);
 
  source_pixmap = GDK_PIXMAP_XID (pixmap);

  color.pixel = 0; 
  color.red = color.blue = color.green = 0;
  
  if (display->closed)
    cursor = None;
  else
    cursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
                                  source_pixmap, source_pixmap,
                                  &color, &color, 1, 1);
  g_object_unref (pixmap);

  return cursor;
}

/**
 * gdk_cursor_new_for_display:
 * @display: the #GdkDisplay for which the cursor will be created
 * @cursor_type: cursor to create
 * 
 * Creates a new cursor from the set of builtin cursors.
 * Some useful ones are:
 * <itemizedlist>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="right_ptr.png"></inlinegraphic> #GDK_RIGHT_PTR (right-facing arrow)
 * </para></listitem>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="crosshair.png"></inlinegraphic> #GDK_CROSSHAIR (crosshair)
 * </para></listitem>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="xterm.png"></inlinegraphic> #GDK_XTERM (I-beam)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="watch.png"></inlinegraphic> #GDK_WATCH (busy)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="fleur.png"></inlinegraphic> #GDK_FLEUR (for moving objects)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="hand1.png"></inlinegraphic> #GDK_HAND1 (a right-pointing hand)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="hand2.png"></inlinegraphic> #GDK_HAND2 (a left-pointing hand)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="left_side.png"></inlinegraphic> #GDK_LEFT_SIDE (resize left side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="right_side.png"></inlinegraphic> #GDK_RIGHT_SIDE (resize right side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_left_corner.png"></inlinegraphic> #GDK_TOP_LEFT_CORNER (resize northwest corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_right_corner.png"></inlinegraphic> #GDK_TOP_RIGHT_CORNER (resize northeast corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_left_corner.png"></inlinegraphic> #GDK_BOTTOM_LEFT_CORNER (resize southwest corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_right_corner.png"></inlinegraphic> #GDK_BOTTOM_RIGHT_CORNER (resize southeast corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_side.png"></inlinegraphic> #GDK_TOP_SIDE (resize top side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_side.png"></inlinegraphic> #GDK_BOTTOM_SIDE (resize bottom side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="sb_h_double_arrow.png"></inlinegraphic> #GDK_SB_H_DOUBLE_ARROW (move vertical splitter)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="sb_v_double_arrow.png"></inlinegraphic> #GDK_SB_V_DOUBLE_ARROW (move horizontal splitter)
 * </para></listitem>
 * <listitem><para>
 * #GDK_BLANK_CURSOR (Blank cursor). Since 2.16
 * </para></listitem>
 * </itemizedlist>
 *
 * Return value: a new #GdkCursor
 *
 * Since: 2.2
 **/
GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
			    GdkCursorType  cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Cursor xcursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->closed)
    {
      xcursor = None;
    } 
  else 
    {
      private = find_in_cache (display, cursor_type, NULL);

      if (private)
        {
          /* Cache had it, add a ref for this user */
          gdk_cursor_ref ((GdkCursor*) private);
       
          return (GdkCursor*) private;
        } 
      else 
        {
	  if (cursor_type != GDK_BLANK_CURSOR)
            xcursor = XCreateFontCursor (GDK_DISPLAY_XDISPLAY (display),
                                         cursor_type);
	  else
	    xcursor = get_blank_cursor (display);
       }
    }
  
  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;
  
  if (xcursor != None)
    add_to_cache (private);

  return cursor;
}

/**
 * gdk_cursor_new_from_pixmap:
 * @source: the pixmap specifying the cursor.
 * @mask: the pixmap specifying the mask, which must be the same size as 
 *    @source.
 * @fg: the foreground color, used for the bits in the source which are 1.
 *    The color does not have to be allocated first. 
 * @bg: the background color, used for the bits in the source which are 0.
 *    The color does not have to be allocated first.
 * @x: the horizontal offset of the 'hotspot' of the cursor. 
 * @y: the vertical offset of the 'hotspot' of the cursor.
 * 
 * Creates a new cursor from a given pixmap and mask. Both the pixmap and mask
 * must have a depth of 1 (i.e. each pixel has only 2 values - on or off).
 * The standard cursor size is 16 by 16 pixels. You can create a bitmap 
 * from inline data as in the below example.
 * 
 * <example><title>Creating a custom cursor</title>
 * <programlisting>
 * /<!-- -->* This data is in X bitmap format, and can be created with the 'bitmap'
 *    utility. *<!-- -->/
 * &num;define cursor1_width 16
 * &num;define cursor1_height 16
 * static unsigned char cursor1_bits[] = {
 *   0x80, 0x01, 0x40, 0x02, 0x20, 0x04, 0x10, 0x08, 0x08, 0x10, 0x04, 0x20,
 *   0x82, 0x41, 0x41, 0x82, 0x41, 0x82, 0x82, 0x41, 0x04, 0x20, 0x08, 0x10,
 *   0x10, 0x08, 0x20, 0x04, 0x40, 0x02, 0x80, 0x01};
 *  
 * static unsigned char cursor1mask_bits[] = {
 *   0x80, 0x01, 0xc0, 0x03, 0x60, 0x06, 0x30, 0x0c, 0x18, 0x18, 0x8c, 0x31,
 *   0xc6, 0x63, 0x63, 0xc6, 0x63, 0xc6, 0xc6, 0x63, 0x8c, 0x31, 0x18, 0x18,
 *   0x30, 0x0c, 0x60, 0x06, 0xc0, 0x03, 0x80, 0x01};
 *  
 *  
 *  GdkCursor *cursor;
 *  GdkPixmap *source, *mask;
 *  GdkColor fg = { 0, 65535, 0, 0 }; /<!-- -->* Red. *<!-- -->/
 *  GdkColor bg = { 0, 0, 0, 65535 }; /<!-- -->* Blue. *<!-- -->/
 *  
 *  
 *  source = gdk_bitmap_create_from_data (NULL, cursor1_bits,
 *                                        cursor1_width, cursor1_height);
 *  mask = gdk_bitmap_create_from_data (NULL, cursor1mask_bits,
 *                                      cursor1_width, cursor1_height);
 *  cursor = gdk_cursor_new_from_pixmap (source, mask, &amp;fg, &amp;bg, 8, 8);
 *  g_object_unref (source);
 *  g_object_unref (mask);
 *  
 *  
 *  gdk_window_set_cursor (widget->window, cursor);
 * </programlisting>
 * </example>
 *
 * Return value: a new #GdkCursor.
 **/
GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap      *source,
			    GdkPixmap      *mask,
			    const GdkColor *fg,
			    const GdkColor *bg,
			    gint            x,
			    gint            y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Pixmap source_pixmap, mask_pixmap;
  Cursor xcursor;
  XColor xfg, xbg;
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  source_pixmap = GDK_PIXMAP_XID (source);
  mask_pixmap   = GDK_PIXMAP_XID (mask);
  display = GDK_PIXMAP_DISPLAY (source);

  xfg.pixel = fg->pixel;
  xfg.red = fg->red;
  xfg.blue = fg->blue;
  xfg.green = fg->green;
  xbg.pixel = bg->pixel;
  xbg.red = bg->red;
  xbg.blue = bg->blue;
  xbg.green = bg->green;
  
  if (display->closed)
    xcursor = None;
  else
    xcursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
				   source_pixmap, mask_pixmap, &xfg, &xbg, x, y);
  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  
  return cursor;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivate *) cursor;
  if (!private->display->closed && private->xcursor)
    XFreeCursor (GDK_DISPLAY_XDISPLAY (private->display), private->xcursor);

  g_free (private->name);
  g_free (private);
}

/**
 * gdk_x11_cursor_get_xdisplay:
 * @cursor: a #GdkCursor.
 * 
 * Returns the display of a #GdkCursor.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_cursor_get_xdisplay (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return GDK_DISPLAY_XDISPLAY(((GdkCursorPrivate *)cursor)->display);
}

/**
 * gdk_x11_cursor_get_xcursor:
 * @cursor: a #GdkCursor.
 * 
 * Returns the X cursor belonging to a #GdkCursor.
 * 
 * Return value: an Xlib <type>Cursor</type>.
 **/
Cursor
gdk_x11_cursor_get_xcursor (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, None);

  return ((GdkCursorPrivate *)cursor)->xcursor;
}

/** 
 * gdk_cursor_get_display:
 * @cursor: a #GdkCursor.
 *
 * Returns the display on which the #GdkCursor is defined.
 *
 * Returns: the #GdkDisplay associated to @cursor
 *
 * Since: 2.2
 */

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return ((GdkCursorPrivate *)cursor)->display;
}

#if defined(HAVE_XCURSOR) && defined(HAVE_XFIXES) && XFIXES_MAJOR >= 2

/**
 * gdk_cursor_get_image:
 * @cursor: a #GdkCursor
 *
 * Returns a #GdkPixbuf with the image used to display the cursor.
 *
 * Note that depending on the capabilities of the windowing system and 
 * on the cursor, GDK may not be able to obtain the image data. In this 
 * case, %NULL is returned.
 *
 * Returns: a #GdkPixbuf representing @cursor, or %NULL
 *
 * Since: 2.8
 */
GdkPixbuf*  
gdk_cursor_get_image (GdkCursor *cursor)
{
  Display *xdisplay;
  GdkCursorPrivate *private;
  XcursorImages *images = NULL;
  XcursorImage *image;
  gint size;
  gchar buf[32];
  guchar *data, *p, tmp;
  GdkPixbuf *pixbuf;
  gchar *theme;
  
  g_return_val_if_fail (cursor != NULL, NULL);

  private = (GdkCursorPrivate *) cursor;
    
  xdisplay = GDK_DISPLAY_XDISPLAY (private->display);

  size = XcursorGetDefaultSize (xdisplay);
  theme = XcursorGetTheme (xdisplay);

  if (cursor->type == GDK_CURSOR_IS_PIXMAP)
    {
      if (private->name)
	images = XcursorLibraryLoadImages (private->name, theme, size);
    }
  else 
    images = XcursorShapeLoadImages (cursor->type, theme, size);

  if (!images)
    return NULL;
  
  image = images->images[0];

  data = g_malloc (4 * image->width * image->height);
  memcpy (data, image->pixels, 4 * image->width * image->height);

  for (p = data; p < data + (4 * image->width * image->height); p += 4)
    {
      tmp = p[0];
      p[0] = p[2];
      p[2] = tmp;
    }

  pixbuf = gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, TRUE,
				     8, image->width, image->height,
				     4 * image->width, 
				     (GdkPixbufDestroyNotify)g_free, NULL);

  if (private->name)
    gdk_pixbuf_set_option (pixbuf, "name", private->name);
  g_snprintf (buf, 32, "%d", image->xhot);
  gdk_pixbuf_set_option (pixbuf, "x_hot", buf);
  g_snprintf (buf, 32, "%d", image->yhot);
  gdk_pixbuf_set_option (pixbuf, "y_hot", buf);

  XcursorImagesDestroy (images);

  return pixbuf;
}

void
_gdk_x11_cursor_update_theme (GdkCursor *cursor)
{
  Display *xdisplay;
  GdkCursorPrivate *private;
  Cursor new_cursor = None;
  GdkDisplayX11 *display_x11;

  private = (GdkCursorPrivate *) cursor;
  xdisplay = GDK_DISPLAY_XDISPLAY (private->display);
  display_x11 = GDK_DISPLAY_X11 (private->display);

  if (!display_x11->have_xfixes)
    return;

  if (private->serial == theme_serial)
    return;

  private->serial = theme_serial;

  if (private->xcursor != None)
    {
      if (cursor->type == GDK_BLANK_CURSOR)
        return;

      if (cursor->type == GDK_CURSOR_IS_PIXMAP)
	{
	  if (private->name)
	    new_cursor = XcursorLibraryLoadCursor (xdisplay, private->name);
	}
      else 
	new_cursor = XcursorShapeLoadCursor (xdisplay, cursor->type);
      
      if (new_cursor != None)
	{
	  XFixesChangeCursor (xdisplay, new_cursor, private->xcursor);
 	  private->xcursor = new_cursor;
	}
    }
}

static void
update_cursor (gpointer data,
	       gpointer user_data)
{
  GdkCursor *cursor;

  cursor = (GdkCursor*)(data);

  if (!cursor)
    return;
  
  _gdk_x11_cursor_update_theme (cursor);
}

/**
 * gdk_x11_display_set_cursor_theme:
 * @display: a #GdkDisplay
 * @theme: the name of the cursor theme to use, or %NULL to unset
 *         a previously set value 
 * @size: the cursor size to use, or 0 to keep the previous size
 *
 * Sets the cursor theme from which the images for cursor
 * should be taken. 
 * 
 * If the windowing system supports it, existing cursors created 
 * with gdk_cursor_new(), gdk_cursor_new_for_display() and 
 * gdk_cursor_new_for_name() are updated to reflect the theme 
 * change. Custom cursors constructed with gdk_cursor_new_from_pixmap() 
 * or gdk_cursor_new_from_pixbuf() will have to be handled
 * by the application (GTK+ applications can learn about 
 * cursor theme changes by listening for change notification
 * for the corresponding #GtkSetting).
 *
 * Since: 2.8
 */
void
gdk_x11_display_set_cursor_theme (GdkDisplay  *display,
				  const gchar *theme,
				  const gint   size)
{
  GdkDisplayX11 *display_x11;
  Display *xdisplay;
  gchar *old_theme;
  gint old_size;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_x11 = GDK_DISPLAY_X11 (display);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  old_theme = XcursorGetTheme (xdisplay);
  old_size = XcursorGetDefaultSize (xdisplay);

  if (old_size == size &&
      (old_theme == theme ||
       (old_theme && theme && strcmp (old_theme, theme) == 0)))
    return;

  theme_serial++;

  XcursorSetTheme (xdisplay, theme);
  if (size > 0)
    XcursorSetDefaultSize (xdisplay, size);
    
  g_slist_foreach (cursor_cache, update_cursor, NULL);
}

#else

GdkPixbuf*  
gdk_cursor_get_image (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);
  
  return NULL;
}

void
gdk_x11_display_set_cursor_theme (GdkDisplay  *display,
				  const gchar *theme,
				  const gint   size)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

void
_gdk_x11_cursor_update_theme (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);
}

#endif

#ifdef HAVE_XCURSOR

static XcursorImage*
create_cursor_image (GdkPixbuf *pixbuf,
		     gint       x,
		     gint       y)
{
  guint width, height, rowstride, n_channels;
  guchar *pixels, *src;
  XcursorImage *xcimage;
  XcursorPixel *dest;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  xcimage = XcursorImageCreate (width, height);

  xcimage->xhot = x;
  xcimage->yhot = y;

  dest = xcimage->pixels;

  if (n_channels == 3)
    {
      gint i, j;

      for (j = 0; j < height; j++)
        {
          src = pixels + j * rowstride;
          for (i = 0; i < width; i++)
            {
              *dest = (0xff << 24) | (src[0] << 16) | (src[1] << 8) | src[2];
            }

	  src += n_channels;
	  dest++;
	}
    }
  else
    {
      _gdk_x11_convert_to_format (pixels, rowstride,
                                  (guchar *) dest, 4 * width,
                                  GDK_X11_FORMAT_ARGB,
                                  (G_BYTE_ORDER == G_BIG_ENDIAN) ?
                                  GDK_MSB_FIRST : GDK_LSB_FIRST,
                                  width, height);
    }

  return xcimage;
}


/**
 * gdk_cursor_new_from_pixbuf:
 * @display: the #GdkDisplay for which the cursor will be created
 * @pixbuf: the #GdkPixbuf containing the cursor image
 * @x: the horizontal offset of the 'hotspot' of the cursor. 
 * @y: the vertical offset of the 'hotspot' of the cursor.
 *
 * Creates a new cursor from a pixbuf. 
 *
 * Not all GDK backends support RGBA cursors. If they are not 
 * supported, a monochrome approximation will be displayed. 
 * The functions gdk_display_supports_cursor_alpha() and 
 * gdk_display_supports_cursor_color() can be used to determine
 * whether RGBA cursors are supported; 
 * gdk_display_get_default_cursor_size() and 
 * gdk_display_get_maximal_cursor_size() give information about 
 * cursor sizes.
 *
 * If @x or @y are <literal>-1</literal>, the pixbuf must have
 * options named "x_hot" and "y_hot", resp., containing
 * integer values between %0 and the width resp. height of
 * the pixbuf. (Since: 3.0)
 *
 * On the X backend, support for RGBA cursors requires a
 * sufficently new version of the X Render extension. 
 *
 * Returns: a new #GdkCursor.
 * 
 * Since: 2.4
 */
GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display, 
			    GdkPixbuf  *pixbuf,
			    gint        x,
			    gint        y)
{
  XcursorImage *xcimage;
  Cursor xcursor;
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  const char *option;
  char *end;
  gint64 value;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  if (x == -1 && (option = gdk_pixbuf_get_option (pixbuf, "x_hot")))
    {
      errno = 0;
      end = NULL;
      value = g_ascii_strtoll (option, &end, 10);
      if (errno == 0 &&
          end != option &&
          value >= 0 && value < G_MAXINT)
        x = (gint) value;
    }
  if (y == -1 && (option = gdk_pixbuf_get_option (pixbuf, "y_hot")))
    {
      errno = 0;
      end = NULL;
      value = g_ascii_strtoll (option, &end, 10);
      if (errno == 0 &&
          end != option &&
          value >= 0 && value < G_MAXINT)
        y = (gint) value;
    }

  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  if (display->closed)
    xcursor = None;
  else 
    {
      xcimage = create_cursor_image (pixbuf, x, y);
      xcursor = XcursorImageLoadCursor (GDK_DISPLAY_XDISPLAY (display), xcimage);
      XcursorImageDestroy (xcimage);
    }

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = NULL;
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  
  return cursor;
}

/**
 * gdk_cursor_new_from_name:
 * @display: the #GdkDisplay for which the cursor will be created
 * @name: the name of the cursor
 *
 * Creates a new cursor by looking up @name in the current cursor
 * theme. 
 * 
 * Returns: a new #GdkCursor, or %NULL if there is no cursor with 
 *   the given name 
 *
 * Since: 2.8
 */
GdkCursor*  
gdk_cursor_new_from_name (GdkDisplay  *display,
			  const gchar *name)
{
  Cursor xcursor;
  Display *xdisplay;
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->closed)
    xcursor = None;
  else 
    {
      private = find_in_cache (display, GDK_CURSOR_IS_PIXMAP, name);

      if (private)
        {
          /* Cache had it, add a ref for this user */
          gdk_cursor_ref ((GdkCursor*) private);

          return (GdkCursor*) private;
        }

      xdisplay = GDK_DISPLAY_XDISPLAY (display);
      xcursor = XcursorLibraryLoadCursor (xdisplay, name);
      if (xcursor == None)
	return NULL;
    }

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  private->name = g_strdup (name);
  private->serial = theme_serial;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  add_to_cache (private);

  return cursor;
}

/**
 * gdk_display_supports_cursor_alpha:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if cursors can use an 8bit alpha channel 
 * on @display. Otherwise, cursors are restricted to bilevel 
 * alpha (i.e. a mask).
 *
 * Returns: whether cursors can have alpha channels.
 *
 * Since: 2.4
 */
gboolean 
gdk_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return XcursorSupportsARGB (GDK_DISPLAY_XDISPLAY (display));
}

/**
 * gdk_display_supports_cursor_color:
 * @display: a #GdkDisplay
 *
 * Returns %TRUE if multicolored cursors are supported
 * on @display. Otherwise, cursors have only a forground
 * and a background color.
 *
 * Returns: whether cursors can have multiple colors.
 *
 * Since: 2.4
 */
gboolean 
gdk_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return XcursorSupportsARGB (GDK_DISPLAY_XDISPLAY (display));
}

/**
 * gdk_display_get_default_cursor_size:
 * @display: a #GdkDisplay
 *
 * Returns the default size to use for cursors on @display.
 *
 * Returns: the default cursor size.
 *
 * Since: 2.4
 */
guint     
gdk_display_get_default_cursor_size (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return XcursorGetDefaultSize (GDK_DISPLAY_XDISPLAY (display));
}

#else

GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display, 
			    GdkPixbuf  *pixbuf,
			    gint        x,
			    gint        y)
{
  GdkCursor *cursor;
  GdkPixmap *pixmap, *mask;
  guint width, height, n_channels, rowstride, i, j;
  guint8 *data, *mask_data, *pixels;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0xffff, 0xffff, 0xffff };
  GdkScreen *screen;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  g_return_val_if_fail (0 <= x && x < width, NULL);
  g_return_val_if_fail (0 <= y && y < height, NULL);

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  data = g_new0 (guint8, (width + 7) / 8 * height);
  mask_data = g_new0 (guint8, (width + 7) / 8 * height);

  for (j = 0; j < height; j++)
    {
      guint8 *src = pixels + j * rowstride;
      guint8 *d = data + (width + 7) / 8 * j;
      guint8 *md = mask_data + (width + 7) / 8 * j;
	
      for (i = 0; i < width; i++)
	{
	  if (src[1] < 0x80)
	    *d |= 1 << (i % 8);
	  
	  if (n_channels == 3 || src[3] >= 0x80)
	    *md |= 1 << (i % 8);
	  
	  src += n_channels;
	  if (i % 8 == 7)
	    {
	      d++; 
	      md++;
	    }
	}
    }
      
  screen = gdk_display_get_default_screen (display);
  pixmap = gdk_bitmap_create_from_data (gdk_screen_get_root_window (screen), 
					data, width, height);
 
  mask = gdk_bitmap_create_from_data (gdk_screen_get_root_window (screen),
				      mask_data, width, height);
   
  cursor = gdk_cursor_new_from_pixmap (pixmap, mask, &fg, &bg, x, y);
   
  g_object_unref (pixmap);
  g_object_unref (mask);

  g_free (data);
  g_free (mask_data);
  
  return cursor;
}

GdkCursor*  
gdk_cursor_new_from_name (GdkDisplay  *display,
			  const gchar *name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

gboolean 
gdk_display_supports_cursor_alpha (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return FALSE;
}

gboolean 
gdk_display_supports_cursor_color (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return FALSE;
}

guint     
gdk_display_get_default_cursor_size (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  /* no idea, really */
  return 20; 
}

#endif


/**
 * gdk_display_get_maximal_cursor_size:
 * @display: a #GdkDisplay
 * @width: (out): the return location for the maximal cursor width
 * @height: (out): the return location for the maximal cursor height
 *
 * Gets the maximal size to use for cursors on @display.
 *
 * Since: 2.4
 */
void     
gdk_display_get_maximal_cursor_size (GdkDisplay *display,
				     guint       *width,
				     guint       *height)
{
  GdkScreen *screen;
  GdkWindow *window;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  screen = gdk_display_get_default_screen (display);
  window = gdk_screen_get_root_window (screen);
  XQueryBestCursor (GDK_DISPLAY_XDISPLAY (display), 
		    GDK_WINDOW_XWINDOW (window), 
		    128, 128, width, height);
}

#define __GDK_CURSOR_X11_C__
#include "gdkaliasdef.c"
