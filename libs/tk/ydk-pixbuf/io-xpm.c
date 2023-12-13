/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GdkPixbuf library - XPM image loader
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
#include <glib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for unlink */
#endif
#include <errno.h>
#include "gdk-pixbuf-private.h"
#include <glib/gstdio.h>




/* I have must have done something to deserve this.
 * XPM is such a crappy format to handle.
 * This code is an ugly hybrid from gdkpixmap.c
 * modified to respect transparent colors.
 * It's still a mess, though.
 */

enum buf_op {
	op_header,
	op_cmap,
	op_body
};

typedef struct {
	gchar *color_string;
	guint16 red;
	guint16 green;
	guint16 blue;
	gint transparent;
} XPMColor;

struct file_handle {
	FILE *infile;
	gchar *buffer;
	guint buffer_size;
};

struct mem_handle {
	const gchar **data;
	int offset;
};

/* The following 2 routines (parse_color, find_color) come from Tk, via the Win32
 * port of GDK. The licensing terms on these (longer than the functions) is:
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The following
 * terms apply to all files associated with the software unless explicitly
 * disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 * 
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal 
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense, the
 * software shall be classified as "Commercial Computer Software" and the
 * Government shall have only "Restricted Rights" as defined in Clause
 * 252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
 * authors grant the U.S. Government and others acting in its behalf
 * permission to use and distribute the software in accordance with the
 * terms specified in this license.
 */

#include "xpm-color-table.h"
 
/*
 *----------------------------------------------------------------------
 *
 * find_color --
 *
 *	This routine finds the color entry that corresponds to the
 *	specified color.
 *
 * Results:
 *	Returns non-zero on success.  The RGB values of the XColor
 *	will be initialized to the proper values on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
compare_xcolor_entries (const void *a, const void *b)
{
  return g_ascii_strcasecmp ((const char *) a, 
			     color_names + ((const XPMColorEntry *)b)->name_offset);
}

static gboolean
find_color(const char *name,
	   XPMColor   *colorPtr)
{
	XPMColorEntry *found;

	found = bsearch (name, xColors, G_N_ELEMENTS (xColors), sizeof (XPMColorEntry),
			 compare_xcolor_entries);
	if (found == NULL)
	  return FALSE;
	
	colorPtr->red = (found->red * 65535) / 255;
	colorPtr->green = (found->green * 65535) / 255;
	colorPtr->blue = (found->blue * 65535) / 255;
	
	return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * parse_color --
 *
 *	Partial implementation of X color name parsing interface.
 *
 * Results:
 *	Returns TRUE on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
parse_color (const char *spec,
	     XPMColor   *colorPtr)
{
	if (spec[0] == '#') {
		char fmt[16];
		int i, red, green, blue;

		if ((i = strlen (spec + 1)) % 3) {
			return FALSE;
		}
		i /= 3;

		g_snprintf (fmt, 16, "%%%dx%%%dx%%%dx", i, i, i);

		if (sscanf (spec + 1, fmt, &red, &green, &blue) != 3) {
			return FALSE;
		}
		if (i == 4) {
			colorPtr->red = red;
			colorPtr->green = green;
			colorPtr->blue = blue;
		} else if (i == 1) {
			colorPtr->red = (red * 65535) / 15;
			colorPtr->green = (green * 65535) / 15;
			colorPtr->blue = (blue * 65535) / 15;
		} else if (i == 2)
		{
			colorPtr->red = (red * 65535) / 255;
			colorPtr->green = (green * 65535) / 255;
			colorPtr->blue = (blue * 65535) / 255;
		} else /* if (i == 3) */ {
			colorPtr->red = (red * 65535) / 4095;
			colorPtr->green = (green * 65535) / 4095;
			colorPtr->blue = (blue * 65535) / 4095;
		}
	} else {
		if (!find_color(spec, colorPtr))
			return FALSE;
	}
	return TRUE;
}

