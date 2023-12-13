/* GTK - The GIMP Toolkit
 * gtkxembed.c: Utilities for the XEMBED protocol
 * Copyright (C) 2001, 2003, Red Hat, Inc.
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
#include <string.h>
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkxembed.h"
#include "gtkalias.h"

typedef struct _GtkXEmbedMessage GtkXEmbedMessage;

struct _GtkXEmbedMessage
{
  glong      message;
  glong      detail;
  glong      data1;
  glong      data2;
  guint32    time;
};

static GSList *current_messages;


/**
 * _gtk_xembed_push_message:
 * @xevent: a XEvent
 * 
 * Adds a client message to the stack of current XEMBED events.
 **/
void
_gtk_xembed_push_message (XEvent *xevent)
{
  GtkXEmbedMessage *message = g_slice_new (GtkXEmbedMessage);
  
  message->time = xevent->xclient.data.l[0];
  message->message = xevent->xclient.data.l[1];
  message->detail = xevent->xclient.data.l[2];
  message->data1 = xevent->xclient.data.l[3];
  message->data2 = xevent->xclient.data.l[4];

  current_messages = g_slist_prepend (current_messages, message);
}

/**
 * _gtk_xembed_pop_message:
 * 
 * Removes an event added with _gtk_xembed_push_message()
 **/
void
_gtk_xembed_pop_message (void)
{
  GtkXEmbedMessage *message = current_messages->data;
  current_messages = g_slist_delete_link (current_messages, current_messages);
  g_slice_free (GtkXEmbedMessage, message);
}

/**
 * _gtk_xembed_set_focus_wrapped:
 * 
 * Sets a flag indicating that the current focus sequence wrapped
 * around to the beginning of the ultimate toplevel.
 **/
void
_gtk_xembed_set_focus_wrapped (void)
{
  GtkXEmbedMessage *message;
  
  g_return_if_fail (current_messages != NULL);
  message = current_messages->data;
  g_return_if_fail (message->message == XEMBED_FOCUS_PREV || message->message == XEMBED_FOCUS_NEXT);
  
  message->data1 |= XEMBED_FOCUS_WRAPAROUND;
}

/**
 * _gtk_xembed_get_focus_wrapped:
 * 
 * Gets whether the current focus sequence has wrapped around
 * to the beginning of the ultimate toplevel.
 * 
 * Return value: %TRUE if the focus sequence has wrapped around.
 **/
gboolean
_gtk_xembed_get_focus_wrapped (void)
{
  GtkXEmbedMessage *message;
  
  g_return_val_if_fail (current_messages != NULL, FALSE);
  message = current_messages->data;

  return (message->data1 & XEMBED_FOCUS_WRAPAROUND) != 0;
}

static guint32
gtk_xembed_get_time (void)
{
  if (current_messages)
    {
      GtkXEmbedMessage *message = current_messages->data;
      return message->time;
    }
  else
    return gtk_get_current_event_time ();
}

/**
 * _gtk_xembed_send_message:
 * @recipient: (allow-none): window to which to send the window, or %NULL
 *             in which case nothing will be sent
 * @message:   type of message
 * @detail:    detail field of message
 * @data1:     data1 field of message
 * @data2:     data2 field of message
 * 
 * Sends a generic XEMBED message to a particular window.
 **/
void
_gtk_xembed_send_message (GdkWindow        *recipient,
			  XEmbedMessageType message,
			  glong             detail,
			  glong             data1,
			  glong             data2)
{
  GdkDisplay *display;
  XClientMessageEvent xclient;

  if (!recipient)
    return;
	  
  g_return_if_fail (GDK_IS_WINDOW (recipient));

  display = gdk_window_get_display (recipient);
  GTK_NOTE (PLUGSOCKET,
	    g_message ("Sending %s", _gtk_xembed_message_name (message)));

  memset (&xclient, 0, sizeof (xclient));
  xclient.window = GDK_WINDOW_XWINDOW (recipient);
  xclient.type = ClientMessage;
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED");
  xclient.format = 32;
  xclient.data.l[0] = gtk_xembed_get_time ();
  xclient.data.l[1] = message;
  xclient.data.l[2] = detail;
  xclient.data.l[3] = data1;
  xclient.data.l[4] = data2;

  gdk_error_trap_push ();
  XSendEvent (GDK_WINDOW_XDISPLAY(recipient),
	      GDK_WINDOW_XWINDOW (recipient),
	      False, NoEventMask, (XEvent *)&xclient);
  gdk_display_sync (display);
  gdk_error_trap_pop ();
}

/**
 * _gtk_xembed_send_focus_message:
 * @recipient: (allow-none): window to which to send the window, or %NULL
 *             in which case nothing will be sent
 * @message:   type of message
 * @detail:    detail field of message
 * 
 * Sends a XEMBED message for moving the focus along the focus
 * chain to a window. The flags field that these messages share
 * will be correctly filled in.
 **/
void
_gtk_xembed_send_focus_message (GdkWindow        *recipient,
				XEmbedMessageType message,
				glong             detail)
{
  gulong flags = 0;

  if (!recipient)
    return;
  
  g_return_if_fail (GDK_IS_WINDOW (recipient));
  g_return_if_fail (message == XEMBED_FOCUS_IN ||
		    message == XEMBED_FOCUS_NEXT ||
		    message == XEMBED_FOCUS_PREV);
		    
  if (current_messages)
    {
      GtkXEmbedMessage *message = current_messages->data;
      switch (message->message)
	{
	case XEMBED_FOCUS_IN:
	case XEMBED_FOCUS_NEXT:
	case XEMBED_FOCUS_PREV:
	  flags = message->data1 & XEMBED_FOCUS_WRAPAROUND;
	  break;
	default:
	  break;
	}
    }

  _gtk_xembed_send_message (recipient, message, detail, flags, 0);
}

const char *
_gtk_xembed_message_name (XEmbedMessageType message)
{
  static char unk[24];
  
  switch (message)
    {
#define CASE(x) case XEMBED_##x: return "XEMBED_"#x
      CASE (EMBEDDED_NOTIFY);
      CASE (WINDOW_ACTIVATE);
      CASE (WINDOW_DEACTIVATE);
      CASE (REQUEST_FOCUS);
      CASE (FOCUS_IN);
      CASE (FOCUS_OUT);
      CASE (FOCUS_NEXT);
      CASE (FOCUS_PREV);
      CASE (GRAB_KEY);
      CASE (UNGRAB_KEY);
      CASE (MODALITY_ON);
      CASE (MODALITY_OFF);
      CASE (GTK_GRAB_KEY);
      CASE (GTK_UNGRAB_KEY);
#undef CASE
    default:
      snprintf (unk, 24, "UNKNOWN(%d)", message);
      return unk;
    }
}
