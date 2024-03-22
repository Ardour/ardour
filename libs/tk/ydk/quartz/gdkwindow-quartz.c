/* gdkwindow-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"
#include <Carbon/Carbon.h>

#include "gdk.h"
#include "gdkwindowimpl.h"
#include "gdkprivate-quartz.h"
#include "gdkscreen-quartz.h"
#include "gdkinputprivate.h"

static gpointer parent_class;

static GSList *main_window_stack;

#define FULLSCREEN_DATA "fullscreen-data"

typedef struct
{
  gint            x, y;
  gint            width, height;
  GdkWMDecoration decor;
} FullscreenSavedGeometry;


static void update_toplevel_order (void);
static void clear_toplevel_order  (void);

static FullscreenSavedGeometry *get_fullscreen_geometry (GdkWindow *window);

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

static void gdk_window_impl_iface_init (GdkWindowImplIface *iface);

gboolean
gdk_quartz_window_is_quartz (GdkWindow *window)
{
  return GDK_WINDOW_IS_QUARTZ (window);
}

NSView *
gdk_quartz_window_get_nsview (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_val_if_fail (GDK_WINDOW_IS_QUARTZ (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return ((GdkWindowImplQuartz *)private->impl)->view;
}

NSWindow *
gdk_quartz_window_get_nswindow (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return ((GdkWindowImplQuartz *)private->impl)->toplevel;
}

static CGContextRef
gdk_window_impl_quartz_get_context (GdkDrawable *drawable,
				    gboolean     antialias)
{
  GdkDrawableImplQuartz *drawable_impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);
  GdkWindowImplQuartz *window_impl = GDK_WINDOW_IMPL_QUARTZ (drawable);
  CGContextRef cg_context;

  if (GDK_WINDOW_DESTROYED (drawable_impl->wrapper))
    return NULL;

  /* Lock focus when not called as part of a drawRect call. This
   * is needed when called from outside "real" expose events, for
   * example for synthesized expose events when realizing windows
   * and for widgets that send fake expose events like the arrow
   * buttons in spinbuttons or the position marker in rulers.
   */
  if (window_impl->in_paint_rect_count == 0)
    {
	    // if (![window_impl->view lockFocusIfCanDraw])
	    // return NULL;
    }
  if (gdk_quartz_osx_version () < GDK_OSX_YOSEMITE)
       cg_context = [[NSGraphicsContext currentContext] graphicsPort];
  else
       cg_context = [[NSGraphicsContext currentContext] CGContext];

  if (!cg_context)
    return NULL;

  CGContextSaveGState (cg_context);
  CGContextSetAllowsAntialiasing (cg_context, antialias);

  /* We'll emulate the clipping caused by double buffering here */
  if (window_impl->begin_paint_count != 0)
    {
      CGRect rect;
      CGRect *cg_rects;
      GdkRectangle *rects;
      gint n_rects, i;

      gdk_region_get_rectangles (window_impl->paint_clip_region,
                                 &rects, &n_rects);

      if (n_rects == 1)
        cg_rects = &rect;
      else
        cg_rects = g_new (CGRect, n_rects);

      for (i = 0; i < n_rects; i++)
        {
          cg_rects[i].origin.x = rects[i].x;
          cg_rects[i].origin.y = rects[i].y;
          cg_rects[i].size.width = rects[i].width;
          cg_rects[i].size.height = rects[i].height;
        }

      CGContextClipToRects (cg_context, cg_rects, n_rects);

      g_free (rects);
      if (cg_rects != &rect)
        g_free (cg_rects);
    }

  return cg_context;
}

static void
check_grab_unmap (GdkWindow *window)
{
  GdkDisplay *display = gdk_drawable_get_display (window);

  _gdk_display_end_pointer_grab (display, 0, window, TRUE);

  if (display->keyboard_grab.window)
    {
      GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
      GdkWindowObject *tmp = GDK_WINDOW_OBJECT (display->keyboard_grab.window);

      while (tmp && tmp != private)
	tmp = tmp->parent;

      if (tmp)
	_gdk_display_unset_has_keyboard_grab (display, TRUE);
    }
}

static void
check_grab_destroy (GdkWindow *window)
{
  GdkDisplay *display = gdk_drawable_get_display (window);
  GdkPointerGrabInfo *grab;

  /* Make sure there is no lasting grab in this native window */
  grab = _gdk_display_get_last_pointer_grab (display);
  if (grab && grab->native_window == window)
    {
      /* Serials are always 0 in quartz, but for clarity: */
      grab->serial_end = grab->serial_start;
      grab->implicit_ungrab = TRUE;
    }

  if (window == display->keyboard_grab.native_window &&
      display->keyboard_grab.window != NULL)
    _gdk_display_unset_has_keyboard_grab (display, TRUE);
}

static void
gdk_window_impl_quartz_finalize (GObject *object)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (object);

  check_grab_destroy (GDK_DRAWABLE_IMPL_QUARTZ (object)->wrapper);

  if (impl->paint_clip_region)
    gdk_region_destroy (impl->paint_clip_region);

  if (impl->transient_for)
    g_object_unref (impl->transient_for);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_window_impl_quartz_class_init (GdkWindowImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableImplQuartzClass *drawable_quartz_class = GDK_DRAWABLE_IMPL_QUARTZ_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_quartz_finalize;

  drawable_quartz_class->get_context = gdk_window_impl_quartz_get_context;
}

static void
gdk_window_impl_quartz_init (GdkWindowImplQuartz *impl)
{
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
}

static void
_gdk_window_quartz_clear_region (GdkWindow *window, const GdkRegion* region, bool ignored)
{
  if (gdk_drawable_get_colormap (window) != gdk_screen_get_rgba_colormap (_gdk_screen)) {

	  /* Window is opaque. We no longer use backing store on Quartz, so the code that fill the backing store with the background
	   * color is no longer in use. We do that here, if there is a background color.
	   */

	  GdkWindowObject *private = GDK_WINDOW_OBJECT(window);
	  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

	  GdkColor bg_color = private->bg_color;
	  CGContextRef cg_context = [[NSGraphicsContext currentContext] graphicsPort];
	  CGContextSaveGState (cg_context);

	  if (g_getenv ("GDK_HARLEQUIN_DEBUGGING")) {
		  CGContextSetRGBFillColor (cg_context,
		                            (random() % 65535) / 65335.0,
		                            (random() % 65535) / 65335.0,
		                            (random() % 65535) / 65335.0,
		                            1.0);
	  } else {
		  CGContextSetRGBFillColor (cg_context,
		                            bg_color.red / 65335.0,
		                            bg_color.green / 65335.0,
		                            bg_color.blue / 65335.0,
		                            1.0);
	  }

	  GdkRectangle *rects;
	  gint n_rects, i;

	  gdk_region_get_rectangles (region, &rects, &n_rects);

	  for (i = 0; i < n_rects; i++)
	  {
		  CGRect cg_rect = CGRectMake (rects[i].x + 0.5, rects[i].y + 0.5, rects[i].width, rects[i].height);

		  CGContextFillRect (cg_context, cg_rect);
	  }

	  CGContextRestoreGState (cg_context);
  }
}

void
_gdk_quartz_window_set_needs_display_in_rect (GdkWindow    *window,
                                              GdkRectangle *rect)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  [impl->view setNeedsDisplayInRect:NSMakeRect (rect->x, rect->y,
                                                rect->width, rect->height)];

}

void
_gdk_quartz_window_set_needs_display_in_region (GdkWindow    *window,
                                                GdkRegion    *region)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  int i, n_rects;
  GdkRectangle *rects;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  gdk_region_get_rectangles (region, &rects, &n_rects);

  for (i = 0; i < n_rects; i++)
    [impl->view setNeedsDisplayInRect:NSMakeRect (rects[i].x, rects[i].y,
                                                  rects[i].width,
                                                  rects[i].height)];

  g_free (rects);

}

void
_gdk_windowing_window_process_updates_recurse (GdkWindow *window,
                                               GdkRegion *region)
{
  if (WINDOW_IS_TOPLEVEL (window))
    _gdk_quartz_window_set_needs_display_in_region (window, region);
  else
    _gdk_window_process_updates_recurse (window, region);
}

void
_gdk_windowing_before_process_all_updates (void)
{
}

