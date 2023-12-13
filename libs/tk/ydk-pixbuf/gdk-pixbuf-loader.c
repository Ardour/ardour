/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Progressive loader object
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
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

#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-animation.h"
#include "gdk-pixbuf-scaled-anim.h"
#include "gdk-pixbuf-loader.h"
#include "gdk-pixbuf-marshal.h"

/**
 * SECTION:gdk-pixbuf-loader
 * @Short_description: Application-driven progressive image loading.
 * @Title: GdkPixbufLoader
 * @See_also: gdk_pixbuf_new_from_file(), gdk_pixbuf_animation_new_from_file()
 * 
 * #GdkPixbufLoader provides a way for applications to drive the
 * process of loading an image, by letting them send the image data
 * directly to the loader instead of having the loader read the data
 * from a file.  Applications can use this functionality instead of
 * gdk_pixbuf_new_from_file() or gdk_pixbuf_animation_new_from_file() 
 * when they need to parse image data in
 * small chunks.  For example, it should be used when reading an
 * image from a (potentially) slow network connection, or when
 * loading an extremely large file.
 * 
 * 
 * To use #GdkPixbufLoader to load an image, just create a new one, and
 * call gdk_pixbuf_loader_write() to send the data to it.  When done,
 * gdk_pixbuf_loader_close() should be called to end the stream and
 * finalize everything. The loader will emit three important signals
 * throughout the process. The first, #GdkPixbufLoader::size-prepared,
 * will be emitted as soon as the image has enough information to
 * determine the size of the image to be used. If you want to scale
 * the image while loading it, you can call gdk_pixbuf_loader_set_size()
 * in response to this signal.
 * 
 * 
 * The second signal, #GdkPixbufLoader::area-prepared, will be emitted as
 * soon as the pixbuf of the desired has been allocated. You can obtain it
 * by calling gdk_pixbuf_loader_get_pixbuf(). If you want to use it, simply
 * ref it.  In addition, no actual information will be passed in yet, so the
 * pixbuf can be safely filled with any temporary graphics (or an initial
 * color) as needed.  You can also call gdk_pixbuf_loader_get_pixbuf() later
 * and get the same pixbuf.
 * 
 * The last signal, #GdkPixbufLoader::area-updated, gets emitted every time
 * a region is updated. This way you can update a partially completed image.
 * Note that you do not know anything about the completeness of an image
 * from the updated area. For example, in an interlaced image, you need to
 * make several passes before the image is done loading.
 * 
 * # Loading an animation 
 *
 * Loading an animation is almost as easy as loading an image. Once the first
 * #GdkPixbufLoader::area-prepared signal has been emitted, you can call
 * gdk_pixbuf_loader_get_animation() to get the #GdkPixbufAnimation struct
 * and gdk_pixbuf_animation_get_iter() to get a #GdkPixbufAnimationIter for
 * displaying it. 
 */


enum {
        SIZE_PREPARED,
        AREA_PREPARED,
        AREA_UPDATED,
        CLOSED,
        LAST_SIGNAL
};


static void gdk_pixbuf_loader_finalize (GObject *loader);

static guint    pixbuf_loader_signals[LAST_SIGNAL] = { 0 };

/* Internal data */

typedef struct
{
        GdkPixbufAnimation *animation;
        gboolean closed;
        guchar header_buf[SNIFF_BUFFER_SIZE];
        gint header_buf_offset;
        GdkPixbufModule *image_module;
        gpointer context;
        gint width;
        gint height;
        gboolean size_fixed;
        gboolean needs_scale;
	gchar *filename;
} GdkPixbufLoaderPrivate;

G_DEFINE_TYPE (GdkPixbufLoader, gdk_pixbuf_loader, G_TYPE_OBJECT)