static gint
xpm_seek_string (FILE *infile, const gchar *str)
{
	char instr[1024];

	while (!feof (infile)) {
		if (fscanf (infile, "%1023s", instr) < 0)
                        return FALSE;
		if (strcmp (instr, str) == 0)
			return TRUE;
	}

	return FALSE;
}

static gint
xpm_seek_char (FILE *infile, gchar c)
{
	gint b, oldb;

	while ((b = getc (infile)) != EOF) {
		if (c != b && b == '/') {
			b = getc (infile);
			if (b == EOF)
				return FALSE;

			else if (b == '*') {	/* we have a comment */
				b = -1;
				do {
					oldb = b;
					b = getc (infile);
					if (b == EOF)
						return FALSE;
				} while (!(oldb == '*' && b == '/'));
			}
		} else if (c == b)
			return TRUE;
	}

	return FALSE;
}

static gint
xpm_read_string (FILE *infile, gchar **buffer, guint *buffer_size)
{
	gint c;
	guint cnt = 0, bufsiz, ret = FALSE;
	gchar *buf;

	buf = *buffer;
	bufsiz = *buffer_size;
	if (buf == NULL) {
		bufsiz = 10 * sizeof (gchar);
		buf = g_new (gchar, bufsiz);
	}

	do {
		c = getc (infile);
	} while (c != EOF && c != '"');

	if (c != '"')
		goto out;

	while ((c = getc (infile)) != EOF) {
		if (cnt == bufsiz) {
			guint new_size = bufsiz * 2;

			if (new_size > bufsiz)
				bufsiz = new_size;
			else
				goto out;

			buf = g_realloc (buf, bufsiz);
			buf[bufsiz - 1] = '\0';
		}

		if (c != '"')
			buf[cnt++] = c;
		else {
			buf[cnt] = 0;
			ret = TRUE;
			break;
		}
	}

 out:
	buf[bufsiz - 1] = '\0';	/* ensure null termination for errors */
	*buffer = buf;
	*buffer_size = bufsiz;
	return ret;
}

static gchar *
xpm_extract_color (const gchar *buffer)
{
	const gchar *p = &buffer[0];
	gint new_key = 0;
	gint key = 0;
	gint current_key = 1;
	gint space = 128;
	gchar word[129], color[129], current_color[129];
	gchar *r; 
	
	word[0] = '\0';
	color[0] = '\0';
	current_color[0] = '\0';
        while (1) {
		/* skip whitespace */
		for (; *p != '\0' && g_ascii_isspace (*p); p++) {
		} 
		/* copy word */
		for (r = word; *p != '\0' && !g_ascii_isspace (*p) && r - word < sizeof (word) - 1; p++, r++) {
			*r = *p;
		}
		*r = '\0';
		if (*word == '\0') {
			if (color[0] == '\0')  /* incomplete colormap entry */
				return NULL; 				
			else  /* end of entry, still store the last color */
				new_key = 1;
		} 
		else if (key > 0 && color[0] == '\0')  /* next word must be a color name part */
			new_key = 0;
		else {
			if (strcmp (word, "c") == 0)
				new_key = 5;
			else if (strcmp (word, "g") == 0)
				new_key = 4;
			else if (strcmp (word, "g4") == 0)
				new_key = 3;
			else if (strcmp (word, "m") == 0)
				new_key = 2;
			else if (strcmp (word, "s") == 0)
				new_key = 1;
			else 
				new_key = 0;
		}
		if (new_key == 0) {  /* word is a color name part */
			if (key == 0)  /* key expected */
				return NULL;
			/* accumulate color name */
			if (color[0] != '\0') {
				strncat (color, " ", space);
				space -= MIN (space, 1);
			}
			strncat (color, word, space);
			space -= MIN (space, strlen (word));
		}
		else {  /* word is a key */
			if (key > current_key) {
				current_key = key;
				strcpy (current_color, color);
			}
			space = 128;
			color[0] = '\0';
			key = new_key;
			if (*p == '\0') break;
		}
		
	}
	if (current_key > 1)
		return g_strdup (current_color);
	else
		return NULL; 
}

