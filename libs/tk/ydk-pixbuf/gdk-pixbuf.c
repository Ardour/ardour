/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Basic memory management
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GDK_PIXBUF_C_COMPILATION
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-features.h"
#include "gdk-pixbuf-enum-types.h"

/* Include the marshallers */
#include <glib-object.h>
#include <gio/gio.h>
#include "gdk-pixbuf-marshal.c"

/**
 * SECTION:creating
 * @Short_description: Creating a pixbuf from image data that is already in memory.
 * @Title: Image Data in Memory
 * @See_also: gdk_pixbuf_finalize().
 * 
 * The most basic way to create a pixbuf is to wrap an existing pixel
 * buffer with a #GdkPixbuf structure.  You can use the
 * gdk_pixbuf_new_from_data() function to do this You need to specify
 * the destroy notification function that will be called when the
 * data buffer needs to be freed; this will happen when a #GdkPixbuf
 * is finalized by the reference counting functions If you have a
 * chunk of static data compiled into your application, you can pass
 * in %NULL as the destroy notification function so that the data
 * will not be freed.
 * 
 * The gdk_pixbuf_new() function can be used as a convenience to
 * create a pixbuf with an empty buffer.  This is equivalent to
 * allocating a data buffer using malloc() and then wrapping it with
 * gdk_pixbuf_new_from_data(). The gdk_pixbuf_new() function will
 * compute an optimal rowstride so that rendering can be performed
 * with an efficient algorithm.
 * 
 * As a special case, you can use the gdk_pixbuf_new_from_xpm_data()
 * function to create a pixbuf from inline XPM image data.
 * 
 * You can also copy an existing pixbuf with the gdk_pixbuf_copy()
 * function.  This is not the same as just doing a g_object_ref()
 * on the old pixbuf; the copy function will actually duplicate the
 * pixel data in memory and create a new #GdkPixbuf structure for it.
 */

/**
 * SECTION:refcounting
 * @Short_description: Functions for reference counting and memory management on pixbufs.
 * @Title: Reference Counting and Memory Mangement
 * @See_also: #GdkPixbuf, gdk_pixbuf_new_from_data().
 * 
 * #GdkPixbuf structures are reference counted.  This means that an
 * application can share a single pixbuf among many parts of the
 * code.  When a piece of the program needs to keep a pointer to a
 * pixbuf, it should add a reference to it by calling g_object_ref().
 * When it no longer needs the pixbuf, it should subtract a reference
 * by calling g_object_unref().  The pixbuf will be destroyed when
 * its reference count drops to zero.  Newly-created #GdkPixbuf
 * structures start with a reference count of one.
 * 
 * > As #GdkPixbuf is derived from #GObject now, gdk_pixbuf_ref() and
 * > gdk_pixbuf_unref() are deprecated in favour of g_object_ref()
 * > and g_object_unref() resp.
 * 
 * Finalizing a pixbuf means to free its pixel data and to free the
 * #GdkPixbuf structure itself.  Most of the library functions that
 * create #GdkPixbuf structures create the pixel data by themselves
 * and define the way it should be freed; you do not need to worry
 * about those.
 *
 * To provide preallocated pixel data, use
 * gdk_pixbuf_new_from_bytes().  The gdk_pixbuf_new_from_data() API is
 * an older variant that predates the existence of #GBytes.
 */

static void gdk_pixbuf_finalize     (GObject        *object);
static void gdk_pixbuf_set_property (GObject        *object,
				     guint           prop_id,
				     const GValue   *value,
				     GParamSpec     *pspec);
static void gdk_pixbuf_get_property (GObject        *object,
				     guint           prop_id,
				     GValue         *value,
				     GParamSpec     *pspec);


enum 
{
  PROP_0,
  PROP_COLORSPACE,
  PROP_N_CHANNELS,
  PROP_HAS_ALPHA,
  PROP_BITS_PER_SAMPLE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ROWSTRIDE,
  PROP_PIXELS,
  PROP_PIXEL_BYTES
};

