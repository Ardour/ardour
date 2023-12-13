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

#include <stdlib.h>
#include <sys/types.h>

#if defined (HAVE_IPC_H) && defined (HAVE_SHM_H) && defined (HAVE_XSHM_H)
#define USE_SHM
#endif

#ifdef USE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* USE_SHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef USE_SHM
#include <X11/extensions/XShm.h>
#endif /* USE_SHM */

#include <errno.h>

#include "gdk.h"		/* For gdk_error_trap_* / gdk_flush_* */
#include "gdkx.h"
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"
#include "gdkalias.h"

typedef struct _GdkImagePrivateX11     GdkImagePrivateX11;

struct _GdkImagePrivateX11
{
  XImage *ximage;
  GdkScreen *screen;
  gpointer x_shm_info;
  Pixmap shm_pixmap;
};

static GList *image_list = NULL;

static void gdk_x11_image_destroy (GdkImage      *image);
static void gdk_image_finalize    (GObject       *object);

#define PRIVATE_DATA(image) ((GdkImagePrivateX11 *) GDK_IMAGE (image)->windowing_data)

G_DEFINE_TYPE (GdkImage, gdk_image, G_TYPE_OBJECT)

static void
gdk_image_init (GdkImage *image)
{
  image->windowing_data = G_TYPE_INSTANCE_GET_PRIVATE (image, 
						       GDK_TYPE_IMAGE, 
						       GdkImagePrivateX11);
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_image_finalize;

  g_type_class_add_private (object_class, sizeof (GdkImagePrivateX11));
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  gdk_x11_image_destroy (image);
  
  G_OBJECT_CLASS (gdk_image_parent_class)->finalize (object);
}


void
_gdk_image_exit (void)
{
  GdkImage *image;

  while (image_list)
    {
      image = image_list->data;
      gdk_x11_image_destroy (image);
    }
}

/**
 * gdk_image_new_bitmap:
 * @visual: the #GdkVisual to use for the image.
 * @data: the pixel data. 
 * @width: the width of the image in pixels. 
 * @height: the height of the image in pixels. 
 * 
 * Creates a new #GdkImage with a depth of 1 from the given data.
 * <warning><para>THIS FUNCTION IS INCREDIBLY BROKEN. The passed-in data must 
 * be allocated by malloc() (NOT g_malloc()) and will be freed when the 
 * image is freed.</para></warning>
 * 
 * Return value: a new #GdkImage.
 **/
GdkImage *
gdk_image_new_bitmap (GdkVisual *visual, 
		      gpointer   data, 
		      gint       width, 
		      gint       height)
{
  Visual *xvisual;
  GdkImage *image;
  GdkDisplay *display;
  GdkImagePrivateX11 *private;
  
  image = g_object_new (gdk_image_get_type (), NULL);
  private = PRIVATE_DATA (image);
  private->screen = gdk_visual_get_screen (visual);
  display = GDK_SCREEN_DISPLAY (private->screen);
  
  image->type = GDK_IMAGE_NORMAL;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = 1;
  image->bits_per_pixel = 1;
  if (display->closed)
    private->ximage = NULL;
  else
    {
      xvisual = ((GdkVisualPrivate*) visual)->xvisual;
      private->ximage = XCreateImage (GDK_SCREEN_XDISPLAY (private->screen),
				      xvisual, 1, XYBitmap,
				      0, NULL, width, height, 8, 0);
      private->ximage->data = data;
      private->ximage->bitmap_bit_order = MSBFirst;
      private->ximage->byte_order = MSBFirst;
    }
  
  image->byte_order = MSBFirst;
  image->mem =  private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  image->bpp = 1;
  return image;
} 

void
_gdk_windowing_image_init (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  if (display_x11->use_xshm)
    {
#ifdef USE_SHM
      Display *xdisplay = display_x11->xdisplay;
      int major, minor, event_base;
      Bool pixmaps;
  
      if (XShmQueryExtension (xdisplay) &&
	  XShmQueryVersion (xdisplay, &major, &minor, &pixmaps))
	{
	  display_x11->have_shm_pixmaps = pixmaps;
	  event_base = XShmGetEventBase (xdisplay);

	  gdk_x11_register_standard_event_type (display,
						event_base, ShmNumberEvents);
	}
      else
#endif /* USE_SHM */
	display_x11->use_xshm = FALSE;
    }
}