static void
gdk_pixbuf_loader_class_init (GdkPixbufLoaderClass *class)
{
        GObjectClass *object_class;
  
        object_class = (GObjectClass *) class;
  
        object_class->finalize = gdk_pixbuf_loader_finalize;

        /**
         * GdkPixbufLoader::size-prepared:
         * @loader: the object which received the signal.
         * @width: the original width of the image
         * @height: the original height of the image
         *
         * This signal is emitted when the pixbuf loader has been fed the
         * initial amount of data that is required to figure out the size
         * of the image that it will create.  Applications can call  
         * gdk_pixbuf_loader_set_size() in response to this signal to set
         * the desired size to which the image should be scaled.
         */
        pixbuf_loader_signals[SIZE_PREPARED] =
                g_signal_new ("size-prepared",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, size_prepared),
                              NULL, NULL,
                              _gdk_pixbuf_marshal_VOID__INT_INT,
                              G_TYPE_NONE, 2, 
                              G_TYPE_INT,
                              G_TYPE_INT);
  
        /**
         * GdkPixbufLoader::area-prepared:
         * @loader: the object which received the signal.
         *
         * This signal is emitted when the pixbuf loader has allocated the 
         * pixbuf in the desired size.  After this signal is emitted, 
         * applications can call gdk_pixbuf_loader_get_pixbuf() to fetch 
         * the partially-loaded pixbuf.
         */
        pixbuf_loader_signals[AREA_PREPARED] =
                g_signal_new ("area-prepared",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, area_prepared),
                              NULL, NULL,
                              _gdk_pixbuf_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        /**
         * GdkPixbufLoader::area-updated:
         * @loader: the object which received the signal.
         * @x: X offset of upper-left corner of the updated area.
         * @y: Y offset of upper-left corner of the updated area.
         * @width: Width of updated area.
         * @height: Height of updated area.
         *
         * This signal is emitted when a significant area of the image being
         * loaded has been updated.  Normally it means that a complete
         * scanline has been read in, but it could be a different area as
         * well.  Applications can use this signal to know when to repaint
         * areas of an image that is being loaded.
         */
        pixbuf_loader_signals[AREA_UPDATED] =
                g_signal_new ("area-updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, area_updated),
                              NULL, NULL,
                              _gdk_pixbuf_marshal_VOID__INT_INT_INT_INT,
                              G_TYPE_NONE, 4,
                              G_TYPE_INT,
                              G_TYPE_INT,
                              G_TYPE_INT,
                              G_TYPE_INT);
  
        /**
         * GdkPixbufLoader::closed:
         * @loader: the object which received the signal.
         *
         * This signal is emitted when gdk_pixbuf_loader_close() is called.
         * It can be used by different parts of an application to receive
         * notification when an image loader is closed by the code that
         * drives it.
         */
        pixbuf_loader_signals[CLOSED] =
                g_signal_new ("closed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, closed),
                              NULL, NULL,
                              _gdk_pixbuf_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
gdk_pixbuf_loader_init (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        priv = g_new0 (GdkPixbufLoaderPrivate, 1);
        priv->width = -1;
        priv->height = -1;

        loader->priv = priv;
}

static void
gdk_pixbuf_loader_finalize (GObject *object)
{
        GdkPixbufLoader *loader;
        GdkPixbufLoaderPrivate *priv = NULL;
  
        loader = GDK_PIXBUF_LOADER (object);
        priv = loader->priv;

        if (!priv->closed) {
                g_warning ("GdkPixbufLoader finalized without calling gdk_pixbuf_loader_close() - this is not allowed. You must explicitly end the data stream to the loader before dropping the last reference.");
        }
        if (priv->animation)
                g_object_unref (priv->animation);
  
	g_free (priv->filename);

        g_free (priv);
  
        G_OBJECT_CLASS (gdk_pixbuf_loader_parent_class)->finalize (object);
}

/**
 * gdk_pixbuf_loader_set_size:
 * @loader: A pixbuf loader.
 * @width: The desired width of the image being loaded.
 * @height: The desired height of the image being loaded.
 *
 * Causes the image to be scaled while it is loaded. The desired
 * image size can be determined relative to the original size of
 * the image by calling gdk_pixbuf_loader_set_size() from a
 * signal handler for the ::size-prepared signal.
 *
 * Attempts to set the desired image size  are ignored after the 
 * emission of the ::size-prepared signal.
 *
 * Since: 2.2
 */
void 
gdk_pixbuf_loader_set_size (GdkPixbufLoader *loader,
			    gint             width,
			    gint             height)
{
        GdkPixbufLoaderPrivate *priv;

        g_return_if_fail (GDK_IS_PIXBUF_LOADER (loader));
        g_return_if_fail (width >= 0 && height >= 0);

        priv = GDK_PIXBUF_LOADER (loader)->priv;

        if (!priv->size_fixed) 
                {
                        priv->width = width;
                        priv->height = height;
                }
}

static void
gdk_pixbuf_loader_size_func (gint *width, gint *height, gpointer loader)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;

        /* allow calling gdk_pixbuf_loader_set_size() before the signal */
        if (priv->width == -1 && priv->height == -1) 
                {
                        priv->width = *width;
                        priv->height = *height;
                }

        g_signal_emit (loader, pixbuf_loader_signals[SIZE_PREPARED], 0, *width, *height);
        priv->size_fixed = TRUE;

        *width = priv->width;
        *height = priv->height;
}

