/* -*- mode: C; c-file-style: "linux" -*- */
/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Michael Zucchi
 * Copyright (C) 1999 The Free Software Foundation
 * 
 * Progressive loading code Copyright (C) 1999 Red Hat, Inc.
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Michael Fulbright <drmike@redhat.com>
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
#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>
#include <math.h>

#include "gdk-pixbuf-private.h"

#ifndef HAVE_SIGSETJMP
#define sigjmp_buf jmp_buf
#define sigsetjmp(jb, x) setjmp(jb)
#define siglongjmp longjmp
#endif


/* we are a "source manager" as far as libjpeg is concerned */
#define JPEG_PROG_BUF_SIZE 65536

typedef struct {
	struct jpeg_source_mgr pub;   /* public fields */

	JOCTET buffer[JPEG_PROG_BUF_SIZE];              /* start of buffer */
	long  skip_next;              /* number of bytes to skip next read */
	
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	sigjmp_buf setjmp_buffer;
        GError **error;
};

/* progressive loader context */
typedef struct {
        GdkPixbufModuleSizeFunc     size_func;
	GdkPixbufModuleUpdatedFunc  updated_func;
	GdkPixbufModulePreparedFunc prepared_func;
	gpointer                    user_data;
	
	GdkPixbuf                *pixbuf;
	guchar                   *dptr;   /* current position in pixbuf */

	gboolean                 did_prescan;  /* are we in image data yet? */
	gboolean                 got_header;  /* have we loaded jpeg header? */
	gboolean                 src_initialized;/* TRUE when jpeg lib initialized */
	gboolean                 in_output;   /* did we get suspended in an output pass? */
	struct jpeg_decompress_struct cinfo;
	struct error_handler_data     jerr;
} JpegProgContext;

/* EXIF context */
typedef struct {
	gint			 orientation;
	gchar			*icc_profile;
	gsize			 icc_profile_size;
	gsize			 icc_profile_size_allocated;
} JpegExifContext;

static GdkPixbuf *gdk_pixbuf__jpeg_image_load (FILE *f, GError **error);
static gpointer gdk_pixbuf__jpeg_image_begin_load (GdkPixbufModuleSizeFunc           func0,
                                                   GdkPixbufModulePreparedFunc func1, 
                                                   GdkPixbufModuleUpdatedFunc func2,
                                                   gpointer user_data,
                                                   GError **error);
static gboolean gdk_pixbuf__jpeg_image_stop_load (gpointer context, GError **error);
static gboolean gdk_pixbuf__jpeg_image_load_increment(gpointer context,
                                                      const guchar *buf, guint size,
                                                      GError **error);


static void
fatal_error_handler (j_common_ptr cinfo)
{
	struct error_handler_data *errmgr;
        char buffer[JMSG_LENGTH_MAX];
        
	errmgr = (struct error_handler_data *) cinfo->err;
        
        /* Create the message */
        (* cinfo->err->format_message) (cinfo, buffer);

        /* broken check for *error == NULL for robustness against
         * crappy JPEG library
         */
        if (errmgr->error && *errmgr->error == NULL) {
                g_set_error (errmgr->error,
                             GDK_PIXBUF_ERROR,
                             cinfo->err->msg_code == JERR_OUT_OF_MEMORY 
			     ? GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY 
			     : GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Error interpreting JPEG image file (%s)"),
                             buffer);
        }
        
	siglongjmp (errmgr->setjmp_buffer, 1);

        g_assert_not_reached ();
}

static void
output_message_handler (j_common_ptr cinfo)
{
  /* This method keeps libjpeg from dumping crap to stderr */

  /* do nothing */
}

/* explode gray image data from jpeg library into rgb components in pixbuf */
static void
explode_gray_into_buf (struct jpeg_decompress_struct *cinfo,
		       guchar **lines) 
{
	gint i, j;
	guint w;

	g_return_if_fail (cinfo != NULL);
	g_return_if_fail (cinfo->output_components == 1);
	g_return_if_fail (cinfo->out_color_space == JCS_GRAYSCALE);

	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	w = cinfo->output_width;
	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		guchar *from, *to;
		
		from = lines[i] + w - 1;
		to = lines[i] + (w - 1) * 3;
		for (j = w - 1; j >= 0; j--) {
			to[0] = from[0];
			to[1] = from[0];
			to[2] = from[0];
			to -= 3;
			from--;
		}
	}
}


static void
convert_cmyk_to_rgb (struct jpeg_decompress_struct *cinfo,
		     guchar **lines) 
{
	gint i, j;

	g_return_if_fail (cinfo != NULL);
	g_return_if_fail (cinfo->output_components == 4);
	g_return_if_fail (cinfo->out_color_space == JCS_CMYK);

	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		guchar *p;
		
		p = lines[i];
		for (j = 0; j < cinfo->output_width; j++) {
			int c, m, y, k;
			c = p[0];
			m = p[1];
			y = p[2];
			k = p[3];
			if (cinfo->saw_Adobe_marker) {
				p[0] = k*c / 255;
				p[1] = k*m / 255;
				p[2] = k*y / 255;
			}
			else {
				p[0] = (255 - k)*(255 - c) / 255;
				p[1] = (255 - k)*(255 - m) / 255;
				p[2] = (255 - k)*(255 - y) / 255;
			}
			p[3] = 255;
			p += 4;
		}
	}
}

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */

  FILE * infile;		/* source stream */
  JOCTET * buffer;		/* start of buffer */
  boolean start_of_file;	/* have we gotten any data yet? */
} stdio_source_mgr;

