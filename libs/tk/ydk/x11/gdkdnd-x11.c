/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif

#include <string.h>

#include "gdk.h"          /* For gdk_flush() */
#include "gdkx.h"
#include "gdkasync.h"
#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

typedef struct _GdkDragContextPrivateX11 GdkDragContextPrivateX11;

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

typedef struct {
  guint32 xid;
  gint x, y, width, height;
  gboolean mapped;
  gboolean shape_selected;
  gboolean shape_valid;
  GdkRegion *shape;
} GdkCacheChild;

typedef struct {
  GList *children;
  GHashTable *child_hash;
  guint old_event_mask;
  GdkScreen *screen;
  gint ref_count;
} GdkWindowCache;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivateX11 {
  GdkDragContext context;

  Atom motif_selection;
  guint   ref_count;

  guint16 last_x;		/* Coordinates from last event */
  guint16 last_y;
  GdkDragAction old_action;	  /* The last action we sent to the source */
  GdkDragAction old_actions;	  /* The last actions we sent to the source */
  GdkDragAction xdnd_actions;     /* What is currently set in XdndActionList */

  Window dest_xid;              /* The last window we looked up */
  Window drop_xid;            /* The (non-proxied) window that is receiving drops */
  guint xdnd_targets_set : 1;   /* Whether we've already set XdndTypeList */
  guint xdnd_actions_set : 1;   /* Whether we've already set XdndActionList */
  guint xdnd_have_actions : 1;  /* Whether an XdndActionList was provided */
  guint motif_targets_set : 1;  /* Whether we've already set motif initiator info */
  guint drag_status : 4;	/* current status of drag */
  
  guint drop_failed : 1;        /* Whether the drop was unsuccessful */
  guint version;                /* Xdnd protocol version */

  GSList *window_caches;
};

#define PRIVATE_DATA(context) ((GdkDragContextPrivateX11 *) GDK_DRAG_CONTEXT (context)->windowing_data)

/* Forward declarations */

static GdkWindowCache *gdk_window_cache_get   (GdkScreen      *screen);
static GdkWindowCache *gdk_window_cache_ref   (GdkWindowCache *cache);
static void            gdk_window_cache_unref (GdkWindowCache *cache);

static void motif_read_target_table (GdkDisplay *display);

static GdkFilterReturn motif_dnd_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

static GdkFilterReturn xdnd_enter_filter    (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);
static GdkFilterReturn xdnd_leave_filter    (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);
static GdkFilterReturn xdnd_position_filter (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);
static GdkFilterReturn xdnd_status_filter   (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);
static GdkFilterReturn xdnd_finished_filter (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);
static GdkFilterReturn xdnd_drop_filter     (GdkXEvent *xev,
					     GdkEvent  *event,
					     gpointer   data);

static void   xdnd_manage_source_filter (GdkDragContext *context,
					 GdkWindow      *window,
					 gboolean        add_filter);

static void gdk_drag_context_finalize   (GObject              *object);

static GList *contexts;
static GSList *window_caches;

static const struct {
  const char *atom_name;
  GdkFilterFunc func;
} xdnd_filters[] = {
  { "XdndEnter",    xdnd_enter_filter },
  { "XdndLeave",    xdnd_leave_filter },
  { "XdndPosition", xdnd_position_filter },
  { "XdndStatus",   xdnd_status_filter },
  { "XdndFinished", xdnd_finished_filter },
  { "XdndDrop",     xdnd_drop_filter },
};
	      
G_DEFINE_TYPE (GdkDragContext, gdk_drag_context, G_TYPE_OBJECT)

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  GdkDragContextPrivateX11 *private;

  private = G_TYPE_INSTANCE_GET_PRIVATE (dragcontext, 
					 GDK_TYPE_DRAG_CONTEXT, 
					 GdkDragContextPrivateX11);
  
  dragcontext->windowing_data = private;

  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_drag_context_finalize;

  g_type_class_add_private (object_class, sizeof (GdkDragContextPrivateX11));
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  
  g_list_free (context->targets);

  if (context->source_window)
    {
      if ((context->protocol == GDK_DRAG_PROTO_XDND) &&
          !context->is_source)
        xdnd_manage_source_filter (context, context->source_window, FALSE);
      
      g_object_unref (context->source_window);
    }
  
  if (context->dest_window)
    g_object_unref (context->dest_window);

  g_slist_free_full (private->window_caches, (GDestroyNotify)gdk_window_cache_unref);
  private->window_caches = NULL;
  
  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (gdk_drag_context_parent_class)->finalize (object);
}

/* Drag Contexts */

/**
 * gdk_drag_context_new:
 * 
 * Creates a new #GdkDragContext.
 * 
 * Return value: the newly created #GdkDragContext.
 *
 * Deprecated: 2.24: This function is not useful, you always
 *   obtain drag contexts by gdk_drag_begin() or similar.
 **/
GdkDragContext *
gdk_drag_context_new (void)
{
  return g_object_new (GDK_TYPE_DRAG_CONTEXT, NULL);
}

/**
 * gdk_drag_context_ref:
 * @context: a #GdkDragContext.
 *
 * Deprecated function; use g_object_ref() instead.
 *
 * Deprecated: 2.2: Use g_object_ref() instead.
 **/
void            
gdk_drag_context_ref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_ref (context);
}

/**
 * gdk_drag_context_unref:
 * @context: a #GdkDragContext.
 *
 * Deprecated function; use g_object_unref() instead.
 *
 * Deprecated: 2.2: Use g_object_unref() instead.
 **/
void            
gdk_drag_context_unref (GdkDragContext *context)
{
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_unref (context);
}

static GdkDragContext *
gdk_drag_context_find (GdkDisplay *display,
		       gboolean    is_source,
		       Window      source_xid,
		       Window      dest_xid)
{
  GList *tmp_list = contexts;
  GdkDragContext *context;
  GdkDragContextPrivateX11 *private;
  Window context_dest_xid;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;
      private = PRIVATE_DATA (context);

      if ((context->source_window && gdk_drawable_get_display (context->source_window) != display) ||
	  (context->dest_window && gdk_drawable_get_display (context->dest_window) != display))
	continue;

      context_dest_xid = context->dest_window ? 
                            (private->drop_xid ?
                              private->drop_xid :
                              GDK_DRAWABLE_XID (context->dest_window)) :
	                     None;

      if ((!context->is_source == !is_source) &&
	  ((source_xid == None) || (context->source_window &&
	    (GDK_DRAWABLE_XID (context->source_window) == source_xid))) &&
	  ((dest_xid == None) || (context_dest_xid == dest_xid)))
	return context;
      
      tmp_list = tmp_list->next;
    }
  
  return NULL;
}

static void
precache_target_list (GdkDragContext *context)
{
  if (context->targets)
    {
      GPtrArray *targets = g_ptr_array_new ();
      GList *tmp_list;
      int i;

      for (tmp_list = context->targets; tmp_list; tmp_list = tmp_list->next)
	g_ptr_array_add (targets, gdk_atom_name (GDK_POINTER_TO_ATOM (tmp_list->data)));

      _gdk_x11_precache_atoms (GDK_WINDOW_DISPLAY (context->source_window),
			       (const gchar **)targets->pdata,
			       targets->len);

      for (i =0; i < targets->len; i++)
	g_free (targets->pdata[i]);

      g_ptr_array_free (targets, TRUE);
    }
}

/* Utility functions */

static void
free_cache_child (GdkCacheChild *child,
                  GdkDisplay    *display)
{
  if (child->shape)
    gdk_region_destroy (child->shape);

  if (child->shape_selected && display)
    {
      GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

      XShapeSelectInput (display_x11->xdisplay, child->xid, 0);
    }

  g_free (child);
}

static void
gdk_window_cache_add (GdkWindowCache *cache,
		      guint32 xid,
		      gint x, gint y, gint width, gint height, 
		      gboolean mapped)
{
  GdkCacheChild *child = g_new (GdkCacheChild, 1);

  child->xid = xid;
  child->x = x;
  child->y = y;
  child->width = width;
  child->height = height;
  child->mapped = mapped;
  child->shape_selected = FALSE;
  child->shape_valid = FALSE;
  child->shape = NULL;

  cache->children = g_list_prepend (cache->children, child);
  g_hash_table_insert (cache->child_hash, GUINT_TO_POINTER (xid), 
		       cache->children);
}

static GdkFilterReturn
gdk_window_cache_shape_filter (GdkXEvent *xev,
                               GdkEvent  *event,
                               gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkWindowCache *cache = data;

  GdkDisplayX11 *display = GDK_DISPLAY_X11 (gdk_screen_get_display (cache->screen));

  if (display->have_shapes &&
      xevent->type == display->shape_event_base + ShapeNotify)
    {
      XShapeEvent *xse = (XShapeEvent*)xevent;
      GList *node;

      node = g_hash_table_lookup (cache->child_hash,
                                  GUINT_TO_POINTER (xse->window));
      if (node)
        {
          GdkCacheChild *child = node->data;
          child->shape_valid = FALSE;
          if (child->shape)
            {
              gdk_region_destroy (child->shape);
              child->shape = NULL;
            }
        }

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
gdk_window_cache_filter (GdkXEvent *xev,
			 GdkEvent  *event,
			 gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkWindowCache *cache = data;

  switch (xevent->type)
    {
    case CirculateNotify:
      break;
    case ConfigureNotify:
      {
	XConfigureEvent *xce = &xevent->xconfigure;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xce->window));
	if (node) 
	  {
	    GdkCacheChild *child = node->data;
	    child->x = xce->x; 
	    child->y = xce->y;
	    child->width = xce->width; 
	    child->height = xce->height;
	    if (xce->above == None && (node->next))
	      {
		GList *last = g_list_last (cache->children);
		cache->children = g_list_remove_link (cache->children, node);
		last->next = node;
		node->next = NULL;
		node->prev = last;
	      }
	    else
	      {
		GList *above_node = g_hash_table_lookup (cache->child_hash, 
							 GUINT_TO_POINTER (xce->above));
		if (above_node && node->next != above_node)
		  {
		    /* Put the window above (before in the list) above_node
		     */
		    cache->children = g_list_remove_link (cache->children, node);
		    node->prev = above_node->prev;
		    if (node->prev)
		      node->prev->next = node;
		    else
		      cache->children = node;
		    node->next = above_node;
		    above_node->prev = node;
		  }
	      }
	  }
	break;
      }
    case CreateNotify:
      {
	XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

	if (!g_hash_table_lookup (cache->child_hash, 
				  GUINT_TO_POINTER (xcwe->window))) 
	  gdk_window_cache_add (cache, xcwe->window, 
				xcwe->x, xcwe->y, xcwe->width, xcwe->height,
				FALSE);
	break;
      }
    case DestroyNotify:
      {
	XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xdwe->window));
	if (node) 
	  {
	    GdkCacheChild *child = node->data;

	    g_hash_table_remove (cache->child_hash,
				 GUINT_TO_POINTER (xdwe->window));
	    cache->children = g_list_remove_link (cache->children, node);
	    /* window is destroyed, no need to disable ShapeNotify */
	    free_cache_child (child, NULL);
	    g_list_free_1 (node);
	  }
	break;
      }
    case MapNotify:
      {
	XMapEvent *xme = &xevent->xmap;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xme->window));
	if (node) 
	  {
	    GdkCacheChild *child = node->data;
	    child->mapped = TRUE;
	  }
	break;
      }
    case ReparentNotify:
      break;
    case UnmapNotify:
      {
	XMapEvent *xume = &xevent->xmap;
	GList *node;

	node = g_hash_table_lookup (cache->child_hash, 
				    GUINT_TO_POINTER (xume->window));
	if (node)
	  {
	    GdkCacheChild *child = node->data;
	    child->mapped = FALSE;
	  }
	break;
      }
    default:
      return GDK_FILTER_CONTINUE;
    }
  return GDK_FILTER_REMOVE;
}

static GdkWindowCache *
gdk_window_cache_new (GdkScreen *screen)
{
  XWindowAttributes xwa;
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  GdkWindow *root_window = gdk_screen_get_root_window (screen);
  GdkChildInfoX11 *children;
  guint nchildren, i;
  Window cow;
  
  GdkWindowCache *result = g_new (GdkWindowCache, 1);

  result->children = NULL;
  result->child_hash = g_hash_table_new (g_direct_hash, NULL);
  result->screen = screen;
  result->ref_count = 1;

  XGetWindowAttributes (xdisplay, GDK_WINDOW_XWINDOW (root_window), &xwa);
  result->old_event_mask = xwa.your_event_mask;

  if (G_UNLIKELY (!GDK_DISPLAY_X11 (GDK_SCREEN_X11 (screen)->display)->trusted_client)) 
    {
      GList *toplevel_windows, *list;
      GdkWindow *window;
      gint x, y, width, height;
      
      toplevel_windows = gdk_screen_get_toplevel_windows (screen);
      for (list = toplevel_windows; list; list = list->next) {
	window = GDK_WINDOW (list->data);
	gdk_window_get_geometry (window, &x, &y, &width, &height, NULL);
	gdk_window_cache_add (result, GDK_WINDOW_XID (window), 
			      x, y, width, height, 
			      gdk_window_is_visible (window));
      }
      g_list_free (toplevel_windows);
      return result;
    }

  XSelectInput (xdisplay, GDK_WINDOW_XWINDOW (root_window),
		result->old_event_mask | SubstructureNotifyMask);
  gdk_window_add_filter (root_window, gdk_window_cache_filter, result);
  gdk_window_add_filter (NULL, gdk_window_cache_shape_filter, result);

  if (!_gdk_x11_get_window_child_info (gdk_screen_get_display (screen),
				       GDK_WINDOW_XWINDOW (root_window),
				       FALSE, NULL,
				       &children, &nchildren))
    return result;

  for (i = 0; i < nchildren ; i++)
    {
      gdk_window_cache_add (result, children[i].window,
			    children[i].x, children[i].y, children[i].width, children[i].height,
			    children[i].is_mapped);
    }

  g_free (children);

#ifdef HAVE_XCOMPOSITE
  /*
   * Add the composite overlay window to the cache, as this can be a reasonable
   * Xdnd proxy as well.
   * This is only done when the screen is composited in order to avoid mapping
   * the COW. We assume that the CM is using the COW (which is true for pretty
   * much any CM currently in use).
   */
  if (gdk_screen_is_composited (screen))
    {
      cow = XCompositeGetOverlayWindow (xdisplay, GDK_WINDOW_XWINDOW (root_window));
      gdk_window_cache_add (result, cow, 0, 0, gdk_screen_get_width (screen), gdk_screen_get_height (screen), TRUE);
      XCompositeReleaseOverlayWindow (xdisplay, GDK_WINDOW_XWINDOW (root_window));
    }
#endif

  return result;
}