static void
gdk_pixbuf_loader_prepare (GdkPixbuf          *pixbuf,
                           GdkPixbufAnimation *anim,
			   gpointer            loader)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;
        gint width, height;
        g_return_if_fail (pixbuf != NULL);

        width = anim ? gdk_pixbuf_animation_get_width (anim) :
                gdk_pixbuf_get_width (pixbuf);
        height = anim ? gdk_pixbuf_animation_get_height (anim) :
                gdk_pixbuf_get_height (pixbuf);

        if (!priv->size_fixed) 
                {
			gint w = width;
			gint h = height;
                        /* Defend against lazy loaders which don't call size_func */
                        gdk_pixbuf_loader_size_func (&w, &h, loader);
                }

        priv->needs_scale = FALSE;
        if (priv->width > 0 && priv->height > 0 &&
            (priv->width != width || priv->height != height))
                priv->needs_scale = TRUE;

        if (anim)
                g_object_ref (anim);
        else
                anim = gdk_pixbuf_non_anim_new (pixbuf);
  
	if (priv->needs_scale) {
		priv->animation  = GDK_PIXBUF_ANIMATION (_gdk_pixbuf_scaled_anim_new (anim,
                                         (double) priv->width / width,
                                         (double) priv->height / height,
					  1.0));
			g_object_unref (anim);
	}
	else
        	priv->animation = anim;
  
        if (!priv->needs_scale)
                g_signal_emit (loader, pixbuf_loader_signals[AREA_PREPARED], 0);
}

static void
gdk_pixbuf_loader_update (GdkPixbuf *pixbuf,
			  gint       x,
			  gint       y,
			  gint       width,
			  gint       height,
			  gpointer   loader)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;
  
        if (!priv->needs_scale)
                g_signal_emit (loader,
                               pixbuf_loader_signals[AREA_UPDATED],
                               0,
                               x, y,
                               /* sanity check in here.  Defend against an errant loader */
                               MIN (width, gdk_pixbuf_animation_get_width (priv->animation)),
                               MIN (height, gdk_pixbuf_animation_get_height (priv->animation)));
}

/* Defense against broken loaders; DO NOT take this as a GError example! */
static void
gdk_pixbuf_loader_ensure_error (GdkPixbufLoader *loader,
                                GError         **error)
{ 
        GdkPixbufLoaderPrivate *priv = loader->priv;

        if (error == NULL || *error != NULL)
                return;

        g_warning ("Bug! loader '%s' didn't set an error on failure",
                   priv->image_module->module_name);
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Internal error: Image loader module '%s' failed to"
                       " complete an operation, but didn't give a reason for"
                       " the failure"),
                     priv->image_module->module_name);
}

static gint
gdk_pixbuf_loader_load_module (GdkPixbufLoader *loader,
                               const char      *image_type,
                               GError         **error)
{
        GdkPixbufLoaderPrivate *priv = loader->priv;

        if (image_type)
                {
                        priv->image_module = _gdk_pixbuf_get_named_module (image_type,
                                                                           error);
                }
        else
                {
                        priv->image_module = _gdk_pixbuf_get_module (priv->header_buf,
                                                                     priv->header_buf_offset,
                                                                     priv->filename,
                                                                     error);
                }
  
        if (priv->image_module == NULL)
                return 0;
  
        if (!_gdk_pixbuf_load_module (priv->image_module, error))
                return 0;
  
        if (priv->image_module->module == NULL)
                return 0;
  
        if ((priv->image_module->begin_load == NULL) ||
            (priv->image_module->stop_load == NULL) ||
            (priv->image_module->load_increment == NULL))
                {
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION,
                                     _("Incremental loading of image type '%s' is not supported"),
                                     priv->image_module->module_name);

                        return 0;
                }

        priv->context = priv->image_module->begin_load (gdk_pixbuf_loader_size_func,
                                                        gdk_pixbuf_loader_prepare,
                                                        gdk_pixbuf_loader_update,
                                                        loader,
                                                        error);
  
        if (priv->context == NULL)
                {
                        gdk_pixbuf_loader_ensure_error (loader, error);
                        return 0;
                }
  
        if (priv->header_buf_offset
            && priv->image_module->load_increment (priv->context, priv->header_buf, priv->header_buf_offset, error))
                return priv->header_buf_offset;
  
        return 0;
}

