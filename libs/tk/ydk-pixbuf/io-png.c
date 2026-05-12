/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - PNG image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <math.h>
#include "gdk-pixbuf-private.h"

/* Helper macros to convert between density units */
#define DPI_TO_DPM(value) ((int) round ((value) * 1000 / 25.4))
#define DPM_TO_DPI(value) ((int) round ((value) * 25.4 / 1000))

#define DEFAULT_FILL_COLOR 0x979899ff


static gboolean
setup_png_transformations(png_structp png_read_ptr, png_infop png_info_ptr,
                          GError **error,
                          png_uint_32* width_p, png_uint_32* height_p,
                          int* color_type_p)
{
        png_uint_32 width, height;
        int bit_depth, color_type, interlace_type, compression_type, filter_type;
        int channels;
        
        /* Get the image info */

        /* Must check bit depth, since png_get_IHDR generates an 
           FPE on bit_depth 0.
        */
        bit_depth = png_get_bit_depth (png_read_ptr, png_info_ptr);
        if (bit_depth < 1 || bit_depth > 16) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Bits per channel of PNG image is invalid."));
                return FALSE;
        }
        png_get_IHDR (png_read_ptr, png_info_ptr,
                      &width, &height,
                      &bit_depth,
                      &color_type,
                      &interlace_type,
                      &compression_type,
                      &filter_type);

        /* set_expand() basically needs to be called unless
           we are already in RGB/RGBA mode
        */
        if (color_type == PNG_COLOR_TYPE_PALETTE &&
            bit_depth <= 8) {

                /* Convert indexed images to RGB */
                png_set_expand (png_read_ptr);

        } else if (color_type == PNG_COLOR_TYPE_GRAY &&
                   bit_depth < 8) {

                /* Convert grayscale to RGB */
                png_set_expand (png_read_ptr);

        } else if (png_get_valid (png_read_ptr, 
                                  png_info_ptr, PNG_INFO_tRNS)) {

                /* If we have transparency header, convert it to alpha
                   channel */
                png_set_expand(png_read_ptr);
                
        } else if (bit_depth < 8) {

                /* If we have < 8 scale it up to 8 */
                png_set_expand(png_read_ptr);


                /* Conceivably, png_set_packing() is a better idea;
                 * God only knows how libpng works
                 */
        }

        /* If we are 16-bit, convert to 8-bit */
        if (bit_depth == 16) {
                png_set_strip_16(png_read_ptr);
        }

        /* If gray scale, convert to RGB */
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(png_read_ptr);
        }
        
        /* If interlaced, handle that */
        if (interlace_type != PNG_INTERLACE_NONE) {
                png_set_interlace_handling(png_read_ptr);
        }
        
        /* Update the info the reflect our transformations */
        png_read_update_info(png_read_ptr, png_info_ptr);
        
        png_get_IHDR (png_read_ptr, png_info_ptr,
                      &width, &height,
                      &bit_depth,
                      &color_type,
                      &interlace_type,
                      &compression_type,
                      &filter_type);

        *width_p = width;
        *height_p = height;
        *color_type_p = color_type;
        
        /* Check that the new info is what we want */
        
        if (width == 0 || height == 0) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Transformed PNG has zero width or height."));
                return FALSE;
        }

        if (bit_depth != 8) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Bits per channel of transformed PNG is not 8."));
                return FALSE;
        }

        if ( ! (color_type == PNG_COLOR_TYPE_RGB ||
                color_type == PNG_COLOR_TYPE_RGB_ALPHA) ) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Transformed PNG not RGB or RGBA."));
                return FALSE;
        }

        channels = png_get_channels(png_read_ptr, png_info_ptr);
        if ( ! (channels == 3 || channels == 4) ) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Transformed PNG has unsupported number of channels, must be 3 or 4."));
                return FALSE;
        }
        return TRUE;
}

static void
png_simple_error_callback(png_structp png_save_ptr,
                          png_const_charp error_msg)
{
        GError **error;
        
        error = png_get_error_ptr(png_save_ptr);

        /* I don't trust libpng to call the error callback only once,
         * so check for already-set error
         */
        if (error && *error == NULL) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Fatal error in PNG image file: %s"),
                             error_msg);
        }

        longjmp (png_jmpbuf(png_save_ptr), 1);
}

