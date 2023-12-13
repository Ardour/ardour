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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_X11_H__
#define __GDK_PRIVATE_X11_H__

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/x11/gdkwindow-x11.h>
#include <gdk/x11/gdkpixmap-x11.h>
#include <gdk/x11/gdkdisplay-x11.h>
#include <gdk/gdkinternals.h>

#define GDK_TYPE_GC_X11              (_gdk_gc_x11_get_type ())
#define GDK_GC_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_GC_X11, GdkGCX11))
#define GDK_GC_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GC_X11, GdkGCX11Class))
#define GDK_IS_GC_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_GC_X11))
#define GDK_IS_GC_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GC_X11))
#define GDK_GC_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GC_X11, GdkGCX11Class))

typedef struct _GdkCursorPrivate       GdkCursorPrivate;
typedef struct _GdkVisualPrivate       GdkVisualPrivate;
typedef struct _GdkGCX11      GdkGCX11;
typedef struct _GdkGCX11Class GdkGCX11Class;

struct _GdkGCX11
{
  GdkGC parent_instance;
  
  GC xgc;
  GdkScreen *screen;
  guint16 dirty_mask;
  guint have_clip_region : 1;
  guint have_clip_mask : 1;
  guint depth : 8;
};

struct _GdkGCX11Class
{
  GdkGCClass parent_class;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  Cursor xcursor;
  GdkDisplay *display;
  gchar *name;
  guint serial;
};

struct _GdkVisualPrivate
{
  GdkVisual visual;
  Visual *xvisual;
  GdkScreen *screen;
};

#define XID_FONT_BIT (1<<31)

void _gdk_xid_table_insert (GdkDisplay *display,
			    XID        *xid,
			    gpointer    data);
void _gdk_xid_table_remove (GdkDisplay *display,
			    XID         xid);
gint _gdk_send_xevent      (GdkDisplay *display,
			    Window      window,
			    gboolean    propagate,
			    glong       event_mask,
			    XEvent     *event_send);

GType _gdk_gc_x11_get_type (void);

gboolean _gdk_x11_have_render           (GdkDisplay *display);

GdkGC *_gdk_x11_gc_new                  (GdkDrawable     *drawable,
					 GdkGCValues     *values,
					 GdkGCValuesMask  values_mask);

GdkImage *_gdk_x11_copy_to_image       (GdkDrawable *drawable,
					GdkImage    *image,
					gint         src_x,
					gint         src_y,
					gint         dest_x,
					gint         dest_y,
					gint         width,
					gint         height);
Pixmap   _gdk_x11_image_get_shm_pixmap (GdkImage    *image);

/* Routines from gdkgeometry-x11.c */
void _gdk_window_move_resize_child (GdkWindow     *window,
                                    gint           x,
                                    gint           y,
                                    gint           width,
                                    gint           height);
void _gdk_window_process_expose    (GdkWindow     *window,
                                    gulong         serial,
                                    GdkRectangle  *area);

gboolean _gdk_x11_window_queue_antiexpose  (GdkWindow *window,
					    GdkRegion *area);
void     _gdk_x11_window_queue_translation (GdkWindow *window,
					    GdkGC     *gc,
					    GdkRegion *area,
					    gint       dx,
					    gint       dy);

void     _gdk_selection_window_destroyed   (GdkWindow            *window);
gboolean _gdk_selection_filter_clear_event (XSelectionClearEvent *event);

GdkRegion* _xwindow_get_shape              (Display *xdisplay,
                                            Window window,
                                            gint shape_type);

void     _gdk_region_get_xrectangles       (const GdkRegion      *region,
                                            gint                  x_offset,
                                            gint                  y_offset,
                                            XRectangle          **rects,
                                            gint                 *n_rects);

gboolean _gdk_moveresize_handle_event   (XEvent     *event);
gboolean _gdk_moveresize_configure_done (GdkDisplay *display,
					 GdkWindow  *window);

void _gdk_keymap_state_changed    (GdkDisplay      *display,
				   XEvent          *event);
void _gdk_keymap_keys_changed     (GdkDisplay      *display);
gint _gdk_x11_get_group_for_state (GdkDisplay      *display,
				   GdkModifierType  state);