void
_gdk_windowing_after_process_all_updates (void)
{
}

GType
_gdk_window_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
	{
	  sizeof (GdkWindowImplQuartzClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_window_impl_quartz_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkWindowImplQuartz),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) gdk_window_impl_quartz_init,
	};

      const GInterfaceInfo window_impl_info =
	{
	  (GInterfaceInitFunc) gdk_window_impl_iface_init,
	  NULL,
	  NULL
	};

      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_QUARTZ,
                                            "GdkWindowImplQuartz",
                                            &object_info, 0);

      g_type_add_interface_static (object_type,
				   GDK_TYPE_WINDOW_IMPL,
				   &window_impl_info);
    }

  return object_type;
}

GType
_gdk_window_impl_get_type (void)
{
  return _gdk_window_impl_quartz_get_type ();
}

static const gchar *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();

  return title;
}

static void
get_ancestor_coordinates_from_child (GdkWindow *child_window,
				     gint       child_x,
				     gint       child_y,
				     GdkWindow *ancestor_window, 
				     gint      *ancestor_x, 
				     gint      *ancestor_y)
{
  GdkWindowObject *child_private = GDK_WINDOW_OBJECT (child_window);
  GdkWindowObject *ancestor_private = GDK_WINDOW_OBJECT (ancestor_window);

  while (child_private != ancestor_private)
    {
      child_x += child_private->x;
      child_y += child_private->y;

      child_private = child_private->parent;
    }

  *ancestor_x = child_x;
  *ancestor_y = child_y;
}

void
_gdk_quartz_window_debug_highlight (GdkWindow *window, gint number)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  gint x, y;
  gint gx, gy;
  GdkWindow *toplevel;
  gint tx, ty;
  static NSWindow *debug_window[10];
  static NSRect old_rect[10];
  NSRect rect;
  NSColor *color;

  g_return_if_fail (number >= 0 && number <= 9);

  if (window == _gdk_root)
    return;

  if (window == NULL)
    {
      if (debug_window[number])
        [debug_window[number] close];
      debug_window[number] = NULL;

      return;
    }

  toplevel = gdk_window_get_effective_toplevel (window);
  get_ancestor_coordinates_from_child (window, 0, 0, toplevel, &x, &y);

  gdk_window_get_origin (toplevel, &tx, &ty);
  x += tx;
  y += ty;

  _gdk_quartz_window_gdk_xy_to_xy (x, y + private->height,
                                   &gx, &gy);

  rect = NSMakeRect (gx, gy, private->width, private->height);

  if (debug_window[number] && NSEqualRects (rect, old_rect[number]))
    return;

  old_rect[number] = rect;

  if (debug_window[number])
    [debug_window[number] close];

  debug_window[number] = [[NSWindow alloc] initWithContentRect:rect
                                                     styleMask:NSBorderlessWindowMask
			                               backing:NSBackingStoreBuffered
			                                 defer:NO];

  switch (number)
    {
    case 0:
      color = [NSColor redColor];
      break;
    case 1:
      color = [NSColor blueColor];
      break;
    case 2:
      color = [NSColor greenColor];
      break;
    case 3:
      color = [NSColor yellowColor];
      break;
    case 4:
      color = [NSColor brownColor];
      break;
    case 5:
      color = [NSColor purpleColor];
      break;
    default:
      color = [NSColor blackColor];
      break;
    }

  [debug_window[number] setBackgroundColor:color];
  [debug_window[number] setAlphaValue:0.4];
  [debug_window[number] setOpaque:NO];
  [debug_window[number] setReleasedWhenClosed:YES];
  [debug_window[number] setIgnoresMouseEvents:YES];
  [debug_window[number] setLevel:NSFloatingWindowLevel];

  [debug_window[number] orderFront:nil];
}

gboolean
_gdk_quartz_window_is_ancestor (GdkWindow *ancestor,
                                GdkWindow *window)
{
  if (ancestor == NULL || window == NULL)
    return FALSE;

  return (gdk_window_get_parent (window) == ancestor ||
          _gdk_quartz_window_is_ancestor (ancestor, 
                                          gdk_window_get_parent (window)));
}


/* See notes on top of gdkscreen-quartz.c */
void
_gdk_quartz_window_gdk_xy_to_xy (gint  gdk_x,
                                 gint  gdk_y,
                                 gint *ns_x,
                                 gint *ns_y)
{
  GdkScreenQuartz *screen_quartz = GDK_SCREEN_QUARTZ (_gdk_screen);

  if (ns_y)
    *ns_y = screen_quartz->height - gdk_y + screen_quartz->min_y;

  if (ns_x)
    *ns_x = gdk_x + screen_quartz->min_x;
}

void
_gdk_quartz_window_xy_to_gdk_xy (gint  ns_x,
                                 gint  ns_y,
                                 gint *gdk_x,
                                 gint *gdk_y)
{
  GdkScreenQuartz *screen_quartz = GDK_SCREEN_QUARTZ (_gdk_screen);

  if (gdk_y)
    *gdk_y = screen_quartz->height - ns_y + screen_quartz->min_y;

  if (gdk_x)
    *gdk_x = ns_x - screen_quartz->min_x;
}

void
_gdk_quartz_window_nspoint_to_gdk_xy (NSPoint  point,
                                      gint    *x,
                                      gint    *y)
{
  _gdk_quartz_window_xy_to_gdk_xy (point.x, point.y,
                                   x, y);
}

static GdkWindow *
find_child_window_helper (GdkWindow *window,
			  gint       x,
			  gint       y,
			  gint       x_offset,
			  gint       y_offset)
{
  GdkWindowImplQuartz *impl;
  GList *l;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (window == _gdk_root)
    update_toplevel_order ();

  for (l = impl->sorted_children; l; l = l->next)
    {
      GdkWindowObject *child_private = l->data;
      GdkWindowImplQuartz *child_impl = GDK_WINDOW_IMPL_QUARTZ (child_private->impl);
      int temp_x, temp_y;

      if (!GDK_WINDOW_IS_MAPPED (child_private))
	continue;

      temp_x = x_offset + child_private->x;
      temp_y = y_offset + child_private->y;

      /* Special-case the root window. We have to include the title
       * bar in the checks, otherwise the window below the title bar
       * will be found i.e. events punch through. (If we can find a
       * better way to deal with the events in gdkevents-quartz, this
       * might not be needed.)
       */
      if (window == _gdk_root)
        {
          NSRect frame = NSMakeRect (0, 0, 100, 100);
          NSRect content;
          NSUInteger mask;
          int titlebar_height;

          mask = [child_impl->toplevel styleMask];

          /* Get the title bar height. */
          content = [NSWindow contentRectForFrameRect:frame
                                            styleMask:mask];
          titlebar_height = frame.size.height - content.size.height;

          if (titlebar_height > 0 &&
              x >= temp_x && y >= temp_y - titlebar_height &&
              x < temp_x + child_private->width && y < temp_y)
            {
              /* The root means "unknown" i.e. a window not managed by
               * GDK.
               */
              return (GdkWindow *)_gdk_root;
            }
        }

      if (x >= temp_x && y >= temp_y &&
	  x < temp_x + child_private->width && y < temp_y + child_private->height)
	{
	  /* Look for child windows. */
	  return find_child_window_helper (l->data,
					   x, y,
					   temp_x, temp_y);
	}
    }
  
  return window;
}

/* Given a GdkWindow and coordinates relative to it, returns the
 * innermost subwindow that contains the point. If the coordinates are
 * outside the passed in window, NULL is returned.
 */
GdkWindow *
_gdk_quartz_window_find_child (GdkWindow *window,
			       gint       x,
			       gint       y)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);

  if (x >= 0 && y >= 0 && x < private->width && y < private->height)
    return find_child_window_helper (window, x, y, 0, 0);

  return NULL;
}


void
_gdk_quartz_window_did_become_main (GdkWindow *window)
{
  main_window_stack = g_slist_remove (main_window_stack, window);

  if (GDK_WINDOW_OBJECT (window)->window_type != GDK_WINDOW_TEMP)
    main_window_stack = g_slist_prepend (main_window_stack, window);

  clear_toplevel_order ();
}

