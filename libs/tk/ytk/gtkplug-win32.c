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

#include "gtkmarshalers.h"
#include "gtkplug.h"
#include "gtkplugprivate.h"

#include "gdk/gdkwin32.h"

#include "gtkwin32embed.h"
#include "gtkalias.h"

#if defined(_MSC_VER) && (WINVER < 0x0500)
#ifndef GA_PARENT
#define GA_PARENT 1
#endif
WINUSERAPI HWND WINAPI GetAncestor(HWND,UINT);
#endif

GdkNativeWindow
_gtk_plug_windowing_get_id (GtkPlug *plug)
{
  return (GdkNativeWindow) GDK_WINDOW_HWND (GTK_WIDGET (plug)->window);
}

void
_gtk_plug_windowing_realize_toplevel (GtkPlug *plug)
{
  if (plug->socket_window)
    {
      _gtk_win32_embed_send (plug->socket_window,
			     GTK_WIN32_EMBED_PARENT_NOTIFY,
			     (WPARAM) GDK_WINDOW_HWND (GTK_WIDGET (plug)->window),
			     GTK_WIN32_EMBED_PROTOCOL_VERSION);
      _gtk_win32_embed_send (plug->socket_window,
			     GTK_WIN32_EMBED_EVENT_PLUG_MAPPED, 0, 0);
    }
}

void
_gtk_plug_windowing_map_toplevel (GtkPlug *plug)
{
  if (plug->socket_window)
    _gtk_win32_embed_send (plug->socket_window,
			   GTK_WIN32_EMBED_EVENT_PLUG_MAPPED,
			   1, 0);
}

void
_gtk_plug_windowing_unmap_toplevel (GtkPlug *plug)
{
  if (plug->socket_window)
    _gtk_win32_embed_send (plug->socket_window,
			   GTK_WIN32_EMBED_EVENT_PLUG_MAPPED,
			   0, 0);
}

void
_gtk_plug_windowing_set_focus (GtkPlug *plug)
{
  if (plug->socket_window)
    _gtk_win32_embed_send (plug->socket_window,
			   GTK_WIN32_EMBED_REQUEST_FOCUS,
			   0, 0);
}

void
_gtk_plug_windowing_add_grabbed_key (GtkPlug        *plug,
				     guint           accelerator_key,
				     GdkModifierType accelerator_mods)
{
  if (plug->socket_window)
    _gtk_win32_embed_send (plug->socket_window,
			   GTK_WIN32_EMBED_GRAB_KEY,
			   accelerator_key, accelerator_mods);
}

void
_gtk_plug_windowing_remove_grabbed_key (GtkPlug        *plug,
					guint           accelerator_key,
					GdkModifierType accelerator_mods)
{
  if (plug->socket_window)
    _gtk_win32_embed_send (plug->socket_window,
			   GTK_WIN32_EMBED_UNGRAB_KEY,
			   accelerator_key, accelerator_mods);
}

void
_gtk_plug_windowing_focus_to_parent (GtkPlug         *plug,
				     GtkDirectionType direction)
{
  GtkWin32EmbedMessageType message = GTK_WIN32_EMBED_FOCUS_PREV;
  
  switch (direction)
    {
    case GTK_DIR_UP:
    case GTK_DIR_LEFT:
    case GTK_DIR_TAB_BACKWARD:
      message = GTK_WIN32_EMBED_FOCUS_PREV;
      break;
    case GTK_DIR_DOWN:
    case GTK_DIR_RIGHT:
    case GTK_DIR_TAB_FORWARD:
      message = GTK_WIN32_EMBED_FOCUS_NEXT;
      break;
    }
  
  _gtk_win32_embed_send_focus_message (plug->socket_window, message, 0);
}

