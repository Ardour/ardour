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

/* By Owen Taylor <otaylor@gtk.org>              98/4/4 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <string.h>

#include "gdk/gdkkeysyms.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkwindow.h"
#include "gtkplug.h"
#include "gtkprivate.h"
#include "gtksocket.h"
#include "gtksocketprivate.h"
#include "gtkdnd.h"

#include "gdk/gdkx.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include "gtkxembed.h"
#include "gtkalias.h"

static gboolean xembed_get_info     (GdkWindow     *gdk_window,
				     unsigned long *version,
				     unsigned long *flags);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20

GdkNativeWindow
_gtk_socket_windowing_get_id (GtkSocket *socket)
{
  return GDK_WINDOW_XWINDOW (GTK_WIDGET (socket)->window);
}

void
_gtk_socket_windowing_realize_window (GtkSocket *socket)
{
  GdkWindow *window = GTK_WIDGET (socket)->window;
  XWindowAttributes xattrs;

  XGetWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			GDK_WINDOW_XWINDOW (window),
			&xattrs);

  /* Sooooo, it turns out that mozilla, as per the gtk2xt code selects
     for input on the socket with a mask of 0x0fffff (for god knows why)
     which includes ButtonPressMask causing a BadAccess if someone else
     also selects for this. As per the client-side windows merge we always
     normally selects for button press so we can emulate it on client
     side children that selects for button press. However, we don't need
     this for GtkSocket, so we unselect it here, fixing the crashes in
     firefox. */
  XSelectInput (GDK_WINDOW_XDISPLAY (window),
		GDK_WINDOW_XWINDOW (window), 
		(xattrs.your_event_mask & ~ButtonPressMask) |
		SubstructureNotifyMask | SubstructureRedirectMask);
}

void
_gtk_socket_windowing_end_embedding_toplevel (GtkSocket *socket)
{
  gtk_window_remove_embedded_xid (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (socket))),
				  GDK_WINDOW_XWINDOW (socket->plug_window));
}

void
_gtk_socket_windowing_size_request (GtkSocket *socket)
{
  XSizeHints hints;
  long supplied;
	  
  gdk_error_trap_push ();

  socket->request_width = 1;
  socket->request_height = 1;
	  
  if (XGetWMNormalHints (GDK_WINDOW_XDISPLAY (socket->plug_window),
			 GDK_WINDOW_XWINDOW (socket->plug_window),
			 &hints, &supplied))
    {
      if (hints.flags & PMinSize)
	{
	  socket->request_width = MAX (hints.min_width, 1);
	  socket->request_height = MAX (hints.min_height, 1);
	}
      else if (hints.flags & PBaseSize)
	{
	  socket->request_width = MAX (hints.base_width, 1);
	  socket->request_height = MAX (hints.base_height, 1);
	}
    }
  socket->have_size = TRUE;
  
  gdk_error_trap_pop ();
}

void
_gtk_socket_windowing_send_key_event (GtkSocket *socket,
				      GdkEvent  *gdk_event,
				      gboolean   mask_key_presses)
{
  XKeyEvent xkey;
  GdkScreen *screen = gdk_window_get_screen (socket->plug_window);

  memset (&xkey, 0, sizeof (xkey));
  xkey.type = (gdk_event->type == GDK_KEY_PRESS) ? KeyPress : KeyRelease;
  xkey.window = GDK_WINDOW_XWINDOW (socket->plug_window);
  xkey.root = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));
  xkey.subwindow = None;
  xkey.time = gdk_event->key.time;
  xkey.x = 0;
  xkey.y = 0;
  xkey.x_root = 0;
  xkey.y_root = 0;
  xkey.state = gdk_event->key.state;
  xkey.keycode = gdk_event->key.hardware_keycode;
  xkey.same_screen = True;/* FIXME ? */

  gdk_error_trap_push ();
  XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
	      GDK_WINDOW_XWINDOW (socket->plug_window),
	      False,
	      (mask_key_presses ? KeyPressMask : NoEventMask),
	      (XEvent *)&xkey);
  gdk_display_sync (gdk_screen_get_display (screen));
  gdk_error_trap_pop ();
}

void
_gtk_socket_windowing_focus_change (GtkSocket *socket,
				    gboolean   focus_in)
{
  if (focus_in)
    _gtk_xembed_send_focus_message (socket->plug_window,
				    XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT);
  else
    _gtk_xembed_send_message (socket->plug_window,
			      XEMBED_FOCUS_OUT, 0, 0, 0);
}

void
_gtk_socket_windowing_update_active (GtkSocket *socket,
				     gboolean   active)
{
  _gtk_xembed_send_message (socket->plug_window,
			    active ? XEMBED_WINDOW_ACTIVATE : XEMBED_WINDOW_DEACTIVATE,
			    0, 0, 0);
}