GdkImage*
_gdk_image_new_for_depth (GdkScreen    *screen,
			  GdkImageType  type,
			  GdkVisual    *visual,
			  gint          width,
			  gint          height,
			  gint          depth)
{
  GdkImage *image;
  GdkImagePrivateX11 *private;
#ifdef USE_SHM
  XShmSegmentInfo *x_shm_info;
#endif /* USE_SHM */
  Visual *xvisual = NULL;
  GdkDisplayX11 *display_x11;
  GdkScreenX11 *screen_x11;

  g_return_val_if_fail (!visual || GDK_IS_VISUAL (visual), NULL);
  g_return_val_if_fail (visual || depth != -1, NULL);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  
  screen_x11 = GDK_SCREEN_X11 (screen);
  display_x11 = GDK_DISPLAY_X11 (screen_x11->display);
  
  if (visual)
    depth = visual->depth;
  
  switch (type)
    {
    case GDK_IMAGE_FASTEST:
      image = _gdk_image_new_for_depth (screen, GDK_IMAGE_SHARED, 
					visual, width, height, depth);
      if (!image)
	image = _gdk_image_new_for_depth (screen, GDK_IMAGE_NORMAL,
					  visual, width, height, depth);
      break;

    default:
      image = g_object_new (gdk_image_get_type (), NULL);
      
      private = PRIVATE_DATA (image);

      private->screen = screen;

      image->type = type;
      image->visual = visual;
      image->width = width;
      image->height = height;
      image->depth = depth;

      if (visual)
	xvisual = ((GdkVisualPrivate*) visual)->xvisual;

      switch (type)
	{
	case GDK_IMAGE_SHARED:
#ifdef USE_SHM
	  if (display_x11->use_xshm)
	    {
	      private->x_shm_info = g_new (XShmSegmentInfo, 1);
	      x_shm_info = private->x_shm_info;
	      x_shm_info->shmid = -1;
	      x_shm_info->shmaddr = (char*) -1;

	      private->ximage = XShmCreateImage (screen_x11->xdisplay, xvisual, depth,
						 ZPixmap, NULL, x_shm_info, width, height);
	      if (private->ximage == NULL)
		{
		  g_warning ("XShmCreateImage failed");
		  display_x11->use_xshm = FALSE;
		  
		  goto error;
		}

	      x_shm_info->shmid = shmget (IPC_PRIVATE,
					  private->ximage->bytes_per_line * private->ximage->height,
					  IPC_CREAT | 0600);

	      if (x_shm_info->shmid == -1)
		{
		  /* EINVAL indicates, most likely, that the segment we asked for
		   * is bigger than SHMMAX, so we don't treat it as a permanent
		   * error. ENOSPC and ENOMEM may also indicate this, but
		   * more likely are permanent errors.
		   */
		  if (errno != EINVAL)
		    {
		      g_warning ("shmget failed: error %d (%s)", errno, g_strerror (errno));
		      display_x11->use_xshm = FALSE;
		    }

		  goto error;
		}

	      x_shm_info->readOnly = False;
	      x_shm_info->shmaddr = shmat (x_shm_info->shmid, NULL, 0);
	      private->ximage->data = x_shm_info->shmaddr;

	      if (x_shm_info->shmaddr == (char*) -1)
		{
		  g_warning ("shmat failed: error %d (%s)", errno, g_strerror (errno));
		  /* Failure in shmat is almost certainly permanent. Most likely error is
		   * EMFILE, which would mean that we've exceeded the per-process
		   * Shm segment limit.
		   */
		  display_x11->use_xshm = FALSE;
		  goto error;
		}

	      gdk_error_trap_push ();

	      XShmAttach (screen_x11->xdisplay, x_shm_info);
	      XSync (screen_x11->xdisplay, False);

	      if (gdk_error_trap_pop ())
		{
		  /* this is the common failure case so omit warning */
		  display_x11->use_xshm = FALSE;
		  goto error;
		}
	      
	      /* We mark the segment as destroyed so that when
	       * the last process detaches, it will be deleted.
	       * There is a small possibility of leaking if
	       * we die in XShmAttach. In theory, a signal handler
	       * could be set up.
	       */
	      shmctl (x_shm_info->shmid, IPC_RMID, NULL);		      

	      if (image)
		image_list = g_list_prepend (image_list, image);
	    }
	  else
#endif /* USE_SHM */
	    goto error;
	  break;
	case GDK_IMAGE_NORMAL:
	  private->ximage = XCreateImage (screen_x11->xdisplay, xvisual, depth,
					  ZPixmap, 0, NULL, width, height, 32, 0);

	  /* Use malloc, not g_malloc here, because X will call free()
	   * on this data
	   */
	  private->ximage->data = malloc (private->ximage->bytes_per_line *
					  private->ximage->height);
	  if (!private->ximage->data)
	    goto error;
	  break;

	case GDK_IMAGE_FASTEST:
	  g_assert_not_reached ();
	}

      if (image)
	{
	  image->byte_order = (private->ximage->byte_order == LSBFirst) ? GDK_LSB_FIRST : GDK_MSB_FIRST;
	  image->mem = private->ximage->data;
	  image->bpl = private->ximage->bytes_per_line;
	  image->bpp = (private->ximage->bits_per_pixel + 7) / 8;
	  image->bits_per_pixel = private->ximage->bits_per_pixel;
	}
    }

  return image;

 error:
  if (private->ximage)
    {
      XDestroyImage (private->ximage);
      private->ximage = NULL;
    }
#ifdef USE_SHM
  if (private->x_shm_info)
    {
      x_shm_info = private->x_shm_info;
      
      if (x_shm_info->shmaddr != (char *)-1)
	shmdt (x_shm_info->shmaddr);
      if (x_shm_info->shmid != -1) 
	shmctl (x_shm_info->shmid, IPC_RMID, NULL);
      
      g_free (x_shm_info);
      private->x_shm_info = NULL;
    }
#endif /* USE_SHM */
  g_object_unref (image);
  
  return NULL;
}

