/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005 Novell, Inc.
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

/* By Tor Lillqvist <tml@novell.com> 2005 */

/*
 * Modified by the GTK+ Team and others 1997-2005.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <string.h>

#include "gtkwindow.h"
#include "gtkplug.h"
#include "gtkprivate.h"
#include "gtksocket.h"
#include "gtksocketprivate.h"

#include "gdk/gdkwin32.h"

#include "gtkwin32embed.h"
#include "gtkalias.h"

GdkNativeWindow
_gtk_socket_windowing_get_id (GtkSocket *socket)
{
  g_return_val_if_fail (GTK_IS_SOCKET (socket), 0);
  g_return_val_if_fail (GTK_WIDGET_ANCHORED (socket), 0);

  if (!gtk_widget_get_realized (socket))
    gtk_widget_realize (GTK_WIDGET (socket));

  return (GdkNativeWindow) GDK_WINDOW_HWND (GTK_WIDGET (socket)->window);
}

void
_gtk_socket_windowing_realize_window (GtkSocket *socket)
{
  /* XXX Anything needed? */
}

void
_gtk_socket_windowing_end_embedding_toplevel (GtkSocket *socket)
{
  gtk_window_remove_embedded_xid (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (socket))),
				  GDK_WINDOW_HWND (socket->plug_window));
}

void
_gtk_socket_windowing_size_request (GtkSocket *socket)
{
  MINMAXINFO mmi;

  socket->request_width = 1;
  socket->request_height = 1;
  
  mmi.ptMaxSize.x = mmi.ptMaxSize.y = 16000; /* ??? */
  mmi.ptMinTrackSize.x = mmi.ptMinTrackSize.y = 1;
  mmi.ptMaxTrackSize.x = mmi.ptMaxTrackSize.y = 16000; /* ??? */
  mmi.ptMaxPosition.x = mmi.ptMaxPosition.y = 0;

  if (SendMessage (GDK_WINDOW_HWND (socket->plug_window), WM_GETMINMAXINFO,
		   0, (LPARAM) &mmi) == 0)
    {
      socket->request_width = mmi.ptMinTrackSize.x;
      socket->request_height = mmi.ptMinTrackSize.y;
    }
  socket->have_size = TRUE;
}

void
_gtk_socket_windowing_send_key_event (GtkSocket *socket,
				      GdkEvent  *gdk_event,
				      gboolean   mask_key_presses)
{
  PostMessage (GDK_WINDOW_HWND (socket->plug_window),
	       (gdk_event->type == GDK_KEY_PRESS ? WM_KEYDOWN : WM_KEYUP),
	       gdk_event->key.hardware_keycode, 0);
}

void
_gtk_socket_windowing_focus_change (GtkSocket *socket,
				    gboolean   focus_in)
{
  if (focus_in)
    _gtk_win32_embed_send_focus_message (socket->plug_window,
					 GTK_WIN32_EMBED_FOCUS_IN,
					 GTK_WIN32_EMBED_FOCUS_CURRENT);
  else
    _gtk_win32_embed_send (socket->plug_window,
			   GTK_WIN32_EMBED_FOCUS_OUT,
			   0, 0);
}

void
_gtk_socket_windowing_update_active (GtkSocket *socket,
				     gboolean   active)
{
  _gtk_win32_embed_send (socket->plug_window,
			 (active ? GTK_WIN32_EMBED_WINDOW_ACTIVATE : GTK_WIN32_EMBED_WINDOW_DEACTIVATE),
			 0, 0);
}

void
_gtk_socket_windowing_update_modality (GtkSocket *socket,
				       gboolean   modality)
{
  _gtk_win32_embed_send (socket->plug_window,
			 (modality ? GTK_WIN32_EMBED_MODALITY_ON : GTK_WIN32_EMBED_MODALITY_OFF),
			 0, 0);
}

