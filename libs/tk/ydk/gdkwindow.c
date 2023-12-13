/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
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
#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdk.h"		/* For gdk_rectangle_union() */
#include "gdkpixmap.h"
#include "gdkdrawable.h"
#include "gdkintl.h"
#include "gdkscreen.h"
#include "gdkmarshalers.h"
#include "gdkalias.h"

#undef DEBUG_WINDOW_PRINTING

#ifdef GDK_WINDOWING_X11
#include "gdkx.h"           /* For workaround */
#endif

#include "math.h"

/* Historically a GdkWindow always matches a platform native window,
 * be it a toplevel window or a child window. In this setup the
 * GdkWindow (and other GdkDrawables) were platform independent classes,
 * and the actual platform specific implementation was in a delegate
 * object availible as "impl" in the window object.
 *
 * With the addition of client side windows and offscreen windows this
 * changes a bit. The application-visible GdkWindow object behaves as
 * it did before, but not all such windows now have a corresponding native
 * window. Instead windows that are "client side" are emulated by the gdk
 * code such that clipping, drawing, moving, events etc work as expected.
 *
 * For GdkWindows that have a native window the "impl" object is the
 * same as before. However, for all client side windows the impl object
 * is shared with its parent (i.e. all client windows descendants of one
 * native window has the same impl.
 *
 * Additionally there is a new type of platform independent impl object,
 * GdkOffscreenWindow. All windows of type GDK_WINDOW_OFFSCREEN get an impl
 * of this type (while their children are generally GDK_WINDOW_CHILD virtual
 * windows). Such windows work by allocating a GdkPixmap as the backing store
 * for drawing operations, which is resized with the window.
 *
 * GdkWindows have a pointer to the "impl window" they are in, i.e.
 * the topmost GdkWindow which have the same "impl" value. This is stored
 * in impl_window, which is different from the window itself only for client
 * side windows.
 * All GdkWindows (native or not) track the position of the window in the parent
 * (x, y), the size of the window (width, height), the position of the window
 * with respect to the impl window (abs_x, abs_y). We also track the clip
 * region of the window wrt parent windows and siblings, in window-relative
 * coordinates with and without child windows included (clip_region,
 * clip_region_with_children).
 *
 * All toplevel windows are native windows, but also child windows can be
 * native (although not children of offscreens). We always listen to
 * a basic set of events (see get_native_event_mask) for these windows
 * so that we can emulate events for any client side children.
 *
 * For native windows we apply the calculated clip region as a window shape
 * so that eg. client side siblings that overlap the native child properly
 * draws over the native child window.
 *
 * In order to minimize flicker and for performance we use a couple of cacheing
 * tricks. First of all, every time we do a window to window copy area, for instance
 * when moving a client side window or when scrolling/moving a region in a window
 * we store this in outstanding_moves instead of applying immediately. We then
 * delay this move until we really need it (because something depends on being
 * able to read it), or until we're handing a redraw from an expose/invalidation
 * (actually we delay it past redraw, but before blitting the double buffer pixmap
 * to the window). This gives us two advantages. First of all it minimizes the time
 * from the window is moved to the exposes related to that move, secondly it allows
 * us to be smart about how to do the copy. We combine multiple moves into one (when
 * possible) and we don't actually do copies to anything that is or will be
 * invalidated and exposed anyway.
 *
 * Secondly, we use something called a "implicit paint" during repaint handling.
 * An implicit paint is similar to a regular paint for the paint stack, but it is
 * not put on the stack. Instead, it is set on the impl window, and later when
 * regular gdk_window_begin_paint_region()  happen on a window of this impl window
 * we reuse the pixmap from the implicit paint. During repaint we create and at the
 * end flush an implicit paint, which means we can collect all the paints on
 * multiple client side windows in the same backing store pixmap.
 *
 * All drawing to windows are wrapped with macros that set up the GC such that
 * the offsets and clip region is right for drawing to the paint object or
 * directly to the emulated window. It also automatically handles any flushing
 * needed when drawing directly to a window. Adding window/paint clipping is
 * done using _gdk_gc_add_drawable_clip which lets us efficiently add and then
 * remove a custom clip region.
 */

#ifndef __APPLE__
#define USE_BACKING_STORE	/* Appears to work on Win32, too, now. */
#endif

/* This adds a local value to the GdkVisibilityState enum */
#define GDK_VISIBILITY_NOT_VIEWABLE 3

enum {
  PICK_EMBEDDED_CHILD, /* only called if children are embedded */
  TO_EMBEDDER,
  FROM_EMBEDDER,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CURSOR
};

typedef enum {
  CLEAR_BG_NONE,
  CLEAR_BG_WINCLEARED, /* Clear backgrounds except those that the window system clears */
  CLEAR_BG_ALL
} ClearBg;

struct _GdkWindowPaint
{
  GdkRegion *region;
  GdkPixmap *pixmap;
  gint x_offset;
  gint y_offset;
  cairo_surface_t *surface;
  guint uses_implicit : 1;
  guint flushed : 1;
  guint32 region_tag;
};

typedef struct {
  GdkRegion *dest_region; /* The destination region */
  int dx, dy; /* The amount that the source was moved to reach dest_region */
} GdkWindowRegionMove;


/* Global info */

static GdkGC *gdk_window_create_gc      (GdkDrawable     *drawable,
					 GdkGCValues     *values,
					 GdkGCValuesMask  mask);
static void   gdk_window_draw_rectangle (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gboolean         filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void   gdk_window_draw_arc       (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gboolean         filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 gint             angle1,
					 gint             angle2);
static void   gdk_window_draw_polygon   (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gboolean         filled,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_window_draw_text      (GdkDrawable     *drawable,
					 GdkFont         *font,
					 GdkGC           *gc,
					 gint             x,
					 gint             y,
					 const gchar     *text,
					 gint             text_length);
static void   gdk_window_draw_text_wc   (GdkDrawable     *drawable,
					 GdkFont         *font,
					 GdkGC           *gc,
					 gint             x,
					 gint             y,
					 const GdkWChar  *text,
					 gint             text_length);
static void   gdk_window_draw_drawable  (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPixmap       *src,
					 gint             xsrc,
					 gint             ysrc,
					 gint             xdest,
					 gint             ydest,
					 gint             width,
					 gint             height,
					 GdkDrawable     *original_src);
static void   gdk_window_draw_points    (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_window_draw_segments  (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkSegment      *segs,
					 gint             nsegs);
static void   gdk_window_draw_lines     (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);

static void gdk_window_draw_glyphs             (GdkDrawable      *drawable,
						GdkGC            *gc,
						PangoFont        *font,
						gint              x,
						gint              y,
						PangoGlyphString *glyphs);
static void gdk_window_draw_glyphs_transformed (GdkDrawable      *drawable,
						GdkGC            *gc,
						PangoMatrix      *matrix,
						PangoFont        *font,
						gint              x,
						gint              y,
						PangoGlyphString *glyphs);

static void   gdk_window_draw_image     (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkImage        *image,
					 gint             xsrc,
					 gint             ysrc,
					 gint             xdest,
					 gint             ydest,
					 gint             width,
					 gint             height);

static void gdk_window_draw_pixbuf (GdkDrawable     *drawable,
				    GdkGC           *gc,
				    GdkPixbuf       *pixbuf,
				    gint             src_x,
				    gint             src_y,
				    gint             dest_x,
				    gint             dest_y,
				    gint             width,
				    gint             height,
				    GdkRgbDither     dither,
				    gint             x_dither,
				    gint             y_dither);

static void gdk_window_draw_trapezoids (GdkDrawable   *drawable,
					GdkGC	      *gc,
					GdkTrapezoid  *trapezoids,
					gint           n_trapezoids);

static GdkImage* gdk_window_copy_to_image (GdkDrawable *drawable,
					   GdkImage    *image,
					   gint         src_x,
					   gint         src_y,
					   gint         dest_x,
					   gint         dest_y,
					   gint         width,
					   gint         height);

static cairo_surface_t *gdk_window_ref_cairo_surface (GdkDrawable *drawable);
static cairo_surface_t *gdk_window_create_cairo_surface (GdkDrawable *drawable,
							 int width,
							 int height);
static void             gdk_window_drop_cairo_surface (GdkWindowObject *private);
static void             gdk_window_set_cairo_clip    (GdkDrawable *drawable,
						      cairo_t *cr);

static void   gdk_window_real_get_size  (GdkDrawable     *drawable,
					 gint            *width,
					 gint            *height);

static GdkVisual*   gdk_window_real_get_visual   (GdkDrawable *drawable);
static gint         gdk_window_real_get_depth    (GdkDrawable *drawable);
static GdkScreen*   gdk_window_real_get_screen   (GdkDrawable *drawable);
static void         gdk_window_real_set_colormap (GdkDrawable *drawable,
						  GdkColormap *cmap);
static GdkColormap* gdk_window_real_get_colormap (GdkDrawable *drawable);

static GdkDrawable* gdk_window_get_source_drawable    (GdkDrawable *drawable);
static GdkDrawable* gdk_window_get_composite_drawable (GdkDrawable *drawable,
						       gint         x,
						       gint         y,
						       gint         width,
						       gint         height,
						       gint        *composite_x_offset,
						       gint        *composite_y_offset);
static GdkRegion*   gdk_window_get_clip_region        (GdkDrawable *drawable);
static GdkRegion*   gdk_window_get_visible_region     (GdkDrawable *drawable);

static void gdk_window_free_paint_stack (GdkWindow *window);

static void gdk_window_init       (GdkWindowObject      *window);
static void gdk_window_class_init (GdkWindowObjectClass *klass);
static void gdk_window_finalize   (GObject              *object);

static void gdk_window_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void gdk_window_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);

static void gdk_window_clear_backing_region (GdkWindow *window,
					     GdkRegion *region);
static void gdk_window_redirect_free      (GdkWindowRedirect *redirect);
static void apply_redirect_to_children    (GdkWindowObject   *private,
					   GdkWindowRedirect *redirect);
static void remove_redirect_from_children (GdkWindowObject   *private,
					   GdkWindowRedirect *redirect);

static void recompute_visible_regions   (GdkWindowObject *private,
					 gboolean recalculate_siblings,
					 gboolean recalculate_children);
static void gdk_window_flush_outstanding_moves (GdkWindow *window);
static void gdk_window_flush_recursive  (GdkWindowObject *window);
static void do_move_region_bits_on_impl (GdkWindowObject *private,
					 GdkRegion *region, /* In impl window coords */
					 int dx, int dy);
static void gdk_window_invalidate_in_parent (GdkWindowObject *private);
static void move_native_children        (GdkWindowObject *private);
static void update_cursor               (GdkDisplay *display);
static void impl_window_add_update_area (GdkWindowObject *impl_window,
					 GdkRegion *region);
static void gdk_window_region_move_free (GdkWindowRegionMove *move);
static void gdk_window_invalidate_region_full (GdkWindow       *window,
					       const GdkRegion *region,
					       gboolean         invalidate_children,
					       ClearBg          clear_bg);
static void gdk_window_invalidate_rect_full (GdkWindow          *window,
					     const GdkRectangle *rect,
					     gboolean            invalidate_children,
					     ClearBg             clear_bg);

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer parent_class = NULL;

static const cairo_user_data_key_t gdk_window_cairo_key;

static guint32
new_region_tag (void)
{
  static guint32 tag = 0;

  return ++tag;
}

GType
gdk_window_object_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    object_type = g_type_register_static_simple (GDK_TYPE_DRAWABLE,
						 "GdkWindow",
						 sizeof (GdkWindowObjectClass),
						 (GClassInitFunc) gdk_window_class_init,
						 sizeof (GdkWindowObject),
						 (GInstanceInitFunc) gdk_window_init,
						 0);

  return object_type;
}

GType
_gdk_paintable_get_type (void)
{
  static GType paintable_type = 0;

  if (!paintable_type)
    {
      const GTypeInfo paintable_info =
      {
	sizeof (GdkPaintableIface),  /* class_size */
	NULL,                        /* base_init */
	NULL,                        /* base_finalize */
      };

      paintable_type = g_type_register_static (G_TYPE_INTERFACE,
					       g_intern_static_string ("GdkPaintable"),
					       &paintable_info, 0);

      g_type_interface_add_prerequisite (paintable_type, G_TYPE_OBJECT);
    }

  return paintable_type;
}

static void
gdk_window_init (GdkWindowObject *window)
{
  /* 0-initialization is good for all other fields. */

  window->window_type = GDK_WINDOW_CHILD;

  window->state = GDK_WINDOW_STATE_WITHDRAWN;
  window->width = 1;
  window->height = 1;
  window->toplevel_window_type = -1;
  /* starts hidden */
  window->effective_visibility = GDK_VISIBILITY_NOT_VIEWABLE;
  window->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
  /* Default to unobscured since some backends don't send visibility events */
  window->native_visibility = GDK_VISIBILITY_UNOBSCURED;
}

/* Stop and return on the first non-NULL parent */
static gboolean
accumulate_get_window (GSignalInvocationHint *ihint,
		       GValue		       *return_accu,
		       const GValue	       *handler_return,
		       gpointer               data)
{
  g_value_copy (handler_return, return_accu);
  /* Continue while returning NULL */
  return g_value_get_object (handler_return) == NULL;
}

static GQuark quark_pointer_window = 0;

static void
gdk_window_class_init (GdkWindowObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_finalize;
  object_class->set_property = gdk_window_set_property;
  object_class->get_property = gdk_window_get_property;

  drawable_class->create_gc = gdk_window_create_gc;
  drawable_class->draw_rectangle = gdk_window_draw_rectangle;
  drawable_class->draw_arc = gdk_window_draw_arc;
  drawable_class->draw_polygon = gdk_window_draw_polygon;
  drawable_class->draw_text = gdk_window_draw_text;
  drawable_class->draw_text_wc = gdk_window_draw_text_wc;
  drawable_class->draw_drawable_with_src = gdk_window_draw_drawable;
  drawable_class->draw_points = gdk_window_draw_points;
  drawable_class->draw_segments = gdk_window_draw_segments;
  drawable_class->draw_lines = gdk_window_draw_lines;
  drawable_class->draw_glyphs = gdk_window_draw_glyphs;
  drawable_class->draw_glyphs_transformed = gdk_window_draw_glyphs_transformed;
  drawable_class->draw_image = gdk_window_draw_image;
  drawable_class->draw_pixbuf = gdk_window_draw_pixbuf;
  drawable_class->draw_trapezoids = gdk_window_draw_trapezoids;
  drawable_class->get_depth = gdk_window_real_get_depth;
  drawable_class->get_screen = gdk_window_real_get_screen;
  drawable_class->get_size = gdk_window_real_get_size;
  drawable_class->set_colormap = gdk_window_real_set_colormap;
  drawable_class->get_colormap = gdk_window_real_get_colormap;
  drawable_class->get_visual = gdk_window_real_get_visual;
  drawable_class->_copy_to_image = gdk_window_copy_to_image;
  drawable_class->ref_cairo_surface = gdk_window_ref_cairo_surface;
  drawable_class->create_cairo_surface = gdk_window_create_cairo_surface;
  drawable_class->set_cairo_clip = gdk_window_set_cairo_clip;
  drawable_class->get_clip_region = gdk_window_get_clip_region;
  drawable_class->get_visible_region = gdk_window_get_visible_region;
  drawable_class->get_composite_drawable = gdk_window_get_composite_drawable;
  drawable_class->get_source_drawable = gdk_window_get_source_drawable;

  quark_pointer_window = g_quark_from_static_string ("gtk-pointer-window");


  /* Properties */

  /**
   * GdkWindow:cursor:
   *
   * The mouse pointer for a #GdkWindow. See gdk_window_set_cursor() and
   * gdk_window_get_cursor() for details.
   *
   * Since: 2.18
   */
  g_object_class_install_property (object_class,
                                   PROP_CURSOR,
                                   g_param_spec_boxed ("cursor",
                                                       P_("Cursor"),
                                                       P_("Cursor"),
                                                       GDK_TYPE_CURSOR,
                                                       G_PARAM_READWRITE));

  /**
   * GdkWindow::pick-embedded-child:
   * @window: the window on which the signal is emitted
   * @x: x coordinate in the window
   * @y: y coordinate in the window
   *
   * The ::pick-embedded-child signal is emitted to find an embedded
   * child at the given position.
   *
   * Returns: (transfer none): the #GdkWindow of the embedded child at
   *     @x, @y, or %NULL
   *
   * Since: 2.18
   */
  signals[PICK_EMBEDDED_CHILD] =
    g_signal_new (g_intern_static_string ("pick-embedded-child"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  accumulate_get_window, NULL,
		  _gdk_marshal_OBJECT__DOUBLE_DOUBLE,
		  GDK_TYPE_WINDOW,
		  2,
		  G_TYPE_DOUBLE,
		  G_TYPE_DOUBLE);

  /**
   * GdkWindow::to-embedder:
   * @window: the offscreen window on which the signal is emitted
   * @offscreen-x: x coordinate in the offscreen window
   * @offscreen-y: y coordinate in the offscreen window
   * @embedder-x: (out) (type double): return location for the x
   *     coordinate in the embedder window
   * @embedder-y: (out) (type double): return location for the y
   *     coordinate in the embedder window
   *
   * The ::to-embedder signal is emitted to translate coordinates
   * in an offscreen window to its embedder.
   *
   * See also #GtkWindow::from-embedder.
   *
   * Since: 2.18
   */
  signals[TO_EMBEDDER] =
    g_signal_new (g_intern_static_string ("to-embedder"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  _gdk_marshal_VOID__DOUBLE_DOUBLE_POINTER_POINTER,
		  G_TYPE_NONE,
		  4,
		  G_TYPE_DOUBLE,
		  G_TYPE_DOUBLE,
		  G_TYPE_POINTER,
		  G_TYPE_POINTER);

  /**
   * GdkWindow::from-embedder:
   * @window: the offscreen window on which the signal is emitted
   * @embedder-x: x coordinate in the embedder window
   * @embedder-y: y coordinate in the embedder window
   * @offscreen-x: (out) (type double): return location for the x
   *     coordinate in the offscreen window
   * @offscreen-y: (out) (type double): return location for the y
   *     coordinate in the offscreen window
   *
   * The ::from-embedder signal is emitted to translate coordinates
   * in the embedder of an offscreen window to the offscreen window.
   *
   * See also #GtkWindow::to-embedder.
   *
   * Since: 2.18
   */
  signals[FROM_EMBEDDER] =
    g_signal_new (g_intern_static_string ("from-embedder"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  _gdk_marshal_VOID__DOUBLE_DOUBLE_POINTER_POINTER,
		  G_TYPE_NONE,
		  4,
		  G_TYPE_DOUBLE,
		  G_TYPE_DOUBLE,
		  G_TYPE_POINTER,
		  G_TYPE_POINTER);
}

static void
gdk_window_finalize (GObject *object)
{
  GdkWindow *window = GDK_WINDOW (object);
  GdkWindowObject *obj = (GdkWindowObject *) object;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  g_warning ("losing last reference to undestroyed window\n");
	  _gdk_window_destroy (window, FALSE);
	}
      else
	/* We use TRUE here, to keep us from actually calling
	 * XDestroyWindow() on the window
	 */
	_gdk_window_destroy (window, TRUE);
    }

  if (obj->impl)
    {
      g_object_unref (obj->impl);
      obj->impl = NULL;
    }

  if (obj->impl_window != obj)
    {
      g_object_unref (obj->impl_window);
      obj->impl_window = NULL;
    }

  if (obj->shape)
    gdk_region_destroy (obj->shape);

  if (obj->input_shape)
    gdk_region_destroy (obj->input_shape);

  if (obj->cursor)
    gdk_cursor_unref (obj->cursor);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_window_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkWindow *window = (GdkWindow *)object;

  switch (prop_id)
    {
    case PROP_CURSOR:
      gdk_window_set_cursor (window, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_window_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkWindow *window = (GdkWindow *) object;

  switch (prop_id)
    {
    case PROP_CURSOR:
      g_value_set_boxed (value, gdk_window_get_cursor (window));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gdk_window_is_offscreen (GdkWindowObject *window)
{
  return window->window_type == GDK_WINDOW_OFFSCREEN;
}

static GdkWindowObject *
gdk_window_get_impl_window (GdkWindowObject *window)
{
  return window->impl_window;
}

GdkWindow *
_gdk_window_get_impl_window (GdkWindow *window)
{
  return (GdkWindow *)gdk_window_get_impl_window ((GdkWindowObject *)window);
}

static gboolean
gdk_window_has_impl (GdkWindowObject *window)
{
  return window->impl_window == window;
}

static gboolean
gdk_window_is_toplevel (GdkWindowObject *window)
{
  return
    window->parent == NULL ||
    window->parent->window_type == GDK_WINDOW_ROOT;
}

gboolean
_gdk_window_has_impl (GdkWindow *window)
{
  return gdk_window_has_impl ((GdkWindowObject *)window);
}

static gboolean
gdk_window_has_no_impl (GdkWindowObject *window)
{
  return window->impl_window != window;
}

static void
remove_child_area (GdkWindowObject *private,
		   GdkWindowObject *until,
		   gboolean for_input,
		   GdkRegion *region)
{
  GdkWindowObject *child;
  GdkRegion *child_region;
  GdkRectangle r;
  GList *l;
  GdkRegion *shape;

  for (l = private->children; l; l = l->next)
    {
      child = l->data;

      if (child == until)
	break;

      /* If region is empty already, no need to do
	 anything potentially costly */
      if (gdk_region_empty (region))
	break;

      if (!GDK_WINDOW_IS_MAPPED (child) || child->input_only || child->composited)
	continue;

      /* Ignore offscreen children, as they don't draw in their parent and
       * don't take part in the clipping */
      if (gdk_window_is_offscreen (child))
	continue;

      r.x = child->x;
      r.y = child->y;
      r.width = child->width;
      r.height = child->height;

      /* Bail early if child totally outside region */
      if (gdk_region_rect_in (region, &r) == GDK_OVERLAP_RECTANGLE_OUT)
	continue;

      child_region = gdk_region_rectangle (&r);

      if (child->shape)
	{
	  /* Adjust shape region to parent window coords */
	  gdk_region_offset (child->shape, child->x, child->y);
	  gdk_region_intersect (child_region, child->shape);
	  gdk_region_offset (child->shape, -child->x, -child->y);
	}
      else if (private->window_type == GDK_WINDOW_FOREIGN)
	{
	  shape = _gdk_windowing_window_get_shape ((GdkWindow *)child);
	  if (shape)
	    {
	      gdk_region_intersect (child_region, shape);
	      gdk_region_destroy (shape);
	    }
	}

      if (for_input)
	{
	  if (child->input_shape)
	    gdk_region_intersect (child_region, child->input_shape);
	  else if (private->window_type == GDK_WINDOW_FOREIGN)
	    {
	      shape = _gdk_windowing_window_get_input_shape ((GdkWindow *)child);
	      if (shape)
		{
		  gdk_region_intersect (child_region, shape);
		  gdk_region_destroy (shape);
		}
	    }
	}

      gdk_region_subtract (region, child_region);
      gdk_region_destroy (child_region);

    }
}

static GdkVisibilityState
effective_visibility (GdkWindowObject *private)
{
  GdkVisibilityState native;

  if (!gdk_window_is_viewable ((GdkWindow *)private))
    return GDK_VISIBILITY_NOT_VIEWABLE;

  native = private->impl_window->native_visibility;

  if (native == GDK_VISIBILITY_FULLY_OBSCURED ||
      private->visibility == GDK_VISIBILITY_FULLY_OBSCURED)
    return GDK_VISIBILITY_FULLY_OBSCURED;
  else if (native == GDK_VISIBILITY_UNOBSCURED)
    return private->visibility;
  else /* native PARTIAL, private partial or unobscured  */
    return GDK_VISIBILITY_PARTIAL;
}

static void
gdk_window_update_visibility (GdkWindowObject *private)
{
  GdkVisibilityState new_visibility;
  GdkEvent *event;

  new_visibility = effective_visibility (private);

  if (new_visibility != private->effective_visibility)
    {
      private->effective_visibility = new_visibility;

      if (new_visibility != GDK_VISIBILITY_NOT_VIEWABLE &&
	  private->event_mask & GDK_VISIBILITY_NOTIFY)
	{
	  event = _gdk_make_event ((GdkWindow *)private, GDK_VISIBILITY_NOTIFY,
				   NULL, FALSE);
	  event->visibility.state = new_visibility;
	}
    }
}

static void
gdk_window_update_visibility_recursively (GdkWindowObject *private,
					  GdkWindowObject *only_for_impl)
{
  GdkWindowObject *child;
  GList *l;

  gdk_window_update_visibility (private);
  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;
      if ((only_for_impl == NULL) ||
	  (only_for_impl == child->impl_window))
	gdk_window_update_visibility_recursively (child, only_for_impl);
    }
}

static gboolean
should_apply_clip_as_shape (GdkWindowObject *private)
{
  return
    gdk_window_has_impl (private) &&
    /* Not for offscreens */
    !gdk_window_is_offscreen (private) &&
    /* or for toplevels */
    !gdk_window_is_toplevel (private) &&
    /* or for foreign windows */
    private->window_type != GDK_WINDOW_FOREIGN &&
    /* or for the root window */
    private->window_type != GDK_WINDOW_ROOT;
}

static void
apply_shape (GdkWindowObject *private,
	     GdkRegion *region)
{
  GdkWindowImplIface *impl_iface;

  /* We trash whether we applied a shape so that
     we can avoid unsetting it many times, which
     could happen in e.g. apply_clip_as_shape as
     windows get resized */
  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  if (region)
    impl_iface->shape_combine_region ((GdkWindow *)private,
				      region, 0, 0);
  else if (private->applied_shape)
    impl_iface->shape_combine_region ((GdkWindow *)private,
				      NULL, 0, 0);

  private->applied_shape = region != NULL;
}

static void
apply_clip_as_shape (GdkWindowObject *private)
{
  GdkRectangle r;

  r.x = r.y = 0;
  r.width = private->width;
  r.height = private->height;

  /* We only apply the clip region if would differ
     from the actual clip region implied by the size
     of the window. This is to avoid unneccessarily
     adding meaningless shapes to all native subwindows */
  if (!gdk_region_rect_equal (private->clip_region, &r))
    apply_shape (private, private->clip_region);
  else
    apply_shape (private, NULL);
}

static void
recompute_visible_regions_internal (GdkWindowObject *private,
				    gboolean recalculate_clip,
				    gboolean recalculate_siblings,
				    gboolean recalculate_children)
{
  GdkRectangle r;
  GList *l;
  GdkWindowObject *child;
  GdkRegion *new_clip, *old_clip_region_with_children;
  gboolean clip_region_changed;
  gboolean abs_pos_changed;
  int old_abs_x, old_abs_y;

  old_abs_x = private->abs_x;
  old_abs_y = private->abs_y;

  /* Update absolute position */
  if (gdk_window_has_impl (private))
    {
      /* Native window starts here */
      private->abs_x = 0;
      private->abs_y = 0;
    }
  else
    {
      private->abs_x = private->parent->abs_x + private->x;
      private->abs_y = private->parent->abs_y + private->y;
    }

  abs_pos_changed =
    private->abs_x != old_abs_x ||
    private->abs_y != old_abs_y;

  /* Update clip region based on:
   * parent clip
   * window size
   * siblings in parents above window
   */
  clip_region_changed = FALSE;
  if (recalculate_clip)
    {
      if (private->viewable)
	{
	  /* Calculate visible region (sans children) in parent window coords */
	  r.x = private->x;
	  r.y = private->y;
	  r.width = private->width;
	  r.height = private->height;
	  new_clip = gdk_region_rectangle (&r);

	  if (!gdk_window_is_toplevel (private))
	    {
	      gdk_region_intersect (new_clip, private->parent->clip_region);

	      /* Remove all overlapping children from parent.
	       * Unless we're all native, because then we don't need to take
	       * siblings into account since X does that clipping for us.
	       * This makes things like SWT that modify the raw X stacking
	       * order without GDKs knowledge work.
	       */
	      if (!_gdk_native_windows)
		remove_child_area (private->parent, private, FALSE, new_clip);
	    }

	  /* Convert from parent coords to window coords */
	  gdk_region_offset (new_clip, -private->x, -private->y);

	  if (private->shape)
	    gdk_region_intersect (new_clip, private->shape);
	}
      else
	new_clip = gdk_region_new ();

      if (private->clip_region == NULL ||
	  !gdk_region_equal (private->clip_region, new_clip))
	clip_region_changed = TRUE;

      if (private->clip_region)
	gdk_region_destroy (private->clip_region);
      private->clip_region = new_clip;

      old_clip_region_with_children = private->clip_region_with_children;
      private->clip_region_with_children = gdk_region_copy (private->clip_region);
      if (private->window_type != GDK_WINDOW_ROOT)
	remove_child_area (private, NULL, FALSE, private->clip_region_with_children);

      if (clip_region_changed ||
	  !gdk_region_equal (private->clip_region_with_children, old_clip_region_with_children))
	  private->clip_tag = new_region_tag ();

      if (old_clip_region_with_children)
	gdk_region_destroy (old_clip_region_with_children);
    }

  if (clip_region_changed)
    {
      GdkVisibilityState visibility;
      gboolean fully_visible;

      if (gdk_region_empty (private->clip_region))
	visibility = GDK_VISIBILITY_FULLY_OBSCURED;
      else
        {
          if (private->shape)
            {
	      fully_visible = gdk_region_equal (private->clip_region,
	                                        private->shape);
            }
          else
            {
	      r.x = 0;
	      r.y = 0;
	      r.width = private->width;
	      r.height = private->height;
	      fully_visible = gdk_region_rect_equal (private->clip_region, &r);
	    }

	  if (fully_visible)
	    visibility = GDK_VISIBILITY_UNOBSCURED;
	  else
	    visibility = GDK_VISIBILITY_PARTIAL;
	}

      if (private->visibility != visibility)
	{
	  private->visibility = visibility;
	  gdk_window_update_visibility (private);
	}
    }

  /* Update all children, recursively (except for root, where children are not exact). */
  if ((abs_pos_changed || clip_region_changed || recalculate_children) &&
      private->window_type != GDK_WINDOW_ROOT)
    {
      for (l = private->children; l; l = l->next)
	{
	  child = l->data;
	  /* Only recalculate clip if the the clip region changed, otherwise
	   * there is no way the child clip region could change (its has not e.g. moved)
	   * Except if recalculate_children is set to force child updates
	   */
	  recompute_visible_regions_internal (child,
					      recalculate_clip && (clip_region_changed || recalculate_children),
					      FALSE, FALSE);
	}
    }

  if (clip_region_changed &&
      should_apply_clip_as_shape (private))
    apply_clip_as_shape (private);

  if (recalculate_siblings &&
      !gdk_window_is_toplevel (private))
    {
      /* If we moved a child window in parent or changed the stacking order, then we
       * need to recompute the visible area of all the other children in the parent
       */
      for (l = private->parent->children; l; l = l->next)
	{
	  child = l->data;

	  if (child != private)
	    recompute_visible_regions_internal (child, TRUE, FALSE, FALSE);
	}

      /* We also need to recompute the _with_children clip for the parent */
      recompute_visible_regions_internal (private->parent, TRUE, FALSE, FALSE);
    }

  if (private->cairo_surface)
    {
      int width, height;

      /* It would be nice if we had some cairo support here so we
	 could set the clip rect on the cairo surface */
      width = private->abs_x + private->width;
      height = private->abs_y + private->height;

      _gdk_windowing_set_cairo_surface_size (private->cairo_surface,
					     width, height);
      cairo_surface_set_device_offset (private->cairo_surface,
				       private->abs_x,
				       private->abs_y);
    }
}

/* Call this when private has changed in one or more of these ways:
 *  size changed
 *  window moved
 *  new window added
 *  stacking order of window changed
 *  child deleted
 *
 * It will recalculate abs_x/y and the clip regions
 *
 * Unless the window didn't change stacking order or size/pos, pass in TRUE
 * for recalculate_siblings. (Mostly used internally for the recursion)
 *
 * If a child window was removed (and you can't use that child for
 * recompute_visible_regions), pass in TRUE for recalculate_children on the parent
 */
static void
recompute_visible_regions (GdkWindowObject *private,
			   gboolean recalculate_siblings,
			   gboolean recalculate_children)
{
  recompute_visible_regions_internal (private,
				      TRUE,
				      recalculate_siblings,
				      recalculate_children);
}

void
_gdk_window_update_size (GdkWindow *window)
{
  recompute_visible_regions ((GdkWindowObject *)window, TRUE, FALSE);
}

/* Find the native window that would be just above "child"
 * in the native stacking order if "child" was a native window
 * (it doesn't have to be native). If there is no such native
 * window inside this native parent then NULL is returned.
 * If child is NULL, find lowest native window in parent.
 */
static GdkWindowObject *
find_native_sibling_above_helper (GdkWindowObject *parent,
				  GdkWindowObject *child)
{
  GdkWindowObject *w;
  GList *l;

  if (child)
    {
      l = g_list_find (parent->children, child);
      g_assert (l != NULL); /* Better be a child of its parent... */
      l = l->prev; /* Start looking at the one above the child */
    }
  else
    l = g_list_last (parent->children);

  for (; l != NULL; l = l->prev)
    {
      w = l->data;

      if (gdk_window_has_impl (w))
	return w;

      g_assert (parent != w);
      w = find_native_sibling_above_helper (w, NULL);
      if (w)
	return w;
    }

  return NULL;
}


static GdkWindowObject *
find_native_sibling_above (GdkWindowObject *parent,
			   GdkWindowObject *child)
{
  GdkWindowObject *w;

  w = find_native_sibling_above_helper (parent, child);
  if (w)
    return w;

  if (gdk_window_has_impl (parent))
    return NULL;
  else
    return find_native_sibling_above (parent->parent, parent);
}

static GdkEventMask
get_native_event_mask (GdkWindowObject *private)
{
  if (_gdk_native_windows ||
      private->window_type == GDK_WINDOW_ROOT ||
      private->window_type == GDK_WINDOW_FOREIGN)
    return private->event_mask;
  else
    {
      GdkEventMask mask;

      /* Do whatever the app asks to, since the app
       * may be asking for weird things for native windows,
       * but don't use motion hints as that may affect non-native
       * child windows that don't want it. Also, we need to
       * set all the app-specified masks since they will be picked
       * up by any implicit grabs (i.e. if they were not set as
       * native we would not get the events we need). */
      mask = private->event_mask & ~GDK_POINTER_MOTION_HINT_MASK;

      /* We need thse for all native windows so we can
	 emulate events on children: */
      mask |=
	GDK_EXPOSURE_MASK |
	GDK_VISIBILITY_NOTIFY_MASK |
	GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK;

      /* Additionally we select for pointer and button events
       * for toplevels as we need to get these to emulate
       * them for non-native subwindows. Even though we don't
       * select on them for all native windows we will get them
       * as the events are propagated out to the first window
       * that select for them.
       * Not selecting for button press on all windows is an
       * important thing, because in X only one client can do
       * so, and we don't want to unexpectedly prevent another
       * client from doing it.
       *
       * We also need to do the same if the app selects for button presses
       * because then we will get implicit grabs for this window, and the
       * event mask used for that grab is based on the rest of the mask
       * for the window, but we might need more events than this window
       * lists due to some non-native child window.
       */
      if (gdk_window_is_toplevel (private) ||
	  mask & GDK_BUTTON_PRESS_MASK)
	mask |=
	  GDK_POINTER_MOTION_MASK |
	  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	  GDK_SCROLL_MASK;

      return mask;
    }
}

static GdkEventMask
get_native_grab_event_mask (GdkEventMask grab_mask)
{
  /* Similar to the above but for pointer events only */
  return
    GDK_POINTER_MOTION_MASK |
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
    GDK_SCROLL_MASK |
    (grab_mask &
     ~GDK_POINTER_MOTION_HINT_MASK);
}

/* Puts the native window in the right order wrt the other native windows
 * in the hierarchy, given the position it has in the client side data.
 * This is useful if some operation changed the stacking order.
 * This calls assumes the native window is now topmost in its native parent.
 */
static void
sync_native_window_stack_position (GdkWindow *window)
{
  GdkWindowObject *above;
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  GList listhead = {0};

  private = (GdkWindowObject *) window;
  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

  above = find_native_sibling_above (private->parent, private);
  if (above)
    {
      listhead.data = window;
      impl_iface->restack_under ((GdkWindow *)above,
				 &listhead);
    }
}

/**
 * gdk_window_new:
 * @parent: (allow-none): a #GdkWindow, or %NULL to create the window as a child of
 *   the default root window for the default display.
 * @attributes: attributes of the new window
 * @attributes_mask: mask indicating which fields in @attributes are valid
 *
 * Creates a new #GdkWindow using the attributes from
 * @attributes. See #GdkWindowAttr and #GdkWindowAttributesType for
 * more details.  Note: to use this on displays other than the default
 * display, @parent must be specified.
 *
 * Return value: (transfer none): the new #GdkWindow
 **/
GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkScreen *screen;
  GdkVisual *visual;
  int x, y;
  gboolean native;
  GdkEventMask event_mask;
  GdkWindow *real_parent;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    {
      GDK_NOTE (MULTIHEAD,
		g_warning ("gdk_window_new(): no parent specified reverting to parent = default root window"));

      screen = gdk_screen_get_default ();
      parent = gdk_screen_get_root_window (screen);
    }
  else
    screen = gdk_drawable_get_screen (parent);

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);

  if (GDK_WINDOW_DESTROYED (parent))
    {
      g_warning ("gdk_window_new(): parent is destroyed\n");
      return NULL;
    }

  if (attributes->window_type == GDK_WINDOW_OFFSCREEN &&
      _gdk_native_windows)
    {
      g_warning ("Offscreen windows not supported with native-windows gdk");
      return NULL;
    }

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *) window;

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  real_parent = parent;
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = gdk_screen_get_root_window (screen);

  private->parent = (GdkWindowObject *)parent;

  private->accept_focus = TRUE;
  private->focus_on_map = TRUE;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;

  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;

  private->x = x;
  private->y = y;
  private->width = (attributes->width > 1) ? (attributes->width) : (1);
  private->height = (attributes->height > 1) ? (attributes->height) : (1);

#ifdef GDK_WINDOWING_X11
  /* Work around a bug where Xorg refuses to map toplevel InputOnly windows
   * from an untrusted client: http://bugs.freedesktop.org/show_bug.cgi?id=6988
   */
  if (attributes->wclass == GDK_INPUT_ONLY &&
      private->parent->window_type == GDK_WINDOW_ROOT &&
      !G_LIKELY (GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (parent))->trusted_client))
    {
      g_warning ("Coercing GDK_INPUT_ONLY toplevel window to GDK_INPUT_OUTPUT to work around bug in Xorg server");
      attributes->wclass = GDK_INPUT_OUTPUT;
    }
#endif

  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->window_type for input-only windows
       * before
       */
      if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_ROOT)
	private->window_type = GDK_WINDOW_TEMP;
      else
	private->window_type = GDK_WINDOW_CHILD;
    }
  else
    private->window_type = attributes->window_type;

  /* Sanity checks */
  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_OFFSCREEN:
      if (GDK_WINDOW_TYPE (parent) != GDK_WINDOW_ROOT)
	g_warning (G_STRLOC "Toplevel windows must be created as children of\n"
		   "of a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
    case GDK_WINDOW_CHILD:
      break;
      break;
    default:
      g_warning (G_STRLOC "cannot make windows of type %d", private->window_type);
      return NULL;
    }

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_screen_get_system_visual (screen);

  private->event_mask = attributes->event_mask;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      private->input_only = FALSE;
      private->depth = visual->depth;

      private->bg_color.pixel = 0; /* TODO: BlackPixel (xdisplay, screen_x11->screen_num); */
      private->bg_color.red = private->bg_color.green = private->bg_color.blue = 0;

      private->bg_pixmap = NULL;
    }
  else
    {
      private->depth = 0;
      private->input_only = TRUE;
    }

  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);

  native = _gdk_native_windows; /* Default */
  if (private->parent->window_type == GDK_WINDOW_ROOT)
    native = TRUE; /* Always use native windows for toplevels */
  else if (!private->input_only &&
	   ((attributes_mask & GDK_WA_COLORMAP &&
	     attributes->colormap != gdk_drawable_get_colormap ((GdkDrawable *)private->parent)) ||
	    (attributes_mask & GDK_WA_VISUAL &&
	     attributes->visual != gdk_drawable_get_visual ((GdkDrawable *)private->parent))))
    native = TRUE; /* InputOutput window with different colormap or visual than parent, needs native window */

  if (gdk_window_is_offscreen (private))
    {
      _gdk_offscreen_window_new (window, screen, visual, attributes, attributes_mask);
      private->impl_window = private;
    }
  else if (native)
    {
      event_mask = get_native_event_mask (private);

      /* Create the impl */
      _gdk_window_impl_new (window, real_parent, screen, visual, event_mask, attributes, attributes_mask);
      private->impl_window = private;

      /* This will put the native window topmost in the native parent, which may
       * be wrong wrt other native windows in the non-native hierarchy, so restack */
      if (!_gdk_window_has_impl (real_parent))
	sync_native_window_stack_position (window);
    }
  else
    {
      private->impl_window = g_object_ref (private->parent->impl_window);
      private->impl = g_object_ref (private->impl_window->impl);
    }

  recompute_visible_regions (private, TRUE, FALSE);

  if (private->parent->window_type != GDK_WINDOW_ROOT)
    {
      /* Inherit redirection from parent */
      private->redirect = private->parent->redirect;
    }

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  return window;
}