static void gdk_pixbuf_icon_iface_init (GIconIface *iface);
static void gdk_pixbuf_loadable_icon_iface_init (GLoadableIconIface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkPixbuf, gdk_pixbuf, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ICON, gdk_pixbuf_icon_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LOADABLE_ICON, gdk_pixbuf_loadable_icon_iface_init))

static void 
gdk_pixbuf_init (GdkPixbuf *pixbuf)
{
}

static void
gdk_pixbuf_class_init (GdkPixbufClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_finalize;
        object_class->set_property = gdk_pixbuf_set_property;
        object_class->get_property = gdk_pixbuf_get_property;

#define PIXBUF_PARAM_FLAGS G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|\
                           G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
        /**
         * GdkPixbuf:n-channels:
         *
         * The number of samples per pixel. 
         * Currently, only 3 or 4 samples per pixel are supported.
         */
        g_object_class_install_property (object_class,
                                         PROP_N_CHANNELS,
                                         g_param_spec_int ("n-channels",
                                                           P_("Number of Channels"),
                                                           P_("The number of samples per pixel"),
                                                           0,
                                                           G_MAXINT,
                                                           3,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_COLORSPACE,
                                         g_param_spec_enum ("colorspace",
                                                            P_("Colorspace"),
                                                            P_("The colorspace in which the samples are interpreted"),
                                                            GDK_TYPE_COLORSPACE,
                                                            GDK_COLORSPACE_RGB,
                                                            PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_HAS_ALPHA,
                                         g_param_spec_boolean ("has-alpha",
                                                               P_("Has Alpha"),
                                                               P_("Whether the pixbuf has an alpha channel"),
                                                               FALSE,
                                                               PIXBUF_PARAM_FLAGS));

        /**
         * GdkPixbuf:bits-per-sample:
         *
         * The number of bits per sample. 
         * Currently only 8 bit per sample are supported.
         */
        g_object_class_install_property (object_class,
                                         PROP_BITS_PER_SAMPLE,
                                         g_param_spec_int ("bits-per-sample",
                                                           P_("Bits per Sample"),
                                                           P_("The number of bits per sample"),
                                                           1,
                                                           16,
                                                           8,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_WIDTH,
                                         g_param_spec_int ("width",
                                                           P_("Width"),
                                                           P_("The number of columns of the pixbuf"),
                                                           1,
                                                           G_MAXINT,
                                                           1,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_HEIGHT,
                                         g_param_spec_int ("height",
                                                           P_("Height"),
                                                           P_("The number of rows of the pixbuf"),
                                                           1,
                                                           G_MAXINT,
                                                           1,
                                                           PIXBUF_PARAM_FLAGS));

        /**
         * GdkPixbuf:rowstride:
         *
         * The number of bytes between the start of a row and 
         * the start of the next row. This number must (obviously)
         * be at least as large as the width of the pixbuf.
         */
        g_object_class_install_property (object_class,
                                         PROP_ROWSTRIDE,
                                         g_param_spec_int ("rowstride",
                                                           P_("Rowstride"),
                                                           P_("The number of bytes between the start of a row and the start of the next row"),
                                                           1,
                                                           G_MAXINT,
                                                           1,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_PIXELS,
                                         g_param_spec_pointer ("pixels",
                                                               P_("Pixels"),
                                                               P_("A pointer to the pixel data of the pixbuf"),
                                                               PIXBUF_PARAM_FLAGS));

        /**
         * GdkPixbuf::pixel-bytes:
         *
         * If set, this pixbuf was created from read-only #GBytes.
         * Replaces GdkPixbuf::pixels.
         * 
         * Since: 2.32
         */
        g_object_class_install_property (object_class,
                                         PROP_PIXEL_BYTES,
                                         g_param_spec_boxed ("pixel-bytes",
                                                             P_("Pixel Bytes"),
                                                             P_("Readonly pixel data"),
                                                             G_TYPE_BYTES,
                                                             PIXBUF_PARAM_FLAGS));
}

static void
gdk_pixbuf_finalize (GObject *object)
{
        GdkPixbuf *pixbuf = GDK_PIXBUF (object);
        
        if (pixbuf->pixels && pixbuf->destroy_fn)
                (* pixbuf->destroy_fn) (pixbuf->pixels, pixbuf->destroy_fn_data);

        g_clear_pointer (&pixbuf->bytes, g_bytes_unref);
        
        G_OBJECT_CLASS (gdk_pixbuf_parent_class)->finalize (object);
}


/**
 * gdk_pixbuf_ref: (skip)
 * @pixbuf: A pixbuf.
 *
 * Adds a reference to a pixbuf.
 *
 * Return value: The same as the @pixbuf argument.
 *
 * Deprecated: 2.0: Use g_object_ref().
 **/
GdkPixbuf *
gdk_pixbuf_ref (GdkPixbuf *pixbuf)
{
        return (GdkPixbuf *) g_object_ref (pixbuf);
}

/**
 * gdk_pixbuf_unref: (skip)
 * @pixbuf: A pixbuf.
 *
 * Removes a reference from a pixbuf.
 *
 * Deprecated: 2.0: Use g_object_unref().
 **/
void
gdk_pixbuf_unref (GdkPixbuf *pixbuf)
{
        g_object_unref (pixbuf);
}

static GBytes *
gdk_pixbuf_make_bytes (GdkPixbuf  *pixbuf,
                       GError    **error)
{
  gchar *buffer;
  gsize size;

  if (!gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &size, "png", error, NULL))
    return NULL;

  return g_bytes_new_take (buffer, size);
}

static GVariant *
gdk_pixbuf_serialize (GIcon *icon)
{
  GError *error = NULL;
  GVariant *result;
  GBytes *bytes;

  bytes = gdk_pixbuf_make_bytes (GDK_PIXBUF (icon), &error);
  if (!bytes)
    {
      g_critical ("Unable to serialise GdkPixbuf to png (via g_icon_serialize()): %s", error->message);
      g_error_free (error);
      return NULL;
    }
  result = g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes, TRUE);
  g_bytes_unref (bytes);

  return g_variant_new ("(sv)", "bytes", result);
}

static GInputStream *
gdk_pixbuf_load (GLoadableIcon  *icon,
                 int             size,
                 char          **type,
                 GCancellable   *cancellable,
                 GError        **error)
{
  GInputStream *stream;
  GBytes *bytes;

  bytes = gdk_pixbuf_make_bytes (GDK_PIXBUF (icon), error);
  if (!bytes)
    return NULL;

  stream = g_memory_input_stream_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  if (type)
    *type = g_strdup ("image/png");

  return stream;
}

static void
gdk_pixbuf_load_async (GLoadableIcon       *icon,
                       int                  size,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  GTask *task;

  task = g_task_new (icon, cancellable, callback, user_data);
  g_task_return_pointer (task, icon, NULL);
  g_object_unref (task);
}

static GInputStream *
gdk_pixbuf_load_finish (GLoadableIcon  *icon,
                        GAsyncResult   *res,
                        char          **type,
                        GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (res, icon), NULL);

  if (!g_task_propagate_pointer (G_TASK (res), error))
    return NULL;

  return gdk_pixbuf_load (icon, 0, type, NULL, error);
}

static void
gdk_pixbuf_loadable_icon_iface_init (GLoadableIconIface *iface)
{
  iface->load = gdk_pixbuf_load;

  /* In theory encoding a png could be time-consuming but we're talking
   * about icons here, so assume it's probably going to be OK and handle
   * the async variant of the call in-thread instead of having the
   * default implementation dispatch it to a worker.
   */
  iface->load_async = gdk_pixbuf_load_async;
  iface->load_finish = gdk_pixbuf_load_finish;
}

static void
gdk_pixbuf_icon_iface_init (GIconIface *iface)
{
        iface->hash = (guint (*) (GIcon *)) g_direct_hash;
        iface->equal = (gboolean (*) (GIcon *, GIcon *)) g_direct_equal;
        iface->serialize = gdk_pixbuf_serialize;
}

/* Used as the destroy notification function for gdk_pixbuf_new() */
static void
free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

/**
 * gdk_pixbuf_new:
 * @colorspace: Color space for image
 * @has_alpha: Whether the image should have transparency information
 * @bits_per_sample: Number of bits per color sample
 * @width: Width of image in pixels, must be > 0
 * @height: Height of image in pixels, must be > 0
 *
 * Creates a new #GdkPixbuf structure and allocates a buffer for it.  The 
 * buffer has an optimal rowstride.  Note that the buffer is not cleared;
 * you will have to fill it completely yourself.
 *
 * Return value: A newly-created #GdkPixbuf with a reference count of 1, or 
 * %NULL if not enough memory could be allocated for the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_new (GdkColorspace colorspace, 
                gboolean      has_alpha,
                int           bits_per_sample,
                int           width,
                int           height)
{
	guchar *buf;
	int channels;
	int rowstride;

	g_return_val_if_fail (colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (bits_per_sample == 8, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	channels = has_alpha ? 4 : 3;
        rowstride = width * channels;
        if (rowstride / channels != width || rowstride + 3 < 0) /* overflow */
                return NULL;
        
	/* Always align rows to 32-bit boundaries */
	rowstride = (rowstride + 3) & ~3;

	buf = g_try_malloc_n (height, rowstride);
	if (!buf)
		return NULL;

	return gdk_pixbuf_new_from_data (buf, colorspace, has_alpha, bits_per_sample,
					 width, height, rowstride,
					 free_buffer, NULL);
}

/**
 * gdk_pixbuf_copy:
 * @pixbuf: A pixbuf.
 * 
 * Creates a new #GdkPixbuf with a copy of the information in the specified
 * @pixbuf.
 * 
 * Return value: (transfer full): A newly-created pixbuf with a reference count of 1, or %NULL if
 * not enough memory could be allocated.
 **/
GdkPixbuf *
gdk_pixbuf_copy (const GdkPixbuf *pixbuf)
{
	guchar *buf;
	int size;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	/* Calculate a semi-exact size.  Here we copy with full rowstrides;
	 * maybe we should copy each row individually with the minimum
	 * rowstride?
	 */

	size = gdk_pixbuf_get_byte_length (pixbuf);

	buf = g_try_malloc (size);
	if (!buf)
		return NULL;

	memcpy (buf, gdk_pixbuf_read_pixels (pixbuf), size);

	return gdk_pixbuf_new_from_data (buf,
					 pixbuf->colorspace, pixbuf->has_alpha,
					 pixbuf->bits_per_sample,
					 pixbuf->width, pixbuf->height,
					 pixbuf->rowstride,
					 free_buffer,
					 NULL);
}

/**
 * gdk_pixbuf_new_subpixbuf:
 * @src_pixbuf: a #GdkPixbuf
 * @src_x: X coord in @src_pixbuf
 * @src_y: Y coord in @src_pixbuf
 * @width: width of region in @src_pixbuf
 * @height: height of region in @src_pixbuf
 * 
 * Creates a new pixbuf which represents a sub-region of @src_pixbuf.
 * The new pixbuf shares its pixels with the original pixbuf, so
 * writing to one affects both.  The new pixbuf holds a reference to
 * @src_pixbuf, so @src_pixbuf will not be finalized until the new
 * pixbuf is finalized.
 *
 * Note that if @src_pixbuf is read-only, this function will force it
 * to be mutable.
 *
 * Return value: (transfer full): a new pixbuf 
 **/
GdkPixbuf*
gdk_pixbuf_new_subpixbuf (GdkPixbuf *src_pixbuf,
                          int        src_x,
                          int        src_y,
                          int        width,
                          int        height)
{
        guchar *pixels;
        GdkPixbuf *sub;

        g_return_val_if_fail (GDK_IS_PIXBUF (src_pixbuf), NULL);
        g_return_val_if_fail (src_x >= 0 && src_x + width <= src_pixbuf->width, NULL);
        g_return_val_if_fail (src_y >= 0 && src_y + height <= src_pixbuf->height, NULL);
        
        /* Note causes an implicit copy where src_pixbuf owns the data */
        pixels = (gdk_pixbuf_get_pixels (src_pixbuf)
                  + src_y * src_pixbuf->rowstride
                  + src_x * src_pixbuf->n_channels);

        sub = gdk_pixbuf_new_from_data (pixels,
                                        src_pixbuf->colorspace,
                                        src_pixbuf->has_alpha,
                                        src_pixbuf->bits_per_sample,
                                        width, height,
                                        src_pixbuf->rowstride,
                                        NULL, NULL);

        /* Keep a reference to src_pixbuf */
        g_object_ref (src_pixbuf);
  
        g_object_set_qdata_full (G_OBJECT (sub),
                                 g_quark_from_static_string ("gdk-pixbuf-subpixbuf-src"),
                                 src_pixbuf,
                                 (GDestroyNotify) g_object_unref);

        return sub;
}



/* Accessors */

/**
 * gdk_pixbuf_get_colorspace:
 * @pixbuf: A pixbuf.
 *
 * Queries the color space of a pixbuf.
 *
 * Return value: Color space.
 **/
GdkColorspace
gdk_pixbuf_get_colorspace (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), GDK_COLORSPACE_RGB);

	return pixbuf->colorspace;
}