static int
gdk_pixbuf_loader_eat_header_write (GdkPixbufLoader *loader,
				    const guchar    *buf,
				    gsize            count,
                                    GError         **error)
{
        gint n_bytes;
        GdkPixbufLoaderPrivate *priv = loader->priv;
  
        n_bytes = MIN(SNIFF_BUFFER_SIZE - priv->header_buf_offset, count);
        memcpy (priv->header_buf + priv->header_buf_offset, buf, n_bytes);
  
        priv->header_buf_offset += n_bytes;
  
        if (priv->header_buf_offset >= SNIFF_BUFFER_SIZE)
                {
                        if (gdk_pixbuf_loader_load_module (loader, NULL, error) == 0)
                                return 0;
                }
  
        return n_bytes;
}

/**
 * gdk_pixbuf_loader_write:
 * @loader: A pixbuf loader.
 * @buf: (array length=count): Pointer to image data.
 * @count: Length of the @buf buffer in bytes.
 * @error: return location for errors
 *
 * This will cause a pixbuf loader to parse the next @count bytes of
 * an image.  It will return %TRUE if the data was loaded successfully,
 * and %FALSE if an error occurred.  In the latter case, the loader
 * will be closed, and will not accept further writes. If %FALSE is
 * returned, @error will be set to an error from the #GDK_PIXBUF_ERROR
 * or #G_FILE_ERROR domains.
 *
 * Return value: %TRUE if the write was successful, or %FALSE if the loader
 * cannot parse the buffer.
 **/
gboolean
gdk_pixbuf_loader_write (GdkPixbufLoader *loader,
			 const guchar    *buf,
			 gsize            count,
                         GError         **error)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), FALSE);
  
        g_return_val_if_fail (buf != NULL, FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  
        priv = loader->priv;

        /* we expect it's not to be closed */
        g_return_val_if_fail (priv->closed == FALSE, FALSE);
  
        if (count > 0 && priv->image_module == NULL)
                {
                        gint eaten;
      
                        eaten = gdk_pixbuf_loader_eat_header_write (loader, buf, count, error);
                        if (eaten <= 0)
                               goto fail; 
      
                        count -= eaten;
                        buf += eaten;
                }
  
        if (count > 0 && priv->image_module->load_increment)
                {
                        if (!priv->image_module->load_increment (priv->context, buf, count,
                                                                 error))
				goto fail;
                }
      
        return TRUE;

 fail:
        gdk_pixbuf_loader_ensure_error (loader, error);
        gdk_pixbuf_loader_close (loader, NULL);

        return FALSE;
}

/**
 * gdk_pixbuf_loader_write_bytes:
 * @loader: A pixbuf loader.
 * @buffer: The image data as a #GBytes
 * @error: return location for errors
 *
 * This will cause a pixbuf loader to parse a buffer inside a #GBytes
 * for an image.  It will return %TRUE if the data was loaded successfully,
 * and %FALSE if an error occurred.  In the latter case, the loader
 * will be closed, and will not accept further writes. If %FALSE is
 * returned, @error will be set to an error from the #GDK_PIXBUF_ERROR
 * or #G_FILE_ERROR domains.
 *
 * See also: gdk_pixbuf_loader_write()
 *
 * Return value: %TRUE if the write was successful, or %FALSE if the loader
 * cannot parse the buffer.
 *
 * Since: 2.30
 */
gboolean
gdk_pixbuf_loader_write_bytes (GdkPixbufLoader *loader,
                               GBytes          *buffer,
                               GError         **error)
{
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), FALSE);

        g_return_val_if_fail (buffer != NULL, FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        return gdk_pixbuf_loader_write (loader,
                                        g_bytes_get_data (buffer, NULL),
                                        g_bytes_get_size (buffer),
                                        error);
}

