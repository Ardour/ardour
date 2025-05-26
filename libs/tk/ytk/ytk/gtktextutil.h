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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_TEXT_UTIL_H__
#define __GTK_TEXT_UTIL_H__

G_BEGIN_DECLS

/* This is a private uninstalled header shared between
 * GtkTextView and GtkEntry
 */

typedef void (* GtkTextUtilCharChosenFunc) (const char *text,
                                            gpointer    data);

void _gtk_text_util_append_special_char_menuitems (GtkMenuShell              *menushell,
                                                   GtkTextUtilCharChosenFunc  func,
                                                   gpointer                   data);

GdkPixmap* _gtk_text_util_create_drag_icon      (GtkWidget     *widget,
                                                 gchar         *text,
                                                 gsize          len);
GdkPixmap* _gtk_text_util_create_rich_drag_icon (GtkWidget     *widget,
                                                 GtkTextBuffer *buffer,
                                                 GtkTextIter   *start,
                                                 GtkTextIter   *end);

gboolean _gtk_text_util_get_block_cursor_location (PangoLayout    *layout,
						   gint            index_,
						   PangoRectangle *rectangle,
						   gboolean       *at_line_end);

G_END_DECLS

#endif /* __GTK_TEXT_UTIL_H__ */
