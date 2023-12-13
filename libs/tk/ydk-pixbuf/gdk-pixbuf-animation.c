/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Simple animation support
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Jonathan Blandford <jrb@redhat.com>
 *          Havoc Pennington <hp@redhat.com>
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
#include <errno.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-animation.h"
#include "gdk-pixbuf-loader.h"

#include <glib/gstdio.h>

/**
 * SECTION:animation
 * @Short_description: Animated images.
 * @Title: Animations
 * @See_also: #GdkPixbufLoader.
 * 
 * The GdkPixBuf library provides a simple mechanism to load and
 * represent animations. An animation is conceptually a series of
 * frames to be displayed over time. The animation may not be
 * represented as a series of frames internally; for example, it may
 * be stored as a sprite and instructions for moving the sprite around
 * a background. To display an animation you don't need to understand
 * its representation, however; you just ask GdkPixBuf what should
 * be displayed at a given point in time.
 * 
 */

typedef struct _GdkPixbufNonAnim GdkPixbufNonAnim;
typedef struct _GdkPixbufNonAnimClass GdkPixbufNonAnimClass;

#define GDK_TYPE_PIXBUF_NON_ANIM              (gdk_pixbuf_non_anim_get_type ())
#define GDK_PIXBUF_NON_ANIM(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF_NON_ANIM, GdkPixbufNonAnim))
#define GDK_IS_PIXBUF_NON_ANIM(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_NON_ANIM))

#define GDK_PIXBUF_NON_ANIM_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_NON_ANIM, GdkPixbufNonAnimClass))
#define GDK_IS_PIXBUF_NON_ANIM_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_NON_ANIM))
#define GDK_PIXBUF_NON_ANIM_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_NON_ANIM, GdkPixbufNonAnimClass))

/* Private part of the GdkPixbufNonAnim structure */
struct _GdkPixbufNonAnim {
        GdkPixbufAnimation parent_instance;

        GdkPixbuf *pixbuf;
};

struct _GdkPixbufNonAnimClass {
        GdkPixbufAnimationClass parent_class;
        
};


typedef struct _GdkPixbufNonAnimIter GdkPixbufNonAnimIter;
typedef struct _GdkPixbufNonAnimIterClass GdkPixbufNonAnimIterClass;


#define GDK_TYPE_PIXBUF_NON_ANIM_ITER              (gdk_pixbuf_non_anim_iter_get_type ())
#define GDK_PIXBUF_NON_ANIM_ITER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF_NON_ANIM_ITER, GdkPixbufNonAnimIter))
#define GDK_IS_PIXBUF_NON_ANIM_ITER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_NON_ANIM_ITER))

#define GDK_PIXBUF_NON_ANIM_ITER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_NON_ANIM_ITER, GdkPixbufNonAnimIterClass))
#define GDK_IS_PIXBUF_NON_ANIM_ITER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_NON_ANIM_ITER))
#define GDK_PIXBUF_NON_ANIM_ITER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_NON_ANIM_ITER, GdkPixbufNonAnimIterClass))

struct _GdkPixbufNonAnimIter {
        GdkPixbufAnimationIter parent_instance;

        GdkPixbufNonAnim   *non_anim;
};

struct _GdkPixbufNonAnimIterClass {
        GdkPixbufAnimationIterClass parent_class;

};

static GType gdk_pixbuf_non_anim_iter_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GdkPixbufAnimation, gdk_pixbuf_animation, G_TYPE_OBJECT);

static void
gdk_pixbuf_animation_class_init (GdkPixbufAnimationClass *klass)
{
}

static void
gdk_pixbuf_animation_init (GdkPixbufAnimation *animation)
{
}

static void
prepared_notify (GdkPixbuf *pixbuf, 
                 GdkPixbufAnimation *anim, 
                 gpointer user_data)
{
        if (anim != NULL)
                g_object_ref (anim);
        else
                anim = gdk_pixbuf_non_anim_new (pixbuf);

        *((GdkPixbufAnimation **)user_data) = anim;
}