typedef stdio_source_mgr * stdio_src_ptr;

static void
stdio_init_source (j_decompress_ptr cinfo)
{
  stdio_src_ptr src = (stdio_src_ptr)cinfo->src;
  src->start_of_file = FALSE;
}

static boolean
stdio_fill_input_buffer (j_decompress_ptr cinfo)
{
  stdio_src_ptr src = (stdio_src_ptr) cinfo->src;
  size_t nbytes;

  nbytes = fread (src->buffer, 1, JPEG_PROG_BUF_SIZE, src->infile);

  if (nbytes <= 0) {
#if 0
    if (src->start_of_file)	/* Treat empty input file as fatal error */
      ERREXIT(cinfo, JERR_INPUT_EMPTY);
    WARNMS(cinfo, JWRN_JPEG_EOF);
#endif
    /* Insert a fake EOI marker */
    src->buffer[0] = (JOCTET) 0xFF;
    src->buffer[1] = (JOCTET) JPEG_EOI;
    nbytes = 2;
  }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;
  src->start_of_file = FALSE;

  return TRUE;
}

static void
stdio_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  stdio_src_ptr src = (stdio_src_ptr) cinfo->src;

  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void)stdio_fill_input_buffer(cinfo);
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
stdio_term_source (j_decompress_ptr cinfo)
{
}

static gchar *
colorspace_name (const J_COLOR_SPACE jpeg_color_space) 
{
	switch (jpeg_color_space) {
	    case JCS_UNKNOWN: return "UNKNOWN"; 
	    case JCS_GRAYSCALE: return "GRAYSCALE"; 
	    case JCS_RGB: return "RGB"; 
	    case JCS_YCbCr: return "YCbCr"; 
	    case JCS_CMYK: return "CMYK"; 
	    case JCS_YCCK: return "YCCK";
	    default: return "invalid";
	}
}

#define DE_ENDIAN16(val) endian == G_BIG_ENDIAN ? GUINT16_FROM_BE(val) : GUINT16_FROM_LE(val)
#define DE_ENDIAN32(val) endian == G_BIG_ENDIAN ? GUINT32_FROM_BE(val) : GUINT32_FROM_LE(val)

#define ENDIAN16_IT(val) endian == G_BIG_ENDIAN ? GUINT16_TO_BE(val) : GUINT16_TO_LE(val)
#define ENDIAN32_IT(val) endian == G_BIG_ENDIAN ? GUINT32_TO_BE(val) : GUINT32_TO_LE(val)

static unsigned short de_get16(void *ptr, guint endian)
{
       unsigned short val;

       memcpy(&val, ptr, sizeof(val));
       val = DE_ENDIAN16(val);

       return val;
}

static unsigned int de_get32(void *ptr, guint endian)
{
       unsigned int val;

       memcpy(&val, ptr, sizeof(val));
       val = DE_ENDIAN32(val);

       return val;
}

/* application specific data segment */
static gboolean
jpeg_parse_exif_app2_segment (JpegExifContext *context, jpeg_saved_marker_ptr marker)
{
	guint ret = FALSE;
	guint sequence_number;
	guint number_of_chunks;
	guint chunk_size;
	guint offset;

	/* do we have enough data? */
	if (marker->data_length < 16)
		goto out;

	/* unique identification string */
	if (memcmp (marker->data, "ICC_PROFILE\0", 12) != 0)
		goto out;

	/* get data about this segment */
	sequence_number = marker->data[12];
	number_of_chunks = marker->data[13];

	/* this is invalid, the base offset is 1 */
	if (sequence_number == 0)
		goto out;

	/* this is invalid, the base offset is 1 */
	if (sequence_number > number_of_chunks)
		goto out;

	/* size includes the id (12 bytes), length field (1 byte), and sequence field (1 byte) */
	chunk_size = marker->data_length - 14;
	offset = (sequence_number - 1) * 0xffef;

	/* Deal with the trivial profile (99% of images) to avoid allocating
	 * 64kb when we might only use a few kb. */
	if (number_of_chunks == 1) {
		if (context->icc_profile_size_allocated > 0)
			goto out;
		context->icc_profile_size = chunk_size;
		context->icc_profile_size_allocated = chunk_size;
		context->icc_profile = g_new (gchar, chunk_size);
		/* copy the segment data to the profile space */
		memcpy (context->icc_profile, marker->data + 14, chunk_size);
		goto out;
	}

	/* There is no promise the APP2 segments are going to be in order, so we
	 * have to allocate a huge swathe of memory and fill in the gaps when
	 * (if) we get the segment.
	 * Theoretically this could be as much as 16Mb, but display profiles are
	 * vary rarely above 100kb, and printer profiles are usually less than
	 * 2Mb */
	if (context->icc_profile_size_allocated == 0) {
		context->icc_profile_size_allocated = number_of_chunks * 0xffff;
		context->icc_profile = g_new0 (gchar, number_of_chunks * 0xffff);
	}

	/* check the data will fit in our previously allocated buffer */
	if (offset + chunk_size > context->icc_profile_size_allocated)
		goto out;

	/* copy the segment data to the profile space */
	memcpy (context->icc_profile + offset, marker->data + 14, chunk_size);

	/* it's now this big plus the new data we've just copied */
	context->icc_profile_size += chunk_size;

	/* success */
	ret = TRUE;
out:
	return ret;
}