GdkFilterReturn
_gtk_plug_windowing_filter_func (GdkXEvent *gdk_xevent,
				 GdkEvent  *event,
				 gpointer   data)
{
  GtkPlug *plug = GTK_PLUG (data);
  MSG *msg = (MSG *) gdk_xevent;
  GdkFilterReturn return_val = GDK_FILTER_CONTINUE;

  switch (msg->message)
    {
      /* What message should we look for to notice the reparenting?
       * Maybe WM_WINDOWPOSCHANGED will work? This is handled in the
       * X11 implementation by handling ReparentNotify. Handle this
       * only for cross-process embedding, otherwise we get odd
       * crashes in testsocket.
       */
    case WM_WINDOWPOSCHANGED:
      if (!plug->same_app)
	{
	  HWND parent = GetAncestor (msg->hwnd, GA_PARENT);
	  gboolean was_embedded = plug->socket_window != NULL;
	  GdkScreen *screen = gdk_drawable_get_screen (event->any.window);
	  GdkDisplay *display = gdk_screen_get_display (screen);

	  GTK_NOTE (PLUGSOCKET, g_printerr ("WM_WINDOWPOSCHANGED: hwnd=%p GA_PARENT=%p socket_window=%p\n", msg->hwnd, parent, plug->socket_window));
	  g_object_ref (plug);
	  if (was_embedded)
	    {
	      /* End of embedding protocol for previous socket */
	      if (parent != GDK_WINDOW_HWND (plug->socket_window))
		{
		  GtkWidget *widget = GTK_WIDGET (plug);

		  GTK_NOTE (PLUGSOCKET, g_printerr ("was_embedded, current parent != socket_window\n"));
		  gdk_window_set_user_data (plug->socket_window, NULL);
		  g_object_unref (plug->socket_window);
		  plug->socket_window = NULL;

		  /* Emit a delete window, as if the user attempted to
		   * close the toplevel. Only do this if we are being
		   * reparented to the desktop window. Moving from one
		   * embedder to another should be invisible to the app.
		   */
		  if (parent == GetDesktopWindow ())
		    {
		      GTK_NOTE (PLUGSOCKET, g_printerr ("current parent is root window\n"));
		      _gtk_plug_send_delete_event (widget);
		      return_val = GDK_FILTER_REMOVE;
		    }
		}
	      else
		{
		  GTK_NOTE (PLUGSOCKET, g_printerr ("still same parent\n"));
		  goto done;
		}
	    }

	  if (parent != GetDesktopWindow ())
	    {
	      /* Start of embedding protocol */

	      GTK_NOTE (PLUGSOCKET, g_printerr ("start of embedding\n"));
	      plug->socket_window = gdk_window_lookup_for_display (display, (GdkNativeWindow) parent);
	      if (plug->socket_window)
		{
		  gpointer user_data = NULL;

		  GTK_NOTE (PLUGSOCKET, g_printerr ("already had socket_window\n"));
		  gdk_window_get_user_data (plug->socket_window, &user_data);

		  if (user_data)
		    {
		      g_warning (G_STRLOC "Plug reparented unexpectedly into window in the same process");
		      plug->socket_window = NULL;
		      break;
		    }

		  g_object_ref (plug->socket_window);
		}
	      else
		{
		  plug->socket_window = gdk_window_foreign_new_for_display (display, (GdkNativeWindow) parent);
		  if (!plug->socket_window) /* Already gone */
		    break;
		}

	      _gtk_plug_add_all_grabbed_keys (plug);

	      if (!was_embedded)
		g_signal_emit_by_name (plug, "embedded");
	    }
	done:
	  g_object_unref (plug);
	}
      break;

    case WM_SIZE:
      if (!plug->same_app && plug->socket_window)
	{
	  _gtk_win32_embed_send (plug->socket_window,
				 GTK_WIN32_EMBED_PLUG_RESIZED,
				 0, 0);
	}
      break;

    default:
      if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_WINDOW_ACTIVATE))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkPlug: WINDOW_ACTIVATE received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_window_set_is_active (GTK_WINDOW (plug), TRUE);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_WINDOW_DEACTIVATE))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkPlug: WINDOW_DEACTIVATE received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_window_set_is_active (GTK_WINDOW (plug), FALSE);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_IN))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkPlug: FOCUS_IN received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_window_set_has_toplevel_focus (GTK_WINDOW (plug), TRUE);
	  switch (msg->wParam)
	    {
	    case GTK_WIN32_EMBED_FOCUS_CURRENT:
	      break;
	    case GTK_WIN32_EMBED_FOCUS_FIRST:
	      _gtk_plug_focus_first_last (plug, GTK_DIR_TAB_FORWARD);
	      break;
	    case GTK_WIN32_EMBED_FOCUS_LAST:
	      _gtk_plug_focus_first_last (plug, GTK_DIR_TAB_BACKWARD);
	      break;
	    }
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_OUT))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkPlug: FOCUS_OUT received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_window_set_has_toplevel_focus (GTK_WINDOW (plug), FALSE);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_MODALITY_ON))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkPlug: MODALITY_ON received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_plug_handle_modality_on (plug);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      else if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_MODALITY_OFF))
	{
	  GTK_NOTE (PLUGSOCKET, g_printerr ("GtkPlug: MODALITY_OFF received\n"));
	  _gtk_win32_embed_push_message (msg);
	  _gtk_plug_handle_modality_off (plug);
	  _gtk_win32_embed_pop_message ();
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    }

  return return_val;
}