static gboolean
is_parent_of (GdkWindow *parent,
	      GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static void
change_impl (GdkWindowObject *private,
	     GdkWindowObject *impl_window,
	     GdkDrawable *new)
{
  GList *l;
  GdkWindowObject *child;
  GdkDrawable *old_impl;
  GdkWindowObject *old_impl_window;

  old_impl = private->impl;
  old_impl_window = private->impl_window;
  if (private != impl_window)
    private->impl_window = g_object_ref (impl_window);
  else
    private->impl_window = private;
  private->impl = g_object_ref (new);
  if (old_impl_window != private)
    g_object_unref (old_impl_window);
  g_object_unref (old_impl);

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (child->impl == old_impl)
	change_impl (child, impl_window, new);
    }
}

static void
reparent_to_impl (GdkWindowObject *private)
{
  GList *l;
  GdkWindowObject *child;
  gboolean show;
  GdkWindowImplIface *impl_iface;

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

  /* Enumerate in reverse order so we get the right order for the native
     windows (first in childrens list is topmost, and reparent places on top) */
  for (l = g_list_last (private->children); l != NULL; l = l->prev)
    {
      child = l->data;

      if (child->impl == private->impl)
	reparent_to_impl (child);
      else
	{
	  show = impl_iface->reparent ((GdkWindow *)child,
				       (GdkWindow *)private,
				       child->x, child->y);
	  if (show)
	    gdk_window_show_unraised ((GdkWindow *)child);
	}
    }
}


/**
 * gdk_window_reparent:
 * @window: a #GdkWindow
 * @new_parent: new parent to move @window into
 * @x: X location inside the new parent
 * @y: Y location inside the new parent
 *
 * Reparents @window into the given @new_parent. The window being
 * reparented will be unmapped as a side effect.
 *
 **/
void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowObject *private;
  GdkWindowObject *new_parent_private;
  GdkWindowObject *old_parent;
  GdkScreen *screen;
  gboolean show, was_mapped, applied_clip_as_shape;
  gboolean do_reparent_to_impl;
  GdkEventMask old_native_event_mask;
  GdkWindowImplIface *impl_iface;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  if (GDK_WINDOW_DESTROYED (window) ||
      (new_parent && GDK_WINDOW_DESTROYED (new_parent)))
    return;

  screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));
  if (!new_parent)
    new_parent = gdk_screen_get_root_window (screen);

  private = (GdkWindowObject *) window;
  new_parent_private = (GdkWindowObject *)new_parent;

  /* No input-output children of input-only windows */
  if (new_parent_private->input_only && !private->input_only)
    return;

  /* Don't create loops in hierarchy */
  if (is_parent_of (window, new_parent))
    return;

  /* This might be wrong in the new parent, e.g. for non-native surfaces.
     To make sure we're ok, just wipe it. */
  gdk_window_drop_cairo_surface (private);

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  old_parent = private->parent;

  /* Break up redirection if inherited */
  if (private->redirect && private->redirect->redirected != private)
    {
      remove_redirect_from_children (private, private->redirect);
      private->redirect = NULL;
    }

  was_mapped = GDK_WINDOW_IS_MAPPED (window);
  show = FALSE;

  /* Reparenting to toplevel. Ensure we have a native window so this can work */
  if (new_parent_private->window_type == GDK_WINDOW_ROOT ||
      new_parent_private->window_type == GDK_WINDOW_FOREIGN)
    gdk_window_ensure_native (window);

  applied_clip_as_shape = should_apply_clip_as_shape (private);

  old_native_event_mask = 0;
  do_reparent_to_impl = FALSE;
  if (gdk_window_has_impl (private))
    {
      old_native_event_mask = get_native_event_mask (private);
      /* Native window */
      show = impl_iface->reparent (window, new_parent, x, y);
    }
  else
    {
      /* This shouldn't happen, as we created a native in this case, check anyway to see if that ever fails */
      g_assert (new_parent_private->window_type != GDK_WINDOW_ROOT &&
		new_parent_private->window_type != GDK_WINDOW_FOREIGN);

      show = was_mapped;
      gdk_window_hide (window);

      do_reparent_to_impl = TRUE;
      change_impl (private,
		   new_parent_private->impl_window,
		   new_parent_private->impl);
    }

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    {
      new_parent = gdk_screen_get_root_window (screen);
      new_parent_private = (GdkWindowObject *)new_parent;
    }

  if (old_parent)
    old_parent->children = g_list_remove (old_parent->children, window);

  private->parent = new_parent_private;
  private->x = x;
  private->y = y;

  new_parent_private->children = g_list_prepend (new_parent_private->children, window);

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_FOREIGN:
      if (private->toplevel_window_type != -1)
	GDK_WINDOW_TYPE (window) = private->toplevel_window_type;
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
      break;
    case GDK_WINDOW_OFFSCREEN:
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
	  GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  /* Save the original window type so we can restore it if the
	   * window is reparented back to be a toplevel
	   */
	  private->toplevel_window_type = GDK_WINDOW_TYPE (window);
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
	}
    }

  /* We might have changed window type for a native windows, so we
     need to change the event mask too. */
  if (gdk_window_has_impl (private))
    {
      GdkEventMask native_event_mask = get_native_event_mask (private);

      if (native_event_mask != old_native_event_mask)
	impl_iface->set_events (window,	native_event_mask);
    }

  /* Inherit parent redirect if we don't have our own */
  if (private->parent && private->redirect == NULL)
    {
      private->redirect = private->parent->redirect;
      apply_redirect_to_children (private, private->redirect);
    }

  _gdk_window_update_viewable (window);

  recompute_visible_regions (private, TRUE, FALSE);
  if (old_parent && GDK_WINDOW_TYPE (old_parent) != GDK_WINDOW_ROOT)
    recompute_visible_regions (old_parent, FALSE, TRUE);

  /* We used to apply the clip as the shape, but no more.
     Reset this to the real shape */
  if (gdk_window_has_impl (private) &&
      applied_clip_as_shape &&
      !should_apply_clip_as_shape (private))
    apply_shape (private, private->shape);

  if (do_reparent_to_impl)
    reparent_to_impl (private);
  else
    {
      /* The reparent will have put the native window topmost in the native parent,
       * which may be wrong wrt other native windows in the non-native hierarchy,
       * so restack */
      if (!gdk_window_has_impl (new_parent_private))
	sync_native_window_stack_position (window);
    }

  if (show)
    gdk_window_show_unraised (window);
  else
    _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static gboolean
temporary_disable_extension_events (GdkWindowObject *window)
{
  GdkWindowObject *child;
  GList *l;
  gboolean res;

  if (window->extension_events != 0)
    {
      g_object_set_data (G_OBJECT (window),
			 "gdk-window-extension-events",
			 GINT_TO_POINTER (window->extension_events));
      gdk_input_set_extension_events ((GdkWindow *)window, 0,
				      GDK_EXTENSION_EVENTS_NONE);
    }
  else
    res = FALSE;

  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (window->impl_window == child->impl_window)
	res |= temporary_disable_extension_events (child);
    }

  return res;
}

static void
reenable_extension_events (GdkWindowObject *window)
{
  GdkWindowObject *child;
  GList *l;
  int mask;

  mask = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
					      "gdk-window-extension-events"));

  if (mask != 0)
    {
      /* We don't have the mode here, so we pass in cursor.
	 This works with the current code since mode is not
	 stored except as part of the mask, and cursor doesn't
	 change the mask. */
      gdk_input_set_extension_events ((GdkWindow *)window, mask,
				      GDK_EXTENSION_EVENTS_CURSOR);
      g_object_set_data (G_OBJECT (window),
			 "gdk-window-extension-events",
			 NULL);
    }

  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (window->impl_window == child->impl_window)
	reenable_extension_events (window);
    }
}

/**
 * gdk_window_ensure_native:
 * @window: a #GdkWindow
 *
 * Tries to ensure that there is a window-system native window for this
 * GdkWindow. This may fail in some situations, returning %FALSE.
 *
 * Offscreen window and children of them can never have native windows.
 *
 * Some backends may not support native child windows.
 *
 * Returns: %TRUE if the window has a native window, %FALSE otherwise
 *
 * Since: 2.18
 */
gboolean
gdk_window_ensure_native (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowObject *impl_window;
  GdkDrawable *new_impl, *old_impl;
  GdkScreen *screen;
  GdkVisual *visual;
  GdkWindowAttr attributes;
  GdkWindowObject *above;
  GList listhead;
  GdkWindowImplIface *impl_iface;
  gboolean disabled_extension_events;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_ROOT ||
      GDK_WINDOW_DESTROYED (window))
    return FALSE;

  private = (GdkWindowObject *) window;

  impl_window = gdk_window_get_impl_window (private);

  if (gdk_window_is_offscreen (impl_window))
    return FALSE; /* native in offscreens not supported */

  if (impl_window == private)
    /* Already has an impl, and its not offscreen . */
    return TRUE;

  /* Need to create a native window */

  /* First we disable any extension events on the window or its
     descendants to handle the native input window moving */
  disabled_extension_events = FALSE;
  if (impl_window->input_window)
    disabled_extension_events = temporary_disable_extension_events (private);

  gdk_window_drop_cairo_surface (private);

  screen = gdk_drawable_get_screen (window);
  visual = gdk_drawable_get_visual (window);

  /* These fields are required in the attributes struct so we can't
     ignore them by clearing a flag in the attributes mask */
  attributes.wclass = private->input_only ? GDK_INPUT_ONLY : GDK_INPUT_OUTPUT;
  attributes.width = private->width;
  attributes.height = private->height;
  attributes.window_type = private->window_type;

  attributes.colormap = gdk_drawable_get_colormap (window);

  old_impl = private->impl;
  _gdk_window_impl_new (window, (GdkWindow *)private->parent,
			screen, visual,
			get_native_event_mask (private),
			&attributes, GDK_WA_COLORMAP);
  new_impl = private->impl;

  private->impl = old_impl;
  change_impl (private, private, new_impl);

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

  /* Native window creation will put the native window topmost in the
   * native parent, which may be wrong wrt the position of the previous
   * non-native window wrt to the other non-native children, so correct this.
   */
  above = find_native_sibling_above (private->parent, private);
  if (above)
    {
      listhead.data = window;
      listhead.prev = NULL;
      listhead.next = NULL;
      impl_iface->restack_under ((GdkWindow *)above, &listhead);
    }

  recompute_visible_regions (private, FALSE, FALSE);

  /* The shape may not have been set, as the clip region doesn't actually
     change, so do it here manually */
  if (should_apply_clip_as_shape (private))
    apply_clip_as_shape (private);

  reparent_to_impl (private);

  if (!private->input_only)
    {
      impl_iface->set_background (window, &private->bg_color);
      if (private->bg_pixmap != NULL)
	impl_iface->set_back_pixmap (window, private->bg_pixmap);
    }

  impl_iface->input_shape_combine_region (window,
					  private->input_shape,
					  0, 0);

  if (gdk_window_is_viewable (window))
    impl_iface->show (window, FALSE);

  if (disabled_extension_events)
    reenable_extension_events (private);

  return TRUE;
}

/**
 * _gdk_event_filter_unref:
 * @window: (allow-none): A #GdkWindow, or %NULL to be the global window
 * @filter: A window filter
 *
 * Release a reference to @filter.  Note this function may
 * mutate the list storage, so you need to handle this
 * if iterating over a list of filters.
 */
void
_gdk_event_filter_unref (GdkWindow       *window,
                         GdkEventFilter  *filter)
{
  GList **filters;
  GList *tmp_list;

  if (window == NULL)
    filters = &_gdk_default_filters;
  else
    {
      GdkWindowObject *private;
      private = (GdkWindowObject *) window;
      filters = &private->filters;
    }

  tmp_list = *filters;
  while (tmp_list)
    {
      GdkEventFilter *iter_filter = tmp_list->data;
      GList *node;

      node = tmp_list;
      tmp_list = tmp_list->next;

      if (iter_filter != filter)
        continue;

      g_assert (iter_filter->ref_count > 0);

      filter->ref_count--;
      if (filter->ref_count != 0)
        continue;

      *filters = g_list_remove_link (*filters, node);
      g_free (filter);
      g_list_free_1 (node);
    }
}

static void
window_remove_filters (GdkWindow *window)
{
  GdkWindowObject *obj = (GdkWindowObject*) window;
  while (obj->filters)
    _gdk_event_filter_unref (window, obj->filters->data);
}

/**
 * _gdk_window_destroy_hierarchy:
 * @window: a #GdkWindow
 * @recursing: If TRUE, then this is being called because a parent
 *            was destroyed.
 * @recursing_native: If TRUE, then this is being called because a native parent
 *            was destroyed. This generally means that the call to the
 *            windowing system to destroy the window can be omitted, since
 *            it will be destroyed as a result of the parent being destroyed.
 *            Unless @foreign_destroy.
 * @foreign_destroy: If TRUE, the window or a parent was destroyed by some
 *            external agency. The window has already been destroyed and no
 *            windowing system calls should be made. (This may never happen
 *            for some windowing systems.)
 *
 * Internal function to destroy a window. Like gdk_window_destroy(),
 * but does not drop the reference count created by gdk_window_new().
 **/
static void
_gdk_window_destroy_hierarchy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   recursing_native,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private;
  GdkWindowObject *temp_private;
  GdkWindowImplIface *impl_iface;
  GdkWindow *temp_window;
  GdkScreen *screen;
  GdkDisplay *display;
  GList *children;
  GList *tmp;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (GDK_DRAWABLE (window));
  screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));
  temp_window = g_object_get_qdata (G_OBJECT (screen), quark_pointer_window);
  if (temp_window == window)
    g_object_set_qdata (G_OBJECT (screen), quark_pointer_window, NULL);


  switch (private->window_type)
    {
    case GDK_WINDOW_ROOT:
      if (!screen->closed)
	{
	  g_error ("attempted to destroy root window");
	  break;
	}
      /* else fall thru */
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_OFFSCREEN:
      if (private->window_type == GDK_WINDOW_FOREIGN && !foreign_destroy)
	{
	  /* Logically, it probably makes more sense to send
	   * a "destroy yourself" message to the foreign window
	   * whether or not it's in our hierarchy; but for historical
	   * reasons, we only send "destroy yourself" messages to
	   * foreign windows in our hierarchy.
	   */
	  if (private->parent)
	    _gdk_windowing_window_destroy_foreign (window);

	  /* Also for historical reasons, we remove any filters
	   * on a foreign window when it or a parent is destroyed;
	   * this likely causes problems if two separate portions
	   * of code are maintaining filter lists on a foreign window.
	   */
	  window_remove_filters (window);
	}
      else
	{
	  if (private->parent)
	    {
	      GdkWindowObject *parent_private = (GdkWindowObject *)private->parent;

	      if (parent_private->children)
		parent_private->children = g_list_remove (parent_private->children, window);

	      if (!recursing &&
		  GDK_WINDOW_IS_MAPPED (window))
		{
		  recompute_visible_regions (private, TRUE, FALSE);
		  gdk_window_invalidate_in_parent (private);
		}
	    }

	  gdk_window_free_paint_stack (window);

	  if (private->bg_pixmap &&
	      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
	      private->bg_pixmap != GDK_NO_BG)
	    {
	      g_object_unref (private->bg_pixmap);
	      private->bg_pixmap = NULL;
	    }

          if (private->background)
            {
              cairo_pattern_destroy (private->background);
              private->background = NULL;
            }

	  if (private->window_type == GDK_WINDOW_FOREIGN)
	    g_assert (private->children == NULL);
	  else
	    {
	      children = tmp = private->children;
	      private->children = NULL;

	      while (tmp)
		{
		  temp_window = tmp->data;
		  tmp = tmp->next;

		  temp_private = (GdkWindowObject*) temp_window;
		  if (temp_private)
		    _gdk_window_destroy_hierarchy (temp_window,
						   TRUE,
						   recursing_native || gdk_window_has_impl (private),
						   foreign_destroy);
		}

	      g_list_free (children);
	    }

	  _gdk_window_clear_update_area (window);

	  gdk_window_drop_cairo_surface (private);

	  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

	  if (private->extension_events)
	    impl_iface->input_window_destroy (window);

	  if (gdk_window_has_impl (private))
	    impl_iface->destroy (window, recursing_native,
				 foreign_destroy);
	  else
	    {
	      /* hide to make sure we repaint and break grabs */
	      gdk_window_hide (window);
	    }

	  private->state |= GDK_WINDOW_STATE_WITHDRAWN;
	  private->parent = NULL;
	  private->destroyed = TRUE;

	  window_remove_filters (window);

	  gdk_drawable_set_colormap (GDK_DRAWABLE (window), NULL);

	  /* If we own the redirect, free it */
	  if (private->redirect && private->redirect->redirected == private)
	    gdk_window_redirect_free (private->redirect);

	  private->redirect = NULL;

	  if (display->pointer_info.toplevel_under_pointer == window)
	    {
	      g_object_unref (display->pointer_info.toplevel_under_pointer);
	      display->pointer_info.toplevel_under_pointer = NULL;
	    }

	  if (private->clip_region)
	    {
	      gdk_region_destroy (private->clip_region);
	      private->clip_region = NULL;
	    }

	  if (private->clip_region_with_children)
	    {
	      gdk_region_destroy (private->clip_region_with_children);
	      private->clip_region_with_children = NULL;
	    }

	  if (private->outstanding_moves)
	    {
	      g_list_foreach (private->outstanding_moves, (GFunc)gdk_window_region_move_free, NULL);
	      g_list_free (private->outstanding_moves);
	      private->outstanding_moves = NULL;
	    }
	}
      break;
    }
}

/**
 * _gdk_window_destroy:
 * @window: a #GdkWindow
 * @foreign_destroy: If TRUE, the window or a parent was destroyed by some
 *            external agency. The window has already been destroyed and no
 *            windowing system calls should be made. (This may never happen
 *            for some windowing systems.)
 *
 * Internal function to destroy a window. Like gdk_window_destroy(),
 * but does not drop the reference count created by gdk_window_new().
 **/
void
_gdk_window_destroy (GdkWindow *window,
		     gboolean   foreign_destroy)
{
  _gdk_window_destroy_hierarchy (window, FALSE, FALSE, foreign_destroy);
}

/**
 * gdk_window_destroy:
 * @window: a #GdkWindow
 *
 * Destroys the window system resources associated with @window and decrements @window's
 * reference count. The window system resources for all children of @window are also
 * destroyed, but the children's reference counts are not decremented.
 *
 * Note that a window will not be destroyed automatically when its reference count
 * reaches zero. You must call this function yourself before that happens.
 *
 **/
void
gdk_window_destroy (GdkWindow *window)
{
  _gdk_window_destroy_hierarchy (window, FALSE, FALSE, FALSE);
  g_object_unref (window);
}

/**
 * gdk_window_set_user_data:
 * @window: a #GdkWindow
 * @user_data: user data
 *
 * For most purposes this function is deprecated in favor of
 * g_object_set_data(). However, for historical reasons GTK+ stores
 * the #GtkWidget that owns a #GdkWindow as user data on the
 * #GdkWindow. So, custom widget implementations should use
 * this function for that. If GTK+ receives an event for a #GdkWindow,
 * and the user data for the window is non-%NULL, GTK+ will assume the
 * user data is a #GtkWidget, and forward the event to that widget.
 *
 **/
void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  ((GdkWindowObject*)window)->user_data = user_data;
}

/**
 * gdk_window_get_user_data:
 * @window: a #GdkWindow
 * @data: (out): return location for user data
 *
 * Retrieves the user data for @window, which is normally the widget
 * that @window belongs to. See gdk_window_set_user_data().
 *
 **/
void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  *data = ((GdkWindowObject*)window)->user_data;
}

/**
 * gdk_window_get_window_type:
 * @window: a #GdkWindow
 *
 * Gets the type of the window. See #GdkWindowType.
 *
 * Return value: type of window
 **/
GdkWindowType
gdk_window_get_window_type (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), (GdkWindowType) -1);

  return GDK_WINDOW_TYPE (window);
}

/**
 * gdk_window_is_destroyed:
 * @window: a #GdkWindow
 *
 * Check to see if a window is destroyed..
 *
 * Return value: %TRUE if the window is destroyed
 *
 * Since: 2.18
 **/
gboolean
gdk_window_is_destroyed (GdkWindow *window)
{
  return GDK_WINDOW_DESTROYED (window);
}

static void
to_embedder (GdkWindowObject *window,
             gdouble          offscreen_x,
             gdouble          offscreen_y,
             gdouble         *embedder_x,
             gdouble         *embedder_y)
{
  g_signal_emit (window, signals[TO_EMBEDDER], 0,
                 offscreen_x, offscreen_y,
                 embedder_x, embedder_y);
}

static void
from_embedder (GdkWindowObject *window,
               gdouble          embedder_x,
               gdouble          embedder_y,
               gdouble         *offscreen_x,
               gdouble         *offscreen_y)
{
  g_signal_emit (window, signals[FROM_EMBEDDER], 0,
                 embedder_x, embedder_y,
                 offscreen_x, offscreen_y);
}

/**
 * gdk_window_has_native:
 * @window: a #GdkWindow
 *
 * Checks whether the window has a native window or not. Note that
 * you can use gdk_window_ensure_native() if a native window is needed.
 *
 * Returns: %TRUE if the %window has a native window, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
gdk_window_has_native (GdkWindow *window)
{
  GdkWindowObject *w;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  w = GDK_WINDOW_OBJECT (window);

  return w->parent == NULL || w->parent->impl != w->impl;
}

/**
 * gdk_window_get_position:
 * @window: a #GdkWindow
 * @x: (out) (allow-none): X coordinate of window
 * @y: (out) (allow-none): Y coordinate of window
 *
 * Obtains the position of the window as reported in the
 * most-recently-processed #GdkEventConfigure. Contrast with
 * gdk_window_get_geometry() which queries the X server for the
 * current window position, regardless of which events have been
 * received or processed.
 *
 * The position coordinates are relative to the window's parent window.
 *
 **/
void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  GdkWindowObject *obj;

  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = (GdkWindowObject*) window;

  if (x)
    *x = obj->x;
  if (y)
    *y = obj->y;
}

/**
 * gdk_window_get_parent:
 * @window: a #GdkWindow
 *
 * Obtains the parent of @window, as known to GDK. Does not query the
 * X server; thus this returns the parent as passed to gdk_window_new(),
 * not the actual parent. This should never matter unless you're using
 * Xlib calls mixed with GDK calls on the X11 platform. It may also
 * matter for toplevel windows, because the window manager may choose
 * to reparent them.
 *
 * Note that you should use gdk_window_get_effective_parent() when
 * writing generic code that walks up a window hierarchy, because
 * gdk_window_get_parent() will most likely not do what you expect if
 * there are offscreen windows in the hierarchy.
 *
 * Return value: parent of @window
 **/
GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return (GdkWindow*) ((GdkWindowObject*) window)->parent;
}

/**
 * gdk_window_get_effective_parent:
 * @window: a #GdkWindow
 *
 * Obtains the parent of @window, as known to GDK. Works like
 * gdk_window_get_parent() for normal windows, but returns the
 * window's embedder for offscreen windows.
 *
 * See also: gdk_offscreen_window_get_embedder()
 *
 * Return value: effective parent of @window
 *
 * Since: 2.22
 **/
GdkWindow *
gdk_window_get_effective_parent (GdkWindow *window)
{
  GdkWindowObject *obj;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  obj = (GdkWindowObject *)window;

  if (gdk_window_is_offscreen (obj))
    return gdk_offscreen_window_get_embedder (window);
  else
    return (GdkWindow *) obj->parent;
}

/**
 * gdk_window_get_toplevel:
 * @window: a #GdkWindow
 *
 * Gets the toplevel window that's an ancestor of @window.
 *
 * Any window type but %GDK_WINDOW_CHILD is considered a
 * toplevel window, as is a %GDK_WINDOW_CHILD window that
 * has a root window as parent.
 *
 * Note that you should use gdk_window_get_effective_toplevel() when
 * you want to get to a window's toplevel as seen on screen, because
 * gdk_window_get_toplevel() will most likely not do what you expect
 * if there are offscreen windows in the hierarchy.
 *
 * Return value: the toplevel window containing @window
 **/
GdkWindow *
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowObject *obj;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  obj = (GdkWindowObject *)window;

  while (obj->window_type == GDK_WINDOW_CHILD)
    {
      if (gdk_window_is_toplevel (obj))
	break;
      obj = obj->parent;
    }

  return GDK_WINDOW (obj);
}

/**
 * gdk_window_get_effective_toplevel:
 * @window: a #GdkWindow
 *
 * Gets the toplevel window that's an ancestor of @window.
 *
 * Works like gdk_window_get_toplevel(), but treats an offscreen window's
 * embedder as its parent, using gdk_window_get_effective_parent().
 *
 * See also: gdk_offscreen_window_get_embedder()
 *
 * Return value: the effective toplevel window containing @window
 *
 * Since: 2.22
 **/
GdkWindow *
gdk_window_get_effective_toplevel (GdkWindow *window)
{
  GdkWindow *parent;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  while ((parent = gdk_window_get_effective_parent (window)) != NULL &&
	 (gdk_window_get_window_type (parent) != GDK_WINDOW_ROOT))
    window = parent;

  return window;
}

/**
 * gdk_window_get_children:
 * @window: a #GdkWindow
 *
 * Gets the list of children of @window known to GDK.
 * This function only returns children created via GDK,
 * so for example it's useless when used with the root window;
 * it only returns windows an application created itself.
 *
 * The returned list must be freed, but the elements in the
 * list need not be.
 *
 * Return value: (transfer container) (element-type GdkWindow):
 *     list of child windows inside @window
 **/
GList*
gdk_window_get_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return g_list_copy (GDK_WINDOW_OBJECT (window)->children);
}

/**
 * gdk_window_peek_children:
 * @window: a #GdkWindow
 *
 * Like gdk_window_get_children(), but does not copy the list of
 * children, so the list does not need to be freed.
 *
 * Return value: (transfer none) (element-type GdkWindow):
 *     a reference to the list of child windows in @window
 **/
GList *
gdk_window_peek_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return GDK_WINDOW_OBJECT (window)->children;
}

/**
 * gdk_window_add_filter:
 * @window: a #GdkWindow
 * @function: filter callback
 * @data: data to pass to filter callback
 *
 * Adds an event filter to @window, allowing you to intercept events
 * before they reach GDK. This is a low-level operation and makes it
 * easy to break GDK and/or GTK+, so you have to know what you're
 * doing. Pass %NULL for @window to get all events for all windows,
 * instead of events for a specific window.
 *
 * See gdk_display_add_client_message_filter() if you are interested
 * in X ClientMessage events.
 **/
void
gdk_window_add_filter (GdkWindow     *window,
		       GdkFilterFunc  function,
		       gpointer       data)
{
  GdkWindowObject *private;
  GList *tmp_list;
  GdkEventFilter *filter;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  if (private && GDK_WINDOW_DESTROYED (window))
    return;

  /* Filters are for the native events on the native window, so
     ensure there is a native window. */
  if (window)
    gdk_window_ensure_native (window);

  if (private)
    tmp_list = private->filters;
  else
    tmp_list = _gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      if ((filter->function == function) && (filter->data == data))
        {
          filter->ref_count++;
          return;
        }
      tmp_list = tmp_list->next;
    }

  filter = g_new (GdkEventFilter, 1);
  filter->function = function;
  filter->data = data;
  filter->ref_count = 1;
  filter->flags = 0;

  if (private)
    private->filters = g_list_append (private->filters, filter);
  else
    _gdk_default_filters = g_list_append (_gdk_default_filters, filter);
}

/**
 * gdk_window_remove_filter:
 * @window: a #GdkWindow
 * @function: previously-added filter function
 * @data: user data for previously-added filter function
 *
 * Remove a filter previously added with gdk_window_add_filter().
 *
 **/
void
gdk_window_remove_filter (GdkWindow     *window,
			  GdkFilterFunc  function,
			  gpointer       data)
{
  GdkWindowObject *private;
  GList *tmp_list;
  GdkEventFilter *filter;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;

  if (private)
    tmp_list = private->filters;
  else
    tmp_list = _gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      tmp_list = tmp_list->next;

      if ((filter->function == function) && (filter->data == data))
        {
          filter->flags |= GDK_EVENT_FILTER_REMOVED;
	  _gdk_event_filter_unref (window, filter);

          return;
        }
    }
}

/**
 * gdk_screen_get_toplevel_windows:
 * @screen: The #GdkScreen where the toplevels are located.
 *
 * Obtains a list of all toplevel windows known to GDK on the screen @screen.
 * A toplevel window is a child of the root window (see
 * gdk_get_default_root_window()).
 *
 * The returned list should be freed with g_list_free(), but
 * its elements need not be freed.
 *
 * Return value: (transfer container) (element-type GdkWindow):
 *     list of toplevel windows, free with g_list_free()
 *
 * Since: 2.2
 **/
GList *
gdk_screen_get_toplevel_windows (GdkScreen *screen)
{
  GdkWindow * root_window;
  GList *new_list = NULL;
  GList *tmp_list;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  root_window = gdk_screen_get_root_window (screen);

  tmp_list = ((GdkWindowObject *)root_window)->children;
  while (tmp_list)
    {
      GdkWindowObject *w = tmp_list->data;

      if (w->window_type != GDK_WINDOW_FOREIGN)
	new_list = g_list_prepend (new_list, w);
      tmp_list = tmp_list->next;
    }

  return new_list;
}

/**
 * gdk_window_get_toplevels:
 *
 * Obtains a list of all toplevel windows known to GDK on the default
 * screen (see gdk_screen_get_toplevel_windows()).
 * A toplevel window is a child of the root window (see
 * gdk_get_default_root_window()).
 *
 * The returned list should be freed with g_list_free(), but
 * its elements need not be freed.
 *
 * Return value: list of toplevel windows, free with g_list_free()
 *
 * Deprecated: 2.16: Use gdk_screen_get_toplevel_windows() instead.
 */
GList *
gdk_window_get_toplevels (void)
{
  return gdk_screen_get_toplevel_windows (gdk_screen_get_default ());
}

/**
 * gdk_window_is_visible:
 * @window: a #GdkWindow
 *
 * Checks whether the window has been mapped (with gdk_window_show() or
 * gdk_window_show_unraised()).
 *
 * Return value: %TRUE if the window is mapped
 **/
gboolean
gdk_window_is_visible (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return GDK_WINDOW_IS_MAPPED (window);
}

/**
 * gdk_window_is_viewable:
 * @window: a #GdkWindow
 *
 * Check if the window and all ancestors of the window are
 * mapped. (This is not necessarily "viewable" in the X sense, since
 * we only check as far as we have GDK window parents, not to the root
 * window.)
 *
 * Return value: %TRUE if the window is viewable
 **/
gboolean
gdk_window_is_viewable (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (private->destroyed)
    return FALSE;

  return private->viewable;
}

/**
 * gdk_window_get_state:
 * @window: a #GdkWindow
 *
 * Gets the bitwise OR of the currently active window state flags,
 * from the #GdkWindowState enumeration.
 *
 * Return value: window state bitfield
 **/
GdkWindowState
gdk_window_get_state (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return private->state;
}


/* This creates an empty "implicit" paint region for the impl window.
 * By itself this does nothing, but real paints to this window
 * or children of it can use this pixmap as backing to avoid allocating
 * multiple pixmaps for subwindow rendering. When doing so they
 * add to the region of the implicit paint region, which will be
 * pushed to the window when the implicit paint region is ended.
 * Such paints should not copy anything to the window on paint end, but
 * should rely on the implicit paint end.
 * The implicit paint will be automatically ended if someone draws
 * directly to the window or a child window.
 */
static gboolean
gdk_window_begin_implicit_paint (GdkWindow *window, GdkRectangle *rect)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint;

  g_assert (gdk_window_has_impl (private));

  if (_gdk_native_windows)
    return FALSE; /* No need for implicit paints since we can't merge draws anyway */

  if (GDK_IS_PAINTABLE (private->impl))
    return FALSE; /* Implementation does double buffering */

  if (private->paint_stack != NULL ||
      private->implicit_paint != NULL)
    return FALSE; /* Don't stack implicit paints */

  if (private->outstanding_surfaces != 0)
    return FALSE; /* May conflict with direct drawing to cairo surface */

  /* Never do implicit paints for foreign windows, they don't need
   * double buffer combination since they have no client side children,
   * and creating pixmaps for them is risky since they could disappear
   * at any time
   */
  if (private->window_type == GDK_WINDOW_FOREIGN)
    return FALSE;

  paint = g_new (GdkWindowPaint, 1);
  paint->region = gdk_region_new (); /* Empty */
  paint->x_offset = rect->x;
  paint->y_offset = rect->y;
  paint->uses_implicit = FALSE;
  paint->flushed = FALSE;
  paint->surface = NULL;
  paint->pixmap =
    gdk_pixmap_new (window,
		    MAX (rect->width, 1), MAX (rect->height, 1), -1);

  private->implicit_paint = paint;

  return TRUE;
}