static void
png_simple_warning_callback(png_structp png_save_ptr,
                            png_const_charp warning_msg)
{
        /* Don't print anything; we should not be dumping junk to
         * stderr, since that may be bad for some apps. If it's
         * important enough to display, we need to add a GError
         * **warning return location wherever we have an error return
         * location.
         */
}

static gboolean
png_text_to_pixbuf_option (png_text   text_ptr,
                           gchar    **key,
                           gchar    **value)
{
        gboolean is_ascii = TRUE;
        int i;

        /* Avoid loading iconv if the text is plain ASCII */
        for (i = 0; i < text_ptr.text_length; i++)
                if (text_ptr.text[i] & 0x80) {
                        is_ascii = FALSE;
                        break;
                }

        if (is_ascii) {
                *value = g_strdup (text_ptr.text);
        } else {
                *value = g_convert (text_ptr.text, -1,
                                     "UTF-8", "ISO-8859-1",
                                     NULL, NULL, NULL);
        }

        if (*value) {
                *key = g_strconcat ("tEXt::", text_ptr.key, NULL);
                return TRUE;
        } else {
                g_warning ("Couldn't convert text chunk value to UTF-8.");
                *key = NULL;
                return FALSE;
        }
}

static png_voidp
png_malloc_callback (png_structp o, png_size_t size)
{
        return g_try_malloc (size);
}

static void
png_free_callback (png_structp o, png_voidp x)
{
        g_free (x);
}

/* Shared library entry point */
static GdkPixbuf *
gdk_pixbuf__png_image_load (FILE *f, GError **error)
{
        GdkPixbuf * volatile pixbuf = NULL;
        gint rowstride;
	png_structp png_ptr;
	png_infop info_ptr;
        png_textp text_ptr;
	gint i, ctype;
	png_uint_32 w, h;
	png_bytepp volatile rows = NULL;
        gint    num_texts;
        gchar *key;
        gchar *value;
        gchar *icc_profile_base64;
        const gchar *icc_profile_title;
        const gchar *icc_profile;
        png_uint_32 icc_profile_size;
        png_uint_32 x_resolution;
        png_uint_32 y_resolution;
        int unit_type;
        gchar *density_str;
        guint32 retval;
        gint compression_type;
        gpointer ptr;

#ifdef PNG_USER_MEM_SUPPORTED
	png_ptr = png_create_read_struct_2 (PNG_LIBPNG_VER_STRING,
                                            error,
                                            png_simple_error_callback,
                                            png_simple_warning_callback,
                                            NULL, 
                                            png_malloc_callback, 
                                            png_free_callback);
#else
	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                          error,
                                          png_simple_error_callback,
                                          png_simple_warning_callback);
#endif
	if (!png_ptr)
		return NULL;

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		return NULL;
	}

	if (setjmp (png_jmpbuf(png_ptr))) {
	    	g_free (rows);

		if (pixbuf)
			g_object_unref (pixbuf);

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	png_init_io (png_ptr, f);
	png_read_info (png_ptr, info_ptr);

        if (!setup_png_transformations(png_ptr, info_ptr, error, &w, &h, &ctype)) {
                png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
                return NULL;
        }
        
        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, ctype & PNG_COLOR_MASK_ALPHA, 8, w, h);

	if (!pixbuf) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Insufficient memory to load PNG file"));

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return NULL;
	}

        rowstride = gdk_pixbuf_get_rowstride (pixbuf);

        gdk_pixbuf_fill (pixbuf, DEFAULT_FILL_COLOR);

	rows = g_new (png_bytep, h);

        for (i = 0, ptr = gdk_pixbuf_get_pixels (pixbuf); i < h; i++, ptr = (guchar *) ptr + rowstride)
		rows[i] = ptr;

	png_read_image (png_ptr, rows);
        png_read_end (png_ptr, info_ptr);

        if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_texts)) {
                for (i = 0; i < num_texts; i++) {
                        png_text_to_pixbuf_option (text_ptr[i], &key, &value);
                        gdk_pixbuf_set_option (pixbuf, key, value);
                        g_free (key);
                        g_free (value);
                }
        }

#if defined(PNG_cHRM_SUPPORTED)
        /* Extract embedded ICC profile */
        retval = png_get_iCCP (png_ptr, info_ptr,
                               (png_charpp) &icc_profile_title, &compression_type,
                               (png_bytepp) &icc_profile, (png_uint_32*) &icc_profile_size);
        if (retval != 0) {
                icc_profile_base64 = g_base64_encode ((const guchar *) icc_profile, (gsize)icc_profile_size);
                gdk_pixbuf_set_option (pixbuf, "icc-profile", icc_profile_base64);
                g_free (icc_profile_base64);
        }
