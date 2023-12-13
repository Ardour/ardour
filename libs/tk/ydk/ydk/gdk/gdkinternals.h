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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

/* Uninstalled header defining types and functions internal to GDK */

#ifndef __GDK_INTERNALS_H__
#define __GDK_INTERNALS_H__

#include <gio/gio.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkprivate.h>
#ifdef USE_MEDIALIB
#include <gdk/gdkmedialib.h>
#endif

G_BEGIN_DECLS

/**********************
 * General Facilities * 
 **********************/

/* Debugging support */

typedef struct _GdkColorInfo           GdkColorInfo;
typedef struct _GdkEventFilter	       GdkEventFilter;
typedef struct _GdkClientFilter	       GdkClientFilter;

typedef enum {
  GDK_COLOR_WRITEABLE = 1 << 0
} GdkColorInfoFlags;

struct _GdkColorInfo
{
  GdkColorInfoFlags flags;
  guint ref_count;
};

typedef enum {
  GDK_EVENT_FILTER_REMOVED = 1 << 0
} GdkEventFilterFlags;

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
  GdkEventFilterFlags flags;
  guint ref_count;
};

struct _GdkClientFilter {
  GdkAtom       type;
  GdkFilterFunc function;
  gpointer      data;
};

typedef enum {
  GDK_DEBUG_MISC          = 1 << 0,
  GDK_DEBUG_EVENTS        = 1 << 1,
  GDK_DEBUG_DND           = 1 << 2,
  GDK_DEBUG_XIM           = 1 << 3,
  GDK_DEBUG_NOGRABS       = 1 << 4,
  GDK_DEBUG_COLORMAP	  = 1 << 5,
  GDK_DEBUG_GDKRGB	  = 1 << 6,
  GDK_DEBUG_GC		  = 1 << 7,
  GDK_DEBUG_PIXMAP	  = 1 << 8,
  GDK_DEBUG_IMAGE	  = 1 << 9,
  GDK_DEBUG_INPUT	  = 1 <<10,
  GDK_DEBUG_CURSOR	  = 1 <<11,
  GDK_DEBUG_MULTIHEAD	  = 1 <<12,
  GDK_DEBUG_XINERAMA	  = 1 <<13,
  GDK_DEBUG_DRAW	  = 1 <<14,
  GDK_DEBUG_EVENTLOOP     = 1 <<15
} GdkDebugFlag;

#ifndef GDK_DISABLE_DEPRECATED

typedef struct _GdkFontPrivate	       GdkFontPrivate;

struct _GdkFontPrivate
{
  GdkFont font;
  guint ref_count;
};

#endif /* GDK_DISABLE_DEPRECATED */

extern GList            *_gdk_default_filters;
extern GdkWindow  	*_gdk_parent_root;
extern gint		 _gdk_error_code;
extern gint		 _gdk_error_warnings;

extern guint _gdk_debug_flags;
extern gboolean _gdk_native_windows;

#ifdef G_ENABLE_DEBUG

#define GDK_NOTE(type,action)		     G_STMT_START { \
    if (_gdk_debug_flags & GDK_DEBUG_##type)		    \
       { action; };			     } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GDK_NOTE(type,action)

#endif /* G_ENABLE_DEBUG */

/* Arg parsing */

typedef enum 
{
  GDK_ARG_STRING,
  GDK_ARG_INT,
  GDK_ARG_BOOL,
  GDK_ARG_NOBOOL,
  GDK_ARG_CALLBACK
} GdkArgType;

typedef struct _GdkArgContext GdkArgContext;
typedef struct _GdkArgDesc GdkArgDesc;

typedef void (*GdkArgFunc) (const char *name, const char *arg, gpointer data);

struct _GdkArgContext
{
  GPtrArray *tables;
  gpointer cb_data;
};

struct _GdkArgDesc
{
  const char *name;
  GdkArgType type;
  gpointer location;
  GdkArgFunc callback;
};

/* Event handling */

typedef struct _GdkEventPrivate GdkEventPrivate;

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0
} GdkEventFlags;