/* Ensure that all content related to this (sub)window is pushed to the
   native region. If there is an active paint then that area is not
   pushed, in order to not show partially finished double buffers. */
static void
gdk_window_flush_implicit_paint (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;
  GdkWindowPaint *paint;
  GdkRegion *region;
  GdkGC *tmp_gc;
  GSList *list;

  impl_window = gdk_window_get_impl_window (private);
  if (impl_window->implicit_paint == NULL)
    return;

  paint = impl_window->implicit_paint;
  paint->flushed = TRUE;
  region = gdk_region_copy (private->clip_region_with_children);

  /* Don't flush active double buffers, as that may show partially done
   * rendering */
  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *tmp_paint = list->data;

      gdk_region_subtract (region, tmp_paint->region);
    }

  gdk_region_offset (region, private->abs_x, private->abs_y);
  gdk_region_intersect (region, paint->region);

  if (!GDK_WINDOW_DESTROYED (window) && !gdk_region_empty (region))
    {
      /* Remove flushed region from the implicit paint */
      gdk_region_subtract (paint->region, region);

      /* Some regions are valid, push these to window now */
      tmp_gc = _gdk_drawable_get_scratch_gc ((GdkDrawable *)window, FALSE);
      _gdk_gc_set_clip_region_internal (tmp_gc, region, TRUE);
      gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
			 0, 0, paint->x_offset, paint->y_offset, -1, -1);
      /* Reset clip region of the cached GdkGC */
      gdk_gc_set_clip_region (tmp_gc, NULL);
    }
  else
    gdk_region_destroy (region);
}

/* Ends an implicit paint, paired with gdk_window_begin_implicit_paint returning TRUE */
static void
gdk_window_end_implicit_paint (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint;
  GdkGC *tmp_gc;

  g_assert (gdk_window_has_impl (private));

  g_assert (private->implicit_paint != NULL);

  paint = private->implicit_paint;

  private->implicit_paint = NULL;

  if (!GDK_WINDOW_DESTROYED (window) && !gdk_region_empty (paint->region))
    {
      /* Some regions are valid, push these to window now */
      tmp_gc = _gdk_drawable_get_scratch_gc ((GdkDrawable *)window, FALSE);
      _gdk_gc_set_clip_region_internal (tmp_gc, paint->region, TRUE);
      gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
			 0, 0, paint->x_offset, paint->y_offset, -1, -1);
      /* Reset clip region of the cached GdkGC */
      gdk_gc_set_clip_region (tmp_gc, NULL);
    }
  else
    gdk_region_destroy (paint->region);

  g_object_unref (paint->pixmap);
  g_free (paint);
}

/**
 * gdk_window_begin_paint_rect:
 * @window: a #GdkWindow
 * @rectangle: rectangle you intend to draw to
 *
 * A convenience wrapper around gdk_window_begin_paint_region() which
 * creates a rectangular region for you. See
 * gdk_window_begin_paint_region() for details.
 *
 **/
void
gdk_window_begin_paint_rect (GdkWindow          *window,
			     const GdkRectangle *rectangle)
{
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  region = gdk_region_rectangle (rectangle);
  gdk_window_begin_paint_region (window, region);
  gdk_region_destroy (region);
}

/**
 * gdk_window_begin_paint_region:
 * @window: a #GdkWindow
 * @region: region you intend to draw to
 *
 * Indicates that you are beginning the process of redrawing @region.
 * A backing store (offscreen buffer) large enough to contain @region
 * will be created. The backing store will be initialized with the
 * background color or background pixmap for @window. Then, all
 * drawing operations performed on @window will be diverted to the
 * backing store.  When you call gdk_window_end_paint(), the backing
 * store will be copied to @window, making it visible onscreen. Only
 * the part of @window contained in @region will be modified; that is,
 * drawing operations are clipped to @region.
 *
 * The net result of all this is to remove flicker, because the user
 * sees the finished product appear all at once when you call
 * gdk_window_end_paint(). If you draw to @window directly without
 * calling gdk_window_begin_paint_region(), the user may see flicker
 * as individual drawing operations are performed in sequence.  The
 * clipping and background-initializing features of
 * gdk_window_begin_paint_region() are conveniences for the
 * programmer, so you can avoid doing that work yourself.
 *
 * When using GTK+, the widget system automatically places calls to
 * gdk_window_begin_paint_region() and gdk_window_end_paint() around
 * emissions of the expose_event signal. That is, if you're writing an
 * expose event handler, you can assume that the exposed area in
 * #GdkEventExpose has already been cleared to the window background,
 * is already set as the clip region, and already has a backing store.
 * Therefore in most cases, application code need not call
 * gdk_window_begin_paint_region(). (You can disable the automatic
 * calls around expose events on a widget-by-widget basis by calling
 * gtk_widget_set_double_buffered().)
 *
 * If you call this function multiple times before calling the
 * matching gdk_window_end_paint(), the backing stores are pushed onto
 * a stack. gdk_window_end_paint() copies the topmost backing store
 * onscreen, subtracts the topmost region from all other regions in
 * the stack, and pops the stack. All drawing operations affect only
 * the topmost backing store in the stack. One matching call to
 * gdk_window_end_paint() is required for each call to
 * gdk_window_begin_paint_region().
 *
 **/
void
gdk_window_begin_paint_region (GdkWindow       *window,
			       const GdkRegion *region)
{
#ifdef USE_BACKING_STORE
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkRectangle clip_box;
  GdkWindowPaint *paint, *implicit_paint;
  GdkWindowObject *impl_window;
  GSList *list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_IS_PAINTABLE (private->impl))
    {
      GdkPaintableIface *iface = GDK_PAINTABLE_GET_IFACE (private->impl);

      if (iface->begin_paint_region)
	iface->begin_paint_region ((GdkPaintable*)private->impl, window, region);

      return;
    }

  impl_window = gdk_window_get_impl_window (private);
  implicit_paint = impl_window->implicit_paint;

  paint = g_new (GdkWindowPaint, 1);
  paint->region = gdk_region_copy (region);
  paint->region_tag = new_region_tag ();

  gdk_region_intersect (paint->region, private->clip_region_with_children);
  gdk_region_get_clipbox (paint->region, &clip_box);

  /* Convert to impl coords */
  gdk_region_offset (paint->region, private->abs_x, private->abs_y);

  /* Mark the region as valid on the implicit paint */

  if (implicit_paint)
    gdk_region_union (implicit_paint->region, paint->region);

  /* Convert back to normal coords */
  gdk_region_offset (paint->region, -private->abs_x, -private->abs_y);

  if (implicit_paint)
    {
      paint->uses_implicit = TRUE;
      paint->pixmap = g_object_ref (implicit_paint->pixmap);
      paint->x_offset = -private->abs_x + implicit_paint->x_offset;
      paint->y_offset = -private->abs_y + implicit_paint->y_offset;
    }
  else
    {
      paint->uses_implicit = FALSE;
      paint->x_offset = clip_box.x;
      paint->y_offset = clip_box.y;
      paint->pixmap =
	gdk_pixmap_new (window,
			MAX (clip_box.width, 1), MAX (clip_box.height, 1), -1);
    }

  paint->surface = _gdk_drawable_ref_cairo_surface (paint->pixmap);

  if (paint->surface)
    cairo_surface_set_device_offset (paint->surface,
				     -paint->x_offset, -paint->y_offset);

  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *tmp_paint = list->data;

      gdk_region_subtract (tmp_paint->region, paint->region);
    }

  private->paint_stack = g_slist_prepend (private->paint_stack, paint);

  if (!gdk_region_empty (paint->region))
    {
      gdk_window_clear_backing_region (window,
				       paint->region);
    }

#endif /* USE_BACKING_STORE */
}

static void
setup_redirect_clip (GdkWindow      *window,
		     GdkGC          *gc,
		     int            *x_offset_out,
		     int            *y_offset_out)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkRegion *visible_region;
  GdkRectangle dest_rect;
  GdkRegion *tmpreg;
  GdkWindow *toplevel;
  int x_offset, y_offset;

  toplevel = GDK_WINDOW (private->redirect->redirected);

  /* Get the clip region for gc clip rect + window hierarchy in
     window relative coords */
  visible_region =
    _gdk_window_calculate_full_clip_region (window, toplevel,
					    TRUE,
					    &x_offset,
					    &y_offset);

  /* Compensate for the source pos/size */
  x_offset -= private->redirect->src_x;
  y_offset -= private->redirect->src_y;
  dest_rect.x = -x_offset;
  dest_rect.y = -y_offset;
  dest_rect.width = private->redirect->width;
  dest_rect.height = private->redirect->height;
  tmpreg = gdk_region_rectangle (&dest_rect);
  gdk_region_intersect (visible_region, tmpreg);
  gdk_region_destroy (tmpreg);

  /* Compensate for the dest pos */
  x_offset += private->redirect->dest_x;
  y_offset += private->redirect->dest_y;

  gdk_gc_set_clip_region (gc, visible_region); /* This resets clip origin! */

  /* offset clip and tiles from window coords to pixmaps coords */
  gdk_gc_offset (gc, -x_offset, -y_offset);

  gdk_region_destroy (visible_region);

  *x_offset_out = x_offset;
  *y_offset_out = y_offset;
}

/**
 * gdk_window_end_paint:
 * @window: a #GdkWindow
 *
 * Indicates that the backing store created by the most recent call to
 * gdk_window_begin_paint_region() should be copied onscreen and
 * deleted, leaving the next-most-recent backing store or no backing
 * store at all as the active paint region. See
 * gdk_window_begin_paint_region() for full details. It is an error to
 * call this function without a matching
 * gdk_window_begin_paint_region() first.
 *
 **/
void
gdk_window_end_paint (GdkWindow *window)
{
#ifdef USE_BACKING_STORE
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *composited;
  GdkWindowPaint *paint;
  GdkGC *tmp_gc;
  GdkRectangle clip_box;
  gint x_offset, y_offset;
  GdkRegion *full_clip;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_IS_PAINTABLE (private->impl))
    {
      GdkPaintableIface *iface = GDK_PAINTABLE_GET_IFACE (private->impl);

      if (iface->end_paint)
	iface->end_paint ((GdkPaintable*)private->impl);
      return;
    }

  if (private->paint_stack == NULL)
    {
      g_warning (G_STRLOC": no preceding call to gdk_window_begin_paint_region(), see documentation");
      return;
    }

  paint = private->paint_stack->data;

  private->paint_stack = g_slist_delete_link (private->paint_stack,
					      private->paint_stack);

  gdk_region_get_clipbox (paint->region, &clip_box);

  tmp_gc = _gdk_drawable_get_scratch_gc (window, FALSE);

  x_offset = -private->abs_x;
  y_offset = -private->abs_y;

  if (!paint->uses_implicit)
    {
      gdk_window_flush_outstanding_moves (window);

      full_clip = gdk_region_copy (private->clip_region_with_children);
      gdk_region_intersect (full_clip, paint->region);
      _gdk_gc_set_clip_region_internal (tmp_gc, full_clip, TRUE); /* Takes ownership of full_clip */
      gdk_gc_set_clip_origin (tmp_gc, - x_offset, - y_offset);
      gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
			 clip_box.x - paint->x_offset,
			 clip_box.y - paint->y_offset,
			 clip_box.x - x_offset, clip_box.y - y_offset,
			 clip_box.width, clip_box.height);
    }

  if (private->redirect)
    {
      int x_offset, y_offset;

      /* TODO: Should also use paint->region for clipping */
      setup_redirect_clip (window, tmp_gc, &x_offset, &y_offset);
      gdk_draw_drawable (private->redirect->pixmap, tmp_gc, paint->pixmap,
			 clip_box.x - paint->x_offset,
			 clip_box.y - paint->y_offset,
			 clip_box.x + x_offset,
			 clip_box.y + y_offset,
			 clip_box.width, clip_box.height);
    }

  /* Reset clip region of the cached GdkGC */
  gdk_gc_set_clip_region (tmp_gc, NULL);

  cairo_surface_destroy (paint->surface);
  g_object_unref (paint->pixmap);
  gdk_region_destroy (paint->region);
  g_free (paint);

  /* find a composited window in our hierarchy to signal its
   * parent to redraw, calculating the clip box as we go...
   *
   * stop if parent becomes NULL since then we'd have nowhere
   * to draw (ie: 'composited' will always be non-NULL here).
   */
  for (composited = private;
       composited->parent;
       composited = composited->parent)
    {
      int width, height;

      gdk_drawable_get_size (GDK_DRAWABLE (composited->parent),
			     &width, &height);

      clip_box.x += composited->x;
      clip_box.y += composited->y;
      clip_box.width = MIN (clip_box.width, width - clip_box.x);
      clip_box.height = MIN (clip_box.height, height - clip_box.y);

      if (composited->composited)
	{
	  gdk_window_invalidate_rect (GDK_WINDOW (composited->parent),
				      &clip_box, FALSE);
	  break;
	}
    }
#endif /* USE_BACKING_STORE */
}

static void
gdk_window_free_paint_stack (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (private->paint_stack)
    {
      GSList *tmp_list = private->paint_stack;

      while (tmp_list)
	{
	  GdkWindowPaint *paint = tmp_list->data;

	  if (tmp_list == private->paint_stack)
	    g_object_unref (paint->pixmap);

	  gdk_region_destroy (paint->region);
	  g_free (paint);

	  tmp_list = tmp_list->next;
	}

      g_slist_free (private->paint_stack);
      private->paint_stack = NULL;
    }
}

static void
do_move_region_bits_on_impl (GdkWindowObject *impl_window,
			     GdkRegion *dest_region, /* In impl window coords */
			     int dx, int dy)
{
  GdkGC *tmp_gc;
  GdkRectangle copy_rect;
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;

  /* We need to get data from subwindows here, because we might have
   * shaped a native window over the moving region (with bg none,
   * so the pixels are still there). In fact we might need to get data
   * from overlapping native window that are not children of this window,
   * so we copy from the toplevel with INCLUDE_INFERIORS.
   */
  private = impl_window;
  while (!gdk_window_is_toplevel (private) &&
         !private->composited &&
         gdk_drawable_get_visual ((GdkDrawable *) private) == gdk_drawable_get_visual ((GdkDrawable *) private->parent))
    {
      dx -= private->parent->abs_x + private->x;
      dy -= private->parent->abs_y + private->y;
      private = gdk_window_get_impl_window (private->parent);
    }
  tmp_gc = _gdk_drawable_get_subwindow_scratch_gc ((GdkWindow *)private);

  gdk_region_get_clipbox (dest_region, &copy_rect);
  gdk_gc_set_clip_region (tmp_gc, dest_region);

  /* The region area is moved and we queue translations for all expose events
     to the source area that were sent prior to the copy */
  gdk_region_offset (dest_region, -dx, -dy); /* Move to source region */
  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

  impl_iface->queue_translation ((GdkWindow *)impl_window,
				 tmp_gc,
				 dest_region, dx, dy);

  gdk_draw_drawable (impl_window->impl,
		     tmp_gc,
		     private->impl,
		     copy_rect.x-dx, copy_rect.y-dy,
		     copy_rect.x, copy_rect.y,
		     copy_rect.width, copy_rect.height);
  gdk_gc_set_clip_region (tmp_gc, NULL);
}

static GdkWindowRegionMove *
gdk_window_region_move_new (GdkRegion *region,
			    int dx, int dy)
{
  GdkWindowRegionMove *move;

  move = g_slice_new (GdkWindowRegionMove);
  move->dest_region  = gdk_region_copy (region);
  move->dx = dx;
  move->dy = dy;

  return move;
}

static void
gdk_window_region_move_free (GdkWindowRegionMove *move)
{
  gdk_region_destroy (move->dest_region);
  g_slice_free (GdkWindowRegionMove, move);
}

static void
append_move_region (GdkWindowObject *impl_window,
		    GdkRegion *new_dest_region,
		    int dx, int dy)
{
  GdkWindowRegionMove *move, *old_move;
  GdkRegion *new_total_region, *old_total_region;
  GdkRegion *source_overlaps_destination;
  GdkRegion *non_overwritten;
  gboolean added_move;
  GList *l, *prev;

  if (gdk_region_empty (new_dest_region))
    return;

  /* In principle this could just append the move to the list of outstanding
     moves that will be replayed before drawing anything when we're handling
     exposes. However, we'd like to do a bit better since its commonly the case
     that we get multiple copies where A is copied to B and then B is copied
     to C, and we'd like to express this as a simple copy A to C operation. */

  /* We approach this by taking the new move and pushing it ahead of moves
     starting at the end of the list and stopping when its not safe to do so.
     It's not safe to push past a move if either the source of the new move
     is in the destination of the old move, or if the destination of the new
     move is in the source of the new move, or if the destination of the new
     move overlaps the destination of the old move. We simplify this by
     just comparing the total regions (src + dest) */
  new_total_region = gdk_region_copy (new_dest_region);
  gdk_region_offset (new_total_region, -dx, -dy);
  gdk_region_union (new_total_region, new_dest_region);

  added_move = FALSE;
  for (l = g_list_last (impl_window->outstanding_moves); l != NULL; l = prev)
    {
      prev = l->prev;
      old_move = l->data;

      old_total_region = gdk_region_copy (old_move->dest_region);
      gdk_region_offset (old_total_region, -old_move->dx, -old_move->dy);
      gdk_region_union (old_total_region, old_move->dest_region);

      gdk_region_intersect (old_total_region, new_total_region);
      /* If these regions intersect then its not safe to push the
	 new region before the old one */
      if (!gdk_region_empty (old_total_region))
	{
	  /* The area where the new moves source overlaps the old ones
	     destination */
	  source_overlaps_destination = gdk_region_copy (new_dest_region);
	  gdk_region_offset (source_overlaps_destination, -dx, -dy);
	  gdk_region_intersect (source_overlaps_destination, old_move->dest_region);
	  gdk_region_offset (source_overlaps_destination, dx, dy);

	  /* We can do all sort of optimizations here, but to do things safely it becomes
	     quite complicated. However, a very common case is that you copy something first,
	     then copy all that or a subset of it to a new location (i.e. if you scroll twice
	     in the same direction). We'd like to detect this case and optimize it to one
	     copy. */
	  if (gdk_region_equal (source_overlaps_destination, new_dest_region))
	    {
	      /* This means we might be able to replace the old move and the new one
		 with the new one read from the old ones source, and a second copy of
		 the non-overwritten parts of the old move. However, such a split
		 is only valid if the source in the old move isn't overwritten
		 by the destination of the new one */

	      /* the new destination of old move if split is ok: */
	      non_overwritten = gdk_region_copy (old_move->dest_region);
	      gdk_region_subtract (non_overwritten, new_dest_region);
	      /* move to source region */
	      gdk_region_offset (non_overwritten, -old_move->dx, -old_move->dy);

	      gdk_region_intersect (non_overwritten, new_dest_region);
	      if (gdk_region_empty (non_overwritten))
		{
		  added_move = TRUE;
		  move = gdk_window_region_move_new (new_dest_region,
						     dx + old_move->dx,
						     dy + old_move->dy);

		  impl_window->outstanding_moves =
		    g_list_insert_before (impl_window->outstanding_moves,
					  l, move);
		  gdk_region_subtract (old_move->dest_region, new_dest_region);
		}
	      gdk_region_destroy (non_overwritten);
	    }

	  gdk_region_destroy (source_overlaps_destination);
	  gdk_region_destroy (old_total_region);
	  break;
	}
      gdk_region_destroy (old_total_region);
    }

  gdk_region_destroy (new_total_region);

  if (!added_move)
    {
      move = gdk_window_region_move_new (new_dest_region, dx, dy);

      if (l == NULL)
	impl_window->outstanding_moves =
	  g_list_prepend (impl_window->outstanding_moves,
			  move);
      else
	impl_window->outstanding_moves =
	  g_list_insert_before (impl_window->outstanding_moves,
				l->next, move);
    }
}

/* Moves bits and update area by dx/dy in impl window.
   Takes ownership of region to avoid copy (because we may change it) */
static void
move_region_on_impl (GdkWindowObject *impl_window,
		     GdkRegion *region, /* In impl window coords */
		     int dx, int dy)
{
  if ((dx == 0 && dy == 0) ||
      gdk_region_empty (region))
    {
      gdk_region_destroy (region);
      return;
    }

  g_assert (impl_window == gdk_window_get_impl_window (impl_window));

  /* Move any old invalid regions in the copy source area by dx/dy */
  if (impl_window->update_area)
    {
      GdkRegion *update_area;

      update_area = gdk_region_copy (region);

      /* Convert from target to source */
      gdk_region_offset (update_area, -dx, -dy);
      gdk_region_intersect (update_area, impl_window->update_area);
      /* We only copy the area, so keep the old update area invalid.
	 It would be safe to remove it too, as code that uses
	 move_region_on_impl generally also invalidate the source
	 area. However, it would just use waste cycles. */

      /* Convert back */
      gdk_region_offset (update_area, dx, dy);
      gdk_region_union (impl_window->update_area, update_area);

      /* This area of the destination is now invalid,
	 so no need to copy to it.  */
      gdk_region_subtract (region, update_area);

      gdk_region_destroy (update_area);
    }

  /* If we're currently exposing this window, don't copy to this
     destination, as it will be overdrawn when the expose is done,
     instead invalidate it and repaint later. */
  if (impl_window->implicit_paint)
    {
      GdkWindowPaint *implicit_paint = impl_window->implicit_paint;
      GdkRegion *exposing;

      exposing = gdk_region_copy (implicit_paint->region);
      gdk_region_intersect (exposing, region);
      gdk_region_subtract (region, exposing);

      impl_window_add_update_area (impl_window, exposing);
      gdk_region_destroy (exposing);
    }

  if (impl_window->outstanding_surfaces == 0) /* Enable flicker free handling of moves. */
    append_move_region (impl_window, region, dx, dy);
  else
    do_move_region_bits_on_impl (impl_window,
				 region, dx, dy);

  gdk_region_destroy (region);
}

/* Flushes all outstanding changes to the window, call this
 * before drawing directly to the window (i.e. outside a begin/end_paint pair).
 */
static void
gdk_window_flush_outstanding_moves (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowObject *impl_window;
  GdkWindowRegionMove *move;

  private = (GdkWindowObject *) window;

  impl_window = gdk_window_get_impl_window (private);

  while (impl_window->outstanding_moves)
    {
      move = impl_window->outstanding_moves->data;
      impl_window->outstanding_moves = g_list_delete_link (impl_window->outstanding_moves,
							   impl_window->outstanding_moves);

      do_move_region_bits_on_impl (impl_window,
				   move->dest_region, move->dx, move->dy);

      gdk_window_region_move_free (move);
    }
}

/**
 * gdk_window_flush:
 * @window: a #GdkWindow
 *
 * Flush all outstanding cached operations on a window, leaving the
 * window in a state which reflects all that has been drawn before.
 *
 * Gdk uses multiple kinds of caching to get better performance and
 * nicer drawing. For instance, during exposes all paints to a window
 * using double buffered rendering are keep on a pixmap until the last
 * window has been exposed. It also delays window moves/scrolls until
 * as long as possible until next update to avoid tearing when moving
 * windows.
 *
 * Normally this should be completely invisible to applications, as
 * we automatically flush the windows when required, but this might
 * be needed if you for instance mix direct native drawing with
 * gdk drawing. For Gtk widgets that don't use double buffering this
 * will be called automatically before sending the expose event.
 *
 * Since: 2.18
 **/
void
gdk_window_flush (GdkWindow *window)
{
  gdk_window_flush_outstanding_moves (window);
  gdk_window_flush_implicit_paint (window);
}

/* If we're about to move/resize or otherwise change the
 * hierarchy of a client side window in an impl and we're
 * called from an expose event handler then we need to
 * flush any already painted parts of the implicit paint
 * that are not part of the current paint, as these may
 * be used when scrolling or may overdraw the changes
 * caused by the hierarchy change.
 */
static void
gdk_window_flush_if_exposing (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowObject *impl_window;

  private = (GdkWindowObject *) window;
  impl_window = gdk_window_get_impl_window (private);

  /* If we're in an implicit paint (i.e. in an expose handler, flush
     all the already finished exposes to get things to an uptodate state. */
  if (impl_window->implicit_paint)
    gdk_window_flush (window);
}


static void
gdk_window_flush_recursive_helper (GdkWindowObject *window,
				   GdkWindow *impl)
{
  GdkWindowObject *child;
  GList *l;

  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (child->impl == impl)
	/* Same impl, ignore */
	gdk_window_flush_recursive_helper (child, impl);
      else
	gdk_window_flush_recursive (child);
    }
}

static void
gdk_window_flush_recursive (GdkWindowObject *window)
{
  gdk_window_flush ((GdkWindow *)window);
  gdk_window_flush_recursive_helper (window, window->impl);
}

static void
gdk_window_get_offsets (GdkWindow *window,
			gint      *x_offset,
			gint      *y_offset)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      *x_offset = paint->x_offset;
      *y_offset = paint->y_offset;
    }
  else
    {
      *x_offset = -private->abs_x;
      *y_offset = -private->abs_y;
    }
}

/**
 * gdk_window_get_internal_paint_info:
 * @window: a #GdkWindow
 * @real_drawable: (out): location to store the drawable to which drawing should be
 *            done.
 * @x_offset: (out): location to store the X offset between coordinates in @window,
 *            and the underlying window system primitive coordinates for
 *            *@real_drawable.
 * @y_offset: (out): location to store the Y offset between coordinates in @window,
 *            and the underlying window system primitive coordinates for
 *            *@real_drawable.
 *
 * If you bypass the GDK layer and use windowing system primitives to
 * draw directly onto a #GdkWindow, then you need to deal with two
 * details: there may be an offset between GDK coordinates and windowing
 * system coordinates, and GDK may have redirected drawing to a offscreen
 * pixmap as the result of a gdk_window_begin_paint_region() calls.
 * This function allows retrieving the information you need to compensate
 * for these effects.
 *
 * This function exposes details of the GDK implementation, and is thus
 * likely to change in future releases of GDK.
 **/
void
gdk_window_get_internal_paint_info (GdkWindow    *window,
				    GdkDrawable **real_drawable,
				    gint         *x_offset,
				    gint         *y_offset)
{
  gint x_off, y_off;

  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;

  if (real_drawable)
    {
      if (private->paint_stack)
	{
	  GdkWindowPaint *paint = private->paint_stack->data;
	  *real_drawable = paint->pixmap;
	}
      else
	{
	  /* This means you're probably gonna be doing some weird shit
	     directly to the window, so we flush all outstanding stuff */
	  gdk_window_flush (window);
	  *real_drawable = window;
	}
    }

  gdk_window_get_offsets (window, &x_off, &y_off);

  if (x_offset)
    *x_offset = x_off;
  if (y_offset)
    *y_offset = y_off;
}

static GdkDrawable *
start_draw_helper (GdkDrawable *drawable,
		   GdkGC *gc,
		   gint *x_offset_out,
		   gint *y_offset_out)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  gint x_offset, y_offset;
  GdkDrawable *impl;
  gint old_clip_x = gc->clip_x_origin;
  gint old_clip_y = gc->clip_y_origin;
  GdkRegion *clip;
  guint32 clip_region_tag;
  GdkWindowPaint *paint;

  paint = NULL;
  if (private->paint_stack)
    paint = private->paint_stack->data;

  if (paint)
    {
      x_offset = paint->x_offset;
      y_offset = paint->y_offset;
    }
  else
    {
      x_offset = -private->abs_x;
      y_offset = -private->abs_y;
    }

  if (x_offset != 0 || y_offset != 0)
    {
      gdk_gc_set_clip_origin (gc,
			      old_clip_x - x_offset,
			      old_clip_y - y_offset);
      gdk_gc_set_ts_origin (gc,
			    gc->ts_x_origin - x_offset,
			    gc->ts_y_origin - y_offset);
    }

  *x_offset_out = x_offset;
  *y_offset_out = y_offset;

  /* Add client side window clip region to gc */
  clip = NULL;
  if (paint)
    {
      /* Only need clipping if using implicit paint, otherwise
	 the pixmap is clipped when copying to the window in end_paint */
      if (paint->uses_implicit)
	{
	  /* This includes the window clip */
	  clip = paint->region;
	}
      clip_region_tag = paint->region_tag;

      /* After having set up the drawable clip rect on a GC we need to make sure
       * that we draw to th the impl, otherwise the pixmap code will reset the
       * drawable clip. */
      impl = ((GdkPixmapObject *)(paint->pixmap))->impl;
    }
  else
    {
      /* Drawing directly to the window, flush anything outstanding to
	 guarantee ordering. */
      gdk_window_flush ((GdkWindow *)drawable);

      /* Don't clip when drawing to root or all native */
      if (!_gdk_native_windows && private->window_type != GDK_WINDOW_ROOT)
	{
	  if (_gdk_gc_get_subwindow (gc) == GDK_CLIP_BY_CHILDREN)
	    clip = private->clip_region_with_children;
	  else
	    clip = private->clip_region;
	}
      clip_region_tag = private->clip_tag;
      impl = private->impl;
    }

  if (clip)
    _gdk_gc_add_drawable_clip (gc,
			       clip_region_tag, clip,
			       /* If there was a clip origin set appart from the
				* window offset, need to take that into
				* consideration */
			       -old_clip_x, -old_clip_y);

  return impl;
}

#define BEGIN_DRAW					\
  {							\
    GdkDrawable *impl;					\
    gint x_offset, y_offset;				\
    gint old_clip_x = gc->clip_x_origin;		\
    gint old_clip_y = gc->clip_y_origin;		\
    gint old_ts_x = gc->ts_x_origin;			\
    gint old_ts_y = gc->ts_y_origin;			\
    impl = start_draw_helper (drawable, gc,		\
			      &x_offset, &y_offset);

#define END_DRAW					    \
    if (x_offset != 0 || y_offset != 0)			    \
     {                                                      \
       gdk_gc_set_clip_origin (gc, old_clip_x, old_clip_y); \
       gdk_gc_set_ts_origin (gc, old_ts_x, old_ts_y);       \
     }                                                      \
  }

#define BEGIN_DRAW_MACRO \
  {

#define END_DRAW_MACRO \
  }

typedef struct
{
  GdkDrawable *drawable;
  GdkGC *gc;

  gint x_offset;
  gint y_offset;

  gint clip_x;
  gint clip_y;
  gint ts_x;
  gint ts_y;
} DirectDrawInfo;

GdkDrawable *
_gdk_drawable_begin_direct_draw (GdkDrawable *drawable,
				 GdkGC *gc,
				 gpointer *priv_data,
				 gint *x_offset_out,
				 gint *y_offset_out)
{
  GdkDrawable *out_impl = NULL;

  g_return_val_if_fail (priv_data != NULL, NULL);

  *priv_data = NULL;

  if (GDK_IS_PIXMAP (drawable))
    {
      /* We bypass the GdkPixmap functions, so do this ourself */
      _gdk_gc_remove_drawable_clip (gc);

      out_impl = drawable;

      *x_offset_out = 0;
      *y_offset_out = 0;
    }
  else
    {
      DirectDrawInfo *priv;

      if (GDK_WINDOW_DESTROYED (drawable))
        return NULL;

      BEGIN_DRAW;

      if (impl == NULL)
        return NULL;

      out_impl = impl;

      *x_offset_out = x_offset;
      *y_offset_out = y_offset;

      priv = g_new (DirectDrawInfo, 1);

      priv->drawable = impl;
      priv->gc = gc;

      priv->x_offset = x_offset;
      priv->y_offset = y_offset;
      priv->clip_x = old_clip_x;
      priv->clip_y = old_clip_y;
      priv->ts_x = old_ts_x;
      priv->ts_y = old_ts_y;

      *priv_data = (gpointer) priv;

      END_DRAW_MACRO;
    }

  return out_impl;
}

void
_gdk_drawable_end_direct_draw (gpointer priv_data)
{
  DirectDrawInfo *priv;
  GdkGC *gc;

  /* Its a GdkPixmap or the call to _gdk_drawable_begin_direct_draw failed. */
  if (priv_data == NULL)
    return;

  priv = priv_data;
  gc = priv->gc;

  /* This is only for GdkWindows - if GdkPixmaps need any handling here in
   * the future, then we should keep track of what type of drawable it is in
   * DirectDrawInfo. */
  BEGIN_DRAW_MACRO;

  {
    gint x_offset = priv->x_offset;
    gint y_offset = priv->y_offset;
    gint old_clip_x = priv->clip_x;
    gint old_clip_y = priv->clip_y;
    gint old_ts_x = priv->ts_x;
    gint old_ts_y = priv->ts_y;

    END_DRAW;
  }

  g_free (priv_data);
}

static GdkGC *
gdk_window_create_gc (GdkDrawable     *drawable,
		      GdkGCValues     *values,
		      GdkGCValuesMask  mask)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  return gdk_gc_new_with_values (((GdkWindowObject *) drawable)->impl,
				 values, mask);
}

static void
gdk_window_draw_rectangle (GdkDrawable *drawable,
			   GdkGC       *gc,
			   gboolean     filled,
			   gint         x,
			   gint         y,
			   gint         width,
			   gint         height)
{
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;
  gdk_draw_rectangle (impl, gc, filled,
		      x - x_offset, y - y_offset, width, height);
  END_DRAW;
}

static void
gdk_window_draw_arc (GdkDrawable *drawable,
		     GdkGC       *gc,
		     gboolean     filled,
		     gint         x,
		     gint         y,
		     gint         width,
		     gint         height,
		     gint         angle1,
		     gint         angle2)
{
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;
  gdk_draw_arc (impl, gc, filled,
		x - x_offset, y - y_offset,
		width, height, angle1, angle2);
  END_DRAW;
}

static void
gdk_window_draw_polygon (GdkDrawable *drawable,
			 GdkGC       *gc,
			 gboolean     filled,
			 GdkPoint    *points,
			 gint         npoints)
{
  GdkPoint *new_points;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  if (x_offset != 0 || y_offset != 0)
    {
      int i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  gdk_draw_polygon (impl, gc, filled, new_points, npoints);

  if (new_points != points)
    g_free (new_points);

  END_DRAW;
}

static void
gdk_window_draw_text (GdkDrawable *drawable,
		      GdkFont     *font,
		      GdkGC       *gc,
		      gint         x,
		      gint         y,
		      const gchar *text,
		      gint         text_length)
{
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;
  gdk_draw_text (impl, font, gc,
		 x - x_offset, y - y_offset, text, text_length);
  END_DRAW;
}

static void
gdk_window_draw_text_wc (GdkDrawable    *drawable,
			 GdkFont        *font,
			 GdkGC          *gc,
			 gint            x,
			 gint            y,
			 const GdkWChar *text,
			 gint            text_length)
{
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;
  gdk_draw_text_wc (impl, font, gc,
		    x - x_offset, y - y_offset, text, text_length);
  END_DRAW;
}

static GdkDrawable *
gdk_window_get_source_drawable (GdkDrawable *drawable)
{
  GdkWindow *window = GDK_WINDOW (drawable);
  GdkWindowObject *private;

  private = (GdkWindowObject *) window;
  if (GDK_DRAWABLE_GET_CLASS (private->impl)->get_source_drawable)
    return GDK_DRAWABLE_GET_CLASS (private->impl)->get_source_drawable (private->impl);

  return drawable;
}

static GdkDrawable *
gdk_window_get_composite_drawable (GdkDrawable *drawable,
				   gint         x,
				   gint         y,
				   gint         width,
				   gint         height,
				   gint        *composite_x_offset,
				   gint        *composite_y_offset)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GSList *list;
  GdkPixmap *tmp_pixmap;
  GdkRectangle rect;
  GdkGC *tmp_gc;
  gboolean overlap_buffer;
  GdkDrawable *source;
  GdkWindowObject *impl_window;
  GdkWindowPaint *implicit_paint;

  *composite_x_offset = -private->abs_x;
  *composite_y_offset = -private->abs_y;

  if ((GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable)))
    return g_object_ref (_gdk_drawable_get_source_drawable (drawable));

  /* See if any buffered part is overlapping the part we want
   * to get
   */
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  overlap_buffer = FALSE;

  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *paint = list->data;
      GdkOverlapType overlap;

      overlap = gdk_region_rect_in (paint->region, &rect);

      if (overlap == GDK_OVERLAP_RECTANGLE_IN)
	{
	  *composite_x_offset = paint->x_offset;
	  *composite_y_offset = paint->y_offset;

	  return g_object_ref (paint->pixmap);
	}
      else if (overlap == GDK_OVERLAP_RECTANGLE_PART)
	{
	  overlap_buffer = TRUE;
	  break;
	}
    }

  impl_window = gdk_window_get_impl_window (private);
  implicit_paint = impl_window->implicit_paint;
  if (implicit_paint)
    {
      GdkOverlapType overlap;

      rect.x += private->abs_x;
      rect.y += private->abs_y;

      overlap = gdk_region_rect_in (implicit_paint->region, &rect);
      if (overlap == GDK_OVERLAP_RECTANGLE_IN)
	{
	  *composite_x_offset = -private->abs_x + implicit_paint->x_offset;
	  *composite_y_offset = -private->abs_y + implicit_paint->y_offset;

	  return g_object_ref (implicit_paint->pixmap);
	}
      else if (overlap == GDK_OVERLAP_RECTANGLE_PART)
	overlap_buffer = TRUE;
    }

  if (!overlap_buffer)
    return g_object_ref (_gdk_drawable_get_source_drawable (drawable));

  tmp_pixmap = gdk_pixmap_new (drawable, width, height, -1);
  tmp_gc = _gdk_drawable_get_scratch_gc (tmp_pixmap, FALSE);

  source = _gdk_drawable_get_source_drawable (drawable);

  /* Copy the current window contents */
  gdk_draw_drawable (tmp_pixmap,
		     tmp_gc,
		     GDK_WINDOW_OBJECT (source)->impl,
		     x - *composite_x_offset,
		     y - *composite_y_offset,
		     0, 0,
		     width, height);

  /* paint the backing stores */
  if (implicit_paint)
    {
      GdkWindowPaint *paint = list->data;

      gdk_gc_set_clip_region (tmp_gc, paint->region);
      gdk_gc_set_clip_origin (tmp_gc, -x  - paint->x_offset, -y  - paint->y_offset);

      gdk_draw_drawable (tmp_pixmap, tmp_gc, paint->pixmap,
			 x - paint->x_offset,
			 y - paint->y_offset,
			 0, 0, width, height);
    }

  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *paint = list->data;

      if (paint->uses_implicit)
	continue; /* We already copied this above */

      gdk_gc_set_clip_region (tmp_gc, paint->region);
      gdk_gc_set_clip_origin (tmp_gc, -x, -y);

      gdk_draw_drawable (tmp_pixmap, tmp_gc, paint->pixmap,
			 x - paint->x_offset,
			 y - paint->y_offset,
			 0, 0, width, height);
    }

  /* Reset clip region of the cached GdkGC */
  gdk_gc_set_clip_region (tmp_gc, NULL);

  /* Set these to location of tmp_pixmap within the window */
  *composite_x_offset = x;
  *composite_y_offset = y;

  return tmp_pixmap;
}