#endif

#ifdef PNG_pHYs_SUPPORTED
        retval = png_get_pHYs (png_ptr, info_ptr, &x_resolution, &y_resolution, &unit_type);
        if (retval != 0 && unit_type == PNG_RESOLUTION_METER) {
                density_str = g_strdup_printf ("%d", DPM_TO_DPI (x_resolution));
                gdk_pixbuf_set_option (pixbuf, "x-dpi", density_str);
                g_free (density_str);
                density_str = g_strdup_printf ("%d", DPM_TO_DPI (y_resolution));
                gdk_pixbuf_set_option (pixbuf, "y-dpi", density_str);
                g_free (density_str);
        }
#endif

	g_free (rows);
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

        return pixbuf;
}

/* I wish these avoided the setjmp()/longjmp() crap in libpng instead
   just allow you to change the error reporting. */
static void png_error_callback  (png_structp png_read_ptr,
                                 png_const_charp error_msg);

static void png_warning_callback (png_structp png_read_ptr,
                                  png_const_charp warning_msg);

/* Called at the start of the progressive load */
static void png_info_callback   (png_structp png_read_ptr,
                                 png_infop   png_info_ptr);

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void png_row_callback   (png_structp png_read_ptr,
                                png_bytep   new_row,
                                png_uint_32 row_num,
                                int pass_num);

/* Called after reading the entire image */
static void png_end_callback   (png_structp png_read_ptr,
                                png_infop   png_info_ptr);

typedef struct _LoadContext LoadContext;

struct _LoadContext {
        png_structp png_read_ptr;
        png_infop   png_info_ptr;

        GdkPixbufModuleSizeFunc size_func;
        GdkPixbufModulePreparedFunc prepared_func;
        GdkPixbufModuleUpdatedFunc updated_func;
        gpointer notify_user_data;

        GdkPixbuf* pixbuf;

        /* row number of first row seen, or -1 if none yet seen */

        gint first_row_seen_in_chunk;

        /* pass number for the first row seen */

        gint first_pass_seen_in_chunk;
        
        /* row number of last row seen */
        gint last_row_seen_in_chunk;

        gint last_pass_seen_in_chunk;

        /* highest row number seen */
        gint max_row_seen_in_chunk;
        
        guint fatal_error_occurred : 1;

        GError **error;
};

static gpointer
gdk_pixbuf__png_image_begin_load (GdkPixbufModuleSizeFunc size_func,
                                  GdkPixbufModulePreparedFunc prepared_func,
				  GdkPixbufModuleUpdatedFunc updated_func,
				  gpointer user_data,
                                  GError **error)
{
        LoadContext* lc;
        
        g_assert (size_func != NULL);
        g_assert (prepared_func != NULL);
        g_assert (updated_func != NULL);

        lc = g_new0(LoadContext, 1);
        
        lc->fatal_error_occurred = FALSE;

        lc->size_func = size_func;
        lc->prepared_func = prepared_func;
        lc->updated_func = updated_func;
        lc->notify_user_data = user_data;

        lc->first_row_seen_in_chunk = -1;
        lc->last_row_seen_in_chunk = -1;
        lc->first_pass_seen_in_chunk = -1;
        lc->last_pass_seen_in_chunk = -1;
        lc->max_row_seen_in_chunk = -1;
        lc->error = error;
        
        /* Create the main PNG context struct */

#ifdef PNG_USER_MEM_SUPPORTED
        lc->png_read_ptr = png_create_read_struct_2 (PNG_LIBPNG_VER_STRING,
                                                     lc, /* error/warning callback data */
                                                     png_error_callback,
                                                     png_warning_callback,
                                                     NULL,
                                                     png_malloc_callback,
                                                     png_free_callback);
#else
        lc->png_read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                  lc, /* error/warning callback data */
                                                  png_error_callback,
                                                  png_warning_callback);