void _gdk_keymap_add_virtual_modifiers_compat (GdkKeymap       *keymap,
                                               GdkModifierType *modifiers);
gboolean _gdk_keymap_key_is_modifier   (GdkKeymap       *keymap,
					guint            keycode);

GC _gdk_x11_gc_flush (GdkGC *gc);

void _gdk_x11_initialize_locale (void);

void _gdk_xgrab_check_unmap        (GdkWindow *window,
				    gulong     serial);
void _gdk_xgrab_check_destroy      (GdkWindow *window);

gboolean _gdk_x11_display_is_root_window (GdkDisplay *display,
					  Window      xroot_window);

void _gdk_x11_precache_atoms (GdkDisplay          *display,
			      const gchar * const *atom_names,
			      gint                 n_atoms);

void _gdk_x11_events_init_screen   (GdkScreen *screen);
void _gdk_x11_events_uninit_screen (GdkScreen *screen);

void _gdk_events_init           (GdkDisplay *display);
void _gdk_events_uninit         (GdkDisplay *display);
void _gdk_windowing_window_init (GdkScreen *screen);
void _gdk_visual_init           (GdkScreen *screen);
void _gdk_dnd_init		(GdkDisplay *display);
void _gdk_windowing_image_init  (GdkDisplay *display);
void _gdk_input_init            (GdkDisplay *display);

PangoRenderer *_gdk_x11_renderer_get (GdkDrawable *drawable,
				      GdkGC       *gc);

void _gdk_x11_cursor_update_theme (GdkCursor *cursor);
void _gdk_x11_cursor_display_finalize (GdkDisplay *display);

gboolean _gdk_x11_get_xft_setting (GdkScreen   *screen,
				   const gchar *name,
				   GValue      *value);

extern GdkDrawableClass  _gdk_x11_drawable_class;
extern gboolean	         _gdk_use_xshm;
extern const int         _gdk_nenvent_masks;
extern const int         _gdk_event_mask_table[];
extern GdkAtom		 _gdk_selection_property;
extern gboolean          _gdk_synchronize;

#define GDK_PIXMAP_SCREEN(pix)	      (GDK_DRAWABLE_IMPL_X11 (((GdkPixmapObject *)pix)->impl)->screen)
#define GDK_PIXMAP_DISPLAY(pix)       (GDK_SCREEN_X11 (GDK_PIXMAP_SCREEN (pix))->display)
#define GDK_PIXMAP_XROOTWIN(pix)      (GDK_SCREEN_X11 (GDK_PIXMAP_SCREEN (pix))->xroot_window)
#define GDK_DRAWABLE_DISPLAY(win)     (GDK_IS_WINDOW (win) ? GDK_WINDOW_DISPLAY (win) : GDK_PIXMAP_DISPLAY (win))
#define GDK_DRAWABLE_SCREEN(win)      (GDK_IS_WINDOW (win) ? GDK_WINDOW_SCREEN (win) : GDK_PIXMAP_SCREEN (win))
#define GDK_DRAWABLE_XROOTWIN(win)    (GDK_IS_WINDOW (win) ? GDK_WINDOW_XROOTWIN (win) : GDK_PIXMAP_XROOTWIN (win))
#define GDK_SCREEN_DISPLAY(screen)    (GDK_SCREEN_X11 (screen)->display)
#define GDK_SCREEN_XROOTWIN(screen)   (GDK_SCREEN_X11 (screen)->xroot_window)
#define GDK_WINDOW_SCREEN(win)	      (GDK_DRAWABLE_IMPL_X11 (((GdkWindowObject *)win)->impl)->screen)
#define GDK_WINDOW_DISPLAY(win)       (GDK_SCREEN_X11 (GDK_WINDOW_SCREEN (win))->display)
#define GDK_WINDOW_XROOTWIN(win)      (GDK_SCREEN_X11 (GDK_WINDOW_SCREEN (win))->xroot_window)
#define GDK_GC_DISPLAY(gc)            (GDK_SCREEN_DISPLAY (GDK_GC_X11(gc)->screen))
#define GDK_WINDOW_IS_X11(win)        (GDK_IS_WINDOW_IMPL_X11 (((GdkWindowObject *)win)->impl))

#endif /* __GDK_PRIVATE_X11_H__ */