Pixmap
_gdk_x11_image_get_shm_pixmap (GdkImage *image)
{
  GdkImagePrivateX11 *private = PRIVATE_DATA (image);
  GdkDisplay *display = GDK_SCREEN_DISPLAY (private->screen);

  if (display->closed)
    return None;

#ifdef USE_SHM  
  /* Future: do we need one of these per-screen per-image? ShmPixmaps
   * are the same for every screen, but can they be shared? Not a concern
   * right now since we tie images to a particular screen.
   */
  if (!private->shm_pixmap && image->type == GDK_IMAGE_SHARED && 
      GDK_DISPLAY_X11 (display)->have_shm_pixmaps)
    private->shm_pixmap = XShmCreatePixmap (GDK_SCREEN_XDISPLAY (private->screen),
					    GDK_SCREEN_XROOTWIN (private->screen),
					    image->mem, private->x_shm_info, 
					    image->width, image->height, image->depth);

  return private->shm_pixmap;
#else
  return None;
#endif    
}

static GdkImage*
get_full_image (GdkDrawable    *drawable,
		gint            src_x,
		gint            src_y,
		gint            width,
		gint            height)
{
  GdkImage *image;
  GdkImagePrivateX11 *private;
  GdkDrawableImplX11 *impl;
  XImage *ximage;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  ximage = XGetImage (GDK_SCREEN_XDISPLAY (impl->screen),
		      impl->xid,
		      src_x, src_y, width, height,
		      AllPlanes, ZPixmap);
  
  if (!ximage)
    return NULL;
  
  image = g_object_new (gdk_image_get_type (), NULL);
  
  private = PRIVATE_DATA (image);
  
  private->screen = impl->screen;
  private->ximage = ximage;
  
  image->type = GDK_IMAGE_NORMAL;
  image->visual = gdk_drawable_get_visual (drawable); /* could be NULL */
  image->width = width;
  image->height = height;
  image->depth = gdk_drawable_get_depth (drawable);
  
  image->mem = private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  image->bits_per_pixel = private->ximage->bits_per_pixel;
  image->bpp = (private->ximage->bits_per_pixel + 7) / 8;
  image->byte_order = (private->ximage->byte_order == LSBFirst) ? GDK_LSB_FIRST : GDK_MSB_FIRST;
  
  return image;
}

GdkImage*
_gdk_x11_copy_to_image (GdkDrawable    *drawable,
			GdkImage       *image,
			gint            src_x,
			gint            src_y,
			gint            dest_x,
			gint            dest_y,
			gint            width,
			gint            height)
{
  GdkImagePrivateX11 *private;
  GdkDrawableImplX11 *impl;
  GdkVisual *visual;
  GdkDisplay *display;
  Display *xdisplay;
  gboolean have_grab;
  GdkRectangle req;
  GdkRectangle window_rect;
  Pixmap shm_pixmap = None;
  gboolean success = TRUE;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_X11 (drawable), NULL);
  g_return_val_if_fail (image != NULL || (dest_x == 0 && dest_y == 0), NULL);

  visual = gdk_drawable_get_visual (drawable);
  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  display = gdk_drawable_get_display (drawable);
  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (display->closed)
    return NULL;
  
  have_grab = FALSE;