#endif
        if (lc->png_read_ptr == NULL) {
                g_free(lc);

                /* A failure here isn't supposed to call the error
                 * callback, but it doesn't hurt to be careful.
                 */
                if (error && *error == NULL) {
                        g_set_error_literal (error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                             _("Couldn’t allocate memory for loading PNG"));
                }

                return NULL;
        }

        /* Create the auxiliary context struct */

        lc->png_info_ptr = png_create_info_struct(lc->png_read_ptr);

        if (lc->png_info_ptr == NULL) {
                png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
                g_free(lc);

                /* A failure here isn't supposed to call the error
                 * callback, but it doesn't hurt to be careful.
                 */
                if (error && *error == NULL) {
                        g_set_error_literal (error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                             _("Couldn’t allocate memory for loading PNG"));
                }

                return NULL;
        }

        if (setjmp (png_jmpbuf(lc->png_read_ptr))) {
                png_destroy_read_struct(&lc->png_read_ptr, &lc->png_info_ptr, NULL);
                g_free(lc);
                /* error callback should have set the error */
                return NULL;
        }

        png_set_progressive_read_fn(lc->png_read_ptr,
                                    lc, /* callback data */
                                    png_info_callback,
                                    png_row_callback,
                                    png_end_callback);
        

        /* We don't want to keep modifying error after returning here,
         * it may no longer be valid.
         */
        lc->error = NULL;
        
        return lc;
}