static void
gdk_window_cache_destroy (GdkWindowCache *cache)
{
  GdkWindow *root_window = gdk_screen_get_root_window (cache->screen);

  XSelectInput (GDK_WINDOW_XDISPLAY (root_window),
		GDK_WINDOW_XWINDOW (root_window),
		cache->old_event_mask);
  gdk_window_remove_filter (root_window, gdk_window_cache_filter, cache);
  gdk_window_remove_filter (NULL, gdk_window_cache_shape_filter, cache);

  gdk_error_trap_push ();

  g_list_foreach (cache->children, (GFunc)free_cache_child,
      gdk_screen_get_display (cache->screen));

  gdk_flush ();
  gdk_error_trap_pop ();

  g_list_free (cache->children);
  g_hash_table_destroy (cache->child_hash);

  g_free (cache);
}

static GdkWindowCache *
gdk_window_cache_ref (GdkWindowCache *cache)
{
  cache->ref_count += 1;

  return cache;
}

static void
gdk_window_cache_unref (GdkWindowCache *cache)
{
  g_assert (cache->ref_count > 0);

  cache->ref_count -= 1;

  if (cache->ref_count == 0)
    {
      window_caches = g_slist_remove (window_caches, cache);
      gdk_window_cache_destroy (cache);
    }
}

GdkWindowCache *
gdk_window_cache_get (GdkScreen *screen)
{
  GSList *list;
  GdkWindowCache *cache;

  for (list = window_caches; list; list = list->next)
    {
      cache = list->data;
      if (cache->screen == screen)
        return gdk_window_cache_ref (cache);
    }

  cache = gdk_window_cache_new (screen);

  window_caches = g_slist_prepend (window_caches, cache);

  return cache;
}


static gboolean
is_pointer_within_shape (GdkDisplay    *display,
                         GdkCacheChild *child,
                         gint           x_pos,
                         gint           y_pos)
{
  if (!child->shape_selected)
    {
      GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

      XShapeSelectInput (display_x11->xdisplay, child->xid, ShapeNotifyMask);
      child->shape_selected = TRUE;
    }
  if (!child->shape_valid)
    {
      GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
      GdkRegion *input_shape;

      child->shape = NULL;
      if (gdk_display_supports_shapes (display))
        child->shape = _xwindow_get_shape (display_x11->xdisplay,
                                           child->xid, ShapeBounding);
#ifdef ShapeInput
      input_shape = NULL;
      if (gdk_display_supports_input_shapes (display))
        input_shape = _xwindow_get_shape (display_x11->xdisplay,
                                          child->xid, ShapeInput);

      if (child->shape && input_shape)
        {
          gdk_region_intersect (child->shape, input_shape);
          gdk_region_destroy (input_shape);
        }
      else if (input_shape)
        {
          child->shape = input_shape;
        }
#endif

      child->shape_valid = TRUE;
    }

  return child->shape == NULL ||
         gdk_region_point_in (child->shape, x_pos, y_pos);
}

static Window
get_client_window_at_coords_recurse (GdkDisplay *display,
				     Window      win,
				     gboolean    is_toplevel,
				     gint        x,
				     gint        y)
{
  GdkChildInfoX11 *children;
  unsigned int nchildren;
  int i;
  gboolean found_child = FALSE;
  GdkChildInfoX11 child = { 0, };
  gboolean has_wm_state = FALSE;

  if (!_gdk_x11_get_window_child_info (display, win, TRUE,
				       is_toplevel? &has_wm_state : NULL,
				       &children, &nchildren))
    return None;

  if (has_wm_state)
    {
      g_free (children);

      return win;
    }

  for (i = nchildren - 1; (i >= 0) && !found_child; i--)
    {
      GdkChildInfoX11 *cur_child = &children[i];
       
      if ((cur_child->is_mapped) && (cur_child->window_class == InputOutput) &&
	  (x >= cur_child->x) && (x < cur_child->x + cur_child->width) &&
	  (y >= cur_child->y) && (y < cur_child->y + cur_child->height))
 	{
	  x -= cur_child->x;
	  y -= cur_child->y;
	  child = *cur_child;
	  found_child = TRUE;
 	}
    }
   
  g_free (children);
 
  if (found_child)
    {
      if (child.has_wm_state)
	return child.window;
      else
	return get_client_window_at_coords_recurse (display, child.window, FALSE, x, y);
    }
  else
    return None;
}

static Window 
get_client_window_at_coords (GdkWindowCache *cache,
			     Window          ignore,
			     gint            x_root,
			     gint            y_root)
{
  GList *tmp_list;
  Window retval = None;

  gdk_error_trap_push ();
  
  tmp_list = cache->children;

  while (tmp_list && !retval)
    {
      GdkCacheChild *child = tmp_list->data;

      if ((child->xid != ignore) && (child->mapped))
        {
          if ((x_root >= child->x) && (x_root < child->x + child->width) &&
              (y_root >= child->y) && (y_root < child->y + child->height))
            {
              GdkDisplay *display = gdk_screen_get_display (cache->screen);

              if (!is_pointer_within_shape (display, child,
                                            x_root - child->x,
                                            y_root - child->y))
                {
                  tmp_list = tmp_list->next;
                  continue;
                }

              retval = get_client_window_at_coords_recurse (display,
                  child->xid, TRUE,
                  x_root - child->x,
                  y_root - child->y);
              if (!retval)
                retval = child->xid;
            }
        }
      tmp_list = tmp_list->next;
    }

  gdk_error_trap_pop ();
  
  if (retval)
    return retval;
  else
    return GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (cache->screen));
}

/*************************************************************
 ***************************** MOTIF *************************
 *************************************************************/

/* values used in the message type for Motif DND */
enum {
    XmTOP_LEVEL_ENTER,
    XmTOP_LEVEL_LEAVE,
    XmDRAG_MOTION,
    XmDROP_SITE_ENTER,
    XmDROP_SITE_LEAVE,
    XmDROP_START,
    XmDROP_FINISH,
    XmDRAG_DROP_FINISH,
    XmOPERATION_CHANGED
};

/* Values used to specify type of protocol to use */
enum {
    XmDRAG_NONE,
    XmDRAG_DROP_ONLY,
    XmDRAG_PREFER_PREREGISTER,
    XmDRAG_PREREGISTER,
    XmDRAG_PREFER_DYNAMIC,
    XmDRAG_DYNAMIC,
    XmDRAG_PREFER_RECEIVER
};

/* Operation codes */
enum {
  XmDROP_NOOP,
  XmDROP_MOVE = 0x01,
  XmDROP_COPY = 0x02,
  XmDROP_LINK = 0x04
};

/* Drop site status */
enum {
  XmNO_DROP_SITE = 0x01,
  XmDROP_SITE_INVALID = 0x02,
  XmDROP_SITE_VALID = 0x03
};

/* completion status */
enum {
  XmDROP,
  XmDROP_HELP,
  XmDROP_CANCEL,
  XmDROP_INTERRUPT
};

/* Byte swapping routines. The motif specification leaves it
 * up to us to save a few bytes in the client messages
 */
static gchar local_byte_order = '\0';

#ifdef G_ENABLE_DEBUG
static void
print_target_list (GList *targets)
{
  while (targets)
    {
      gchar *name = gdk_atom_name (GDK_POINTER_TO_ATOM (targets->data));
      g_message ("\t%s", name);
      g_free (name);
      targets = targets->next;
    }
}
#endif /* G_ENABLE_DEBUG */

static void
init_byte_order (void)
{
  guint32 myint = 0x01020304;
  local_byte_order = (*(gchar *)&myint == 1) ? 'B' : 'l';
}

static guint16
card16_to_host (guint16 x, gchar byte_order) {
  if (byte_order == local_byte_order)
    return x;
  else
    return (x << 8) | (x >> 8);
}