/**
 * gdk_pixbuf_get_n_channels:
 * @pixbuf: A pixbuf.
 *
 * Queries the number of channels of a pixbuf.
 *
 * Return value: Number of channels.
 **/
int
gdk_pixbuf_get_n_channels (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->n_channels;
}

/**
 * gdk_pixbuf_get_has_alpha:
 * @pixbuf: A pixbuf.
 *
 * Queries whether a pixbuf has an alpha channel (opacity information).
 *
 * Return value: %TRUE if it has an alpha channel, %FALSE otherwise.
 **/
gboolean
gdk_pixbuf_get_has_alpha (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), FALSE);

	return pixbuf->has_alpha ? TRUE : FALSE;
}

/**
 * gdk_pixbuf_get_bits_per_sample:
 * @pixbuf: A pixbuf.
 *
 * Queries the number of bits per color sample in a pixbuf.
 *
 * Return value: Number of bits per color sample.
 **/
int
gdk_pixbuf_get_bits_per_sample (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->bits_per_sample;
}

/**
 * gdk_pixbuf_get_pixels:
 * @pixbuf: A pixbuf.
 *
 * Queries a pointer to the pixel data of a pixbuf.
 *
 * Return value: (array): A pointer to the pixbuf's pixel data.
 * Please see the section on [image data](image-data) for information
 * about how the pixel data is stored in memory.
 *
 * This function will cause an implicit copy of the pixbuf data if the
 * pixbuf was created from read-only data.
 **/