/**
 * gdk_pixbuf_animation_new_from_file:
 * @filename: Name of file to load, in the GLib file name encoding
 * @error: return location for error
 *
 * Creates a new animation by loading it from a file.  The file format is
 * detected automatically.  If the file's format does not support multi-frame
 * images, then an animation with a single frame will be created. Possible errors
 * are in the #GDK_PIXBUF_ERROR and #G_FILE_ERROR domains.
 *
 * Return value: A newly-created animation with a reference count of 1, or %NULL
 * if any of several error conditions ocurred:  the file could not be opened,
 * there was no loader for the file's format, there was not enough memory to
 * allocate the image buffer, or the image file contained invalid data.
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_file (const char *filename,
                                    GError    **error)
{
	GdkPixbufAnimation *animation;
	int size;
	FILE *f;
	guchar buffer [SNIFF_BUFFER_SIZE];
	GdkPixbufModule *image_module;
        gchar *display_name;

	g_return_val_if_fail (filename != NULL, NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        display_name = g_filename_display_name (filename);
	f = g_fopen (filename, "rb");
	if (!f) {
                gint save_errno = errno;
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (save_errno),
                             _("Failed to open file '%s': %s"),
                             display_name,
                             g_strerror (save_errno));
                g_free (display_name);
		return NULL;
        }

	size = fread (&buffer, 1, sizeof (buffer), f);

	if (size == 0) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image file '%s' contains no data"),
                             display_name);
                g_free (display_name);
		fclose (f);
		return NULL;
	}

	image_module = _gdk_pixbuf_get_module (buffer, size, filename, error);
	if (!image_module) {
                g_free (display_name);
		fclose (f);
		return NULL;
	}

	if (image_module->module == NULL)
                if (!_gdk_pixbuf_load_module (image_module, error)) {
                        g_free (display_name);
                        fclose (f);
                        return NULL;
                }

	if (image_module->load_animation != NULL) {
		fseek (f, 0, SEEK_SET);
		animation = (* image_module->load_animation) (f, error);

                if (animation == NULL && error != NULL && *error == NULL) {
                        /* I don't trust these crufty longjmp()'ing
                         * image libs to maintain proper error
                         * invariants, and I don't want user code to
                         * segfault as a result. We need to maintain
                         * the invariant that error gets set if NULL
                         * is returned.
                         */
                        
                        g_warning ("Bug! gdk-pixbuf loader '%s' didn't set an error on failure.",
                                   image_module->module_name);
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_FAILED,
                                     _("Failed to load animation '%s': reason not known, probably a corrupt animation file"),
                                     display_name);
                }
                
		fclose (f);
        } else if (image_module->begin_load != NULL) {
                guchar buffer[4096];
                size_t length;
                gpointer context;
                gboolean success;

                success = FALSE;
                animation = NULL;
		fseek (f, 0, SEEK_SET);

                context = image_module->begin_load (NULL, prepared_notify, NULL, &animation, error);
                if (!context)
                        goto fail_begin_load;
                
                while (!feof (f) && !ferror (f)) {
                        length = fread (buffer, 1, sizeof (buffer), f);
                        if (length > 0) {
                                if (!image_module->load_increment (context, buffer, length, error)) {
                                        error = NULL;
                                        goto fail_load_increment;
                                }
                        }
                }

                success = TRUE;

fail_load_increment:
                if (!image_module->stop_load (context, error))
                        success = FALSE;

fail_begin_load:
		fclose (f);

                if (success) {
                        /* If there was no error, there must be an animation that was successfully loaded */
                        g_assert (animation);
                } else {
                        if (animation) {
                                g_object_unref (animation);
                                animation = NULL;
                        }
                }
	} else {
		GdkPixbuf *pixbuf;

		/* Keep this logic in sync with gdk_pixbuf_new_from_file() */

		fseek (f, 0, SEEK_SET);
		pixbuf = _gdk_pixbuf_generic_image_load (image_module, f, error);
		fclose (f);

                if (pixbuf == NULL && error != NULL && *error == NULL) {
                        /* I don't trust these crufty longjmp()'ing image libs
                         * to maintain proper error invariants, and I don't
                         * want user code to segfault as a result. We need to maintain
                         * the invariant that error gets set if NULL is returned.
                         */
                        
                        g_warning ("Bug! gdk-pixbuf loader '%s' didn't set an error on failure.",
                                   image_module->module_name);
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_FAILED,
                                     _("Failed to load image '%s': reason not known, probably a corrupt image file"),
                                     display_name);
                }
                
		if (pixbuf == NULL) {
                        g_free (display_name);
                        animation = NULL;
                        goto out;
                }

                animation = gdk_pixbuf_non_anim_new (pixbuf);

                g_object_unref (pixbuf);
	}

        g_free (display_name);

 out:
	return animation;
}