#define UNGRAB() G_STMT_START {					\
    if (have_grab) {						\
      gdk_x11_display_ungrab (display);				\
      have_grab = FALSE; }					\
  } G_STMT_END

  if (!image && !GDK_IS_WINDOW_IMPL_X11 (drawable))
    return get_full_image (drawable, src_x, src_y, width, height);

  if (image && image->type == GDK_IMAGE_SHARED)
    {
      shm_pixmap = _gdk_x11_image_get_shm_pixmap (image);
      if (shm_pixmap)
	{
	  GC xgc;
	  XGCValues values;

	  /* Again easy, we can just XCopyArea, and don't have to worry about clipping
	   */
	  values.subwindow_mode = IncludeInferiors;
	  xgc = XCreateGC (xdisplay, impl->xid, GCSubwindowMode, &values);
	  
	  XCopyArea (xdisplay, impl->xid, shm_pixmap, xgc,
		     src_x, src_y, width, height, dest_x, dest_y);
	  XSync (xdisplay, FALSE);
	  
	  XFreeGC (xdisplay, xgc);
	  
	  return image;
	}
    }

  /* Now the general case - we may have to worry about clipping to the screen
   * bounds, in which case we'll have to grab the server and only get a piece
   * of the window.
   */
  if (GDK_IS_WINDOW_IMPL_X11 (drawable))
    {
      GdkRectangle screen_rect;
      Window child;

      have_grab = TRUE;
      gdk_x11_display_grab (display);

      /* Translate screen area into window coordinates */
      XTranslateCoordinates (xdisplay,
			     GDK_SCREEN_XROOTWIN (impl->screen),
			     impl->xid,
			     0, 0, 
			     &screen_rect.x, &screen_rect.y, 
			     &child);

      screen_rect.width = gdk_screen_get_width (impl->screen);
      screen_rect.height = gdk_screen_get_height (impl->screen);
      
      gdk_error_trap_push ();

      window_rect.x = 0;
      window_rect.y = 0;
      
      gdk_window_get_geometry (GDK_WINDOW (impl->wrapper),
                               NULL, NULL,
                               &window_rect.width,
                               &window_rect.height,
                               NULL);
      
      /* compute intersection of screen and window, in window
       * coordinates
       */
      if (gdk_error_trap_pop () ||
          !gdk_rectangle_intersect (&window_rect, &screen_rect, 
                                    &window_rect))
	goto out;
    }
  else
    {
      window_rect.x = 0;
      window_rect.y = 0;
      gdk_drawable_get_size (drawable,
			     &window_rect.width,
			     &window_rect.height);
    }
      
  req.x = src_x;
  req.y = src_y;
  req.width = width;
  req.height = height;
  
  /* window_rect specifies the part of drawable which we can get from
   * the server in window coordinates. 
   * For pixmaps this is all of the pixmap, for windows it is just 
   * the onscreen part.
   */
  if (!gdk_rectangle_intersect (&req, &window_rect, &req))
    goto out;

  gdk_error_trap_push ();
  
  if (!image &&
      req.x == src_x && req.y == src_y && req.width == width && req.height == height)
    {
      image = get_full_image (drawable, src_x, src_y, width, height);
      if (!image)
	success = FALSE;
    }
  else
    {
      gboolean created_image = FALSE;
      
      if (!image)
	{
	  image = _gdk_image_new_for_depth (impl->screen, GDK_IMAGE_NORMAL, 
					    visual, width, height,
					    gdk_drawable_get_depth (drawable));
	  created_image = TRUE;
	}

      private = PRIVATE_DATA (image);

      /* In the ShmImage but no ShmPixmap case, we could use XShmGetImage when
       * we are getting the entire image.
       */
      if (XGetSubImage (xdisplay, impl->xid,
			req.x, req.y, req.width, req.height,
			AllPlanes, ZPixmap,
			private->ximage,
			dest_x + req.x - src_x, dest_y + req.y - src_y) == None)
	{
	  if (created_image)
	    g_object_unref (image);
	  image = NULL;
	  success = FALSE;
	}
    }

  gdk_error_trap_pop ();

 out:
  
  if (have_grab)
    {				
      gdk_x11_display_ungrab (display);
      have_grab = FALSE;
    }
  
  if (success && !image)
    {
      /* We "succeeded", but could get no content for the image so return junk */
      image = _gdk_image_new_for_depth (impl->screen, GDK_IMAGE_NORMAL, 
					visual, width, height,
					gdk_drawable_get_depth (drawable));
    }
      
  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  guint32 pixel;
  GdkImagePrivateX11 *private;

  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);
  g_return_val_if_fail (x >= 0 && x < image->width, 0);
  g_return_val_if_fail (y >= 0 && y < image->height, 0);

  private = PRIVATE_DATA (image);

  if (!private->screen->closed)
    pixel = XGetPixel (private->ximage, x, y);
  else
    pixel = 0;

  return pixel;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  GdkImagePrivateX11 *private;

  g_return_if_fail (GDK_IS_IMAGE (image));
  g_return_if_fail (x >= 0 && x < image->width);
  g_return_if_fail (y >= 0 && y < image->height);

  private = PRIVATE_DATA (image);

  if (!private->screen->closed)
    pixel = XPutPixel (private->ximage, x, y, pixel);
}