static guint32
card32_to_host (guint32 x, gchar byte_order) {
  if (byte_order == local_byte_order)
    return x;
  else
    return (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
}

/* Motif packs together fields of varying length into the
 * client message. We can't rely on accessing these
 * through data.s[], data.l[], etc, because on some architectures
 * (i.e., Alpha) these won't be valid for format == 8. 
 */

#define MOTIF_XCLIENT_BYTE(xevent,i) \
  (xevent)->xclient.data.b[i]
#define MOTIF_XCLIENT_SHORT(xevent,i) \
  ((gint16 *)&((xevent)->xclient.data.b[0]))[i]
#define MOTIF_XCLIENT_LONG(xevent,i) \
  ((gint32 *)&((xevent)->xclient.data.b[0]))[i]

#define MOTIF_UNPACK_BYTE(xevent,i) MOTIF_XCLIENT_BYTE(xevent,i)
#define MOTIF_UNPACK_SHORT(xevent,i) \
  card16_to_host (MOTIF_XCLIENT_SHORT(xevent,i), MOTIF_XCLIENT_BYTE(xevent, 1))
#define MOTIF_UNPACK_LONG(xevent,i) \
  card32_to_host (MOTIF_XCLIENT_LONG(xevent,i), MOTIF_XCLIENT_BYTE(xevent, 1))

/***** Dest side ***********/

/* Property placed on source windows */
typedef struct _MotifDragInitiatorInfo {
  guint8 byte_order;
  guint8 protocol_version;
  guint16 targets_index;
  guint32 selection_atom;
} MotifDragInitiatorInfo;

/* Header for target table on the drag window */
typedef struct _MotifTargetTableHeader {
  guchar byte_order;
  guchar protocol_version;
  guint16 n_lists;
  guint32 total_size;
} MotifTargetTableHeader;

/* Property placed on target windows */
typedef struct _MotifDragReceiverInfo {
  guint8 byte_order;
  guint8 protocol_version;
  guint8 protocol_style;
  guint8 pad;
  guint32 proxy_window;
  guint16 num_drop_sites;
  guint16 padding;
  guint32 total_size;
} MotifDragReceiverInfo;

/* Target table handling */

static GdkFilterReturn
motif_drag_window_filter (GdkXEvent *xevent,
			  GdkEvent  *event,
			  gpointer data)
{
  XEvent *xev = (XEvent *)xevent;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (event->any.window); 
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  switch (xev->xany.type)
    {
    case DestroyNotify:
      display_x11->motif_drag_window = None;
      display_x11->motif_drag_gdk_window = NULL;
      break;
    case PropertyNotify:
      if (display_x11->motif_target_lists &&
	  (xev->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_TARGETS")))
	motif_read_target_table (display);
      break;
    }
  return GDK_FILTER_REMOVE;
}

static Window
motif_lookup_drag_window (GdkDisplay *display,
			  Display    *lookup_xdisplay)
{
  Window retval = None;
  gulong bytes_after, nitems;
  Atom type;
  gint format;
  guchar *data;

  XGetWindowProperty (lookup_xdisplay, RootWindow (lookup_xdisplay, 0),
		      gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_WINDOW"),
		      0, 1, FALSE,
		      XA_WINDOW, &type, &format, &nitems, &bytes_after,
		      &data);
  
  if ((format == 32) && (nitems == 1) && (bytes_after == 0))
    {
      retval = *(Window *)data;
      GDK_NOTE (DND, 
		g_message ("Found drag window %#lx\n", GDK_DISPLAY_X11 (display)->motif_drag_window));
    }

  if (type != None)
    XFree (data);

  return retval;
}

/* Finds the window where global Motif drag information is stored.
 * If it doesn't exist and 'create' is TRUE, create one.
 */
static Window 
motif_find_drag_window (GdkDisplay *display,
			gboolean    create)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  if (!display_x11->motif_drag_window)
    {
      Atom motif_drag_window_atom = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_WINDOW");
      display_x11->motif_drag_window = motif_lookup_drag_window (display, display_x11->xdisplay);
      
      if (!display_x11->motif_drag_window && create)
	{
	  /* Create a persistant window. (Copied from LessTif) */
	  
	  Display *persistant_xdisplay;
	  XSetWindowAttributes attr;
	  persistant_xdisplay = XOpenDisplay (gdk_display_get_name (display));
	  XSetCloseDownMode (persistant_xdisplay, RetainPermanent);

	  XGrabServer (persistant_xdisplay);
	  
	  display_x11->motif_drag_window = motif_lookup_drag_window (display, persistant_xdisplay);

	  if (!display_x11->motif_drag_window)
	    {
	      attr.override_redirect = True;
	      attr.event_mask = PropertyChangeMask;
	      
	       display_x11->motif_drag_window = 
		XCreateWindow (persistant_xdisplay, 
			       RootWindow (persistant_xdisplay, 0),
			      -100, -100, 10, 10, 0, 0,
			      InputOnly, (Visual *)CopyFromParent,
			      (CWOverrideRedirect | CWEventMask), &attr);
	      
	      GDK_NOTE (DND,
			g_message ("Created drag window %#lx\n", display_x11->motif_drag_window));
	      
	      XChangeProperty (persistant_xdisplay, 
			       RootWindow (persistant_xdisplay, 0),
			       motif_drag_window_atom, XA_WINDOW,
			       32, PropModeReplace,
			       (guchar *)&motif_drag_window_atom, 1);

	    }
	  XUngrabServer (persistant_xdisplay);
	  XCloseDisplay (persistant_xdisplay);
	}

      /* There is a miniscule race condition here if the drag window
       * gets destroyed exactly now.
       */
      if (display_x11->motif_drag_window)
	{
	  display_x11->motif_drag_gdk_window = 
	    gdk_window_foreign_new_for_display (display, display_x11->motif_drag_window);
	  gdk_window_add_filter (display_x11->motif_drag_gdk_window,
				 motif_drag_window_filter,
				 NULL);
	}
    }

  return display_x11->motif_drag_window;
}

static void 
motif_read_target_table (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  gulong bytes_after, nitems;
  Atom type;
  gint format;
  gint i, j;
  
  Atom motif_drag_targets_atom = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_TARGETS");

  if (display_x11->motif_target_lists)
    {
      for (i=0; i<display_x11->motif_n_target_lists; i++)
	g_list_free (display_x11->motif_target_lists[i]);
      
      g_free (display_x11->motif_target_lists);
      display_x11->motif_target_lists = NULL;
      display_x11->motif_n_target_lists = 0;
    }

  if (motif_find_drag_window (display, FALSE))
    {
      guchar *data;
      MotifTargetTableHeader *header = NULL;
      guchar *target_bytes = NULL;
      guchar *p;
      gboolean success = FALSE;

      gdk_error_trap_push ();
      XGetWindowProperty (display_x11->xdisplay, 
			  display_x11->motif_drag_window, 
			  motif_drag_targets_atom,
			  0, (sizeof(MotifTargetTableHeader)+3)/4, FALSE,
			  motif_drag_targets_atom, 
			  &type, &format, &nitems, &bytes_after,
			  &data);

      if (gdk_error_trap_pop () || (format != 8) || (nitems < sizeof (MotifTargetTableHeader)))
	goto error;

      header = (MotifTargetTableHeader *)data;

      header->n_lists = card16_to_host (header->n_lists, header->byte_order);
      header->total_size = card32_to_host (header->total_size, header->byte_order);

      gdk_error_trap_push ();
      XGetWindowProperty (display_x11->xdisplay, 
			  display_x11->motif_drag_window, 
		          motif_drag_targets_atom,
			  (sizeof(MotifTargetTableHeader)+3)/4, 
			  (header->total_size + 3)/4 - (sizeof(MotifTargetTableHeader) + 3)/4,
			  FALSE,
			  motif_drag_targets_atom, &type, &format, &nitems, 
			  &bytes_after, &target_bytes);
      
      if (gdk_error_trap_pop () || (format != 8) || (bytes_after != 0) || 
	  (nitems != header->total_size - sizeof(MotifTargetTableHeader)))
	  goto error;

      display_x11->motif_n_target_lists = header->n_lists;
      display_x11->motif_target_lists = g_new0 (GList *, display_x11->motif_n_target_lists);

      p = target_bytes;
      for (i=0; i<header->n_lists; i++)
	{
	  gint n_targets;
	  guint32 *targets;
	  
	  if (p + sizeof(guint16) - target_bytes > nitems)
	    goto error;

	  n_targets = card16_to_host (*(gushort *)p, header->byte_order);

	  /* We need to make a copy of the targets, since it may
	   * be unaligned
	   */
	  targets = g_new (guint32, n_targets);
	  memcpy (targets, p + sizeof(guint16), sizeof(guint32) * n_targets);

	  p +=  sizeof(guint16) + n_targets * sizeof(guint32);
	  if (p - target_bytes > nitems)
	    goto error;

	  for (j=0; j<n_targets; j++)
	    display_x11->motif_target_lists[i] = 
	      g_list_prepend (display_x11->motif_target_lists[i],
			      GUINT_TO_POINTER (card32_to_host (targets[j],
								header->byte_order)));
	  g_free (targets);
	  display_x11->motif_target_lists[i] = g_list_reverse (display_x11->motif_target_lists[i]);
	}

      success = TRUE;
      
    error:
      if (header)
	XFree (header);
      
      if (target_bytes)
	XFree (target_bytes);

      if (!success)
	{
	  if (display_x11->motif_target_lists)
	    {
	      g_free (display_x11->motif_target_lists);
	      display_x11->motif_target_lists = NULL;
	      display_x11->motif_n_target_lists = 0;
	    }
	  g_warning ("Error reading Motif target table\n");
	}
    }
}

static gint
targets_sort_func (gconstpointer a, gconstpointer b)
{
  return (GPOINTER_TO_UINT (a) < GPOINTER_TO_UINT (b)) ?
    -1 : ((GPOINTER_TO_UINT (a) > GPOINTER_TO_UINT (b)) ? 1 : 0);
}

/* Check if given (sorted) list is in the targets table */
static gint
motif_target_table_check (GdkDisplay *display,
			  GList      *sorted)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  GList *tmp_list1, *tmp_list2;
  gint i;

  for (i=0; i<display_x11->motif_n_target_lists; i++)
    {
      tmp_list1 = display_x11->motif_target_lists[i];
      tmp_list2 = sorted;
      
      while (tmp_list1 && tmp_list2)
	{
	  if (tmp_list1->data != tmp_list2->data)
	    break;

	  tmp_list1 = tmp_list1->next;
	  tmp_list2 = tmp_list2->next;
	}
      if (!tmp_list1 && !tmp_list2)	/* Found it */
	return i;
    }

  return -1;
}

static gint
motif_add_to_target_table (GdkDisplay *display,
			   GList      *targets) /* targets is list of GdkAtom */
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  GList *sorted = NULL;
  gint index = -1;
  gint i;
  GList *tmp_list;
  
  /* make a sorted copy of the list */
  
  while (targets)
    {
      Atom xatom = gdk_x11_atom_to_xatom_for_display (display, GDK_POINTER_TO_ATOM (targets->data));
      sorted = g_list_insert_sorted (sorted, GUINT_TO_POINTER (xatom), targets_sort_func);
      targets = targets->next;
    }

  /* First check if it is there already */

  if (display_x11->motif_target_lists)
    index = motif_target_table_check (display, sorted);

  /* We need to grab the server while doing this, to ensure
   * atomiticity. Ugh
   */

  if (index < 0)
    {
      /* We need to make sure that it exists _before_ we grab the
       * server, since we can't open a new connection after we
       * grab the server. 
       */
      motif_find_drag_window (display, TRUE);

      gdk_x11_display_grab (display);
      motif_read_target_table (display);
    
      /* Check again, in case it was added in the meantime */
      
      if (display_x11->motif_target_lists)
	index = motif_target_table_check (display, sorted);

      if (index < 0)
	{
	  guint32 total_size = 0;
	  guchar *data;
	  guchar *p;
	  guint16 *p16;
	  MotifTargetTableHeader *header;
	  
	  if (!display_x11->motif_target_lists)
	    {
	      display_x11->motif_target_lists = g_new (GList *, 1);
	      display_x11->motif_n_target_lists = 1;
	    }
	  else
	    {
	      display_x11->motif_n_target_lists++;
	      display_x11->motif_target_lists = g_realloc (display_x11->motif_target_lists,
							   sizeof(GList *) * display_x11->motif_n_target_lists);
	    }
	  display_x11->motif_target_lists[display_x11->motif_n_target_lists - 1] = sorted;
	  sorted = NULL;
	  index = display_x11->motif_n_target_lists - 1;

	  total_size = sizeof (MotifTargetTableHeader);
	  for (i = 0; i < display_x11->motif_n_target_lists ; i++)
	    total_size += sizeof(guint16) + sizeof(guint32) * g_list_length (display_x11->motif_target_lists[i]);

	  data = g_malloc (total_size);

	  header = (MotifTargetTableHeader *)data;
	  p = data + sizeof(MotifTargetTableHeader);

	  header->byte_order = local_byte_order;
	  header->protocol_version = 0;
	  header->n_lists = display_x11->motif_n_target_lists;
	  header->total_size = total_size;

	  for (i = 0; i < display_x11->motif_n_target_lists ; i++)
	    {
	      guint16 n_targets = g_list_length (display_x11->motif_target_lists[i]);
	      guint32 *targets = g_new (guint32, n_targets);
	      guint32 *p32 = targets;
	      
	      tmp_list = display_x11->motif_target_lists[i];
	      while (tmp_list)
		{
		  *p32 = GPOINTER_TO_UINT (tmp_list->data);
		  
		  tmp_list = tmp_list->next;
		  p32++;
		}

	      p16 = (guint16 *)p;
	      p += sizeof(guint16);

	      memcpy (p, targets, n_targets * sizeof(guint32));

	      *p16 = n_targets;
	      p += sizeof(guint32) * n_targets;
	      g_free (targets);
	    }

	  XChangeProperty (display_x11->xdisplay,
			   display_x11->motif_drag_window,
			   gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_TARGETS"),
			   gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_TARGETS"),
			   8, PropModeReplace,
			   data, total_size);
	}
      gdk_x11_display_ungrab (display);
    }

  g_list_free (sorted);
  return index;
}

/* Translate flags */

static void
motif_dnd_translate_flags (GdkDragContext *context, guint16 flags)
{
  guint recommended_op = flags & 0x000f;
  guint possible_ops = (flags & 0x0f0) >> 4;
  
  switch (recommended_op)
    {
    case XmDROP_MOVE:
      context->suggested_action = GDK_ACTION_MOVE;
      break;
    case XmDROP_COPY:
      context->suggested_action = GDK_ACTION_COPY;
      break;
    case XmDROP_LINK:
      context->suggested_action = GDK_ACTION_LINK;
      break;
    default:
      context->suggested_action = GDK_ACTION_COPY;
      break;
    }

  context->actions = 0;
  if (possible_ops & XmDROP_MOVE)
    context->actions |= GDK_ACTION_MOVE;
  if (possible_ops & XmDROP_COPY)
    context->actions |= GDK_ACTION_COPY;
  if (possible_ops & XmDROP_LINK)
    context->actions |= GDK_ACTION_LINK;
}

static guint16
motif_dnd_get_flags (GdkDragContext *context)
{
  guint16 flags = 0;
  
  switch (context->suggested_action)
    {
    case GDK_ACTION_MOVE:
      flags = XmDROP_MOVE;
      break;
    case GDK_ACTION_COPY:
      flags = XmDROP_COPY;
      break;
    case GDK_ACTION_LINK:
      flags = XmDROP_LINK;
      break;
    default:
      flags = XmDROP_NOOP;
      break;
    }
  
  if (context->actions & GDK_ACTION_MOVE)
    flags |= XmDROP_MOVE << 8;
  if (context->actions & GDK_ACTION_COPY)
    flags |= XmDROP_COPY << 8;
  if (context->actions & GDK_ACTION_LINK)
    flags |= XmDROP_LINK << 8;

  return flags;
}

/* Source Side */

static void
motif_set_targets (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  MotifDragInitiatorInfo info;
  gint i;
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  
  info.byte_order = local_byte_order;
  info.protocol_version = 0;
  
  info.targets_index = motif_add_to_target_table (display, context->targets);

  for (i=0; ; i++)
    {
      gchar buf[20];
      g_snprintf(buf, 20, "_GDK_SELECTION_%d", i);
      
      private->motif_selection = gdk_x11_get_xatom_by_name_for_display (display, buf);
      if (!XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display), private->motif_selection))
	break;
    }

  info.selection_atom = private->motif_selection;

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		   GDK_DRAWABLE_XID (context->source_window),
		   private->motif_selection,
		   gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_INITIATOR_INFO"),
		   8, PropModeReplace,
		   (guchar *)&info, sizeof (info));

  private->motif_targets_set = 1;
}

static guint32
motif_check_dest (GdkDisplay *display,
		  Window      win)
{
  gboolean retval = FALSE;
  guchar *data;
  MotifDragReceiverInfo *info;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  Atom motif_drag_receiver_info_atom = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_RECEIVER_INFO");

  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), win, 
		      motif_drag_receiver_info_atom, 
		      0, (sizeof(*info)+3)/4, False, AnyPropertyType,
		      &type, &format, &nitems, &after, 
		      &data);

  if (gdk_error_trap_pop() == 0)
    {
      if (type != None)
	{
	  info = (MotifDragReceiverInfo *)data;
	  
	  if ((format == 8) && (nitems == sizeof(*info)))
	    {
	      if ((info->protocol_version == 0) &&
		  ((info->protocol_style == XmDRAG_PREFER_PREREGISTER) ||
		   (info->protocol_style == XmDRAG_PREFER_DYNAMIC) ||
		   (info->protocol_style == XmDRAG_DYNAMIC)))
		retval = TRUE;
	    }
	  else
	    {
	      GDK_NOTE (DND, 
			g_warning ("Invalid Motif drag receiver property on window %ld\n", win));
	    }
	  
	  XFree (info);
	}
    }

  return retval ? win : None;
}