static gboolean
jpeg_parse_exif_app1 (JpegExifContext *context, jpeg_saved_marker_ptr marker)
{
	guint i;
	guint ret = FALSE;
	guint offset;
	guint tags;	   /* number of tags in current ifd */
	guint endian = 0;	/* detected endian of data */
	const char leth[]  = {0x49, 0x49, 0x2a, 0x00};	// Little endian TIFF header
	const char beth[]  = {0x4d, 0x4d, 0x00, 0x2a};	// Big endian TIFF header

	/* do we have enough data? */
	if (marker->data_length < 4)
		goto out;

	/* unique identification string */
	if (memcmp (marker->data, "Exif", 4) != 0)
		goto out;

	/* do we have enough data? */
	if (marker->data_length < 32)
		goto out;

	/* Just skip data until TIFF header - it should be within 16 bytes from marker start.
	   Normal structure relative to APP1 marker -
		0x0000: APP1 marker entry = 2 bytes
		0x0002: APP1 length entry = 2 bytes
		0x0004: Exif Identifier entry = 6 bytes
		0x000A: Start of TIFF header (Byte order entry) - 4 bytes
			- This is what we look for, to determine endianess.
		0x000E: 0th IFD offset pointer - 4 bytes

		marker->data points to the first data after the APP1 marker
		and length entries, which is the exif identification string.
		The TIFF header should thus normally be found at i=6, below,
		and the pointer to IFD0 will be at 6+4 = 10.
	*/

	for (i=0; i<16; i++) {
		/* little endian TIFF header */
		if (memcmp (&marker->data[i], leth, 4) == 0) {
			endian = G_LITTLE_ENDIAN;
			ret = TRUE;
			break;
		}

		/* big endian TIFF header */
		if (memcmp (&marker->data[i], beth, 4) == 0) {
			endian = G_BIG_ENDIAN;
			ret = TRUE;
			break;
		}
	}

	/* could not find header */
	if (!ret)
		goto out;

	/* read out the offset pointer to IFD0 */
	offset  = de_get32(&marker->data[i] + 4, endian);
	i = i + offset;

	/* check that we still are within the buffer and can read the tag count */
	if ((i + 2) > marker->data_length) {
		ret = FALSE;
		goto out;
	}

	/* find out how many tags we have in IFD0. As per the TIFF spec, the first
	   two bytes of the IFD contain a count of the number of tags. */
	tags = de_get16(&marker->data[i], endian);
	i = i + 2;

	/* check that we still have enough data for all tags to check. The tags
	   are listed in consecutive 12-byte blocks. The tag ID, type, size, and
	   a pointer to the actual value, are packed into these 12 byte entries. */
	if ((i + tags * 12) > marker->data_length) {
		ret = FALSE;
		goto out;
	}

	/* check through IFD0 for tags */
	while (tags--){
		guint tag   = de_get16(&marker->data[i + 0], endian);
		guint type  = de_get16(&marker->data[i + 2], endian);
		guint count = de_get32(&marker->data[i + 4], endian);
		/* values of types small enough to fit are stored directly in the (first) bytes of the Value Offset field */
		guint short_value = de_get16(&marker->data[i + 8], endian);

		/* orientation tag? */
		if (tag == 0x112){

			/* The orientation field should consist of a single 2-byte integer */
			if (type != 0x3 || count != 1)
				continue;

			/* get the orientation value */
			context->orientation = short_value <= 8 ? short_value : 0;
		}
		/* move the pointer to the next 12-byte tag field. */
		i = i + 12;
	}

out:
	return ret;
}

static void
jpeg_parse_exif (JpegExifContext *context, j_decompress_ptr cinfo)
{
	jpeg_saved_marker_ptr cmarker;

	/* check for interesting Exif markers */
	cmarker = cinfo->marker_list;
	while (cmarker != NULL) {
		if (cmarker->marker == JPEG_APP0+1)
			jpeg_parse_exif_app1 (context, cmarker);
		else if (cmarker->marker == JPEG_APP0+2)
			jpeg_parse_exif_app2_segment (context, cmarker);
		cmarker = cmarker->next;
	}
}

static void
jpeg_destroy_exif_context (JpegExifContext *context)
{
	g_free (context->icc_profile);
}