#ifdef G_OS_WIN32

#undef gdk_pixbuf_animation_new_from_file

GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_file (const char *filename,
                                    GError    **error)
{
	gchar *utf8_filename =
		g_locale_to_utf8 (filename, -1, NULL, NULL, error);
	GdkPixbufAnimation *retval;

	if (utf8_filename == NULL)
		return NULL;

	retval = gdk_pixbuf_animation_new_from_file_utf8 (utf8_filename, error);

	g_free (utf8_filename);

	return retval;
}

#endif

/**
 * gdk_pixbuf_animation_new_from_stream:
 * @stream:  a #GInputStream to load the pixbuf from
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @error: Return location for an error
 *
 * Creates a new animation by loading it from an input stream.
 *
 * The file format is detected automatically. If %NULL is returned, then 
 * @error will be set. The @cancellable can be used to abort the operation
 * from another thread. If the operation was cancelled, the error 
 * %G_IO_ERROR_CANCELLED will be returned. Other possible errors are in 
 * the #GDK_PIXBUF_ERROR and %G_IO_ERROR domains. 
 *
 * The stream is not closed.
 *
 * Return value: A newly-created pixbuf, or %NULL if any of several error 
 * conditions occurred: the file could not be opened, the image format is 
 * not supported, there was not enough memory to allocate the image buffer, 
 * the stream contained invalid data, or the operation was cancelled.
 *
 * Since: 2.28
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_stream  (GInputStream  *stream,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
        GdkPixbufAnimation *animation;
        GdkPixbufLoader *loader;
        gssize n_read;
        guchar buffer[LOAD_BUFFER_SIZE];
        gboolean res;

        g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        loader = gdk_pixbuf_loader_new ();

        res = TRUE;
        while (1) { 
                n_read = g_input_stream_read (stream, 
                                              buffer, 
                                              sizeof (buffer), 
                                              cancellable, 
                                              error);
                if (n_read < 0) {
                        res = FALSE;
                        error = NULL; /* Ignore further errors */
                        break;
                }

                if (n_read == 0)
                        break;

                if (!gdk_pixbuf_loader_write (loader, 
                                              buffer, 
                                              n_read, 
                                              error)) {
                        res = FALSE;
                        error = NULL;
                        break;
                }
        }

        if (!gdk_pixbuf_loader_close (loader, error)) {
                res = FALSE;
                error = NULL;
        }

        if (res) {
                animation = gdk_pixbuf_loader_get_animation (loader);
                if (animation)
                        g_object_ref (animation);
        } else {
                animation = NULL;
        }

        g_object_unref (loader);

        return animation;
}

static void
animation_new_from_stream_thread (GSimpleAsyncResult *result,
                                  GInputStream       *stream,
                                  GCancellable       *cancellable)
{
	GdkPixbufAnimation *animation;
	GError *error = NULL;

	animation = gdk_pixbuf_animation_new_from_stream (stream, cancellable, &error);

	/* Set the new pixbuf as the result, or error out */
	if (animation == NULL) {
		g_simple_async_result_take_error (result, error);
	} else {
		g_simple_async_result_set_op_res_gpointer (result, g_object_ref (animation), g_object_unref);
	}
}