static void
motif_send_enter (GdkDragContext  *context,
		  guint32          time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  XEvent xev;

  if (!G_LIKELY (GDK_DISPLAY_X11 (display)->trusted_client))
    return; /* Motif Dnd requires getting properties on the root window */

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_AND_DROP_MESSAGE");
  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 0) = XmTOP_LEVEL_ENTER;
  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = 0;
  MOTIF_XCLIENT_LONG (&xev, 1) = time;
  MOTIF_XCLIENT_LONG (&xev, 2) = GDK_DRAWABLE_XID (context->source_window);

  if (!private->motif_targets_set)
    motif_set_targets (context);

  MOTIF_XCLIENT_LONG (&xev, 3) = private->motif_selection;
  MOTIF_XCLIENT_LONG (&xev, 4) = 0;

  if (!_gdk_send_xevent (display,
			 GDK_DRAWABLE_XID (context->dest_window),
			 FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));
}

static void
motif_send_leave (GdkDragContext  *context,
		  guint32          time)
{
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_AND_DROP_MESSAGE");
  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 0) = XmTOP_LEVEL_LEAVE;
  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = 0;
  MOTIF_XCLIENT_LONG (&xev, 1) = time;
  MOTIF_XCLIENT_LONG (&xev, 2) = 0;
  MOTIF_XCLIENT_LONG (&xev, 3) = 0;
  MOTIF_XCLIENT_LONG (&xev, 4) = 0;

  if (!_gdk_send_xevent (display,
			 GDK_DRAWABLE_XID (context->dest_window),
			 FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));
}

static gboolean
motif_send_motion (GdkDragContext  *context,
		    gint            x_root, 
		    gint            y_root,
		    GdkDragAction   action,
		    guint32         time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  gboolean retval;
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_AND_DROP_MESSAGE");
  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = motif_dnd_get_flags (context);
  MOTIF_XCLIENT_LONG (&xev, 1) = time;
  MOTIF_XCLIENT_LONG (&xev, 3) = 0;
  MOTIF_XCLIENT_LONG (&xev, 4) = 0;

  if ((context->suggested_action != private->old_action) ||
      (context->actions != private->old_actions))
    {
      MOTIF_XCLIENT_BYTE (&xev, 0) = XmOPERATION_CHANGED;

      /* private->drag_status = GDK_DRAG_STATUS_ACTION_WAIT; */
      retval = TRUE;
    }
  else
    {
      MOTIF_XCLIENT_BYTE (&xev, 0) = XmDRAG_MOTION;

      MOTIF_XCLIENT_SHORT (&xev, 4) = x_root;
      MOTIF_XCLIENT_SHORT (&xev, 5) = y_root;
      
      private->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
      retval = FALSE;
    }

  if (!_gdk_send_xevent (display,
			 GDK_DRAWABLE_XID (context->dest_window),
			 FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));

  return retval;
}

static void
motif_send_drop (GdkDragContext *context, guint32 time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_AND_DROP_MESSAGE");
  xev.xclient.format = 8;
  xev.xclient.window = GDK_DRAWABLE_XID (context->dest_window);

  MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_START;
  MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
  MOTIF_XCLIENT_SHORT (&xev, 1) = motif_dnd_get_flags (context);
  MOTIF_XCLIENT_LONG (&xev, 1)  = time;

  MOTIF_XCLIENT_SHORT (&xev, 4) = private->last_x;
  MOTIF_XCLIENT_SHORT (&xev, 5) = private->last_y;

  MOTIF_XCLIENT_LONG (&xev, 3)  = private->motif_selection;
  MOTIF_XCLIENT_LONG (&xev, 4)  = GDK_DRAWABLE_XID (context->source_window);

  if (!_gdk_send_xevent (display,
			 GDK_DRAWABLE_XID (context->dest_window),
			 FALSE, 0, &xev))
    GDK_NOTE (DND, 
	      g_message ("Send event to %lx failed",
			 GDK_DRAWABLE_XID (context->dest_window)));
}

/* Target Side */

static gboolean
motif_read_initiator_info (GdkDisplay *display,
			   Window      source_window, 
			   Atom        atom,
			   GList     **targets,
			   Atom       *selection)
{
  GList *tmp_list;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  MotifDragInitiatorInfo *initiator_info;
  
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), source_window, atom,
		      0, sizeof(*initiator_info), FALSE,
		      gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_INITIATOR_INFO"),
		      &type, &format, &nitems, &bytes_after,
		      &data);

  if (gdk_error_trap_pop () || (format != 8) || (nitems != sizeof (MotifDragInitiatorInfo)) || (bytes_after != 0))
    {
      g_warning ("Error reading initiator info\n");
      return FALSE;
    }

  initiator_info = (MotifDragInitiatorInfo *)data;

  motif_read_target_table (display);

  initiator_info->targets_index = 
    card16_to_host (initiator_info->targets_index, initiator_info->byte_order);
  initiator_info->selection_atom = 
    card32_to_host (initiator_info->selection_atom, initiator_info->byte_order);
  
  if (initiator_info->targets_index >= display_x11->motif_n_target_lists)
    {
      g_warning ("Invalid target index in TOP_LEVEL_ENTER MESSAGE");
      XFree (initiator_info);
      return FALSE;
    }

  tmp_list = g_list_last (display_x11->motif_target_lists[initiator_info->targets_index]);

  *targets = NULL;
  while (tmp_list)
    {
      GdkAtom atom = gdk_x11_xatom_to_atom_for_display (display, GPOINTER_TO_UINT (tmp_list->data));
      *targets = g_list_prepend (*targets, GDK_ATOM_TO_POINTER (atom));
      tmp_list = tmp_list->prev;
    }

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_DND)
    print_target_list (*targets);
#endif /* G_ENABLE_DEBUG */

  *selection = initiator_info->selection_atom;

  XFree (initiator_info);

  return TRUE;
}

static GdkDragContext *
motif_drag_context_new (GdkWindow *dest_window,
			guint32    timestamp,
			guint32    source_window,
			guint32    atom)
{
  GdkDragContext *new_context;
  GdkDragContextPrivateX11 *private;
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (dest_window);
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  /* FIXME, current_dest_drag really shouldn't be NULL'd
   * if we error below.
   */
  if (display_x11->current_dest_drag != NULL)
    {
      if (timestamp >= display_x11->current_dest_drag->start_time)
	{
	  g_object_unref (display_x11->current_dest_drag);
	  display_x11->current_dest_drag = NULL;
	}
      else
	return NULL;
    }

  new_context = gdk_drag_context_new ();
  private = PRIVATE_DATA (new_context);

  new_context->protocol = GDK_DRAG_PROTO_MOTIF;
  new_context->is_source = FALSE;

  new_context->source_window = gdk_window_lookup_for_display (display, source_window);
  if (new_context->source_window)
    g_object_ref (new_context->source_window);
  else
    {
      new_context->source_window = gdk_window_foreign_new_for_display (display, source_window);
      if (!new_context->source_window)
	{
	  g_object_unref (new_context);
	  return NULL;
	}
    }

  new_context->dest_window = dest_window;
  g_object_ref (dest_window);
  new_context->start_time = timestamp;

  if (!motif_read_initiator_info (GDK_WINDOW_DISPLAY (dest_window),
				  source_window,
				  atom,
				  &new_context->targets,
				  &private->motif_selection))
    {
      g_object_unref (new_context);
      return NULL;
    }

  return new_context;
}

/*
 * The MOTIF drag protocol has no real provisions for distinguishing
 * multiple simultaneous drops. If the sources grab the pointer
 * when doing drags, that shouldn't happen, in any case. If it
 * does, we can't do much except hope for the best.
 */

static GdkFilterReturn
motif_top_level_enter (GdkEvent *event,
		       guint16   flags, 
		       guint32   timestamp, 
		       guint32   source_window, 
		       guint32   atom)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_DRAWABLE_DISPLAY (event->any.window));
  GdkDragContext *new_context;

  GDK_NOTE(DND, g_message ("Motif DND top level enter: flags: %#4x time: %d source_widow: %#4x atom: %d",
			   flags, timestamp, source_window, atom));

  new_context = motif_drag_context_new (event->any.window, timestamp, source_window, atom);
  if (!new_context)
    return GDK_FILTER_REMOVE;

  event->dnd.type = GDK_DRAG_ENTER;
  event->dnd.context = new_context;
  g_object_ref (new_context);

  display_x11->current_dest_drag = new_context;

  return GDK_FILTER_TRANSLATE;
}

static GdkFilterReturn
motif_top_level_leave (GdkEvent *event,
		       guint16   flags, 
		       guint32   timestamp)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_DRAWABLE_DISPLAY (event->any.window));

  GDK_NOTE(DND, g_message ("Motif DND top level leave: flags: %#4x time: %d",
			   flags, timestamp));

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_MOTIF) &&
      (timestamp >= display_x11->current_dest_drag->start_time))
    {
      event->dnd.type = GDK_DRAG_LEAVE;
      /* Pass ownership of context to the event */
      event->dnd.context = display_x11->current_dest_drag;

      display_x11->current_dest_drag = NULL;

      return GDK_FILTER_TRANSLATE;
    }
  else
    return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_motion (GdkEvent *event,
	      guint16   flags, 
	      guint32   timestamp,
	      gint16    x_root,
	      gint16    y_root)
{
  GdkDragContextPrivateX11 *private;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_DRAWABLE_DISPLAY (event->any.window));
  
  GDK_NOTE(DND, g_message ("Motif DND motion: flags: %#4x time: %d (%d, %d)",
			   flags, timestamp, x_root, y_root));

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_MOTIF) &&
      (timestamp >= display_x11->current_dest_drag->start_time))
    {
      private = PRIVATE_DATA (display_x11->current_dest_drag);

      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.context = display_x11->current_dest_drag;
      g_object_ref (display_x11->current_dest_drag);

      event->dnd.time = timestamp;

      motif_dnd_translate_flags (display_x11->current_dest_drag, flags);

      event->dnd.x_root = x_root;
      event->dnd.y_root = y_root;

      private->last_x = x_root;
      private->last_y = y_root;

      private->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_operation_changed (GdkEvent *event,
			 guint16   flags, 
			 guint32   timestamp)
{
  GdkDragContextPrivateX11 *private;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_DRAWABLE_DISPLAY (event->any.window));
  GDK_NOTE(DND, g_message ("Motif DND operation changed: flags: %#4x time: %d",
			   flags, timestamp));

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_MOTIF) &&
      (timestamp >= display_x11->current_dest_drag->start_time))
    {
      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.send_event = FALSE;
      event->dnd.context = display_x11->current_dest_drag;
      g_object_ref (display_x11->current_dest_drag);

      event->dnd.time = timestamp;
      private = PRIVATE_DATA (display_x11->current_dest_drag);

      motif_dnd_translate_flags (display_x11->current_dest_drag, flags);

      event->dnd.x_root = private->last_x;
      event->dnd.y_root = private->last_y;

      private->drag_status = GDK_DRAG_STATUS_ACTION_WAIT;

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_drop_start (GdkEvent *event,
		  guint16   flags,
		  guint32   timestamp,
		  guint32   source_window,
		  guint32   atom,
		  gint16    x_root,
		  gint16    y_root)
{
  GdkDragContext *new_context;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_DRAWABLE_DISPLAY (event->any.window));

  GDK_NOTE(DND, g_message ("Motif DND drop start: flags: %#4x time: %d (%d, %d) source_widow: %#4x atom: %d",
			   flags, timestamp, x_root, y_root, source_window, atom));

  new_context = motif_drag_context_new (event->any.window, timestamp, source_window, atom);
  if (!new_context)
    return GDK_FILTER_REMOVE;

  motif_dnd_translate_flags (new_context, flags);

  event->dnd.type = GDK_DROP_START;
  event->dnd.context = new_context;
  event->dnd.time = timestamp;
  event->dnd.x_root = x_root;
  event->dnd.y_root = y_root;

  gdk_x11_window_set_user_time (event->any.window, timestamp);

  g_object_ref (new_context);
  display_x11->current_dest_drag = new_context;

  return GDK_FILTER_TRANSLATE;
}  