static GdkRegion*
gdk_window_get_clip_region (GdkDrawable *drawable)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkRegion *result;

  result = gdk_region_copy (private->clip_region);

  if (private->paint_stack)
    {
      GdkRegion *paint_region = gdk_region_new ();
      GSList *tmp_list = private->paint_stack;

      while (tmp_list)
	{
	  GdkWindowPaint *paint = tmp_list->data;

	  gdk_region_union (paint_region, paint->region);

	  tmp_list = tmp_list->next;
	}

      gdk_region_intersect (result, paint_region);
      gdk_region_destroy (paint_region);
    }

  return result;
}

static GdkRegion*
gdk_window_get_visible_region (GdkDrawable *drawable)
{
  GdkWindowObject *private = (GdkWindowObject*) drawable;

  return gdk_region_copy (private->clip_region);
}

static void
gdk_window_draw_drawable (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkPixmap   *src,
			  gint         xsrc,
			  gint         ysrc,
			  gint         xdest,
			  gint         ydest,
			  gint         width,
			  gint         height,
			  GdkDrawable *original_src)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  /* Call the method directly to avoid getting the composite drawable again */
  GDK_DRAWABLE_GET_CLASS (impl)->draw_drawable_with_src (impl, gc,
							 src,
							 xsrc, ysrc,
							 xdest - x_offset,
							 ydest - y_offset,
							 width, height,
							 original_src);

  if (!private->paint_stack)
    {
      /* We might have drawn from an obscured part of a client
	 side window, if so we need to send graphics exposures */
      if (_gdk_gc_get_exposures (gc) &&
	  GDK_IS_WINDOW (original_src))
	{
	  GdkRegion *exposure_region;
	  GdkRegion *clip;
	  GdkRectangle r;

	  r.x = xdest;
	  r.y = ydest;
	  r.width = width;
	  r.height = height;
	  exposure_region = gdk_region_rectangle (&r);

	  if (_gdk_gc_get_subwindow (gc) == GDK_CLIP_BY_CHILDREN)
	    clip = private->clip_region_with_children;
	  else
	    clip = private->clip_region;
	  gdk_region_intersect (exposure_region, clip);

	  _gdk_gc_remove_drawable_clip (gc);
	  clip = _gdk_gc_get_clip_region (gc);
	  if (clip)
	    {
	      gdk_region_offset (exposure_region,
				 old_clip_x,
				 old_clip_y);
	      gdk_region_intersect (exposure_region, clip);
	      gdk_region_offset (exposure_region,
				 -old_clip_x,
				 -old_clip_y);
	    }

	  /* Note: We don't clip by the clip mask if set, so this
	     may invalidate to much */

	  /* Remove the area that is correctly copied from the src.
	   * Note that xsrc/ysrc has been corrected for abs_x/y offsets already,
	   * which need to be undone */
	  clip = gdk_drawable_get_visible_region (original_src);
	  gdk_region_offset (clip,
			     xdest - (xsrc - GDK_WINDOW_OBJECT (original_src)->abs_x),
			     ydest - (ysrc - GDK_WINDOW_OBJECT (original_src)->abs_y));
	  gdk_region_subtract (exposure_region, clip);
	  gdk_region_destroy (clip);

	  gdk_window_invalidate_region_full (GDK_WINDOW (private),
					      exposure_region,
					      _gdk_gc_get_subwindow (gc) == GDK_INCLUDE_INFERIORS,
					      CLEAR_BG_ALL);

	  gdk_region_destroy (exposure_region);
	}
    }

  END_DRAW;
}

static void
gdk_window_draw_points (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPoint    *points,
			gint         npoints)
{
  GdkPoint *new_points;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  gdk_draw_points (impl, gc, new_points, npoints);

  if (new_points != points)
    g_free (new_points);

  END_DRAW;
}

static void
gdk_window_draw_segments (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkSegment  *segs,
			  gint         nsegs)
{
  GdkSegment *new_segs;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_segs = g_new (GdkSegment, nsegs);
      for (i=0; i<nsegs; i++)
	{
	  new_segs[i].x1 = segs[i].x1 - x_offset;
	  new_segs[i].y1 = segs[i].y1 - y_offset;
	  new_segs[i].x2 = segs[i].x2 - x_offset;
	  new_segs[i].y2 = segs[i].y2 - y_offset;
	}
    }
  else
    new_segs = segs;

  gdk_draw_segments (impl, gc, new_segs, nsegs);

  if (new_segs != segs)
    g_free (new_segs);

  END_DRAW;
}

static void
gdk_window_draw_lines (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  GdkPoint *new_points;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  gdk_draw_lines (impl, gc, new_points, npoints);

  if (new_points != points)
    g_free (new_points);

  END_DRAW;
}

static void
gdk_window_draw_glyphs (GdkDrawable      *drawable,
			GdkGC            *gc,
			PangoFont        *font,
			gint              x,
			gint              y,
			PangoGlyphString *glyphs)
{
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;
  gdk_draw_glyphs (impl, gc, font,
		   x - x_offset, y - y_offset, glyphs);
  END_DRAW;
}

static void
gdk_window_draw_glyphs_transformed (GdkDrawable      *drawable,
				    GdkGC            *gc,
				    PangoMatrix      *matrix,
				    PangoFont        *font,
				    gint              x,
				    gint              y,
				    PangoGlyphString *glyphs)
{
  PangoMatrix tmp_matrix;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  if (x_offset != 0 || y_offset != 0)
    {
      if (matrix)
	{
	  tmp_matrix = *matrix;
	  tmp_matrix.x0 -= x_offset;
	  tmp_matrix.y0 -= y_offset;
	  matrix = &tmp_matrix;
	}
      else if (GDK_PANGO_UNITS_OVERFLOWS (x_offset, y_offset))
	{
	  PangoMatrix identity = PANGO_MATRIX_INIT;

	  tmp_matrix = identity;
	  tmp_matrix.x0 -= x_offset;
	  tmp_matrix.y0 -= y_offset;
	  matrix = &tmp_matrix;
	}
      else
	{
	  x -= x_offset * PANGO_SCALE;
	  y -= y_offset * PANGO_SCALE;
	}
    }

  gdk_draw_glyphs_transformed (impl, gc, matrix, font, x, y, glyphs);

  END_DRAW;
}

typedef struct {
  cairo_t *cr; /* if non-null, it means use this cairo context */
  GdkGC *gc;   /* if non-null, it means use this GC instead */
} BackingRectMethod;

static void
setup_backing_rect_method (BackingRectMethod *method, GdkWindow *window, GdkWindowPaint *paint, int x_offset_cairo, int y_offset_cairo)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (private->bg_pixmap == GDK_PARENT_RELATIVE_BG && private->parent)
    {
      GdkWindowPaint tmp_paint;

      tmp_paint = *paint;
      tmp_paint.x_offset += private->x;
      tmp_paint.y_offset += private->y;

      x_offset_cairo += private->x;
      y_offset_cairo += private->y;

      setup_backing_rect_method (method, GDK_WINDOW (private->parent), &tmp_paint, x_offset_cairo, y_offset_cairo);
    }
  else if (private->bg_pixmap &&
	   private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
	   private->bg_pixmap != GDK_NO_BG)
    {
/* This is a workaround for https://bugs.freedesktop.org/show_bug.cgi?id=4320.
 * In it, using a pixmap as a repeating pattern in Cairo, and painting it to a
 * pixmap destination surface, can be very slow (on the order of seconds for a
 * whole-screen copy).  The workaround is to use pretty much the same code that
 * we used in GTK+ 2.6 (pre-Cairo), which clears the double-buffer pixmap with
 * a tiled GC XFillRectangle().
 */

/* Actually computing this flag is left as an exercise for the reader */
#if defined (G_OS_UNIX)
#  define GDK_CAIRO_REPEAT_IS_FAST 0
#else
#  define GDK_CAIRO_REPEAT_IS_FAST 1
#endif

#if GDK_CAIRO_REPEAT_IS_FAST
      cairo_surface_t *surface = _gdk_drawable_ref_cairo_surface (private->bg_pixmap);
      cairo_pattern_t *pattern = cairo_pattern_create_for_surface (surface);
      cairo_surface_destroy (surface);

      if (x_offset_cairo != 0 || y_offset_cairo != 0)
	{
	  cairo_matrix_t matrix;
	  cairo_matrix_init_translate (&matrix, x_offset_cairo, y_offset_cairo);
	  cairo_pattern_set_matrix (pattern, &matrix);
	}

      cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

      method->cr = cairo_create (paint->surface);
      method->gc = NULL;

      cairo_set_source (method->cr, pattern);
      cairo_pattern_destroy (pattern);
#else
      guint gc_mask;
      GdkGCValues gc_values;

      gc_values.fill = GDK_TILED;
      gc_values.tile = private->bg_pixmap;
      gc_values.ts_x_origin = -x_offset_cairo;
      gc_values.ts_y_origin = -y_offset_cairo;

      gc_mask = GDK_GC_FILL | GDK_GC_TILE | GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN;

      method->gc = gdk_gc_new_with_values (paint->pixmap, &gc_values, gc_mask);
#endif
    }
  else
    {
      method->cr = cairo_create (paint->surface);

      gdk_cairo_set_source_color (method->cr, &private->bg_color);
    }
}

static void
gdk_window_clear_backing_region (GdkWindow *window,
				 GdkRegion *region)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint = private->paint_stack->data;
  BackingRectMethod method;
  GdkRegion *clip;
  GdkRectangle clipbox;
#if 0
  GTimer *timer;
  double elapsed;
#endif

  if (GDK_WINDOW_DESTROYED (window))
    return;

#if 0
  timer = g_timer_new ();
#endif

  method.cr = NULL;
  method.gc = NULL;
  setup_backing_rect_method (&method, window, paint, 0, 0);

  clip = gdk_region_copy (paint->region);
  gdk_region_intersect (clip, region);
  gdk_region_get_clipbox (clip, &clipbox);


  if (method.cr)
    {
      g_assert (method.gc == NULL);

      gdk_cairo_region (method.cr, clip);
      cairo_fill (method.cr);

      cairo_destroy (method.cr);
#if 0
      elapsed = g_timer_elapsed (timer, NULL);
      g_print ("Draw the background with Cairo: %fs\n", elapsed);
#endif
    }
  else
    {
      g_assert (method.gc != NULL);

      gdk_gc_set_clip_region (method.gc, clip);
      gdk_draw_rectangle (window, method.gc, TRUE,
			  clipbox.x, clipbox.y,
			  clipbox.width, clipbox.height);
      g_object_unref (method.gc);

#if 0
      elapsed = g_timer_elapsed (timer, NULL);
      g_print ("Draw the background with GDK: %fs\n", elapsed);
#endif
    }

  gdk_region_destroy (clip);

#if 0
  g_timer_destroy (timer);
#endif
}

static void
gdk_window_clear_backing_region_redirect (GdkWindow *window,
					  GdkRegion *region)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowRedirect *redirect = private->redirect;
  GdkRegion *clip_region;
  GdkRectangle clipbox;
  gint x_offset, y_offset;
  BackingRectMethod method;
  GdkWindowPaint paint;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  clip_region = _gdk_window_calculate_full_clip_region (window,
							GDK_WINDOW (redirect->redirected),
							TRUE,
							&x_offset, &y_offset);
  gdk_region_intersect (clip_region, region);

  /* offset is from redirected window origin to window origin, convert to
     the offset from the redirected pixmap origin to the window origin */
  x_offset += redirect->dest_x - redirect->src_x;
  y_offset += redirect->dest_y - redirect->src_y;

  /* Convert region to pixmap coords */
  gdk_region_offset (clip_region, x_offset, y_offset);

  paint.x_offset = 0;
  paint.y_offset = 0;
  paint.pixmap = redirect->pixmap;
  paint.surface = _gdk_drawable_ref_cairo_surface (redirect->pixmap);

  method.cr = NULL;
  method.gc = NULL;
  setup_backing_rect_method (&method, window, &paint, -x_offset, -y_offset);

  if (method.cr)
    {
      g_assert (method.gc == NULL);

      gdk_cairo_region (method.cr, clip_region);
      cairo_fill (method.cr);

      cairo_destroy (method.cr);
    }
  else
    {
      g_assert (method.gc != NULL);

      gdk_region_get_clipbox (clip_region, &clipbox);
      gdk_gc_set_clip_region (method.gc, clip_region);
      gdk_draw_rectangle (redirect->pixmap, method.gc, TRUE,
			  clipbox.x, clipbox.y,
			  clipbox.width, clipbox.height);
      g_object_unref (method.gc);

    }

  gdk_region_destroy (clip_region);
  cairo_surface_destroy (paint.surface);
}

static void
gdk_window_clear_backing_region_direct (GdkWindow *window,
					GdkRegion *region)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  BackingRectMethod method;
  GdkWindowPaint paint;
  GdkRegion *clip;
  GdkRectangle clipbox;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  paint.x_offset = 0;
  paint.y_offset = 0;
  paint.pixmap = window;
  paint.surface = _gdk_drawable_ref_cairo_surface (window);

  method.cr = NULL;
  method.gc = NULL;
  setup_backing_rect_method (&method, window, &paint, 0, 0);

  clip = gdk_region_copy (private->clip_region_with_children);
  gdk_region_intersect (clip, region);
  gdk_region_get_clipbox (clip, &clipbox);

  if (method.cr)
    {
      g_assert (method.gc == NULL);

      gdk_cairo_region (method.cr, clip);
      cairo_fill (method.cr);

      cairo_destroy (method.cr);
    }
  else
    {
      g_assert (method.gc != NULL);

      gdk_gc_set_clip_region (method.gc, clip);
      gdk_draw_rectangle (window, method.gc, TRUE,
			  clipbox.x, clipbox.y,
			  clipbox.width, clipbox.height);
      g_object_unref (method.gc);

    }

  gdk_region_destroy (clip);
  cairo_surface_destroy (paint.surface);
}


/**
 * gdk_window_clear:
 * @window: a #GdkWindow
 *
 * Clears an entire @window to the background color or background pixmap.
 **/
void
gdk_window_clear (GdkWindow *window)
{
  gint width, height;

  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);

  gdk_window_clear_area (window, 0, 0,
			 width, height);
}

/* TRUE if the window clears to the same pixels as a native
   window clear. This means you can use the native window
   clearing operation, and additionally it means any clearing
   done by the native window system for you will already be right */
static gboolean
clears_as_native (GdkWindowObject *private)
{
  GdkWindowObject *next;

  next = private;
  do
    {
      private = next;
      if (gdk_window_has_impl (private))
	return TRUE;
      next = private->parent;
    }
  while (private->bg_pixmap == GDK_PARENT_RELATIVE_BG &&
	 next && next->window_type != GDK_WINDOW_ROOT);
  return FALSE;
}

static void
gdk_window_clear_region_internal (GdkWindow *window,
				  GdkRegion *region,
				  gboolean   send_expose)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplIface *impl_iface;

  if (private->paint_stack)
    gdk_window_clear_backing_region (window, region);
  else
    {
      if (private->redirect)
	gdk_window_clear_backing_region_redirect (window, region);

      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

      if (impl_iface->clear_region && clears_as_native (private))
	{
	  GdkRegion *copy;
	  copy = gdk_region_copy (region);
	  gdk_region_intersect (copy,
				private->clip_region_with_children);


	  /* Drawing directly to the window, flush anything outstanding to
	     guarantee ordering. */
	  gdk_window_flush (window);
	  impl_iface->clear_region (window, copy, send_expose);

	  gdk_region_destroy (copy);
	}
      else
	{
	  gdk_window_clear_backing_region_direct (window, region);
	  if (send_expose)
	    gdk_window_invalidate_region (window, region, FALSE);
	}
    }
}

static void
gdk_window_clear_area_internal (GdkWindow *window,
				gint       x,
				gint       y,
				gint       width,
				gint       height,
				gboolean   send_expose)
{
  GdkRectangle rect;
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Terminate early to avoid weird interpretation of
     zero width/height by XClearArea */
  if (width == 0 || height == 0)
    return;

  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  region = gdk_region_rectangle (&rect);
  gdk_window_clear_region_internal (window,
				    region,
				    send_expose);
  gdk_region_destroy (region);

}

/**
 * gdk_window_clear_area:
 * @window: a #GdkWindow
 * @x: x coordinate of rectangle to clear
 * @y: y coordinate of rectangle to clear
 * @width: width of rectangle to clear
 * @height: height of rectangle to clear
 *
 * Clears an area of @window to the background color or background pixmap.
 *
 **/
void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  gdk_window_clear_area_internal (window,
				  x, y,
				  width, height,
				  FALSE);
}

/**
 * gdk_window_clear_area_e:
 * @window: a #GdkWindow
 * @x: x coordinate of rectangle to clear
 * @y: y coordinate of rectangle to clear
 * @width: width of rectangle to clear
 * @height: height of rectangle to clear
 *
 * Like gdk_window_clear_area(), but also generates an expose event for
 * the cleared area.
 *
 * This function has a stupid name because it dates back to the mists
 * time, pre-GDK-1.0.
 *
 **/
void
gdk_window_clear_area_e (GdkWindow *window,
			 gint       x,
			 gint       y,
			 gint       width,
			 gint       height)
{
  gdk_window_clear_area_internal (window,
				  x, y,
				  width, height,
				  TRUE);
}

static void
gdk_window_draw_image (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkImage    *image,
		       gint         xsrc,
		       gint         ysrc,
		       gint         xdest,
		       gint         ydest,
		       gint         width,
		       gint         height)
{
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;
  gdk_draw_image (impl, gc, image, xsrc, ysrc,
		  xdest - x_offset, ydest - y_offset,
		  width, height);
  END_DRAW;
}

static void
gdk_window_draw_pixbuf (GdkDrawable     *drawable,
			GdkGC           *gc,
			GdkPixbuf       *pixbuf,
			gint             src_x,
			gint             src_y,
			gint             dest_x,
			gint             dest_y,
			gint             width,
			gint             height,
			GdkRgbDither     dither,
			gint             x_dither,
			gint             y_dither)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkDrawableClass *klass;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  /* If no gc => no user clipping, but we need clipping
     for window emulation, so use a scratch gc */
  if (!gc)
    gc = _gdk_drawable_get_scratch_gc (drawable, FALSE);

  BEGIN_DRAW;

  klass = GDK_DRAWABLE_GET_CLASS (impl);

  if (private->paint_stack)
    klass->draw_pixbuf (impl, gc, pixbuf, src_x, src_y,
			dest_x - x_offset, dest_y - y_offset,
			width, height,
			dither, x_dither - x_offset, y_dither - y_offset);
  else
    klass->draw_pixbuf (impl, gc, pixbuf, src_x, src_y,
			dest_x - x_offset, dest_y - y_offset,
			width, height,
			dither, x_dither, y_dither);
  END_DRAW;
}

static void
gdk_window_draw_trapezoids (GdkDrawable   *drawable,
			    GdkGC	  *gc,
			    GdkTrapezoid  *trapezoids,
			    gint           n_trapezoids)
{
  GdkTrapezoid *new_trapezoids = NULL;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  BEGIN_DRAW;

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_trapezoids = g_new (GdkTrapezoid, n_trapezoids);
      for (i=0; i < n_trapezoids; i++)
	{
	  new_trapezoids[i].y1 = trapezoids[i].y1 - y_offset;
	  new_trapezoids[i].x11 = trapezoids[i].x11 - x_offset;
	  new_trapezoids[i].x21 = trapezoids[i].x21 - x_offset;
	  new_trapezoids[i].y2 = trapezoids[i].y2 - y_offset;
	  new_trapezoids[i].x12 = trapezoids[i].x12 - x_offset;
	  new_trapezoids[i].x22 = trapezoids[i].x22 - x_offset;
	}

      trapezoids = new_trapezoids;
    }

  gdk_draw_trapezoids (impl, gc, trapezoids, n_trapezoids);

  g_free (new_trapezoids);

  END_DRAW;
}

static void
gdk_window_real_get_size (GdkDrawable *drawable,
			  gint *width,
			  gint *height)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  if (width)
    *width = private->width;
  if (height)
    *height = private->height;
}

static GdkVisual*
gdk_window_real_get_visual (GdkDrawable *drawable)
{
  GdkColormap *colormap;

  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  colormap = gdk_drawable_get_colormap (drawable);
  return colormap ? gdk_colormap_get_visual (colormap) : NULL;
}

static gint
gdk_window_real_get_depth (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), 0);

  return ((GdkWindowObject *)GDK_WINDOW (drawable))->depth;
}

static GdkScreen*
gdk_window_real_get_screen (GdkDrawable *drawable)
{
  return gdk_drawable_get_screen (GDK_WINDOW_OBJECT (drawable)->impl);
}

static void
gdk_window_real_set_colormap (GdkDrawable *drawable,
			      GdkColormap *cmap)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (drawable));

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  private = (GdkWindowObject *)drawable;

  /* different colormap than parent, requires native window */
  if (!private->input_only &&
      cmap != gdk_drawable_get_colormap ((GdkDrawable *)(private->parent)))
    gdk_window_ensure_native ((GdkWindow *)drawable);

  gdk_drawable_set_colormap (private->impl, cmap);
}

static GdkColormap*
gdk_window_real_get_colormap (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  return gdk_drawable_get_colormap (((GdkWindowObject*)drawable)->impl);
}

static GdkImage*
gdk_window_copy_to_image (GdkDrawable     *drawable,
			  GdkImage        *image,
			  gint             src_x,
			  gint             src_y,
			  gint             dest_x,
			  gint             dest_y,
			  gint             width,
			  gint             height)
{
  GdkWindowObject *private = (GdkWindowObject *) drawable;
  gint x_offset, y_offset;

  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  /* If we're here, a composite image was not necessary, so
   * we can ignore the paint stack.
   */

  /* TODO: Is this right? */
  x_offset = 0;
  y_offset = 0;

  return gdk_drawable_copy_to_image (private->impl,
				     image,
				     src_x - x_offset,
				     src_y - y_offset,
				     dest_x, dest_y,
				     width, height);
}

static void
gdk_window_drop_cairo_surface (GdkWindowObject *private)
{
  if (private->cairo_surface)
    {
      cairo_surface_finish (private->cairo_surface);
      cairo_surface_set_user_data (private->cairo_surface, &gdk_window_cairo_key,
				   NULL, NULL);
    }
}

static void
gdk_window_cairo_surface_destroy (void *data)
{
  GdkWindowObject *private = (GdkWindowObject*) data;

  private->cairo_surface = NULL;
  private->impl_window->outstanding_surfaces--;
}

static cairo_surface_t *
gdk_window_create_cairo_surface (GdkDrawable *drawable,
				 int width,
				 int height)
{
  return _gdk_windowing_create_cairo_surface (GDK_WINDOW_OBJECT(drawable)->impl,
					      width, height);
}


static cairo_surface_t *
gdk_window_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkWindowObject *private = (GdkWindowObject*) drawable;
  cairo_surface_t *surface;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      surface = paint->surface;
      cairo_surface_reference (surface);
    }
  else
    {

      /* This will be drawing directly to the window, so flush implicit paint */
      gdk_window_flush ((GdkWindow *)drawable);

      if (!private->cairo_surface)
	{
	  int width, height;
	  GdkDrawable *source;

          gdk_drawable_get_size ((GdkWindow *) private->impl_window,
                                 &width, &height);

	  source = _gdk_drawable_get_source_drawable (drawable);

	  private->cairo_surface = _gdk_drawable_create_cairo_surface (source, width, height);

	  if (private->cairo_surface)
	    {
	      private->impl_window->outstanding_surfaces++;

	      cairo_surface_set_device_offset (private->cairo_surface,
					       private->abs_x,
					       private->abs_y);

	      cairo_surface_set_user_data (private->cairo_surface, &gdk_window_cairo_key,
					   drawable, gdk_window_cairo_surface_destroy);
	    }
	}
      else
	cairo_surface_reference (private->cairo_surface);

      surface = private->cairo_surface;
    }

  return surface;
}

static void
gdk_window_set_cairo_clip (GdkDrawable *drawable,
			   cairo_t *cr)
{
  GdkWindowObject *private = (GdkWindowObject*) drawable;

  if (!private->paint_stack)
    {
      cairo_reset_clip (cr);

      cairo_save (cr);
      cairo_identity_matrix (cr);

      cairo_new_path (cr);
      gdk_cairo_region (cr, private->clip_region_with_children);

      cairo_restore (cr);
      cairo_clip (cr);
    }
  else
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      /* Only needs to clip to region if piggybacking
	 on an implicit paint pixmap */
      cairo_reset_clip (cr);
      if (paint->uses_implicit)
	{
	  cairo_save (cr);
	  cairo_identity_matrix (cr);

	  cairo_new_path (cr);
	  gdk_cairo_region (cr, paint->region);
	  cairo_restore (cr);

	  cairo_clip (cr);
	}
    }
}

/* Code for dirty-region queueing
 */
static GSList *update_windows = NULL;
static guint update_idle = 0;
static gboolean debug_updates = FALSE;

static inline gboolean
gdk_window_is_ancestor (GdkWindow *window,
			GdkWindow *ancestor)
{
  while (window)
    {
      GdkWindow *parent = (GdkWindow*) ((GdkWindowObject*) window)->parent;

      if (parent == ancestor)
	return TRUE;

      window = parent;
    }

  return FALSE;
}

static void
gdk_window_add_update_window (GdkWindow *window)
{
  GSList *tmp;
  GSList *prev = NULL;
  gboolean has_ancestor_in_list = FALSE;

  /*  Check whether "window" is already in "update_windows" list.
   *  It could be added during execution of gtk_widget_destroy() when
   *  setting focus widget to NULL and redrawing old focus widget.
   *  See bug 711552.
   */
  tmp = g_slist_find (update_windows, window);
  if (tmp != NULL)
    return;

  for (tmp = update_windows; tmp; tmp = tmp->next)
    {
      GdkWindowObject *parent = GDK_WINDOW_OBJECT (window)->parent;

      /*  check if tmp is an ancestor of "window"; if it is, set a
       *  flag indicating that all following windows are either
       *  children of "window" or from a differen hierarchy
       */
      if (!has_ancestor_in_list && gdk_window_is_ancestor (window, tmp->data))
	has_ancestor_in_list = TRUE;

      /* insert in reverse stacking order when adding around siblings,
       * so processing updates properly paints over lower stacked windows
       */
      if (parent == GDK_WINDOW_OBJECT (tmp->data)->parent)
	{
	  gint index = g_list_index (parent->children, window);
	  for (; tmp && parent == GDK_WINDOW_OBJECT (tmp->data)->parent; tmp = tmp->next)
	    {
	      gint sibling_index = g_list_index (parent->children, tmp->data);
	      if (index > sibling_index)
		break;
	      prev = tmp;
	    }
	  /* here, tmp got advanced past all lower stacked siblings */
	  tmp = g_slist_prepend (tmp, g_object_ref (window));
	  if (prev)
	    prev->next = tmp;
	  else
	    update_windows = tmp;
	  return;
	}

      /*  if "window" has an ancestor in the list and tmp is one of
       *  "window's" children, insert "window" before tmp
       */
      if (has_ancestor_in_list && gdk_window_is_ancestor (tmp->data, window))
	{
	  tmp = g_slist_prepend (tmp, g_object_ref (window));

	  if (prev)
	    prev->next = tmp;
	  else
	    update_windows = tmp;
	  return;
	}

      /*  if we're at the end of the list and had an ancestor it it,
       *  append to the list
       */
      if (! tmp->next && has_ancestor_in_list)
	{
	  tmp = g_slist_append (tmp, g_object_ref (window));
	  return;
	}

      prev = tmp;
    }

  /*  if all above checks failed ("window" is from a different
   *  hierarchy than what is already in the list) or the list is
   *  empty, prepend
   */
  update_windows = g_slist_prepend (update_windows, g_object_ref (window));
}

static void
gdk_window_remove_update_window (GdkWindow *window)
{
  GSList *link;

  link = g_slist_find (update_windows, window);
  if (link != NULL)
    {
      update_windows = g_slist_delete_link (update_windows, link);
      g_object_unref (window);
    }
}

static gboolean
gdk_window_update_idle (gpointer data)
{
  gdk_window_process_all_updates ();

  return FALSE;
}

static gboolean
gdk_window_is_toplevel_frozen (GdkWindow *window)
{
  GdkWindowObject *toplevel;

  toplevel = (GdkWindowObject *)gdk_window_get_toplevel (window);

  return toplevel->update_and_descendants_freeze_count > 0;
}

static void
gdk_window_schedule_update (GdkWindow *window)
{
  if (window &&
      (GDK_WINDOW_OBJECT (window)->update_freeze_count ||
       gdk_window_is_toplevel_frozen (window)))
    return;

  if (!update_idle)
    update_idle =
      gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW,
				 gdk_window_update_idle,
				 NULL, NULL);
}

void
_gdk_window_process_updates_recurse (GdkWindow *window,
				     GdkRegion *expose_region)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *child;
  GdkRegion *child_region;
  GdkRectangle r;
  GList *l, *children;

  if (gdk_region_empty (expose_region))
    return;

  /* Make this reentrancy safe for expose handlers freeing windows */
  children = g_list_copy (private->children);
  g_list_foreach (children, (GFunc)g_object_ref, NULL);

  /* Iterate over children, starting at topmost */
  for (l = children; l != NULL; l = l->next)
    {
      child = l->data;

      if (child->destroyed || !GDK_WINDOW_IS_MAPPED (child) || child->input_only || child->composited)
	continue;

      /* Ignore offscreen children, as they don't draw in their parent and
       * don't take part in the clipping */
      if (gdk_window_is_offscreen (child))
	continue;

      r.x = child->x;
      r.y = child->y;
      r.width = child->width;
      r.height = child->height;

      child_region = gdk_region_rectangle (&r);
      if (child->shape)
	{
	  /* Adjust shape region to parent window coords */
	  gdk_region_offset (child->shape, child->x, child->y);
	  gdk_region_intersect (child_region, child->shape);
	  gdk_region_offset (child->shape, -child->x, -child->y);
	}

      if (child->impl == private->impl)
	{
	  /* Client side child, expose */
	  gdk_region_intersect (child_region, expose_region);
	  gdk_region_subtract (expose_region, child_region);
	  gdk_region_offset (child_region, -child->x, -child->y);
	  _gdk_window_process_updates_recurse ((GdkWindow *)child, child_region);
	}
      else
	{
	  /* Native child, just remove area from expose region */
	  gdk_region_subtract (expose_region, child_region);
	}
      gdk_region_destroy (child_region);
    }

  g_list_foreach (children, (GFunc)g_object_unref, NULL);
  g_list_free (children);

  if (!gdk_region_empty (expose_region) &&
      !private->destroyed)
    {
      if (private->event_mask & GDK_EXPOSURE_MASK)
	{
	  GdkEvent event;

#ifdef __APPLE__
	  /* we no longer double-buffer on macOS/quartz, but double-buffering has seeped
	     into GTK sufficiently that several widgets rely on the background paint that
	     gdk_window_begin_paint_region() would do. So here we sort-of-explicitly paint
	     the window directly to provide the same starting point for the window drawing
	     that will take place as the expose is processed.
	  */
	  gdk_window_clear_region_internal (window, expose_region, FALSE);
#endif
	  event.expose.type = GDK_EXPOSE;
	  event.expose.window = g_object_ref (window);
	  event.expose.send_event = FALSE;
	  event.expose.count = 0;
	  event.expose.region = expose_region;
	  gdk_region_get_clipbox (expose_region, &event.expose.area);

	  (*_gdk_event_func) (&event, _gdk_event_data);

	  g_object_unref (window);
	}
      else if (private->bg_pixmap != GDK_NO_BG &&
	       private->window_type != GDK_WINDOW_FOREIGN)
	{
	  /* No exposure mask set, so nothing will be drawn, the
	   * app relies on the background being what it specified
	   * for the window. So, we need to clear this manually.
	   *
	   * For foreign windows if expose is not set that generally
	   * means some other client paints them, so don't clear
	   * there.
	   *
	   * We use begin/end_paint around the clear so that we can
	   * piggyback on the implicit paint */

	  gdk_window_begin_paint_region (window, expose_region);
	  gdk_window_clear_region_internal (window, expose_region, FALSE);
	  gdk_window_end_paint (window);
	}
    }
}

/* Process and remove any invalid area on the native window by creating
 * expose events for the window and all non-native descendants.
 * Also processes any outstanding moves on the window before doing
 * any drawing. Note that its possible to have outstanding moves without
 * any invalid area as we use the update idle mechanism to coalesce
 * multiple moves as well as multiple invalidations.
 */