/* Shared library entry point */
static GdkPixbuf *
gdk_pixbuf__jpeg_image_load (FILE *f, GError **error)
{
	gint   i;
	char   otag_str[5];
	GdkPixbuf * volatile pixbuf = NULL;
	guchar *dptr;
	guchar *lines[4]; /* Used to expand rows, via rec_outbuf_height, 
                           * from the header file: 
                           * " Usually rec_outbuf_height will be 1 or 2, 
                           * at most 4."
			   */
	guchar **lptr;
	struct jpeg_decompress_struct cinfo;
	struct error_handler_data jerr;
	stdio_src_ptr src;
	gchar *icc_profile_base64;
	JpegExifContext exif_context = { 0, };

	/* setup error handler */
	cinfo.err = jpeg_std_error (&jerr.pub);
	jerr.pub.error_exit = fatal_error_handler;
        jerr.pub.output_message = output_message_handler;
        jerr.error = error;
        
	if (sigsetjmp (jerr.setjmp_buffer, 1)) {
		/* Whoops there was a jpeg error */
		if (pixbuf)
			g_object_unref (pixbuf);

		jpeg_destroy_decompress (&cinfo);
		jpeg_destroy_exif_context (&exif_context);

		/* error should have been set by fatal_error_handler () */
		return NULL;
	}

	/* load header, setup */
	jpeg_create_decompress (&cinfo);

	cinfo.src = (struct jpeg_source_mgr *)
	  (*cinfo.mem->alloc_small) ((j_common_ptr) &cinfo, JPOOL_PERMANENT,
				  sizeof (stdio_source_mgr));
	src = (stdio_src_ptr) cinfo.src;
	src->buffer = (JOCTET *)
	  (*cinfo.mem->alloc_small) ((j_common_ptr) &cinfo, JPOOL_PERMANENT,
				      JPEG_PROG_BUF_SIZE * sizeof (JOCTET));

	src->pub.init_source = stdio_init_source;
	src->pub.fill_input_buffer = stdio_fill_input_buffer;
	src->pub.skip_input_data = stdio_skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = stdio_term_source;
	src->infile = f;
	src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */

	jpeg_save_markers (&cinfo, JPEG_APP0+1, 0xffff);
	jpeg_save_markers (&cinfo, JPEG_APP0+2, 0xffff);
	jpeg_read_header (&cinfo, TRUE);

	/* parse exif data */
	jpeg_parse_exif (&exif_context, &cinfo);
	
	jpeg_start_decompress (&cinfo);
	cinfo.do_fancy_upsampling = FALSE;
	cinfo.do_block_smoothing = FALSE;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
				 cinfo.out_color_components == 4 ? TRUE : FALSE, 
				 8, cinfo.output_width, cinfo.output_height);
	      
	if (!pixbuf) {
                /* broken check for *error == NULL for robustness against
                 * crappy JPEG library
                 */
                if (error && *error == NULL) {
                        g_set_error_literal (error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                             _("Insufficient memory to load image, try exiting some applications to free memory"));
                }
               
		goto out; 
	}

	/* if orientation tag was found */
	if (exif_context.orientation != 0) {
		g_snprintf (otag_str, sizeof (otag_str), "%d", exif_context.orientation);
		gdk_pixbuf_set_option (pixbuf, "orientation", otag_str);
	}

	/* if icc profile was found */
	if (exif_context.icc_profile != NULL) {
		icc_profile_base64 = g_base64_encode ((const guchar *) exif_context.icc_profile, exif_context.icc_profile_size);
		gdk_pixbuf_set_option (pixbuf, "icc-profile", icc_profile_base64);
		g_free (icc_profile_base64);
	}

	dptr = pixbuf->pixels;

	/* decompress all the lines, a few at a time */
	while (cinfo.output_scanline < cinfo.output_height) {
		lptr = lines;
		for (i = 0; i < cinfo.rec_outbuf_height; i++) {
			*lptr++ = dptr;
			dptr += pixbuf->rowstride;
		}

		jpeg_read_scanlines (&cinfo, lines, cinfo.rec_outbuf_height);

		switch (cinfo.out_color_space) {
		    case JCS_GRAYSCALE:
		      explode_gray_into_buf (&cinfo, lines);
		      break;
		    case JCS_RGB:
		      /* do nothing */
		      break;
		    case JCS_CMYK:
		      convert_cmyk_to_rgb (&cinfo, lines);
		      break;
		    default:
		      g_object_unref (pixbuf);
		      pixbuf = NULL;
		      if (error && *error == NULL) {
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
				     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
				     _("Unsupported JPEG color space (%s)"),
				     colorspace_name (cinfo.out_color_space)); 
		      }
               	      goto out; 
		}
	}

out:
	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);
	jpeg_destroy_exif_context (&exif_context);

	return pixbuf;
}


/**** Progressive image loading handling *****/

/* these routines required because we are acting as a source manager for */
/* libjpeg. */
static void
init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	src->skip_next = 0;
}


static void
term_source (j_decompress_ptr cinfo)
{
	/* XXXX - probably should scream something has happened */
}


/* for progressive loading (called "I/O Suspension" by libjpeg docs) */
/* we do nothing except return "FALSE"                               */
static boolean
fill_input_buffer (j_decompress_ptr cinfo)
{
	return FALSE;
}


static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
	long   num_can_do;

	/* move as far as we can into current buffer */
	/* then set skip_next to catch the rest      */
	if (num_bytes > 0) {
		num_can_do = MIN (src->pub.bytes_in_buffer, num_bytes);
		src->pub.next_input_byte += (size_t) num_can_do;
		src->pub.bytes_in_buffer -= (size_t) num_can_do;

		src->skip_next = num_bytes - num_can_do;
	}
}

 
/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