void
_gdk_quartz_window_did_resign_main (GdkWindow *window)
{
  GdkWindow *new_window = NULL;

  if (main_window_stack)
    new_window = main_window_stack->data;
  else
    {
      GList *toplevels;

      toplevels = gdk_window_get_toplevels ();
      if (toplevels)
        new_window = toplevels->data;
      g_list_free (toplevels);
    }

  if (new_window &&
      new_window != window &&
      GDK_WINDOW_IS_MAPPED (new_window) &&
      WINDOW_IS_TOPLEVEL (new_window))
    {
      GdkWindowObject *private = (GdkWindowObject *) new_window;
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
    }

  clear_toplevel_order ();
}

static NSScreen *
get_nsscreen_for_point (gint x, gint y)
{
  int i;
  NSArray *screens;
  NSScreen *screen = NULL;

  GDK_QUARTZ_ALLOC_POOL;

  screens = [NSScreen screens];

  for (i = 0; i < [screens count]; i++)
    {
      NSRect rect = [[screens objectAtIndex:i] frame];

      if (x >= rect.origin.x && x <= rect.origin.x + rect.size.width &&
          y >= rect.origin.y && y <= rect.origin.y + rect.size.height)
        {
          screen = [screens objectAtIndex:i];
          break;
        }
    }

  GDK_QUARTZ_RELEASE_POOL;

  return screen;
}

void
_gdk_window_impl_new (GdkWindow     *window,
		      GdkWindow     *real_parent,
		      GdkScreen     *screen,
		      GdkVisual     *visual,
		      GdkEventMask   event_mask,
		      GdkWindowAttr *attributes,
		      gint           attributes_mask)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkDrawableImplQuartz *draw_impl;
  GdkWindowImplQuartz *parent_impl;

  GDK_QUARTZ_ALLOC_POOL;

  private = (GdkWindowObject *)window;

  impl = g_object_new (_gdk_window_impl_get_type (), NULL);
  private->impl = (GdkDrawable *)impl;
  draw_impl = GDK_DRAWABLE_IMPL_QUARTZ (impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  parent_impl = GDK_WINDOW_IMPL_QUARTZ (private->parent->impl);

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT)
	{
	  /* The common code warns for this case */
          parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (_gdk_root)->impl);
	}
    }

  if (!private->input_only)
    {
      if (attributes_mask & GDK_WA_COLORMAP)
	{
	  draw_impl->colormap = attributes->colormap;
	  g_object_ref (attributes->colormap);
	}
      else
	{
	  if (visual == gdk_screen_get_system_visual (_gdk_screen))
	    {
	      draw_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
	      g_object_ref (draw_impl->colormap);
	    }
	  else if (visual == gdk_screen_get_rgba_visual (_gdk_screen))
	    {
	      draw_impl->colormap = gdk_screen_get_rgba_colormap (_gdk_screen);
	      g_object_ref (draw_impl->colormap);
	    }
	  else
	    {
	      draw_impl->colormap = gdk_colormap_new (visual, FALSE);
	    }
	}
    }
  else
    {
      draw_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
      g_object_ref (draw_impl->colormap);
    }

  impl->needs_display_region = NULL;

  /* Maintain the z-ordered list of children. */
  if (private->parent != (GdkWindowObject *)_gdk_root)
    parent_impl->sorted_children = g_list_prepend (parent_impl->sorted_children, window);
  else
    clear_toplevel_order ();

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  switch (attributes->window_type) 
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      {
        NSScreen *screen;
        NSRect screen_rect;
        NSRect content_rect;
        NSUInteger style_mask;
        int nx, ny;
        const char *title;

        /* initWithContentRect will place on the mainScreen by default.
         * We want to select the screen to place on ourselves.  We need
         * to find the screen the window will be on and correct the
         * content_rect coordinates to be relative to that screen.
         */
        _gdk_quartz_window_gdk_xy_to_xy (private->x, private->y, &nx, &ny);

        screen = get_nsscreen_for_point (nx, ny);
        screen_rect = [screen frame];
        nx -= screen_rect.origin.x;
        ny -= screen_rect.origin.y;

        content_rect = NSMakeRect (nx, ny - private->height,
                                   private->width,
                                   private->height);

        if (attributes->window_type == GDK_WINDOW_TEMP ||
            attributes->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN)
          {
            style_mask = NSBorderlessWindowMask;
          }
        else
          {
            style_mask = (NSTitledWindowMask |
                          NSClosableWindowMask |
                          NSMiniaturizableWindowMask |
                          NSResizableWindowMask);
          }

	impl->toplevel = [[GdkQuartzWindow alloc] initWithContentRect:content_rect 
			                                    styleMask:style_mask
			                                      backing:NSBackingStoreBuffered
			                                        defer:NO
                                                                screen:screen];

	if (attributes_mask & GDK_WA_TITLE)
	  title = attributes->title;
	else
	  title = get_default_title ();

	gdk_window_set_title (window, title);
  
	if (draw_impl->colormap == gdk_screen_get_rgba_colormap (_gdk_screen))
	  {
	    [impl->toplevel setOpaque:NO];
	    [impl->toplevel setBackgroundColor:[NSColor clearColor]];
	  }

        content_rect.origin.x = 0;
        content_rect.origin.y = 0;

	impl->view = [[GdkQuartzView alloc] initWithFrame:content_rect];
	[impl->view setGdkWindow:window];
	[impl->toplevel setContentView:impl->view];
	[impl->view release];
      }
      break;

    case GDK_WINDOW_CHILD:
      {
	GdkWindowImplQuartz *parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (private->parent)->impl);

	if (!private->input_only)
	  {
	    NSRect frame_rect = NSMakeRect (private->x + private->parent->abs_x,
                                            private->y + private->parent->abs_y,
                                            private->width,
                                            private->height);
	
	    impl->view = [[GdkQuartzView alloc] initWithFrame:frame_rect];
	    
	    [impl->view setGdkWindow:window];

	    /* GdkWindows should be hidden by default */
	    [impl->view setHidden:YES];
	    [parent_impl->view addSubview:impl->view];
	    [impl->view release];
	  }
      }
      break;

    default:
      g_assert_not_reached ();
    }

  GDK_QUARTZ_RELEASE_POOL;

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);
}

void
_gdk_quartz_window_update_position (GdkWindow *window)
{
  NSRect frame_rect;
  NSRect content_rect;
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  GDK_QUARTZ_ALLOC_POOL;

  frame_rect = [impl->toplevel frame];
  content_rect = [impl->toplevel contentRectForFrameRect:frame_rect];

  _gdk_quartz_window_xy_to_gdk_xy (content_rect.origin.x,
                                   content_rect.origin.y + content_rect.size.height,
                                   &private->x, &private->y);


  GDK_QUARTZ_RELEASE_POOL;
}

void
_gdk_windowing_update_window_sizes (GdkScreen *screen)
{
  GList *windows, *list;
  GdkWindowObject *private = (GdkWindowObject *)_gdk_root;

  /* The size of the root window is so that it can contain all
   * monitors attached to this machine.  The monitors are laid out
   * within this root window.  We calculate the size of the root window
   * and the positions of the different monitors in gdkscreen-quartz.c.
   *
   * This data is updated when the monitor configuration is changed.
   */
  private->x = 0;
  private->y = 0;
  private->abs_x = 0;
  private->abs_y = 0;
  private->width = gdk_screen_get_width (screen);
  private->height = gdk_screen_get_height (screen);

  windows = gdk_screen_get_toplevel_windows (screen);

  for (list = windows; list; list = list->next)
    _gdk_quartz_window_update_position (list->data);

  g_list_free (windows);
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkDrawableImplQuartz *drawable_impl;

  g_assert (_gdk_root == NULL);

  _gdk_root = g_object_new (GDK_TYPE_WINDOW, NULL);

  private = (GdkWindowObject *)_gdk_root;
  private->impl = g_object_new (_gdk_window_impl_get_type (), NULL);
  private->impl_window = private;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (_gdk_root)->impl);

  _gdk_windowing_update_window_sizes (_gdk_screen);

  private->state = 0; /* We don't want GDK_WINDOW_STATE_WITHDRAWN here */
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = 24;
  private->viewable = TRUE;

  drawable_impl = GDK_DRAWABLE_IMPL_QUARTZ (private->impl);
  
  drawable_impl->wrapper = GDK_DRAWABLE (private);
  drawable_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
  g_object_ref (drawable_impl->colormap);
}