/**
 * gdk_pixbuf_loader_new:
 *
 * Creates a new pixbuf loader object.
 *
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new (void)
{
        return g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);
}

/**
 * gdk_pixbuf_loader_new_with_type:
 * @image_type: name of the image format to be loaded with the image
 * @error: (allow-none): return location for an allocated #GError, or %NULL to ignore errors
 *
 * Creates a new pixbuf loader object that always attempts to parse
 * image data as if it were an image of type @image_type, instead of
 * identifying the type automatically. Useful if you want an error if
 * the image isn't the expected type, for loading image formats
 * that can't be reliably identified by looking at the data, or if
 * the user manually forces a specific type.
 *
 * The list of supported image formats depends on what image loaders
 * are installed, but typically "png", "jpeg", "gif", "tiff" and 
 * "xpm" are among the supported formats. To obtain the full list of
 * supported image formats, call gdk_pixbuf_format_get_name() on each 
 * of the #GdkPixbufFormat structs returned by gdk_pixbuf_get_formats().
 *
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new_with_type (const char *image_type,
                                 GError    **error)
{
        GdkPixbufLoader *retval;
        GError *tmp;
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  
        retval = g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);

        tmp = NULL;
        gdk_pixbuf_loader_load_module (retval, image_type, &tmp);
        if (tmp != NULL)
                {
                        g_propagate_error (error, tmp);
                        gdk_pixbuf_loader_close (retval, NULL);
                        g_object_unref (retval);
                        return NULL;
                }

        return retval;
}

/**
 * gdk_pixbuf_loader_new_with_mime_type:
 * @mime_type: the mime type to be loaded 
 * @error: (allow-none): return location for an allocated #GError, or %NULL to ignore errors
 *
 * Creates a new pixbuf loader object that always attempts to parse
 * image data as if it were an image of mime type @mime_type, instead of
 * identifying the type automatically. Useful if you want an error if
 * the image isn't the expected mime type, for loading image formats
 * that can't be reliably identified by looking at the data, or if
 * the user manually forces a specific mime type.
 *
 * The list of supported mime types depends on what image loaders
 * are installed, but typically "image/png", "image/jpeg", "image/gif", 
 * "image/tiff" and "image/x-xpixmap" are among the supported mime types. 
 * To obtain the full list of supported mime types, call 
 * gdk_pixbuf_format_get_mime_types() on each of the #GdkPixbufFormat 
 * structs returned by gdk_pixbuf_get_formats().
 *
 * Return value: A newly-created pixbuf loader.
 * Since: 2.4
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new_with_mime_type (const char *mime_type,
                                      GError    **error)
{
        const char * image_type = NULL;
        char ** mimes;

        GdkPixbufLoader *retval;
        GError *tmp;
  
        GSList * formats;
        GdkPixbufFormat *info;
        int i, j, length;

        formats = gdk_pixbuf_get_formats ();
        length = g_slist_length (formats);

        for (i = 0; i < length && image_type == NULL; i++) {
                info = (GdkPixbufFormat *)g_slist_nth_data (formats, i);
                mimes = info->mime_types;
                
                for (j = 0; mimes[j] != NULL; j++)
                        if (g_ascii_strcasecmp (mimes[j], mime_type) == 0) {
                                image_type = info->name;
                                break;
                        }
        }

        g_slist_free (formats);

        retval = g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);

        tmp = NULL;
        gdk_pixbuf_loader_load_module (retval, image_type, &tmp);
        if (tmp != NULL)
                {
                        g_propagate_error (error, tmp);
                        gdk_pixbuf_loader_close (retval, NULL);
                        g_object_unref (retval);
                        return NULL;
                }

        return retval;
}

GdkPixbufLoader *
_gdk_pixbuf_loader_new_with_filename (const char *filename)
{
	GdkPixbufLoader *retval;
        GdkPixbufLoaderPrivate *priv;

        retval = g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);
	priv = retval->priv;
	priv->filename = g_strdup (filename);

	return retval;
}

/**
 * gdk_pixbuf_loader_get_pixbuf:
 * @loader: A pixbuf loader.
 *
 * Queries the #GdkPixbuf that a pixbuf loader is currently creating.
 * In general it only makes sense to call this function after the
 * "area-prepared" signal has been emitted by the loader; this means
 * that enough data has been read to know the size of the image that
 * will be allocated.  If the loader has not received enough data via
 * gdk_pixbuf_loader_write(), then this function returns %NULL.  The
 * returned pixbuf will be the same in all future calls to the loader,
 * so simply calling g_object_ref() should be sufficient to continue
 * using it.  Additionally, if the loader is an animation, it will
 * return the "static image" of the animation
 * (see gdk_pixbuf_animation_get_static_image()).
 * 
 * Return value: (transfer none): The #GdkPixbuf that the loader is creating, or %NULL if not
 * enough data has been read to determine how to create the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_loader_get_pixbuf (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
        priv = loader->priv;

        if (priv->animation)
                return gdk_pixbuf_animation_get_static_image (priv->animation);
        else
                return NULL;
}

/**
 * gdk_pixbuf_loader_get_animation:
 * @loader: A pixbuf loader
 *
 * Queries the #GdkPixbufAnimation that a pixbuf loader is currently creating.
 * In general it only makes sense to call this function after the "area-prepared"
 * signal has been emitted by the loader. If the loader doesn't have enough
 * bytes yet (hasn't emitted the "area-prepared" signal) this function will 
 * return %NULL.
 *
 * Return value: (transfer none): The #GdkPixbufAnimation that the loader is loading, or %NULL if
 not enough data has been read to determine the information.
**/
GdkPixbufAnimation *
gdk_pixbuf_loader_get_animation (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
        priv = loader->priv;
  
        return priv->animation;
}