guchar *
gdk_pixbuf_get_pixels (const GdkPixbuf *pixbuf)
{
        return gdk_pixbuf_get_pixels_with_length (pixbuf, NULL);
}

/**
 * gdk_pixbuf_get_pixels_with_length:
 * @pixbuf: A pixbuf.
 * @length: (out): The length of the binary data.
 *
 * Queries a pointer to the pixel data of a pixbuf.
 *
 * Return value: (array length=length): A pointer to the pixbuf's
 * pixel data.  Please see the section on [image data](image-data)
 * for information about how the pixel data is stored in memory.
 *
 * This function will cause an implicit copy of the pixbuf data if the
 * pixbuf was created from read-only data.
 *
 * Rename to: gdk_pixbuf_get_pixels
 *
 * Since: 2.26
 */
guchar *
gdk_pixbuf_get_pixels_with_length (const GdkPixbuf *pixbuf,
                                   guint           *length)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

        if (pixbuf->bytes) {
                GdkPixbuf *mut_pixbuf = (GdkPixbuf*)pixbuf;
                gsize len;
                mut_pixbuf->pixels = g_bytes_unref_to_data (pixbuf->bytes, &len);
                mut_pixbuf->bytes = NULL;
        }

        if (length)
                *length = gdk_pixbuf_get_byte_length (pixbuf);

	return pixbuf->pixels;
}