void
_gtk_socket_windowing_focus (GtkSocket       *socket,
			     GtkDirectionType direction)
{
  int detail = -1;

  switch (direction)
    {
    case GTK_DIR_UP:
    case GTK_DIR_LEFT:
    case GTK_DIR_TAB_BACKWARD:
      detail = GTK_WIN32_EMBED_FOCUS_LAST;
      break;
    case GTK_DIR_DOWN:
    case GTK_DIR_RIGHT:
    case GTK_DIR_TAB_FORWARD:
      detail = GTK_WIN32_EMBED_FOCUS_FIRST;
      break;
    }
  
  _gtk_win32_embed_send_focus_message (socket->plug_window,
				       GTK_WIN32_EMBED_FOCUS_IN,
				       detail);
}

void
_gtk_socket_windowing_send_configure_event (GtkSocket *socket)
{
  /* XXX Nothing needed? */
}

void
_gtk_socket_windowing_select_plug_window_input (GtkSocket *socket)
{
  /* XXX Nothing needed? */
}

void
_gtk_socket_windowing_embed_get_info (GtkSocket *socket)
{
  socket->is_mapped = TRUE;	/* XXX ? */
}

void
_gtk_socket_windowing_embed_notify (GtkSocket *socket)
{
  /* XXX Nothing needed? */
}

gboolean
_gtk_socket_windowing_embed_get_focus_wrapped (void)
{
  return _gtk_win32_embed_get_focus_wrapped ();
}

void
_gtk_socket_windowing_embed_set_focus_wrapped (void)
{
  _gtk_win32_embed_set_focus_wrapped ();
}

GdkFilterReturn
_gtk_socket_windowing_filter_func (GdkXEvent *gdk_xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  GtkSocket *socket;
  GtkWidget *widget;
  MSG *msg;
  GdkFilterReturn return_val;

  socket = GTK_SOCKET (data);

  return_val = GDK_FILTER_CONTINUE;

  if (socket->plug_widget)
    return return_val;

  widget = GTK_WIDGET (socket);
  msg = (MSG *) gdk_xevent;

  switch (msg->message)
    {
    default:
      if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_PARENT_NOTIFY))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: PARENT_NOTIFY received window=%p version=%d\n",
					    (gpointer) msg->wParam, (int) msg->lParam));
	  /* If we some day different protocols deployed need to add
	   * some more elaborate version handshake
	   */
	  if (msg->lParam != GTK_WIN32_EMBED_PROTOCOL_VERSION)
	    g_warning ("GTK Win32 embedding protocol version mismatch, "
		       "client uses version %d, we understand version %d",
		       (int) msg->lParam, GTK_WIN32_EMBED_PROTOCOL_VERSION);
	  if (!socket->plug_window)
	    {
	      _gtk_socket_add_window (socket, (GdkNativeWindow) msg->wParam, FALSE);
	      
	      if (socket->plug_window)
		GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: window created"));
	      
	      return_val = GDK_FILTER_REMOVE;
	    }
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_EVENT_PLUG_MAPPED))
	{
	  gboolean was_mapped = socket->is_mapped;
	  gboolean is_mapped = msg->wParam != 0;

	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: PLUG_MAPPED received is_mapped:%d\n", is_mapped));
	  if (was_mapped != is_mapped)
	    {
	      if (is_mapped)
		_gtk_socket_handle_map_request (socket);
	      else
		{
		  gdk_window_show (socket->plug_window);
		  _gtk_socket_unmap_notify (socket);
		}
	    }
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_PLUG_RESIZED))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: PLUG_RESIZED received\n"));
	  socket->have_size = FALSE;
	  gtk_widget_queue_resize (widget);
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_REQUEST_FOCUS))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: REQUEST_FOCUS received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_socket_claim_focus (socket, TRUE);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_NEXT))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: FOCUS_NEXT received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_socket_advance_toplevel_focus (socket, GTK_DIR_TAB_FORWARD);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_PREV))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: FOCUS_PREV received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_socket_advance_toplevel_focus (socket, GTK_DIR_TAB_BACKWARD);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_GRAB_KEY))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: GRAB_KEY received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_socket_add_grabbed_key (socket, msg->wParam, msg->lParam);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_UNGRAB_KEY))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkSocket: UNGRAB_KEY received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_socket_remove_grabbed_key (socket, msg->wParam, msg->lParam);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    }

  return return_val;
}