static void
gdk_x11_image_destroy (GdkImage *image)
{
  GdkImagePrivateX11 *private;
#ifdef USE_SHM
  XShmSegmentInfo *x_shm_info;
#endif /* USE_SHM */

  g_return_if_fail (GDK_IS_IMAGE (image));

  private = PRIVATE_DATA (image);

  if (private->ximage)		/* Deal with failure of creation */
    {
      switch (image->type)
	{
	case GDK_IMAGE_NORMAL:
	  if (!private->screen->closed)
	    XDestroyImage (private->ximage);
	  break;
	  
	case GDK_IMAGE_SHARED:
#ifdef USE_SHM
	  if (!private->screen->closed)
	    {
	      gdk_display_sync (GDK_SCREEN_DISPLAY (private->screen));

	      if (private->shm_pixmap)
		XFreePixmap (GDK_SCREEN_XDISPLAY (private->screen), private->shm_pixmap);
	  	  
	      XShmDetach (GDK_SCREEN_XDISPLAY (private->screen), private->x_shm_info);
	      XDestroyImage (private->ximage);
	    }
	  
	  image_list = g_list_remove (image_list, image);

	  x_shm_info = private->x_shm_info;
	  shmdt (x_shm_info->shmaddr);
	  
	  g_free (private->x_shm_info);
	  private->x_shm_info = NULL;

#else /* USE_SHM */
	  g_error ("trying to destroy shared memory image when gdk was compiled without shared memory support");
#endif /* USE_SHM */
	  break;
	  
	case GDK_IMAGE_FASTEST:
	  g_assert_not_reached ();
	}
      
      private->ximage = NULL;
    }
}

/**
 * gdk_x11_image_get_xdisplay:
 * @image: a #GdkImage.
 * 
 * Returns the display of a #GdkImage.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_image_get_xdisplay (GdkImage *image)
{
  GdkImagePrivateX11 *private;

  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  private = PRIVATE_DATA (image);

  return GDK_SCREEN_XDISPLAY (private->screen);
}

/**
 * gdk_x11_image_get_ximage:
 * @image: a #GdkImage.
 * 
 * Returns the X image belonging to a #GdkImage.
 * 
 * Return value: an <type>XImage*</type>.
 **/
XImage *
gdk_x11_image_get_ximage (GdkImage *image)
{
  GdkImagePrivateX11 *private;

  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  private = PRIVATE_DATA (image);

  if (private->screen->closed)
    return NULL;
  else
    return private->ximage;
}

gint
_gdk_windowing_get_bits_for_depth (GdkDisplay *display,
				   gint        depth)
{
  XPixmapFormatValues *formats;
  gint count, i;

  formats = XListPixmapFormats (GDK_DISPLAY_XDISPLAY (display), &count);
  
  for (i = 0; i < count; i++)
    if (formats[i].depth == depth)
      {
	gint result = formats[i].bits_per_pixel;
	XFree (formats);
	return result;
      }

  g_assert_not_reached ();
  return -1;
}


#define __GDK_IMAGE_X11_C__
#include "gdkaliasdef.c"