/**
 * gdk_pixbuf_read_pixels:
 * @pixbuf: A pixbuf
 *
 * Returns a read-only pointer to the raw pixel data; must not be
 * modified.  This function allows skipping the implicit copy that
 * must be made if gdk_pixbuf_get_pixels() is called on a read-only
 * pixbuf.
 *
 * Since: 2.32
 */
const guint8*
gdk_pixbuf_read_pixels (const GdkPixbuf  *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
        
        if (pixbuf->bytes) {
                gsize len;
                /* Ignore len; callers know the size via other variables */
                return g_bytes_get_data (pixbuf->bytes, &len);
        } else {
                return pixbuf->pixels;
        }
}

/**
 * gdk_pixbuf_read_pixel_bytes:
 * @pixbuf: A pixbuf
 *
 * Returns: (transfer full): A new reference to a read-only copy of
 * the pixel data.  Note that for mutable pixbufs, this function will
 * incur a one-time copy of the pixel data for conversion into the
 * returned #GBytes.
 *
 * Since: 2.32
 */
GBytes *
gdk_pixbuf_read_pixel_bytes (const GdkPixbuf  *pixbuf)
{
        g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

        if (pixbuf->bytes) {
                return g_bytes_ref (pixbuf->bytes);
        } else {
                return g_bytes_new (pixbuf->pixels,
                                    gdk_pixbuf_get_byte_length (pixbuf));
        }
}

