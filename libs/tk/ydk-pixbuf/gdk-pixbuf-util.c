/* GdkPixbuf library - Utilities and miscellaneous convenience functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gimp.org>
 *          Cody Russell  <bratsche@gnome.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>
#include <libintl.h>

#include "gdk-pixbuf-transform.h"
#include "gdk-pixbuf-private.h"

/**
 * SECTION:util
 * @Short_description: Utility and miscellaneous convenience functions.
 * @Title: Utilities
 * @See_also: #GdkPixbuf
 * 
 * These functions provide miscellaneous utilities for manipulating
 * pixbufs.  The pixel data in pixbufs may of course be manipulated
 * directly by applications, but several common operations can be
 * performed by these functions instead.
 */


/**
 * gdk_pixbuf_add_alpha:
 * @pixbuf: A #GdkPixbuf.
 * @substitute_color: Whether to set a color to zero opacity.  If this
 * is %FALSE, then the (@r, @g, @b) arguments will be ignored.
 * @r: Red value to substitute.
 * @g: Green value to substitute.
 * @b: Blue value to substitute.
 *
 * Takes an existing pixbuf and adds an alpha channel to it.
 * If the existing pixbuf already had an alpha channel, the channel
 * values are copied from the original; otherwise, the alpha channel
 * is initialized to 255 (full opacity).
 * 
 * If @substitute_color is %TRUE, then the color specified by (@r, @g, @b) will be
 * assigned zero opacity. That is, if you pass (255, 255, 255) for the
 * substitute color, all white pixels will become fully transparent.
 *
 * Return value: (transfer full): A newly-created pixbuf with a reference count of 1.
 **/
GdkPixbuf *
gdk_pixbuf_add_alpha (const GdkPixbuf *pixbuf,
		      gboolean substitute_color, guchar r, guchar g, guchar b)
{
	GdkPixbuf *new_pixbuf;
	int x, y;
	const guint8 *src_pixels;
	guint8 *ret_pixels;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
	g_return_val_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4, NULL);
	g_return_val_if_fail (pixbuf->bits_per_sample == 8, NULL);

	src_pixels = gdk_pixbuf_read_pixels (pixbuf);

	if (pixbuf->has_alpha) {
		new_pixbuf = gdk_pixbuf_copy (pixbuf);
		if (!new_pixbuf)
			return NULL;

                if (!substitute_color)
                        return new_pixbuf;
	} else {
                new_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pixbuf->width, pixbuf->height);
        }
        
	if (!new_pixbuf)
		return NULL;

	ret_pixels = gdk_pixbuf_get_pixels (new_pixbuf);

	for (y = 0; y < pixbuf->height; y++) {
		const guchar *src;
		guchar *dest;
		guchar tr, tg, tb;

		src = src_pixels + y * pixbuf->rowstride;
		dest = ret_pixels + y * new_pixbuf->rowstride;
                
                if (pixbuf->has_alpha) {
                        /* Just subst color, we already copied everything else */
                        for (x = 0; x < pixbuf->width; x++) {
                                if (src[0] == r && src[1] == g && src[2] == b)
                                        dest[3] = 0;
                                src += 4;
                                dest += 4;
                        }
                } else {                        
                        for (x = 0; x < pixbuf->width; x++) {
                                tr = *dest++ = *src++;
                                tg = *dest++ = *src++;
                                tb = *dest++ = *src++;
                                
                                if (substitute_color && tr == r && tg == g && tb == b)
                                        *dest++ = 0;
                                else
                                        *dest++ = 255;
                        }
		}
	}

	return new_pixbuf;
}

/**
 * gdk_pixbuf_copy_area:
 * @src_pixbuf: Source pixbuf.
 * @src_x: Source X coordinate within @src_pixbuf.
 * @src_y: Source Y coordinate within @src_pixbuf.
 * @width: Width of the area to copy.
 * @height: Height of the area to copy.
 * @dest_pixbuf: Destination pixbuf.
 * @dest_x: X coordinate within @dest_pixbuf.
 * @dest_y: Y coordinate within @dest_pixbuf.
 *
 * Copies a rectangular area from @src_pixbuf to @dest_pixbuf.  Conversion of
 * pixbuf formats is done automatically.
 *
 * If the source rectangle overlaps the destination rectangle on the
 * same pixbuf, it will be overwritten during the copy operation.
 * Therefore, you can not use this function to scroll a pixbuf.
 **/
