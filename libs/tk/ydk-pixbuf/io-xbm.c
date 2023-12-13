/* -*- mode: C; c-file-style: "linux" -*- */
/* GdkPixbuf library - XBM image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 * Copyright (C) 2001 Eazel, Inc.
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
 *	    John Harper <jsh@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/* Following code adapted from io-tiff.c, which was ``(almost) blatantly
   ripped from Imlib'' */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <errno.h>
#include "gdk-pixbuf-private.h"
#include <glib/gstdio.h>




typedef struct _XBMData XBMData;
struct _XBMData
{
	GdkPixbufModulePreparedFunc prepare_func;
	GdkPixbufModuleUpdatedFunc update_func;
	gpointer user_data;

	gchar *tempname;
	FILE *file;
	gboolean all_okay;
};


/* xbm parser borrowed from xc/lib/X11/RdBitF.c */

#define MAX_SIZE 255

/* shared data for the image read/parse logic */
static short hex_table[256];		/* conversion value */
static gboolean initialized = FALSE;	/* easier to fill in at run time */


/* Table index for the hex values. Initialized once, first time.
 * Used for translation value or delimiter significance lookup.
 */
static void
init_hex_table (void)
{
	/*
	 * We build the table at run time for several reasons:
	 *
	 * 1. portable to non-ASCII machines.
	 * 2. still reentrant since we set the init flag after setting table.
	 * 3. easier to extend.
	 * 4. less prone to bugs.
	 */
	hex_table['0'] = 0;
	hex_table['1'] = 1;
	hex_table['2'] = 2;
	hex_table['3'] = 3;
	hex_table['4'] = 4;
	hex_table['5'] = 5;
	hex_table['6'] = 6;
	hex_table['7'] = 7;
	hex_table['8'] = 8;
	hex_table['9'] = 9;
	hex_table['A'] = 10;
	hex_table['B'] = 11;
	hex_table['C'] = 12;
	hex_table['D'] = 13;
	hex_table['E'] = 14;
	hex_table['F'] = 15;
	hex_table['a'] = 10;
	hex_table['b'] = 11;
	hex_table['c'] = 12;
	hex_table['d'] = 13;
	hex_table['e'] = 14;
	hex_table['f'] = 15;

	/* delimiters of significance are flagged w/ negative value */
	hex_table[' '] = -1;
	hex_table[','] = -1;
	hex_table['}'] = -1;
	hex_table['\n'] = -1;
	hex_table['\t'] = -1;

	initialized = TRUE;
}

/* Read next hex value in the input stream, return -1 if EOF */
static int
next_int (FILE *fstream)
{
	int ch;
	int value = 0;
	int gotone = 0;
	int done = 0;
    
	/* loop, accumulate hex value until find delimiter 
	   skip any initial delimiters found in read stream */

	while (!done) {
		ch = getc (fstream);
		if (ch == EOF) {
			value = -1;
			done++;
		} else {
			/* trim high bits, check type and accumulate */
			ch &= 0xff;
			if (g_ascii_isxdigit (ch)) {
				value = (value << 4) + g_ascii_xdigit_value (ch);
				gotone++;
			} else if ((hex_table[ch]) < 0 && gotone) {
				done++;
			}
		}
	}
	return value;
}