/* (almost) direct copy from gdkpixmap.c... loads an XPM from a file */

static const gchar *
file_buffer (enum buf_op op, gpointer handle)
{
	struct file_handle *h = handle;

	switch (op) {
	case op_header:
		if (xpm_seek_string (h->infile, "XPM") != TRUE)
			break;

		if (xpm_seek_char (h->infile, '{') != TRUE)
			break;
		/* Fall through to the next xpm_seek_char. */

	case op_cmap:
		xpm_seek_char (h->infile, '"');
		fseek (h->infile, -1, SEEK_CUR);
		/* Fall through to the xpm_read_string. */

	case op_body:
		if(!xpm_read_string (h->infile, &h->buffer, &h->buffer_size))
			return NULL;
		return h->buffer;

	default:
		g_assert_not_reached ();
	}

	return NULL;
}

/* This reads from memory */
static const gchar *
mem_buffer (enum buf_op op, gpointer handle)
{
	struct mem_handle *h = handle;
	switch (op) {
	case op_header:
	case op_cmap:
	case op_body:
                if (h->data[h->offset]) {
                        const gchar* retval;

                        retval = h->data[h->offset];
                        h->offset += 1;
                        return retval;
                }
                break;

	default:
		g_assert_not_reached ();
                break;
	}

	return NULL;
}