static void
_gdk_quartz_window_destroy (GdkWindow *window,
                            gboolean   recursing,
                            gboolean   foreign_destroy)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkWindowObject *parent;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  main_window_stack = g_slist_remove (main_window_stack, window);

  g_list_free (impl->sorted_children);
  impl->sorted_children = NULL;

  parent = private->parent;
  if (parent)
    {
      GdkWindowImplQuartz *parent_impl = GDK_WINDOW_IMPL_QUARTZ (parent->impl);

      parent_impl->sorted_children = g_list_remove (parent_impl->sorted_children, window);
    }

  _gdk_quartz_drawable_finish (GDK_DRAWABLE (impl));

  if (!recursing && !foreign_destroy)
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel close];
      else if (impl->view)
	[impl->view removeFromSuperview];

      GDK_QUARTZ_RELEASE_POOL;
    }
}

void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
  /* Foreign windows aren't supported in OSX. */
}

/* FIXME: This might be possible to simplify with client-side windows. Also
 * note that already_mapped is not used yet, see the x11 backend.
*/
static void
gdk_window_quartz_show (GdkWindow *window, gboolean already_mapped)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  gboolean focus_on_map;

  GDK_QUARTZ_ALLOC_POOL;

  if (!GDK_WINDOW_IS_MAPPED (window))
    focus_on_map = private->focus_on_map;
  else
    focus_on_map = TRUE;

  if (impl->toplevel && WINDOW_IS_TOPLEVEL (window))
    {
      gboolean make_key;

      make_key = (private->accept_focus && focus_on_map &&
                  private->window_type != GDK_WINDOW_TEMP);

      [(GdkQuartzWindow*)impl->toplevel showAndMakeKey:make_key];
      clear_toplevel_order ();

      _gdk_quartz_events_send_map_event (window);
    }
  else
    {
      [impl->view setHidden:NO];
    }

  [impl->view setNeedsDisplay:YES];

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_WITHDRAWN, 0);

  if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
    gdk_window_maximize (window);

  if (private->state & GDK_WINDOW_STATE_ICONIFIED)
    gdk_window_iconify (window);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    _gdk_quartz_window_attach_to_parent (window);

  GDK_QUARTZ_RELEASE_POOL;
}

/* Temporarily unsets the parent window, if the window is a
 * transient. 
 */
void
_gdk_quartz_window_detach_from_parent (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  
  g_return_if_fail (impl->toplevel != NULL);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    {
      GdkWindowImplQuartz *parent_impl;

      parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (impl->transient_for)->impl);
      [parent_impl->toplevel removeChildWindow:impl->toplevel];
      clear_toplevel_order ();
    }
}

/* Re-sets the parent window, if the window is a transient. */
void
_gdk_quartz_window_attach_to_parent (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  
  g_return_if_fail (impl->toplevel != NULL);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    {
      GdkWindowImplQuartz *parent_impl;

      parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (impl->transient_for)->impl);
      [parent_impl->toplevel addChildWindow:impl->toplevel ordered:NSWindowAbove];
      clear_toplevel_order ();
    }
}

void
gdk_window_quartz_hide (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;

  /* Make sure we're not stuck in fullscreen mode. */
  if (get_fullscreen_geometry (window))
    SetSystemUIMode (kUIModeNormal, 0);

  check_grab_unmap (window);

  _gdk_window_clear_update_area (window);

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (window && WINDOW_IS_TOPLEVEL (window))
    {
     /* Update main window. */
      main_window_stack = g_slist_remove (main_window_stack, window);
      if ([NSApp mainWindow] == impl->toplevel)
        _gdk_quartz_window_did_resign_main (window);

      if (impl->transient_for)
        _gdk_quartz_window_detach_from_parent (window);

      [(GdkQuartzWindow*)impl->toplevel hide];
    }
  else if (impl->view)
    {
      [impl->view setHidden:YES];
    }
}

void
gdk_window_quartz_withdraw (GdkWindow *window)
{
  gdk_window_hide (window);
}

static void
move_resize_window_internal (GdkWindow *window,
			     gint       x,
			     gint       y,
			     gint       width,
			     gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;
  GdkRectangle old_visible;
  GdkRectangle new_visible;
  GdkRectangle scroll_rect;
  GdkRegion *old_region;
  GdkRegion *expose_region;
  NSSize delta;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if ((x == -1 || (x == private->x)) &&
      (y == -1 || (y == private->y)) &&
      (width == -1 || (width == private->width)) &&
      (height == -1 || (height == private->height)))
    {
      return;
    }

  if (!impl->toplevel)
    {
      /* The previously visible area of this window in a coordinate
       * system rooted at the origin of this window.
       */
      old_visible.x = -private->x;
      old_visible.y = -private->y;

      gdk_window_get_size (GDK_DRAWABLE (private->parent), 
                           &old_visible.width, 
                           &old_visible.height);
    }

  if (x != -1)
    {
      delta.width = x - private->x;
      private->x = x;
    }
  else
    {
      delta.width = 0;
    }

  if (y != -1)
    {
      delta.height = y - private->y;
      private->y = y;
    }
  else
    {
      delta.height = 0;
    }

  if (width != -1)
    private->width = width;

  if (height != -1)
    private->height = height;

  GDK_QUARTZ_ALLOC_POOL;

  if (impl->toplevel)
    {
      NSRect content_rect;
      NSRect frame_rect;
      gint gx, gy;

      _gdk_quartz_window_gdk_xy_to_xy (private->x, private->y + private->height,
                                       &gx, &gy);

      content_rect = NSMakeRect (gx, gy, private->width, private->height);

      frame_rect = [impl->toplevel frameRectForContentRect:content_rect];
      [impl->toplevel setFrame:frame_rect display:YES];
    }
  else 
    {
      if (!private->input_only)
        {
          NSRect nsrect;

          nsrect = NSMakeRect (private->x, private->y, private->width, private->height);

          /* The newly visible area of this window in a coordinate
           * system rooted at the origin of this window.
           */
          new_visible.x = -private->x;
          new_visible.y = -private->y;
          new_visible.width = old_visible.width;   /* parent has not changed size */
          new_visible.height = old_visible.height; /* parent has not changed size */

          expose_region = gdk_region_rectangle (&new_visible);
          old_region = gdk_region_rectangle (&old_visible);
          gdk_region_subtract (expose_region, old_region);

          /* Determine what (if any) part of the previously visible
           * part of the window can be copied without a redraw
           */
          scroll_rect = old_visible;
          scroll_rect.x -= delta.width;
          scroll_rect.y -= delta.height;
          gdk_rectangle_intersect (&scroll_rect, &old_visible, &scroll_rect);

          if (!gdk_region_empty (expose_region))
            {
              GdkRectangle* rects;
              gint n_rects;
              gint n;

              if (scroll_rect.width != 0 && scroll_rect.height != 0)
                {
                  [impl->view scrollRect:NSMakeRect (scroll_rect.x,
                                                     scroll_rect.y,
                                                     scroll_rect.width,
                                                     scroll_rect.height)
			              by:delta];
                }

              [impl->view setFrame:nsrect];

              gdk_region_get_rectangles (expose_region, &rects, &n_rects);

              for (n = 0; n < n_rects; ++n)
                _gdk_quartz_window_set_needs_display_in_rect (window, &rects[n]);

              g_free (rects);
            }
          else
            {
              [impl->view setFrame:nsrect];
              [impl->view setNeedsDisplay:YES];
            }

          gdk_region_destroy (expose_region);
          gdk_region_destroy (old_region);
        }
    }

  GDK_QUARTZ_RELEASE_POOL;
}