void
_gtk_socket_windowing_update_modality (GtkSocket *socket,
				       gboolean   modality)
{
  _gtk_xembed_send_message (socket->plug_window,
			    modality ? XEMBED_MODALITY_ON : XEMBED_MODALITY_OFF,
			    0, 0, 0);
}

void
_gtk_socket_windowing_focus (GtkSocket       *socket,
			     GtkDirectionType direction)
{
  gint detail = -1;

  switch (direction)
    {
    case GTK_DIR_UP:
    case GTK_DIR_LEFT:
    case GTK_DIR_TAB_BACKWARD:
      detail = XEMBED_FOCUS_LAST;
      break;
    case GTK_DIR_DOWN:
    case GTK_DIR_RIGHT:
    case GTK_DIR_TAB_FORWARD:
      detail = XEMBED_FOCUS_FIRST;
      break;
    }
  
  _gtk_xembed_send_focus_message (socket->plug_window, XEMBED_FOCUS_IN, detail);
}

void
_gtk_socket_windowing_send_configure_event (GtkSocket *socket)
{
  XConfigureEvent xconfigure;
  gint x, y;

  g_return_if_fail (socket->plug_window != NULL);

  memset (&xconfigure, 0, sizeof (xconfigure));
  xconfigure.type = ConfigureNotify;

  xconfigure.event = GDK_WINDOW_XWINDOW (socket->plug_window);
  xconfigure.window = GDK_WINDOW_XWINDOW (socket->plug_window);

  /* The ICCCM says that synthetic events should have root relative
   * coordinates. We still aren't really ICCCM compliant, since
   * we don't send events when the real toplevel is moved.
   */
  gdk_error_trap_push ();
  gdk_window_get_origin (socket->plug_window, &x, &y);
  gdk_error_trap_pop ();
			 
  xconfigure.x = x;
  xconfigure.y = y;
  xconfigure.width = GTK_WIDGET(socket)->allocation.width;
  xconfigure.height = GTK_WIDGET(socket)->allocation.height;

  xconfigure.border_width = 0;
  xconfigure.above = None;
  xconfigure.override_redirect = False;

  gdk_error_trap_push ();
  XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
	      GDK_WINDOW_XWINDOW (socket->plug_window),
	      False, NoEventMask, (XEvent *)&xconfigure);
  gdk_display_sync (gtk_widget_get_display (GTK_WIDGET (socket)));
  gdk_error_trap_pop ();
}

void
_gtk_socket_windowing_select_plug_window_input (GtkSocket *socket)
{
  XSelectInput (GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (socket))),
		GDK_WINDOW_XWINDOW (socket->plug_window),
		StructureNotifyMask | PropertyChangeMask);
}

void
_gtk_socket_windowing_embed_get_info (GtkSocket *socket)
{
  unsigned long version;
  unsigned long flags;

  socket->xembed_version = -1;
  if (xembed_get_info (socket->plug_window, &version, &flags))
    {
      socket->xembed_version = MIN (GTK_XEMBED_PROTOCOL_VERSION, version);
      socket->is_mapped = (flags & XEMBED_MAPPED) != 0;
    }
  else
    {
      /* FIXME, we should probably actually check the state before we started */
      socket->is_mapped = TRUE;
    }
}

void
_gtk_socket_windowing_embed_notify (GtkSocket *socket)
{
#ifdef HAVE_XFIXES
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (socket));

  XFixesChangeSaveSet (GDK_DISPLAY_XDISPLAY (display),
		       GDK_WINDOW_XWINDOW (socket->plug_window),
		       SetModeInsert, SaveSetRoot, SaveSetUnmap);
#endif
  _gtk_xembed_send_message (socket->plug_window,
			    XEMBED_EMBEDDED_NOTIFY, 0,
			    GDK_WINDOW_XWINDOW (GTK_WIDGET (socket)->window),
			    socket->xembed_version);
}

static gboolean
xembed_get_info (GdkWindow     *window,
		 unsigned long *version,
		 unsigned long *flags)
{
  GdkDisplay *display = gdk_window_get_display (window);
  Atom xembed_info_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO");
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data;
  unsigned long *data_long;
  int status;
  
  gdk_error_trap_push();
  status = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
			       GDK_WINDOW_XWINDOW (window),
			       xembed_info_atom,
			       0, 2, False,
			       xembed_info_atom, &type, &format,
			       &nitems, &bytes_after, &data);
  gdk_error_trap_pop();

  if (status != Success)
    return FALSE;		/* Window vanished? */

  if (type == None)		/* No info property */
    return FALSE;

  if (type != xembed_info_atom)
    {
      g_warning ("_XEMBED_INFO property has wrong type\n");
      return FALSE;
    }
  
  if (nitems < 2)
    {
      g_warning ("_XEMBED_INFO too short\n");
      XFree (data);
      return FALSE;
    }
  
  data_long = (unsigned long *)data;
  if (version)
    *version = data_long[0];
  if (flags)
    *flags = data_long[1] & XEMBED_MAPPED;
  
  XFree (data);
  return TRUE;
}

