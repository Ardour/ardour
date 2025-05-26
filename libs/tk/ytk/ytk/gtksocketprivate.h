/* GTK - The GIMP Toolkit
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_SOCKET_PRIVATE_H__
#define __GTK_SOCKET_PRIVATE_H__

typedef struct _GtkSocketPrivate GtkSocketPrivate;

struct _GtkSocketPrivate
{
  gint resize_count;
};

/* In gtksocket.c: */
GtkSocketPrivate *_gtk_socket_get_private (GtkSocket *socket);

void _gtk_socket_add_grabbed_key  (GtkSocket        *socket,
				   guint             keyval,
				   GdkModifierType   modifiers);
void _gtk_socket_remove_grabbed_key (GtkSocket      *socket,
				     guint           keyval,
				     GdkModifierType modifiers);
void _gtk_socket_claim_focus 	  (GtkSocket        *socket,
			     	   gboolean          send_event);
void _gtk_socket_add_window  	  (GtkSocket        *socket,
			     	   GdkNativeWindow   xid,
			     	   gboolean          need_reparent);
void _gtk_socket_end_embedding    (GtkSocket        *socket);

void _gtk_socket_handle_map_request     (GtkSocket        *socket);
void _gtk_socket_unmap_notify           (GtkSocket        *socket);
void _gtk_socket_advance_toplevel_focus (GtkSocket        *socket,
					 GtkDirectionType  direction);

/* In backend-specific file: */

/*
 * _gtk_socket_windowing_get_id:
 *
 * @socket: a #GtkSocket
 *
 * Returns the native windowing system identifier for the plug's window.
 */
GdkNativeWindow _gtk_socket_windowing_get_id (GtkSocket *socket);

/*
 * _gtk_socket_windowing_realize_window:
 *
 */
void _gtk_socket_windowing_realize_window (GtkSocket *socket);

/*
 * _gtk_socket_windowing_end_embedding_toplevel:
 *
 */
void _gtk_socket_windowing_end_embedding_toplevel (GtkSocket *socket);

/*
 * _gtk_socket_windowing_size_request:
 *
 */
void _gtk_socket_windowing_size_request (GtkSocket *socket);

/*
 * _gtk_socket_windowing_send_key_event:
 *
 */
void _gtk_socket_windowing_send_key_event (GtkSocket *socket,
					   GdkEvent  *gdk_event,
					   gboolean   mask_key_presses);

/*
 * _gtk_socket_windowing_focus_change:
 *
 */
void _gtk_socket_windowing_focus_change (GtkSocket *socket,
					 gboolean   focus_in);

/*
 * _gtk_socket_windowing_update_active:
 *
 */
void _gtk_socket_windowing_update_active (GtkSocket *socket,
					  gboolean   active);

/*
 * _gtk_socket_windowing_update_modality:
 *
 */
void _gtk_socket_windowing_update_modality (GtkSocket *socket,
					    gboolean   modality);

/*
 * _gtk_socket_windowing_focus:
 *
 */
void _gtk_socket_windowing_focus (GtkSocket *socket,
				  GtkDirectionType direction);

/*
 * _gtk_socket_windowing_send_configure_event:
 *
 */
void _gtk_socket_windowing_send_configure_event (GtkSocket *socket);

/*
 * _gtk_socket_windowing_select_plug_window_input:
 *
 * Asks the windowing system to send necessary events related to the
 * plug window to the socket window. Called only for out-of-process
 * embedding.
 */
void _gtk_socket_windowing_select_plug_window_input (GtkSocket *socket);

/*
 * _gtk_socket_windowing_embed_get_info:
 *
 * Gets whatever information necessary about an out-of-process plug
 * window.
 */
void _gtk_socket_windowing_embed_get_info (GtkSocket *socket);

/*
 * _gtk_socket_windowing_embed_notify:
 *
 */
void _gtk_socket_windowing_embed_notify (GtkSocket *socket);

/*
 * _gtk_socket_windowing_embed_get_focus_wrapped:
 *
 */
gboolean _gtk_socket_windowing_embed_get_focus_wrapped (void);

/*
 * _gtk_socket_windowing_embed_set_focus_wrapped:
 *
 */
void _gtk_socket_windowing_embed_set_focus_wrapped (void);

/*
 * _gtk_socket_windowing_filter_func:
 *
 */
GdkFilterReturn _gtk_socket_windowing_filter_func (GdkXEvent *gdk_xevent,
						   GdkEvent  *event,
						   gpointer   data);

#endif /* __GTK_SOCKET_PRIVATE_H__ */
