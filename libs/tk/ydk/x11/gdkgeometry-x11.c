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

#include "config.h"
#include "gdk.h"		/* For gdk_rectangle_intersect */
#include "gdkprivate-x11.h"
#include "gdkx.h"
#include "gdkregion.h"
#include "gdkinternals.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkwindow-x11.h"
#include "gdkalias.h"

typedef struct _GdkWindowQueueItem GdkWindowQueueItem;
typedef struct _GdkWindowParentPos GdkWindowParentPos;

typedef enum {
  GDK_WINDOW_QUEUE_TRANSLATE,
  GDK_WINDOW_QUEUE_ANTIEXPOSE
} GdkWindowQueueType;

struct _GdkWindowQueueItem
{
  GdkWindow *window;
  gulong serial;
  GdkWindowQueueType type;
  union {
    struct {
      GdkRegion *area;
      gint dx;
      gint dy;
    } translate;
    struct {
      GdkRegion *area;
    } antiexpose;
  } u;
};

void
_gdk_window_move_resize_child (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowObject *obj;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  obj = GDK_WINDOW_OBJECT (window);

  if (width > 65535 ||
      height > 65535)
    {
      g_warning ("Native children wider or taller than 65535 pixels are not supported");

      if (width > 65535)
	width = 65535;
      if (height > 65535)
	height = 65535;
    }

  obj->x = x;
  obj->y = y;
  obj->width = width;
  obj->height = height;

  /* We don't really care about origin overflow, because on overflow
     the window won't be visible anyway and thus it will be shaped
     to nothing */

  _gdk_x11_window_tmp_unset_parent_bg (window);
  _gdk_x11_window_tmp_unset_bg (window, TRUE);
  XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window),
		     GDK_WINDOW_XID (window),
		     obj->x + obj->parent->abs_x,
		     obj->y + obj->parent->abs_y,
		     width, height);
  _gdk_x11_window_tmp_reset_parent_bg (window);
  _gdk_x11_window_tmp_reset_bg (window, TRUE);
}

static Bool
expose_serial_predicate (Display *xdisplay,
			 XEvent  *xev,
			 XPointer arg)
{
  gulong *serial = (gulong *)arg;

  if (xev->xany.type == Expose || xev->xany.type == GraphicsExpose)
    *serial = MIN (*serial, xev->xany.serial);

  return False;
}

/* Find oldest possible serial for an outstanding expose event
 */
static gulong
find_current_serial (Display *xdisplay)
{
  XEvent xev;
  gulong serial = NextRequest (xdisplay);
  
  XSync (xdisplay, False);

  XCheckIfEvent (xdisplay, &xev, expose_serial_predicate, (XPointer)&serial);

  return serial;
}

static void
queue_delete_link (GQueue *queue,
		   GList  *link)
{
  if (queue->tail == link)
    queue->tail = link->prev;
  
  queue->head = g_list_remove_link (queue->head, link);
  g_list_free_1 (link);
  queue->length--;
}

static void
queue_item_free (GdkWindowQueueItem *item)
{
  if (item->window)
    {
      g_object_remove_weak_pointer (G_OBJECT (item->window),
				    (gpointer *)&(item->window));
    }
  
  if (item->type == GDK_WINDOW_QUEUE_ANTIEXPOSE)
    gdk_region_destroy (item->u.antiexpose.area);
  else
    {
      if (item->u.translate.area)
	gdk_region_destroy (item->u.translate.area);
    }
  
  g_free (item);
}