struct _GdkEventPrivate
{
  GdkEvent   event;
  guint      flags;
  GdkScreen *screen;
  gpointer   windowing_data;
};

/* Tracks information about the pointer grab on this display */
typedef struct
{
  GdkWindow *window;
  GdkWindow *native_window;
  gulong serial_start;
  gulong serial_end; /* exclusive, i.e. not active on serial_end */
  gboolean owner_events;
  guint event_mask;
  gboolean implicit;
  guint32 time;

  gboolean activated;
  gboolean implicit_ungrab;
} GdkPointerGrabInfo;

typedef struct _GdkInputWindow GdkInputWindow;

/* Private version of GdkWindowObject. The initial part of this strucuture
   is public for historical reasons. Don't change that part */
typedef struct _GdkWindowPaint             GdkWindowPaint;

struct _GdkWindowObject
{
  /* vvvvvvv THIS PART IS PUBLIC. DON'T CHANGE vvvvvvvvvvvvvv */
  GdkDrawable parent_instance;

  GdkDrawable *impl; /* window-system-specific delegate object */  
  
  GdkWindowObject *parent;

  gpointer user_data;

  gint x;
  gint y;
  
  gint extension_events;

  GList *filters;
  GList *children;

  GdkColor bg_color;
  GdkPixmap *bg_pixmap;
  
  GSList *paint_stack;
  
  GdkRegion *update_area;
  guint update_freeze_count;
  
  guint8 window_type;
  guint8 depth;
  guint8 resize_count;

  GdkWindowState state;
  
  guint guffaw_gravity : 1;
  guint input_only : 1;
  guint modal_hint : 1;
  guint composited : 1;
  
  guint destroyed : 2;

  guint accept_focus : 1;
  guint focus_on_map : 1;
  guint shaped : 1;
  
  GdkEventMask event_mask;

  guint update_and_descendants_freeze_count;

  GdkWindowRedirect *redirect;

  /* ^^^^^^^^^^ THIS PART IS PUBLIC. DON'T CHANGE ^^^^^^^^^^ */
  
  /* The GdkWindowObject that has the impl, ref:ed if another window.
   * This ref is required to keep the wrapper of the impl window alive
   * for as long as any GdkWindow references the impl. */
  GdkWindowObject *impl_window; 
  int abs_x, abs_y; /* Absolute offset in impl */
  gint width, height;
  guint32 clip_tag;
  GdkRegion *clip_region; /* Clip region (wrt toplevel) in window coords */
  GdkRegion *clip_region_with_children; /* Clip region in window coords */
  GdkCursor *cursor;
  gint8 toplevel_window_type;
  guint synthesize_crossing_event_queued : 1;
  guint effective_visibility : 2;
  guint visibility : 2; /* The visibility wrt the toplevel (i.e. based on clip_region) */
  guint native_visibility : 2; /* the native visibility of a impl windows */
  guint viewable : 1; /* mapped and all parents mapped */
  guint applied_shape : 1;

  guint num_offscreen_children;
  GdkWindowPaint *implicit_paint;
  GdkInputWindow *input_window; /* only set for impl windows */

  GList *outstanding_moves;

  GdkRegion *shape;
  GdkRegion *input_shape;
  
  cairo_surface_t *cairo_surface;
  guint outstanding_surfaces; /* only set on impl window */

  cairo_pattern_t *background;
};

#define GDK_WINDOW_TYPE(d) (((GdkWindowObject*)(GDK_WINDOW (d)))->window_type)
#define GDK_WINDOW_DESTROYED(d) (((GdkWindowObject*)(GDK_WINDOW (d)))->destroyed)

extern GdkEventFunc   _gdk_event_func;    /* Callback for events */
extern gpointer       _gdk_event_data;
extern GDestroyNotify _gdk_event_notify;

extern GSList    *_gdk_displays;
extern gchar     *_gdk_display_name;
extern gint       _gdk_screen_number;
extern gchar     *_gdk_display_arg_name;

void      _gdk_events_queue  (GdkDisplay *display);
GdkEvent* _gdk_event_unqueue (GdkDisplay *display);

void _gdk_event_filter_unref        (GdkWindow      *window,
				     GdkEventFilter *filter);