/**
 * gdk_pixbuf_animation_new_from_stream_async:
 * @stream: a #GInputStream from which to load the animation
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: a #GAsyncReadyCallback to call when the the pixbuf is loaded
 * @user_data: the data to pass to the callback function
 *
 * Creates a new animation by asynchronously loading an image from an input stream.
 *
 * For more details see gdk_pixbuf_new_from_stream(), which is the synchronous
 * version of this function.
 *
 * When the operation is finished, @callback will be called in the main thread.
 * You can then call gdk_pixbuf_animation_new_from_stream_finish() to get the
 * result of the operation.
 *
 * Since: 2.28
 **/
void
gdk_pixbuf_animation_new_from_stream_async (GInputStream        *stream,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
	GSimpleAsyncResult *result;

	g_return_if_fail (G_IS_INPUT_STREAM (stream));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	result = g_simple_async_result_new (G_OBJECT (stream), callback, user_data, gdk_pixbuf_animation_new_from_stream_async);
	g_simple_async_result_run_in_thread (result, (GSimpleAsyncThreadFunc) animation_new_from_stream_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (result);
}

/**
 * gdk_pixbuf_animation_new_from_stream_finish:
 * @async_result: a #GAsyncResult
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous pixbuf animation creation operation started with
 * gdk_pixbuf_animation_new_from_stream_async().
 *
 * Return value: a #GdkPixbufAnimation or %NULL on error. Free the returned
 * object with g_object_unref().
 *
 * Since: 2.28
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_stream_finish (GAsyncResult  *async_result,
			              	     GError       **error)
{
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (async_result);

	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), NULL);
	g_return_val_if_fail (!error || (error && !*error), NULL);
	g_warn_if_fail (g_simple_async_result_get_source_tag (result) == gdk_pixbuf_animation_new_from_stream_async);

	if (g_simple_async_result_propagate_error (result, error))
		return NULL;

	return g_simple_async_result_get_op_res_gpointer (result);
}

/**
 * gdk_pixbuf_animation_new_from_resource:
 * @resource_path: the path of the resource file
 * @error: Return location for an error
 *
 * Creates a new pixbuf animation by loading an image from an resource.
 *
 * The file format is detected automatically. If %NULL is returned, then
 * @error will be set.
 *
 * Return value: A newly-created animation, or %NULL if any of several error
 * conditions occurred: the file could not be opened, the image format is
 * not supported, there was not enough memory to allocate the image buffer,
 * the stream contained invalid data, or the operation was cancelled.
 *
 * Since: 2.28
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_resource (const char *resource_path,
                                        GError    **error)
{
	GInputStream *stream;
	GdkPixbufAnimation *anim;
	GdkPixbuf *pixbuf;

        pixbuf = _gdk_pixbuf_new_from_resource_try_mmap (resource_path);
        if (pixbuf) {
                anim = gdk_pixbuf_non_anim_new (pixbuf);
                g_object_unref (pixbuf);
                return anim;
        }

	stream = g_resources_open_stream (resource_path, 0, error);
	if (stream == NULL)
		return NULL;

	anim = gdk_pixbuf_animation_new_from_stream (stream, NULL, error);
	g_object_unref (stream);
	return anim;
}

/**
 * gdk_pixbuf_animation_ref: (skip)
 * @animation: An animation.
 *
 * Adds a reference to an animation.
 *
 * Return value: The same as the @animation argument.
 *
 * Deprecated: 2.0: Use g_object_ref().
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_ref (GdkPixbufAnimation *animation)
{
        return (GdkPixbufAnimation*) g_object_ref (animation);
}

/**
 * gdk_pixbuf_animation_unref: (skip)
 * @animation: An animation.
 *
 * Removes a reference from an animation.
 *
 * Deprecated: 2.0: Use g_object_unref().
 **/
void
gdk_pixbuf_animation_unref (GdkPixbufAnimation *animation)
{
        g_object_unref (animation);
}

/**
 * gdk_pixbuf_animation_is_static_image:
 * @animation: a #GdkPixbufAnimation
 * 
 * If you load a file with gdk_pixbuf_animation_new_from_file() and it turns
 * out to be a plain, unanimated image, then this function will return
 * %TRUE. Use gdk_pixbuf_animation_get_static_image() to retrieve
 * the image.
 * 
 * Return value: %TRUE if the "animation" was really just an image
 **/
gboolean
gdk_pixbuf_animation_is_static_image (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), FALSE);

        return GDK_PIXBUF_ANIMATION_GET_CLASS (animation)->is_static_image (animation);
}