/* This function does all the work. */
static GdkPixbuf *
pixbuf_create_from_xpm (const gchar * (*get_buf) (enum buf_op op, gpointer handle), gpointer handle,
                        GError **error)
{
	gint w, h, n_col, cpp, x_hot, y_hot, items;
	gint cnt, xcnt, ycnt, wbytes, n;
	gint is_trans = FALSE;
	const gchar *buffer;
        gchar *name_buf;
	gchar pixel_str[32];
	GHashTable *color_hash;
	XPMColor *colors, *color, *fallbackcolor;
	guchar *pixtmp;
	GdkPixbuf *pixbuf;

	fallbackcolor = NULL;

	buffer = (*get_buf) (op_header, handle);
	if (!buffer) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("No XPM header found"));
		return NULL;
	}
	items = sscanf (buffer, "%d %d %d %d %d %d", &w, &h, &n_col, &cpp, &x_hot, &y_hot);

	if (items != 4 && items != 6) {
		g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Invalid XPM header"));
		return NULL;
	}

	if (w <= 0) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("XPM file has image width <= 0"));
		return NULL;

	}
	if (h <= 0) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("XPM file has image height <= 0"));
		return NULL;

	}
	if (cpp <= 0 || cpp >= 32) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("XPM has invalid number of chars per pixel"));
		return NULL;
	}
	if (n_col <= 0 || 
	    n_col >= G_MAXINT / (cpp + 1) || 
	    n_col >= G_MAXINT / sizeof (XPMColor)) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("XPM file has invalid number of colors"));
		return NULL;
	}

	/* The hash is used for fast lookups of color from chars */
	color_hash = g_hash_table_new (g_str_hash, g_str_equal);

	name_buf = g_try_malloc (n_col * (cpp + 1));
	if (!name_buf) {
		g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Cannot allocate memory for loading XPM image"));
		g_hash_table_destroy (color_hash);
		return NULL;
	}
	colors = (XPMColor *) g_try_malloc (sizeof (XPMColor) * n_col);
	if (!colors) {
		g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Cannot allocate memory for loading XPM image"));
		g_hash_table_destroy (color_hash);
		g_free (name_buf);
		return NULL;
	}

	for (cnt = 0; cnt < n_col; cnt++) {
		gchar *color_name;

		buffer = (*get_buf) (op_cmap, handle);
		if (!buffer) {
                        g_set_error_literal (error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                             _("Cannot read XPM colormap"));
			g_hash_table_destroy (color_hash);
			g_free (name_buf);
			g_free (colors);
			return NULL;
		}

		color = &colors[cnt];
		color->color_string = &name_buf[cnt * (cpp + 1)];
		strncpy (color->color_string, buffer, cpp);
		color->color_string[cpp] = 0;
		buffer += strlen (color->color_string);
		color->transparent = FALSE;

		color_name = xpm_extract_color (buffer);

		if ((color_name == NULL) || (g_ascii_strcasecmp (color_name, "None") == 0)
		    || (parse_color (color_name, color) == FALSE)) {
			color->transparent = TRUE;
			color->red = 0;
			color->green = 0;
			color->blue = 0;
			is_trans = TRUE;
		}

		g_free (color_name);
		g_hash_table_insert (color_hash, color->color_string, color);

		if (cnt == 0)
			fallbackcolor = color;
	}

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, is_trans, 8, w, h);

	if (!pixbuf) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Cannot allocate memory for loading XPM image"));
		g_hash_table_destroy (color_hash);
		g_free (colors);
		g_free (name_buf);
		return NULL;
	}

	wbytes = w * cpp;

	for (ycnt = 0; ycnt < h; ycnt++) {
		pixtmp = pixbuf->pixels + ycnt * pixbuf->rowstride;

		buffer = (*get_buf) (op_body, handle);
		if ((!buffer) || (strlen (buffer) < wbytes))
			continue;

		for (n = 0, xcnt = 0; n < wbytes; n += cpp, xcnt++) {
			strncpy (pixel_str, &buffer[n], cpp);
			pixel_str[cpp] = 0;

			color = g_hash_table_lookup (color_hash, pixel_str);

			/* Bad XPM...punt */
			if (!color)
				color = fallbackcolor;

			*pixtmp++ = color->red >> 8;
			*pixtmp++ = color->green >> 8;
			*pixtmp++ = color->blue >> 8;

			if (is_trans && color->transparent)
				*pixtmp++ = 0;
			else if (is_trans)
				*pixtmp++ = 0xFF;
		}
	}

	g_hash_table_destroy (color_hash);
	g_free (colors);
	g_free (name_buf);

	if (items == 6) {
		gchar hot[10];
		g_snprintf (hot, 10, "%d", x_hot);
		gdk_pixbuf_set_option (pixbuf, "x_hot", hot);
		g_snprintf (hot, 10, "%d", y_hot);
		gdk_pixbuf_set_option (pixbuf, "y_hot", hot);

	}

	return pixbuf;
}

/* Shared library entry point for file loading */
static GdkPixbuf *
gdk_pixbuf__xpm_image_load (FILE *f,
                            GError **error)
{
	GdkPixbuf *pixbuf;
	struct file_handle h;

	memset (&h, 0, sizeof (h));
	h.infile = f;
	pixbuf = pixbuf_create_from_xpm (file_buffer, &h, error);
	g_free (h.buffer);

	return pixbuf;
}

/* Shared library entry point for memory loading */
static GdkPixbuf *
gdk_pixbuf__xpm_image_load_xpm_data (const gchar **data)
{
        GdkPixbuf *pixbuf;
        struct mem_handle h;
        GError *error = NULL;
        
        h.data = data;
        h.offset = 0;
        
	pixbuf = pixbuf_create_from_xpm (mem_buffer, &h, &error);

        if (error) {
                g_warning ("Inline XPM data is broken: %s", error->message);
                g_error_free (error);
                error = NULL;
        }
        
	return pixbuf;
}

/* Progressive loader */
typedef struct _XPMContext XPMContext;
struct _XPMContext
{
       GdkPixbufModulePreparedFunc prepare_func;
       GdkPixbufModuleUpdatedFunc update_func;
       gpointer user_data;