static inline void
window_quartz_move (GdkWindow *window,
                    gint       x,
                    gint       y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (((GdkWindowObject *)window)->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  move_resize_window_internal (window, x, y, -1, -1);
}

static inline void
window_quartz_resize (GdkWindow *window,
                      gint       width,
                      gint       height)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (((GdkWindowObject *)window)->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  move_resize_window_internal (window, -1, -1, width, height);
}

static inline void
window_quartz_move_resize (GdkWindow *window,
                           gint       x,
                           gint       y,
                           gint       width,
                           gint       height)
{
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  move_resize_window_internal (window, x, y, width, height);
}

static void
gdk_window_quartz_move_resize (GdkWindow *window,
                               gboolean   with_move,
                               gint       x,
                               gint       y,
                               gint       width,
                               gint       height)
{
  if (with_move && (width < 0 && height < 0))
    window_quartz_move (window, x, y);
  else
    {
      if (with_move)
        window_quartz_move_resize (window, x, y, width, height);
      else
        window_quartz_resize (window, width, height);
    }
}

/* FIXME: This might need fixing (reparenting didn't work before client-side
 * windows either).
 */
static gboolean
gdk_window_quartz_reparent (GdkWindow *window,
                            GdkWindow *new_parent,
                            gint       x,
                            gint       y)
{
  GdkWindowObject *private, *old_parent_private, *new_parent_private;
  GdkWindowImplQuartz *impl, *old_parent_impl, *new_parent_impl;
  NSView *view, *new_parent_view;

  if (new_parent == _gdk_root)
    {
      /* Could be added, just needs implementing. */
      g_warning ("Reparenting to root window is not supported yet in the Mac OS X backend");
      return FALSE;
    }

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  view = impl->view;

  new_parent_private = GDK_WINDOW_OBJECT (new_parent);
  new_parent_impl = GDK_WINDOW_IMPL_QUARTZ (new_parent_private->impl);
  new_parent_view = new_parent_impl->view;

  old_parent_private = GDK_WINDOW_OBJECT (private->parent);
  old_parent_impl = GDK_WINDOW_IMPL_QUARTZ (old_parent_private->impl);

  [view retain];

  [view removeFromSuperview];
  [new_parent_view addSubview:view];

  [view release];

  private->parent = new_parent_private;

  if (old_parent_private)
    {
      old_parent_impl->sorted_children = g_list_remove (old_parent_impl->sorted_children, window);
    }

  new_parent_impl->sorted_children = g_list_prepend (new_parent_impl->sorted_children, window);

  return FALSE;
}

/* Get the toplevel ordering from NSApp and update our own list. We do
 * this on demand since the NSApp's list is not up to date directly
 * after we get windowDidBecomeMain.
 */
static void
update_toplevel_order (void)
{
  GdkWindowObject *root;
  GdkWindowImplQuartz *root_impl;
  NSEnumerator *enumerator;
  id nswindow;
  GList *toplevels = NULL;

  root = GDK_WINDOW_OBJECT (_gdk_root);
  root_impl = GDK_WINDOW_IMPL_QUARTZ (root->impl);

  if (root_impl->sorted_children)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  enumerator = [[NSApp orderedWindows] objectEnumerator];
  while ((nswindow = [enumerator nextObject]))
    {
      GdkWindow *window;

      if (![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
        continue;

      window = [(GdkQuartzView *)[nswindow contentView] gdkWindow];
      toplevels = g_list_prepend (toplevels, window);
    }

  GDK_QUARTZ_RELEASE_POOL;

  root_impl->sorted_children = g_list_reverse (toplevels);
}

static void
clear_toplevel_order (void)
{
  GdkWindowObject *root;
  GdkWindowImplQuartz *root_impl;

  root = GDK_WINDOW_OBJECT (_gdk_root);
  root_impl = GDK_WINDOW_IMPL_QUARTZ (root->impl);

  g_list_free (root_impl->sorted_children);
  root_impl->sorted_children = NULL;
}

static void
gdk_window_quartz_raise (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (WINDOW_IS_TOPLEVEL (window))
    {
      GdkWindowImplQuartz *impl;

      impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
      [impl->toplevel orderFront:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkWindowObject *parent = GDK_WINDOW_OBJECT (window)->parent;

      if (parent)
        {
          GdkWindowImplQuartz *impl;

          impl = (GdkWindowImplQuartz *)parent->impl;

          impl->sorted_children = g_list_remove (impl->sorted_children, window);
          impl->sorted_children = g_list_prepend (impl->sorted_children, window);
        }
    }
}

static void
gdk_window_quartz_lower (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (WINDOW_IS_TOPLEVEL (window))
    {
      GdkWindowImplQuartz *impl;

      impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
      [impl->toplevel orderBack:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkWindowObject *parent = GDK_WINDOW_OBJECT (window)->parent;

      if (parent)
        {
          GdkWindowImplQuartz *impl;

          impl = (GdkWindowImplQuartz *)parent->impl;

          impl->sorted_children = g_list_remove (impl->sorted_children, window);
          impl->sorted_children = g_list_append (impl->sorted_children, window);
        }
    }
}

static void
gdk_window_quartz_restack_toplevel (GdkWindow *window,
				    GdkWindow *sibling,
				    gboolean   above)
{
  GdkWindowImplQuartz *impl;
  gint sibling_num;

  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *)sibling)->impl);
  sibling_num = [impl->toplevel windowNumber];

  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *)window)->impl);

  if (above)
    [impl->toplevel orderWindow:NSWindowAbove relativeTo:sibling_num];
  else
    [impl->toplevel orderWindow:NSWindowBelow relativeTo:sibling_num];
}

static void
gdk_window_quartz_set_background (GdkWindow      *window,
                                  const GdkColor *color)
{
  /* FIXME: We could theoretically set the background color for toplevels
   * here. (Currently we draw the background before emitting expose events)
   */
  GdkWindowImplQuartz *impl;
  GdkWindowObject *private;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (color)
    {
      impl->background_color = *color;
      impl->background_color_set = TRUE;
    }
  else
    impl->background_color_set = FALSE;
}

static void
gdk_window_quartz_set_back_pixmap (GdkWindow *window,
                                   GdkPixmap *pixmap)
{
  /* FIXME: Could theoretically set some background image here. (Currently
   * the back pixmap is drawn before emitting expose events.
   */
}

static void
gdk_window_quartz_set_cursor (GdkWindow *window,
                              GdkCursor *cursor)
{
  GdkCursorPrivate *cursor_private;
  NSCursor *nscursor;

  cursor_private = (GdkCursorPrivate *)cursor;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!cursor)
    nscursor = [NSCursor arrowCursor];
  else 
    nscursor = cursor_private->nscursor;

  [nscursor set];
}

static void
gdk_window_quartz_get_geometry (GdkWindow *window,
                                gint      *x,
                                gint      *y,
                                gint      *width,
                                gint      *height,
                                gint      *depth)
{
  GdkWindowImplQuartz *impl;
  GdkWindowObject *private;
  NSRect ns_rect;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  private = GDK_WINDOW_OBJECT (window);
  if (window == _gdk_root)
    {
      if (x) 
        *x = 0;
      if (y) 
        *y = 0;

      if (width) 
        *width = private->width;
      if (height)
        *height = private->height;
    }
  else if (WINDOW_IS_TOPLEVEL (window))
    {
      ns_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

      /* This doesn't work exactly as in X. There doesn't seem to be a
       * way to get the coords relative to the parent window (usually
       * the window frame), but that seems useless except for
       * borderless windows where it's relative to the root window. So
       * we return (0, 0) (should be something like (0, 22)) for
       * windows with borders and the root relative coordinates
       * otherwise.
       */
      if ([impl->toplevel styleMask] == NSBorderlessWindowMask)
        {
          _gdk_quartz_window_xy_to_gdk_xy (ns_rect.origin.x,
                                           ns_rect.origin.y + ns_rect.size.height,
                                           x, y);
        }
      else 
        {
          if (x)
            *x = 0;
          if (y)
            *y = 0;
        }

      if (width)
        *width = ns_rect.size.width;
      if (height)
        *height = ns_rect.size.height;
    }
  else
    {
      ns_rect = [impl->view frame];
      
      if (x)
        *x = ns_rect.origin.x;
      if (y)
        *y = ns_rect.origin.y;
      if (width)
        *width  = ns_rect.size.width;
      if (height)
        *height = ns_rect.size.height;
    }
    
  if (depth)
      *depth = gdk_drawable_get_depth (window);
}