/**
 * gdk_pixbuf_animation_get_static_image:
 * @animation: a #GdkPixbufAnimation
 * 
 * If an animation is really just a plain image (has only one frame),
 * this function returns that image. If the animation is an animation,
 * this function returns a reasonable thing to display as a static
 * unanimated image, which might be the first frame, or something more
 * sophisticated. If an animation hasn't loaded any frames yet, this
 * function will return %NULL.
 * 
 * Return value: (transfer none): unanimated image representing the animation
 **/
GdkPixbuf*
gdk_pixbuf_animation_get_static_image (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), NULL);
        
        return GDK_PIXBUF_ANIMATION_GET_CLASS (animation)->get_static_image (animation);
}

/**
 * gdk_pixbuf_animation_get_width:
 * @animation: An animation.
 *
 * Queries the width of the bounding box of a pixbuf animation.
 * 
 * Return value: Width of the bounding box of the animation.
 **/
int
gdk_pixbuf_animation_get_width (GdkPixbufAnimation *animation)
{
        int width;
        
	g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), 0);

        width = 0;
        GDK_PIXBUF_ANIMATION_GET_CLASS (animation)->get_size (animation,
                                                              &width, NULL);
        

	return width;
}

/**
 * gdk_pixbuf_animation_get_height:
 * @animation: An animation.
 *
 * Queries the height of the bounding box of a pixbuf animation.
 * 
 * Return value: Height of the bounding box of the animation.
 **/
int
gdk_pixbuf_animation_get_height (GdkPixbufAnimation *animation)
{
        int height;
        
	g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), 0);

        height = 0;
        GDK_PIXBUF_ANIMATION_GET_CLASS (animation)->get_size (animation,
                                                              NULL, &height);
        

	return height;
}


/**
 * gdk_pixbuf_animation_get_iter:
 * @animation: a #GdkPixbufAnimation
 * @start_time: (allow-none): time when the animation starts playing
 * 
 * Get an iterator for displaying an animation. The iterator provides
 * the frames that should be displayed at a given time.
 * It should be freed after use with g_object_unref().
 * 
 * @start_time would normally come from g_get_current_time(), and
 * marks the beginning of animation playback. After creating an
 * iterator, you should immediately display the pixbuf returned by
 * gdk_pixbuf_animation_iter_get_pixbuf(). Then, you should install a
 * timeout (with g_timeout_add()) or by some other mechanism ensure
 * that you'll update the image after
 * gdk_pixbuf_animation_iter_get_delay_time() milliseconds. Each time
 * the image is updated, you should reinstall the timeout with the new,
 * possibly-changed delay time.
 *
 * As a shortcut, if @start_time is %NULL, the result of
 * g_get_current_time() will be used automatically.
 *
 * To update the image (i.e. possibly change the result of
 * gdk_pixbuf_animation_iter_get_pixbuf() to a new frame of the animation),
 * call gdk_pixbuf_animation_iter_advance().
 *
 * If you're using #GdkPixbufLoader, in addition to updating the image
 * after the delay time, you should also update it whenever you
 * receive the area_updated signal and
 * gdk_pixbuf_animation_iter_on_currently_loading_frame() returns
 * %TRUE. In this case, the frame currently being fed into the loader
 * has received new data, so needs to be refreshed. The delay time for
 * a frame may also be modified after an area_updated signal, for
 * example if the delay time for a frame is encoded in the data after
 * the frame itself. So your timeout should be reinstalled after any
 * area_updated signal.
 *
 * A delay time of -1 is possible, indicating "infinite."
 * 
 * Return value: (transfer full): an iterator to move over the animation
 **/