       gchar *tempname;
       FILE *file;
       gboolean all_okay;
};

/*
 * FIXME xpm loading progressively is not properly implemented.
 * Instead we will buffer to a file then load that file when done.
 * This is very broken but it should be relatively simple to fix
 * in the future.
 */
static gpointer
gdk_pixbuf__xpm_image_begin_load (GdkPixbufModuleSizeFunc size_func,
                                  GdkPixbufModulePreparedFunc prepare_func,
                                  GdkPixbufModuleUpdatedFunc update_func,
                                  gpointer user_data,
                                  GError **error)
{
       XPMContext *context;
       gint fd;

       context = g_new (XPMContext, 1);
       context->prepare_func = prepare_func;
       context->update_func = update_func;
       context->user_data = user_data;
       context->all_okay = TRUE;
       fd = g_file_open_tmp ("gdkpixbuf-xpm-tmp.XXXXXX", &context->tempname,
			     NULL);
       if (fd < 0) {
               g_free (context);
               return NULL;
       }

       context->file = fdopen (fd, "w+");
       if (context->file == NULL) {
               g_free (context->tempname);
               g_free (context);
               return NULL;
       }

       return context;
}

static gboolean
gdk_pixbuf__xpm_image_stop_load (gpointer data,
                                 GError **error)
{
       XPMContext *context = (XPMContext*) data;
       GdkPixbuf *pixbuf;
       gboolean retval = FALSE;
       
       g_return_val_if_fail (data != NULL, FALSE);

       fflush (context->file);
       rewind (context->file);
       if (context->all_okay) {
               pixbuf = gdk_pixbuf__xpm_image_load (context->file, error);

               if (pixbuf != NULL) {
		       if (context->prepare_func)
			       (* context->prepare_func) (pixbuf,
							  NULL,
							  context->user_data);
		       if (context->update_func)
			       (* context->update_func) (pixbuf, 0, 0, pixbuf->width, pixbuf->height, context->user_data);
                       g_object_unref (pixbuf);

                       retval = TRUE;
               }
       }

       fclose (context->file);
       g_unlink (context->tempname);
       g_free (context->tempname);
       g_free ((XPMContext *) context);

       return retval;
}

static gboolean
gdk_pixbuf__xpm_image_load_increment (gpointer data,
                                      const guchar *buf,
                                      guint    size,
                                      GError **error)
{
       XPMContext *context = (XPMContext *) data;

       g_return_val_if_fail (data != NULL, FALSE);

       if (fwrite (buf, sizeof (guchar), size, context->file) != size) {
	       gint save_errno = errno;
               context->all_okay = FALSE;
               g_set_error_literal (error,
                                    G_FILE_ERROR,
                                    g_file_error_from_errno (save_errno),
                                    _("Failed to write to temporary file when loading XPM image"));
               return FALSE;
       }

       return TRUE;
}

#ifndef INCLUDE_xpm
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__xpm_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
	module->load = gdk_pixbuf__xpm_image_load;
	module->load_xpm_data = gdk_pixbuf__xpm_image_load_xpm_data;
	module->begin_load = gdk_pixbuf__xpm_image_begin_load;
	module->stop_load = gdk_pixbuf__xpm_image_stop_load;
	module->load_increment = gdk_pixbuf__xpm_image_load_increment;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
	static const GdkPixbufModulePattern signature[] = {
		{ "/* XPM */", NULL, 100 },
		{ NULL, NULL, 0 }
	};
	static const gchar *mime_types[] = {
		"image/x-xpixmap",
		NULL
	};
	static const gchar *extensions[] = {
		"xpm",
		NULL
	};

	info->name = "xpm";
	info->signature = (GdkPixbufModulePattern *) signature;
	info->description = N_("The XPM image format");
	info->mime_types = (gchar **) mime_types;
	info->extensions = (gchar **) extensions;
	info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = "LGPL";
}