static gboolean
gdk_pixbuf__png_image_stop_load (gpointer context, GError **error)
{
        LoadContext* lc = context;
        gboolean retval = TRUE;

        g_return_val_if_fail(lc != NULL, TRUE);

        /* FIXME this thing needs to report errors if
         * we have unused image data
         */
        
        if (lc->pixbuf)
                g_object_unref (lc->pixbuf);
        else {
                g_set_error_literal (error, GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Premature end-of-file encountered"));
                retval = FALSE;
	}
        
        png_destroy_read_struct(&lc->png_read_ptr, &lc->png_info_ptr, NULL);
        g_free(lc);

        return retval;
}

static gboolean
gdk_pixbuf__png_image_load_increment(gpointer context,
                                     const guchar *buf, guint size,
                                     GError **error)
{
        LoadContext* lc = context;

        g_return_val_if_fail(lc != NULL, FALSE);

        /* reset */
        lc->first_row_seen_in_chunk = -1;
        lc->last_row_seen_in_chunk = -1;
        lc->first_pass_seen_in_chunk = -1;
        lc->last_pass_seen_in_chunk = -1;
        lc->max_row_seen_in_chunk = -1;
        lc->error = error;
        
        /* Invokes our callbacks as needed */
	if (setjmp (png_jmpbuf(lc->png_read_ptr))) {
                lc->error = NULL;
		return FALSE;
	} else {
		png_process_data(lc->png_read_ptr, lc->png_info_ptr,
                                 (guchar*) buf, size);
	}

        if (lc->fatal_error_occurred) {
                lc->error = NULL;
                return FALSE;
        } else {
                if (lc->first_row_seen_in_chunk >= 0) {
                        gint width = gdk_pixbuf_get_width (lc->pixbuf);
                        /* We saw at least one row */
                        gint pass_diff = lc->last_pass_seen_in_chunk - lc->first_pass_seen_in_chunk;
                        
                        g_assert(pass_diff >= 0);
                        
                        if (pass_diff == 0) {
                                /* start and end row were in the same pass */
                                (lc->updated_func)(lc->pixbuf, 0,
                                                   lc->first_row_seen_in_chunk,
                                                   width,
                                                   (lc->last_row_seen_in_chunk -
                                                    lc->first_row_seen_in_chunk) + 1,
                                                   lc->notify_user_data);
                        } else if (pass_diff == 1) {
                                /* We have from the first row seen to
                                   the end of the image (max row
                                   seen), then from the top of the
                                   image to the last row seen */
                                /* first row to end */
                                (lc->updated_func)(lc->pixbuf, 0,
                                                   lc->first_row_seen_in_chunk,
                                                   width,
                                                   (lc->max_row_seen_in_chunk -
                                                    lc->first_row_seen_in_chunk) + 1,
                                                   lc->notify_user_data);
                                /* top to last row */
                                (lc->updated_func)(lc->pixbuf,
                                                   0, 0, 
                                                   width,
                                                   lc->last_row_seen_in_chunk + 1,
                                                   lc->notify_user_data);
                        } else {
                                /* We made at least one entire pass, so update the
                                   whole image */
                                (lc->updated_func)(lc->pixbuf,
                                                   0, 0, 
                                                   width,
                                                   lc->max_row_seen_in_chunk + 1,
                                                   lc->notify_user_data);
                        }
                }

                lc->error = NULL;
                
                return TRUE;
        }
}

/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback   (png_structp png_read_ptr,
                     png_infop   png_info_ptr)
{
        LoadContext* lc;
        png_uint_32 width, height;
        png_textp png_text_ptr;
        int i, num_texts;
        int color_type;
        gboolean have_alpha = FALSE;
        gchar *icc_profile_base64;
        const gchar *icc_profile_title;
        const gchar *icc_profile;
        png_uint_32 icc_profile_size;
        png_uint_32 x_resolution;
        png_uint_32 y_resolution;
        int unit_type;
        gchar *density_str;
        guint32 retval;
        gint compression_type;

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;

        if (!setup_png_transformations(lc->png_read_ptr,
                                       lc->png_info_ptr,
                                       lc->error,
                                       &width, &height, &color_type)) {
                lc->fatal_error_occurred = TRUE;
                return;
        }

        /* If we have alpha, set a flag */
        if (color_type & PNG_COLOR_MASK_ALPHA)
                have_alpha = TRUE;
        
        {
                gint w = width;
                gint h = height;
                (* lc->size_func) (&w, &h, lc->notify_user_data);
                
                if (w == 0 || h == 0) {
                        lc->fatal_error_occurred = TRUE;
                        g_set_error_literal (lc->error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_FAILED,
                                             _("Transformed PNG has zero width or height."));
                        return;
                }
        }

        lc->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, have_alpha, 8, width, height);

        if (lc->pixbuf == NULL) {
                /* Failed to allocate memory */
                lc->fatal_error_occurred = TRUE;
                g_set_error (lc->error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                             _("Insufficient memory to store a %lu by %lu image; try exiting some applications to reduce memory usage"),
                             (gulong) width, (gulong) height);
                return;
        }

        gdk_pixbuf_fill (lc->pixbuf, DEFAULT_FILL_COLOR);

        /* Extract text chunks and attach them as pixbuf options */
        
        if (png_get_text (png_read_ptr, png_info_ptr, &png_text_ptr, &num_texts)) {
                for (i = 0; i < num_texts; i++) {
                        gchar *key, *value;

                        if (png_text_to_pixbuf_option (png_text_ptr[i],
                                                       &key, &value)) {
                                gdk_pixbuf_set_option (lc->pixbuf, key, value);
                                g_free (key);
                                g_free (value);
                        }
                }
        }

#if defined(PNG_cHRM_SUPPORTED)
        /* Extract embedded ICC profile */
        retval = png_get_iCCP (png_read_ptr, png_info_ptr,
                               (png_charpp) &icc_profile_title, &compression_type,
                               (png_bytepp) &icc_profile, &icc_profile_size);
        if (retval != 0) {
                icc_profile_base64 = g_base64_encode ((const guchar *) icc_profile, (gsize)icc_profile_size);
                gdk_pixbuf_set_option (lc->pixbuf, "icc-profile", icc_profile_base64);
                g_free (icc_profile_base64);
        }
#endif

#ifdef PNG_pHYs_SUPPORTED
        retval = png_get_pHYs (png_read_ptr, png_info_ptr, &x_resolution, &y_resolution, &unit_type);
        if (retval != 0 && unit_type == PNG_RESOLUTION_METER) {
                density_str = g_strdup_printf ("%d", DPM_TO_DPI (x_resolution));
                gdk_pixbuf_set_option (lc->pixbuf, "x-dpi", density_str);
                g_free (density_str);
                density_str = g_strdup_printf ("%d", DPM_TO_DPI (y_resolution));
                gdk_pixbuf_set_option (lc->pixbuf, "y-dpi", density_str);
                g_free (density_str);
        }
#endif

        /* Notify the client that we are ready to go */

        (* lc->prepared_func) (lc->pixbuf, NULL, lc->notify_user_data);

        return;
}

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback   (png_structp png_read_ptr,
                    png_bytep   new_row,
                    png_uint_32 row_num,
                    int pass_num)
{
        LoadContext* lc;
        guchar* old_row = NULL;
        gsize rowstride;

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;

        if (row_num >= gdk_pixbuf_get_height (lc->pixbuf)) {
                lc->fatal_error_occurred = TRUE;
                g_set_error_literal (lc->error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Fatal error reading PNG image file"));
                return;
        }

        if (lc->first_row_seen_in_chunk < 0) {
                lc->first_row_seen_in_chunk = row_num;
                lc->first_pass_seen_in_chunk = pass_num;
        }

        lc->max_row_seen_in_chunk = MAX(lc->max_row_seen_in_chunk, ((gint)row_num));
        lc->last_row_seen_in_chunk = row_num;
        lc->last_pass_seen_in_chunk = pass_num;

        rowstride = gdk_pixbuf_get_rowstride (lc->pixbuf);
        old_row = gdk_pixbuf_get_pixels (lc->pixbuf) + (row_num * rowstride);

        png_progressive_combine_row(lc->png_read_ptr, old_row, new_row);
}

/* Called after reading the entire image */
static void
png_end_callback   (png_structp png_read_ptr,
                    png_infop   png_info_ptr)
{
        LoadContext* lc;

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;
}

static void
png_error_callback(png_structp png_read_ptr,
                   png_const_charp error_msg)
{
        LoadContext* lc;
        
        lc = png_get_error_ptr(png_read_ptr);
        
        lc->fatal_error_occurred = TRUE;

        /* I don't trust libpng to call the error callback only once,
         * so check for already-set error
         */
        if (lc->error && *lc->error == NULL) {
                g_set_error (lc->error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Fatal error reading PNG image file: %s"),
                             error_msg);
        }

        longjmp (png_jmpbuf(png_read_ptr), 1);
}

static void
png_warning_callback (png_structp png_read_ptr,
                      png_const_charp warning_msg)
{
        /* Don't print anything; we should not be dumping junk to
         * stderr, since that may be bad for some apps. If it's
         * important enough to display, we need to add a GError
         * **warning return location wherever we have an error return
         * location.
         */
}


/* Save */

typedef struct {
        GdkPixbufSaveFunc save_func;
        gpointer user_data;
        GError **error;
} SaveToFunctionIoPtr;

static void
png_save_to_callback_write_func (png_structp png_ptr,
                                 png_bytep   data,
                                 png_size_t  length)
{
        SaveToFunctionIoPtr *ioptr = png_get_io_ptr (png_ptr);

        if (!ioptr->save_func ((gchar *)data, length, ioptr->error, ioptr->user_data)) {
                /* If save_func has already set an error, which it
                   should have done, this won't overwrite it. */
                png_error (png_ptr, "write function failed");
        }
}

static void
png_save_to_callback_flush_func (png_structp png_ptr)
{
        ;
}

static gboolean
real_save_png (GdkPixbuf        *pixbuf,
               int               n_keys,
               gchar           **keys,
               gchar           **values,
               GError          **error,
               gboolean          to_callback,
               FILE             *f,
               GdkPixbufSaveFunc save_func,
               gpointer          user_data)
{
        png_structp png_ptr = NULL;
        png_infop info_ptr;
        guchar *ptr;
        guchar *pixels;
        int y;
        png_bytep row_ptr;
        png_color_8 sig_bit;
        int w, h, rowstride;
        int has_alpha;
        int bpc;
        int compression = -1;
        int x_density = 0;
        int y_density = 0;
        gboolean success = TRUE;
        guchar *icc_profile = NULL;
        gsize icc_profile_size = 0;
        SaveToFunctionIoPtr to_callback_ioptr;
        int num_keys = 0;
        png_textp text_ptr = NULL;
        GArray *text_data = NULL;

        text_data = g_array_sized_new (FALSE, TRUE, sizeof (png_text), n_keys);

        for (int i = 0; i < n_keys; i++) {
                const char *key = keys[i];
                const char *value = values[i];

                if (strncmp (key, "tEXt::", 6) == 0) {
                        const char *unprefixed_key = key + 6;
                        int len = strlen (unprefixed_key);
                        png_text text;

                        if (len < 1 || len > 79) {
                                /* Translators notice: '%s' is the name of the
                                 * PNG text key
                                 */
                                g_set_error (error, GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_BAD_OPTION,
                                             _("Invalid key “%s”. Keys for PNG text chunks must have at least 1 and at most 79 characters."),
                                             unprefixed_key);
                                success = FALSE;
                                goto cleanup;
                        }

                        for (int i = 0; i < len; i++) {
                                if ((guchar) unprefixed_key[i] > 127) {
                                        /* Translators notice: '%s' is the name of
                                         * the PNG text key
                                         */
                                        g_set_error (error, GDK_PIXBUF_ERROR,
                                                     GDK_PIXBUF_ERROR_BAD_OPTION,
                                                     _("Invalid key “%s”. Keys for PNG text chunks must be ASCII characters."),
                                                     unprefixed_key);
                                        success = FALSE;
                                        goto cleanup;
                                }
                        }

                        text.compression = PNG_TEXT_COMPRESSION_NONE;
                        text.key = unprefixed_key;
                        text.text = g_convert (value, -1,
                                               "ISO-8859-1", "UTF-8",
                                               NULL,
                                               &text.text_length,
                                               NULL);

#ifdef PNG_iTXt_SUPPORTED 
                        if (text.text == NULL) {
                                text.compression = PNG_ITXT_COMPRESSION_NONE;
                                text.text = g_strdup (value);
                                text.text_length = 0;
                                text.itxt_length = strlen (value);
                                text.lang = NULL;
                                text.lang_key = NULL;
                        }
#endif

                        if (text.text == NULL) {
                                g_set_error (error,
                                            GDK_PIXBUF_ERROR,
                                            GDK_PIXBUF_ERROR_BAD_OPTION,
                                            _("Value for PNG text chunk '%s' cannot be converted to ISO-8859-1 encoding."), unprefixed_key);
                                success = FALSE;
                                goto cleanup;
                        }

                        g_array_append_val (text_data, text);
                } else if (strcmp (key, "icc-profile") == 0) {
                        icc_profile = g_base64_decode (value, &icc_profile_size);

                        if (icc_profile_size < 127) {
                                g_set_error (error, GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_BAD_OPTION,
                                             _("Color profile has invalid length %d"),
                                             (int) icc_profile_size);
                                success = FALSE;
                                goto cleanup;
                        }
                } else if (strcmp (key, "compression") == 0) {
                        char *endptr = NULL;

                        compression = strtol (value, &endptr, 10);
                        if (endptr == value || (compression < 0 || compression > 9)) {
                                g_set_error (error, GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_BAD_OPTION,
                                             _("PNG compression level must be a value between 0 and 9; value “%s” is invalid"),
                                             value);
                                success = FALSE;
                                goto cleanup;
                        }
                } else if (strcmp (key, "x-dpi") == 0 || strcmp (key, "y-dpi") == 0) {
                        gboolean is_horizontal = strcmp (key, "x-dpi") == 0;
                        char *endptr = NULL;

                        int dpi = strtol (value, &endptr, 10);

                        if (endptr == value || dpi <= 0) {
                                g_set_error (error, GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_BAD_OPTION,
                                             _("PNG %s must be greater than zero; value “%s” is not allowed"),
                                             is_horizontal ? "x-dpi" : "y-dpi",
                                             value);
                                success = FALSE;
                                goto cleanup;
                        }

                        if (is_horizontal) {
                                x_density = dpi;
                        } else {
                                y_density = dpi;
                        }
                } else {
                        g_warning ("Unrecognized parameter “%s” passed to the PNG saver", key);
                }
        }

        bpc = gdk_pixbuf_get_bits_per_sample (pixbuf);
        w = gdk_pixbuf_get_width (pixbuf);
        h = gdk_pixbuf_get_height (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);

        if (text_data->len > 0) {
                num_keys = text_data->len;
                text_ptr = (png_textp) g_array_free (text_data, FALSE);
                text_data = NULL;
        } else {
                g_clear_pointer (&text_data, g_array_unref);
                num_keys = 0;
                text_ptr = NULL;
        }

        /* Guaranteed by the caller. */
        g_assert (w >= 0);
        g_assert (h >= 0);
        g_assert (rowstride >= 0);

        png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,
                                           error,
                                           png_simple_error_callback,
                                           png_simple_warning_callback);
        if (png_ptr == NULL) {
	        success = FALSE;
	        goto cleanup;
        }

        info_ptr = png_create_info_struct (png_ptr);
        if (info_ptr == NULL) {
	        success = FALSE;
	        goto cleanup;
        }

        if (setjmp (png_jmpbuf (png_ptr))) {
	        success = FALSE;
	        goto cleanup;
        }

        if (num_keys > 0) {
                png_set_text (png_ptr, info_ptr, text_ptr, num_keys);
        }

        if (to_callback) {
                to_callback_ioptr.save_func = save_func;
                to_callback_ioptr.user_data = user_data;
                to_callback_ioptr.error = error;
                png_set_write_fn (png_ptr, &to_callback_ioptr,
                                  png_save_to_callback_write_func,
                                  png_save_to_callback_flush_func);
        } else {
                png_init_io (png_ptr, f);
        }

        if (compression >= 0) {
                png_set_compression_level (png_ptr, compression);
        }

#ifdef PNG_pHYs_SUPPORTED
        if (x_density > 0 && y_density > 0) {
                png_set_pHYs (png_ptr, info_ptr,
                              DPI_TO_DPM (x_density),
                              DPI_TO_DPM (y_density),
                              PNG_RESOLUTION_METER);
        }
#endif

#if defined(PNG_iCCP_SUPPORTED)
        /* the proper ICC profile title is encoded in the profile */
        if (icc_profile != NULL) {
                png_set_iCCP (png_ptr, info_ptr,
                              "ICC profile",
                              PNG_COMPRESSION_TYPE_BASE,
                              (png_bytep) icc_profile,
                              icc_profile_size);
        }
#endif

        if (has_alpha) {
                png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
                              PNG_COLOR_TYPE_RGB_ALPHA,
                              PNG_INTERLACE_NONE,
                              PNG_COMPRESSION_TYPE_BASE,
                              PNG_FILTER_TYPE_BASE);
        } else {
                png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
                              PNG_COLOR_TYPE_RGB,
                              PNG_INTERLACE_NONE,
                              PNG_COMPRESSION_TYPE_BASE,
                              PNG_FILTER_TYPE_BASE);
        }

        /* Note bpc is always 8 */
        sig_bit.red = bpc;
        sig_bit.green = bpc;
        sig_bit.blue = bpc;
        sig_bit.alpha = bpc;
        png_set_sBIT (png_ptr, info_ptr, &sig_bit);
        png_write_info (png_ptr, info_ptr);
        png_set_packing (png_ptr);

        for (y = 0, ptr = pixels; y < h; y++, ptr += rowstride) {
                row_ptr = (png_bytep)ptr;
                png_write_rows (png_ptr, &row_ptr, 1);
        }

        png_write_end (png_ptr, info_ptr);

        for (int i = 0; i < num_keys; i++) {
                g_free (text_ptr[i].text);
        }

        g_free (text_ptr);