/**
 * gdk_pixbuf_loader_close:
 * @loader: A pixbuf loader.
 * @error: (allow-none): return location for a #GError, or %NULL to ignore errors
 *
 * Informs a pixbuf loader that no further writes with
 * gdk_pixbuf_loader_write() will occur, so that it can free its
 * internal loading structures. Also, tries to parse any data that
 * hasn't yet been parsed; if the remaining data is partial or
 * corrupt, an error will be returned.  If %FALSE is returned, @error
 * will be set to an error from the #GDK_PIXBUF_ERROR or #G_FILE_ERROR
 * domains. If you're just cancelling a load rather than expecting it
 * to be finished, passing %NULL for @error to ignore it is
 * reasonable.
 *
 * Remember that this does not unref the loader, so if you plan not to
 * use it anymore, please g_object_unref() it.
 *
 * Returns: %TRUE if all image data written so far was successfully
            passed out via the update_area signal
 **/
gboolean
gdk_pixbuf_loader_close (GdkPixbufLoader *loader,
                         GError         **error)
{
        GdkPixbufLoaderPrivate *priv;
        gboolean retval = TRUE;
  
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), TRUE);
        g_return_val_if_fail (error == NULL || *error == NULL, TRUE);
  
        priv = loader->priv;
  
        if (priv->closed)
                return TRUE;
  
        /* We have less than SNIFF_BUFFER_SIZE bytes in the image.  
         * Flush it, and keep going. 
         */
        if (priv->image_module == NULL)
                {
                        GError *tmp = NULL;
                        gdk_pixbuf_loader_load_module (loader, NULL, &tmp);
                        if (tmp != NULL)
                                {
                                        g_propagate_error (error, tmp);
                                        retval = FALSE;
                                }
                }  

        if (priv->image_module && priv->image_module->stop_load && priv->context) 
                {
                        GError *tmp = NULL;
                        if (!priv->image_module->stop_load (priv->context, &tmp) || tmp)
                                {
					/* don't call gdk_pixbuf_loader_ensure_error()
 					 * here, since we might not get an error in the
 					 * gdk_pixbuf_get_file_info() case
 					 */
					if (tmp) {
						if (error && *error == NULL)
							g_propagate_error (error, tmp);
						else
							g_error_free (tmp);
					}
                                        retval = FALSE;
                                }
                }
  
        priv->closed = TRUE;

        if (priv->needs_scale) 
                {

                        g_signal_emit (loader, pixbuf_loader_signals[AREA_PREPARED], 0);
                        g_signal_emit (loader, pixbuf_loader_signals[AREA_UPDATED], 0, 
                                       0, 0, priv->width, priv->height);
                }

        
        g_signal_emit (loader, pixbuf_loader_signals[CLOSED], 0);

        return retval;
}

/**
 * gdk_pixbuf_loader_get_format:
 * @loader: A pixbuf loader.
 *
 * Obtains the available information about the format of the 
 * currently loading image file.
 *
 * Returns: (nullable) (transfer none): A #GdkPixbufFormat or
 * %NULL. The return value is owned by GdkPixbuf and should not be
 * freed.
 * 
 * Since: 2.2
 */
GdkPixbufFormat *
gdk_pixbuf_loader_get_format (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
        priv = loader->priv;

        if (priv->image_module)
                return _gdk_pixbuf_get_format (priv->image_module);
        else
                return NULL;
}