static GdkFilterReturn
motif_drag_status (GdkEvent *event,
		   guint16   flags,
		   guint32   timestamp)
{
  GdkDragContext *context;
  GdkDisplay *display;
  
  GDK_NOTE (DND, 
	    g_message ("Motif status message: flags %x", flags));

  display = gdk_drawable_get_display (event->any.window);
  if (!display)
    return GDK_FILTER_REMOVE;
  
  context = gdk_drag_context_find (display, TRUE, GDK_DRAWABLE_XID (event->any.window), None);

  if (context)
    {
      GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
      if ((private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT) ||
	  (private->drag_status == GDK_DRAG_STATUS_ACTION_WAIT))
	private->drag_status = GDK_DRAG_STATUS_DRAG;
      
      event->dnd.type = GDK_DRAG_STATUS;
      event->dnd.send_event = FALSE;
      event->dnd.context = context;
      g_object_ref (context);

      event->dnd.time = timestamp;

      if ((flags & 0x00f0) >> 4 == XmDROP_SITE_VALID)
	{
	  switch (flags & 0x000f)
	    {
	    case XmDROP_NOOP:
	      context->action = 0;
	      break;
	    case XmDROP_MOVE:
		context->action = GDK_ACTION_MOVE;
		break;
	    case XmDROP_COPY:
	      context->action = GDK_ACTION_COPY;
	      break;
	    case XmDROP_LINK:
	      context->action = GDK_ACTION_LINK;
	      break;
	    }
	}
      else
	context->action = 0;

      return GDK_FILTER_TRANSLATE;
    }
  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn
motif_dnd_filter (GdkXEvent *xev,
		  GdkEvent  *event,
		  gpointer data)
{
  XEvent *xevent = (XEvent *)xev;

  guint8 reason;
  guint16 flags;
  guint32 timestamp;
  guint32 source_window;
  Atom atom;
  gint16 x_root, y_root;
  gboolean is_reply;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;			/* Not for us */
  
  /* First read some fields common to all Motif DND messages */

  reason = MOTIF_UNPACK_BYTE (xevent, 0);
  flags = MOTIF_UNPACK_SHORT (xevent, 1);
  timestamp = MOTIF_UNPACK_LONG (xevent, 1);

  is_reply = ((reason & 0x80) != 0);

  switch (reason & 0x7f)
    {
    case XmTOP_LEVEL_ENTER:
      source_window = MOTIF_UNPACK_LONG (xevent, 2);
      atom = MOTIF_UNPACK_LONG (xevent, 3);
      return motif_top_level_enter (event, flags, timestamp, source_window, atom);
    case XmTOP_LEVEL_LEAVE:
      return motif_top_level_leave (event, flags, timestamp);

    case XmDRAG_MOTION:
      x_root = MOTIF_UNPACK_SHORT (xevent, 4);
      y_root = MOTIF_UNPACK_SHORT (xevent, 5);
      
      if (!is_reply)
	return motif_motion (event, flags, timestamp, x_root, y_root);
      else
	return motif_drag_status (event, flags, timestamp);

    case XmDROP_SITE_ENTER:
      return motif_drag_status (event, flags, timestamp);

    case XmDROP_SITE_LEAVE:
      return motif_drag_status (event,
				XmNO_DROP_SITE << 8 | XmDROP_NOOP, 
				timestamp);
    case XmDROP_START:
      x_root = MOTIF_UNPACK_SHORT (xevent, 4);
      y_root = MOTIF_UNPACK_SHORT (xevent, 5);
      atom = MOTIF_UNPACK_LONG (xevent, 3);
      source_window = MOTIF_UNPACK_LONG (xevent, 4);

      if (!is_reply)
	return motif_drop_start (event, flags, timestamp, source_window, atom, x_root, y_root);
      
     break;
    case XmOPERATION_CHANGED:
      if (!is_reply)
	return motif_operation_changed (event, flags, timestamp);
      else
	return motif_drag_status (event, flags, timestamp);

      break;
      /* To the best of my knowledge, these next two messages are 
       * not part of the protocol, though they are defined in
       * the header files.
       */
    case XmDROP_FINISH:
    case XmDRAG_DROP_FINISH:
      break;
    }

  return GDK_FILTER_REMOVE;
}

/*************************************************************
 ***************************** XDND **************************
 *************************************************************/

/* Utility functions */

static struct {
  const gchar *name;
  GdkAtom atom;
  GdkDragAction action;
} xdnd_actions_table[] = {
    { "XdndActionCopy",    None, GDK_ACTION_COPY },
    { "XdndActionMove",    None, GDK_ACTION_MOVE },
    { "XdndActionLink",    None, GDK_ACTION_LINK },
    { "XdndActionAsk",     None, GDK_ACTION_ASK  },
    { "XdndActionPrivate", None, GDK_ACTION_COPY },
  };

static const gint xdnd_n_actions = sizeof(xdnd_actions_table) / sizeof(xdnd_actions_table[0]);
static gboolean xdnd_actions_initialized = FALSE;

static void
xdnd_initialize_actions (void)
{
  gint i;
  
  xdnd_actions_initialized = TRUE;
  for (i=0; i < xdnd_n_actions; i++)
    xdnd_actions_table[i].atom = gdk_atom_intern_static_string (xdnd_actions_table[i].name);
}

static GdkDragAction
xdnd_action_from_atom (GdkDisplay *display,
		       Atom        xatom)
{
  GdkAtom atom;
  gint i;

  if (xatom == None)
    return 0;

  atom = gdk_x11_xatom_to_atom_for_display (display, xatom);

  if (!xdnd_actions_initialized)
    xdnd_initialize_actions();

  for (i=0; i<xdnd_n_actions; i++)
    if (atom == xdnd_actions_table[i].atom)
      return xdnd_actions_table[i].action;

  return 0;
}

static Atom
xdnd_action_to_atom (GdkDisplay    *display,
		     GdkDragAction  action)
{
  gint i;

  if (!xdnd_actions_initialized)
    xdnd_initialize_actions();

  for (i=0; i<xdnd_n_actions; i++)
    if (action == xdnd_actions_table[i].action)
      return gdk_x11_atom_to_xatom_for_display (display, xdnd_actions_table[i].atom);

  return None;
}

/* Source side */

static GdkFilterReturn 
xdnd_status_filter (GdkXEvent *xev,
		    GdkEvent  *event,
		    gpointer   data)
{
  GdkDisplay *display;
  XEvent *xevent = (XEvent *)xev;
  guint32 dest_window = xevent->xclient.data.l[0];
  guint32 flags = xevent->xclient.data.l[1];
  Atom action = xevent->xclient.data.l[4];
  GdkDragContext *context;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;			/* Not for us */
  
  GDK_NOTE (DND, 
	    g_message ("XdndStatus: dest_window: %#x  action: %ld",
		       dest_window, action));

  display = gdk_drawable_get_display (event->any.window);
  context = gdk_drag_context_find (display, TRUE, xevent->xclient.window, dest_window);
  
  if (context)
    {
      GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
      if (private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
	private->drag_status = GDK_DRAG_STATUS_DRAG;
      
      event->dnd.send_event = FALSE;
      event->dnd.type = GDK_DRAG_STATUS;
      event->dnd.context = context;
      g_object_ref (context);

      event->dnd.time = GDK_CURRENT_TIME; /* FIXME? */
      if (!(action != 0) != !(flags & 1))
	{
	  GDK_NOTE (DND,
		    g_warning ("Received status event with flags not corresponding to action!\n"));
	  action = 0;
	}

      context->action = xdnd_action_from_atom (display, action);

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn 
xdnd_finished_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  GdkDisplay *display;
  XEvent *xevent = (XEvent *)xev;
  guint32 dest_window = xevent->xclient.data.l[0];
  GdkDragContext *context;
  GdkDragContextPrivateX11 *private;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;			/* Not for us */
  
  GDK_NOTE (DND, 
	    g_message ("XdndFinished: dest_window: %#x", dest_window));

  display = gdk_drawable_get_display (event->any.window);
  context = gdk_drag_context_find (display, TRUE, xevent->xclient.window, dest_window);
  
  if (context)
    {
      private = PRIVATE_DATA (context);
      if (private->version == 5)
	private->drop_failed = xevent->xclient.data.l[1] == 0;
      
      event->dnd.type = GDK_DROP_FINISHED;
      event->dnd.context = context;
      g_object_ref (context);

      event->dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static void
xdnd_set_targets (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  Atom *atomlist;
  GList *tmp_list = context->targets;
  gint i;
  gint n_atoms = g_list_length (context->targets);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);

  atomlist = g_new (Atom, n_atoms);
  i = 0;
  while (tmp_list)
    {
      atomlist[i] = gdk_x11_atom_to_xatom_for_display (display, GDK_POINTER_TO_ATOM (tmp_list->data));
      tmp_list = tmp_list->next;
      i++;
    }

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		   GDK_DRAWABLE_XID (context->source_window),
		   gdk_x11_get_xatom_by_name_for_display (display, "XdndTypeList"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  private->xdnd_targets_set = 1;
}

static void
xdnd_set_actions (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  Atom *atomlist;
  gint i;
  gint n_atoms;
  guint actions;
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);

  if (!xdnd_actions_initialized)
    xdnd_initialize_actions();
  
  actions = context->actions;
  n_atoms = 0;
  for (i=0; i<xdnd_n_actions; i++)
    {
      if (actions & xdnd_actions_table[i].action)
	{
	  actions &= ~xdnd_actions_table[i].action;
	  n_atoms++;
	}
    }

  atomlist = g_new (Atom, n_atoms);

  actions = context->actions;
  n_atoms = 0;
  for (i=0; i<xdnd_n_actions; i++)
    {
      if (actions & xdnd_actions_table[i].action)
	{
	  actions &= ~xdnd_actions_table[i].action;
	  atomlist[n_atoms] = gdk_x11_atom_to_xatom_for_display (display, xdnd_actions_table[i].atom);
	  n_atoms++;
	}
    }

  XChangeProperty (GDK_DRAWABLE_XDISPLAY (context->source_window),
		   GDK_DRAWABLE_XID (context->source_window),
		   gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)atomlist, n_atoms);

  g_free (atomlist);

  private->xdnd_actions_set = TRUE;
  private->xdnd_actions = context->actions;
}

static void
send_client_message_async_cb (Window   window,
			      gboolean success,
			      gpointer data)
{
  GdkDragContext *context = data;
  GDK_NOTE (DND,
	    g_message ("Got async callback for #%lx, success = %d",
		       window, success));

  /* On failure, we immediately continue with the protocol
   * so we don't end up blocking for a timeout
   */
  if (!success &&
      context->dest_window &&
      window == GDK_WINDOW_XID (context->dest_window))
    {
      GdkEvent temp_event;
      GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
      context->action = 0;

      private->drag_status = GDK_DRAG_STATUS_DRAG;

      temp_event.dnd.type = GDK_DRAG_STATUS;
      temp_event.dnd.window = context->source_window;
      temp_event.dnd.send_event = TRUE;
      temp_event.dnd.context = context;
      temp_event.dnd.time = GDK_CURRENT_TIME;

      gdk_event_put (&temp_event);
    }

  g_object_unref (context);
}


static GdkDisplay *
gdk_drag_context_get_display (GdkDragContext *context)
{
  if (context->source_window)
    return GDK_DRAWABLE_DISPLAY (context->source_window);
  else if (context->dest_window)
    return GDK_DRAWABLE_DISPLAY (context->dest_window);

  g_assert_not_reached ();
  return NULL;
}

static void
send_client_message_async (GdkDragContext      *context,
			   Window               window, 
			   gboolean             propagate,
			   glong                event_mask,
			   XClientMessageEvent *event_send)
{
  GdkDisplay *display = gdk_drag_context_get_display (context);
  
  g_object_ref (context);

  _gdk_x11_send_client_message_async (display, window,
				      propagate, event_mask, event_send,
				      send_client_message_async_cb, context);
}

static gboolean
xdnd_send_xevent (GdkDragContext *context,
		  GdkWindow      *window, 
		  gboolean        propagate,
		  XEvent         *event_send)
{
  GdkDisplay *display = gdk_drag_context_get_display (context);
  Window xwindow;
  glong event_mask;

  g_assert (event_send->xany.type == ClientMessage);

  /* We short-circuit messages to ourselves */
  if (gdk_window_get_window_type (window) != GDK_WINDOW_FOREIGN)
    {
      gint i;
      
      for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
	{
	  if (gdk_x11_get_xatom_by_name_for_display (display, xdnd_filters[i].atom_name) ==
	      event_send->xclient.message_type)
	    {
	      GdkEvent temp_event;
	      temp_event.any.window = window;

	      if  ((*xdnd_filters[i].func) (event_send, &temp_event, NULL) == GDK_FILTER_TRANSLATE)
		{
		  gdk_event_put (&temp_event);
		  g_object_unref (temp_event.dnd.context);
		}
	      
	      return TRUE;
	    }
	}
    }

  xwindow = GDK_WINDOW_XWINDOW (window);
  
  if (_gdk_x11_display_is_root_window (display, xwindow))
    event_mask = ButtonPressMask;
  else
    event_mask = 0;
  
  send_client_message_async (context, xwindow, propagate, event_mask,
			     &event_send->xclient);

  return TRUE;
}
 
static void
xdnd_send_enter (GdkDragContext *context)
{
  XEvent xev;
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->dest_window);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndEnter");
  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = (private->version << 24); /* version */
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  GDK_NOTE(DND,
	   g_message ("Sending enter source window %#lx XDND protocol version %d\n",
		      GDK_DRAWABLE_XID (context->source_window), private->version));
  if (g_list_length (context->targets) > 3)
    {
      if (!private->xdnd_targets_set)
	xdnd_set_targets (context);
      xev.xclient.data.l[1] |= 1;
    }
  else
    {
      GList *tmp_list = context->targets;
      gint i = 2;

      while (tmp_list)
	{
	  xev.xclient.data.l[i] = gdk_x11_atom_to_xatom_for_display (display,
								     GDK_POINTER_TO_ATOM (tmp_list->data));
	  tmp_list = tmp_list->next;
	  i++;
	}
    }

  if (!xdnd_send_xevent (context, context->dest_window,
			 FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_leave (GdkDragContext *context)
{
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  XEvent xev;

  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndLeave");
  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent (context, context->dest_window,
			 FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_drop (GdkDragContext *context, guint32 time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndDrop");
  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = time;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  if (!xdnd_send_xevent (context, context->dest_window,
			 FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

static void
xdnd_send_motion (GdkDragContext *context,
		  gint            x_root, 
		  gint            y_root,
		  GdkDragAction   action,
		  guint32         time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndPosition");
  xev.xclient.format = 32;
  xev.xclient.window = private->drop_xid ? 
                           private->drop_xid : 
                           GDK_DRAWABLE_XID (context->dest_window);
  xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->source_window);
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (x_root << 16) | y_root;
  xev.xclient.data.l[3] = time;
  xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);

  if (!xdnd_send_xevent (context, context->dest_window,
			 FALSE, &xev))
    {
      GDK_NOTE (DND, 
		g_message ("Send event to %lx failed",
			   GDK_DRAWABLE_XID (context->dest_window)));
      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
  private->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
}

static guint32
xdnd_check_dest (GdkDisplay *display,
		 Window      win,
		 guint      *xdnd_version)
{
  gboolean retval = FALSE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  guchar *data;
  Atom *version;
  Window *proxy_data;
  Window proxy;
  Atom xdnd_proxy_atom = gdk_x11_get_xatom_by_name_for_display (display, "XdndProxy");
  Atom xdnd_aware_atom = gdk_x11_get_xatom_by_name_for_display (display, "XdndAware");

  proxy = None;

  gdk_error_trap_push ();
  
  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), win, 
			  xdnd_proxy_atom, 0, 
			  1, False, AnyPropertyType,
			  &type, &format, &nitems, &after, 
			  &data) == Success)
    {
      if (type != None)
	{
	  proxy_data = (Window *)data;
	  
	  if ((format == 32) && (nitems == 1))
	    {
	      proxy = *proxy_data;
	    }
	  else
	    GDK_NOTE (DND, 
		      g_warning ("Invalid XdndProxy "
				 "property on window %ld\n", win));
	  
	  XFree (proxy_data);
	}
      
      if ((XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), proxy ? proxy : win,
			       xdnd_aware_atom, 0, 
			       1, False, AnyPropertyType,
			       &type, &format, &nitems, &after, 
			       &data) == Success) &&
	  type != None)
	{
	  version = (Atom *)data;
	  
	  if ((format == 32) && (nitems == 1))
	    {
	      if (*version >= 3)
		retval = TRUE;
	      if (xdnd_version)
		*xdnd_version = *version;
	    }
	  else
	    GDK_NOTE (DND, 
		      g_warning ("Invalid XdndAware "
				 "property on window %ld\n", win));
	  
	  XFree (version);
	}
    }

  gdk_error_trap_pop ();
  
  return retval ? (proxy ? proxy : win) : None;
}

/* Target side */

static void
xdnd_read_actions (GdkDragContext *context)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (context->source_window);
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;

  gint i;
  
  PRIVATE_DATA (context)->xdnd_have_actions = FALSE;

  if (gdk_window_get_window_type (context->source_window) == GDK_WINDOW_FOREIGN)
    {
      /* Get the XdndActionList, if set */
      
      gdk_error_trap_push ();
      
      if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
			      GDK_DRAWABLE_XID (context->source_window),
			      gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList"),
			      0, 65536,
			      False, XA_ATOM, &type, &format, &nitems,
			      &after, &data) == Success &&
	  type == XA_ATOM)
	{
	  atoms = (Atom *)data;
	  
	  context->actions = 0;
	  
	  for (i=0; i<nitems; i++)
	    context->actions |= xdnd_action_from_atom (display, atoms[i]);
	  
	  PRIVATE_DATA (context)->xdnd_have_actions = TRUE;
	  
#ifdef G_ENABLE_DEBUG
	  if (_gdk_debug_flags & GDK_DEBUG_DND)
	    {
	      GString *action_str = g_string_new (NULL);
	      if (context->actions & GDK_ACTION_MOVE)
		g_string_append(action_str, "MOVE ");
	      if (context->actions & GDK_ACTION_COPY)
		g_string_append(action_str, "COPY ");
	      if (context->actions & GDK_ACTION_LINK)
		g_string_append(action_str, "LINK ");
	      if (context->actions & GDK_ACTION_ASK)
		g_string_append(action_str, "ASK ");
	      
	      g_message("Xdnd actions = %s", action_str->str);
	      g_string_free (action_str, TRUE);
	    }
#endif /* G_ENABLE_DEBUG */
	  
	}

      if (data)
	XFree (data);
      
      gdk_error_trap_pop ();
    }
  else
    {
      /* Local drag
       */
      GdkDragContext *source_context;

      source_context = gdk_drag_context_find (display, TRUE,
					      GDK_DRAWABLE_XID (context->source_window),
					      GDK_DRAWABLE_XID (context->dest_window));

      if (source_context)
	{
	  context->actions = source_context->actions;
	  PRIVATE_DATA (context)->xdnd_have_actions = TRUE;
	}
    }
}

/* We have to make sure that the XdndActionList we keep internally
 * is up to date with the XdndActionList on the source window
 * because we get no notification, because Xdnd wasn't meant
 * to continually send actions. So we select on PropertyChangeMask
 * and add this filter.
 */
static GdkFilterReturn 
xdnd_source_window_filter (GdkXEvent *xev,
			   GdkEvent  *event,
			   gpointer   cb_data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkDragContext *context = cb_data;
  GdkDisplay *display = GDK_WINDOW_DISPLAY(event->any.window);

  if ((xevent->xany.type == PropertyNotify) &&
      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "XdndActionList")))
    {
      xdnd_read_actions (context);

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

static void
xdnd_manage_source_filter (GdkDragContext *context,
			   GdkWindow      *window,
			   gboolean        add_filter)
{
  if (!GDK_WINDOW_DESTROYED (window) &&
      gdk_window_get_window_type (window) == GDK_WINDOW_FOREIGN)
    {
      gdk_error_trap_push ();

      if (add_filter)
	{
	  gdk_window_set_events (window,
				 gdk_window_get_events (window) |
				 GDK_PROPERTY_CHANGE_MASK);
	  gdk_window_add_filter (window, xdnd_source_window_filter, context);
	}
      else
	{
	  gdk_window_remove_filter (window,
				    xdnd_source_window_filter,
				    context);
	  /* Should we remove the GDK_PROPERTY_NOTIFY mask?
	   * but we might want it for other reasons. (Like
	   * INCR selection transactions).
	   */
	}
      
      gdk_display_sync (gdk_drawable_get_display (window));
      gdk_error_trap_pop ();  
    }
}

static void
base_precache_atoms (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (!display_x11->base_dnd_atoms_precached)
    {
      static const char *const precache_atoms[] = {
	"ENLIGHTENMENT_DESKTOP",
	"WM_STATE",
	"XdndAware",
	"XdndProxy",
	"_MOTIF_DRAG_RECEIVER_INFO"
      };

      _gdk_x11_precache_atoms (display,
			       precache_atoms, G_N_ELEMENTS (precache_atoms));

      display_x11->base_dnd_atoms_precached = TRUE;
    }
}

static void
xdnd_precache_atoms (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);

  if (!display_x11->xdnd_atoms_precached)
    {
      static const char *const precache_atoms[] = {
	"XdndActionAsk",
	"XdndActionCopy",
	"XdndActionLink",
	"XdndActionList",
	"XdndActionMove",
	"XdndActionPrivate",
	"XdndDrop",
	"XdndEnter",
	"XdndFinished",
	"XdndLeave",
	"XdndPosition",
	"XdndSelection",
	"XdndStatus",
	"XdndTypeList"
      };

      _gdk_x11_precache_atoms (display,
			       precache_atoms, G_N_ELEMENTS (precache_atoms));

      display_x11->xdnd_atoms_precached = TRUE;
    }
}

