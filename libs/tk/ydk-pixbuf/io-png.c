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
#include "gdk-pixbuf-private.h"



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
        guint32 retval;
        gint compression_type;

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
                if (error && *error == NULL) {
                        g_set_error_literal (error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                             _("Insufficient memory to load PNG file"));
                }
                

		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	rows = g_new (png_bytep, h);

	for (i = 0; i < h; i++)
		rows[i] = pixbuf->pixels + i * pixbuf->rowstride;

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
        GdkPixbufModulePreparedFunc prepare_func;
        GdkPixbufModuleUpdatedFunc update_func;
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
                                  GdkPixbufModulePreparedFunc prepare_func,
				  GdkPixbufModuleUpdatedFunc update_func,
				  gpointer user_data,
                                  GError **error)
{
        LoadContext* lc;
        
        lc = g_new0(LoadContext, 1);
        
        lc->fatal_error_occurred = FALSE;

        lc->size_func = size_func;
        lc->prepare_func = prepare_func;
        lc->update_func = update_func;
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
                /* error callback should have set the error */
                return NULL;
        }
        
	if (setjmp (png_jmpbuf(lc->png_read_ptr))) {
		if (lc->png_info_ptr)
			png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
                g_free(lc);
                /* error callback should have set the error */
                return NULL;
	}

        /* Create the auxiliary context struct */

        lc->png_info_ptr = png_create_info_struct(lc->png_read_ptr);

        if (lc->png_info_ptr == NULL) {
                png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
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

        g_return_val_if_fail(lc != NULL, TRUE);

        /* FIXME this thing needs to report errors if
         * we have unused image data
         */
        
        if (lc->pixbuf)
                g_object_unref (lc->pixbuf);
        
        png_destroy_read_struct(&lc->png_read_ptr, &lc->png_info_ptr, NULL);
        g_free(lc);

        return TRUE;
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
                if (lc->first_row_seen_in_chunk >= 0 && lc->update_func) {
                        /* We saw at least one row */
                        gint pass_diff = lc->last_pass_seen_in_chunk - lc->first_pass_seen_in_chunk;
                        
                        g_assert(pass_diff >= 0);
                        
                        if (pass_diff == 0) {
                                /* start and end row were in the same pass */
                                (lc->update_func)(lc->pixbuf, 0,
                                                  lc->first_row_seen_in_chunk,
                                                  lc->pixbuf->width,
                                                  (lc->last_row_seen_in_chunk -
                                                   lc->first_row_seen_in_chunk) + 1,
						  lc->notify_user_data);
                        } else if (pass_diff == 1) {
                                /* We have from the first row seen to
                                   the end of the image (max row
                                   seen), then from the top of the
                                   image to the last row seen */
                                /* first row to end */
                                (lc->update_func)(lc->pixbuf, 0,
                                                  lc->first_row_seen_in_chunk,
                                                  lc->pixbuf->width,
                                                  (lc->max_row_seen_in_chunk -
                                                   lc->first_row_seen_in_chunk) + 1,
						  lc->notify_user_data);
                                /* top to last row */
                                (lc->update_func)(lc->pixbuf,
                                                  0, 0, 
                                                  lc->pixbuf->width,
                                                  lc->last_row_seen_in_chunk + 1,
						  lc->notify_user_data);
                        } else {
                                /* We made at least one entire pass, so update the
                                   whole image */
                                (lc->update_func)(lc->pixbuf,
                                                  0, 0, 
                                                  lc->pixbuf->width,
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
        
        if (lc->size_func) {
                gint w = width;
                gint h = height;
                (* lc->size_func) (&w, &h, lc->notify_user_data);
                
                if (w == 0 || h == 0) {
                        lc->fatal_error_occurred = TRUE;
                        if (lc->error && *lc->error == NULL) {
                                g_set_error_literal (lc->error,
                                                     GDK_PIXBUF_ERROR,
                                                     GDK_PIXBUF_ERROR_FAILED,
                                                     _("Transformed PNG has zero width or height."));
                        }
                        return;
                }
        }

        lc->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, have_alpha, 8, width, height);

        if (lc->pixbuf == NULL) {
                /* Failed to allocate memory */
                lc->fatal_error_occurred = TRUE;
                if (lc->error && *lc->error == NULL) {
                        g_set_error (lc->error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Insufficient memory to store a %lu by %lu image; try exiting some applications to reduce memory usage"),
                                     (gulong) width, (gulong) height);
                }
                return;
        }

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

        /* Notify the client that we are ready to go */

        if (lc->prepare_func)
                (* lc->prepare_func) (lc->pixbuf, NULL, lc->notify_user_data);

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

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;

        if (row_num >= lc->pixbuf->height) {
                lc->fatal_error_occurred = TRUE;
                if (lc->error && *lc->error == NULL) {
                        g_set_error_literal (lc->error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                             _("Fatal error reading PNG image file"));
                }
                return;
        }

        if (lc->first_row_seen_in_chunk < 0) {
                lc->first_row_seen_in_chunk = row_num;
                lc->first_pass_seen_in_chunk = pass_num;
        }

        lc->max_row_seen_in_chunk = MAX(lc->max_row_seen_in_chunk, ((gint)row_num));
        lc->last_row_seen_in_chunk = row_num;
        lc->last_pass_seen_in_chunk = pass_num;
        
        old_row = lc->pixbuf->pixels + (row_num * lc->pixbuf->rowstride);

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

static gboolean real_save_png (GdkPixbuf        *pixbuf, 
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
       png_textp text_ptr = NULL;
       guchar *ptr;
       guchar *pixels;
       int y;
       int i;
       png_bytep row_ptr;
       png_color_8 sig_bit;
       int w, h, rowstride;
       int has_alpha;
       int bpc;
       int num_keys;
       int compression = -1;
       gboolean success = TRUE;
       guchar *icc_profile = NULL;
       gsize icc_profile_size = 0;
       SaveToFunctionIoPtr to_callback_ioptr;

       num_keys = 0;

       if (keys && *keys) {
               gchar **kiter = keys;
               gchar **viter = values;

               while (*kiter) {
                       if (strncmp (*kiter, "tEXt::", 6) == 0) {
                               gchar  *key = *kiter + 6;
                               int     len = strlen (key);
                               if (len < 1 || len > 79) {
                                       g_set_error_literal (error,
                                                            GDK_PIXBUF_ERROR,
                                                            GDK_PIXBUF_ERROR_BAD_OPTION,
                                                            _("Keys for PNG text chunks must have at least 1 and at most 79 characters."));
                                       success = FALSE;
                                       goto cleanup;
                               }
                               for (i = 0; i < len; i++) {
                                       if ((guchar) key[i] > 127) {
                                               g_set_error_literal (error,
                                                                    GDK_PIXBUF_ERROR,
                                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                                    _("Keys for PNG text chunks must be ASCII characters."));
                                               success = FALSE;
                                               goto cleanup;
                                       }
                               }
                               num_keys++;
                       } else if (strcmp (*kiter, "icc-profile") == 0) {
                               /* decode from base64 */
                               icc_profile = g_base64_decode (*viter, &icc_profile_size);
                               if (icc_profile_size < 127) {
                                       /* This is a user-visible error */
                                       g_set_error (error,
                                                    GDK_PIXBUF_ERROR,
                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                    _("Color profile has invalid length %d."),
                                                    (gint)icc_profile_size);
                                       success = FALSE;
                                       goto cleanup;
                               }
                       } else if (strcmp (*kiter, "compression") == 0) {
                               char *endptr = NULL;
                               compression = strtol (*viter, &endptr, 10);

                               if (endptr == *viter) {
                                       g_set_error (error,
                                                    GDK_PIXBUF_ERROR,
                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                    _("PNG compression level must be a value between 0 and 9; value '%s' could not be parsed."),
                                                    *viter);
                                       success = FALSE;
                                       goto cleanup;
                               }
                               if (compression < 0 || compression > 9) {
                                       /* This is a user-visible error;
                                        * lets people skip the range-checking
                                        * in their app.
                                        */
                                       g_set_error (error,
                                                    GDK_PIXBUF_ERROR,
                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                    _("PNG compression level must be a value between 0 and 9; value '%d' is not allowed."),
                                                    compression);
                                       success = FALSE;
                                       goto cleanup;
                               }
                       } else {
                               g_warning ("Unrecognized parameter (%s) passed to PNG saver.", *kiter);
                       }

                       ++kiter;
                       ++viter;
               }
       }

       if (num_keys > 0) {
               gchar **kiter = keys;
               gchar **viter = values;

               text_ptr = g_new0 (png_text, num_keys);
               for (i = 0; i < num_keys; i++) {
                       if (strncmp (*kiter, "tEXt::", 6) != 0) {
                                kiter++;
                                viter++;
                       }

                       text_ptr[i].compression = PNG_TEXT_COMPRESSION_NONE;
                       text_ptr[i].key  = *kiter + 6;
                       text_ptr[i].text = g_convert (*viter, -1, 
                                                     "ISO-8859-1", "UTF-8", 
                                                     NULL, &text_ptr[i].text_length, 
                                                     NULL);

#ifdef PNG_iTXt_SUPPORTED 
                       if (!text_ptr[i].text) {
                               text_ptr[i].compression = PNG_ITXT_COMPRESSION_NONE;
                               text_ptr[i].text = g_strdup (*viter);
                               text_ptr[i].text_length = 0;
                               text_ptr[i].itxt_length = strlen (text_ptr[i].text);
                               text_ptr[i].lang = NULL;
                               text_ptr[i].lang_key = NULL;
                       }
#endif

                       if (!text_ptr[i].text) {
                               gint j;
                               g_set_error (error,
                                            GDK_PIXBUF_ERROR,
                                            GDK_PIXBUF_ERROR_BAD_OPTION,
                                            _("Value for PNG text chunk %s cannot be converted to ISO-8859-1 encoding."), *kiter + 6);
                               for (j = 0; j < i; j++)
                                       g_free (text_ptr[j].text);
                               g_free (text_ptr);
                               return FALSE;
                       }

                        kiter++;
                        viter++;
               }
       }

       bpc = gdk_pixbuf_get_bits_per_sample (pixbuf);
       w = gdk_pixbuf_get_width (pixbuf);
       h = gdk_pixbuf_get_height (pixbuf);
       rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
       pixels = gdk_pixbuf_get_pixels (pixbuf);

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
       if (setjmp (png_jmpbuf(png_ptr))) {
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

       if (compression >= 0)
               png_set_compression_level (png_ptr, compression);

#if defined(PNG_iCCP_SUPPORTED)
        /* the proper ICC profile title is encoded in the profile */
        if (icc_profile != NULL) {
                png_set_iCCP (png_ptr, info_ptr,
                              "ICC profile", PNG_COMPRESSION_TYPE_BASE,
                              (png_bytep) icc_profile, icc_profile_size);
        }
#endif

       if (has_alpha) {
               png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
                             PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
       } else {
               png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
                             PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
       }
       sig_bit.red = bpc;
       sig_bit.green = bpc;
       sig_bit.blue = bpc;
       sig_bit.alpha = bpc;
       png_set_sBIT (png_ptr, info_ptr, &sig_bit);
       png_write_info (png_ptr, info_ptr);
       png_set_shift (png_ptr, &sig_bit);
       png_set_packing (png_ptr);

       ptr = pixels;
       for (y = 0; y < h; y++) {
               row_ptr = (png_bytep)ptr;
               png_write_rows (png_ptr, &row_ptr, 1);
               ptr += rowstride;
       }

       png_write_end (png_ptr, info_ptr);

cleanup:
        if (png_ptr != NULL)
                png_destroy_write_struct (&png_ptr, &info_ptr);

        g_free (icc_profile);

        if (text_ptr != NULL) {
                for (i = 0; i < num_keys; i++)
                        g_free (text_ptr[i].text);
                g_free (text_ptr);
        }

       return success;
}

static gboolean
gdk_pixbuf__png_image_save (FILE          *f, 
                            GdkPixbuf     *pixbuf, 
                            gchar        **keys,
                            gchar        **values,
                            GError       **error)
{
        return real_save_png (pixbuf, keys, values, error,
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
        return real_save_png (pixbuf, keys, values, error,
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
