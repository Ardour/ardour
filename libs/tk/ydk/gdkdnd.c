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
#include <gdkdnd.h>
#include <gdkdrawable.h>
#include <gdkdisplay.h>
#include "gdkalias.h"

/**
 * gdk_drag_find_window:
 * @context: a #GdkDragContext.
 * @drag_window: a window which may be at the pointer position, but
 *      should be ignored, since it is put up by the drag source as an icon.
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
 * Deprecated: 2.24: Use gdk_drag_find_window_for_screen() instead.
 **/
void
gdk_drag_find_window (GdkDragContext  *context,
		      GdkWindow       *drag_window,
		      gint             x_root,
		      gint             y_root,
		      GdkWindow      **dest_window,
		      GdkDragProtocol *protocol)
{
  gdk_drag_find_window_for_screen (context, drag_window,
				   gdk_drawable_get_screen (context->source_window),
				   x_root, y_root, dest_window, protocol);
}

/**
 * gdk_drag_get_protocol:
 * @xid: the windowing system id of the destination window.
 * @protocol: location where the supported DND protocol is returned.
 * 
 * Finds out the DND protocol supported by a window.
 * 
 * Return value: the windowing system specific id for the window where
 *    the drop should happen. This may be @xid or the id of a proxy
 *    window, or zero if @xid doesn't support Drag and Drop.
 *
 * Deprecated: 2.24: Use gdk_drag_get_protocol_for_display() instead
 **/
GdkNativeWindow
gdk_drag_get_protocol (GdkNativeWindow  xid,
		       GdkDragProtocol *protocol)
{
  return gdk_drag_get_protocol_for_display (gdk_display_get_default (), xid, protocol);
}

/**
 * gdk_drag_context_list_targets:
 * @context: a #GdkDragContext
 *
 * Retrieves the list of targets of the context.
 *
 * Return value: (transfer none) (element-type GdkAtom): a #GList of targets
 *
 * Since: 2.22
 **/
GList *
gdk_drag_context_list_targets (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->targets;
}

/**
 * gdk_drag_context_get_actions:
 * @context: a #GdkDragContext
 *
 * Determines the bitmask of actions proposed by the source if
 * gdk_drag_context_suggested_action() returns GDK_ACTION_ASK.
 *
 * Return value: the #GdkDragAction flags
 *
 * Since: 2.22
 **/
GdkDragAction
gdk_drag_context_get_actions (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_ACTION_DEFAULT);

  return context->actions;
}

/**
 * gdk_drag_context_get_suggested_action:
 * @context: a #GdkDragContext
 *
 * Determines the suggested drag action of the context.
 *
 * Return value: a #GdkDragAction value
 *
 * Since: 2.22
 **/
GdkDragAction
gdk_drag_context_get_suggested_action (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), 0);

  return context->suggested_action;
}

/**
 * gdk_drag_context_get_selected_action:
 * @context: a #GdkDragContext
 *
 * Determines the action chosen by the drag destination.
 *
 * Return value: a #GdkDragAction value
 *
 * Since: 2.22
 **/
GdkDragAction
gdk_drag_context_get_selected_action (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), 0);

  return context->action;
}

/**
 * gdk_drag_context_get_source_window:
 * @context: a #GdkDragContext
 *
 * Returns the #GdkWindow where the DND operation started.
 *
 * Return value: (transfer none): a #GdkWindow
 *
 * Since: 2.22
 **/
GdkWindow *
gdk_drag_context_get_source_window (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->source_window;
}

/**
 * gdk_drag_context_get_dest_window:
 * @context: a #GdkDragContext
 *
 * Returns the destination windw for the DND operation.
 *
 * Return value: (transfer none): a #GdkWindow
 *
 * Since: 2.24
 */
GdkWindow *
gdk_drag_context_get_dest_window (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), NULL);

  return context->dest_window;
}

/**
 * gdk_drag_context_get_protocol:
 * @context: a #GdkDragContext
 *
 * Returns the drag protocol thats used by this context.
 *
 * Returns: the drag protocol
 *
 * Since: 2.24
 */
GdkDragProtocol
gdk_drag_context_get_protocol (GdkDragContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_DRAG_PROTO_NONE);

  return context->protocol;
}

#define __GDK_DND_C__
#include "gdkaliasdef.c"