static gpointer
gdk_pixbuf__jpeg_image_begin_load (GdkPixbufModuleSizeFunc size_func,
				   GdkPixbufModulePreparedFunc prepared_func, 
				   GdkPixbufModuleUpdatedFunc updated_func,
				   gpointer user_data,
                                   GError **error)
{
	JpegProgContext *context;
	my_source_mgr   *src;

	context = g_new0 (JpegProgContext, 1);
	context->size_func = size_func;
	context->prepared_func = prepared_func;
	context->updated_func  = updated_func;
	context->user_data = user_data;
	context->pixbuf = NULL;
	context->got_header = FALSE;
	context->did_prescan = FALSE;
	context->src_initialized = FALSE;
	context->in_output = FALSE;

        /* From jpeglib.h: "NB: you must set up the error-manager
         * BEFORE calling jpeg_create_xxx". */
	context->cinfo.err = jpeg_std_error (&context->jerr.pub);
	context->jerr.pub.error_exit = fatal_error_handler;
        context->jerr.pub.output_message = output_message_handler;
        context->jerr.error = error;

        if (sigsetjmp (context->jerr.setjmp_buffer, 1)) {
                jpeg_destroy_decompress (&context->cinfo);
                g_free(context);
                /* error should have been set by fatal_error_handler () */
                return NULL;
        }

	/* create libjpeg structures */
	jpeg_create_decompress (&context->cinfo);

	context->cinfo.src = (struct jpeg_source_mgr *) g_try_malloc (sizeof (my_source_mgr));
	if (!context->cinfo.src) {
		g_set_error_literal (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Couldn't allocate memory for loading JPEG file"));
		return NULL;
	}
	memset (context->cinfo.src, 0, sizeof (my_source_mgr));

	src = (my_src_ptr) context->cinfo.src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

        context->jerr.error = NULL;
        
	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
static gboolean
gdk_pixbuf__jpeg_image_stop_load (gpointer data, GError **error)
{
	JpegProgContext *context = (JpegProgContext *) data;
        gboolean retval;

	g_return_val_if_fail (context != NULL, TRUE);
	
        /* FIXME this thing needs to report errors if
         * we have unused image data
         */
        
	if (context->pixbuf)
		g_object_unref (context->pixbuf);
	
	/* if we have an error? */
	context->jerr.error = error;
	if (sigsetjmp (context->jerr.setjmp_buffer, 1)) {
                retval = FALSE;
	} else {
		jpeg_finish_decompress (&context->cinfo);
                retval = TRUE;
	}

        jpeg_destroy_decompress (&context->cinfo);

	if (context->cinfo.src) {
		my_src_ptr src = (my_src_ptr) context->cinfo.src;
		
		g_free (src);
	}

	g_free (context);

        return retval;
}


static gboolean
gdk_pixbuf__jpeg_image_load_lines (JpegProgContext  *context,
                                   GError          **error)
{
        struct jpeg_decompress_struct *cinfo = &context->cinfo;
        guchar *lines[4];
        guchar **lptr;
        guchar *rowptr;
        gint   nlines, i;

        /* keep going until we've done all scanlines */
        while (cinfo->output_scanline < cinfo->output_height) {
                lptr = lines;
                rowptr = context->dptr;
                for (i=0; i < cinfo->rec_outbuf_height; i++) {
                        *lptr++ = rowptr;
                        rowptr += context->pixbuf->rowstride;
                }

                nlines = jpeg_read_scanlines (cinfo, lines,
                                              cinfo->rec_outbuf_height);
                if (nlines == 0)
                        break;

                switch (cinfo->out_color_space) {
                case JCS_GRAYSCALE:
                        explode_gray_into_buf (cinfo, lines);
                        break;
                case JCS_RGB:
                        /* do nothing */
                        break;
                case JCS_CMYK:
                        convert_cmyk_to_rgb (cinfo, lines);
                        break;
                default:
                        if (error && *error == NULL) {
                                g_set_error (error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                                             _("Unsupported JPEG color space (%s)"),
                                             colorspace_name (cinfo->out_color_space));
                        }

                        return FALSE;
                }

                context->dptr += nlines * context->pixbuf->rowstride;

                /* send updated signal */
		if (context->updated_func)
			(* context->updated_func) (context->pixbuf,
						   0,
						   cinfo->output_scanline - 1,
						   cinfo->image_width,
						   nlines,
						   context->user_data);
        }

        return TRUE;
}


/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
static gboolean
gdk_pixbuf__jpeg_image_load_increment (gpointer data,
                                       const guchar *buf, guint size,
                                       GError **error)
{
	JpegProgContext *context = (JpegProgContext *)data;
	struct           jpeg_decompress_struct *cinfo;
	my_src_ptr       src;
	guint            num_left, num_copy;
	guint            last_num_left, last_bytes_left;
	guint            spinguard;
	gboolean         first;
	const guchar    *bufhd;
	gint             width, height;
	char             otag_str[5];
	gchar 		*icc_profile_base64;
	JpegExifContext  exif_context = { 0, };
	gboolean	 retval;

	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);

	src = (my_src_ptr) context->cinfo.src;

	cinfo = &context->cinfo;

        context->jerr.error = error;
        
	/* check for fatal error */
	if (sigsetjmp (context->jerr.setjmp_buffer, 1)) {
		retval = FALSE;
		goto out;
	}

	/* skip over data if requested, handle unsigned int sizes cleanly */
	/* only can happen if we've already called jpeg_get_header once   */
	if (context->src_initialized && src->skip_next) {
		if (src->skip_next > size) {
			src->skip_next -= size;
			retval = TRUE;
			goto out;
		} else {
			num_left = size - src->skip_next;
			bufhd = buf + src->skip_next;
			src->skip_next = 0;
		}
	} else {
		num_left = size;
		bufhd = buf;
	}

	if (num_left == 0) {
		retval = TRUE;
		goto out;
	}

	last_num_left = num_left;
	last_bytes_left = 0;
	spinguard = 0;
	first = TRUE;
	while (TRUE) {

		/* handle any data from caller we haven't processed yet */
		if (num_left > 0) {
			if(src->pub.bytes_in_buffer && 
			   src->pub.next_input_byte != src->buffer)
				memmove(src->buffer, src->pub.next_input_byte,
					src->pub.bytes_in_buffer);


			num_copy = MIN (JPEG_PROG_BUF_SIZE - src->pub.bytes_in_buffer,
					num_left);

			memcpy(src->buffer + src->pub.bytes_in_buffer, bufhd,num_copy);
			src->pub.next_input_byte = src->buffer;
			src->pub.bytes_in_buffer += num_copy;
			bufhd += num_copy;
			num_left -= num_copy;
		}

                /* did anything change from last pass, if not return */
                if (first) {
                        last_bytes_left = src->pub.bytes_in_buffer;
                        first = FALSE;
                } else if (src->pub.bytes_in_buffer == last_bytes_left
			   && num_left == last_num_left) {
                        spinguard++;
		} else {
                        last_bytes_left = src->pub.bytes_in_buffer;
			last_num_left = num_left;
		}

		/* should not go through twice and not pull bytes out of buf */
		if (spinguard > 2) {
			retval = TRUE;
			goto out;
		}

		/* try to load jpeg header */
		if (!context->got_header) {
			int rc;
		
			jpeg_save_markers (cinfo, JPEG_APP0+1, 0xffff);
			jpeg_save_markers (cinfo, JPEG_APP0+2, 0xffff);
			rc = jpeg_read_header (cinfo, TRUE);
			context->src_initialized = TRUE;
			
			if (rc == JPEG_SUSPENDED)
				continue;
			
			context->got_header = TRUE;

			/* parse exif data */
			jpeg_parse_exif (&exif_context, cinfo);
		
			width = cinfo->image_width;
			height = cinfo->image_height;
			if (context->size_func) {
				(* context->size_func) (&width, &height, context->user_data);
				if (width == 0 || height == 0) {
					g_set_error_literal (error,
                                                             GDK_PIXBUF_ERROR,
                                                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                                             _("Transformed JPEG has zero width or height."));
					retval = FALSE;
					goto out;
				}
			}
			
			cinfo->scale_num = 1;
			for (cinfo->scale_denom = 2; cinfo->scale_denom <= 8; cinfo->scale_denom *= 2) {
				jpeg_calc_output_dimensions (cinfo);
				if (cinfo->output_width < width || cinfo->output_height < height) {
					cinfo->scale_denom /= 2;
					break;
				}
			}
			jpeg_calc_output_dimensions (cinfo);
			
			context->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
							  cinfo->output_components == 4 ? TRUE : FALSE,
							  8, 
							  cinfo->output_width,
							  cinfo->output_height);

			if (context->pixbuf == NULL) {
                                g_set_error_literal (error,
                                                     GDK_PIXBUF_ERROR,
                                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                                     _("Couldn't allocate memory for loading JPEG file"));
                                retval = FALSE;
				goto out;
			}
		
		        /* if orientation tag was found set an option to remember its value */
			if (exif_context.orientation != 0) {
				g_snprintf (otag_str, sizeof (otag_str), "%d", exif_context.orientation);
		                gdk_pixbuf_set_option (context->pixbuf, "orientation", otag_str);
		        }
			/* if icc profile was found */
			if (exif_context.icc_profile != NULL) {
				icc_profile_base64 = g_base64_encode ((const guchar *) exif_context.icc_profile, exif_context.icc_profile_size);
				gdk_pixbuf_set_option (context->pixbuf, "icc-profile", icc_profile_base64);
				g_free (icc_profile_base64);
			}


			/* Use pixbuf buffer to store decompressed data */
			context->dptr = context->pixbuf->pixels;
			
			/* Notify the client that we are ready to go */
			if (context->prepared_func)
				(* context->prepared_func) (context->pixbuf,
							    NULL,
							    context->user_data);
			
		} else if (!context->did_prescan) {
			int rc;			
			
			/* start decompression */
			cinfo->buffered_image = cinfo->progressive_mode;
			rc = jpeg_start_decompress (cinfo);
			cinfo->do_fancy_upsampling = FALSE;
			cinfo->do_block_smoothing = FALSE;

			if (rc == JPEG_SUSPENDED)
				continue;

			context->did_prescan = TRUE;
		} else if (!cinfo->buffered_image) {
                        /* we're decompressing unbuffered so
                         * simply get scanline by scanline from jpeg lib
                         */
                        if (! gdk_pixbuf__jpeg_image_load_lines (context,
                                                                 error)) {
                                retval = FALSE;
				goto out;
			}

			if (cinfo->output_scanline >= cinfo->output_height) {
				retval = TRUE;
				goto out;
			}
		} else {
                        /* we're decompressing buffered (progressive)
                         * so feed jpeg lib scanlines
                         */

			/* keep going until we've done all passes */
			while (!jpeg_input_complete (cinfo)) {
				if (!context->in_output) {
					if (jpeg_start_output (cinfo, cinfo->input_scan_number)) {
						context->in_output = TRUE;
						context->dptr = context->pixbuf->pixels;
					}
					else
						break;
				}

                                /* get scanlines from jpeg lib */
                                if (! gdk_pixbuf__jpeg_image_load_lines (context,
                                                                         error)) {
                                        retval = FALSE;
					goto out;
				}

				if (cinfo->output_scanline >= cinfo->output_height &&
				    jpeg_finish_output (cinfo))
					context->in_output = FALSE;
				else
					break;
			}
			if (jpeg_input_complete (cinfo)) {
				/* did entire image */
				retval = TRUE;
				goto out;
			}
			else
				continue;
		}
	}
out:
	jpeg_destroy_exif_context (&exif_context);
	return retval;
}