GdkPixbufAnimationIter*
gdk_pixbuf_animation_get_iter (GdkPixbufAnimation *animation,
                               const GTimeVal     *start_time)
{
        GTimeVal val;
        
        g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), NULL);


        if (start_time)
                val = *start_time;
        else
                g_get_current_time (&val);
        
        return GDK_PIXBUF_ANIMATION_GET_CLASS (animation)->get_iter (animation, &val);
}

G_DEFINE_TYPE (GdkPixbufAnimationIter, gdk_pixbuf_animation_iter, G_TYPE_OBJECT);

static void
gdk_pixbuf_animation_iter_class_init (GdkPixbufAnimationIterClass *klass)
{
}

static void
gdk_pixbuf_animation_iter_init (GdkPixbufAnimationIter *iter)
{
}

/**
 * gdk_pixbuf_animation_iter_get_delay_time:
 * @iter: an animation iterator
 *
 * Gets the number of milliseconds the current pixbuf should be displayed,
 * or -1 if the current pixbuf should be displayed forever. g_timeout_add()
 * conveniently takes a timeout in milliseconds, so you can use a timeout
 * to schedule the next update.
 *
 * Return value: delay time in milliseconds (thousandths of a second)
 **/
int
gdk_pixbuf_animation_iter_get_delay_time (GdkPixbufAnimationIter *iter)
{
        g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION_ITER (iter), -1);
        g_return_val_if_fail (GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->get_delay_time, -1);

        return GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->get_delay_time (iter);
}

/**
 * gdk_pixbuf_animation_iter_get_pixbuf:
 * @iter: an animation iterator
 * 
 * Gets the current pixbuf which should be displayed; the pixbuf might not
 * be the same size as the animation itself
 * (gdk_pixbuf_animation_get_width(), gdk_pixbuf_animation_get_height()). 
 * This pixbuf should be displayed for 
 * gdk_pixbuf_animation_iter_get_delay_time() milliseconds.  The caller
 * of this function does not own a reference to the returned pixbuf;
 * the returned pixbuf will become invalid when the iterator advances
 * to the next frame, which may happen anytime you call
 * gdk_pixbuf_animation_iter_advance(). Copy the pixbuf to keep it
 * (don't just add a reference), as it may get recycled as you advance
 * the iterator.
 *
 * Return value: (transfer none): the pixbuf to be displayed
 **/
GdkPixbuf*
gdk_pixbuf_animation_iter_get_pixbuf (GdkPixbufAnimationIter *iter)
{
        g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION_ITER (iter), NULL);
        g_return_val_if_fail (GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->get_pixbuf, NULL);

        return GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->get_pixbuf (iter);
}

/**
 * gdk_pixbuf_animation_iter_on_currently_loading_frame:
 * @iter: a #GdkPixbufAnimationIter
 *
 * Used to determine how to respond to the area_updated signal on
 * #GdkPixbufLoader when loading an animation. area_updated is emitted
 * for an area of the frame currently streaming in to the loader. So if
 * you're on the currently loading frame, you need to redraw the screen for
 * the updated area.
 *
 * Return value: %TRUE if the frame we're on is partially loaded, or the last frame
 **/
gboolean
gdk_pixbuf_animation_iter_on_currently_loading_frame (GdkPixbufAnimationIter *iter)
{
        g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION_ITER (iter), FALSE);
        g_return_val_if_fail (GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->on_currently_loading_frame, FALSE);

        return GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->on_currently_loading_frame (iter);
}