static void
gdk_window_process_updates_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplIface *impl_iface;
  gboolean save_region = FALSE;
  GdkRectangle clip_box;
  int iteration;

  /* Ensure the window lives while updating it */
  g_object_ref (window);

  /* If an update got queued during update processing, we can get a
   * window in the update queue that has an empty update_area.
   * just ignore it.
   *
   * We run this multiple times if needed because on win32 the
   * first run can cause new (synchronous) updates from
   * gdk_window_flush_outstanding_moves(). However, we
   * limit it to two iterations to avoid any potential loops.
   */
  iteration = 0;
  while (private->update_area &&
	 iteration++ < 2)
    {
      GdkRegion *update_area = private->update_area;
      private->update_area = NULL;

      if (_gdk_event_func && gdk_window_is_viewable (window))
	{
	  GdkRegion *expose_region;
	  gboolean end_implicit;

	  /* Clip to part visible in toplevel */
	  gdk_region_intersect (update_area, private->clip_region);

	  if (debug_updates)
	    {
	      /* Make sure we see the red invalid area before redrawing. */
	      gdk_display_sync (gdk_drawable_get_display (window));
	      g_usleep (70000);
	    }

	  /* At this point we will be completely redrawing all of update_area.
	   * If we have any outstanding moves that end up moving stuff inside
	   * this area we don't actually need to move that as that part would
	   * be overdrawn by the expose anyway. So, in order to copy less data
	   * we remove these areas from the outstanding moves.
	   */
	  if (private->outstanding_moves)
	    {
	      GdkWindowRegionMove *move;
	      GdkRegion *remove;
	      GList *l, *prev;

	      remove = gdk_region_copy (update_area);
	      /* We iterate backwards, starting from the state that would be
		 if we had applied all the moves. */
	      for (l = g_list_last (private->outstanding_moves); l != NULL; l = prev)
		{
		  prev = l->prev;
		  move = l->data;

		  /* Don't need this area */
		  gdk_region_subtract (move->dest_region, remove);

		  /* However if any of the destination we do need has a source
		     in the updated region we do need that as a destination for
		     the earlier moves */
		  gdk_region_offset (move->dest_region, -move->dx, -move->dy);
		  gdk_region_subtract (remove, move->dest_region);

		  if (gdk_region_empty (move->dest_region))
		    {
		      gdk_window_region_move_free (move);
		      private->outstanding_moves =
			g_list_delete_link (private->outstanding_moves, l);
		    }
		  else /* move back */
		    gdk_region_offset (move->dest_region, move->dx, move->dy);
		}
	      gdk_region_destroy (remove);
	    }

	  /* By now we a set of window moves that should be applied, and then
	   * an update region that should be repainted. A trivial implementation
	   * would just do that in order, however in order to get nicer drawing
	   * we do some tricks:
	   *
	   * First of all, each subwindow expose may be double buffered by
	   * itself (depending on widget setting) via
	   * gdk_window_begin/end_paint(). But we also do an "implicit" paint,
	   * creating a single pixmap the size of the invalid area on the
	   * native window which all the individual normal paints will draw
	   * into. This way in the normal case there will be only one pixmap
	   * allocated and only once pixmap draw done for all the windows
	   * in this native window.
	   * There are a couple of reasons this may fail, for instance, some
	   * backends (like quartz) do its own double buffering, so we disable
	   * gdk double buffering there. Secondly, some subwindow could be
	   * non-double buffered and draw directly to the window outside a
	   * begin/end_paint pair. That will be lead to a gdk_window_flush
	   * which immediately executes all outstanding moves and paints+removes
	   * the implicit paint (further paints will allocate their own pixmap).
	   *
	   * Secondly, in the case of implicit double buffering we expose all
	   * the child windows into the implicit pixmap before we execute
	   * the outstanding moves. This way we minimize the time between
	   * doing the moves and rendering the new update area, thus minimizing
	   * flashing. Of course, if any subwindow is non-double buffered we
	   * well flush earlier than that.
	   *
	   * Thirdly, after having done the outstanding moves we queue an
	   * "antiexpose" on the area that will be drawn by the expose, which
	   * means that any invalid region on the native window side before
	   * the first expose drawing operation will be discarded, as it
	   * has by then been overdrawn with valid data. This means we can
	   * avoid doing the unnecessary repaint any outstanding expose events.
	   */

	  gdk_region_get_clipbox (update_area, &clip_box);
	  end_implicit = gdk_window_begin_implicit_paint (window, &clip_box);
	  expose_region = gdk_region_copy (update_area);
	  if (!end_implicit)
	    {
	      /* Rendering is not double buffered by gdk, do outstanding
	       * moves and queue antiexposure immediately. No need to do
	       * any tricks */
	      gdk_window_flush_outstanding_moves (window);
	      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
	      save_region = impl_iface->queue_antiexpose (window, update_area);
	    }

	  /* Render the invalid areas to the implicit paint, by sending exposes.
	   * May flush if non-double buffered widget draw. */
	  _gdk_windowing_window_process_updates_recurse (window, expose_region);

	  if (end_implicit)
	    {
	      /* Do moves right before exposes are rendered to the window */
	      gdk_window_flush_outstanding_moves (window);

	      /* By this time we know that any outstanding expose for this
	       * area is invalid and we can avoid it, so queue an antiexpose.
	       * However, it may be that due to an non-double buffered expose
	       * we have already started drawing to the window, so it would
	       * be to late to anti-expose now. Since this is merely an
	       * optimization we just avoid doing it at all in that case.
	       */
	      if (private->implicit_paint != NULL &&
		  !private->implicit_paint->flushed)
		{
		  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
		  save_region = impl_iface->queue_antiexpose (window, update_area);
		}

	      gdk_window_end_implicit_paint (window);
	    }
	  gdk_region_destroy (expose_region);
	}
      if (!save_region)
	gdk_region_destroy (update_area);
    }

  if (private->outstanding_moves)
    {
      /* Flush any outstanding moves, may happen if we moved a window but got
	 no actual invalid area */
      gdk_window_flush_outstanding_moves (window);
    }

  g_object_unref (window);
}

static void
flush_all_displays (void)
{
  GSList *displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  GSList *tmp_list;

  for (tmp_list = displays; tmp_list; tmp_list = tmp_list->next)
    gdk_display_flush (tmp_list->data);

  g_slist_free (displays);
}

/* Currently it is not possible to override
 * gdk_window_process_all_updates in the same manner as
 * gdk_window_process_updates and gdk_window_invalidate_maybe_recurse
 * by implementing the GdkPaintable interface.  If in the future a
 * backend would need this, the right solution would be to add a
 * method to GdkDisplay that can be optionally
 * NULL. gdk_window_process_all_updates can then walk the list of open
 * displays and call the mehod.
 */

/**
 * gdk_window_process_all_updates:
 *
 * Calls gdk_window_process_updates() for all windows (see #GdkWindow)
 * in the application.
 *
 **/
void
gdk_window_process_all_updates (void)
{
  GSList *old_update_windows = update_windows;
  GSList *tmp_list = update_windows;
  static gboolean in_process_all_updates = FALSE;
  static gboolean got_recursive_update = FALSE;

  if (in_process_all_updates)
    {
      /* We can't do this now since that would recurse, so
	 delay it until after the recursion is done. */
      got_recursive_update = TRUE;
      update_idle = 0;
      return;
    }

  in_process_all_updates = TRUE;
  got_recursive_update = FALSE;

  if (update_idle)
    g_source_remove (update_idle);

  update_windows = NULL;
  update_idle = 0;

  _gdk_windowing_before_process_all_updates ();

  while (tmp_list)
    {
      GdkWindowObject *private = (GdkWindowObject *)tmp_list->data;

      if (!GDK_WINDOW_DESTROYED (tmp_list->data))
	{
	  if (private->update_freeze_count ||
	      gdk_window_is_toplevel_frozen (tmp_list->data))
	    gdk_window_add_update_window ((GdkWindow *) private);
	  else
	    gdk_window_process_updates_internal (tmp_list->data);
	}

      g_object_unref (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  g_slist_free (old_update_windows);

  flush_all_displays ();

  _gdk_windowing_after_process_all_updates ();

  in_process_all_updates = FALSE;

  /* If we ignored a recursive call, schedule a
     redraw now so that it eventually happens,
     otherwise we could miss an update if nothing
     else schedules an update. */
  if (got_recursive_update && !update_idle)
    update_idle =
      gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW,
				 gdk_window_update_idle,
				 NULL, NULL);
}

/**
 * gdk_window_process_updates:
 * @window: a #GdkWindow
 * @update_children: whether to also process updates for child windows
 *
 * Sends one or more expose events to @window. The areas in each
 * expose event will cover the entire update area for the window (see
 * gdk_window_invalidate_region() for details). Normally GDK calls
 * gdk_window_process_all_updates() on your behalf, so there's no
 * need to call this function unless you want to force expose events
 * to be delivered immediately and synchronously (vs. the usual
 * case, where GDK delivers them in an idle handler). Occasionally
 * this is useful to produce nicer scrolling behavior, for example.
 *
 **/
void
gdk_window_process_updates (GdkWindow *window,
			    gboolean   update_children)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Make sure the window lives during the expose callouts */
  g_object_ref (window);

  impl_window = gdk_window_get_impl_window (private);
  if ((impl_window->update_area ||
       impl_window->outstanding_moves) &&
      !impl_window->update_freeze_count &&
      !gdk_window_is_toplevel_frozen (window) &&

      /* Don't recurse into process_updates_internal, we'll
       * do the update later when idle instead. */
      impl_window->implicit_paint == NULL)
    {
      gdk_window_process_updates_internal ((GdkWindow *)impl_window);
      gdk_window_remove_update_window ((GdkWindow *)impl_window);
    }

  if (update_children)
    {
      /* process updates in reverse stacking order so composition or
       * painting over achieves the desired effect for offscreen windows
       */
      GList *node, *children;

      children = g_list_copy (private->children);
      g_list_foreach (children, (GFunc)g_object_ref, NULL);

      for (node = g_list_last (children); node; node = node->prev)
	{
	  gdk_window_process_updates (node->data, TRUE);
	  g_object_unref (node->data);
	}

      g_list_free (children);
    }

  g_object_unref (window);
}

static void
gdk_window_invalidate_rect_full (GdkWindow          *window,
				  const GdkRectangle *rect,
				  gboolean            invalidate_children,
				  ClearBg             clear_bg)
{
  GdkRectangle window_rect;
  GdkRegion *region;
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (private->input_only || !private->viewable)
    return;

  if (!rect)
    {
      window_rect.x = 0;
      window_rect.y = 0;
      gdk_drawable_get_size (GDK_DRAWABLE (window),
			     &window_rect.width,
			     &window_rect.height);
      rect = &window_rect;
    }

  region = gdk_region_rectangle (rect);
  gdk_window_invalidate_region_full (window, region, invalidate_children, clear_bg);
  gdk_region_destroy (region);
}

/**
 * gdk_window_invalidate_rect:
 * @window: a #GdkWindow
 * @rect: (allow-none): rectangle to invalidate or %NULL to invalidate the whole
 *      window
 * @invalidate_children: whether to also invalidate child windows
 *
 * A convenience wrapper around gdk_window_invalidate_region() which
 * invalidates a rectangular region. See
 * gdk_window_invalidate_region() for details.
 **/
void
gdk_window_invalidate_rect (GdkWindow          *window,
			    const GdkRectangle *rect,
			    gboolean            invalidate_children)
{
  gdk_window_invalidate_rect_full (window, rect, invalidate_children, CLEAR_BG_NONE);
}

static void
draw_ugly_color (GdkWindow       *window,
		 const GdkRegion *region)
{
  /* Draw ugly color all over the newly-invalid region */
  GdkColor ugly_color = { 0, 50000, 10000, 10000 };
  GdkGC *ugly_gc;
  GdkRectangle clipbox;

  ugly_gc = gdk_gc_new (window);
  gdk_gc_set_rgb_fg_color (ugly_gc, &ugly_color);
  gdk_gc_set_clip_region (ugly_gc, region);

  gdk_region_get_clipbox (region, &clipbox);

  gdk_draw_rectangle (window,
		      ugly_gc,
		      TRUE,
		      clipbox.x, clipbox.y,
		      clipbox.width, clipbox.height);

  g_object_unref (ugly_gc);
}

static void
impl_window_add_update_area (GdkWindowObject *impl_window,
			     GdkRegion *region)
{
  if (impl_window->update_area)
    gdk_region_union (impl_window->update_area, region);
  else
    {
      gdk_window_add_update_window ((GdkWindow *)impl_window);
      impl_window->update_area = gdk_region_copy (region);
      gdk_window_schedule_update ((GdkWindow *)impl_window);
    }
}

/* clear_bg controls if the region will be cleared to
 * the background color/pixmap if the exposure mask is not
 * set for the window, whereas this might not otherwise be
 * done (unless necessary to emulate background settings).
 * Set this to CLEAR_BG_WINCLEARED or CLEAR_BG_ALL if you
 * need to clear the background, such as when exposing the area beneath a
 * hidden or moved window, but not when an app requests repaint or when the
 * windowing system exposes a newly visible area (because then the windowing
 * system has already cleared the area).
 */
static void
gdk_window_invalidate_maybe_recurse_full (GdkWindow       *window,
					  const GdkRegion *region,
					  ClearBg          clear_bg,
					  gboolean       (*child_func) (GdkWindow *,
									gpointer),
					  gpointer   user_data)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;
  GdkRegion *visible_region;
  GList *tmp_list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (private->input_only ||
      !private->viewable ||
      gdk_region_empty (region) ||
      private->window_type == GDK_WINDOW_ROOT)
    return;

  visible_region = gdk_drawable_get_visible_region (window);
  gdk_region_intersect (visible_region, region);

  tmp_list = private->children;
  while (tmp_list)
    {
      GdkWindowObject *child = tmp_list->data;

      if (!child->input_only)
	{
	  GdkRegion *child_region;
	  GdkRectangle child_rect;

	  child_rect.x = child->x;
	  child_rect.y = child->y;
	  child_rect.width = child->width;
	  child_rect.height = child->height;
	  child_region = gdk_region_rectangle (&child_rect);

	  /* remove child area from the invalid area of the parent */
	  if (GDK_WINDOW_IS_MAPPED (child) && !child->shaped &&
	      !child->composited &&
	      !gdk_window_is_offscreen (child))
	    gdk_region_subtract (visible_region, child_region);

	  if (child_func && (*child_func) ((GdkWindow *)child, user_data))
	    {
	      GdkRegion *tmp = gdk_region_copy (region);

	      gdk_region_offset (tmp, - child_rect.x, - child_rect.y);
	      gdk_region_offset (child_region, - child_rect.x, - child_rect.y);
	      gdk_region_intersect (child_region, tmp);

	      gdk_window_invalidate_maybe_recurse_full ((GdkWindow *)child,
							child_region, clear_bg, child_func, user_data);

	      gdk_region_destroy (tmp);
	    }

	  gdk_region_destroy (child_region);
	}

      tmp_list = tmp_list->next;
    }

  impl_window = gdk_window_get_impl_window (private);

  if (!gdk_region_empty (visible_region)  ||
      /* Even if we're not exposing anything, make sure we process
	 idles for windows with outstanding moves */
      (impl_window->outstanding_moves != NULL &&
       impl_window->update_area == NULL))
    {
      if (debug_updates)
	draw_ugly_color (window, region);

      /* Convert to impl coords */
      gdk_region_offset (visible_region, private->abs_x, private->abs_y);

      /* Only invalidate area if app requested expose events or if
	 we need to clear the area (by request or to emulate background
	 clearing for non-native windows or native windows with no support
	 for window backgrounds */
      if (private->event_mask & GDK_EXPOSURE_MASK ||
	  clear_bg == CLEAR_BG_ALL ||
	  (clear_bg == CLEAR_BG_WINCLEARED &&
	   (!clears_as_native (private) ||
	    !GDK_WINDOW_IMPL_GET_IFACE (private->impl)->supports_native_bg)))
	impl_window_add_update_area (impl_window, visible_region);
    }

  gdk_region_destroy (visible_region);
}

/**
 * gdk_window_invalidate_maybe_recurse:
 * @window: a #GdkWindow
 * @region: a #GdkRegion
 * @child_func: function to use to decide if to recurse to a child,
 *              %NULL means never recurse.
 * @user_data: data passed to @child_func
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * The @child_func parameter controls whether the region of
 * each child window that intersects @region will also be invalidated.
 * Only children for which @child_func returns TRUE will have the area
 * invalidated.
 **/
void
gdk_window_invalidate_maybe_recurse (GdkWindow       *window,
				     const GdkRegion *region,
				     gboolean       (*child_func) (GdkWindow *,
								   gpointer),
				     gpointer   user_data)
{
  gdk_window_invalidate_maybe_recurse_full (window, region, CLEAR_BG_NONE,
					    child_func, user_data);
}

static gboolean
true_predicate (GdkWindow *window,
		gpointer   user_data)
{
  return TRUE;
}

static void
gdk_window_invalidate_region_full (GdkWindow       *window,
				    const GdkRegion *region,
				    gboolean         invalidate_children,
				    ClearBg          clear_bg)
{
  gdk_window_invalidate_maybe_recurse_full (window, region, clear_bg,
					    invalidate_children ?
					    true_predicate : (gboolean (*) (GdkWindow *, gpointer))NULL,
				       NULL);
}

/**
 * gdk_window_invalidate_region:
 * @window: a #GdkWindow
 * @region: a #GdkRegion
 * @invalidate_children: %TRUE to also invalidate child windows
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * The @invalidate_children parameter controls whether the region of
 * each child window that intersects @region will also be invalidated.
 * If %FALSE, then the update area for child windows will remain
 * unaffected. See gdk_window_invalidate_maybe_recurse if you need
 * fine grained control over which children are invalidated.
 **/
void
gdk_window_invalidate_region (GdkWindow       *window,
			      const GdkRegion *region,
			      gboolean         invalidate_children)
{
  gdk_window_invalidate_maybe_recurse (window, region,
				       invalidate_children ?
					 true_predicate : (gboolean (*) (GdkWindow *, gpointer))NULL,
				       NULL);
}

/**
 * _gdk_window_invalidate_for_expose:
 * @window: a #GdkWindow
 * @region: a #GdkRegion
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * This version of invalidation is used when you recieve expose events
 * from the native window system. It exposes the native window, plus
 * any non-native child windows (but not native child windows, as those would
 * have gotten their own expose events).
 **/
void
_gdk_window_invalidate_for_expose (GdkWindow       *window,
				   GdkRegion       *region)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowRegionMove *move;
  GdkRegion *move_region;
  GList *l;

  /* Any invalidations comming from the windowing system will
     be in areas that may be moved by outstanding moves,
     so we need to modify the expose region correspondingly,
     otherwise we would expose in the wrong place, as the
     outstanding moves will be copied before we draw the
     exposes. */
  for (l = private->outstanding_moves; l != NULL; l = l->next)
    {
      move = l->data;

      /* covert to move source region */
      move_region = gdk_region_copy (move->dest_region);
      gdk_region_offset (move_region, -move->dx, -move->dy);

      /* Move area of region that intersects with move source
	 by dx, dy of the move*/
      gdk_region_intersect (move_region, region);
      gdk_region_subtract (region, move_region);
      gdk_region_offset (move_region, move->dx, move->dy);
      gdk_region_union (region, move_region);

      gdk_region_destroy (move_region);
    }

  gdk_window_invalidate_maybe_recurse_full (window, region, CLEAR_BG_WINCLEARED,
					    (gboolean (*) (GdkWindow *, gpointer))gdk_window_has_no_impl,
					    NULL);
}


/**
 * gdk_window_get_update_area:
 * @window: a #GdkWindow
 *
 * Transfers ownership of the update area from @window to the caller
 * of the function. That is, after calling this function, @window will
 * no longer have an invalid/dirty region; the update area is removed
 * from @window and handed to you. If a window has no update area,
 * gdk_window_get_update_area() returns %NULL. You are responsible for
 * calling gdk_region_destroy() on the returned region if it's non-%NULL.
 *
 * Return value: the update area for @window
 **/
GdkRegion *
gdk_window_get_update_area (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;
  GdkRegion *tmp_region;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  impl_window = gdk_window_get_impl_window (private);

  if (impl_window->update_area)
    {
      tmp_region = gdk_region_copy (private->clip_region_with_children);
      /* Convert to impl coords */
      gdk_region_offset (tmp_region, private->abs_x, private->abs_y);
      gdk_region_intersect (tmp_region, impl_window->update_area);

      if (gdk_region_empty (tmp_region))
	{
	  gdk_region_destroy (tmp_region);
	  return NULL;
	}
      else
	{
	  gdk_region_subtract (impl_window->update_area, tmp_region);

	  if (gdk_region_empty (impl_window->update_area) &&
	      impl_window->outstanding_moves == NULL)
	    {
	      gdk_region_destroy (impl_window->update_area);
	      impl_window->update_area = NULL;

	      gdk_window_remove_update_window ((GdkWindow *)impl_window);
	    }

	  /* Convert from impl coords */
	  gdk_region_offset (tmp_region, -private->abs_x, -private->abs_y);
	  return tmp_region;

	}
    }
  else
    return NULL;
}

/**
 * _gdk_window_clear_update_area:
 * @window: a #GdkWindow.
 *
 * Internal function to clear the update area for a window. This
 * is called when the window is hidden or destroyed.
 **/
void
_gdk_window_clear_update_area (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->update_area)
    {
      gdk_window_remove_update_window (window);

      gdk_region_destroy (private->update_area);
      private->update_area = NULL;
    }
}

/**
 * gdk_window_freeze_updates:
 * @window: a #GdkWindow
 *
 * Temporarily freezes a window such that it won't receive expose
 * events.  The window will begin receiving expose events again when
 * gdk_window_thaw_updates() is called. If gdk_window_freeze_updates()
 * has been called more than once, gdk_window_thaw_updates() must be called
 * an equal number of times to begin processing exposes.
 **/
void
gdk_window_freeze_updates (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl_window = gdk_window_get_impl_window (private);
  impl_window->update_freeze_count++;
}

/**
 * gdk_window_thaw_updates:
 * @window: a #GdkWindow
 *
 * Thaws a window frozen with gdk_window_freeze_updates().
 **/
void
gdk_window_thaw_updates (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl_window = gdk_window_get_impl_window (private);

  g_return_if_fail (impl_window->update_freeze_count > 0);

  if (--impl_window->update_freeze_count == 0)
    gdk_window_schedule_update (GDK_WINDOW (impl_window));
}

/**
 * gdk_window_freeze_toplevel_updates_libgtk_only:
 * @window: a #GdkWindow
 *
 * Temporarily freezes a window and all its descendants such that it won't
 * receive expose events.  The window will begin receiving expose events
 * again when gdk_window_thaw_toplevel_updates_libgtk_only() is called. If
 * gdk_window_freeze_toplevel_updates_libgtk_only()
 * has been called more than once,
 * gdk_window_thaw_toplevel_updates_libgtk_only() must be called
 * an equal number of times to begin processing exposes.
 *
 * This function is not part of the GDK public API and is only
 * for use by GTK+.
 **/
void
gdk_window_freeze_toplevel_updates_libgtk_only (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->window_type != GDK_WINDOW_CHILD);

  private->update_and_descendants_freeze_count++;
}

/**
 * gdk_window_thaw_toplevel_updates_libgtk_only:
 * @window: a #GdkWindow
 *
 * Thaws a window frozen with
 * gdk_window_freeze_toplevel_updates_libgtk_only().
 *
 * This function is not part of the GDK public API and is only
 * for use by GTK+.
 **/
void
gdk_window_thaw_toplevel_updates_libgtk_only (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->window_type != GDK_WINDOW_CHILD);
  g_return_if_fail (private->update_and_descendants_freeze_count > 0);

  private->update_and_descendants_freeze_count--;

  gdk_window_schedule_update (window);
}

/**
 * gdk_window_set_debug_updates:
 * @setting: %TRUE to turn on update debugging
 *
 * With update debugging enabled, calls to
 * gdk_window_invalidate_region() clear the invalidated region of the
 * screen to a noticeable color, and GDK pauses for a short time
 * before sending exposes to windows during
 * gdk_window_process_updates().  The net effect is that you can see
 * the invalid region for each window and watch redraws as they
 * occur. This allows you to diagnose inefficiencies in your application.
 *
 * In essence, because the GDK rendering model prevents all flicker,
 * if you are redrawing the same region 400 times you may never
 * notice, aside from noticing a speed problem. Enabling update
 * debugging causes GTK to flicker slowly and noticeably, so you can
 * see exactly what's being redrawn when, in what order.
 *
 * The --gtk-debug=updates command line option passed to GTK+ programs
 * enables this debug option at application startup time. That's
 * usually more useful than calling gdk_window_set_debug_updates()
 * yourself, though you might want to use this function to enable
 * updates sometime after application startup time.
 *
 **/
void
gdk_window_set_debug_updates (gboolean setting)
{
  debug_updates = setting;
}

/**
 * gdk_window_constrain_size:
 * @geometry: a #GdkGeometry structure
 * @flags: a mask indicating what portions of @geometry are set
 * @width: desired width of window
 * @height: desired height of the window
 * @new_width: (out): location to store resulting width
 * @new_height: (out): location to store resulting height
 *
 * Constrains a desired width and height according to a
 * set of geometry hints (such as minimum and maximum size).
 */
void
gdk_window_constrain_size (GdkGeometry *geometry,
			   guint        flags,
			   gint         width,
			   gint         height,
			   gint        *new_width,
			   gint        *new_height)
{
  /* This routine is partially borrowed from fvwm.
   *
   * Copyright 1993, Robert Nation
   *     You may use this code for any purpose, as long as the original
   *     copyright remains in the source code and all documentation
   *
   * which in turn borrows parts of the algorithm from uwm
   */
  gint min_width = 0;
  gint min_height = 0;
  gint base_width = 0;
  gint base_height = 0;
  gint xinc = 1;
  gint yinc = 1;
  gint max_width = G_MAXINT;
  gint max_height = G_MAXINT;

#define FLOOR(value, base)	( ((gint) ((value) / (base))) * (base) )

  if ((flags & GDK_HINT_BASE_SIZE) && (flags & GDK_HINT_MIN_SIZE))
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }
  else if (flags & GDK_HINT_BASE_SIZE)
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->base_width;
      min_height = geometry->base_height;
    }
  else if (flags & GDK_HINT_MIN_SIZE)
    {
      base_width = geometry->min_width;
      base_height = geometry->min_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }

  if (flags & GDK_HINT_MAX_SIZE)
    {
      max_width = geometry->max_width ;
      max_height = geometry->max_height;
    }

  if (flags & GDK_HINT_RESIZE_INC)
    {
      xinc = MAX (xinc, geometry->width_inc);
      yinc = MAX (yinc, geometry->height_inc);
    }

  /* clamp width and height to min and max values
   */
  width = CLAMP (width, min_width, max_width);
  height = CLAMP (height, min_height, max_height);

  /* shrink to base + N * inc
   */
  width = base_width + FLOOR (width - base_width, xinc);
  height = base_height + FLOOR (height - base_height, yinc);

  /* constrain aspect ratio, according to:
   *
   *                width
   * min_aspect <= -------- <= max_aspect
   *                height
   */

  if (flags & GDK_HINT_ASPECT &&
      geometry->min_aspect > 0 &&
      geometry->max_aspect > 0)
    {
      gint delta;

      if (geometry->min_aspect * height > width)
	{
	  delta = FLOOR (height - width / geometry->min_aspect, yinc);
	  if (height - delta >= min_height)
	    height -= delta;
	  else
	    {
	      delta = FLOOR (height * geometry->min_aspect - width, xinc);
	      if (width + delta <= max_width)
		width += delta;
	    }
	}

      if (geometry->max_aspect * height < width)
	{
	  delta = FLOOR (width - height * geometry->max_aspect, xinc);
	  if (width - delta >= min_width)
	    width -= delta;
	  else
	    {
	      delta = FLOOR (width / geometry->max_aspect - height, yinc);
	      if (height + delta <= max_height)
		height += delta;
	    }
	}
    }

#undef FLOOR

  *new_width = width;
  *new_height = height;
}

/**
 * gdk_window_get_pointer:
 * @window: a #GdkWindow
 * @x: (out) (allow-none): return location for X coordinate of pointer or %NULL to not
 *      return the X coordinate
 * @y: (out) (allow-none):  return location for Y coordinate of pointer or %NULL to not
 *      return the Y coordinate
 * @mask: (out) (allow-none): return location for modifier mask or %NULL to not return the
 *      modifier mask
 *
 * Obtains the current pointer position and modifier state.
 * The position is given in coordinates relative to the upper left
 * corner of @window.
 *
 * Return value: (transfer none): the window containing the pointer (as with
 * gdk_window_at_pointer()), or %NULL if the window containing the
 * pointer isn't known to GDK
 **/
GdkWindow*
gdk_window_get_pointer (GdkWindow	  *window,
			gint		  *x,
			gint		  *y,
			GdkModifierType   *mask)
{
  GdkDisplay *display;
  gint tmp_x, tmp_y;
  GdkModifierType tmp_mask;
  GdkWindow *child;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);

  if (window)
    {
      display = gdk_drawable_get_display (window);
    }
  else
    {
      GdkScreen *screen = gdk_screen_get_default ();

      display = gdk_screen_get_display (screen);
      window = gdk_screen_get_root_window (screen);

      GDK_NOTE (MULTIHEAD,
		g_message ("Passing NULL for window to gdk_window_get_pointer()\n"
			   "is not multihead safe"));
    }

  child = display->pointer_hooks->window_get_pointer (display, window, &tmp_x, &tmp_y, &tmp_mask);

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;

  _gdk_display_enable_motion_hints (display);

  return child;
}

/**
 * gdk_window_at_pointer:
 * @win_x: (out) (allow-none): return location for origin of the window under the pointer
 * @win_y: (out) (allow-none): return location for origin of the window under the pointer
 *
 * Obtains the window underneath the mouse pointer, returning the
 * location of that window in @win_x, @win_y. Returns %NULL if the
 * window under the mouse pointer is not known to GDK (if the window
 * belongs to another application and a #GdkWindow hasn't been created
 * for it with gdk_window_foreign_new())
 *
 * NOTE: For multihead-aware widgets or applications use
 * gdk_display_get_window_at_pointer() instead.
 *
 * Return value: (transfer none): window under the mouse pointer
 **/
GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  return gdk_display_get_window_at_pointer (gdk_display_get_default (), win_x, win_y);
}

/**
 * gdk_get_default_root_window:
 *
 * Obtains the root window (parent all other windows are inside)
 * for the default display and screen.
 *
 * Return value: the default root window
 **/
GdkWindow *
gdk_get_default_root_window (void)
{
  return gdk_screen_get_root_window (gdk_screen_get_default ());
}

/**
 * gdk_window_foreign_new:
 * @anid: a native window handle.
 *
 * Wraps a native window for the default display in a #GdkWindow.
 * This may fail if the window has been destroyed.
 *
 * For example in the X backend, a native window handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the newly-created #GdkWindow wrapper for the
 *    native window or %NULL if the window has been destroyed.
 **/
GdkWindow *
gdk_window_foreign_new (GdkNativeWindow anid)
{
  return gdk_window_foreign_new_for_display (gdk_display_get_default (), anid);
}

static void
get_all_native_children (GdkWindowObject *private,
			 GList **native)
{
  GdkWindowObject *child;
  GList *l;

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (gdk_window_has_impl (child))
	*native = g_list_prepend (*native, child);
      else
	get_all_native_children (child, native);
    }
}


static inline void
gdk_window_raise_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *parent = private->parent;
  GdkWindowObject *above;
  GList *native_children;
  GList *l, listhead;
  GdkWindowImplIface *impl_iface;

  if (parent)
    {
      parent->children = g_list_remove (parent->children, window);
      parent->children = g_list_prepend (parent->children, window);
    }

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  /* Just do native raise for toplevels */
  if (gdk_window_is_toplevel (private) ||
      /* The restack_under codepath should work correctly even if the parent
	 is native, but it relies on the order of ->children to be correct,
	 and some apps like SWT reorder the x windows without gdks knowledge,
	 so we use raise directly in order to make these behave as before
	 when using native windows */
      (gdk_window_has_impl (private) && gdk_window_has_impl (parent)))
    {
      impl_iface->raise (window);
    }
  else if (gdk_window_has_impl (private))
    {
      above = find_native_sibling_above (parent, private);
      if (above)
	{
	  listhead.data = window;
	  listhead.next = NULL;
	  listhead.prev = NULL;
	  impl_iface->restack_under ((GdkWindow *)above,
				     &listhead);
	}
      else
	impl_iface->raise (window);
    }
  else
    {
      native_children = NULL;
      get_all_native_children (private, &native_children);
      if (native_children != NULL)
	{
	  above = find_native_sibling_above (parent, private);

	  if (above)
	    impl_iface->restack_under ((GdkWindow *)above,
				       native_children);
	  else
	    {
	      /* Right order, since native_children is bottom-topmost first */
	      for (l = native_children; l != NULL; l = l->next)
		impl_iface->raise (l->data);
	    }

	  g_list_free (native_children);
	}

    }
}

/* Returns TRUE If the native window was mapped or unmapped */
static gboolean
set_viewable (GdkWindowObject *w,
	      gboolean val)
{
  GdkWindowObject *child;
  GdkWindowImplIface *impl_iface;
  GList *l;

  if (w->viewable == val)
    return FALSE;

  w->viewable = val;

  if (val)
    recompute_visible_regions (w, FALSE, FALSE);

  for (l = w->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (GDK_WINDOW_IS_MAPPED (child) &&
	  child->window_type != GDK_WINDOW_FOREIGN)
	set_viewable (child, val);
    }

  if (!_gdk_native_windows &&
      gdk_window_has_impl (w)  &&
      w->window_type != GDK_WINDOW_FOREIGN &&
      !gdk_window_is_toplevel (w))
    {
      /* For most native windows we show/hide them not when they are
       * mapped/unmapped, because that may not produce the correct results.
       * For instance, if a native window have a non-native parent which is
       * hidden, but its native parent is viewable then showing the window
       * would make it viewable to X but its not viewable wrt the non-native
       * hierarchy. In order to handle this we track the gdk side viewability
       * and only map really viewable windows.
       *
       * There are two exceptions though:
       *
       * For foreign windows we don't want ever change the mapped state
       * except when explicitly done via gdk_window_show/hide, as this may
       * cause problems for client owning the foreign window when its window
       * is suddenly mapped or unmapped.
       *
       * For toplevel windows embedded in a foreign window (e.g. a plug)
       * we sometimes synthesize a map of a window, but the native
       * window is really shown by the embedder, so we don't want to
       * do the show ourselves. We can't really tell this case from the normal
       * toplevel show as such toplevels are seen by gdk as parents of the
       * root window, so we make an exception for all toplevels.
       *
       * Also, when in GDK_NATIVE_WINDOW mode we never need to play games
       * like this, so we just always show/hide directly.
       */

      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (w->impl);
      if (val)
	impl_iface->show ((GdkWindow *)w, FALSE);
      else
	impl_iface->hide ((GdkWindow *)w);

      return TRUE;
    }

  return FALSE;
}

/* Returns TRUE If the native window was mapped or unmapped */
gboolean
_gdk_window_update_viewable (GdkWindow *window)
{
  GdkWindowObject *priv = (GdkWindowObject *)window;
  gboolean viewable;

  if (priv->window_type == GDK_WINDOW_FOREIGN ||
      priv->window_type == GDK_WINDOW_ROOT)
    viewable = TRUE;
  else if (gdk_window_is_toplevel (priv) ||
	   priv->parent->viewable)
    viewable = GDK_WINDOW_IS_MAPPED (priv);
  else
    viewable = FALSE;

  return set_viewable (priv, viewable);
}

static void
gdk_window_show_internal (GdkWindow *window, gboolean raise)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  gboolean was_mapped, was_viewable;
  gboolean did_show;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);
  was_viewable = private->viewable;

  if (raise)
    /* Keep children in (reverse) stacking order */
    gdk_window_raise_internal (window);

  if (gdk_window_has_impl (private))
    {
      if (!was_mapped)
	gdk_synthesize_window_state (window,
				     GDK_WINDOW_STATE_WITHDRAWN,
				     0);
    }
  else
    {
      private->state = 0;
    }

  did_show = _gdk_window_update_viewable (window);

  /* If it was already viewable the backend show op won't be called, call it
     again to ensure things happen right if the mapped tracking was not right
     for e.g. a foreign window.
     Dunno if this is strictly needed but its what happened pre-csw.
     Also show if not done by gdk_window_update_viewable. */
  if (gdk_window_has_impl (private) && (was_viewable || !did_show))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->show ((GdkWindow *)private,
			!did_show ? was_mapped : TRUE);
    }

  if (!was_mapped && !gdk_window_has_impl (private))
    {
      if (private->event_mask & GDK_STRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_MAP, NULL, FALSE);

      if (private->parent && private->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_MAP, NULL, FALSE);
    }

  if (!was_mapped || raise)
    {
      recompute_visible_regions (private, TRUE, FALSE);

      /* If any decendants became visible we need to send visibility notify */
      gdk_window_update_visibility_recursively (private, NULL);

      if (gdk_window_is_viewable (window))
	{
	  _gdk_synthesize_crossing_events_for_geometry_change (window);
	  gdk_window_invalidate_rect_full (window, NULL, TRUE, CLEAR_BG_ALL);
	}
    }
}

/**
 * gdk_window_show_unraised:
 * @window: a #GdkWindow
 *
 * Shows a #GdkWindow onscreen, but does not modify its stacking
 * order. In contrast, gdk_window_show() will raise the window
 * to the top of the window stack.
 *
 * On the X11 platform, in Xlib terms, this function calls
 * XMapWindow() (it also updates some internal GDK state, which means
 * that you can't really use XMapWindow() directly on a GDK window).
 */
void
gdk_window_show_unraised (GdkWindow *window)
{
  gdk_window_show_internal (window, FALSE);
}

/**
 * gdk_window_raise:
 * @window: a #GdkWindow
 *
 * Raises @window to the top of the Z-order (stacking order), so that
 * other windows with the same parent window appear below @window.
 * This is true whether or not the windows are visible.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_raise() only
 * requests the restack, does not guarantee it.
 */
void
gdk_window_raise (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkRegion *old_region, *new_region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  gdk_window_flush_if_exposing (window);

  old_region = NULL;
  if (gdk_window_is_viewable (window) &&
      !private->input_only)
    old_region = gdk_region_copy (private->clip_region);

  /* Keep children in (reverse) stacking order */
  gdk_window_raise_internal (window);

  recompute_visible_regions (private, TRUE, FALSE);

  if (old_region)
    {
      new_region = gdk_region_copy (private->clip_region);

      gdk_region_subtract (new_region, old_region);
      gdk_window_invalidate_region_full (window, new_region, TRUE, CLEAR_BG_ALL);

      gdk_region_destroy (old_region);
      gdk_region_destroy (new_region);
    }
}