static gint
gdk_window_quartz_get_root_coords (GdkWindow *window,
                                   gint       x,
                                   gint       y,
                                   gint      *root_x,
                                   gint      *root_y)
{
  GdkWindowObject *private;
  int tmp_x = 0, tmp_y = 0;
  GdkWindow *toplevel;
  NSRect content_rect;
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window)) 
    {
      if (root_x)
	*root_x = 0;
      if (root_y)
	*root_y = 0;
      
      return 0;
    }

  if (window == _gdk_root)
    {
      if (root_x)
        *root_x = x;
      if (root_y)
        *root_y = y;

      return 1;
    }
  
  private = GDK_WINDOW_OBJECT (window);

  toplevel = gdk_window_get_toplevel (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);

  content_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

  _gdk_quartz_window_xy_to_gdk_xy (content_rect.origin.x,
                                   content_rect.origin.y + content_rect.size.height,
                                   &tmp_x, &tmp_y);

  tmp_x += x;
  tmp_y += y;

  while (private != GDK_WINDOW_OBJECT (toplevel))
    {
      if (_gdk_window_has_impl ((GdkWindow *)private))
        {
          tmp_x += private->x;
          tmp_y += private->y;
        }

      private = private->parent;
    }

  if (root_x)
    *root_x = tmp_x;
  if (root_y)
    *root_y = tmp_y;

  return TRUE;
}

static gboolean
gdk_window_quartz_get_deskrelative_origin (GdkWindow *window,
                                           gint      *x,
                                           gint      *y)
{
  return gdk_window_get_origin (window, x, y);
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkRectangle rect;

  rect.x = 0;
  rect.y = 0;
  
  gdk_window_get_frame_extents (window, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;
}

/* Returns coordinates relative to the passed in window. */
static GdkWindow *
gdk_window_quartz_get_pointer_helper (GdkWindow       *window,
                                      gint            *x,
                                      gint            *y,
                                      GdkModifierType *mask)
{
  GdkWindowObject *toplevel;
  GdkWindowObject *private;
  NSPoint point;
  gint x_tmp, y_tmp;
  GdkWindow *found_window;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    {
      *x = 0;
      *y = 0;
      *mask = 0;
      return NULL;
    }
  
  toplevel = GDK_WINDOW_OBJECT (gdk_window_get_effective_toplevel (window));

  *mask = _gdk_quartz_events_get_current_keyboard_modifiers ()
	| _gdk_quartz_events_get_current_mouse_modifiers ();

  /* Get the y coordinate, needs to be flipped. */
  if (window == _gdk_root)
    {
      point = [NSEvent mouseLocation];
      _gdk_quartz_window_nspoint_to_gdk_xy (point, &x_tmp, &y_tmp);
    }
  else
    {
      GdkWindowImplQuartz *impl;
      NSWindow *nswindow;

      impl = GDK_WINDOW_IMPL_QUARTZ (toplevel->impl);
      private = GDK_WINDOW_OBJECT (toplevel);
      nswindow = impl->toplevel;

      point = [nswindow mouseLocationOutsideOfEventStream];

      x_tmp = point.x;
      y_tmp = private->height - point.y;

      window = (GdkWindow *)toplevel;
    }

  found_window = _gdk_quartz_window_find_child (window, x_tmp, y_tmp);

  /* We never return the root window. */
  if (found_window == _gdk_root)
    found_window = NULL;

  *x = x_tmp;
  *y = y_tmp;

  return found_window;
}

static gboolean
gdk_window_quartz_get_pointer (GdkWindow       *window,
                               gint            *x,
                               gint            *y,
                               GdkModifierType *mask)
{
  return gdk_window_quartz_get_pointer_helper (window, x, y, mask) != NULL;
}

/* Returns coordinates relative to the root. */
void
_gdk_windowing_get_pointer (GdkDisplay       *display,
                            GdkScreen       **screen,
                            gint             *x,
                            gint             *y,
                            GdkModifierType  *mask)
{
  g_return_if_fail (display == _gdk_display);
  
  *screen = _gdk_screen;
  gdk_window_quartz_get_pointer_helper (_gdk_root, x, y, mask);
}

void
gdk_display_warp_pointer (GdkDisplay *display,
			  GdkScreen  *screen,
			  gint        x,
			  gint        y)
{
  CGDisplayMoveCursorToPoint (CGMainDisplayID (), CGPointMake (x, y));
}

/* Returns coordinates relative to the found window. */
GdkWindow *
_gdk_windowing_window_at_pointer (GdkDisplay      *display,
				  gint            *win_x,
				  gint            *win_y,
                                  GdkModifierType *mask,
				  gboolean         get_toplevel)
{
  GdkWindow *found_window;
  gint x, y;
  GdkModifierType tmp_mask = 0;

  found_window = gdk_window_quartz_get_pointer_helper (_gdk_root,
                                                       &x, &y,
                                                       &tmp_mask);
  if (found_window)
    {
      GdkWindowObject *private;

      /* The coordinates returned above are relative the root, we want
       * coordinates relative the window here. 
       */
      private = GDK_WINDOW_OBJECT (found_window);
      while (private != GDK_WINDOW_OBJECT (_gdk_root))
	{
	  x -= private->x;
	  y -= private->y;
	  
	  private = private->parent;
	}

      *win_x = x;
      *win_y = y;
    }
  else
    {
      /* Mimic the X backend here, -1,-1 for unknown windows. */
      *win_x = -1;
      *win_y = -1;
    }

  if (mask)
    *mask = tmp_mask;

  if (get_toplevel)
    {
      GdkWindowObject *w = (GdkWindowObject *)found_window;
      /* Requested toplevel, find it. */
      /* TODO: This can be implemented more efficient by never
	 recursing into children in the first place */
      if (w)
	{
	  /* Convert to toplevel */
	  while (w->parent != NULL &&
		 w->parent->window_type != GDK_WINDOW_ROOT)
	    {
	      *win_x += w->x;
	      *win_y += w->y;
	      w = w->parent;
	    }
	  found_window = (GdkWindow *)w;
	}
    }

  return found_window;
}

static GdkEventMask  
gdk_window_quartz_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_OBJECT (window)->event_mask;
}

static void
gdk_window_quartz_set_events (GdkWindow       *window,
                              GdkEventMask     event_mask)
{
  /* The mask is set in the common code. */
}

void
gdk_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

void 
gdk_window_set_geometry_hints (GdkWindow         *window,
			       const GdkGeometry *geometry,
			       GdkWindowHints     geom_mask)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (geometry != NULL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
  
  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *) window)->impl);
  if (!impl->toplevel)
    return;

  if (geom_mask & GDK_HINT_POS)
    {
      /* FIXME: Implement */
    }

  if (geom_mask & GDK_HINT_USER_POS)
    {
      /* FIXME: Implement */
    }

  if (geom_mask & GDK_HINT_USER_SIZE)
    {
      /* FIXME: Implement */
    }
  
  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      NSSize size;

      size.width = geometry->min_width;
      size.height = geometry->min_height;

      [impl->toplevel setContentMinSize:size];
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      NSSize size;

      size.width = geometry->max_width;
      size.height = geometry->max_height;

      [impl->toplevel setContentMaxSize:size];
    }
  
  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      /* FIXME: Implement */
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      NSSize size;

      size.width = geometry->width_inc;
      size.height = geometry->height_inc;

      [impl->toplevel setContentResizeIncrements:size];
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      NSSize size;

      if (geometry->min_aspect != geometry->max_aspect)
        {
          g_warning ("Only equal minimum and maximum aspect ratios are supported on Mac OS. Using minimum aspect ratio...");
        }

      size.width = geometry->min_aspect;
      size.height = 1.0;

      [impl->toplevel setContentAspectRatio:size];
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      /* FIXME: Implement */
    }
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *)window)->impl);

  if (impl->toplevel)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel setTitle:[NSString stringWithUTF8String:title]];
      GDK_QUARTZ_RELEASE_POOL;
    }
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