static gboolean
read_bitmap_file_data (FILE    *fstream,
		       guint   *width, 
		       guint   *height,
		       guchar **data,
		       int     *x_hot, 
		       int     *y_hot)
{
	guchar *bits = NULL;		/* working variable */
	char line[MAX_SIZE];		/* input line from file */
	int size;			/* number of bytes of data */
	char name_and_type[MAX_SIZE];	/* an input line */
	char *type;			/* for parsing */
	int value;			/* from an input line */
	int version10p;			/* boolean, old format */
	int padding;			/* to handle alignment */
	int bytes_per_line;		/* per scanline of data */
	guint ww = 0;			/* width */
	guint hh = 0;			/* height */
	int hx = -1;			/* x hotspot */
	int hy = -1;			/* y hotspot */

	/* first time initialization */
	if (!initialized) {
		init_hex_table ();
	}

	/* error cleanup and return macro */
#define	RETURN(code) { g_free (bits); return code; }

	while (fgets (line, MAX_SIZE, fstream)) {
		if (strlen (line) == MAX_SIZE-1)
			RETURN (FALSE);
		if (sscanf (line,"#define %s %d",name_and_type,&value) == 2) {
			if (!(type = strrchr (name_and_type, '_')))
				type = name_and_type;
			else {
				type++;
			}

			if (!strcmp ("width", type)) {
                                if (value <= 0)
                                        RETURN (FALSE);
				ww = (unsigned int) value;
                        }
			if (!strcmp ("height", type)) {
                                if (value <= 0)
                                        RETURN (FALSE);
				hh = (unsigned int) value;
                        }
			if (!strcmp ("hot", type)) {
				if (type-- == name_and_type
				    || type-- == name_and_type)
					continue;
				if (!strcmp ("x_hot", type))
					hx = value;
				if (!strcmp ("y_hot", type))
					hy = value;
			}
			continue;
		}
    
		if (sscanf (line, "static short %s = {", name_and_type) == 1)
			version10p = 1;
		else if (sscanf (line,"static const unsigned char %s = {",name_and_type) == 1)
			version10p = 0;
		else if (sscanf (line,"static unsigned char %s = {",name_and_type) == 1)
			version10p = 0;
		else if (sscanf (line, "static const char %s = {", name_and_type) == 1)
			version10p = 0;
		else if (sscanf (line, "static char %s = {", name_and_type) == 1)
			version10p = 0;
		else
			continue;

		if (!(type = strrchr (name_and_type, '_')))
			type = name_and_type;
		else
			type++;

		if (strcmp ("bits[]", type))
			continue;
    
		if (!ww || !hh)
			RETURN (FALSE);

		if ((ww % 16) && ((ww % 16) < 9) && version10p)
			padding = 1;
		else
			padding = 0;

		bytes_per_line = (ww+7)/8 + padding;

		size = bytes_per_line * hh;
                if (size / bytes_per_line != hh) /* overflow */
                        RETURN (FALSE);
		bits = g_malloc (size);

		if (version10p) {
			unsigned char *ptr;
			int bytes;

			for (bytes = 0, ptr = bits; bytes < size; (bytes += 2)) {
				if ((value = next_int (fstream)) < 0)
					RETURN (FALSE);
				*(ptr++) = value;
				if (!padding || ((bytes+2) % bytes_per_line))
					*(ptr++) = value >> 8;
			}
		} else {
			unsigned char *ptr;
			int bytes;

			for (bytes = 0, ptr = bits; bytes < size; bytes++, ptr++) {
				if ((value = next_int (fstream)) < 0) 
					RETURN (FALSE);
				*ptr=value;
			}
		}
		break;
	}

	if (!bits)
		RETURN (FALSE);

	*data = bits;
	*width = ww;
	*height = hh;
	if (x_hot)
		*x_hot = hx;
	if (y_hot)
		*y_hot = hy;

	return TRUE;
}



static GdkPixbuf *
gdk_pixbuf__xbm_image_load_real (FILE     *f, 
				 XBMData  *context, 
				 GError  **error)
{
	guint w, h;
	int x_hot, y_hot;
	guchar *data, *ptr;
	guchar *pixels;
	guint row_stride;
	int x, y;
	int reg = 0; /* Quiet compiler */
	int bits;

	GdkPixbuf *pixbuf;

	if (!read_bitmap_file_data (f, &w, &h, &data, &x_hot, &y_hot)) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                     _("Invalid XBM file"));
		return NULL;
	}

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, w, h);

        if (pixbuf == NULL) {
                g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Insufficient memory to load XBM image file"));
                return NULL;
        }
        
	if (x_hot != -1 && y_hot != -1) {
		gchar hot[10];
		g_snprintf (hot, 10, "%d", x_hot);
		gdk_pixbuf_set_option (pixbuf, "x_hot", hot);
		g_snprintf (hot, 10, "%d", y_hot);
		gdk_pixbuf_set_option (pixbuf, "y_hot", hot);
	}

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);

	if (context && context->prepare_func)
		(* context->prepare_func) (pixbuf, NULL, context->user_data);


	/* Initialize PIXBUF */

	ptr = data;
	for (y = 0; y < h; y++) {
		bits = 0;
		for (x = 0; x < w; x++) {
			guchar channel;
			if (bits == 0) {
				reg = *ptr++;
				bits = 8;
			}

			channel = (reg & 1) ? 0 : 255;
			reg >>= 1;
			bits--;

			pixels[x*3+0] = channel;
			pixels[x*3+1] = channel;
			pixels[x*3+2] = channel;
		}
		pixels += row_stride;
	}
	g_free (data);

	if (context) {
		if (context->update_func)
			(* context->update_func) (pixbuf, 0, 0, w, h, context->user_data);
	}

	return pixbuf;
}