/**
 * gdk_pixbuf_animation_iter_advance:
 * @iter: a #GdkPixbufAnimationIter
 * @current_time: (allow-none): current time
 *
 * Possibly advances an animation to a new frame. Chooses the frame based
 * on the start time passed to gdk_pixbuf_animation_get_iter().
 *
 * @current_time would normally come from g_get_current_time(), and
 * must be greater than or equal to the time passed to
 * gdk_pixbuf_animation_get_iter(), and must increase or remain
 * unchanged each time gdk_pixbuf_animation_iter_get_pixbuf() is
 * called. That is, you can't go backward in time; animations only
 * play forward.
 *
 * As a shortcut, pass %NULL for the current time and g_get_current_time()
 * will be invoked on your behalf. So you only need to explicitly pass
 * @current_time if you're doing something odd like playing the animation
 * at double speed.
 *
 * If this function returns %FALSE, there's no need to update the animation
 * display, assuming the display had been rendered prior to advancing;
 * if %TRUE, you need to call gdk_pixbuf_animation_iter_get_pixbuf()
 * and update the display with the new pixbuf.
 *
 * Returns: %TRUE if the image may need updating
 * 
 **/
gboolean
gdk_pixbuf_animation_iter_advance (GdkPixbufAnimationIter *iter,
                                   const GTimeVal         *current_time)
{
        GTimeVal val;

        g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION_ITER (iter), FALSE);
        g_return_val_if_fail (GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->advance, FALSE);

        if (current_time)
                val = *current_time;
        else
                g_get_current_time (&val);

        return GDK_PIXBUF_ANIMATION_ITER_GET_CLASS (iter)->advance (iter, &val);
}

static void                    gdk_pixbuf_non_anim_finalize         (GObject            *object);
static gboolean                gdk_pixbuf_non_anim_is_static_image  (GdkPixbufAnimation *animation);
static GdkPixbuf*              gdk_pixbuf_non_anim_get_static_image (GdkPixbufAnimation *animation);
static void                    gdk_pixbuf_non_anim_get_size         (GdkPixbufAnimation *anim,
                                                                     int                *width,
                                                                     int                *height);
static GdkPixbufAnimationIter* gdk_pixbuf_non_anim_get_iter         (GdkPixbufAnimation *anim,
                                                                     const GTimeVal     *start_time);

G_DEFINE_TYPE (GdkPixbufNonAnim, gdk_pixbuf_non_anim, GDK_TYPE_PIXBUF_ANIMATION);

static void
gdk_pixbuf_non_anim_class_init (GdkPixbufNonAnimClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GdkPixbufAnimationClass *anim_class = GDK_PIXBUF_ANIMATION_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_non_anim_finalize;

        anim_class->is_static_image = gdk_pixbuf_non_anim_is_static_image;
        anim_class->get_static_image = gdk_pixbuf_non_anim_get_static_image;
        anim_class->get_size = gdk_pixbuf_non_anim_get_size;
        anim_class->get_iter = gdk_pixbuf_non_anim_get_iter;
}

static void
gdk_pixbuf_non_anim_init (GdkPixbufNonAnim *non_anim)
{
}

static void
gdk_pixbuf_non_anim_finalize (GObject *object)
{
        GdkPixbufNonAnim *non_anim = GDK_PIXBUF_NON_ANIM (object);

        if (non_anim->pixbuf)
                g_object_unref (non_anim->pixbuf);
        
        G_OBJECT_CLASS (gdk_pixbuf_non_anim_parent_class)->finalize (object);
}

GdkPixbufAnimation*
gdk_pixbuf_non_anim_new (GdkPixbuf *pixbuf)
{
        GdkPixbufNonAnim *non_anim;

        non_anim = g_object_new (GDK_TYPE_PIXBUF_NON_ANIM, NULL);

        non_anim->pixbuf = pixbuf;

        if (pixbuf)
                g_object_ref (pixbuf);

        return GDK_PIXBUF_ANIMATION (non_anim);
}

static gboolean
gdk_pixbuf_non_anim_is_static_image  (GdkPixbufAnimation *animation)
{

        return TRUE;
}