/**
 * gdk_pixbuf_get_width:
 * @pixbuf: A pixbuf.
 *
 * Queries the width of a pixbuf.
 *
 * Return value: Width in pixels.
 **/
int
gdk_pixbuf_get_width (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->width;
}

/**
 * gdk_pixbuf_get_height:
 * @pixbuf: A pixbuf.
 *
 * Queries the height of a pixbuf.
 *
 * Return value: Height in pixels.
 **/
int
gdk_pixbuf_get_height (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->height;
}

/**
 * gdk_pixbuf_get_rowstride:
 * @pixbuf: A pixbuf.
 *
 * Queries the rowstride of a pixbuf, which is the number of bytes between
 * the start of a row and the start of the next row.
 *
 * Return value: Distance between row starts.
 **/
int
gdk_pixbuf_get_rowstride (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->rowstride;
}

/**
 * gdk_pixbuf_get_byte_length:
 * @pixbuf: A pixbuf
 *
 * Returns the length of the pixel data, in bytes.
 *
 * Return value: The length of the pixel data.
 *
 * Since: 2.26
 */
gsize
gdk_pixbuf_get_byte_length (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

        return ((pixbuf->height - 1) * pixbuf->rowstride +
                pixbuf->width * ((pixbuf->n_channels * pixbuf->bits_per_sample + 7) / 8));
}



/* General initialization hooks */
const guint gdk_pixbuf_major_version = GDK_PIXBUF_MAJOR;
const guint gdk_pixbuf_minor_version = GDK_PIXBUF_MINOR;
const guint gdk_pixbuf_micro_version = GDK_PIXBUF_MICRO;