GList* _gdk_event_queue_find_first   (GdkDisplay *display);
void   _gdk_event_queue_remove_link  (GdkDisplay *display,
				      GList      *node);
GList* _gdk_event_queue_prepend      (GdkDisplay *display,
				      GdkEvent   *event);
GList* _gdk_event_queue_append       (GdkDisplay *display,
				      GdkEvent   *event);
GList* _gdk_event_queue_insert_after (GdkDisplay *display,
                                      GdkEvent   *after_event,
                                      GdkEvent   *event);
GList* _gdk_event_queue_insert_before(GdkDisplay *display,
                                      GdkEvent   *after_event,
                                      GdkEvent   *event);
void   _gdk_event_button_generate    (GdkDisplay *display,
				      GdkEvent   *event);

void _gdk_windowing_event_data_copy (const GdkEvent *src,
                                     GdkEvent       *dst);
void _gdk_windowing_event_data_free (GdkEvent       *event);

void gdk_synthesize_window_state (GdkWindow     *window,
                                  GdkWindowState unset_flags,
                                  GdkWindowState set_flags);

#define GDK_SCRATCH_IMAGE_WIDTH 256
#define GDK_SCRATCH_IMAGE_HEIGHT 64

GdkImage* _gdk_image_new_for_depth (GdkScreen    *screen,
				    GdkImageType  type,
				    GdkVisual    *visual,
				    gint          width,
				    gint          height,
				    gint          depth);
GdkImage *_gdk_image_get_scratch (GdkScreen *screen,
				  gint	     width,
				  gint	     height,
				  gint	     depth,
				  gint	    *x,
				  gint	    *y);

GdkImage *_gdk_drawable_copy_to_image (GdkDrawable  *drawable,
				       GdkImage     *image,
				       gint          src_x,
				       gint          src_y,
				       gint          dest_x,
				       gint          dest_y,
				       gint          width,
				       gint          height);

cairo_surface_t *_gdk_drawable_ref_cairo_surface (GdkDrawable *drawable);

GdkDrawable *_gdk_drawable_get_source_drawable (GdkDrawable *drawable);
cairo_surface_t * _gdk_drawable_create_cairo_surface (GdkDrawable *drawable,
						      int width,
						      int height);

/* GC caching */
GdkGC *_gdk_drawable_get_scratch_gc (GdkDrawable *drawable,
				     gboolean     graphics_exposures);
GdkGC *_gdk_drawable_get_subwindow_scratch_gc (GdkDrawable *drawable);

void _gdk_gc_update_context (GdkGC          *gc,
			     cairo_t        *cr,
			     const GdkColor *override_foreground,
			     GdkBitmap      *override_stipple,
			     gboolean        gc_changed,
			     GdkDrawable    *target_drawable);

/*************************************
 * Interfaces used by windowing code *
 *************************************/

GdkPixmap *_gdk_pixmap_new               (GdkDrawable    *drawable,
                                          gint            width,
                                          gint            height,
                                          gint            depth);
GdkPixmap *_gdk_pixmap_create_from_data  (GdkDrawable    *drawable,
                                          const gchar    *data,
                                          gint            width,
                                          gint            height,
                                          gint            depth,
                                          const GdkColor *fg,
                                          const GdkColor *bg);
GdkPixmap *_gdk_bitmap_create_from_data  (GdkDrawable    *drawable,
                                          const gchar    *data,
                                          gint            width,
                                          gint            height);

void       _gdk_window_impl_new          (GdkWindow      *window,
					  GdkWindow      *real_parent,
					  GdkScreen      *screen,
					  GdkVisual      *visual,
					  GdkEventMask    event_mask,
                                          GdkWindowAttr  *attributes,
                                          gint            attributes_mask);
void       _gdk_window_destroy           (GdkWindow      *window,
                                          gboolean        foreign_destroy);
void       _gdk_window_clear_update_area (GdkWindow      *window);
void       _gdk_window_update_size       (GdkWindow      *window);
gboolean   _gdk_window_update_viewable   (GdkWindow      *window);

void       _gdk_window_process_updates_recurse (GdkWindow *window,
                                                GdkRegion *expose_region);