void
gdk_pixbuf_copy_area (const GdkPixbuf *src_pixbuf,
		      int src_x, int src_y,
		      int width, int height,
		      GdkPixbuf *dest_pixbuf,
		      int dest_x, int dest_y)
{
	g_return_if_fail (src_pixbuf != NULL);
	g_return_if_fail (dest_pixbuf != NULL);

	g_return_if_fail (src_x >= 0 && src_x + width <= src_pixbuf->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= src_pixbuf->height);

	g_return_if_fail (dest_x >= 0 && dest_x + width <= dest_pixbuf->width);
	g_return_if_fail (dest_y >= 0 && dest_y + height <= dest_pixbuf->height);

        g_return_if_fail (!(gdk_pixbuf_get_has_alpha (src_pixbuf) && !gdk_pixbuf_get_has_alpha (dest_pixbuf)));
        
	/* This will perform format conversions automatically */

	gdk_pixbuf_scale (src_pixbuf,
			  dest_pixbuf,
			  dest_x, dest_y,
			  width, height,
			  (double) (dest_x - src_x),
			  (double) (dest_y - src_y),
			  1.0, 1.0,
			  GDK_INTERP_NEAREST);
}



/**
 * gdk_pixbuf_saturate_and_pixelate:
 * @src: source image
 * @dest: place to write modified version of @src
 * @saturation: saturation factor
 * @pixelate: whether to pixelate
 *
 * Modifies saturation and optionally pixelates @src, placing the result in
 * @dest. @src and @dest may be the same pixbuf with no ill effects.  If
 * @saturation is 1.0 then saturation is not changed. If it's less than 1.0,
 * saturation is reduced (the image turns toward grayscale); if greater than
 * 1.0, saturation is increased (the image gets more vivid colors). If @pixelate
 * is %TRUE, then pixels are faded in a checkerboard pattern to create a
 * pixelated image. @src and @dest must have the same image format, size, and
 * rowstride.
 * 
 **/
void
gdk_pixbuf_saturate_and_pixelate(const GdkPixbuf *src,
                                 GdkPixbuf *dest,
                                 gfloat saturation,
                                 gboolean pixelate)
{
        /* NOTE that src and dest MAY be the same pixbuf! */
  
        g_return_if_fail (GDK_IS_PIXBUF (src));
        g_return_if_fail (GDK_IS_PIXBUF (dest));
        g_return_if_fail (gdk_pixbuf_get_height (src) == gdk_pixbuf_get_height (dest));
        g_return_if_fail (gdk_pixbuf_get_width (src) == gdk_pixbuf_get_width (dest));
        g_return_if_fail (gdk_pixbuf_get_has_alpha (src) == gdk_pixbuf_get_has_alpha (dest));
        g_return_if_fail (gdk_pixbuf_get_colorspace (src) == gdk_pixbuf_get_colorspace (dest));
  
        if (saturation == 1.0 && !pixelate) {
                if (dest != src)
                        gdk_pixbuf_copy_area (src, 0, 0, 
                                              gdk_pixbuf_get_width (src),
                                              gdk_pixbuf_get_height (src),
                                              dest, 0, 0);
        } else {
                int i, j, t;
                int width, height, has_alpha, src_rowstride, dest_rowstride, bytes_per_pixel;
		const guchar *src_line;
		guchar *dest_line;
                const guchar *src_pixel;
		guchar *dest_pixel;
                guchar intensity;

                has_alpha = gdk_pixbuf_get_has_alpha (src);
		bytes_per_pixel = has_alpha ? 4 : 3;
                width = gdk_pixbuf_get_width (src);
                height = gdk_pixbuf_get_height (src);
                src_rowstride = gdk_pixbuf_get_rowstride (src);
                dest_rowstride = gdk_pixbuf_get_rowstride (dest);
                
                dest_line = gdk_pixbuf_get_pixels (dest);
                src_line = gdk_pixbuf_read_pixels (src);
		
#define DARK_FACTOR 0.7
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
#define CLAMP_UCHAR(v) (t = (v), CLAMP (t, 0, 255))
#define SATURATE(v) ((1.0 - saturation) * intensity + saturation * (v))

		for (i = 0 ; i < height ; i++) {
			src_pixel = src_line;
			src_line += src_rowstride;
			dest_pixel = dest_line;
			dest_line += dest_rowstride;

			for (j = 0 ; j < width ; j++) {
                                intensity = INTENSITY (src_pixel[0], src_pixel[1], src_pixel[2]);
                                if (pixelate && (i + j) % 2 == 0) {
                                        dest_pixel[0] = intensity / 2 + 127;
                                        dest_pixel[1] = intensity / 2 + 127;
                                        dest_pixel[2] = intensity / 2 + 127;
                                } else if (pixelate) {
                                        dest_pixel[0] = CLAMP_UCHAR ((SATURATE (src_pixel[0])) * DARK_FACTOR);
					dest_pixel[1] = CLAMP_UCHAR ((SATURATE (src_pixel[1])) * DARK_FACTOR);
                                        dest_pixel[2] = CLAMP_UCHAR ((SATURATE (src_pixel[2])) * DARK_FACTOR);
                                } else {
                                        dest_pixel[0] = CLAMP_UCHAR (SATURATE (src_pixel[0]));
                                        dest_pixel[1] = CLAMP_UCHAR (SATURATE (src_pixel[1]));
                                        dest_pixel[2] = CLAMP_UCHAR (SATURATE (src_pixel[2]));
                                }
				
                                if (has_alpha)
                                        dest_pixel[3] = src_pixel[3];

				src_pixel += bytes_per_pixel;
				dest_pixel += bytes_per_pixel;
			}
                }
        }
}


