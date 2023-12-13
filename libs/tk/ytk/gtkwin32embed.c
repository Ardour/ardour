/* GTK - The GIMP Toolkit
 * gtkwin32embed.c: Utilities for Win32 embedding
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

#include "config.h"

#include "gdk/gdkwin32.h"

#include "gtkwin32embed.h"

#include "gtkalias.h"

static guint message_type[GTK_WIN32_EMBED_LAST];

static GSList *current_messages;

guint
_gtk_win32_embed_message_type (GtkWin32EmbedMessageType type)
{
  if (type < 0 || type >= GTK_WIN32_EMBED_LAST)
    return 0;

  if (message_type[type] == 0)
    {
      char name[100];
      sprintf (name, "gtk-win32-embed:%d", type);
      message_type[type] = RegisterWindowMessage (name);
    }

  return message_type[type];
}

void
_gtk_win32_embed_push_message (MSG *msg)
{
  MSG *message = g_new (MSG, 1);

  *message = *msg;

  current_messages = g_slist_prepend (current_messages, message);
}

void
_gtk_win32_embed_pop_message (void)
{
  MSG *message = current_messages->data;

  current_messages = g_slist_delete_link (current_messages, current_messages);

  g_free (message);
}

void
_gtk_win32_embed_send (GdkWindow               *recipient,
		       GtkWin32EmbedMessageType message,
		       WPARAM		        wparam,
		       LPARAM			lparam)
{
  PostMessage (GDK_WINDOW_HWND (recipient),
	       _gtk_win32_embed_message_type (message),
	       wparam, lparam);
}

void
_gtk_win32_embed_send_focus_message (GdkWindow               *recipient,
				     GtkWin32EmbedMessageType message,
				     WPARAM		      wparam)
{
  int lparam = 0;

  if (!recipient)
    return;
  
  g_return_if_fail (GDK_IS_WINDOW (recipient));
  g_return_if_fail (message == GTK_WIN32_EMBED_FOCUS_IN ||
		    message == GTK_WIN32_EMBED_FOCUS_NEXT ||
		    message == GTK_WIN32_EMBED_FOCUS_PREV);
		    
  if (current_messages)
    {
      MSG *msg = current_messages->data;
      if (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_IN) ||
	  msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_NEXT) ||
	  msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_PREV))
	lparam = (msg->lParam & GTK_WIN32_EMBED_FOCUS_WRAPAROUND);
    }

  _gtk_win32_embed_send (recipient, message, wparam, lparam);
}

void
_gtk_win32_embed_set_focus_wrapped (void)
{
  MSG *msg;
  
  g_return_if_fail (current_messages != NULL);

  msg = current_messages->data;

  g_return_if_fail (msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_PREV) ||
		    msg->message == _gtk_win32_embed_message_type (GTK_WIN32_EMBED_FOCUS_NEXT));
  
  msg->lParam |= GTK_WIN32_EMBED_FOCUS_WRAPAROUND;
}

gboolean
_gtk_win32_embed_get_focus_wrapped (void)
{
  MSG *msg;
  
  g_return_val_if_fail (current_messages != NULL, FALSE);

  msg = current_messages->data;

  return (msg->lParam & GTK_WIN32_EMBED_FOCUS_WRAPAROUND) != 0;
}