void       _gdk_screen_close             (GdkScreen      *screen);

const char *_gdk_get_sm_client_id (void);

void _gdk_gc_init (GdkGC           *gc,
		   GdkDrawable     *drawable,
		   GdkGCValues     *values,
		   GdkGCValuesMask  values_mask);

GdkRegion *_gdk_gc_get_clip_region (GdkGC *gc);
GdkBitmap *_gdk_gc_get_clip_mask   (GdkGC *gc);
gboolean   _gdk_gc_get_exposures   (GdkGC *gc);
GdkFill    _gdk_gc_get_fill        (GdkGC *gc);
GdkPixmap *_gdk_gc_get_tile        (GdkGC *gc);
GdkBitmap *_gdk_gc_get_stipple     (GdkGC *gc);
guint32    _gdk_gc_get_fg_pixel    (GdkGC *gc);
guint32    _gdk_gc_get_bg_pixel    (GdkGC *gc);
void      _gdk_gc_add_drawable_clip     (GdkGC     *gc,
					 guint32    region_tag,
					 GdkRegion *region,
					 int        offset_x,
					 int        offset_y);
void      _gdk_gc_remove_drawable_clip  (GdkGC     *gc);
void       _gdk_gc_set_clip_region_internal (GdkGC     *gc,
					     GdkRegion *region,
					     gboolean reset_origin);
GdkSubwindowMode _gdk_gc_get_subwindow (GdkGC *gc);

GdkDrawable *_gdk_drawable_begin_direct_draw (GdkDrawable *drawable,
					      GdkGC *gc,
					      gpointer *priv_data,
					      gint *x_offset_out,
					      gint *y_offset_out);
void         _gdk_drawable_end_direct_draw (gpointer priv_data);


/*****************************************
 * Interfaces provided by windowing code *
 *****************************************/

/* Font/string functions implemented in module-specific code */
gint _gdk_font_strlen (GdkFont *font, const char *str);
void _gdk_font_destroy (GdkFont *font);

void _gdk_colormap_real_destroy (GdkColormap *colormap);

void _gdk_cursor_destroy (GdkCursor *cursor);

void     _gdk_windowing_init                    (void);

extern const GOptionEntry _gdk_windowing_args[];
void     _gdk_windowing_set_default_display     (GdkDisplay *display);

gchar *_gdk_windowing_substitute_screen_number (const gchar *display_name,
					        gint         screen_number);

gulong   _gdk_windowing_window_get_next_serial  (GdkDisplay *display);
void     _gdk_windowing_window_get_offsets      (GdkWindow  *window,
						 gint       *x_offset,
						 gint       *y_offset);
GdkRegion *_gdk_windowing_window_get_shape      (GdkWindow  *window);
GdkRegion *_gdk_windowing_window_get_input_shape(GdkWindow  *window);
GdkRegion *_gdk_windowing_get_shape_for_mask    (GdkBitmap *mask);
void     _gdk_windowing_window_beep             (GdkWindow *window);


void       _gdk_windowing_get_pointer        (GdkDisplay       *display,
					      GdkScreen       **screen,
					      gint             *x,
					      gint             *y,
					      GdkModifierType  *mask);
GdkWindow* _gdk_windowing_window_at_pointer  (GdkDisplay       *display,
					      gint             *win_x,
					      gint             *win_y,
					      GdkModifierType  *mask,
					      gboolean          get_toplevel);
GdkGrabStatus _gdk_windowing_pointer_grab    (GdkWindow        *window,
					      GdkWindow        *native,
					      gboolean          owner_events,
					      GdkEventMask      event_mask,
					      GdkWindow        *confine_to,
					      GdkCursor        *cursor,
					      guint32           time);
void _gdk_windowing_got_event                (GdkDisplay       *display,
					      GList            *event_link,
					      GdkEvent         *event,
					      gulong            serial);

void _gdk_windowing_window_process_updates_recurse (GdkWindow *window,
                                                    GdkRegion *expose_region);
void _gdk_windowing_before_process_all_updates     (void);
void _gdk_windowing_after_process_all_updates      (void);