static void
gdk_window_lower_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *parent = private->parent;
  GdkWindowImplIface *impl_iface;
  GdkWindowObject *above;
  GList *native_children;
  GList *l, listhead;

  if (parent)
    {
      parent->children = g_list_remove (parent->children, window);
      parent->children = g_list_append (parent->children, window);
    }

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  /* Just do native lower for toplevels */
  if (gdk_window_is_toplevel (private) ||
      /* The restack_under codepath should work correctly even if the parent
	 is native, but it relies on the order of ->children to be correct,
	 and some apps like SWT reorder the x windows without gdks knowledge,
	 so we use lower directly in order to make these behave as before
	 when using native windows */
      (gdk_window_has_impl (private) && gdk_window_has_impl (parent)))
    {
      impl_iface->lower (window);
    }
  else if (gdk_window_has_impl (private))
    {
      above = find_native_sibling_above (parent, private);
      if (above)
	{
	  listhead.data = window;
	  listhead.next = NULL;
	  listhead.prev = NULL;
	  impl_iface->restack_under ((GdkWindow *)above, &listhead);
	}
      else
	impl_iface->raise (window);
    }
  else
    {
      native_children = NULL;
      get_all_native_children (private, &native_children);
      if (native_children != NULL)
	{
	  above = find_native_sibling_above (parent, private);

	  if (above)
	    impl_iface->restack_under ((GdkWindow *)above,
				       native_children);
	  else
	    {
	      /* Right order, since native_children is bottom-topmost first */
	      for (l = native_children; l != NULL; l = l->next)
		impl_iface->raise (l->data);
	    }

	  g_list_free (native_children);
	}

    }
}

static void
gdk_window_invalidate_in_parent (GdkWindowObject *private)
{
  GdkRectangle r, child;

  if (gdk_window_is_toplevel (private))
    return;

  /* get the visible rectangle of the parent */
  r.x = r.y = 0;
  r.width = private->parent->width;
  r.height = private->parent->height;

  child.x = private->x;
  child.y = private->y;
  child.width = private->width;
  child.height = private->height;
  gdk_rectangle_intersect (&r, &child, &r);

  gdk_window_invalidate_rect_full (GDK_WINDOW (private->parent), &r, TRUE, CLEAR_BG_ALL);
}


/**
 * gdk_window_lower:
 * @window: a #GdkWindow
 *
 * Lowers @window to the bottom of the Z-order (stacking order), so that
 * other windows with the same parent window appear above @window.
 * This is true whether or not the other windows are visible.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_lower() only
 * requests the restack, does not guarantee it.
 *
 * Note that gdk_window_show() raises the window again, so don't call this
 * function before gdk_window_show(). (Try gdk_window_show_unraised().)
 */
void
gdk_window_lower (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  gdk_window_flush_if_exposing (window);

  /* Keep children in (reverse) stacking order */
  gdk_window_lower_internal (window);

  recompute_visible_regions (private, TRUE, FALSE);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  gdk_window_invalidate_in_parent (private);
}

/**
 * gdk_window_restack:
 * @window: a #GdkWindow
 * @sibling: (allow-none): a #GdkWindow that is a sibling of @window, or %NULL
 * @above: a boolean
 *
 * Changes the position of  @window in the Z-order (stacking order), so that
 * it is above @sibling (if @above is %TRUE) or below @sibling (if @above is
 * %FALSE).
 *
 * If @sibling is %NULL, then this either raises (if @above is %TRUE) or
 * lowers the window.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_restack() only
 * requests the restack, does not guarantee it.
 *
 * Since: 2.18
 */
void
gdk_window_restack (GdkWindow     *window,
		    GdkWindow     *sibling,
		    gboolean       above)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  GdkWindowObject *parent;
  GdkWindowObject *above_native;
  GList *sibling_link;
  GList *native_children;
  GList *l, listhead;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (sibling == NULL || GDK_IS_WINDOW (sibling));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  if (sibling == NULL)
    {
      if (above)
	gdk_window_raise (window);
      else
	gdk_window_lower (window);
      return;
    }

  gdk_window_flush_if_exposing (window);

  if (gdk_window_is_toplevel (private))
    {
      g_return_if_fail (gdk_window_is_toplevel (GDK_WINDOW_OBJECT (sibling)));
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->restack_toplevel (window, sibling, above);
      return;
    }

  parent = private->parent;
  if (parent)
    {
      sibling_link = g_list_find (parent->children, sibling);
      g_return_if_fail (sibling_link != NULL);
      if (sibling_link == NULL)
	return;

      parent->children = g_list_remove (parent->children, window);
      if (above)
	parent->children = g_list_insert_before (parent->children,
						 sibling_link,
						 window);
      else
	parent->children = g_list_insert_before (parent->children,
						 sibling_link->next,
						 window);

      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      if (gdk_window_has_impl (private))
	{
	  above_native = find_native_sibling_above (parent, private);
	  if (above_native)
	    {
	      listhead.data = window;
	      listhead.next = NULL;
	      listhead.prev = NULL;
	      impl_iface->restack_under ((GdkWindow *)above_native, &listhead);
	    }
	  else
	    impl_iface->raise (window);
	}
      else
	{
	  native_children = NULL;
	  get_all_native_children (private, &native_children);
	  if (native_children != NULL)
	    {
	      above_native = find_native_sibling_above (parent, private);
	      if (above_native)
		impl_iface->restack_under ((GdkWindow *)above_native,
					   native_children);
	      else
		{
		  /* Right order, since native_children is bottom-topmost first */
		  for (l = native_children; l != NULL; l = l->next)
		    impl_iface->raise (l->data);
		}

	      g_list_free (native_children);
	    }
	}
    }

  recompute_visible_regions (private, TRUE, FALSE);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  gdk_window_invalidate_in_parent (private);
}


/**
 * gdk_window_show:
 * @window: a #GdkWindow
 *
 * Like gdk_window_show_unraised(), but also raises the window to the
 * top of the window stack (moves the window to the front of the
 * Z-order).
 *
 * This function maps a window so it's visible onscreen. Its opposite
 * is gdk_window_hide().
 *
 * When implementing a #GtkWidget, you should call this function on the widget's
 * #GdkWindow as part of the "map" method.
 */
void
gdk_window_show (GdkWindow *window)
{
  gdk_window_show_internal (window, TRUE);
}

/**
 * gdk_window_hide:
 * @window: a #GdkWindow
 *
 * For toplevel windows, withdraws them, so they will no longer be
 * known to the window manager; for all windows, unmaps them, so
 * they won't be displayed. Normally done automatically as
 * part of gtk_widget_hide().
 */
void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  gboolean was_mapped, did_hide;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (private);

  if (gdk_window_has_impl (private))
    {

      if (GDK_WINDOW_IS_MAPPED (window))
	gdk_synthesize_window_state (window,
				     0,
				     GDK_WINDOW_STATE_WITHDRAWN);
    }
  else if (was_mapped)
    {
      GdkDisplay *display;

      /* May need to break grabs on children */
      display = gdk_drawable_get_display (window);

      if (_gdk_display_end_pointer_grab (display,
					 _gdk_windowing_window_get_next_serial (display),
					 window,
					 TRUE))
	gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);

      if (display->keyboard_grab.window != NULL)
	{
	  if (is_parent_of (window, display->keyboard_grab.window))
	    {
	      /* Call this ourselves, even though gdk_display_keyboard_ungrab
		 does so too, since we want to pass implicit == TRUE so the
		 broken grab event is generated */
	      _gdk_display_unset_has_keyboard_grab (display,
						    TRUE);
	      gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
	    }
	}

      private->state = GDK_WINDOW_STATE_WITHDRAWN;
    }

  did_hide = _gdk_window_update_viewable (window);

  /* Hide foreign window as those are not handled by update_viewable. */
  if (gdk_window_has_impl (private) && (!did_hide))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->hide (window);
    }

  recompute_visible_regions (private, TRUE, FALSE);

  /* all decendants became non-visible, we need to send visibility notify */
  gdk_window_update_visibility_recursively (private, NULL);

  if (was_mapped && !gdk_window_has_impl (private))
    {
      if (private->event_mask & GDK_STRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);

      if (private->parent && private->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);

      _gdk_synthesize_crossing_events_for_geometry_change (GDK_WINDOW (private->parent));
    }

  /* Invalidate the rect */
  if (was_mapped)
    gdk_window_invalidate_in_parent (private);
}

/**
 * gdk_window_withdraw:
 * @window: a toplevel #GdkWindow
 *
 * Withdraws a window (unmaps it and asks the window manager to forget about it).
 * This function is not really useful as gdk_window_hide() automatically
 * withdraws toplevel windows before hiding them.
 **/
void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (private);

  if (gdk_window_has_impl (private))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->withdraw (window);

      if (was_mapped)
	{
	  if (private->event_mask & GDK_STRUCTURE_MASK)
	    _gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);

	  if (private->parent && private->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	    _gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);

	  _gdk_synthesize_crossing_events_for_geometry_change (GDK_WINDOW (private->parent));
	}

      recompute_visible_regions (private, TRUE, FALSE);
    }
}

/**
 * gdk_window_set_events:
 * @window: a #GdkWindow
 * @event_mask: event mask for @window
 *
 * The event mask for a window determines which events will be reported
 * for that window. For example, an event mask including #GDK_BUTTON_PRESS_MASK
 * means the window should report button press events. The event mask
 * is the bitwise OR of values from the #GdkEventMask enumeration.
 **/
void
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  /* If motion hint is disabled, enable motion events again */
  display = gdk_drawable_get_display (window);
  if ((private->event_mask & GDK_POINTER_MOTION_HINT_MASK) &&
      !(event_mask & GDK_POINTER_MOTION_HINT_MASK))
    _gdk_display_enable_motion_hints (display);

  private->event_mask = event_mask;

  if (gdk_window_has_impl (private))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->set_events (window,
			      get_native_event_mask (private));
    }

}

/**
 * gdk_window_get_events:
 * @window: a #GdkWindow
 *
 * Gets the event mask for @window. See gdk_window_set_events().
 *
 * Return value: event mask for @window
 **/
GdkEventMask
gdk_window_get_events (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return 0;

  return private->event_mask;
}

static void
gdk_window_move_resize_toplevel (GdkWindow *window,
				 gboolean   with_move,
				 gint       x,
				 gint       y,
				 gint       width,
				 gint       height)
{
  GdkWindowObject *private;
  GdkRegion *old_region, *new_region;
  GdkWindowImplIface *impl_iface;
  gboolean expose;
  int old_x, old_y, old_abs_x, old_abs_y;
  int dx, dy;
  gboolean is_resize;

  private = (GdkWindowObject *) window;

  expose = FALSE;
  old_region = NULL;

  old_x = private->x;
  old_y = private->y;

  is_resize = (width != -1) || (height != -1);

  if (gdk_window_is_viewable (window) &&
      !private->input_only)
    {
      expose = TRUE;
      old_region = gdk_region_copy (private->clip_region);
    }

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  impl_iface->move_resize (window, with_move, x, y, width, height);

  dx = private->x - old_x;
  dy = private->y - old_y;

  old_abs_x = private->abs_x;
  old_abs_y = private->abs_y;

  /* Avoid recomputing for pure toplevel moves, for performance reasons */
  if (is_resize)
    recompute_visible_regions (private, TRUE, FALSE);

  if (expose)
    {
      new_region = gdk_region_copy (private->clip_region);

      /* This is the newly exposed area (due to any resize),
       * X will expose it, but lets do that without the
       * roundtrip
       */
      gdk_region_subtract (new_region, old_region);
      gdk_window_invalidate_region_full (window, new_region, TRUE, CLEAR_BG_WINCLEARED);

      gdk_region_destroy (old_region);
      gdk_region_destroy (new_region);
    }

  _gdk_synthesize_crossing_events_for_geometry_change (window);
}


static void
move_native_children (GdkWindowObject *private)
{
  GList *l;
  GdkWindowObject *child;
  GdkWindowImplIface *impl_iface;

  for (l = private->children; l; l = l->next)
    {
      child = l->data;

      if (child->impl != private->impl)
	{
	  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (child->impl);
	  impl_iface->move_resize ((GdkWindow *)child, TRUE,
				   child->x, child->y,
				   child->width, child->height);
	}
      else
	move_native_children  (child);
    }
}

static gboolean
collect_native_child_region_helper (GdkWindowObject *window,
				    GdkWindow *impl,
				    GdkRegion **region,
				    int x_offset,
				    int y_offset)
{
  GdkWindowObject *child;
  GdkRegion *tmp;
  GList *l;

  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (!GDK_WINDOW_IS_MAPPED (child) || child->input_only)
	continue;

      if (child->impl != impl)
	{
	  tmp = gdk_region_copy (child->clip_region);
	  gdk_region_offset (tmp,
			     x_offset + child->x,
			     y_offset + child->y);
	  if (*region == NULL)
	    *region = tmp;
	  else
	    {
	      gdk_region_union (*region, tmp);
	      gdk_region_destroy (tmp);
	    }
	}
      else
	collect_native_child_region_helper (child, impl, region,
					    x_offset + child->x,
					    y_offset + child->y);
    }

  return FALSE;
}

static GdkRegion *
collect_native_child_region (GdkWindowObject *window,
			     gboolean include_this)
{
  GdkRegion *region;

  if (include_this && gdk_window_has_impl (window) && window->viewable)
    return gdk_region_copy (window->clip_region);

  region = NULL;

  collect_native_child_region_helper (window, window->impl, &region, 0, 0);

  return region;
}


static void
gdk_window_move_resize_internal (GdkWindow *window,
				 gboolean   with_move,
				 gint       x,
				 gint       y,
				 gint       width,
				 gint       height)
{
  GdkWindowObject *private;
  GdkRegion *old_region, *new_region, *copy_area;
  GdkRegion *old_native_child_region, *new_native_child_region;
  GdkWindowObject *impl_window;
  GdkWindowImplIface *impl_iface;
  gboolean expose;
  int old_x, old_y, old_abs_x, old_abs_y;
  int dx, dy;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  if (gdk_window_is_toplevel (private))
    {
      gdk_window_move_resize_toplevel (window, with_move, x, y, width, height);
      return;
    }

  /* Bail early if no change */
  if (private->width == width &&
      private->height == height &&
      (!with_move ||
       (private->x == x &&
	private->y == y)))
    return;

  gdk_window_flush_if_exposing (window);

  /* Handle child windows */

  expose = FALSE;
  old_region = NULL;

  impl_window = gdk_window_get_impl_window (private);

  old_x = private->x;
  old_y = private->y;

  old_native_child_region = NULL;
  if (gdk_window_is_viewable (window) &&
      !private->input_only)
    {
      expose = TRUE;

      old_region = gdk_region_copy (private->clip_region);
      /* Adjust region to parent window coords */
      gdk_region_offset (old_region, private->x, private->y);

      old_native_child_region = collect_native_child_region (private, TRUE);
      if (old_native_child_region)
	{
	  /* Adjust region to parent window coords */
	  gdk_region_offset (old_native_child_region, private->x, private->y);

	  /* Any native window move will immediately copy stuff to the destination, which may overwrite a
	   * source or destination for a delayed GdkWindowRegionMove. So, we need
	   * to flush those here for the parent window and all overlapped subwindows
	   * of it. And we need to do this before setting the new clips as those will be
	   * affecting this.
	   */
	  gdk_window_flush_recursive (private->parent);
	}
    }

  /* Set the new position and size */
  if (with_move)
    {
      private->x = x;
      private->y = y;
    }
  if (!(width < 0 && height < 0))
    {
      if (width < 1)
	width = 1;
      private->width = width;
      if (height < 1)
	height = 1;
      private->height = height;
    }

  dx = private->x - old_x;
  dy = private->y - old_y;

  old_abs_x = private->abs_x;
  old_abs_y = private->abs_y;

  recompute_visible_regions (private, TRUE, FALSE);

  new_native_child_region = NULL;
  if (old_native_child_region)
    {
      new_native_child_region = collect_native_child_region (private, TRUE);
      /* Adjust region to parent window coords */
      gdk_region_offset (new_native_child_region, private->x, private->y);
    }

  if (gdk_window_has_impl (private))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);

      /* Do the actual move after recomputing things, as this will have set the shape to
	 the now correct one, thus avoiding copying regions that should not be copied. */
      impl_iface->move_resize (window, TRUE,
			       private->x, private->y,
			       private->width, private->height);
    }
  else if (old_abs_x != private->abs_x ||
	   old_abs_y != private->abs_y)
    move_native_children (private);

  if (expose)
    {
      new_region = gdk_region_copy (private->clip_region);
      /* Adjust region to parent window coords */
      gdk_region_offset (new_region, private->x, private->y);

      /* copy_area:
       * Part of the data at the new location can be copied from the
       * old location, this area is the intersection of the old region
       * moved as the copy will move it and then intersected with
       * the new region.
       *
       * new_region:
       * Everything in the old and new regions that is not copied must be
       * invalidated (including children) as this is newly exposed
       */
      copy_area = gdk_region_copy (new_region);

      gdk_region_union (new_region, old_region);

      if (old_native_child_region)
	{
	  /* Don't copy from inside native children, as this is copied by
	   * the native window move.
	   */
	  gdk_region_subtract (old_region, old_native_child_region);
	}
      gdk_region_offset (old_region, dx, dy);

      gdk_region_intersect (copy_area, old_region);

      if (new_native_child_region)
	{
	  /* Don't copy any bits that would cause a read from the moved
	     native windows, as we can't read that data */
	  gdk_region_offset (new_native_child_region, dx, dy);
	  gdk_region_subtract (copy_area, new_native_child_region);
	  gdk_region_offset (new_native_child_region, -dx, -dy);
	}

      gdk_region_subtract (new_region, copy_area);

      /* Convert old region to impl coords */
      gdk_region_offset (old_region, -dx + private->abs_x - private->x, -dy + private->abs_y - private->y);

      /* convert from parent coords to impl */
      gdk_region_offset (copy_area, private->abs_x - private->x, private->abs_y - private->y);

      move_region_on_impl (impl_window, copy_area, dx, dy); /* takes ownership of copy_area */

      /* Invalidate affected part in the parent window
       *  (no higher window should be affected)
       * We also invalidate any children in that area, which could include
       * this window if it still overlaps that area.
       */
      if (old_native_child_region)
	{
	  /* No need to expose the region that the native window move copies */
	  gdk_region_offset (old_native_child_region, dx, dy);
	  gdk_region_intersect (old_native_child_region, new_native_child_region);
	  gdk_region_subtract (new_region, old_native_child_region);
	}
      gdk_window_invalidate_region_full (GDK_WINDOW (private->parent), new_region, TRUE, CLEAR_BG_ALL);

      gdk_region_destroy (old_region);
      gdk_region_destroy (new_region);
    }

  if (old_native_child_region)
    {
      gdk_region_destroy (old_native_child_region);
      gdk_region_destroy (new_native_child_region);
    }

  _gdk_synthesize_crossing_events_for_geometry_change (window);
}



/**
 * gdk_window_move:
 * @window: a #GdkWindow
 * @x: X coordinate relative to window's parent
 * @y: Y coordinate relative to window's parent
 *
 * Repositions a window relative to its parent window.
 * For toplevel windows, window managers may ignore or modify the move;
 * you should probably use gtk_window_move() on a #GtkWindow widget
 * anyway, instead of using GDK functions. For child windows,
 * the move will reliably succeed.
 *
 * If you're also planning to resize the window, use gdk_window_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  gdk_window_move_resize_internal (window, TRUE, x, y, -1, -1);
}

/**
 * gdk_window_resize:
 * @window: a #GdkWindow
 * @width: new width of the window
 * @height: new height of the window
 *
 * Resizes @window; for toplevel windows, asks the window manager to resize
 * the window. The window manager may not allow the resize. When using GTK+,
 * use gtk_window_resize() instead of this low-level GDK function.
 *
 * Windows may not be resized below 1x1.
 *
 * If you're also planning to move the window, use gdk_window_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  gdk_window_move_resize_internal (window, FALSE, 0, 0, width, height);
}


/**
 * gdk_window_move_resize:
 * @window: a #GdkWindow
 * @x: new X position relative to window's parent
 * @y: new Y position relative to window's parent
 * @width: new width
 * @height: new height
 *
 * Equivalent to calling gdk_window_move() and gdk_window_resize(),
 * except that both operations are performed at once, avoiding strange
 * visual effects. (i.e. the user may be able to see the window first
 * move, then resize, if you don't use gdk_window_move_resize().)
 **/
void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  gdk_window_move_resize_internal (window, TRUE, x, y, width, height);
}


/**
 * gdk_window_scroll:
 * @window: a #GdkWindow
 * @dx: Amount to scroll in the X direction
 * @dy: Amount to scroll in the Y direction
 *
 * Scroll the contents of @window, both pixels and children, by the
 * given amount. @window itself does not move. Portions of the window
 * that the scroll operation brings in from offscreen areas are
 * invalidated. The invalidated region may be bigger than what would
 * strictly be necessary.
 *
 * For X11, a minimum area will be invalidated if the window has no
 * subwindows, or if the edges of the window's parent do not extend
 * beyond the edges of the window. In other cases, a multi-step process
 * is used to scroll the window which may produce temporary visual
 * artifacts and unnecessary invalidations.
 **/
void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowObject *impl_window;
  GdkRegion *copy_area, *noncopy_area;
  GdkRegion *old_native_child_region, *new_native_child_region;
  GList *tmp_list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (dx == 0 && dy == 0)
    return;

  if (private->destroyed)
    return;

  gdk_window_flush_if_exposing (window);

  old_native_child_region = collect_native_child_region (private, FALSE);
  if (old_native_child_region)
    {
      /* Any native window move will immediately copy stuff to the destination, which may overwrite a
       * source or destination for a delayed GdkWindowRegionMove. So, we need
       * to flush those here for the window and all overlapped subwindows
       * of it. And we need to do this before setting the new clips as those will be
       * affecting this.
       */
      gdk_window_flush_recursive (private);
    }


  /* First move all child windows, without causing invalidation */

  tmp_list = private->children;
  while (tmp_list)
    {
      GdkWindow *child = GDK_WINDOW (tmp_list->data);
      GdkWindowObject *child_obj = GDK_WINDOW_OBJECT (child);

      /* Just update the positions, the bits will move with the copy */
      child_obj->x += dx;
      child_obj->y += dy;

      tmp_list = tmp_list->next;
    }

  recompute_visible_regions (private, FALSE, TRUE);

  new_native_child_region = NULL;
  if (old_native_child_region)
    new_native_child_region = collect_native_child_region (private, FALSE);

  move_native_children (private);

  /* Then copy the actual bits of the window w/ child windows */

  impl_window = gdk_window_get_impl_window (private);

  /* Calculate the area that can be gotten by copying the old area */
  copy_area = gdk_region_copy (private->clip_region);
  if (old_native_child_region)
    {
      /* Don't copy from inside native children, as this is copied by
       * the native window move.
       */
      gdk_region_subtract (copy_area, old_native_child_region);

      /* Don't copy any bits that would cause a read from the moved
	 native windows, as we can't read that data */
      gdk_region_subtract (copy_area, new_native_child_region);
    }
  gdk_region_offset (copy_area, dx, dy);
  gdk_region_intersect (copy_area, private->clip_region);

  /* And the rest need to be invalidated */
  noncopy_area = gdk_region_copy (private->clip_region);
  gdk_region_subtract (noncopy_area, copy_area);

  /* convert from window coords to impl */
  gdk_region_offset (copy_area, private->abs_x, private->abs_y);

  move_region_on_impl (impl_window, copy_area, dx, dy); /* takes ownership of copy_area */

  /* Invalidate not copied regions */
  if (old_native_child_region)
    {
      /* No need to expose the region that the native window move copies */
      gdk_region_offset (old_native_child_region, dx, dy);
      gdk_region_intersect (old_native_child_region, new_native_child_region);
      gdk_region_subtract (noncopy_area, old_native_child_region);
    }
  gdk_window_invalidate_region_full (window, noncopy_area, TRUE, CLEAR_BG_ALL);

  gdk_region_destroy (noncopy_area);

  if (old_native_child_region)
    {
      gdk_region_destroy (old_native_child_region);
      gdk_region_destroy (new_native_child_region);
    }

  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

/**
 * gdk_window_move_region:
 * @window: a #GdkWindow
 * @region: The #GdkRegion to move
 * @dx: Amount to move in the X direction
 * @dy: Amount to move in the Y direction
 *
 * Move the part of @window indicated by @region by @dy pixels in the Y
 * direction and @dx pixels in the X direction. The portions of @region
 * that not covered by the new position of @region are invalidated.
 *
 * Child windows are not moved.
 *
 * Since: 2.8
 */
void
gdk_window_move_region (GdkWindow       *window,
			const GdkRegion *region,
			gint             dx,
			gint             dy)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowObject *impl_window;
  GdkRegion *nocopy_area;
  GdkRegion *copy_area;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (region != NULL);

  if (dx == 0 && dy == 0)
    return;

  if (private->destroyed)
    return;

  impl_window = gdk_window_get_impl_window (private);

  /* compute source regions */
  copy_area = gdk_region_copy (region);
  gdk_region_intersect (copy_area, private->clip_region_with_children);

  /* compute destination regions */
  gdk_region_offset (copy_area, dx, dy);
  gdk_region_intersect (copy_area, private->clip_region_with_children);

  /* Invalidate parts of the region (source and dest) not covered
     by the copy */
  nocopy_area = gdk_region_copy (region);
  gdk_region_offset (nocopy_area, dx, dy);
  gdk_region_union (nocopy_area, region);
  gdk_region_subtract (nocopy_area, copy_area);

  /* convert from window coords to impl */
  gdk_region_offset (copy_area, private->abs_x, private->abs_y);
  move_region_on_impl (impl_window, copy_area, dx, dy); /* Takes ownership of copy_area */

  gdk_window_invalidate_region_full (window, nocopy_area, FALSE, CLEAR_BG_ALL);
  gdk_region_destroy (nocopy_area);
}

/**
 * gdk_window_set_background:
 * @window: a #GdkWindow
 * @color: an allocated #GdkColor
 *
 * Sets the background color of @window. (However, when using GTK+,
 * set the background of a widget with gtk_widget_modify_bg() - if
 * you're an application - or gtk_style_set_background() - if you're
 * implementing a custom widget.)
 *
 * The @color must be allocated; gdk_rgb_find_color() is the best way
 * to allocate a color.
 *
 * See also gdk_window_set_back_pixmap().
 */
void
gdk_window_set_background (GdkWindow      *window,
			   const GdkColor *color)
{
  GdkWindowObject *private;
  GdkColormap *colormap = gdk_drawable_get_colormap (window);
  GdkWindowImplIface *impl_iface;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  private->bg_color = *color;
  gdk_colormap_query_color (colormap, private->bg_color.pixel, &private->bg_color);

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  private->bg_pixmap = NULL;

  if (private->background)
    {
      cairo_pattern_destroy (private->background);
      private->background = NULL;
    }

  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_window_has_impl (private) &&
      !private->input_only)
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->set_background (window, &private->bg_color);
    }
}

/**
 * gdk_window_set_back_pixmap:
 * @window: a #GdkWindow
 * @pixmap: (allow-none): a #GdkPixmap, or %NULL
 * @parent_relative: whether the tiling origin is at the origin of
 *   @window's parent
 *
 * Sets the background pixmap of @window. May also be used to set a
 * background of "None" on @window, by setting a background pixmap
 * of %NULL.
 *
 * A background pixmap will be tiled, positioning the first tile at
 * the origin of @window, or if @parent_relative is %TRUE, the tiling
 * will be done based on the origin of the parent window (useful to
 * align tiles in a parent with tiles in a child).
 *
 * A background pixmap of %NULL means that the window will have no
 * background.  A window with no background will never have its
 * background filled by the windowing system, instead the window will
 * contain whatever pixels were already in the corresponding area of
 * the display.
 *
 * The windowing system will normally fill a window with its background
 * when the window is obscured then exposed, and when you call
 * gdk_window_clear().
 */
void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gboolean   parent_relative)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);
  g_return_if_fail (pixmap == NULL || gdk_drawable_get_depth (window) == gdk_drawable_get_depth (pixmap));

  private = (GdkWindowObject *) window;

  if (pixmap && !gdk_drawable_get_colormap (pixmap))
    {
      g_warning ("gdk_window_set_back_pixmap(): pixmap must have a colormap");
      return;
    }

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  if (private->background)
    {
      cairo_pattern_destroy (private->background);
      private->background = NULL;
    }

  if (parent_relative)
    private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
  else if (pixmap)
    private->bg_pixmap = g_object_ref (pixmap);
  else
    private->bg_pixmap = GDK_NO_BG;

  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_window_has_impl (private) &&
      !private->input_only)
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->set_back_pixmap (window, private->bg_pixmap);
    }
}

/**
 * gdk_window_get_background_pattern:
 * @window: a window
 *
 * Gets the pattern used to clear the background on @window. If @window
 * does not have its own background and reuses the parent's, %NULL is
 * returned and you'll have to query it yourself.
 *
 * Returns: (transfer none): The pattern to use for the background or
 *     %NULL to use the parent's background.
 *
 * Since: 2.22
 **/
cairo_pattern_t *
gdk_window_get_background_pattern (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *) window;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (private->background == NULL)
    {
      if (private->bg_pixmap == GDK_PARENT_RELATIVE_BG)
        private->background = NULL;
      else if (private->bg_pixmap != GDK_NO_BG &&
               private->bg_pixmap != NULL)
        {
          static cairo_user_data_key_t key;
          cairo_surface_t *surface;

          surface = _gdk_drawable_ref_cairo_surface (private->bg_pixmap);
          private->background = cairo_pattern_create_for_surface (surface);
          cairo_surface_destroy (surface);

          cairo_pattern_set_extend (private->background, CAIRO_EXTEND_REPEAT);
          cairo_pattern_set_user_data (private->background,
                                       &key,
                                       g_object_ref (private->bg_pixmap),
                                       g_object_unref);
        }
      else
        private->background =
            cairo_pattern_create_rgb (private->bg_color.red   / 65535.,
                                      private->bg_color.green / 65535.,
                                      private->bg_color.blue / 65535.);
    }   

  return private->background;
}

/**
 * gdk_window_get_cursor:
 * @window: a #GdkWindow
 *
 * Retrieves a #GdkCursor pointer for the cursor currently set on the
 * specified #GdkWindow, or %NULL.  If the return value is %NULL then
 * there is no custom cursor set on the specified window, and it is
 * using the cursor for its parent window.
 *
 * Return value: (transfer none): a #GdkCursor, or %NULL. The returned
 *   object is owned by the #GdkWindow and should not be unreferenced
 *   directly. Use gdk_window_set_cursor() to unset the cursor of the
 *   window
 *
 * Since: 2.18
 */
GdkCursor *
gdk_window_get_cursor (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  private = (GdkWindowObject *) window;

  return private->cursor;
}

/**
 * gdk_window_set_cursor:
 * @window: a #GdkWindow
 * @cursor: (allow-none): a cursor
 *
 * Sets the mouse pointer for a #GdkWindow. Use gdk_cursor_new_for_display()
 * or gdk_cursor_new_from_pixmap() to create the cursor. To make the cursor
 * invisible, use %GDK_BLANK_CURSOR. Passing %NULL for the @cursor argument
 * to gdk_window_set_cursor() means that @window will use the cursor of its
 * parent window. Most windows should use this default.
 */
void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  display = gdk_drawable_get_display (window);

  if (private->cursor)
    {
      gdk_cursor_unref (private->cursor);
      private->cursor = NULL;
    }

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (cursor)
	private->cursor = gdk_cursor_ref (cursor);

      if (_gdk_native_windows ||
	  private->window_type == GDK_WINDOW_ROOT ||
          private->window_type == GDK_WINDOW_FOREIGN)
	{
	  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
	  impl_iface->set_cursor (window, cursor);
	}
      else if (_gdk_window_event_parent_of (window, display->pointer_info.window_under_pointer))
	update_cursor (display);

      g_object_notify (G_OBJECT (window), "cursor");
    }
}

/**
 * gdk_window_get_geometry:
 * @window: a #GdkWindow
 * @x: return location for X coordinate of window (relative to its parent)
 * @y: return location for Y coordinate of window (relative to its parent)
 * @width: return location for width of window
 * @height: return location for height of window
 * @depth: return location for bit depth of window
 *
 * Any of the return location arguments to this function may be %NULL,
 * if you aren't interested in getting the value of that field.
 *
 * The X and Y coordinates returned are relative to the parent window
 * of @window, which for toplevels usually means relative to the
 * window decorations (titlebar, etc.) rather than relative to the
 * root window (screen-size background window).
 *
 * On the X11 platform, the geometry is obtained from the X server,
 * so reflects the latest position of @window; this may be out-of-sync
 * with the position of @window delivered in the most-recently-processed
 * #GdkEventConfigure. gdk_window_get_position() in contrast gets the
 * position from the most recent configure event.
 *
 * <note>
 * If @window is not a toplevel, it is <emphasis>much</emphasis> better
 * to call gdk_window_get_position() and gdk_drawable_get_size() instead,
 * because it avoids the roundtrip to the X server and because
 * gdk_drawable_get_size() supports the full 32-bit coordinate space,
 * whereas gdk_window_get_geometry() is restricted to the 16-bit
 * coordinates of X11.
 *</note>
 **/
void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GdkWindowObject *private, *parent;
  GdkWindowImplIface *impl_iface;

  if (!window)
    {
      GDK_NOTE (MULTIHEAD,
		g_message ("gdk_window_get_geometry(): Window needs "
			   "to be non-NULL to be multi head safe"));
      window = gdk_screen_get_root_window ((gdk_screen_get_default ()));
    }

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (gdk_window_has_impl (private))
	{
	  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
	  impl_iface->get_geometry (window, x, y,
				    width, height,
				    depth);
	  /* This reports the position wrt to the native parent, we need to convert
	     it to be relative to the client side parent */
	  parent = private->parent;
	  if (parent && !gdk_window_has_impl (parent))
	    {
	      if (x)
		*x -= parent->abs_x;
	      if (y)
		*y -= parent->abs_y;
	    }
	}
      else
	{
          if (x)
            *x = private->x;
          if (y)
            *y = private->y;
	  if (width)
	    *width = private->width;
	  if (height)
	    *height = private->height;
	  if (depth)
	    *depth = private->depth;
	}
    }
}

/**
 * gdk_window_get_origin:
 * @window: a #GdkWindow
 * @x: return location for X coordinate
 * @y: return location for Y coordinate
 *
 * Obtains the position of a window in root window coordinates.
 * (Compare with gdk_window_get_position() and
 * gdk_window_get_geometry() which return the position of a window
 * relative to its parent window.)
 *
 * Return value: not meaningful, ignore
 */
gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    {
      if (x)
	*x = 0;
      if (y)
	*y = 0;
      return 0;
    }
  
  private = (GdkWindowObject *) window;

  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  impl_iface->get_root_coords (window,
			       private->abs_x,
			       private->abs_y,
			       x, y);

  return TRUE;
}

/**
 * gdk_window_get_root_coords:
 * @window: a #GdkWindow
 * @x: X coordinate in window
 * @y: Y coordinate in window
 * @root_x: (out): return location for X coordinate
 * @root_y: (out): return location for Y coordinate
 *
 * Obtains the position of a window position in root
 * window coordinates. This is similar to
 * gdk_window_get_origin() but allows you go pass
 * in any position in the window, not just the origin.
 *
 * Since: 2.18
 */
void
gdk_window_get_root_coords (GdkWindow *window,
			    gint       x,
			    gint       y,
			    gint      *root_x,
			    gint      *root_y)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (GDK_WINDOW_DESTROYED (window))
    {
      if (x)
	*root_x = x;
      if (y)
	*root_y = y;
      return;
    }
  
  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
  impl_iface->get_root_coords (window,
			       x + private->abs_x,
			       y + private->abs_y,
			       root_x, root_y);
}

/**
 * gdk_window_coords_to_parent:
 * @window: a child window
 * @x: X coordinate in child's coordinate system
 * @y: Y coordinate in child's coordinate system
 * @parent_x: (out) (allow-none): return location for X coordinate
 * in parent's coordinate system, or %NULL
 * @parent_y: (out) (allow-none): return location for Y coordinate
 * in parent's coordinate system, or %NULL
 *
 * Transforms window coordinates from a child window to its parent
 * window, where the parent window is the normal parent as returned by
 * gdk_window_get_parent() for normal windows, and the window's
 * embedder as returned by gdk_offscreen_window_get_embedder() for
 * offscreen windows.
 *
 * For normal windows, calling this function is equivalent to adding
 * the return values of gdk_window_get_position() to the child coordinates.
 * For offscreen windows however (which can be arbitrarily transformed),
 * this function calls the GdkWindow::to-embedder: signal to translate
 * the coordinates.
 *
 * You should always use this function when writing generic code that
 * walks up a window hierarchy.
 *
 * See also: gdk_window_coords_from_parent()
 *
 * Since: 2.22
 **/
void
gdk_window_coords_to_parent (GdkWindow *window,
                             gdouble    x,
                             gdouble    y,
                             gdouble   *parent_x,
                             gdouble   *parent_y)
{
  GdkWindowObject *obj;

  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = (GdkWindowObject *) window;

  if (gdk_window_is_offscreen (obj))
    {
      gdouble px, py;

      to_embedder (obj, x, y, &px, &py);

      if (parent_x)
        *parent_x = px;

      if (parent_y)
        *parent_y = py;
    }
  else
    {
      if (parent_x)
        *parent_x = x + obj->x;

      if (parent_y)
        *parent_y = y + obj->y;
    }
}