/* Save */

#define TO_FUNCTION_BUF_SIZE 4096

typedef struct {
	struct jpeg_destination_mgr pub;
	JOCTET             *buffer;
	GdkPixbufSaveFunc   save_func;
	gpointer            user_data;
	GError            **error;
} ToFunctionDestinationManager;

void
to_callback_init (j_compress_ptr cinfo)
{
	ToFunctionDestinationManager *destmgr;

	destmgr	= (ToFunctionDestinationManager*) cinfo->dest;
	destmgr->pub.next_output_byte = destmgr->buffer;
	destmgr->pub.free_in_buffer = TO_FUNCTION_BUF_SIZE;
}

static void
to_callback_do_write (j_compress_ptr cinfo, gsize length)
{
	ToFunctionDestinationManager *destmgr;

	destmgr	= (ToFunctionDestinationManager*) cinfo->dest;
        if (!destmgr->save_func ((gchar *)destmgr->buffer,
				 length,
				 destmgr->error,
				 destmgr->user_data)) {
		struct error_handler_data *errmgr;
        
		errmgr = (struct error_handler_data *) cinfo->err;
		/* Use a default error message if the callback didn't set one,
		 * which it should have.
		 */
		if (errmgr->error && *errmgr->error == NULL) {
			g_set_error_literal (errmgr->error,
                                             GDK_PIXBUF_ERROR,
                                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                             "write function failed");
		}
		siglongjmp (errmgr->setjmp_buffer, 1);
		g_assert_not_reached ();
        }
}