/* Return the number of bits-per-pixel for images of the specified depth. */
gint _gdk_windowing_get_bits_for_depth (GdkDisplay *display,
					gint        depth);


#define GDK_WINDOW_IS_MAPPED(window) ((((GdkWindowObject*)window)->state & GDK_WINDOW_STATE_WITHDRAWN) == 0)


/* Called when gdk_window_destroy() is called on a foreign window
 * or an ancestor of the foreign window. It should generally reparent
 * the window out of it's current heirarchy, hide it, and then
 * send a message to the owner requesting that the window be destroyed.
 */
void _gdk_windowing_window_destroy_foreign (GdkWindow *window);

void _gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					      const gchar *sm_client_id);

void _gdk_windowing_window_set_composited (GdkWindow *window,
					   gboolean composited);

#define GDK_TYPE_PAINTABLE            (_gdk_paintable_get_type ())
#define GDK_PAINTABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_PAINTABLE, GdkPaintable))
#define GDK_IS_PAINTABLE(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_PAINTABLE))
#define GDK_PAINTABLE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDK_TYPE_PAINTABLE, GdkPaintableIface))

typedef struct _GdkPaintable        GdkPaintable;
typedef struct _GdkPaintableIface   GdkPaintableIface;

struct _GdkPaintableIface
{
  GTypeInterface g_iface;
  
  void (* begin_paint_region)       (GdkPaintable    *paintable,
                                     GdkWindow       *window,
                                     const GdkRegion *region);
  void (* end_paint)                (GdkPaintable    *paintable);
};

GType _gdk_paintable_get_type (void) G_GNUC_CONST;

/* Implementation types */
GType _gdk_window_impl_get_type (void) G_GNUC_CONST;
GType _gdk_pixmap_impl_get_type (void) G_GNUC_CONST;


/**
 * _gdk_windowing_gc_set_clip_region:
 * @gc: a #GdkGC
 * @region: the new clip region
 * @reset_origin: if TRUE, reset the clip_x/y_origin values to 0
 * 
 * Do any window-system specific processing necessary
 * for a change in clip region. Since the clip origin
 * will likely change before the GC is used with the
 * new clip, frequently this function will only set a flag and
 * do the real processing later.
 *
 * When this function is called, _gdk_gc_get_clip_region
 * will already return the new region.
 **/
void _gdk_windowing_gc_set_clip_region (GdkGC           *gc,
					const GdkRegion *region,
					gboolean reset_origin);

/**
 * _gdk_windowing_gc_copy:
 * @dst_gc: a #GdkGC from the GDK backend
 * @src_gc: a #GdkGC from the GDK backend
 * 
 * Copies backend specific state from @src_gc to @dst_gc.
 * This is called before the generic state is copied, so
 * the old generic state is still available from @dst_gc
 **/
void _gdk_windowing_gc_copy (GdkGC *dst_gc,
			     GdkGC *src_gc);
     
/* Queries the current foreground color of a GdkGC */
void _gdk_windowing_gc_get_foreground (GdkGC    *gc,
				       GdkColor *color);
/* Queries the current background color of a GdkGC */
void _gdk_windowing_gc_get_background (GdkGC    *gc,
				       GdkColor *color);

struct GdkAppLaunchContextPrivate
{
  GdkDisplay *display;
  GdkScreen *screen;
  gint workspace;
  guint32 timestamp;
  GIcon *icon;
  char *icon_name;
};

char *_gdk_windowing_get_startup_notify_id (GAppLaunchContext *context,
					    GAppInfo          *info, 
					    GList             *files);
void  _gdk_windowing_launch_failed         (GAppLaunchContext *context, 
				            const char        *startup_notify_id);

GdkPointerGrabInfo *_gdk_display_get_active_pointer_grab (GdkDisplay *display);
void _gdk_display_pointer_grab_update                    (GdkDisplay *display,
							  gulong current_serial);