void
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  GdkWindowImplQuartz *window_impl;
  GdkWindowImplQuartz *parent_impl;

  if (GDK_WINDOW_DESTROYED (window)  || GDK_WINDOW_DESTROYED (parent) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  window_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  if (!window_impl->toplevel)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  if (window_impl->transient_for)
    {
      _gdk_quartz_window_detach_from_parent (window);

      g_object_unref (window_impl->transient_for);
      window_impl->transient_for = NULL;
    }

  parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (parent)->impl);
  if (parent_impl->toplevel)
    {
      /* We save the parent because it needs to be unset/reset when
       * hiding and showing the window. 
       */

      /* We don't set transients for tooltips, they are already
       * handled by the window level being the top one. If we do, then
       * the parent window will be brought to the top just because the
       * tooltip is, which is not what we want.
       */
      if (gdk_window_get_type_hint (window) != GDK_WINDOW_TYPE_HINT_TOOLTIP)
        {
          window_impl->transient_for = g_object_ref (parent);

          /* We only add the window if it is shown, otherwise it will
           * be shown unconditionally here. If it is not shown, the
           * window will be added in show() instead.
           */
          if (!(GDK_WINDOW_OBJECT (window)->state & GDK_WINDOW_STATE_WITHDRAWN))
            _gdk_quartz_window_attach_to_parent (window);
        }
    }
  
  GDK_QUARTZ_RELEASE_POOL;
}

static void
gdk_window_quartz_shape_combine_region (GdkWindow       *window,
                                        const GdkRegion *shape,
                                        gint             x,
                                        gint             y)
{
  /* FIXME: Implement */
}

static void
gdk_window_quartz_input_shape_combine_region (GdkWindow       *window,
                                              const GdkRegion *shape_region,
                                              gint             offset_x,
                                              gint             offset_y)
{
  /* FIXME: Implement */
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  /* FIXME: Implement */
}

void
gdk_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;  

  private->accept_focus = accept_focus != FALSE;
}

static gboolean 
gdk_window_quartz_set_static_gravities (GdkWindow *window,
                                        gboolean   use_static)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return FALSE;

  /* FIXME: Implement */
  return FALSE;
}

void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;  
  
  private->focus_on_map = focus_on_map != FALSE;
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  /* FIXME: Implement */
}

void          
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  /* FIXME: Implement */
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
	
  private = (GdkWindowObject*) window;
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  if (private->accept_focus && private->window_type != GDK_WINDOW_TEMP)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
      clear_toplevel_order ();
      GDK_QUARTZ_RELEASE_POOL;
    }
}

void
gdk_window_set_hints (GdkWindow *window,
		      gint       x,
		      gint       y,
		      gint       min_width,
		      gint       min_height,
		      gint       max_width,
		      gint       max_height,
		      gint       flags)
{
  /* FIXME: Implement */
}

static gint
window_type_hint_to_level (GdkWindowTypeHint hint)
{
  /*  the order in this switch statement corresponds to the actual
   *  stacking order: the first group is top, the last group is bottom
   */
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_DND:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return NSPopUpMenuWindowLevel;

    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      return NSStatusWindowLevel;

    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
      return NSTornOffMenuWindowLevel;

    case GDK_WINDOW_TYPE_HINT_DOCK:
      return NSFloatingWindowLevel; /* NSDockWindowLevel is deprecated, and not replaced */

    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_DIALOG:  /* Dialog window */
#if 1 // gtk 2.24.33
			return NSFloatingWindowLevel;
#endif
    case GDK_WINDOW_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_WINDOW_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
      return NSNormalWindowLevel;

    case GDK_WINDOW_TYPE_HINT_DESKTOP:
      return kCGDesktopWindowLevelKey; /* doesn't map to any real Cocoa model */

    default:
      break;
    }

  return NSNormalWindowLevel;
}

static gboolean
window_type_hint_to_shadow (GdkWindowTypeHint hint)
{
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_WINDOW_TYPE_HINT_DIALOG:  /* Dialog window */
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return TRUE;

    case GDK_WINDOW_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
    case GDK_WINDOW_TYPE_HINT_DESKTOP: /* N/A */
    case GDK_WINDOW_TYPE_HINT_DND:
      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
window_type_hint_to_hides_on_deactivate (GdkWindowTypeHint hint)
{
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *) window)->impl);

  impl->type_hint = hint;

  /* Match the documentation, only do something if we're not mapped yet. */
  if (GDK_WINDOW_IS_MAPPED (window))
    return;

  [impl->toplevel setHasShadow: window_type_hint_to_shadow (hint)];
  [impl->toplevel setLevel: window_type_hint_to_level (hint)];
  [impl->toplevel setHidesOnDeactivate: window_type_hint_to_hides_on_deactivate (hint)];
}

GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;
  
  return GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *) window)->impl)->type_hint;
}

void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
				  gboolean   skips_taskbar)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

void
gdk_window_set_skip_pager_hint (GdkWindow *window,
				gboolean   skips_pager)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (edge != GDK_WINDOW_EDGE_SOUTH_EAST)
    {
      g_warning ("Resizing is only implemented for GDK_WINDOW_EDGE_SOUTH_EAST on Mac OS");
      return;
    }

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!impl->toplevel)
    {
      g_warning ("Can't call gdk_window_begin_resize_drag on non-toplevel window");
      return;
    }

  [(GdkQuartzWindow *)impl->toplevel beginManualResize];
}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!impl->toplevel)
    {
      g_warning ("Can't call gdk_window_begin_move_drag on non-toplevel window");
      return;
    }

  [(GdkQuartzWindow *)impl->toplevel beginManualMove];
}

void
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  /* FIXME: Implement */
}

void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkWindowObject *private;
  GdkWindow *toplevel;
  GdkWindowImplQuartz *impl;
  NSRect ns_rect;

  g_return_if_fail (rect != NULL);

  private = GDK_WINDOW_OBJECT (window);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  toplevel = gdk_window_get_effective_toplevel (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);

  ns_rect = [impl->toplevel frame];

  _gdk_quartz_window_xy_to_gdk_xy (ns_rect.origin.x,
                                   ns_rect.origin.y + ns_rect.size.height,
                                   &rect->x, &rect->y);

  rect->width = ns_rect.size.width;
  rect->height = ns_rect.size.height;
}

/* Fake protocol to make gcc think that it's OK to call setStyleMask
   even if it isn't. We check to make sure before actually calling
   it. */

@protocol CanSetStyleMask
- (void)setStyleMask:(int)mask;
@end

void
gdk_window_set_decorations (GdkWindow       *window,
			    GdkWMDecoration  decorations)
{
  GdkWindowImplQuartz *impl;
  NSUInteger old_mask, new_mask;
  NSView *old_view;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (decorations == 0 || GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP ||
      impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN )
    {
      new_mask = NSBorderlessWindowMask;
    }
  else
    {
      /* FIXME: Honor other GDK_DECOR_* flags. */
      new_mask = (NSTitledWindowMask | NSClosableWindowMask |
                    NSMiniaturizableWindowMask | NSResizableWindowMask);
    }

  GDK_QUARTZ_ALLOC_POOL;

  old_mask = [impl->toplevel styleMask];

  if (old_mask != new_mask)
    {
      NSRect rect;

      old_view = [[impl->toplevel contentView] retain];

      rect = [impl->toplevel frame];

      /* Properly update the size of the window when the titlebar is
       * added or removed.
       */
      if (old_mask == NSBorderlessWindowMask &&
          new_mask != NSBorderlessWindowMask)
        {
          rect = [NSWindow frameRectForContentRect:rect styleMask:new_mask];

        }
      else if (old_mask != NSBorderlessWindowMask &&
               new_mask == NSBorderlessWindowMask)
        {
          rect = [NSWindow contentRectForFrameRect:rect styleMask:old_mask];
        }

      /* Note, before OS 10.6 there doesn't seem to be a way to change this
       * without recreating the toplevel. From 10.6 onward, a simple call to
       * setStyleMask takes care of most of this, except for ensuring that the
       * title is set.
       */
      if ([impl->toplevel respondsToSelector:@selector(setStyleMask:)])
        {
          NSString *title = [impl->toplevel title];

          [(id<CanSetStyleMask>)impl->toplevel setStyleMask:new_mask];

          /* It appears that unsetting and then resetting NSTitledWindowMask
           * does not reset the title in the title bar as might be expected.
           *
           * In theory we only need to set this if new_mask includes
           * NSTitledWindowMask. This behaved extremely oddly when
           * conditionalized upon that and since it has no side effects (i.e.
           * if NSTitledWindowMask is not requested, the title will not be
           * displayed) just do it unconditionally. We also must null check
           * 'title' before setting it to avoid crashing.
           */
          if (title)
            [impl->toplevel setTitle:title];
        }
      else
        {
          NSString *title = [impl->toplevel title];
          NSColor *bg = [impl->toplevel backgroundColor];
          NSScreen *screen = [impl->toplevel screen];

          /* Make sure the old window is closed, recall that releasedWhenClosed
           * is set on GdkQuartzWindows.
           */
          [impl->toplevel close];

          impl->toplevel = [[GdkQuartzWindow alloc] initWithContentRect:rect
                                                              styleMask:new_mask
                                                                backing:NSBackingStoreBuffered
                                                                  defer:NO
                                                                 screen:screen];
          [impl->toplevel setHasShadow: window_type_hint_to_shadow (impl->type_hint)];
          [impl->toplevel setLevel: window_type_hint_to_level (impl->type_hint)];
          if (title)
            [impl->toplevel setTitle:title];
          [impl->toplevel setBackgroundColor:bg];
          [impl->toplevel setHidesOnDeactivate: window_type_hint_to_hides_on_deactivate (impl->type_hint)];
          [impl->toplevel setContentView:old_view];
        }

      if (new_mask == NSBorderlessWindowMask)
        [impl->toplevel setContentSize:rect.size];
      else
        [impl->toplevel setFrame:rect display:YES];

      /* Invalidate the window shadow for non-opaque views that have shadow
       * enabled, to get the shadow shape updated.
       */
      if (![old_view isOpaque] && [impl->toplevel hasShadow])
        [(GdkQuartzView*)old_view setNeedsInvalidateShadow:YES];

      [old_view release];
    }

  GDK_QUARTZ_RELEASE_POOL;
}