/* Static loader */

static GdkPixbuf *
gdk_pixbuf__xbm_image_load (FILE    *f, 
			    GError **error)
{
	return gdk_pixbuf__xbm_image_load_real (f, NULL, error);
}


/* Progressive loader */

/*
 * Proper XBM progressive loading isn't implemented.  Instead we write
 * it to a file, then load the file when it's done.  It's not pretty.
 */

static gpointer
gdk_pixbuf__xbm_image_begin_load (GdkPixbufModuleSizeFunc       size_func,
                                  GdkPixbufModulePreparedFunc   prepare_func,
				  GdkPixbufModuleUpdatedFunc    update_func,
				  gpointer                      user_data,
				  GError                      **error)
{
	XBMData *context;
	gint fd;

	context = g_new (XBMData, 1);
	context->prepare_func = prepare_func;
	context->update_func = update_func;
	context->user_data = user_data;
	context->all_okay = TRUE;
	fd = g_file_open_tmp ("gdkpixbuf-xbm-tmp.XXXXXX",
			      &context->tempname,
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
gdk_pixbuf__xbm_image_stop_load (gpointer   data,
                                 GError   **error)
{
	XBMData *context = (XBMData*) data;
        gboolean retval = TRUE;

	g_return_val_if_fail (data != NULL, TRUE);

	fflush (context->file);
	rewind (context->file);
	if (context->all_okay) {
                GdkPixbuf *pixbuf;
                pixbuf = gdk_pixbuf__xbm_image_load_real (context->file, 
							  context,
                                                          error);
                if (pixbuf == NULL)
                        retval = FALSE;
		else
			g_object_unref (pixbuf);
        }

	fclose (context->file);
	g_unlink (context->tempname);
	g_free (context->tempname);
	g_free ((XBMData *) context);

        return retval;
}

static gboolean
gdk_pixbuf__xbm_image_load_increment (gpointer       data,
                                      const guchar  *buf,
                                      guint          size,
                                      GError       **error)
{
	XBMData *context = (XBMData *) data;

	g_return_val_if_fail (data != NULL, FALSE);

	if (fwrite (buf, sizeof (guchar), size, context->file) != size) {
		gint save_errno = errno;
		context->all_okay = FALSE;
                g_set_error_literal (error,
                                     G_FILE_ERROR,
                                     g_file_error_from_errno (save_errno),
                                     _("Failed to write to temporary file when loading XBM image"));
		return FALSE;
	}

	return TRUE;
}

#ifndef INCLUDE_xbm
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__xbm_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
	module->load = gdk_pixbuf__xbm_image_load;
	module->begin_load = gdk_pixbuf__xbm_image_begin_load;
	module->stop_load = gdk_pixbuf__xbm_image_stop_load;
	module->load_increment = gdk_pixbuf__xbm_image_load_increment;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
	static const GdkPixbufModulePattern signature[] = {
		{ "#define ", NULL, 100 },
		{ "/*", NULL, 50 },
		{ NULL, NULL, 0 }
	};
	static const gchar *mime_types[] = {
		"image/x-xbitmap",
		NULL
	};
	static const gchar *extensions[] = {
		"xbm",
		NULL
	};

	info->name = "xbm";
	info->signature = (GdkPixbufModulePattern *) signature;
	info->description = N_("The XBM image format");
	info->mime_types = (gchar **) mime_types;
	info->extensions = (gchar **) extensions;
	info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = "LGPL";
}