GdkPointerGrabInfo *_gdk_display_get_last_pointer_grab (GdkDisplay *display);
GdkPointerGrabInfo *_gdk_display_add_pointer_grab  (GdkDisplay *display,
						    GdkWindow *window,
						    GdkWindow *native_window,
						    gboolean owner_events,
						    GdkEventMask event_mask,
						    unsigned long serial_start,
						    guint32 time,
						    gboolean implicit);
GdkPointerGrabInfo * _gdk_display_has_pointer_grab (GdkDisplay *display,
						    gulong serial);
gboolean _gdk_display_end_pointer_grab (GdkDisplay *display,
					gulong serial,
					GdkWindow *if_child,
					gboolean implicit);
void _gdk_display_set_has_keyboard_grab (GdkDisplay *display,
					 GdkWindow *window,
					 GdkWindow *native_window,
					 gboolean owner_events,
					 unsigned long serial,
					 guint32 time);
void _gdk_display_unset_has_keyboard_grab (GdkDisplay *display,
					   gboolean implicit);
void _gdk_display_enable_motion_hints     (GdkDisplay *display);


void _gdk_window_invalidate_for_expose (GdkWindow       *window,
					GdkRegion       *region);

void _gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
					    int width,
					    int height);

cairo_surface_t * _gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
						       int width,
						       int height);
GdkWindow * _gdk_window_find_child_at (GdkWindow *window,
				       int x, int y);
GdkWindow * _gdk_window_find_descendant_at (GdkWindow *toplevel,
					    double x, double y,
					    double *found_x,
					    double *found_y);

void _gdk_window_add_damage (GdkWindow *toplevel,
			     GdkRegion *damaged_region);

GdkEvent * _gdk_make_event (GdkWindow    *window,
			    GdkEventType  type,
			    GdkEvent     *event_in_queue,
			    gboolean      before_event);
gboolean _gdk_window_event_parent_of (GdkWindow *parent,
                                      GdkWindow *child);

void _gdk_synthesize_crossing_events (GdkDisplay                 *display,
				      GdkWindow                  *src,
				      GdkWindow                  *dest,
				      GdkCrossingMode             mode,
				      gint                        toplevel_x,
				      gint                        toplevel_y,
				      GdkModifierType             mask,
				      guint32                     time_,
				      GdkEvent                   *event_in_queue,
				      gulong                      serial,
				      gboolean                    non_linear);
void _gdk_display_set_window_under_pointer (GdkDisplay *display,
					    GdkWindow *window);


void _gdk_synthesize_crossing_events_for_geometry_change (GdkWindow *changed_window);

GdkRegion *_gdk_window_calculate_full_clip_region    (GdkWindow     *window,
                                                      GdkWindow     *base_window,
                                                      gboolean       do_children,
                                                      gint          *base_x_offset,
                                                      gint          *base_y_offset);
gboolean    _gdk_window_has_impl (GdkWindow *window);
GdkWindow * _gdk_window_get_impl_window (GdkWindow *window);
GdkWindow *_gdk_window_get_input_window_for_event (GdkWindow *native_window,
						   GdkEventType event_type,
						   GdkModifierType mask,
						   int x, int y,
						   gulong serial);
GdkRegion  *_gdk_region_new_from_yxbanded_rects (GdkRectangle *rects, int n_rects);

/*****************************
 * offscreen window routines *
 *****************************/
typedef struct _GdkOffscreenWindow      GdkOffscreenWindow;
#define GDK_TYPE_OFFSCREEN_WINDOW            (gdk_offscreen_window_get_type())
#define GDK_OFFSCREEN_WINDOW(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindow))
#define GDK_IS_OFFSCREEN_WINDOW(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OFFSCREEN_WINDOW))
GType gdk_offscreen_window_get_type (void);
GdkDrawable * _gdk_offscreen_window_get_real_drawable (GdkOffscreenWindow *window);
void       _gdk_offscreen_window_new                 (GdkWindow     *window,
						      GdkScreen     *screen,
						      GdkVisual     *visual,
						      GdkWindowAttr *attributes,
						      gint           attributes_mask);


/************************************
 * Initialization and exit routines *
 ************************************/

void _gdk_image_exit  (void);
void _gdk_windowing_exit (void);

G_END_DECLS

#endif /* __GDK_INTERNALS_H__ */