static GdkFilterReturn 
xdnd_enter_filter (GdkXEvent *xev,
		   GdkEvent  *event,
		   gpointer   cb_data)
{
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  XEvent *xevent = (XEvent *)xev;
  GdkDragContext *new_context;
  gint i;
  
  Atom type;
  int format;
  gulong nitems, after;
  guchar *data;
  Atom *atoms;

  guint32 source_window;
  gboolean get_types;
  gint version;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;			/* Not for us */

  source_window = xevent->xclient.data.l[0];
  get_types = ((xevent->xclient.data.l[1] & 1) != 0);
  version = (xevent->xclient.data.l[1] & 0xff000000) >> 24;
  
  display = GDK_DRAWABLE_DISPLAY (event->any.window);
  display_x11 = GDK_DISPLAY_X11 (display);

  xdnd_precache_atoms (display);

  GDK_NOTE (DND, 
	    g_message ("XdndEnter: source_window: %#x, version: %#x",
		       source_window, version));

  if (version < 3)
    {
      /* Old source ignore */
      GDK_NOTE (DND, g_message ("Ignored old XdndEnter message"));
      return GDK_FILTER_REMOVE;
    }
  
  if (display_x11->current_dest_drag != NULL)
    {
      g_object_unref (display_x11->current_dest_drag);
      display_x11->current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol = GDK_DRAG_PROTO_XDND;
  PRIVATE_DATA(new_context)->version = version;

  new_context->source_window = gdk_window_lookup_for_display (display, source_window);
  if (new_context->source_window)
    g_object_ref (new_context->source_window);
  else
    {
      new_context->source_window = gdk_window_foreign_new_for_display (display, source_window);
      if (!new_context->source_window)
	{
	  g_object_unref (new_context);
	  return GDK_FILTER_REMOVE;
	}
    }
  new_context->dest_window = event->any.window;
  g_object_ref (new_context->dest_window);

  new_context->targets = NULL;
  if (get_types)
    {
      gdk_error_trap_push ();
      XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (event->any.window), 
			  source_window, 
			  gdk_x11_get_xatom_by_name_for_display (display, "XdndTypeList"),
			  0, 65536,
			  False, XA_ATOM, &type, &format, &nitems,
			  &after, &data);

      if (gdk_error_trap_pop () || (format != 32) || (type != XA_ATOM))
	{
	  g_object_unref (new_context);

	  if (data)
	    XFree (data);

	  return GDK_FILTER_REMOVE;
	}

      atoms = (Atom *)data;

      for (i=0; i<nitems; i++)
	new_context->targets = 
	  g_list_append (new_context->targets,
			 GDK_ATOM_TO_POINTER (gdk_x11_xatom_to_atom_for_display (display,
										 atoms[i])));

      XFree(atoms);
    }
  else
    {
      for (i=0; i<3; i++)
	if (xevent->xclient.data.l[2+i])
	  new_context->targets =
	    g_list_append (new_context->targets,
			   GDK_ATOM_TO_POINTER (gdk_x11_xatom_to_atom_for_display (display, 
										   xevent->xclient.data.l[2+i])));
    }

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_DND)
    print_target_list (new_context->targets);
#endif /* G_ENABLE_DEBUG */

  xdnd_manage_source_filter (new_context, new_context->source_window, TRUE);
  xdnd_read_actions (new_context);

  event->dnd.type = GDK_DRAG_ENTER;
  event->dnd.context = new_context;
  g_object_ref (new_context);

  display_x11->current_dest_drag = new_context;

  return GDK_FILTER_TRANSLATE;
}

static GdkFilterReturn 
xdnd_leave_filter (GdkXEvent *xev,
		   GdkEvent  *event,
		   gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;

  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;			/* Not for us */
 
  GDK_NOTE (DND, 
	    g_message ("XdndLeave: source_window: %#x",
		       source_window));

  display = GDK_DRAWABLE_DISPLAY (event->any.window);
  display_x11 = GDK_DISPLAY_X11 (display);

  xdnd_precache_atoms (display);

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_DRAWABLE_XID (display_x11->current_dest_drag->source_window) == source_window))
    {
      event->dnd.type = GDK_DRAG_LEAVE;
      /* Pass ownership of context to the event */
      event->dnd.context = display_x11->current_dest_drag;

      display_x11->current_dest_drag = NULL;

      return GDK_FILTER_TRANSLATE;
    }
  else
    return GDK_FILTER_REMOVE;
}