static boolean
to_callback_empty_output_buffer (j_compress_ptr cinfo)
{
	ToFunctionDestinationManager *destmgr;

	destmgr	= (ToFunctionDestinationManager*) cinfo->dest;
	to_callback_do_write (cinfo, TO_FUNCTION_BUF_SIZE);
	destmgr->pub.next_output_byte = destmgr->buffer;
	destmgr->pub.free_in_buffer = TO_FUNCTION_BUF_SIZE;
	return TRUE;
}

void
to_callback_terminate (j_compress_ptr cinfo)
{
	ToFunctionDestinationManager *destmgr;

	destmgr	= (ToFunctionDestinationManager*) cinfo->dest;
	to_callback_do_write (cinfo, TO_FUNCTION_BUF_SIZE - destmgr->pub.free_in_buffer);
}

static gboolean
real_save_jpeg (GdkPixbuf          *pixbuf,
		gchar             **keys,
		gchar             **values,
		GError            **error,
		gboolean            to_callback,
		FILE               *f,
		GdkPixbufSaveFunc   save_func,
		gpointer            user_data)
{
        /* FIXME error handling is broken */
        
       struct jpeg_compress_struct cinfo;
       guchar *buf = NULL;
       guchar *ptr;
       guchar *pixels = NULL;
       JSAMPROW *jbuf;
       int y = 0;
       volatile int quality = 75; /* default; must be between 0 and 100 */
       int i, j;
       int w, h = 0;
       int rowstride = 0;
       int n_channels;
       struct error_handler_data jerr;
       ToFunctionDestinationManager to_callback_destmgr;
       gchar *icc_profile = NULL;
       gchar *data;
       gint retval = TRUE;
       gsize icc_profile_size = 0;

       to_callback_destmgr.buffer = NULL;

       if (keys && *keys) {
               gchar **kiter = keys;
               gchar **viter = values;

               while (*kiter) {
                       if (strcmp (*kiter, "quality") == 0) {
                               char *endptr = NULL;
                               quality = strtol (*viter, &endptr, 10);

                               if (endptr == *viter) {
                                       g_set_error (error,
                                                    GDK_PIXBUF_ERROR,
                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                    _("JPEG quality must be a value between 0 and 100; value '%s' could not be parsed."),
                                                    *viter);

                                       retval = FALSE;
                                       goto cleanup;
                               }
                               
                               if (quality < 0 ||
                                   quality > 100) {
                                       /* This is a user-visible error;
                                        * lets people skip the range-checking
                                        * in their app.
                                        */
                                       g_set_error (error,
                                                    GDK_PIXBUF_ERROR,
                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                    _("JPEG quality must be a value between 0 and 100; value '%d' is not allowed."),
                                                    quality);

                                       retval = FALSE;
                                       goto cleanup;
                               }
                       } else if (strcmp (*kiter, "icc-profile") == 0) {
                               /* decode from base64 */
                               icc_profile = (gchar*) g_base64_decode (*viter, &icc_profile_size);
                               if (icc_profile_size < 127) {
                                       /* This is a user-visible error */
                                       g_set_error (error,
                                                    GDK_PIXBUF_ERROR,
                                                    GDK_PIXBUF_ERROR_BAD_OPTION,
                                                    _("Color profile has invalid length '%u'."),
                                                    (guint) icc_profile_size);
                                       retval = FALSE;
                                       goto cleanup;
                               }
                       } else {
                               g_warning ("Unrecognized parameter (%s) passed to JPEG saver.", *kiter);
                       }
               
                       ++kiter;
                       ++viter;
               }
       }
       
       rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       n_channels = gdk_pixbuf_get_n_channels (pixbuf);

       w = gdk_pixbuf_get_width (pixbuf);
       h = gdk_pixbuf_get_height (pixbuf);
       pixels = gdk_pixbuf_get_pixels (pixbuf);

       /* Allocate a small buffer to convert image data,
	* and a larger buffer if doing to_callback save.
	*/
       buf = g_try_malloc (w * 3 * sizeof (guchar));
       if (!buf) {
	       g_set_error_literal (error,
                                    GDK_PIXBUF_ERROR,
                                    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                    _("Couldn't allocate memory for loading JPEG file"));
	       retval = FALSE;
	       goto cleanup;
       }
       if (to_callback) {
	       to_callback_destmgr.buffer = g_try_malloc (TO_FUNCTION_BUF_SIZE);
	       if (!to_callback_destmgr.buffer) {
		       g_set_error_literal (error,
                                            GDK_PIXBUF_ERROR,
                                            GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                            _("Couldn't allocate memory for loading JPEG file"));
		       retval = FALSE;
		       goto cleanup;
	       }
       }

       /* set up error handling */
       cinfo.err = jpeg_std_error (&(jerr.pub));
       jerr.pub.error_exit = fatal_error_handler;
       jerr.pub.output_message = output_message_handler;
       jerr.error = error;
       
       if (sigsetjmp (jerr.setjmp_buffer, 1)) {
               jpeg_destroy_compress (&cinfo);
	       retval = FALSE;
	       goto cleanup;
       }

       /* setup compress params */
       jpeg_create_compress (&cinfo);
       if (to_callback) {
	       to_callback_destmgr.pub.init_destination    = to_callback_init;
	       to_callback_destmgr.pub.empty_output_buffer = to_callback_empty_output_buffer;
	       to_callback_destmgr.pub.term_destination    = to_callback_terminate;
	       to_callback_destmgr.error = error;
	       to_callback_destmgr.save_func = save_func;
	       to_callback_destmgr.user_data = user_data;
	       cinfo.dest = (struct jpeg_destination_mgr*) &to_callback_destmgr;
       } else {
	       jpeg_stdio_dest (&cinfo, f);
       }
       cinfo.image_width      = w;
       cinfo.image_height     = h;
       cinfo.input_components = 3; 
       cinfo.in_color_space   = JCS_RGB;

       /* set up jepg compression parameters */
       jpeg_set_defaults (&cinfo);
       jpeg_set_quality (&cinfo, quality, TRUE);

       jpeg_start_compress (&cinfo, TRUE);

	/* write ICC profile data */
	if (icc_profile != NULL) {
		/* optimise for the common case where only one APP2 segment is required */
		if (icc_profile_size < 0xffef) {
			data = g_new (gchar, icc_profile_size + 14);
			memcpy (data, "ICC_PROFILE\000\001\001", 14);
			memcpy (data + 14, icc_profile, icc_profile_size);
			jpeg_write_marker (&cinfo, JPEG_APP0+2, (const JOCTET *) data, icc_profile_size + 14);
			g_free (data);
		} else {
			guint segments;
			guint size = 0xffef;
			guint offset;

			segments = (guint) ceilf ((gfloat) icc_profile_size / (gfloat) 0xffef);
			data = g_new (gchar, 0xffff);
			memcpy (data, "ICC_PROFILE\000", 12);
			data[13] = segments;
			for (i=0; i<=segments; i++) {
				data[12] = i;
				offset = 0xffef * i;

				/* last segment */
				if (i == segments)
					size = icc_profile_size % 0xffef;

				memcpy (data + 14, icc_profile + offset, size);
				jpeg_write_marker (&cinfo, JPEG_APP0+2, (const JOCTET *) data, size + 14);
			}
			g_free (data);
		}
	}

       /* get the start pointer */
       ptr = pixels;
       /* go one scanline at a time... and save */
       i = 0;
       while (cinfo.next_scanline < cinfo.image_height) {
               /* convert scanline from ARGB to RGB packed */
               for (j = 0; j < w; j++)
                       memcpy (&(buf[j*3]), &(ptr[i*rowstride + j*n_channels]), 3);

               /* write scanline */
               jbuf = (JSAMPROW *)(&buf);
               jpeg_write_scanlines (&cinfo, jbuf, 1);
               i++;
               y++;

       }

       /* finish off */
       jpeg_finish_compress (&cinfo);
       jpeg_destroy_compress(&cinfo);
cleanup:
	g_free (buf);
	g_free (to_callback_destmgr.buffer);
	g_free (icc_profile);
	return retval;
}

