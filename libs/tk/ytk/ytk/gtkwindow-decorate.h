/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 Red Hat, Inc.
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
 * Authors: Alexander Larsson <alexl@redhat.com>
 */

#ifndef __GTK_WINDOW_DECORATE_H__
#define __GTK_WINDOW_DECORATE_H__

G_BEGIN_DECLS

void gtk_decorated_window_init                 (GtkWindow   *window);
void gtk_decorated_window_calculate_frame_size (GtkWindow   *window);
void gtk_decorated_window_set_title            (GtkWindow   *window,
						const gchar *title);
void gtk_decorated_window_move_resize_window   (GtkWindow   *window,
						gint         x,
						gint         y,
						gint         width,
						gint         height);

G_END_DECLS

#endif /* __GTK_WINDOW_DECORATE_H__ */