gboolean
gdk_window_get_decorations (GdkWindow       *window,
			    GdkWMDecoration *decorations)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return FALSE;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (decorations)
    {
      /* Borderless is 0, so we can't check it as a bit being set. */
      if ([impl->toplevel styleMask] == NSBorderlessWindowMask)
        {
          *decorations = 0;
        }
      else
        {
          /* FIXME: Honor the other GDK_DECOR_* flags. */
          *decorations = GDK_DECOR_ALL;
        }
    }

  return TRUE;
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow  *window,
					GdkRegion  *area)
{
  return FALSE;
}

void
gdk_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
}

void
gdk_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
}

void
gdk_window_maximize (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && ![impl->toplevel isZoomed])
	[impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   0,
				   GDK_WINDOW_STATE_MAXIMIZED);
    }
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && [impl->toplevel isZoomed])
	[impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   GDK_WINDOW_STATE_MAXIMIZED,
				   0);
    }
}

void
gdk_window_iconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel miniaturize:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   0,
				   GDK_WINDOW_STATE_ICONIFIED);
    }
}

void
gdk_window_deiconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel deminiaturize:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   GDK_WINDOW_STATE_ICONIFIED,
				   0);
    }
}

static FullscreenSavedGeometry *
get_fullscreen_geometry (GdkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), FULLSCREEN_DATA);
}

void
gdk_window_fullscreen (GdkWindow *window)
{
  FullscreenSavedGeometry *geometry;
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  NSRect frame;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  geometry = get_fullscreen_geometry (window);
  if (!geometry)
    {
      geometry = g_new (FullscreenSavedGeometry, 1);

      geometry->x = private->x;
      geometry->y = private->y;
      geometry->width = private->width;
      geometry->height = private->height;

      if (!gdk_window_get_decorations (window, &geometry->decor))
        geometry->decor = GDK_DECOR_ALL;

      g_object_set_data_full (G_OBJECT (window),
                              FULLSCREEN_DATA, geometry, 
                              g_free);

      gdk_window_set_decorations (window, 0);

      frame = [[impl->toplevel screen] frame];
      move_resize_window_internal (window,
                                   0, 0, 
                                   frame.size.width, frame.size.height);
      [impl->toplevel setContentSize:frame.size];
      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];

      clear_toplevel_order ();
    }

  if ([NSWindow respondsToSelector:@selector(toggleFullScreen:)])
    {
       [impl->toplevel toggleFullScreen:nil];
    }
  else
    {
      SetSystemUIMode (kUIModeAllHidden, kUIOptionAutoShowMenuBar);
    }

  gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  FullscreenSavedGeometry *geometry;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  geometry = get_fullscreen_geometry (window);
  if (geometry)
    {
      if ([NSWindow respondsToSelector:@selector(toggleFullScreen:)])
        {
          [impl->toplevel toggleFullScreen:nil];
        }
      else
        {
	  SetSystemUIMode (kUIModeNormal, 0);
	}

      move_resize_window_internal (window,
                                   geometry->x,
                                   geometry->y,
                                   geometry->width,
                                   geometry->height);

      gdk_window_set_decorations (window, geometry->decor);

      g_object_set_data (G_OBJECT (window), FULLSCREEN_DATA, NULL);

      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
      clear_toplevel_order ();

      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
    }
}

void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  gint level;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  level = window_type_hint_to_level (gdk_window_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level + (setting ? 1 : 0)];
}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  gint level;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
  
  level = window_type_hint_to_level (gdk_window_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level - (setting ? 1 : 0)];
}

GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  g_return_val_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD, NULL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return NULL;

  /* FIXME: Implement */

  return NULL;
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  /* FIXME: Implement */	
}

GdkWindow*
gdk_window_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  /* Foreign windows aren't supported in Mac OS X */
  return NULL;
}

GdkWindow*
gdk_window_lookup (GdkNativeWindow anid)
{
  /* Foreign windows aren't supported in Mac OS X */
  return NULL;
}

GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  /* Foreign windows aren't supported in Mac OS X */
  return NULL;
}

void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
}

void
gdk_window_configure_finished (GdkWindow *window)
{
}

void
gdk_window_destroy_notify (GdkWindow *window)
{
  check_grab_destroy (window);
}

void 
_gdk_windowing_window_beep (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_display_beep (_gdk_display);
}

void
gdk_window_set_opacity (GdkWindow *window,
			gdouble    opacity)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  [impl->toplevel setAlphaValue: opacity];
}

void
_gdk_windowing_window_set_composited (GdkWindow *window, gboolean composited)
{
}

GdkRegion *
_gdk_windowing_get_shape_for_mask (GdkBitmap *mask)
{
  /* FIXME: implement */
  return NULL;
}

GdkRegion *
_gdk_windowing_window_get_shape (GdkWindow *window)
{
  /* FIXME: implement */
  return NULL;
}

GdkRegion *
_gdk_windowing_window_get_input_shape (GdkWindow *window)
{
  /* FIXME: implement */
  return NULL;
}

static void
gdk_window_impl_iface_init (GdkWindowImplIface *iface)
{
  iface->show = gdk_window_quartz_show;
  iface->hide = gdk_window_quartz_hide;
  iface->withdraw = gdk_window_quartz_withdraw;
  iface->set_events = gdk_window_quartz_set_events;
  iface->get_events = gdk_window_quartz_get_events;
  iface->raise = gdk_window_quartz_raise;
  iface->lower = gdk_window_quartz_lower;
  iface->restack_toplevel = gdk_window_quartz_restack_toplevel;
  iface->move_resize = gdk_window_quartz_move_resize;
  iface->set_background = gdk_window_quartz_set_background;
  iface->set_back_pixmap = gdk_window_quartz_set_back_pixmap;
  iface->reparent = gdk_window_quartz_reparent;
  iface->set_cursor = gdk_window_quartz_set_cursor;
  iface->get_geometry = gdk_window_quartz_get_geometry;
  iface->get_root_coords = gdk_window_quartz_get_root_coords;
  iface->get_pointer = gdk_window_quartz_get_pointer;
  iface->get_deskrelative_origin = gdk_window_quartz_get_deskrelative_origin;
  iface->shape_combine_region = gdk_window_quartz_shape_combine_region;
  iface->input_shape_combine_region = gdk_window_quartz_input_shape_combine_region;
  iface->set_static_gravities = gdk_window_quartz_set_static_gravities;
  iface->queue_antiexpose = _gdk_quartz_window_queue_antiexpose;
  iface->queue_translation = _gdk_quartz_window_queue_translation;
  iface->destroy = _gdk_quartz_window_destroy;
  iface->input_window_destroy = _gdk_input_window_destroy;
  iface->input_window_crossing = _gdk_input_window_crossing;
}