static gboolean
gdk_pixbuf__jpeg_image_save (FILE          *f, 
                             GdkPixbuf     *pixbuf, 
                             gchar        **keys,
                             gchar        **values,
                             GError       **error)
{
	return real_save_jpeg (pixbuf, keys, values, error,
			       FALSE, f, NULL, NULL);
}

static gboolean
gdk_pixbuf__jpeg_image_save_to_callback (GdkPixbufSaveFunc   save_func,
					 gpointer            user_data,
					 GdkPixbuf          *pixbuf, 
					 gchar             **keys,
					 gchar             **values,
					 GError            **error)
{
	return real_save_jpeg (pixbuf, keys, values, error,
			       TRUE, NULL, save_func, user_data);
}

#ifndef INCLUDE_jpeg
#define MODULE_ENTRY(function) G_MODULE_EXPORT void function
#else
#define MODULE_ENTRY(function) void _gdk_pixbuf__jpeg_ ## function
#endif

MODULE_ENTRY (fill_vtable) (GdkPixbufModule *module)
{
	module->load = gdk_pixbuf__jpeg_image_load;
	module->begin_load = gdk_pixbuf__jpeg_image_begin_load;
	module->stop_load = gdk_pixbuf__jpeg_image_stop_load;
	module->load_increment = gdk_pixbuf__jpeg_image_load_increment;
	module->save = gdk_pixbuf__jpeg_image_save;
	module->save_to_callback = gdk_pixbuf__jpeg_image_save_to_callback;
}

MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
	static const GdkPixbufModulePattern signature[] = {
		{ "\xff\xd8", NULL, 100 },
		{ NULL, NULL, 0 }
	};
	static const gchar *mime_types[] = {
		"image/jpeg",
		NULL
	};
	static const gchar *extensions[] = {
		"jpeg",
		"jpe",
		"jpg",
		NULL
	};

	info->name = "jpeg";
	info->signature = (GdkPixbufModulePattern *) signature;
	info->description = N_("The JPEG image format");
	info->mime_types = (gchar **) mime_types;
	info->extensions = (gchar **) extensions;
	info->flags = GDK_PIXBUF_FORMAT_WRITABLE | GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = "LGPL";
}