/**
 * gdk_window_coords_from_parent:
 * @window: a child window
 * @parent_x: X coordinate in parent's coordinate system
 * @parent_y: Y coordinate in parent's coordinate system
 * @x: (out) (allow-none): return location for X coordinate in child's coordinate system
 * @y: (out) (allow-none): return location for Y coordinate in child's coordinate system
 *
 * Transforms window coordinates from a parent window to a child
 * window, where the parent window is the normal parent as returned by
 * gdk_window_get_parent() for normal windows, and the window's
 * embedder as returned by gdk_offscreen_window_get_embedder() for
 * offscreen windows.
 *
 * For normal windows, calling this function is equivalent to subtracting
 * the return values of gdk_window_get_position() from the parent coordinates.
 * For offscreen windows however (which can be arbitrarily transformed),
 * this function calls the GdkWindow::from-embedder: signal to translate
 * the coordinates.
 *
 * You should always use this function when writing generic code that
 * walks down a window hierarchy.
 *
 * See also: gdk_window_coords_to_parent()
 *
 * Since: 2.22
 **/
void
gdk_window_coords_from_parent (GdkWindow *window,
                               gdouble    parent_x,
                               gdouble    parent_y,
                               gdouble   *x,
                               gdouble   *y)
{
  GdkWindowObject *obj;

  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = (GdkWindowObject *) window;

  if (gdk_window_is_offscreen (obj))
    {
      gdouble cx, cy;

      from_embedder (obj, parent_x, parent_y, &cx, &cy);

      if (x)
        *x = cx;

      if (y)
        *y = cy;
    }
  else
    {
      if (x)
        *x = parent_x - obj->x;

      if (y)
        *y = parent_y - obj->y;
    }
}

/**
 * gdk_window_get_deskrelative_origin:
 * @window: a toplevel #GdkWindow
 * @x: return location for X coordinate
 * @y: return location for Y coordinate
 *
 * This gets the origin of a #GdkWindow relative to
 * an Enlightenment-window-manager desktop. As long as you don't
 * assume that the user's desktop/workspace covers the entire
 * root window (i.e. you don't assume that the desktop begins
 * at root window coordinate 0,0) this function is not necessary.
 * It's deprecated for that reason.
 *
 * Return value: not meaningful
 **/
gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;
  gboolean return_val = FALSE;
  gint tx = 0;
  gint ty = 0;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *) window;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      return_val = impl_iface->get_deskrelative_origin (window, &tx, &ty);

      if (x)
	*x = tx + private->abs_x;
      if (y)
	*y = ty + private->abs_y;
    }

  return return_val;
}

/**
 * gdk_window_shape_combine_mask:
 * @window: a #GdkWindow
 * @mask: shape mask
 * @x: X position of shape mask with respect to @window
 * @y: Y position of shape mask with respect to @window
 *
 * Applies a shape mask to @window. Pixels in @window corresponding to
 * set bits in the @mask will be visible; pixels in @window
 * corresponding to unset bits in the @mask will be transparent. This
 * gives a non-rectangular window.
 *
 * If @mask is %NULL, the shape mask will be unset, and the @x/@y
 * parameters are not used.
 *
 * On the X11 platform, this uses an X server extension which is
 * widely available on most common platforms, but not available on
 * very old X servers, and occasionally the implementation will be
 * buggy. On servers without the shape extension, this function
 * will do nothing.
 *
 * This function works on both toplevel and child windows.
 */
void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint       x,
			       gint       y)
{
  GdkWindowObject *private;
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (mask)
    region = _gdk_windowing_get_shape_for_mask (mask);
  else
    region = NULL;

  gdk_window_shape_combine_region (window,
				   region,
				   x, y);

  if (region)
    gdk_region_destroy (region);
}

/**
 * gdk_window_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 *
 * Makes pixels in @window outside @shape_region be transparent,
 * so that the window may be nonrectangular. See also
 * gdk_window_shape_combine_mask() to use a bitmap as the mask.
 *
 * If @shape_region is %NULL, the shape will be unset, so the whole
 * window will be opaque again. @offset_x and @offset_y are ignored
 * if @shape_region is %NULL.
 *
 * On the X11 platform, this uses an X server extension which is
 * widely available on most common platforms, but not available on
 * very old X servers, and occasionally the implementation will be
 * buggy. On servers without the shape extension, this function
 * will do nothing.
 *
 * This function works on both toplevel and child windows.
 */
void
gdk_window_shape_combine_region (GdkWindow       *window,
				 const GdkRegion *shape_region,
				 gint             offset_x,
				 gint             offset_y)
{
  GdkWindowObject *private;
  GdkRegion *old_region, *new_region, *diff;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!private->shaped && shape_region == NULL)
    return;

  private->shaped = (shape_region != NULL);

  if (private->shape)
    gdk_region_destroy (private->shape);

  old_region = NULL;
  if (GDK_WINDOW_IS_MAPPED (window))
    old_region = gdk_region_copy (private->clip_region);

  if (shape_region)
    {
      private->shape = gdk_region_copy (shape_region);
      gdk_region_offset (private->shape, offset_x, offset_y);
    }
  else
    private->shape = NULL;

  recompute_visible_regions (private, TRUE, FALSE);

  if (gdk_window_has_impl (private) &&
      !should_apply_clip_as_shape (private))
    apply_shape (private, private->shape);

  if (old_region)
    {
      new_region = gdk_region_copy (private->clip_region);

      /* New area in the window, needs invalidation */
      diff = gdk_region_copy (new_region);
      gdk_region_subtract (diff, old_region);

      gdk_window_invalidate_region_full (window, diff, TRUE, CLEAR_BG_ALL);

      gdk_region_destroy (diff);

      if (!gdk_window_is_toplevel (private))
	{
	  /* New area in the non-root parent window, needs invalidation */
	  diff = gdk_region_copy (old_region);
	  gdk_region_subtract (diff, new_region);

	  /* Adjust region to parent window coords */
	  gdk_region_offset (diff, private->x, private->y);

	  gdk_window_invalidate_region_full (GDK_WINDOW (private->parent), diff, TRUE, CLEAR_BG_ALL);

	  gdk_region_destroy (diff);
	}

      gdk_region_destroy (new_region);
      gdk_region_destroy (old_region);
    }
}

static void
do_child_shapes (GdkWindow *window,
		 gboolean merge)
{
  GdkWindowObject *private;
  GdkRectangle r;
  GdkRegion *region;

  private = (GdkWindowObject *) window;

  r.x = 0;
  r.y = 0;
  r.width = private->width;
  r.height = private->height;

  region = gdk_region_rectangle (&r);
  remove_child_area (private, NULL, FALSE, region);

  if (merge && private->shape)
    gdk_region_subtract (region, private->shape);

  gdk_window_shape_combine_region (window, region, 0, 0);

  gdk_region_destroy (region);
}

/**
 * gdk_window_set_child_shapes:
 * @window: a #GdkWindow
 *
 * Sets the shape mask of @window to the union of shape masks
 * for all children of @window, ignoring the shape mask of @window
 * itself. Contrast with gdk_window_merge_child_shapes() which includes
 * the shape mask of @window in the masks to be merged.
 **/
void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_shapes (window, FALSE);
}

/**
 * gdk_window_merge_child_shapes:
 * @window: a #GdkWindow
 *
 * Merges the shape masks for any child windows into the
 * shape mask for @window. i.e. the union of all masks
 * for @window and its children will become the new mask
 * for @window. See gdk_window_shape_combine_mask().
 *
 * This function is distinct from gdk_window_set_child_shapes()
 * because it includes @window's shape mask in the set of shapes to
 * be merged.
 */
void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_shapes (window, TRUE);
}

/**
 * gdk_window_input_shape_combine_mask:
 * @window: a #GdkWindow
 * @mask: (allow-none): shape mask, or %NULL
 * @x: X position of shape mask with respect to @window
 * @y: Y position of shape mask with respect to @window
 *
 * Like gdk_window_shape_combine_mask(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the
 * mask will be passed on the window below @window.
 *
 * An input shape is typically used with RGBA windows.
 * The alpha channel of the window defines which pixels are
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the window is
 * "clickable".
 *
 * On the X11 platform, this requires version 1.1 of the
 * shape extension.
 *
 * On the Win32 platform, this functionality is not present and the
 * function does nothing.
 *
 * Since: 2.10
 */
void
gdk_window_input_shape_combine_mask (GdkWindow *window,
				     GdkBitmap *mask,
				     gint       x,
				     gint       y)
{
  GdkWindowObject *private;
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (mask)
    region = _gdk_windowing_get_shape_for_mask (mask);
  else
    region = NULL;

  gdk_window_input_shape_combine_region (window,
					 region,
					 x, y);

  if (region != NULL)
    gdk_region_destroy (region);
}

/**
 * gdk_window_input_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 *
 * Like gdk_window_shape_combine_region(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the
 * mask will be passed on the window below @window.
 *
 * An input shape is typically used with RGBA windows.
 * The alpha channel of the window defines which pixels are
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the window is
 * "clickable".
 *
 * On the X11 platform, this requires version 1.1 of the
 * shape extension.
 *
 * On the Win32 platform, this functionality is not present and the
 * function does nothing.
 *
 * Since: 2.10
 */
void
gdk_window_input_shape_combine_region (GdkWindow       *window,
				       const GdkRegion *shape_region,
				       gint             offset_x,
				       gint             offset_y)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (private->input_shape)
    gdk_region_destroy (private->input_shape);

  if (shape_region)
    {
      private->input_shape = gdk_region_copy (shape_region);
      gdk_region_offset (private->input_shape, offset_x, offset_y);
    }
  else
    private->input_shape = NULL;

  if (gdk_window_has_impl (private))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      impl_iface->input_shape_combine_region (window, private->input_shape, 0, 0);
    }

  /* Pointer may have e.g. moved outside window due to the input mask change */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static void
do_child_input_shapes (GdkWindow *window,
		       gboolean merge)
{
  GdkWindowObject *private;
  GdkRectangle r;
  GdkRegion *region;

  private = (GdkWindowObject *) window;

  r.x = 0;
  r.y = 0;
  r.width = private->width;
  r.height = private->height;

  region = gdk_region_rectangle (&r);
  remove_child_area (private, NULL, TRUE, region);

  if (merge && private->shape)
    gdk_region_subtract (region, private->shape);
  if (merge && private->input_shape)
    gdk_region_subtract (region, private->input_shape);

  gdk_window_input_shape_combine_region (window, region, 0, 0);
}


/**
 * gdk_window_set_child_input_shapes:
 * @window: a #GdkWindow
 *
 * Sets the input shape mask of @window to the union of input shape masks
 * for all children of @window, ignoring the input shape mask of @window
 * itself. Contrast with gdk_window_merge_child_input_shapes() which includes
 * the input shape mask of @window in the masks to be merged.
 *
 * Since: 2.10
 **/
void
gdk_window_set_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_input_shapes (window, FALSE);
}

/**
 * gdk_window_merge_child_input_shapes:
 * @window: a #GdkWindow
 *
 * Merges the input shape masks for any child windows into the
 * input shape mask for @window. i.e. the union of all input masks
 * for @window and its children will become the new input mask
 * for @window. See gdk_window_input_shape_combine_mask().
 *
 * This function is distinct from gdk_window_set_child_input_shapes()
 * because it includes @window's input shape mask in the set of
 * shapes to be merged.
 *
 * Since: 2.10
 **/
void
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_input_shapes (window, TRUE);
}


/**
 * gdk_window_set_static_gravities:
 * @window: a #GdkWindow
 * @use_static: %TRUE to turn on static gravity
 *
 * Set the bit gravity of the given window to static, and flag it so
 * all children get static subwindow gravity. This is used if you are
 * implementing scary features that involve deep knowledge of the
 * windowing system. Don't worry about it unless you have to.
 *
 * Return value: %TRUE if the server supports static gravity
 */
gboolean
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowObject *private;
  GdkWindowImplIface *impl_iface;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *) window;

  if (gdk_window_has_impl (private))
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (private->impl);
      return impl_iface->set_static_gravities (window, use_static);
    }

  return FALSE;
}

/**
 * gdk_window_get_composited:
 * @window: a #GdkWindow
 *
 * Determines whether @window is composited.
 *
 * See gdk_window_set_composited().
 *
 * Returns: %TRUE if the window is composited.
 *
 * Since: 2.22
 **/
gboolean
gdk_window_get_composited (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *)window;

  return private->composited;
}

/**
 * gdk_window_set_composited:
 * @window: a #GdkWindow
 * @composited: %TRUE to set the window as composited
 *
 * Sets a #GdkWindow as composited, or unsets it. Composited
 * windows do not automatically have their contents drawn to
 * the screen. Drawing is redirected to an offscreen buffer
 * and an expose event is emitted on the parent of the composited
 * window. It is the responsibility of the parent's expose handler
 * to manually merge the off-screen content onto the screen in
 * whatever way it sees fit. See <xref linkend="composited-window-example"/>
 * for an example.
 *
 * It only makes sense for child windows to be composited; see
 * gdk_window_set_opacity() if you need translucent toplevel
 * windows.
 *
 * An additional effect of this call is that the area of this
 * window is no longer clipped from regions marked for
 * invalidation on its parent. Draws done on the parent
 * window are also no longer clipped by the child.
 *
 * This call is only supported on some systems (currently,
 * only X11 with new enough Xcomposite and Xdamage extensions).
 * You must call gdk_display_supports_composite() to check if
 * setting a window as composited is supported before
 * attempting to do so.
 *
 * Since: 2.12
 */
void
gdk_window_set_composited (GdkWindow *window,
			   gboolean   composited)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  composited = composited != FALSE;

  if (private->composited == composited)
    return;

  if (composited)
    gdk_window_ensure_native (window);

  display = gdk_drawable_get_display (GDK_DRAWABLE (window));

  if (!gdk_display_supports_composite (display) && composited)
    {
      g_warning ("gdk_window_set_composited called but "
		 "compositing is not supported");
      return;
    }

  _gdk_windowing_window_set_composited (window, composited);

  recompute_visible_regions (private, TRUE, FALSE);

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_window_invalidate_in_parent (private);

  private->composited = composited;
}


static void
remove_redirect_from_children (GdkWindowObject   *private,
			       GdkWindowRedirect *redirect)
{
  GList *l;
  GdkWindowObject *child;

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      /* Don't redirect this child if it already has another redirect */
      if (child->redirect == redirect)
	{
	  child->redirect = NULL;
	  remove_redirect_from_children (child, redirect);
	}
    }
}

/**
 * gdk_window_remove_redirection:
 * @window: a #GdkWindow
 *
 * Removes any active redirection started by
 * gdk_window_redirect_to_drawable().
 *
 * Since: 2.14
 **/
void
gdk_window_remove_redirection (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (private->redirect &&
      private->redirect->redirected == private)
    {
      remove_redirect_from_children (private, private->redirect);
      gdk_window_redirect_free (private->redirect);
      private->redirect = NULL;
    }
}

/**
 * gdk_window_get_modal_hint:
 * @window: A toplevel #GdkWindow.
 *
 * Determines whether or not the window manager is hinted that @window
 * has modal behaviour.
 *
 * Return value: whether or not the window has the modal hint set.
 *
 * Since: 2.22
 */
gboolean
gdk_window_get_modal_hint (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject*) window;

  return private->modal_hint;
}

/**
 * gdk_window_get_accept_focus:
 * @window: a toplevel #GdkWindow.
 *
 * Determines whether or not the desktop environment shuld be hinted that
 * the window does not want to receive input focus.
 *
 * Return value: whether or not the window should receive input focus.
 *
 * Since: 2.22
 */
gboolean
gdk_window_get_accept_focus (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *)window;

  return private->accept_focus;
}

/**
 * gdk_window_get_focus_on_map:
 * @window: a toplevel #GdkWindow.
 *
 * Determines whether or not the desktop environment should be hinted that the
 * window does not want to receive input focus when it is mapped.
 *
 * Return value: whether or not the window wants to receive input focus when
 * it is mapped.
 *
 * Since: 2.22
 */
gboolean
gdk_window_get_focus_on_map (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *)window;

  return private->focus_on_map;
}

/**
 * gdk_window_is_input_only:
 * @window: a toplevel #GdkWindow
 *
 * Determines whether or not the window is an input only window.
 *
 * Return value: %TRUE if @window is input only
 *
 * Since: 2.22
 */
gboolean
gdk_window_is_input_only (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *)window;

  return private->input_only;
}

/**
 * gdk_window_is_shaped:
 * @window: a toplevel #GdkWindow
 *
 * Determines whether or not the window is shaped.
 *
 * Return value: %TRUE if @window is shaped
 *
 * Since: 2.22
 */
gboolean
gdk_window_is_shaped (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *)window;

  return private->shaped;
}

static void
apply_redirect_to_children (GdkWindowObject   *private,
			    GdkWindowRedirect *redirect)
{
  GList *l;
  GdkWindowObject *child;

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      /* Don't redirect this child if it already has another redirect */
      if (!child->redirect)
	{
	  child->redirect = redirect;
	  apply_redirect_to_children (child, redirect);
	}
    }
}

/**
 * gdk_window_redirect_to_drawable:
 * @window: a #GdkWindow
 * @drawable: a #GdkDrawable
 * @src_x: x position in @window
 * @src_y: y position in @window
 * @dest_x: x position in @drawable
 * @dest_y: y position in @drawable
 * @width: width of redirection, or -1 to use the width of @window
 * @height: height of redirection or -1 to use the height of @window
 *
 * Redirects drawing into @window so that drawing to the
 * window in the rectangle specified by @src_x, @src_y,
 * @width and @height is also drawn into @drawable at
 * @dest_x, @dest_y.
 *
 * Only drawing between gdk_window_begin_paint_region() or
 * gdk_window_begin_paint_rect() and gdk_window_end_paint() is
 * redirected.
 *
 * Redirection is active until gdk_window_remove_redirection()
 * is called.
 *
 * Since: 2.14
 **/
void
gdk_window_redirect_to_drawable (GdkWindow   *window,
				 GdkDrawable *drawable,
				 gint         src_x,
				 gint         src_y,
				 gint         dest_x,
				 gint         dest_y,
				 gint         width,
				 gint         height)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  private = (GdkWindowObject *) window;

  if (private->redirect)
    gdk_window_remove_redirection (window);

  if (width == -1 || height == -1)
    {
      gint w, h;
      gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
      if (width == -1)
	width = w;
      if (height == -1)
	height = h;
    }

  private->redirect = g_new0 (GdkWindowRedirect, 1);
  private->redirect->redirected = private;
  private->redirect->pixmap = g_object_ref (drawable);
  private->redirect->src_x = src_x;
  private->redirect->src_y = src_y;
  private->redirect->dest_x = dest_x;
  private->redirect->dest_y = dest_y;
  private->redirect->width = width;
  private->redirect->height = height;

  apply_redirect_to_children (private, private->redirect);
}

static void
window_get_size_rectangle (GdkWindow    *window,
			   GdkRectangle *rect)
{
  GdkWindowObject *private = (GdkWindowObject *) window;

  rect->x = rect->y = 0;
  rect->width = private->width;
  rect->height = private->height;
}

/* Calculates the real clipping region for a window, in window coordinates,
 * taking into account other windows, gc clip region and gc clip mask.
 */
GdkRegion *
_gdk_window_calculate_full_clip_region (GdkWindow *window,
					GdkWindow *base_window,
					gboolean   do_children,
					gint      *base_x_offset,
					gint      *base_y_offset)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkRectangle visible_rect;
  GdkRegion *real_clip_region, *tmpreg;
  gint x_offset, y_offset;
  GdkWindowObject *parentwin, *lastwin;

  if (base_x_offset)
    *base_x_offset = 0;
  if (base_y_offset)
    *base_y_offset = 0;

  if (!private->viewable || private->input_only)
    return gdk_region_new ();

  window_get_size_rectangle (window, &visible_rect);

  /* real_clip_region is in window coordinates */
  real_clip_region = gdk_region_rectangle (&visible_rect);

  x_offset = y_offset = 0;

  lastwin = private;
  if (do_children)
    parentwin = lastwin;
  else
    parentwin = lastwin->parent;

  /* Remove the areas of all overlapping windows above parentwin in the hiearachy */
  for (; parentwin != NULL &&
	 (parentwin == private || lastwin != (GdkWindowObject*) base_window);
       lastwin = parentwin, parentwin = lastwin->parent)
    {
      GList *cur;
      GdkRectangle real_clip_rect;

      if (parentwin != private)
	{
	  x_offset += GDK_WINDOW_OBJECT (lastwin)->x;
	  y_offset += GDK_WINDOW_OBJECT (lastwin)->y;
	}

      /* children is ordered in reverse stack order */
      for (cur = parentwin->children;
	   cur && cur->data != lastwin;
	   cur = cur->next)
	{
	  GdkWindow *child = cur->data;
	  GdkWindowObject *child_private = (GdkWindowObject *)child;

	  if (!GDK_WINDOW_IS_MAPPED (child) || child_private->input_only)
	    continue;

	  /* Ignore offscreen children, as they don't draw in their parent and
	   * don't take part in the clipping */
	  if (gdk_window_is_offscreen (child_private))
	    continue;

	  window_get_size_rectangle (child, &visible_rect);

	  /* Convert rect to "window" coords */
	  visible_rect.x += child_private->x - x_offset;
	  visible_rect.y += child_private->y - y_offset;

	  /* This shortcut is really necessary for performance when there are a lot of windows */
	  gdk_region_get_clipbox (real_clip_region, &real_clip_rect);
	  if (visible_rect.x >= real_clip_rect.x + real_clip_rect.width ||
	      visible_rect.x + visible_rect.width <= real_clip_rect.x ||
	      visible_rect.y >= real_clip_rect.y + real_clip_rect.height ||
	      visible_rect.y + visible_rect.height <= real_clip_rect.y)
	    continue;

	  tmpreg = gdk_region_rectangle (&visible_rect);
	  gdk_region_subtract (real_clip_region, tmpreg);
	  gdk_region_destroy (tmpreg);
	}

      /* Clip to the parent */
      window_get_size_rectangle ((GdkWindow *)parentwin, &visible_rect);
      /* Convert rect to "window" coords */
      visible_rect.x += - x_offset;
      visible_rect.y += - y_offset;

      tmpreg = gdk_region_rectangle (&visible_rect);
      gdk_region_intersect (real_clip_region, tmpreg);
      gdk_region_destroy (tmpreg);
    }

  if (base_x_offset)
    *base_x_offset = x_offset;
  if (base_y_offset)
    *base_y_offset = y_offset;

  return real_clip_region;
}

void
_gdk_window_add_damage (GdkWindow *toplevel,
			GdkRegion *damaged_region)
{
  GdkDisplay *display;
  GdkEvent event = { 0, };
  event.expose.type = GDK_DAMAGE;
  event.expose.window = toplevel;
  event.expose.send_event = FALSE;
  event.expose.region = damaged_region;
  gdk_region_get_clipbox (event.expose.region, &event.expose.area);
  display = gdk_drawable_get_display (event.expose.window);
  _gdk_event_queue_append (display, gdk_event_copy (&event));
}

static void
gdk_window_redirect_free (GdkWindowRedirect *redirect)
{
  g_object_unref (redirect->pixmap);
  g_free (redirect);
}

/* Gets the toplevel for a window as used for events,
   i.e. including offscreen parents */
static GdkWindowObject *
get_event_parent (GdkWindowObject *window)
{
  if (gdk_window_is_offscreen (window))
    return (GdkWindowObject *)gdk_offscreen_window_get_embedder ((GdkWindow *)window);
  else
    return window->parent;
}

/* Gets the toplevel for a window as used for events,
   i.e. including offscreen parents going up to the native
   toplevel */
static GdkWindow *
get_event_toplevel (GdkWindow *w)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (w);
  GdkWindowObject *parent;

  while ((parent = get_event_parent (private)) != NULL &&
	 (parent->window_type != GDK_WINDOW_ROOT))
    private = parent;

  return GDK_WINDOW (private);
}

gboolean
_gdk_window_event_parent_of (GdkWindow *parent,
 	  	             GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = (GdkWindow *)get_event_parent ((GdkWindowObject *)w);
    }

  return FALSE;
}

static void
update_cursor (GdkDisplay *display)
{
  GdkWindowObject *cursor_window, *parent, *toplevel;
  GdkWindow *pointer_window;
  GdkWindowImplIface *impl_iface;
  GdkPointerGrabInfo *grab;

  pointer_window = display->pointer_info.window_under_pointer;

  /* We ignore the serials here and just pick the last grab
     we've sent, as that would shortly be used anyway. */
  grab = _gdk_display_get_last_pointer_grab (display);
  if (/* have grab */
      grab != NULL &&
      /* the pointer is not in a descendant of the grab window */
      !_gdk_window_event_parent_of (grab->window, pointer_window))
    /* use the cursor from the grab window */
    cursor_window = (GdkWindowObject *)grab->window;
  else
    /* otherwise use the cursor from the pointer window */
    cursor_window = (GdkWindowObject *)pointer_window;

  /* Find the first window with the cursor actually set, as
     the cursor is inherited from the parent */
  while (cursor_window->cursor == NULL &&
	 (parent = get_event_parent (cursor_window)) != NULL &&
	 parent->window_type != GDK_WINDOW_ROOT)
    cursor_window = parent;

  /* Set all cursors on toplevel, otherwise its tricky to keep track of
   * which native window has what cursor set. */
  toplevel = (GdkWindowObject *)get_event_toplevel (pointer_window);
  impl_iface = GDK_WINDOW_IMPL_GET_IFACE (toplevel->impl);
  impl_iface->set_cursor ((GdkWindow *)toplevel, cursor_window->cursor);
}

static gboolean
point_in_window (GdkWindowObject *window,
		 gdouble          x,
                 gdouble          y)
{
  return
    x >= 0 && x < window->width &&
    y >= 0 && y < window->height &&
    (window->shape == NULL ||
     gdk_region_point_in (window->shape,
			  x, y)) &&
    (window->input_shape == NULL ||
     gdk_region_point_in (window->input_shape,
			  x, y));
}

static GdkWindow *
convert_native_coords_to_toplevel (GdkWindow *window,
				   gdouble    child_x,
                                   gdouble    child_y,
				   gdouble   *toplevel_x,
                                   gdouble   *toplevel_y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  gdouble x, y;

  x = child_x;
  y = child_y;

  while (!gdk_window_is_toplevel (private))
    {
      x += private->x;
      y += private->y;
      private = private->parent;
    }

  *toplevel_x = x;
  *toplevel_y = y;

  return (GdkWindow *)private;
}

static void
convert_toplevel_coords_to_window (GdkWindow *window,
				   gdouble    toplevel_x,
				   gdouble    toplevel_y,
				   gdouble   *window_x,
				   gdouble   *window_y)
{
  GdkWindowObject *private;
  GdkWindowObject *parent;
  gdouble x, y;
  GList *children, *l;

  private = GDK_WINDOW_OBJECT (window);

  x = toplevel_x;
  y = toplevel_y;

  children = NULL;
  while ((parent = get_event_parent (private)) != NULL &&
	 (parent->window_type != GDK_WINDOW_ROOT))
    {
      children = g_list_prepend (children, private);
      private = parent;
    }

  for (l = children; l != NULL; l = l->next)
    gdk_window_coords_from_parent (l->data, x, y, &x, &y);

  g_list_free (children);

  *window_x = x;
  *window_y = y;
}

static GdkWindowObject *
pick_embedded_child (GdkWindowObject *window,
		     gdouble          x,
                     gdouble          y)
{
  GdkWindowObject *res;

  res = NULL;
  g_signal_emit (window,
		 signals[PICK_EMBEDDED_CHILD], 0,
		 x, y, &res);

  return res;
}

GdkWindow *
_gdk_window_find_child_at (GdkWindow *window,
			   int        x,
                           int        y)
{
  GdkWindowObject *private, *sub;
  double child_x, child_y;
  GList *l;

  private = (GdkWindowObject *)window;

  if (point_in_window (private, x, y))
    {
      /* Children is ordered in reverse stack order, i.e. first is topmost */
      for (l = private->children; l != NULL; l = l->next)
	{
	  sub = l->data;

	  if (!GDK_WINDOW_IS_MAPPED (sub))
	    continue;

	  gdk_window_coords_from_parent ((GdkWindow *)sub,
                                         x, y,
                                         &child_x, &child_y);
	  if (point_in_window (sub, child_x, child_y))
	    return (GdkWindow *)sub;
	}

      if (private->num_offscreen_children > 0)
	{
	  sub = pick_embedded_child (private,
				     x, y);
	  if (sub)
	    return (GdkWindow *)sub;
	}
    }

  return NULL;
}

GdkWindow *
_gdk_window_find_descendant_at (GdkWindow *toplevel,
				gdouble    x,
                                gdouble    y,
				gdouble   *found_x,
				gdouble   *found_y)
{
  GdkWindowObject *private, *sub;
  gdouble child_x, child_y;
  GList *l;
  gboolean found;

  private = (GdkWindowObject *)toplevel;

  if (point_in_window (private, x, y))
    {
      do
	{
	  found = FALSE;
	  /* Children is ordered in reverse stack order, i.e. first is topmost */
	  for (l = private->children; l != NULL; l = l->next)
	    {
	      sub = l->data;

	      if (!GDK_WINDOW_IS_MAPPED (sub))
		continue;

	      gdk_window_coords_from_parent ((GdkWindow *)sub,
                                             x, y,
                                             &child_x, &child_y);
	      if (point_in_window (sub, child_x, child_y))
		{
		  x = child_x;
		  y = child_y;
		  private = sub;
		  found = TRUE;
		  break;
		}
	    }
	  if (!found &&
	      private->num_offscreen_children > 0)
	    {
	      sub = pick_embedded_child (private,
					 x, y);
	      if (sub)
		{
		  found = TRUE;
		  private = sub;
		  from_embedder (sub, x, y, &x, &y);
		}
	    }
	}
      while (found);
    }
  else
    {
      /* Not in window at all */
      private = NULL;
    }

  if (found_x)
    *found_x = x;
  if (found_y)
    *found_y = y;

  return (GdkWindow *)private;
}

/**
 * gdk_window_beep:
 * @window: a toplevel #GdkWindow
 *
 * Emits a short beep associated to @window in the appropriate
 * display, if supported. Otherwise, emits a short beep on
 * the display just as gdk_display_beep().
 *
 * Since: 2.12
 **/
void
gdk_window_beep (GdkWindow *window)
{
  GdkDisplay *display;
  GdkWindow *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = get_event_toplevel (window);
  display = gdk_drawable_get_display (GDK_DRAWABLE (window));

  if (toplevel && !gdk_window_is_offscreen ((GdkWindowObject *)toplevel))
    _gdk_windowing_window_beep (toplevel);
  else
    gdk_display_beep (display);
}

static const guint type_masks[] = {
  GDK_SUBSTRUCTURE_MASK, /* GDK_DELETE                 = 0  */
  GDK_STRUCTURE_MASK, /* GDK_DESTROY                   = 1  */
  GDK_EXPOSURE_MASK, /* GDK_EXPOSE                     = 2  */
  GDK_POINTER_MOTION_MASK, /* GDK_MOTION_NOTIFY        = 3  */
  GDK_BUTTON_PRESS_MASK, /* GDK_BUTTON_PRESS           = 4  */
  GDK_BUTTON_PRESS_MASK, /* GDK_2BUTTON_PRESS          = 5  */
  GDK_BUTTON_PRESS_MASK, /* GDK_3BUTTON_PRESS          = 6  */
  GDK_BUTTON_RELEASE_MASK, /* GDK_BUTTON_RELEASE       = 7  */
  GDK_KEY_PRESS_MASK, /* GDK_KEY_PRESS                 = 8  */
  GDK_KEY_RELEASE_MASK, /* GDK_KEY_RELEASE             = 9  */
  GDK_ENTER_NOTIFY_MASK, /* GDK_ENTER_NOTIFY           = 10 */
  GDK_LEAVE_NOTIFY_MASK, /* GDK_LEAVE_NOTIFY           = 11 */
  GDK_FOCUS_CHANGE_MASK, /* GDK_FOCUS_CHANGE           = 12 */
  GDK_STRUCTURE_MASK, /* GDK_CONFIGURE                 = 13 */
  GDK_VISIBILITY_NOTIFY_MASK, /* GDK_MAP               = 14 */
  GDK_VISIBILITY_NOTIFY_MASK, /* GDK_UNMAP             = 15 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_PROPERTY_NOTIFY     = 16 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_CLEAR     = 17 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_REQUEST   = 18 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_NOTIFY    = 19 */
  GDK_PROXIMITY_IN_MASK, /* GDK_PROXIMITY_IN           = 20 */
  GDK_PROXIMITY_OUT_MASK, /* GDK_PROXIMITY_OUT         = 21 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_ENTER               = 22 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_LEAVE               = 23 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_MOTION              = 24 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_STATUS              = 25 */
  GDK_ALL_EVENTS_MASK, /* GDK_DROP_START               = 26 */
  GDK_ALL_EVENTS_MASK, /* GDK_DROP_FINISHED            = 27 */
  GDK_ALL_EVENTS_MASK, /* GDK_CLIENT_EVENT	       = 28 */
  GDK_VISIBILITY_NOTIFY_MASK, /* GDK_VISIBILITY_NOTIFY = 29 */
  GDK_EXPOSURE_MASK, /* GDK_NO_EXPOSE                  = 30 */
  GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK,/* GDK_SCROLL= 31 */
  0, /* GDK_WINDOW_STATE = 32 */
  0, /* GDK_SETTING = 33 */
  0, /* GDK_OWNER_CHANGE = 34 */
  0, /* GDK_GRAB_BROKEN = 35 */
  0, /* GDK_DAMAGE = 36 */
};
G_STATIC_ASSERT (G_N_ELEMENTS (type_masks) == GDK_EVENT_LAST);

/* send motion events if the right buttons are down */
static guint
update_evmask_for_button_motion (guint           evmask,
				 GdkModifierType mask)
{
  if (evmask & GDK_BUTTON_MOTION_MASK &&
      mask & (GDK_BUTTON1_MASK |
	      GDK_BUTTON2_MASK |
	      GDK_BUTTON3_MASK |
	      GDK_BUTTON4_MASK |
	      GDK_BUTTON5_MASK))
    evmask |= GDK_POINTER_MOTION_MASK;

  if ((evmask & GDK_BUTTON1_MOTION_MASK && mask & GDK_BUTTON1_MASK) ||
      (evmask & GDK_BUTTON2_MOTION_MASK && mask & GDK_BUTTON2_MASK) ||
      (evmask & GDK_BUTTON3_MOTION_MASK && mask & GDK_BUTTON3_MASK))
    evmask |= GDK_POINTER_MOTION_MASK;

  return evmask;
}

static gboolean
is_button_type (GdkEventType type)
{
  return type == GDK_BUTTON_PRESS ||
	 type == GDK_2BUTTON_PRESS ||
	 type == GDK_3BUTTON_PRESS ||
	 type == GDK_BUTTON_RELEASE ||
	 type == GDK_SCROLL;
}

static gboolean
is_motion_type (GdkEventType type)
{
  return type == GDK_MOTION_NOTIFY ||
	 type == GDK_ENTER_NOTIFY ||
	 type == GDK_LEAVE_NOTIFY;
}

static GdkWindowObject *
find_common_ancestor (GdkWindowObject *win1,
		      GdkWindowObject *win2)
{
  GdkWindowObject *tmp;
  GList *path1 = NULL, *path2 = NULL;
  GList *list1, *list2;

  tmp = win1;
  while (tmp != NULL && tmp->window_type != GDK_WINDOW_ROOT)
    {
      path1 = g_list_prepend (path1, tmp);
      tmp = get_event_parent (tmp);
    }

  tmp = win2;
  while (tmp != NULL && tmp->window_type != GDK_WINDOW_ROOT)
    {
      path2 = g_list_prepend (path2, tmp);
      tmp = get_event_parent (tmp);
    }

  list1 = path1;
  list2 = path2;
  tmp = NULL;
  while (list1 && list2 && (list1->data == list2->data))
    {
      tmp = (GdkWindowObject *)list1->data;
      list1 = g_list_next (list1);
      list2 = g_list_next (list2);
    }
  g_list_free (path1);
  g_list_free (path2);

  return tmp;
}

GdkEvent *
_gdk_make_event (GdkWindow    *window,
		 GdkEventType  type,
		 GdkEvent     *event_in_queue,
		 gboolean      before_event)
{
  GdkEvent *event = gdk_event_new (type);
  guint32 the_time;
  GdkModifierType the_state;

  the_time = gdk_event_get_time (event_in_queue);
  gdk_event_get_state (event_in_queue, &the_state);

  event->any.window = g_object_ref (window);
  event->any.send_event = FALSE;
  if (event_in_queue && event_in_queue->any.send_event)
    event->any.send_event = TRUE;

  switch (type)
    {
    case GDK_MOTION_NOTIFY:
      event->motion.time = the_time;
      event->motion.axes = NULL;
      event->motion.state = the_state;
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.time = the_time;
      event->button.axes = NULL;
      event->button.state = the_state;
      break;

    case GDK_SCROLL:
      event->scroll.time = the_time;
      event->scroll.state = the_state;
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      event->key.time = the_time;
      event->key.state = the_state;
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event->crossing.time = the_time;
      event->crossing.state = the_state;
      break;

    case GDK_PROPERTY_NOTIFY:
      event->property.time = the_time;
      event->property.state = the_state;
      break;

    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
      event->selection.time = the_time;
      break;

    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      event->proximity.time = the_time;
      break;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      event->dnd.time = the_time;
      break;

    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_MAP:
    case GDK_UNMAP:
    case GDK_CLIENT_EVENT:
    case GDK_VISIBILITY_NOTIFY:
    case GDK_NO_EXPOSE:
    case GDK_DELETE:
    case GDK_DESTROY:
    case GDK_EXPOSE:
    default:
      break;
    }

  if (event_in_queue)
    {
    if (before_event)
      _gdk_event_queue_insert_before (gdk_drawable_get_display (window), event_in_queue, event);
    else
      _gdk_event_queue_insert_after (gdk_drawable_get_display (window), event_in_queue, event);
    }
  else
    _gdk_event_queue_append (gdk_drawable_get_display (window), event);

  return event;
}