gboolean
_gtk_socket_windowing_embed_get_focus_wrapped (void)
{
  return _gtk_xembed_get_focus_wrapped ();
}

void
_gtk_socket_windowing_embed_set_focus_wrapped (void)
{
  _gtk_xembed_set_focus_wrapped ();
}

static void
handle_xembed_message (GtkSocket        *socket,
		       XEmbedMessageType message,
		       glong             detail,
		       glong             data1,
		       glong             data2,
		       guint32           time)
{
  GTK_NOTE (PLUGSOCKET,
	    g_message ("GtkSocket: %s received", _gtk_xembed_message_name (message)));
  
  switch (message)
    {
    case XEMBED_EMBEDDED_NOTIFY:
    case XEMBED_WINDOW_ACTIVATE:
    case XEMBED_WINDOW_DEACTIVATE:
    case XEMBED_MODALITY_ON:
    case XEMBED_MODALITY_OFF:
    case XEMBED_FOCUS_IN:
    case XEMBED_FOCUS_OUT:
      g_warning ("GtkSocket: Invalid _XEMBED message %s received", _gtk_xembed_message_name (message));
      break;
      
    case XEMBED_REQUEST_FOCUS:
      _gtk_socket_claim_focus (socket, TRUE);
      break;

    case XEMBED_FOCUS_NEXT:
    case XEMBED_FOCUS_PREV:
      _gtk_socket_advance_toplevel_focus (socket,
					  (message == XEMBED_FOCUS_NEXT ?
					   GTK_DIR_TAB_FORWARD : GTK_DIR_TAB_BACKWARD));
      break;
      
    case XEMBED_GTK_GRAB_KEY:
      _gtk_socket_add_grabbed_key (socket, data1, data2);
      break; 
    case XEMBED_GTK_UNGRAB_KEY:
      _gtk_socket_remove_grabbed_key (socket, data1, data2);
      break;

    case XEMBED_GRAB_KEY:
    case XEMBED_UNGRAB_KEY:
      break;
      
    default:
      GTK_NOTE (PLUGSOCKET,
		g_message ("GtkSocket: Ignoring unknown _XEMBED message of type %d", message));
      break;
    }
}