cleanup:
        if (png_ptr != NULL) {
                png_destroy_write_struct (&png_ptr, &info_ptr);
        }

        if (text_data != NULL) {
                for (guint i = 0; i < text_data->len; i++) {
                        png_textp text = &g_array_index (text_data, png_text, i);

                        g_free (text->text);
                }

                g_array_unref (text_data);
        }

        g_free (icc_profile);

        return success;
}

static gboolean
gdk_pixbuf__png_image_save (FILE          *f, 
                            GdkPixbuf     *pixbuf, 
                            gchar        **keys,
                            gchar        **values,
                            GError       **error)
{
        int n_keys = keys != NULL ? g_strv_length (keys) : 0;

        return real_save_png (pixbuf, n_keys, keys, values, error,
                              FALSE, f, NULL, NULL);
}

static gboolean
gdk_pixbuf__png_image_save_to_callback (GdkPixbufSaveFunc   save_func,
                                        gpointer            user_data,
                                        GdkPixbuf          *pixbuf, 
                                        gchar             **keys,
                                        gchar             **values,
                                        GError            **error)
{
        int n_keys = keys != NULL ? g_strv_length (keys) : 0;

        return real_save_png (pixbuf, n_keys, keys, values, error,
                              TRUE, NULL, save_func, user_data);
}

#ifndef INCLUDE_png
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__png_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
        module->load = gdk_pixbuf__png_image_load;
        module->begin_load = gdk_pixbuf__png_image_begin_load;
        module->stop_load = gdk_pixbuf__png_image_stop_load;
        module->load_increment = gdk_pixbuf__png_image_load_increment;
        module->save = gdk_pixbuf__png_image_save;
        module->save_to_callback = gdk_pixbuf__png_image_save_to_callback;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
        static const GdkPixbufModulePattern signature[] = {
                { "\x89PNG\r\n\x1a\x0a", NULL, 100 },
                { NULL, NULL, 0 }
        };
	static const gchar *mime_types[] = {
		"image/png",
		NULL
	};
	static const gchar *extensions[] = {
		"png",
		NULL
	};

	info->name = "png";
        info->signature = (GdkPixbufModulePattern *) signature;
	info->description = N_("The PNG image format");
	info->mime_types = (gchar **) mime_types;
	info->extensions = (gchar **) extensions;
	info->flags = GDK_PIXBUF_FORMAT_WRITABLE | GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = "LGPL";
}