const char *gdk_pixbuf_version = GDK_PIXBUF_VERSION;

/* Error quark */
GQuark
gdk_pixbuf_error_quark (void)
{
  return g_quark_from_static_string ("gdk-pixbuf-error-quark");
}

/**
 * gdk_pixbuf_fill:
 * @pixbuf: a #GdkPixbuf
 * @pixel: RGBA pixel to clear to
 *         (0xffffffff is opaque white, 0x00000000 transparent black)
 *
 * Clears a pixbuf to the given RGBA value, converting the RGBA value into
 * the pixbuf's pixel format. The alpha will be ignored if the pixbuf
 * doesn't have an alpha channel.
 * 
 **/
void
gdk_pixbuf_fill (GdkPixbuf *pixbuf,
                 guint32    pixel)
{
        guchar *pixels;
        guint r, g, b, a;
        guchar *p;
        guint w, h;

        g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

        if (pixbuf->width == 0 || pixbuf->height == 0)
                return;

        /* Force an implicit copy */
        pixels = gdk_pixbuf_get_pixels (pixbuf);

        r = (pixel & 0xff000000) >> 24;
        g = (pixel & 0x00ff0000) >> 16;
        b = (pixel & 0x0000ff00) >> 8;
        a = (pixel & 0x000000ff);

        h = pixbuf->height;
        
        while (h--) {
                w = pixbuf->width;
                p = pixels;

                switch (pixbuf->n_channels) {
                case 3:
                        while (w--) {
                                p[0] = r;
                                p[1] = g;
                                p[2] = b;
                                p += 3;
                        }
                        break;
                case 4:
                        while (w--) {
                                p[0] = r;
                                p[1] = g;
                                p[2] = b;
                                p[3] = a;
                                p += 4;
                        }
                        break;
                default:
                        break;
                }
                
                pixels += pixbuf->rowstride;
        }
}



/**
 * gdk_pixbuf_get_option:
 * @pixbuf: a #GdkPixbuf
 * @key: a nul-terminated string.
 * 
 * Looks up @key in the list of options that may have been attached to the
 * @pixbuf when it was loaded, or that may have been attached by another
 * function using gdk_pixbuf_set_option().
 *
 * For instance, the ANI loader provides "Title" and "Artist" options. 
 * The ICO, XBM, and XPM loaders provide "x_hot" and "y_hot" hot-spot 
 * options for cursor definitions. The PNG loader provides the tEXt ancillary
 * chunk key/value pairs as options. Since 2.12, the TIFF and JPEG loaders
 * return an "orientation" option string that corresponds to the embedded 
 * TIFF/Exif orientation tag (if present).
 * 
 * Return value: the value associated with @key. This is a nul-terminated 
 * string that should not be freed or %NULL if @key was not found.
 **/
const gchar *
gdk_pixbuf_get_option (GdkPixbuf   *pixbuf,
                       const gchar *key)
{
        gchar **options;
        gint i;

        g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
        g_return_val_if_fail (key != NULL, NULL);
  
        options = g_object_get_qdata (G_OBJECT (pixbuf), 
                                      g_quark_from_static_string ("gdk_pixbuf_options"));
        if (options) {
                for (i = 0; options[2*i]; i++) {
                        if (strcmp (options[2*i], key) == 0)
                                return options[2*i+1];
                }
        }
        
        return NULL;
}

/**
 * gdk_pixbuf_set_option:
 * @pixbuf: a #GdkPixbuf
 * @key: a nul-terminated string.
 * @value: a nul-terminated string.
 * 
 * Attaches a key/value pair as an option to a #GdkPixbuf. If @key already
 * exists in the list of options attached to @pixbuf, the new value is 
 * ignored and %FALSE is returned.
 *
 * Return value: %TRUE on success.
 *
 * Since: 2.2
 **/