static void
send_crossing_event (GdkDisplay                 *display,
		     GdkWindowObject            *toplevel,
		     GdkWindowObject            *window,
		     GdkEventType                type,
		     GdkCrossingMode             mode,
		     GdkNotifyType               notify_type,
		     GdkWindow                  *subwindow,
		     gint                        toplevel_x,
		     gint                        toplevel_y,
		     GdkModifierType             mask,
		     guint32                     time_,
		     GdkEvent                   *event_in_queue,
		     gulong                      serial)
{
  GdkEvent *event;
  guint32 window_event_mask, type_event_mask;
  GdkPointerGrabInfo *grab;
  GdkWindowImplIface *impl_iface;

  grab = _gdk_display_has_pointer_grab (display, serial);

  if (grab != NULL &&
      !grab->owner_events)
    {
      /* !owner_event => only report events wrt grab window, ignore rest */
      if ((GdkWindow *)window != grab->window)
	return;
      window_event_mask = grab->event_mask;
    }
  else
    window_event_mask = window->event_mask;

  if (type == GDK_LEAVE_NOTIFY)
    type_event_mask = GDK_LEAVE_NOTIFY_MASK;
  else
    type_event_mask = GDK_ENTER_NOTIFY_MASK;

  if (window->extension_events != 0)
    {
      impl_iface = GDK_WINDOW_IMPL_GET_IFACE (window->impl);
      impl_iface->input_window_crossing ((GdkWindow *)window,
					 type == GDK_ENTER_NOTIFY);
    }

  if (window_event_mask & type_event_mask)
    {
      event = _gdk_make_event ((GdkWindow *)window, type, event_in_queue, TRUE);
      event->crossing.time = time_;
      event->crossing.subwindow = subwindow;
      if (subwindow)
	g_object_ref (subwindow);
      convert_toplevel_coords_to_window ((GdkWindow *)window,
					 toplevel_x, toplevel_y,
					 &event->crossing.x, &event->crossing.y);
      event->crossing.x_root = toplevel_x + toplevel->x;
      event->crossing.y_root = toplevel_y + toplevel->y;
      event->crossing.mode = mode;
      event->crossing.detail = notify_type;
      event->crossing.focus = FALSE;
      event->crossing.state = mask;
    }
}


/* The coordinates are in the toplevel window that src/dest are in.
 * src and dest are always (if != NULL) in the same toplevel, as
 * we get a leave-notify and set the window_under_pointer to null
 * before crossing to another toplevel.
 */
void
_gdk_synthesize_crossing_events (GdkDisplay                 *display,
				 GdkWindow                  *src,
				 GdkWindow                  *dest,
				 GdkCrossingMode             mode,
				 gint                        toplevel_x,
				 gint                        toplevel_y,
				 GdkModifierType             mask,
				 guint32                     time_,
				 GdkEvent                   *event_in_queue,
				 gulong                      serial,
				 gboolean                    non_linear)
{
  GdkWindowObject *c;
  GdkWindowObject *win, *last, *next;
  GList *path, *list;
  GdkWindowObject *a;
  GdkWindowObject *b;
  GdkWindowObject *toplevel;
  GdkNotifyType notify_type;

  /* TODO: Don't send events to toplevel, as we get those from the windowing system */

  a = (GdkWindowObject *)src;
  b = (GdkWindowObject *)dest;
  if (a == b)
    return; /* No crossings generated between src and dest */

  c = find_common_ancestor (a, b);

  non_linear |= (c != a) && (c != b);

  if (a) /* There might not be a source (i.e. if no previous pointer_in_window) */
    {
      toplevel = (GdkWindowObject *)gdk_window_get_toplevel ((GdkWindow *)a);

      /* Traverse up from a to (excluding) c sending leave events */
      if (non_linear)
	notify_type = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
	notify_type = GDK_NOTIFY_INFERIOR;
      else
	notify_type = GDK_NOTIFY_ANCESTOR;
      send_crossing_event (display, toplevel,
			   a, GDK_LEAVE_NOTIFY,
			   mode,
			   notify_type,
			   NULL,
			   toplevel_x, toplevel_y,
			   mask, time_,
			   event_in_queue,
			   serial);

      if (c != a)
	{
	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  last = a;
	  win = get_event_parent (a);
	  while (win != c && win->window_type != GDK_WINDOW_ROOT)
	    {
	      send_crossing_event (display, toplevel,
				   win, GDK_LEAVE_NOTIFY,
				   mode,
				   notify_type,
				   (GdkWindow *)last,
				   toplevel_x, toplevel_y,
				   mask, time_,
				   event_in_queue,
				   serial);

	      last = win;
	      win = get_event_parent (win);
	    }
	}
    }

  if (b) /* Might not be a dest, e.g. if we're moving out of the window */
    {
      toplevel = (GdkWindowObject *)gdk_window_get_toplevel ((GdkWindow *)b);

      /* Traverse down from c to b */
      if (c != b)
	{
	  path = NULL;
	  win = get_event_parent (b);
	  while (win != c && win->window_type != GDK_WINDOW_ROOT)
	    {
	      path = g_list_prepend (path, win);
	      win = get_event_parent (win);
	    }

	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  list = path;
	  while (list)
	    {
	      win = (GdkWindowObject *)list->data;
	      list = g_list_next (list);
	      if (list)
		next = (GdkWindowObject *)list->data;
	      else
		next = b;

	      send_crossing_event (display, toplevel,
				   win, GDK_ENTER_NOTIFY,
				   mode,
				   notify_type,
				   (GdkWindow *)next,
				   toplevel_x, toplevel_y,
				   mask, time_,
				   event_in_queue,
				   serial);
	    }
	  g_list_free (path);
	}


      if (non_linear)
	notify_type = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
	notify_type = GDK_NOTIFY_ANCESTOR;
      else
	notify_type = GDK_NOTIFY_INFERIOR;

      send_crossing_event (display, toplevel,
			   b, GDK_ENTER_NOTIFY,
			   mode,
			   notify_type,
			   NULL,
			   toplevel_x, toplevel_y,
			   mask, time_,
			   event_in_queue,
			   serial);
    }
}

/* Returns the window inside the event window with the pointer in it
 * at the specified coordinates, or NULL if its not in any child of
 * the toplevel. It also takes into account !owner_events grabs.
 */
static GdkWindow *
get_pointer_window (GdkDisplay *display,
		    GdkWindow *event_window,
		    gdouble toplevel_x,
		    gdouble toplevel_y,
		    gulong serial)
{
  GdkWindow *pointer_window;
  GdkPointerGrabInfo *grab;

  if (event_window == display->pointer_info.toplevel_under_pointer)
    pointer_window =
      _gdk_window_find_descendant_at (event_window,
				      toplevel_x, toplevel_y,
				      NULL, NULL);
  else
    pointer_window = NULL;

  grab = _gdk_display_has_pointer_grab (display, serial);
  if (grab != NULL &&
      !grab->owner_events &&
      pointer_window != grab->window)
    pointer_window = NULL;

  return pointer_window;
}

void
_gdk_display_set_window_under_pointer (GdkDisplay *display,
				       GdkWindow *window)
{
  /* We don't track this if all native, and it can cause issues
     with the update_cursor call below */
  if (_gdk_native_windows)
    return;

  if (display->pointer_info.window_under_pointer)
    g_object_unref (display->pointer_info.window_under_pointer);
  display->pointer_info.window_under_pointer = window;
  if (window)
    g_object_ref (window);

  if (window)
    update_cursor (display);

  _gdk_display_enable_motion_hints (display);
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_grab
 *
 *   Grabs the pointer to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "event_mask" masks only interesting events
 *   "confine_to" limits the cursor movement to the specified window
 *   "cursor" changes the cursor for the duration of the grab
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_pointer_ungrab
 *
 *--------------------------------------------------------------
 */
GdkGrabStatus
gdk_pointer_grab (GdkWindow *	  window,
		  gboolean	  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  GdkWindow *native;
  GdkDisplay *display;
  GdkGrabStatus res;
  gulong serial;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  /* We need a native window for confine to to work, ensure we have one */
  if (confine_to)
    {
      if (!gdk_window_ensure_native (confine_to))
	{
	  g_warning ("Can't confine to grabbed window, not native");
	  confine_to = NULL;
	}
    }

  /* Non-viewable client side window => fail */
  if (!_gdk_window_has_impl (window) &&
      !gdk_window_is_viewable (window))
    return GDK_GRAB_NOT_VIEWABLE;

  if (_gdk_native_windows)
    native = window;
  else
    native = gdk_window_get_toplevel (window);
  while (gdk_window_is_offscreen ((GdkWindowObject *)native))
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
	  (!_gdk_window_has_impl (native) &&
	   !gdk_window_is_viewable (native)))
	return GDK_GRAB_NOT_VIEWABLE;

      native = gdk_window_get_toplevel (native);
    }

  display = gdk_drawable_get_display (window);

  serial = _gdk_windowing_window_get_next_serial (display);

  res = _gdk_windowing_pointer_grab (window,
				     native,
				     owner_events,
				     get_native_grab_event_mask (event_mask),
				     confine_to,
				     cursor,
				     time);

  if (res == GDK_GRAB_SUCCESS)
    _gdk_display_add_pointer_grab (display,
				   window,
				   native,
				   owner_events,
				   event_mask,
				   serial,
				   time,
				   FALSE);

  return res;
}

/**
 * gdk_window_geometry_changed:
 * @window: an embedded offscreen #GdkWindow
 *
 * This function informs GDK that the geometry of an embedded
 * offscreen window has changed. This is necessary for GDK to keep
 * track of which offscreen window the pointer is in.
 *
 * Since: 2.18
 */
void
gdk_window_geometry_changed (GdkWindow *window)
{
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static gboolean
do_synthesize_crossing_event (gpointer data)
{
  GdkDisplay *display;
  GdkWindow *changed_toplevel;
  GdkWindowObject *changed_toplevel_priv;
  GdkWindow *new_window_under_pointer;
  gulong serial;

  changed_toplevel = data;
  changed_toplevel_priv = (GdkWindowObject *)changed_toplevel;

  changed_toplevel_priv->synthesize_crossing_event_queued = FALSE;

  if (GDK_WINDOW_DESTROYED (changed_toplevel))
    return FALSE;

  display = gdk_drawable_get_display (changed_toplevel);
  serial = _gdk_windowing_window_get_next_serial (display);

  if (changed_toplevel == display->pointer_info.toplevel_under_pointer)
    {
      new_window_under_pointer =
	get_pointer_window (display, changed_toplevel,
			    display->pointer_info.toplevel_x,
			    display->pointer_info.toplevel_y,
			    serial);
      if (new_window_under_pointer !=
	  display->pointer_info.window_under_pointer)
	{
	  _gdk_synthesize_crossing_events (display,
					   display->pointer_info.window_under_pointer,
					   new_window_under_pointer,
					   GDK_CROSSING_NORMAL,
					   display->pointer_info.toplevel_x,
					   display->pointer_info.toplevel_y,
					   display->pointer_info.state,
					   GDK_CURRENT_TIME,
					   NULL,
					   serial,
					   FALSE);
	  _gdk_display_set_window_under_pointer (display, new_window_under_pointer);
	}
    }

  return FALSE;
}

void
_gdk_synthesize_crossing_events_for_geometry_change (GdkWindow *changed_window)
{
  GdkDisplay *display;
  GdkWindow *toplevel;
  GdkWindowObject *toplevel_priv;

  if (_gdk_native_windows)
    return; /* We use the native crossing events if all native */

  display = gdk_drawable_get_display (changed_window);

  toplevel = get_event_toplevel (changed_window);
  toplevel_priv = (GdkWindowObject *)toplevel;

  if (toplevel == display->pointer_info.toplevel_under_pointer &&
      !toplevel_priv->synthesize_crossing_event_queued)
    {
      toplevel_priv->synthesize_crossing_event_queued = TRUE;
      gdk_threads_add_idle_full (GDK_PRIORITY_EVENTS - 1,
                                 do_synthesize_crossing_event,
                                 g_object_ref (toplevel),
                                 g_object_unref);
    }
}

/* Don't use for crossing events */
static GdkWindow *
get_event_window (GdkDisplay                 *display,
		  GdkWindow                  *pointer_window,
		  GdkEventType                type,
		  GdkModifierType             mask,
		  guint                      *evmask_out,
		  gulong                      serial)
{
  guint evmask;
  GdkWindow *grab_window;
  GdkWindowObject *w;
  GdkPointerGrabInfo *grab;

  grab = _gdk_display_has_pointer_grab (display, serial);

  if (grab != NULL && !grab->owner_events)
    {
      evmask = grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      grab_window = grab->window;

      if (evmask & type_masks[type])
	{
	  if (evmask_out)
	    *evmask_out = evmask;
	  return grab_window;
	}
      else
	return NULL;
    }

  w = (GdkWindowObject *)pointer_window;
  while (w != NULL)
    {
      evmask = w->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & type_masks[type])
	{
	  if (evmask_out)
	    *evmask_out = evmask;
	  return (GdkWindow *)w;
	}

      w = get_event_parent (w);
    }

  if (grab != NULL &&
      grab->owner_events)
    {
      evmask = grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & type_masks[type])
	{
	  if (evmask_out)
	    *evmask_out = evmask;
	  return grab->window;
	}
      else
	return NULL;
    }

  return NULL;
}

static gboolean
proxy_pointer_event (GdkDisplay                 *display,
		     GdkEvent                   *source_event,
		     gulong                      serial)
{
  GdkWindow *toplevel_window, *event_window;
  GdkWindow *pointer_window;
  GdkEvent *event;
  guint state;
  gdouble toplevel_x, toplevel_y;
  guint32 time_;
  gboolean non_linear;

  event_window = source_event->any.window;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  time_ = gdk_event_get_time (source_event);
  toplevel_window = convert_native_coords_to_toplevel (event_window,
						       toplevel_x, toplevel_y,
						       &toplevel_x, &toplevel_y);

  non_linear = FALSE;
  if ((source_event->type == GDK_LEAVE_NOTIFY ||
       source_event->type == GDK_ENTER_NOTIFY) &&
      (source_event->crossing.detail == GDK_NOTIFY_NONLINEAR ||
       source_event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL))
    non_linear = TRUE;

  /* If we get crossing events with subwindow unexpectedly being NULL
     that means there is a native subwindow that gdk doesn't know about.
     We track these and forward them, with the correct virtual window
     events inbetween.
     This is important to get right, as metacity uses gdk for the frame
     windows, but gdk doesn't know about the client windows reparented
     into the frame. */
  if (((source_event->type == GDK_LEAVE_NOTIFY &&
	source_event->crossing.detail == GDK_NOTIFY_INFERIOR) ||
       (source_event->type == GDK_ENTER_NOTIFY &&
	(source_event->crossing.detail == GDK_NOTIFY_VIRTUAL ||
	 source_event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL))) &&
      source_event->crossing.subwindow == NULL)
    {
      /* Left for an unknown (to gdk) subwindow */

      /* Send leave events from window under pointer to event window
	 that will get the subwindow == NULL window */
      _gdk_synthesize_crossing_events (display,
				       display->pointer_info.window_under_pointer,
				       event_window,
				       source_event->crossing.mode,
				       toplevel_x, toplevel_y,
				       state, time_,
				       source_event,
				       serial,
				       non_linear);

      /* Send subwindow == NULL event */
      send_crossing_event (display,
			   (GdkWindowObject *)toplevel_window,
			   (GdkWindowObject *)event_window,
			   source_event->type,
			   source_event->crossing.mode,
			   source_event->crossing.detail,
			   NULL,
			   toplevel_x,   toplevel_y,
			   state, time_,
			   source_event,
			   serial);

      _gdk_display_set_window_under_pointer (display, NULL);
      return TRUE;
    }

  pointer_window = get_pointer_window (display, toplevel_window,
				       toplevel_x, toplevel_y, serial);

  if (((source_event->type == GDK_ENTER_NOTIFY &&
	source_event->crossing.detail == GDK_NOTIFY_INFERIOR) ||
       (source_event->type == GDK_LEAVE_NOTIFY &&
	(source_event->crossing.detail == GDK_NOTIFY_VIRTUAL ||
	 source_event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL))) &&
      source_event->crossing.subwindow == NULL)
    {
      /* Entered from an unknown (to gdk) subwindow */

      /* Send subwindow == NULL event */
      send_crossing_event (display,
			   (GdkWindowObject *)toplevel_window,
			   (GdkWindowObject *)event_window,
			   source_event->type,
			   source_event->crossing.mode,
			   source_event->crossing.detail,
			   NULL,
			   toplevel_x,   toplevel_y,
			   state, time_,
			   source_event,
			   serial);

      /* Send enter events from event window to pointer_window */
      _gdk_synthesize_crossing_events (display,
				       event_window,
				       pointer_window,
				       source_event->crossing.mode,
				       toplevel_x, toplevel_y,
				       state, time_,
				       source_event,
				       serial, non_linear);
      _gdk_display_set_window_under_pointer (display, pointer_window);
      return TRUE;
    }

  if (display->pointer_info.window_under_pointer != pointer_window)
    {
      /* Either a toplevel crossing notify that ended up inside a child window,
	 or a motion notify that got into another child window  */

      /* Different than last time, send crossing events */
      _gdk_synthesize_crossing_events (display,
				       display->pointer_info.window_under_pointer,
				       pointer_window,
				       GDK_CROSSING_NORMAL,
				       toplevel_x, toplevel_y,
				       state, time_,
				       source_event,
				       serial, non_linear);
      _gdk_display_set_window_under_pointer (display, pointer_window);
    }
  else if (source_event->type == GDK_MOTION_NOTIFY)
    {
      GdkWindow *event_win;
      guint evmask;
      gboolean is_hint;

      event_win = get_event_window (display,
				    pointer_window,
				    source_event->type,
				    state,
				    &evmask,
				    serial);

      is_hint = FALSE;

      if (event_win &&
	  (evmask & GDK_POINTER_MOTION_HINT_MASK))
	{
	  if (display->pointer_info.motion_hint_serial != 0 &&
	      serial < display->pointer_info.motion_hint_serial)
	    event_win = NULL; /* Ignore event */
	  else
	    {
	      is_hint = TRUE;
	      display->pointer_info.motion_hint_serial = G_MAXULONG;
	    }
	}

      if (event_win && !display->ignore_core_events)
	{
	  event = _gdk_make_event (event_win, GDK_MOTION_NOTIFY, source_event, FALSE);
	  event->motion.time = time_;
	  convert_toplevel_coords_to_window (event_win,
					     toplevel_x, toplevel_y,
					     &event->motion.x, &event->motion.y);
	  event->motion.x_root = source_event->motion.x_root;
	  event->motion.y_root = source_event->motion.y_root;;
	  event->motion.state = state;
	  event->motion.is_hint = is_hint;
	  event->motion.device = NULL;
	  event->motion.device = source_event->motion.device;
	}
    }

  /* unlink all move events from queue.
     We handle our own, including our emulated masks. */
  return TRUE;
}

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
			     GDK_BUTTON2_MASK | \
			     GDK_BUTTON3_MASK | \
			     GDK_BUTTON4_MASK | \
			     GDK_BUTTON5_MASK)

static gboolean
proxy_button_event (GdkEvent *source_event,
		    gulong serial)
{
  GdkWindow *toplevel_window, *event_window;
  GdkWindow *event_win;
  GdkWindow *pointer_window;
  GdkWindowObject *parent;
  GdkEvent *event;
  guint state;
  guint32 time_;
  GdkEventType type;
  gdouble toplevel_x, toplevel_y;
  GdkDisplay *display;
  GdkWindowObject *w;

  type = source_event->any.type;
  event_window = source_event->any.window;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  time_ = gdk_event_get_time (source_event);
  display = gdk_drawable_get_display (source_event->any.window);
  toplevel_window = convert_native_coords_to_toplevel (event_window,
						       toplevel_x, toplevel_y,
						       &toplevel_x, &toplevel_y);

  if (type == GDK_BUTTON_PRESS &&
      !source_event->any.send_event &&
      _gdk_display_has_pointer_grab (display, serial) == NULL)
    {
      pointer_window =
	_gdk_window_find_descendant_at (toplevel_window,
					toplevel_x, toplevel_y,
					NULL, NULL);

      /* Find the event window, that gets the grab */
      w = (GdkWindowObject *)pointer_window;
      while (w != NULL &&
	     (parent = get_event_parent (w)) != NULL &&
	     parent->window_type != GDK_WINDOW_ROOT)
	{
	  if (w->event_mask & GDK_BUTTON_PRESS_MASK)
	    break;
	  w = parent;
	}
      pointer_window = (GdkWindow *)w;

      _gdk_display_add_pointer_grab  (display,
				      pointer_window,
				      event_window,
				      FALSE,
				      gdk_window_get_events (pointer_window),
				      serial,
				      time_,
				      TRUE);
      _gdk_display_pointer_grab_update (display, serial);
    }

  pointer_window = get_pointer_window (display, toplevel_window,
				       toplevel_x, toplevel_y,
				       serial);

  event_win = get_event_window (display,
				pointer_window,
				type, state,
				NULL, serial);

  if (event_win == NULL || display->ignore_core_events)
    return TRUE;

  event = _gdk_make_event (event_win, type, source_event, FALSE);

  switch (type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.button = source_event->button.button;
      convert_toplevel_coords_to_window (event_win,
					 toplevel_x, toplevel_y,
					 &event->button.x, &event->button.y);
      event->button.x_root = source_event->button.x_root;
      event->button.y_root = source_event->button.y_root;
      event->button.state = state;
      event->button.device = source_event->button.device;

      if (type == GDK_BUTTON_PRESS)
	_gdk_event_button_generate (display, event);
      return TRUE;

    case GDK_SCROLL:
      event->scroll.direction = source_event->scroll.direction;
      convert_toplevel_coords_to_window (event_win,
					 toplevel_x, toplevel_y,
					 &event->scroll.x, &event->scroll.y);
      event->scroll.x_root = source_event->scroll.x_root;
      event->scroll.y_root = source_event->scroll.y_root;
      event->scroll.state = state;
      event->scroll.device = source_event->scroll.device;
      event->scroll.has_deltas = source_event->scroll.has_deltas;
      event->scroll.delta_x = source_event->scroll.delta_x;
      event->scroll.delta_y = source_event->scroll.delta_y;
      return TRUE;

    default:
      return FALSE;
    }

  return TRUE; /* Always unlink original, we want to obey the emulated event mask */
}

#ifdef DEBUG_WINDOW_PRINTING
static void
gdk_window_print (GdkWindowObject *window,
		  int indent)
{
  GdkRectangle r;
  const char *window_types[] = {
    "root",
    "toplevel",
    "child",
    "dialog",
    "temp",
    "foreign",
    "offscreen"
  };

  g_print ("%*s%p: [%s] %d,%d %dx%d", indent, "", window,
	   window->user_data ? g_type_name_from_instance (window->user_data) : "no widget",
	   window->x, window->y,
	   window->width, window->height
	   );

  if (gdk_window_has_impl (window))
    {
#ifdef GDK_WINDOWING_X11
      g_print (" impl(0x%lx)", gdk_x11_drawable_get_xid (GDK_DRAWABLE (window)));
#endif
    }

  if (window->window_type != GDK_WINDOW_CHILD)
    g_print (" %s", window_types[window->window_type]);

  if (window->input_only)
    g_print (" input-only");

  if (window->shaped)
    g_print (" shaped");

  if (!gdk_window_is_visible ((GdkWindow *)window))
    g_print (" hidden");

  g_print (" abs[%d,%d]",
	   window->abs_x, window->abs_y);

  gdk_region_get_clipbox (window->clip_region, &r);
  if (gdk_region_empty (window->clip_region))
    g_print (" clipbox[empty]");
  else
    g_print (" clipbox[%d,%d %dx%d]", r.x, r.y, r.width, r.height);

  g_print ("\n");
}


static void
gdk_window_print_tree (GdkWindow *window,
		       int indent,
		       gboolean include_input_only)
{
  GdkWindowObject *private;
  GList *l;

  private = (GdkWindowObject *)window;

  if (private->input_only && !include_input_only)
    return;

  gdk_window_print (private, indent);

  for (l = private->children; l != NULL; l = l->next)
    gdk_window_print_tree (l->data, indent + 4, include_input_only);
}

#endif /* DEBUG_WINDOW_PRINTING */

static gboolean
is_input_event (GdkDisplay *display,
		GdkEvent *event)
{
  GdkDevice *core_pointer;

  core_pointer = gdk_display_get_core_pointer (display);
  if ((event->type == GDK_MOTION_NOTIFY &&
       event->motion.device != core_pointer) ||
      ((event->type == GDK_BUTTON_PRESS ||
	event->type == GDK_BUTTON_RELEASE) &&
       event->button.device != core_pointer))
    return TRUE;
  return FALSE;
}

void
_gdk_windowing_got_event (GdkDisplay *display,
			  GList      *event_link,
			  GdkEvent   *event,
			  gulong      serial)
{
  GdkWindow *event_window;
  GdkWindowObject *event_private;
  gdouble x, y;
  gboolean unlink_event;
  guint old_state, old_button;
  GdkPointerGrabInfo *button_release_grab;
  gboolean is_toplevel;

  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    display->last_event_time = gdk_event_get_time (event);

  _gdk_display_pointer_grab_update (display,
				    serial);

  event_window = event->any.window;
  if (!event_window)
    return;

  event_private = GDK_WINDOW_OBJECT (event_window);

#ifdef DEBUG_WINDOW_PRINTING
  if (event->type == GDK_KEY_PRESS &&
      (event->key.keyval == 0xa7 ||
       event->key.keyval == 0xbd))
    {
      gdk_window_print_tree (event_window, 0,
			     event->key.keyval == 0xbd);
    }
#endif

  if (_gdk_native_windows)
    {
      if (event->type == GDK_BUTTON_PRESS &&
	  !event->any.send_event &&
	  _gdk_display_has_pointer_grab (display, serial) == NULL)
	{
	  _gdk_display_add_pointer_grab  (display,
					  event_window,
					  event_window,
					  FALSE,
					  gdk_window_get_events (event_window),
					  serial,
					  gdk_event_get_time (event),
					  TRUE);
	  _gdk_display_pointer_grab_update (display,
					    serial);
	}
      if (event->type == GDK_BUTTON_RELEASE &&
	  !event->any.send_event)
	{
	  button_release_grab =
	    _gdk_display_has_pointer_grab (display, serial);
	  if (button_release_grab &&
	      button_release_grab->implicit &&
	      (event->button.state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (event->button.button - 1))) == 0)
	    {
	      button_release_grab->serial_end = serial;
	      button_release_grab->implicit_ungrab = FALSE;
	      _gdk_display_pointer_grab_update (display, serial);
	    }
	}

      if (event->type == GDK_BUTTON_PRESS)
	_gdk_event_button_generate (display, event);

      return;
    }

  if (event->type == GDK_VISIBILITY_NOTIFY)
    {
      event_private->native_visibility = event->visibility.state;
      gdk_window_update_visibility_recursively (event_private,
						event_private);
      return;
    }

  if (is_input_event (display, event))
    return;

  if (!(is_button_type (event->type) ||
	is_motion_type (event->type)) ||
      event_private->window_type == GDK_WINDOW_ROOT)
    return;

  is_toplevel = gdk_window_is_toplevel (event_private);

  if ((event->type == GDK_ENTER_NOTIFY ||
       event->type == GDK_LEAVE_NOTIFY) &&
      (event->crossing.mode == GDK_CROSSING_GRAB ||
       event->crossing.mode == GDK_CROSSING_UNGRAB) &&
      (_gdk_display_has_pointer_grab (display, serial) ||
       event->crossing.detail == GDK_NOTIFY_INFERIOR))
    {
      /* We synthesize all crossing events due to grabs ourselves,
       * so we ignore the native ones caused by our native pointer_grab
       * calls. Otherwise we would proxy these crossing event and cause
       * multiple copies of crossing events for grabs.
       *
       * We do want to handle grabs from other clients though, as for
       * instance alt-tab in metacity causes grabs like these and
       * we want to handle those. Thus the has_pointer_grab check.
       *
       * Implicit grabs on child windows create some grabbing events
       * that are sent before the button press. This means we can't
       * detect these with the has_pointer_grab check (as the implicit
       * grab is only noticed when we get button press event), so we
       * detect these events by checking for INFERIOR enter or leave
       * events. These should never be a problem to filter out.
       */

      /* We ended up in this window after some (perhaps other clients)
	 grab, so update the toplevel_under_window state */
      if (is_toplevel &&
	  event->type == GDK_ENTER_NOTIFY &&
	  event->crossing.mode == GDK_CROSSING_UNGRAB)
	{
	  if (display->pointer_info.toplevel_under_pointer)
	    g_object_unref (display->pointer_info.toplevel_under_pointer);
	  display->pointer_info.toplevel_under_pointer = g_object_ref (event_window);
	}

      unlink_event = TRUE;
      goto out;
    }

  /* Track toplevel_under_pointer */
  if (is_toplevel)
    {
      if (event->type == GDK_ENTER_NOTIFY &&
	  event->crossing.detail != GDK_NOTIFY_INFERIOR)
	{
	  if (display->pointer_info.toplevel_under_pointer)
	    g_object_unref (display->pointer_info.toplevel_under_pointer);
	  display->pointer_info.toplevel_under_pointer = g_object_ref (event_window);
	}
      else if (event->type == GDK_LEAVE_NOTIFY &&
	       event->crossing.detail != GDK_NOTIFY_INFERIOR &&
	       display->pointer_info.toplevel_under_pointer == event_window)
	{
	  if (display->pointer_info.toplevel_under_pointer)
	    g_object_unref (display->pointer_info.toplevel_under_pointer);
	  display->pointer_info.toplevel_under_pointer = NULL;
	}
    }

  /* Store last pointer window and position/state */
  old_state = display->pointer_info.state;
  old_button = display->pointer_info.button;

  gdk_event_get_coords (event, &x, &y);
  convert_native_coords_to_toplevel (event_window, x, y,  &x, &y);
  display->pointer_info.toplevel_x = x;
  display->pointer_info.toplevel_y = y;
  gdk_event_get_state (event, &display->pointer_info.state);
  if (event->type == GDK_BUTTON_PRESS ||
      event->type == GDK_BUTTON_RELEASE)
    display->pointer_info.button = event->button.button;

  if (display->pointer_info.state != old_state ||
      display->pointer_info.button != old_button)
    _gdk_display_enable_motion_hints (display);

  unlink_event = FALSE;
  if (is_motion_type (event->type))
    unlink_event = proxy_pointer_event (display,
					event,
					serial);
  else if (is_button_type (event->type))
    unlink_event = proxy_button_event (event,
				       serial);

  if (event->type == GDK_BUTTON_RELEASE &&
      !event->any.send_event)
    {
      button_release_grab =
	_gdk_display_has_pointer_grab (display, serial);
      if (button_release_grab &&
	  button_release_grab->implicit &&
	  (event->button.state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (event->button.button - 1))) == 0)
	{
	  button_release_grab->serial_end = serial;
	  button_release_grab->implicit_ungrab = FALSE;
	  _gdk_display_pointer_grab_update (display, serial);
	}
    }

 out:
  if (unlink_event)
    {
      _gdk_event_queue_remove_link (display, event_link);
      g_list_free_1 (event_link);
      gdk_event_free (event);
    }
}


static GdkWindow *
get_extension_event_window (GdkDisplay                 *display,
			    GdkWindow                  *pointer_window,
			    GdkEventType                type,
			    GdkModifierType             mask,
			    gulong                      serial)
{
  guint evmask;
  GdkWindow *grab_window;
  GdkWindowObject *w;
  GdkPointerGrabInfo *grab;

  grab = _gdk_display_has_pointer_grab (display, serial);

  if (grab != NULL && !grab->owner_events)
    {
      evmask = grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      grab_window = grab->window;

      if (evmask & type_masks[type])
	return grab_window;
      else
	return NULL;
    }

  w = (GdkWindowObject *)pointer_window;
  while (w != NULL)
    {
      evmask = w->extension_events;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & type_masks[type])
	return (GdkWindow *)w;

      w = get_event_parent (w);
    }

  if (grab != NULL &&
      grab->owner_events)
    {
      evmask = grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & type_masks[type])
	return grab->window;
      else
	return NULL;
    }

  return NULL;
}


GdkWindow *
_gdk_window_get_input_window_for_event (GdkWindow *native_window,
					GdkEventType event_type,
					GdkModifierType mask,
					int x, int y,
					gulong serial)
{
  GdkDisplay *display;
  GdkWindow *toplevel_window;
  GdkWindow *pointer_window;
  GdkWindow *event_win;
  gdouble toplevel_x, toplevel_y;

  toplevel_x = x;
  toplevel_y = y;

  display = gdk_drawable_get_display (native_window);
  toplevel_window = convert_native_coords_to_toplevel (native_window,
						       toplevel_x, toplevel_y,
						       &toplevel_x, &toplevel_y);
  pointer_window = get_pointer_window (display, toplevel_window,
				       toplevel_x, toplevel_y, serial);
  event_win = get_extension_event_window (display,
					  pointer_window,
					  event_type,
					  mask,
					  serial);

  return event_win;
}

/**
 * gdk_window_create_similar_surface:
 * @window: window to make new surface similar to
 * @content: the content for the new surface
 * @width: width of the new surface
 * @height: height of the new surface
 *
 * Create a new surface that is as compatible as possible with the
 * given @window. For example the new surface will have the same
 * fallback resolution and font options as @window. Generally, the new
 * surface will also use the same backend as @window, unless that is
 * not possible for some reason. The type of the returned surface may
 * be examined with cairo_surface_get_type().
 *
 * Initially the surface contents are all 0 (transparent if contents
 * have transparency, black otherwise.)
 *
 * Returns: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if @other is already in an error state
 * or any other error occurs.
 *
 * Since: 2.22
 **/
cairo_surface_t *
gdk_window_create_similar_surface (GdkWindow *     window,
                                   cairo_content_t content,
                                   int             width,
                                   int             height)
{
  cairo_surface_t *window_surface, *surface;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  
  window_surface = _gdk_drawable_ref_cairo_surface (window);

  surface = cairo_surface_create_similar (window_surface,
                                          content,
                                          width, height);

  cairo_surface_destroy (window_surface);

  return surface;
}

/**
 * gdk_window_get_screen:
 * @window: a #GdkWindow
 *
 * Gets the #GdkScreen associated with a #GdkWindow.
 *
 * Return value: the #GdkScreen associated with @window
 *
 * Since: 2.24
 */
GdkScreen*
gdk_window_get_screen (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return gdk_drawable_get_screen (GDK_DRAWABLE (window));
}

/**
 * gdk_window_get_display:
 * @window: a #GdkWindow
 *
 * Gets the #GdkDisplay associated with a #GdkWindow.
 *
 * Return value: the #GdkDisplay associated with @window
 *
 * Since: 2.24
 */
GdkDisplay *
gdk_window_get_display (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return gdk_drawable_get_display (GDK_DRAWABLE (window));
}

/**
 * gdk_window_get_visual:
 * @window: a #GdkWindow
 *
 * Gets the #GdkVisual describing the pixel format of @window.
 *
 * Return value: a #GdkVisual
 *
 * Since: 2.24
 */
GdkVisual*
gdk_window_get_visual (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return gdk_drawable_get_visual (GDK_DRAWABLE (window));
}

/**
 * gdk_window_get_width:
 * @window: a #GdkWindow
 *
 * Returns the width of the given @window.
 *
 * On the X11 platform the returned size is the size reported in the
 * most-recently-processed configure event, rather than the current
 * size on the X server.
 *
 * Returns: The width of @window
 *
 * Since: 2.24
 */
int
gdk_window_get_width (GdkWindow *window)
{
  gint width, height;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);

  return width;
}

/**
 * gdk_window_get_height:
 * @window: a #GdkWindow
 *
 * Returns the height of the given @window.
 *
 * On the X11 platform the returned size is the size reported in the
 * most-recently-processed configure event, rather than the current
 * size on the X server.
 *
 * Returns: The height of @window
 *
 * Since: 2.24
 */
int
gdk_window_get_height (GdkWindow *window)
{
  gint width, height;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);

  return height;
}


#define __GDK_WINDOW_C__
#include "gdkaliasdef.c"
