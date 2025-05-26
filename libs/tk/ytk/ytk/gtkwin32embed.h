/* GTK - The GIMP Toolkit
 * gtkwin32embed.h: Utilities for Win32 embedding
 * Copyright (C) 2005, Novell, Inc.
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

/* By Tor Lillqvist <tml@novell.com> 2005 */

#ifndef __GTK_WIN32_EMBED_H__
#define __GTK_WIN32_EMBED_H__

G_BEGIN_DECLS

#define GTK_WIN32_EMBED_PROTOCOL_VERSION 1

/*
 * When the plug and socket are in separate processes they use a
 * simple protocol, more or less based on XEMBED. The protocol uses
 * registered window messages. The name passed to
 * RegisterWindowMessage() is gtk-win32-embed:%d, with %d being the
 * numeric value of an GtkWin32EmbedMessageType enum. Each message
 * carries the message type enum value and two integers, the "wparam"
 * and "lparam", like all window messages.
 *
 * So far all the window messages are posted to the other
 * process. Maybe some later enhancement will add also messages that
 * are sent, i.e. where the sending process waits for the receiving
 * process's window procedure to handle the message.
 */

typedef enum {					/* send or post? */
  /* First those sent from the socket
   * to the plug
   */
  GTK_WIN32_EMBED_WINDOW_ACTIVATE,		/* post */
  GTK_WIN32_EMBED_WINDOW_DEACTIVATE,		/* post */
  GTK_WIN32_EMBED_FOCUS_IN,			/* post */
  GTK_WIN32_EMBED_FOCUS_OUT,			/* post */
  GTK_WIN32_EMBED_MODALITY_ON,			/* post */
  GTK_WIN32_EMBED_MODALITY_OFF,			/* post */

  /* Then the ones sent from the plug
   * to the socket.
   */
  GTK_WIN32_EMBED_PARENT_NOTIFY,		/* post */
  GTK_WIN32_EMBED_EVENT_PLUG_MAPPED,		/* post */
  GTK_WIN32_EMBED_PLUG_RESIZED,			/* post */
  GTK_WIN32_EMBED_REQUEST_FOCUS,		/* post */
  GTK_WIN32_EMBED_FOCUS_NEXT,			/* post */
  GTK_WIN32_EMBED_FOCUS_PREV,			/* post */
  GTK_WIN32_EMBED_GRAB_KEY,			/* post */
  GTK_WIN32_EMBED_UNGRAB_KEY,			/* post */
  GTK_WIN32_EMBED_LAST
} GtkWin32EmbedMessageType;

/* wParam values for GTK_WIN32_EMBED_FOCUS_IN: */
#define GTK_WIN32_EMBED_FOCUS_CURRENT 0
#define GTK_WIN32_EMBED_FOCUS_FIRST 1
#define GTK_WIN32_EMBED_FOCUS_LAST 2

/* Flags for lParam in GTK_WIN32_EMBED_FOCUS_IN, GTK_WIN32_EMBED_FOCUS_NEXT,
 * GTK_WIN32_EMBED_FOCUS_PREV
 */
#define GTK_WIN32_EMBED_FOCUS_WRAPAROUND         (1 << 0)

guint _gtk_win32_embed_message_type (GtkWin32EmbedMessageType type);
void _gtk_win32_embed_push_message (MSG *msg);
void _gtk_win32_embed_pop_message (void);
void _gtk_win32_embed_send (GdkWindow		    *recipient,
			    GtkWin32EmbedMessageType message,
			    WPARAM		     wparam,
			    LPARAM                   lparam);
void _gtk_win32_embed_send_focus_message (GdkWindow		  *recipient,
					  GtkWin32EmbedMessageType message,
					  WPARAM	           wparam);
void     _gtk_win32_embed_set_focus_wrapped  (void);
gboolean _gtk_win32_embed_get_focus_wrapped  (void);

G_END_DECLS

#endif /*  __GTK_WIN32_EMBED_H__ */