static void
gdk_window_queue (GdkWindow          *window,
		  GdkWindowQueueItem *item)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
  
  if (!display_x11->translate_queue)
    display_x11->translate_queue = g_queue_new ();

  /* Keep length of queue finite by, if it grows too long,
   * figuring out the latest relevant serial and discarding
   * irrelevant queue items.
   */
  if (display_x11->translate_queue->length >= 64)
    {
      gulong serial = find_current_serial (GDK_WINDOW_XDISPLAY (window));
      GList *tmp_list = display_x11->translate_queue->head;
      
      while (tmp_list)
	{
	  GdkWindowQueueItem *item = tmp_list->data;
	  GList *next = tmp_list->next;
	  
	  /* an overflow-safe (item->serial < serial) */
	  if (item->serial - serial > (gulong) G_MAXLONG)
	    {
	      queue_delete_link (display_x11->translate_queue, tmp_list);
	      queue_item_free (item);
	    }

	  tmp_list = next;
	}
    }

  /* Catch the case where someone isn't processing events and there
   * is an event stuck in the event queue with an old serial:
   * If we can't reduce the queue length by the above method,
   * discard anti-expose items. (We can't discard translate
   * items 
   */
  if (display_x11->translate_queue->length >= 64)
    {
      GList *tmp_list = display_x11->translate_queue->head;
      
      while (tmp_list)
	{
	  GdkWindowQueueItem *item = tmp_list->data;
	  GList *next = tmp_list->next;
	  
	  if (item->type == GDK_WINDOW_QUEUE_ANTIEXPOSE)
	    {
	      queue_delete_link (display_x11->translate_queue, tmp_list);
	      queue_item_free (item);
	    }

	  tmp_list = next;
	}
    }

  item->window = window;
  item->serial = NextRequest (GDK_WINDOW_XDISPLAY (window));
  
  g_object_add_weak_pointer (G_OBJECT (window),
			     (gpointer *)&(item->window));

  g_queue_push_tail (display_x11->translate_queue, item);
}

void
_gdk_x11_window_queue_translation (GdkWindow *window,
				   GdkGC     *gc,
				   GdkRegion *area,
				   gint       dx,
				   gint       dy)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->type = GDK_WINDOW_QUEUE_TRANSLATE;
  item->u.translate.area = area ? gdk_region_copy (area) : NULL;
  item->u.translate.dx = dx;
  item->u.translate.dy = dy;

  /* Ensure that the gc is flushed so that we get the right
     serial from NextRequest in gdk_window_queue, i.e. the
     the serial for the XCopyArea, not the ones from flushing
     the gc. */
  _gdk_x11_gc_flush (gc);
  gdk_window_queue (window, item);
}

gboolean
_gdk_x11_window_queue_antiexpose (GdkWindow *window,
				  GdkRegion *area)
{
  GdkWindowQueueItem *item = g_new (GdkWindowQueueItem, 1);
  item->type = GDK_WINDOW_QUEUE_ANTIEXPOSE;
  item->u.antiexpose.area = area;

  gdk_window_queue (window, item);

  return TRUE;
}

void
_gdk_window_process_expose (GdkWindow    *window,
			    gulong        serial,
			    GdkRectangle *area)
{
  GdkRegion *invalidate_region = gdk_region_rectangle (area);
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  if (display_x11->translate_queue)
    {
      GList *tmp_list = display_x11->translate_queue->head;

      while (tmp_list)
	{
	  GdkWindowQueueItem *item = tmp_list->data;
          GList *next = tmp_list->next;

	  /* an overflow-safe (serial < item->serial) */
	  if (serial - item->serial > (gulong) G_MAXLONG)
	    {
	      if (item->window == window)
		{
		  if (item->type == GDK_WINDOW_QUEUE_TRANSLATE)
		    {
		      if (item->u.translate.area)
			{
			  GdkRegion *intersection;

			  intersection = gdk_region_copy (invalidate_region);
			  gdk_region_intersect (intersection, item->u.translate.area);
			  gdk_region_subtract (invalidate_region, intersection);
			  gdk_region_offset (intersection, item->u.translate.dx, item->u.translate.dy);
			  gdk_region_union (invalidate_region, intersection);
			  gdk_region_destroy (intersection);
			}
		      else
			gdk_region_offset (invalidate_region, item->u.translate.dx, item->u.translate.dy);
		    }
		  else		/* anti-expose */
		    {
		      gdk_region_subtract (invalidate_region, item->u.antiexpose.area);
		    }
		}
	    }
	  else
	    {
	      queue_delete_link (display_x11->translate_queue, tmp_list);
	      queue_item_free (item);
	    }
	  tmp_list = next;
	}
    }

  if (!gdk_region_empty (invalidate_region))
    _gdk_window_invalidate_for_expose (window, invalidate_region);

  gdk_region_destroy (invalidate_region);
}

#define __GDK_GEOMETRY_X11_C__
#include "gdkaliasdef.c"