GdkFilterReturn
_gtk_socket_windowing_filter_func (GdkXEvent *gdk_xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  GtkSocket *socket;
  GtkWidget *widget;
  GdkDisplay *display;
  XEvent *xevent;

  GdkFilterReturn return_val;
  
  socket = GTK_SOCKET (data);

  return_val = GDK_FILTER_CONTINUE;

  if (socket->plug_widget)
    return return_val;

  widget = GTK_WIDGET (socket);
  xevent = (XEvent *)gdk_xevent;
  display = gtk_widget_get_display (widget);

  switch (xevent->type)
    {
    case ClientMessage:
      if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED"))
	{
	  _gtk_xembed_push_message (xevent);
	  handle_xembed_message (socket,
				 xevent->xclient.data.l[1],
				 xevent->xclient.data.l[2],
				 xevent->xclient.data.l[3],
				 xevent->xclient.data.l[4],
				 xevent->xclient.data.l[0]);
	  _gtk_xembed_pop_message ();
	  
	  return_val = GDK_FILTER_REMOVE;
	}
      break;

    case CreateNotify:
      {
	XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

	if (!socket->plug_window)
	  {
	    _gtk_socket_add_window (socket, xcwe->window, FALSE);

	    if (socket->plug_window)
	      {
		GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - window created"));
	      }
	  }
	
	return_val = GDK_FILTER_REMOVE;
	
	break;
      }

    case ConfigureRequest:
      {
	XConfigureRequestEvent *xcre = &xevent->xconfigurerequest;
	
	if (!socket->plug_window)
	  _gtk_socket_add_window (socket, xcre->window, FALSE);
	
	if (socket->plug_window)
	  {
	    GtkSocketPrivate *private = _gtk_socket_get_private (socket);
	    
	    if (xcre->value_mask & (CWWidth | CWHeight))
	      {
		GTK_NOTE (PLUGSOCKET,
			  g_message ("GtkSocket - configure request: %d %d",
				     socket->request_width,
				     socket->request_height));

		private->resize_count++;
		gtk_widget_queue_resize (widget);
	      }
	    else if (xcre->value_mask & (CWX | CWY))
	      {
		_gtk_socket_windowing_send_configure_event (socket);
	      }
	    /* Ignore stacking requests. */
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
	break;
      }

    case DestroyNotify:
      {
	XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;

	/* Note that we get destroy notifies both from SubstructureNotify on
	 * our window and StructureNotify on socket->plug_window
	 */
	if (socket->plug_window && (xdwe->window == GDK_WINDOW_XWINDOW (socket->plug_window)))
	  {
	    gboolean result;
	    
	    GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - destroy notify"));
	    
	    gdk_window_destroy_notify (socket->plug_window);
	    _gtk_socket_end_embedding (socket);

	    g_object_ref (widget);
	    g_signal_emit_by_name (widget, "plug-removed", &result);
	    if (!result)
	      gtk_widget_destroy (widget);
	    g_object_unref (widget);
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
	break;
      }

    case FocusIn:
      if (xevent->xfocus.mode == EMBEDDED_APP_WANTS_FOCUS)
	{
	  _gtk_socket_claim_focus (socket, TRUE);
	}
      return_val = GDK_FILTER_REMOVE;
      break;
    case FocusOut:
      return_val = GDK_FILTER_REMOVE;
      break;
    case MapRequest:
      if (!socket->plug_window)
	{
	  _gtk_socket_add_window (socket, xevent->xmaprequest.window, FALSE);
	}
	
      if (socket->plug_window)
	{
	  GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - Map Request"));

	  _gtk_socket_handle_map_request (socket);
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    case PropertyNotify:
      if (socket->plug_window &&
	  xevent->xproperty.window == GDK_WINDOW_XWINDOW (socket->plug_window))
	{
	  GdkDragProtocol protocol;

	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_NORMAL_HINTS"))
	    {
	      GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - received PropertyNotify for plug's WM_NORMAL_HINTS"));
	      socket->have_size = FALSE;
	      gtk_widget_queue_resize (widget);
	      return_val = GDK_FILTER_REMOVE;
	    }
	  else if ((xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "XdndAware")) ||
	      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_RECEIVER_INFO")))
	    {
	      gdk_error_trap_push ();
	      if (gdk_drag_get_protocol_for_display (display,
						     xevent->xproperty.window,
						     &protocol))
		gtk_drag_dest_set_proxy (GTK_WIDGET (socket),
					 socket->plug_window,
					 protocol, TRUE);

	      gdk_display_sync (display);
	      gdk_error_trap_pop ();
	      return_val = GDK_FILTER_REMOVE;
	    }
	  else if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO"))
	    {
	      unsigned long flags;
	      
	      if (xembed_get_info (socket->plug_window, NULL, &flags))
		{
		  gboolean was_mapped = socket->is_mapped;
		  gboolean is_mapped = (flags & XEMBED_MAPPED) != 0;

		  if (was_mapped != is_mapped)
		    {
		      if (is_mapped)
			_gtk_socket_handle_map_request (socket);
		      else
			{
			  gdk_error_trap_push ();
			  gdk_window_show (socket->plug_window);
			  gdk_flush ();
			  gdk_error_trap_pop ();
			  
			  _gtk_socket_unmap_notify (socket);
			}
		    }
		}
	      return_val = GDK_FILTER_REMOVE;
	    }
	}
      break;
    case ReparentNotify:
      {
	XReparentEvent *xre = &xevent->xreparent;

	GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - ReparentNotify received"));
	if (!socket->plug_window && xre->parent == GDK_WINDOW_XWINDOW (widget->window))
	  {
	    _gtk_socket_add_window (socket, xre->window, FALSE);
	    
	    if (socket->plug_window)
	      {
		GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - window reparented"));
	      }
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
        else
          {
            if (socket->plug_window && xre->window == GDK_WINDOW_XWINDOW (socket->plug_window) && xre->parent != GDK_WINDOW_XWINDOW (widget->window))
              {
                gboolean result;

                _gtk_socket_end_embedding (socket);

                g_object_ref (widget);
                g_signal_emit_by_name (widget, "plug-removed", &result);
                if (!result)
                  gtk_widget_destroy (widget);
                g_object_unref (widget);

                return_val = GDK_FILTER_REMOVE;
              }
          }

	break;
      }
    case UnmapNotify:
      if (socket->plug_window &&
	  xevent->xunmap.window == GDK_WINDOW_XWINDOW (socket->plug_window))
	{
	  GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - Unmap notify"));

	  _gtk_socket_unmap_notify (socket);
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
      
    }
  
  return return_val;
}