static GdkFilterReturn 
xdnd_position_filter (GdkXEvent *xev,
		      GdkEvent  *event,
		      gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  gint16 x_root = xevent->xclient.data.l[2] >> 16;
  gint16 y_root = xevent->xclient.data.l[2] & 0xffff;
  guint32 time = xevent->xclient.data.l[3];
  Atom action = xevent->xclient.data.l[4];

  GdkDisplay *display;
  GdkDisplayX11 *display_x11;

   if (!event->any.window ||
       gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
     return GDK_FILTER_CONTINUE;			/* Not for us */
   
  GDK_NOTE (DND, 
	    g_message ("XdndPosition: source_window: %#x position: (%d, %d)  time: %d  action: %ld",
		       source_window, x_root, y_root, time, action));

  display = GDK_DRAWABLE_DISPLAY (event->any.window);
  display_x11 = GDK_DISPLAY_X11 (display);
  
  xdnd_precache_atoms (display);

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_DRAWABLE_XID (display_x11->current_dest_drag->source_window) == source_window))
    {
      event->dnd.type = GDK_DRAG_MOTION;
      event->dnd.context = display_x11->current_dest_drag;
      g_object_ref (display_x11->current_dest_drag);

      event->dnd.time = time;

      display_x11->current_dest_drag->suggested_action = xdnd_action_from_atom (display, action);
      
      if (!(PRIVATE_DATA (display_x11->current_dest_drag))->xdnd_have_actions)
	display_x11->current_dest_drag->actions = display_x11->current_dest_drag->suggested_action;

      event->dnd.x_root = x_root;
      event->dnd.y_root = y_root;

      (PRIVATE_DATA (display_x11->current_dest_drag))->last_x = x_root;
      (PRIVATE_DATA (display_x11->current_dest_drag))->last_y = y_root;
      
      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

static GdkFilterReturn 
xdnd_drop_filter (GdkXEvent *xev,
		  GdkEvent  *event,
		  gpointer   data)
{
  XEvent *xevent = (XEvent *)xev;
  guint32 source_window = xevent->xclient.data.l[0];
  guint32 time = xevent->xclient.data.l[2];
  GdkDisplay *display;
  GdkDisplayX11 *display_x11;
  
  if (!event->any.window ||
      gdk_window_get_window_type (event->any.window) == GDK_WINDOW_FOREIGN)
    return GDK_FILTER_CONTINUE;			/* Not for us */
  
  GDK_NOTE (DND, 
	    g_message ("XdndDrop: source_window: %#x  time: %d",
		       source_window, time));

  display = GDK_DRAWABLE_DISPLAY (event->any.window);
  display_x11 = GDK_DISPLAY_X11 (display);

  xdnd_precache_atoms (display);

  if ((display_x11->current_dest_drag != NULL) &&
      (display_x11->current_dest_drag->protocol == GDK_DRAG_PROTO_XDND) &&
      (GDK_DRAWABLE_XID (display_x11->current_dest_drag->source_window) == source_window))
    {
      GdkDragContextPrivateX11 *private;
      private = PRIVATE_DATA (display_x11->current_dest_drag);

      event->dnd.type = GDK_DROP_START;

      event->dnd.context = display_x11->current_dest_drag;
      g_object_ref (display_x11->current_dest_drag);

      event->dnd.time = time;
      event->dnd.x_root = private->last_x;
      event->dnd.y_root = private->last_y;

      gdk_x11_window_set_user_time (event->any.window, time);
      
      return GDK_FILTER_TRANSLATE;
    }

  return GDK_FILTER_REMOVE;
}

/*************************************************************
 ************************** Public API ***********************
 *************************************************************/
void
_gdk_dnd_init (GdkDisplay *display)
{
  int i;
  init_byte_order ();

  gdk_display_add_client_message_filter (
	display,
	gdk_atom_intern_static_string ("_MOTIF_DRAG_AND_DROP_MESSAGE"),
	motif_dnd_filter, NULL);
  
  for (i = 0; i < G_N_ELEMENTS (xdnd_filters); i++)
    {
      gdk_display_add_client_message_filter (
	display,
	gdk_atom_intern_static_string (xdnd_filters[i].atom_name),
	xdnd_filters[i].func, NULL);
    }
}		      

/* Source side */

static void
gdk_drag_do_leave (GdkDragContext *context, guint32 time)
{
  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_MOTIF:
	  motif_send_leave (context, time);
	  break;
	case GDK_DRAG_PROTO_XDND:
	  xdnd_send_leave (context);
	  break;
	case GDK_DRAG_PROTO_ROOTWIN:
	case GDK_DRAG_PROTO_NONE:
	default:
	  break;
	}

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

/**
 * gdk_drag_begin:
 * @window: the source window for this drag.
 * @targets: (transfer none) (element-type GdkAtom): the offered targets,
 *     as list of #GdkAtom<!-- -->s
 * 
 * Starts a drag and creates a new drag context for it.
 *
 * This function is called by the drag source.
 * 
 * Return value: a newly created #GdkDragContext.
 **/
GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
  GdkDragContext *new_context;
  
  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_WINDOW_IS_X11 (window), NULL);

  new_context = gdk_drag_context_new ();
  new_context->is_source = TRUE;
  new_context->source_window = window;
  g_object_ref (window);

  new_context->targets = g_list_copy (targets);
  precache_target_list (new_context);
  
  new_context->actions = 0;

  return new_context;
}

static GdkNativeWindow
_gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				    GdkNativeWindow  xid,
				    GdkDragProtocol *protocol,
				    guint           *version)

{
  GdkWindow *window;
  GdkNativeWindow retval;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);

  base_precache_atoms (display);

  /* Check for a local drag
   */
  window = gdk_window_lookup_for_display (display, xid);
  if (window &&
      gdk_window_get_window_type (window) != GDK_WINDOW_FOREIGN)
    {
      if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
	{
	  *protocol = GDK_DRAG_PROTO_XDND;
	  *version = 5;
	  xdnd_precache_atoms (display);
	  GDK_NOTE (DND, g_message ("Entering local Xdnd window %#x\n", xid));
	  return xid;
	}
      else if (_gdk_x11_display_is_root_window (display, (Window) xid))
	{
	  *protocol = GDK_DRAG_PROTO_ROOTWIN;
	  GDK_NOTE (DND, g_message ("Entering root window\n"));
	  return xid;
	}
    }
  else if ((retval = xdnd_check_dest (display, xid, version)))
    {
      *protocol = GDK_DRAG_PROTO_XDND;
      xdnd_precache_atoms (display);
      GDK_NOTE (DND, g_message ("Entering Xdnd window %#x\n", xid));
      return retval;
    }
  else if ((retval = motif_check_dest (display, xid)))
    {
      *protocol = GDK_DRAG_PROTO_MOTIF;
      GDK_NOTE (DND, g_message ("Entering motif window %#x\n", xid));
      return retval;
    }
  else
    {
      /* Check if this is a root window */

      gboolean rootwin = FALSE;
      Atom type = None;
      int format;
      unsigned long nitems, after;
      unsigned char *data;

      if (_gdk_x11_display_is_root_window (display, (Window) xid))
	rootwin = TRUE;

      gdk_error_trap_push ();
      
      if (!rootwin)
	{
	  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), xid,
				  gdk_x11_get_xatom_by_name_for_display (display, "ENLIGHTENMENT_DESKTOP"),
				  0, 0, False, AnyPropertyType,
				  &type, &format, &nitems, &after, &data) == Success &&
	      type != None)
	    {
	      XFree (data);
	      rootwin = TRUE;
	    }
	}

      /* Until I find out what window manager the next one is for,
       * I'm leaving it commented out. It's supported in the
       * xscreensaver sources, though.
       */
#if 0
      if (!rootwin)
	{
	  if (XGetWindowProperty (gdk_display, win,
				  gdk_x11_get_xatom_by_name ("__SWM_VROOT"),
				  0, 0, False, AnyPropertyType,
				  &type, &format, &nitems, &data) &&
	      type != None)
	    {
	      XFree (data);
	      rootwin = TRUE;
	    }
	}
#endif      

      gdk_error_trap_pop ();

      if (rootwin)
	{
	  GDK_NOTE (DND, g_message ("Entering root window\n"));
	  *protocol = GDK_DRAG_PROTO_ROOTWIN;
	  return xid;
	}
    }

  *protocol = GDK_DRAG_PROTO_NONE;

  return 0; /* a.k.a. None */
}

/**
 * gdk_drag_get_protocol_for_display:
 * @display: the #GdkDisplay where the destination window resides
 * @xid: the windowing system id of the destination window.
 * @protocol: location where the supported DND protocol is returned.
 * @returns: the windowing system id of the window where the drop should happen. This 
 *     may be @xid or the id of a proxy window, or zero if @xid doesn't
 *     support Drag and Drop.
 *
 * Finds out the DND protocol supported by a window.
 *
 * Since: 2.2
 */ 
GdkNativeWindow
gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				   GdkNativeWindow  xid,
				   GdkDragProtocol *protocol)
{
  return _gdk_drag_get_protocol_for_display (display, xid, protocol, NULL);
}

static GdkWindowCache *
drag_context_find_window_cache (GdkDragContext  *context,
				GdkScreen       *screen)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GSList *tmp_list;
  GdkWindowCache *cache;

  for (tmp_list = private->window_caches; tmp_list; tmp_list = tmp_list->next)
    {
      cache = tmp_list->data;
      if (cache->screen == screen)
	return cache;
    }

  cache = gdk_window_cache_get (screen);
  private->window_caches = g_slist_prepend (private->window_caches, cache);
  
  return cache;
}

/**
 * gdk_drag_find_window_for_screen:
 * @context: a #GdkDragContext
 * @drag_window: a window which may be at the pointer position, but
 * should be ignored, since it is put up by the drag source as an icon.
 * @screen: the screen where the destination window is sought. 
 * @x_root: the x position of the pointer in root coordinates.
 * @y_root: the y position of the pointer in root coordinates.
 * @dest_window: (out): location to store the destination window in.
 * @protocol: (out): location to store the DND protocol in.
 *
 * Finds the destination window and DND protocol to use at the
 * given pointer position.
 *
 * This function is called by the drag source to obtain the 
 * @dest_window and @protocol parameters for gdk_drag_motion().
 *
 * Since: 2.2
 **/
void
gdk_drag_find_window_for_screen (GdkDragContext  *context,
				 GdkWindow       *drag_window,
				 GdkScreen       *screen,
				 gint             x_root,
				 gint             y_root,
				 GdkWindow      **dest_window,
				 GdkDragProtocol *protocol)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);
  GdkWindowCache *window_cache;
  GdkDisplay *display;
  Window dest;

  g_return_if_fail (context != NULL);

  display = GDK_WINDOW_DISPLAY (context->source_window);

  window_cache = drag_context_find_window_cache (context, screen);

  dest = get_client_window_at_coords (window_cache,
				      drag_window && GDK_WINDOW_IS_X11 (drag_window) ? 
				      GDK_DRAWABLE_XID (drag_window) : None,
				      x_root, y_root);

  if (private->dest_xid != dest)
    {
      Window recipient;
      private->dest_xid = dest;

      /* Check if new destination accepts drags, and which protocol */

      /* There is some ugliness here. We actually need to pass
       * _three_ pieces of information to drag_motion - dest_window,
       * protocol, and the XID of the unproxied window. The first
       * two are passed explicitely, the third implicitly through
       * protocol->dest_xid.
       */
      if ((recipient = _gdk_drag_get_protocol_for_display (display, dest, 
							   protocol, &private->version)))
	{
	  *dest_window = gdk_window_lookup_for_display (display, recipient);
	  if (*dest_window)
	    g_object_ref (*dest_window);
	  else
	    *dest_window = gdk_window_foreign_new_for_display (display, recipient);
	}
      else
	*dest_window = NULL;
    }
  else
    {
      *dest_window = context->dest_window;
      if (*dest_window)
	g_object_ref (*dest_window);
      *protocol = context->protocol;
    }
}

/**
 * gdk_drag_motion:
 * @context: a #GdkDragContext.
 * @dest_window: the new destination window, obtained by 
 *     gdk_drag_find_window().
 * @protocol: the DND protocol in use, obtained by gdk_drag_find_window().
 * @x_root: the x position of the pointer in root coordinates.
 * @y_root: the y position of the pointer in root coordinates.
 * @suggested_action: the suggested action.
 * @possible_actions: the possible actions.
 * @time_: the timestamp for this operation.
 * 
 * Updates the drag context when the pointer moves or the 
 * set of actions changes.
 *
 * This function is called by the drag source.
 * 
 * Return value: FIXME
 **/
gboolean        
gdk_drag_motion (GdkDragContext *context,
		 GdkWindow      *dest_window,
		 GdkDragProtocol protocol,
		 gint            x_root, 
		 gint            y_root,
		 GdkDragAction   suggested_action,
		 GdkDragAction   possible_actions,
		 guint32         time)
{
  GdkDragContextPrivateX11 *private = PRIVATE_DATA (context);

  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (dest_window == NULL || GDK_WINDOW_IS_X11 (dest_window), FALSE);

  private->old_actions = context->actions;
  context->actions = possible_actions;
  
  if (private->old_actions != possible_actions)
    private->xdnd_actions_set = FALSE;
  
  if (protocol == GDK_DRAG_PROTO_XDND && private->version == 0)
    {
      /* This ugly hack is necessary since GTK+ doesn't know about
       * the XDND protocol version, and in particular doesn't know 
       * that gdk_drag_find_window_for_screen() has the side-effect 
       * of setting private->version, and therefore sometimes call
       * gdk_drag_motion() without a prior call to 
       * gdk_drag_find_window_for_screen(). This happens, e.g.
       * when GTK+ is proxying DND events to embedded windows.
       */ 
      if (dest_window)
	{
	  GdkDisplay *display = GDK_WINDOW_DISPLAY (dest_window);
	  
	  xdnd_check_dest (display, 
			   GDK_DRAWABLE_XID (dest_window), 
			   &private->version);
	}
    }

  /* When we have a Xdnd target, make sure our XdndActionList
   * matches the current actions;
   */
  if (protocol == GDK_DRAG_PROTO_XDND && !private->xdnd_actions_set)
    {
      if (dest_window)
	{
	  if (gdk_window_get_window_type (dest_window) == GDK_WINDOW_FOREIGN)
	    xdnd_set_actions (context);
	  else if (context->dest_window == dest_window)
	    {
	      GdkDisplay *display = GDK_WINDOW_DISPLAY (dest_window);
	      GdkDragContext *dest_context;
		    
	      dest_context = gdk_drag_context_find (display, FALSE,
						    GDK_DRAWABLE_XID (context->source_window),
						    GDK_DRAWABLE_XID (dest_window));

	      if (dest_context)
		{
		  dest_context->actions = context->actions;
		  PRIVATE_DATA (dest_context)->xdnd_have_actions = TRUE;
		}
	    }
	}
    }

  if (context->dest_window != dest_window)
    {
      GdkEvent temp_event;

      /* Send a leave to the last destination */
      gdk_drag_do_leave (context, time);
      private->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */

      if (dest_window)
	{
	  context->dest_window = dest_window;
	  private->drop_xid = private->dest_xid;
	  g_object_ref (context->dest_window);
	  context->protocol = protocol;

	  switch (protocol)
	    {
	    case GDK_DRAG_PROTO_MOTIF:
	      motif_send_enter (context, time);
	      break;

	    case GDK_DRAG_PROTO_XDND:
	      xdnd_send_enter (context);
	      break;

	    case GDK_DRAG_PROTO_ROOTWIN:
	    case GDK_DRAG_PROTO_NONE:
	    default:
	      break;
	    }
	  private->old_action = suggested_action;
	  context->suggested_action = suggested_action;
	  private->old_actions = possible_actions;
	}
      else
	{
	  context->dest_window = NULL;
	  private->drop_xid = None;
	  context->action = 0;
	}

      /* Push a status event, to let the client know that
       * the drag changed 
       */

      temp_event.dnd.type = GDK_DRAG_STATUS;
      temp_event.dnd.window = context->source_window;
      /* We use this to signal a synthetic status. Perhaps
       * we should use an extra field...
       */
      temp_event.dnd.send_event = TRUE;

      temp_event.dnd.context = context;
      temp_event.dnd.time = time;

      gdk_event_put (&temp_event);
    }
  else
    {
      private->old_action = context->suggested_action;
      context->suggested_action = suggested_action;
    }

  /* Send a drag-motion event */

  private->last_x = x_root;
  private->last_y = y_root;
      
  if (context->dest_window)
    {
      if (private->drag_status == GDK_DRAG_STATUS_DRAG)
	{
	  switch (context->protocol)
	    {
	    case GDK_DRAG_PROTO_MOTIF:
	      motif_send_motion (context, x_root, y_root, suggested_action, time);
	      break;
	      
	    case GDK_DRAG_PROTO_XDND:
	      xdnd_send_motion (context, x_root, y_root, suggested_action, time);
	      break;

	    case GDK_DRAG_PROTO_ROOTWIN:
	      {
		GdkEvent temp_event;
		/* GTK+ traditionally has used application/x-rootwin-drop,
		 * but the XDND spec specifies x-rootwindow-drop.
		 */
		GdkAtom target1 = gdk_atom_intern_static_string ("application/x-rootwindow-drop");
		GdkAtom target2 = gdk_atom_intern_static_string ("application/x-rootwin-drop");

		if (g_list_find (context->targets,
				 GDK_ATOM_TO_POINTER (target1)) ||
		    g_list_find (context->targets,
				 GDK_ATOM_TO_POINTER (target2)))
		  context->action = context->suggested_action;
		else
		  context->action = 0;

		temp_event.dnd.type = GDK_DRAG_STATUS;
		temp_event.dnd.window = context->source_window;
		temp_event.dnd.send_event = FALSE;
		temp_event.dnd.context = context;
		temp_event.dnd.time = time;

		gdk_event_put (&temp_event);
	      }
	      break;
	    case GDK_DRAG_PROTO_NONE:
	      g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_motion()");
	      break;
	    default:
	      break;
	    }
	}
      else
	return TRUE;
    }

  return FALSE;
}