gboolean
gdk_pixbuf_set_option (GdkPixbuf   *pixbuf,
                       const gchar *key,
                       const gchar *value)
{
        GQuark  quark;
        gchar **options;
        gint n = 0;
 
        g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), FALSE);
        g_return_val_if_fail (key != NULL, FALSE);
        g_return_val_if_fail (value != NULL, FALSE);

        quark = g_quark_from_static_string ("gdk_pixbuf_options");

        options = g_object_get_qdata (G_OBJECT (pixbuf), quark);

        if (options) {
                for (n = 0; options[2*n]; n++) {
                        if (strcmp (options[2*n], key) == 0)
                                return FALSE;
                }

                g_object_steal_qdata (G_OBJECT (pixbuf), quark);
                options = g_renew (gchar *, options, 2*(n+1) + 1);
        } else {
                options = g_new (gchar *, 3);
        }
        
        options[2*n]   = g_strdup (key);
        options[2*n+1] = g_strdup (value);
        options[2*n+2] = NULL;

        g_object_set_qdata_full (G_OBJECT (pixbuf), quark,
                                 options, (GDestroyNotify) g_strfreev);
        
        return TRUE;
}

static void
gdk_pixbuf_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GdkPixbuf *pixbuf = GDK_PIXBUF (object);

  switch (prop_id)
          {
          case PROP_COLORSPACE:
                  pixbuf->colorspace = g_value_get_enum (value);
                  break;
          case PROP_N_CHANNELS:
                  pixbuf->n_channels = g_value_get_int (value);
                  break;
          case PROP_HAS_ALPHA:
                  pixbuf->has_alpha = g_value_get_boolean (value);
                  break;
          case PROP_BITS_PER_SAMPLE:
                  pixbuf->bits_per_sample = g_value_get_int (value);
                  break;
          case PROP_WIDTH:
                  pixbuf->width = g_value_get_int (value);
                  break;
          case PROP_HEIGHT:
                  pixbuf->height = g_value_get_int (value);
                  break;
          case PROP_ROWSTRIDE:
                  pixbuf->rowstride = g_value_get_int (value);
                  break;
          case PROP_PIXELS:
                  pixbuf->pixels = (guchar *) g_value_get_pointer (value);
                  break;
          case PROP_PIXEL_BYTES:
                  pixbuf->bytes = g_value_dup_boxed (value);
                  break;
          default:
                  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                  break;
          }
}

static void
gdk_pixbuf_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
  GdkPixbuf *pixbuf = GDK_PIXBUF (object);
  
  switch (prop_id)
          {
          case PROP_COLORSPACE:
                  g_value_set_enum (value, gdk_pixbuf_get_colorspace (pixbuf));
                  break;
          case PROP_N_CHANNELS:
                  g_value_set_int (value, gdk_pixbuf_get_n_channels (pixbuf));
                  break;
          case PROP_HAS_ALPHA:
                  g_value_set_boolean (value, gdk_pixbuf_get_has_alpha (pixbuf));
                  break;
          case PROP_BITS_PER_SAMPLE:
                  g_value_set_int (value, gdk_pixbuf_get_bits_per_sample (pixbuf));
                  break;
          case PROP_WIDTH:
                  g_value_set_int (value, gdk_pixbuf_get_width (pixbuf));
                  break;
          case PROP_HEIGHT:
                  g_value_set_int (value, gdk_pixbuf_get_height (pixbuf));
                  break;
          case PROP_ROWSTRIDE:
                  g_value_set_int (value, gdk_pixbuf_get_rowstride (pixbuf));
                  break;
          case PROP_PIXELS:
                  g_value_set_pointer (value, gdk_pixbuf_get_pixels (pixbuf));
                  break;
          case PROP_PIXEL_BYTES:
                  g_value_set_boxed (value, pixbuf->bytes);
                  break;
          default:
                  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                  break;
          }
}