static GdkPixbuf*
gdk_pixbuf_non_anim_get_static_image (GdkPixbufAnimation *animation)
{
        GdkPixbufNonAnim *non_anim;

        non_anim = GDK_PIXBUF_NON_ANIM (animation);

        return non_anim->pixbuf;
}

static void
gdk_pixbuf_non_anim_get_size (GdkPixbufAnimation *anim,
                              int                *width,
                              int                *height)
{
        GdkPixbufNonAnim *non_anim;

        non_anim = GDK_PIXBUF_NON_ANIM (anim);

        if (width)
                *width = gdk_pixbuf_get_width (non_anim->pixbuf);

        if (height)
                *height = gdk_pixbuf_get_height (non_anim->pixbuf);
}

static GdkPixbufAnimationIter*
gdk_pixbuf_non_anim_get_iter (GdkPixbufAnimation *anim,
                              const GTimeVal     *start_time)
{
        GdkPixbufNonAnimIter *iter;

        iter = g_object_new (GDK_TYPE_PIXBUF_NON_ANIM_ITER, NULL);

        iter->non_anim = GDK_PIXBUF_NON_ANIM (anim);

        g_object_ref (iter->non_anim);
        
        return GDK_PIXBUF_ANIMATION_ITER (iter);
}

static void       gdk_pixbuf_non_anim_iter_finalize                   (GObject                *object);
static int        gdk_pixbuf_non_anim_iter_get_delay_time             (GdkPixbufAnimationIter *iter);
static GdkPixbuf* gdk_pixbuf_non_anim_iter_get_pixbuf                 (GdkPixbufAnimationIter *iter);
static gboolean   gdk_pixbuf_non_anim_iter_on_currently_loading_frame (GdkPixbufAnimationIter *iter);
static gboolean   gdk_pixbuf_non_anim_iter_advance                    (GdkPixbufAnimationIter *iter,
                                                                       const GTimeVal         *current_time);

G_DEFINE_TYPE (GdkPixbufNonAnimIter,
               gdk_pixbuf_non_anim_iter,
               GDK_TYPE_PIXBUF_ANIMATION_ITER);

static void
gdk_pixbuf_non_anim_iter_class_init (GdkPixbufNonAnimIterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GdkPixbufAnimationIterClass *anim_iter_class =
                GDK_PIXBUF_ANIMATION_ITER_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_non_anim_iter_finalize;

        anim_iter_class->get_delay_time = gdk_pixbuf_non_anim_iter_get_delay_time;
        anim_iter_class->get_pixbuf = gdk_pixbuf_non_anim_iter_get_pixbuf;
        anim_iter_class->on_currently_loading_frame = gdk_pixbuf_non_anim_iter_on_currently_loading_frame;
        anim_iter_class->advance = gdk_pixbuf_non_anim_iter_advance;
}

static void
gdk_pixbuf_non_anim_iter_init (GdkPixbufNonAnimIter *non_iter)
{
}

static void
gdk_pixbuf_non_anim_iter_finalize (GObject *object)
{
        GdkPixbufNonAnimIter *iter = GDK_PIXBUF_NON_ANIM_ITER (object);

        g_object_unref (iter->non_anim);
        
        G_OBJECT_CLASS (gdk_pixbuf_non_anim_iter_parent_class)->finalize (object);
}

static int
gdk_pixbuf_non_anim_iter_get_delay_time (GdkPixbufAnimationIter *iter)
{
        return -1; /* show only frame forever */
}

static GdkPixbuf*
gdk_pixbuf_non_anim_iter_get_pixbuf (GdkPixbufAnimationIter *iter)
{
        return GDK_PIXBUF_NON_ANIM_ITER (iter)->non_anim->pixbuf;
}


static gboolean
gdk_pixbuf_non_anim_iter_on_currently_loading_frame (GdkPixbufAnimationIter *iter)
{
        return TRUE;
}
        
static gboolean
gdk_pixbuf_non_anim_iter_advance (GdkPixbufAnimationIter *iter,
                                  const GTimeVal         *current_time)
{

        /* Advancing never requires a refresh */
        return FALSE;
}