/**
 * gdk_drag_drop:
 * @context: a #GdkDragContext.
 * @time_: the timestamp for this operation.
 * 
 * Drops on the current destination.
 * 
 * This function is called by the drag source.
 **/
void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_MOTIF:
	  motif_send_leave (context, time);
	  motif_send_drop (context, time);
	  break;
	  
	case GDK_DRAG_PROTO_XDND:
	  xdnd_send_drop (context, time);
	  break;

	case GDK_DRAG_PROTO_ROOTWIN:
	  g_warning ("Drops for GDK_DRAG_PROTO_ROOTWIN must be handled internally");
	  break;
	case GDK_DRAG_PROTO_NONE:
	  g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_drop()");
	  break;
	default:
	  break;
	}
    }
}

/**
 * gdk_drag_abort:
 * @context: a #GdkDragContext.
 * @time_: the timestamp for this operation.
 * 
 * Aborts a drag without dropping. 
 *
 * This function is called by the drag source.
 **/
void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  gdk_drag_do_leave (context, time);
}

/* Destination side */

/**
 * gdk_drag_status:
 * @context: a #GdkDragContext.
 * @action: the selected action which will be taken when a drop happens, 
 *    or 0 to indicate that a drop will not be accepted.
 * @time_: the timestamp for this operation.
 * 
 * Selects one of the actions offered by the drag source.
 *
 * This function is called by the drag destination in response to
 * gdk_drag_motion() called by the drag source.
 **/
void             
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  GdkDragContextPrivateX11 *private;
  XEvent xev;
  GdkDisplay *display;

  g_return_if_fail (context != NULL);

  private = PRIVATE_DATA (context);
  display = GDK_DRAWABLE_DISPLAY (context->source_window);
  
  context->action = action;

  if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      gboolean need_coords = FALSE;
      
      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display,
									"_MOTIF_DRAG_AND_DROP_MESSAGE");
      xev.xclient.format = 8;
      xev.xclient.window = GDK_DRAWABLE_XID (context->source_window);

      if (private->drag_status == GDK_DRAG_STATUS_ACTION_WAIT)
	{
	  MOTIF_XCLIENT_BYTE (&xev, 0) = XmOPERATION_CHANGED | 0x80;
	}
      else
	{
	  if ((action != 0) != (private->old_action != 0))
	    {
	      if (action != 0)
		{
		  MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_SITE_ENTER | 0x80;
		  need_coords = TRUE;
		}
	      else
		MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_SITE_LEAVE | 0x80;
	    }
	  else
	    {
	      MOTIF_XCLIENT_BYTE (&xev, 0) = XmDRAG_MOTION | 0x80;
	      need_coords = TRUE;
	    }
	}

      MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;

      switch (action)
	{
	case GDK_ACTION_MOVE:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_MOVE;
	  break;
	case GDK_ACTION_COPY:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_COPY;
	  break;
	case GDK_ACTION_LINK:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_LINK;
	  break;
	default:
	  MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_NOOP;
	  break;
	}

      if (action)
	MOTIF_XCLIENT_SHORT (&xev, 1) |= (XmDROP_SITE_VALID << 4);
      else
	MOTIF_XCLIENT_SHORT (&xev, 1) |= (XmNO_DROP_SITE << 4);

      MOTIF_XCLIENT_LONG (&xev, 1) = time;
      
      if (need_coords)
	{
	  MOTIF_XCLIENT_SHORT (&xev, 4) = private->last_x;
	  MOTIF_XCLIENT_SHORT (&xev, 5) = private->last_y;
	}
      else
	MOTIF_XCLIENT_LONG (&xev, 2) = 0;
      
      MOTIF_XCLIENT_LONG (&xev, 3) = 0;
      MOTIF_XCLIENT_LONG (&xev, 4) = 0;

      if (!_gdk_send_xevent (display,
			     GDK_DRAWABLE_XID (context->source_window),
			     FALSE, 0, &xev))
	GDK_NOTE (DND, 
		  g_message ("Send event to %lx failed",
			     GDK_DRAWABLE_XID (context->source_window)));
    }
  else if (context->protocol == GDK_DRAG_PROTO_XDND)
    {
      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndStatus");
      xev.xclient.format = 32;
      xev.xclient.window = GDK_DRAWABLE_XID (context->source_window);

      xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->dest_window);
      xev.xclient.data.l[1] = (action != 0) ? (2 | 1) : 0;
      xev.xclient.data.l[2] = 0;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = xdnd_action_to_atom (display, action);
      
      if (!xdnd_send_xevent (context, context->source_window,
			     FALSE, &xev))
	GDK_NOTE (DND, 
		  g_message ("Send event to %lx failed",
			     GDK_DRAWABLE_XID (context->source_window)));
    }

  private->old_action = action;
}

/**
 * gdk_drop_reply:
 * @context: a #GdkDragContext.
 * @ok: %TRUE if the drop is accepted.
 * @time_: the timestamp for this operation.
 * 
 * Accepts or rejects a drop. 
 *
 * This function is called by the drag destination in response
 * to a drop initiated by the drag source.
 **/
void 
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
  GdkDragContextPrivateX11 *private;

  g_return_if_fail (context != NULL);

  private = PRIVATE_DATA (context);
  
  if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display,
									"_MOTIF_DRAG_AND_DROP_MESSAGE");
      xev.xclient.format = 8;

      MOTIF_XCLIENT_BYTE (&xev, 0) = XmDROP_START | 0x80;
      MOTIF_XCLIENT_BYTE (&xev, 1) = local_byte_order;
      if (ok)
	MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_COPY | 
	                               (XmDROP_SITE_VALID << 4) |
	                               (XmDROP_NOOP << 8) |
	                               (XmDROP << 12);
      else
	MOTIF_XCLIENT_SHORT (&xev, 1) = XmDROP_NOOP | 
	                               (XmNO_DROP_SITE << 4) |
	                               (XmDROP_NOOP << 8) |
	                               (XmDROP_CANCEL << 12);
      MOTIF_XCLIENT_SHORT (&xev, 2) = private->last_x;
      MOTIF_XCLIENT_SHORT (&xev, 3) = private->last_y;
      MOTIF_XCLIENT_LONG (&xev, 2) = 0;
      MOTIF_XCLIENT_LONG (&xev, 3) = 0;
      MOTIF_XCLIENT_LONG (&xev, 4) = 0;
      
      _gdk_send_xevent (display,
			GDK_DRAWABLE_XID (context->source_window),
			FALSE, 0, &xev);
    }
}

/**
 * gdk_drop_finish:
 * @context: a #GtkDragContext.
 * @success: %TRUE if the data was successfully received.
 * @time_: the timestamp for this operation.
 * 
 * Ends the drag operation after a drop.
 *
 * This function is called by the drag destination.
 **/
void             
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
  g_return_if_fail (context != NULL);

  if (context->protocol == GDK_DRAG_PROTO_XDND)
    {
      GdkDisplay *display = GDK_DRAWABLE_DISPLAY (context->source_window);
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "XdndFinished");
      xev.xclient.format = 32;
      xev.xclient.window = GDK_DRAWABLE_XID (context->source_window);
      
      xev.xclient.data.l[0] = GDK_DRAWABLE_XID (context->dest_window);
      if (success)
	{
	  xev.xclient.data.l[1] = 1;
	  xev.xclient.data.l[2] = xdnd_action_to_atom (display, 
						       context->action);
	}
      else
	{
	  xev.xclient.data.l[1] = 0;
	  xev.xclient.data.l[2] = None;
	}
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = 0;

      if (!xdnd_send_xevent (context, context->source_window,
			     FALSE, &xev))
	GDK_NOTE (DND, 
		  g_message ("Send event to %lx failed",
			     GDK_DRAWABLE_XID (context->source_window)));
    }
}


void            
gdk_window_register_dnd (GdkWindow      *window)
{
  static const gulong xdnd_version = 5;
  MotifDragReceiverInfo info;
  Atom motif_drag_receiver_info_atom;
  GdkDisplay *display = gdk_drawable_get_display (window);

  g_return_if_fail (window != NULL);

  if (gdk_window_get_window_type (window) == GDK_WINDOW_OFFSCREEN)
    return;

  base_precache_atoms (display);

  if (g_object_get_data (G_OBJECT (window), "gdk-dnd-registered") != NULL)
    return;
  else
    g_object_set_data (G_OBJECT (window), "gdk-dnd-registered", GINT_TO_POINTER (TRUE));
  
  /* Set Motif drag receiver information property */

  motif_drag_receiver_info_atom = gdk_x11_get_xatom_by_name_for_display (display,
									 "_MOTIF_DRAG_RECEIVER_INFO");
  /* initialize to zero to avoid writing uninitialized data to socket */
  memset(&info, 0, sizeof(info));
  info.byte_order = local_byte_order;
  info.protocol_version = 0;
  info.protocol_style = XmDRAG_DYNAMIC;
  info.proxy_window = None;
  info.num_drop_sites = 0;
  info.total_size = sizeof(info);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_DRAWABLE_XID (window),
		   motif_drag_receiver_info_atom,
		   motif_drag_receiver_info_atom,
		   8, PropModeReplace,
		   (guchar *)&info,
		   sizeof (info));

  /* Set XdndAware */

  /* The property needs to be of type XA_ATOM, not XA_INTEGER. Blech */
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_DRAWABLE_XID (window),
		   gdk_x11_get_xatom_by_name_for_display (display, "XdndAware"),
		   XA_ATOM, 32, PropModeReplace,
		   (guchar *)&xdnd_version, 1);
}

/**
 * gdk_drag_get_selection:
 * @context: a #GdkDragContext.
 * 
 * Returns the selection atom for the current source window.
 * 
 * Return value: the selection atom.
 **/
GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  if (context->protocol == GDK_DRAG_PROTO_MOTIF)
    return gdk_x11_xatom_to_atom_for_display (GDK_DRAWABLE_DISPLAY (context->source_window),
					      (PRIVATE_DATA (context))->motif_selection);
  else if (context->protocol == GDK_DRAG_PROTO_XDND)
    return gdk_atom_intern_static_string ("XdndSelection");
  else
    return GDK_NONE;
}

/**
 * gdk_drag_drop_succeeded:
 * @context: a #GdkDragContext
 * 
 * Returns whether the dropped data has been successfully 
 * transferred. This function is intended to be used while 
 * handling a %GDK_DROP_FINISHED event, its return value is
 * meaningless at other times.
 * 
 * Return value: %TRUE if the drop was successful.
 *
 * Since: 2.6
 **/
gboolean 
gdk_drag_drop_succeeded (GdkDragContext *context)
{
  GdkDragContextPrivateX11 *private;

  g_return_val_if_fail (context != NULL, FALSE);

  private = PRIVATE_DATA (context);

  return !private->drop_failed;
}

#define __GDK_DND_X11_C__
#include "gdkaliasdef.c"