/**
 * gdk_pixbuf_apply_embedded_orientation:
 * @src: A #GdkPixbuf.
 *
 * Takes an existing pixbuf and checks for the presence of an
 * associated "orientation" option, which may be provided by the 
 * jpeg loader (which reads the exif orientation tag) or the 
 * tiff loader (which reads the tiff orientation tag, and
 * compensates it for the partial transforms performed by 
 * libtiff). If an orientation option/tag is present, the
 * appropriate transform will be performed so that the pixbuf
 * is oriented correctly.
 *
 * Return value: (transfer full): A newly-created pixbuf, or a reference to the
 * input pixbuf (with an increased reference count).
 *
 * Since: 2.12
 **/
GdkPixbuf *
gdk_pixbuf_apply_embedded_orientation (GdkPixbuf *src)
{
  	const gchar *orientation_string;
	int          transform = 0;
	GdkPixbuf   *temp;
	GdkPixbuf   *dest;

	g_return_val_if_fail (GDK_IS_PIXBUF (src), NULL);

	/* Read the orientation option associated with the pixbuf */
	orientation_string = gdk_pixbuf_get_option (src, "orientation");	

	if (orientation_string) {
		/* If an orientation option was found, convert the 
		   orientation string into an integer. */
		transform = (int) g_ascii_strtoll (orientation_string, NULL, 10);
	}

	/* Apply the actual transforms, which involve rotations and flips. 
	   The meaning of orientation values 1-8 and the required transforms
	   are defined by the TIFF and EXIF (for JPEGs) standards. */
        switch (transform) {
        case 1:
                dest = src;
                g_object_ref (dest);
                break;
        case 2:
                dest = gdk_pixbuf_flip (src, TRUE);
                break;
        case 3:
                dest = gdk_pixbuf_rotate_simple (src, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
                break;
        case 4:
                dest = gdk_pixbuf_flip (src, FALSE);
                break;
        case 5:
                temp = gdk_pixbuf_rotate_simple (src, GDK_PIXBUF_ROTATE_CLOCKWISE);
                dest = gdk_pixbuf_flip (temp, TRUE);
                g_object_unref (temp);
                break;
        case 6:
                dest = gdk_pixbuf_rotate_simple (src, GDK_PIXBUF_ROTATE_CLOCKWISE);
                break;
        case 7:
                temp = gdk_pixbuf_rotate_simple (src, GDK_PIXBUF_ROTATE_CLOCKWISE);
                dest = gdk_pixbuf_flip (temp, FALSE);
                g_object_unref (temp);
                break;
        case 8:
                dest = gdk_pixbuf_rotate_simple (src, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
                break;
        default:
		/* if no orientation tag was present */
                dest = src;
                g_object_ref (dest);
                break;
        }

        return dest;
}

#ifdef G_OS_WIN32

static const gchar *
get_localedir (void)
{
    gchar *temp;
    gchar *retval;
    
    /* In gdk-pixbuf-io.c */
    extern char *_gdk_pixbuf_win32_get_toplevel (void);

    temp = g_build_filename (_gdk_pixbuf_win32_get_toplevel (), "share/locale", NULL);

    /* The localedir is passed to bindtextdomain() which isn't
     * UTF-8-aware.
     */
    retval = g_win32_locale_filename_from_utf8 (temp);
    g_free (temp);
    return retval;
}

#undef GDK_PIXBUF_LOCALEDIR
#define GDK_PIXBUF_LOCALEDIR get_localedir ()

#endif

const gchar *
gdk_pixbuf_gettext (const gchar *msgid)
{
        static gsize gettext_initialized = FALSE;

        if (G_UNLIKELY (g_once_init_enter (&gettext_initialized))) {
                bindtextdomain (GETTEXT_PACKAGE, GDK_PIXBUF_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
                bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
                g_once_init_leave (&gettext_initialized, TRUE);
        }

        return g_dgettext (GETTEXT_PACKAGE, msgid);
}
