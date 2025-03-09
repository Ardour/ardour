/*
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
 *
 * Authors: Cody Russell <crussell@canonical.com>
 *          Alexander Larsson <alexl@redhat.com>
 */

#ifndef __GTK_OFFSCREEN_WINDOW_H__
#define __GTK_OFFSCREEN_WINDOW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <ytk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_OFFSCREEN_WINDOW         (gtk_offscreen_window_get_type ())
#define GTK_OFFSCREEN_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_OFFSCREEN_WINDOW, GtkOffscreenWindow))
#define GTK_OFFSCREEN_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_OFFSCREEN_WINDOW, GtkOffscreenWindowClass))
#define GTK_IS_OFFSCREEN_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_OFFSCREEN_WINDOW))
#define GTK_IS_OFFSCREEN_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_OFFSCREEN_WINDOW))
#define GTK_OFFSCREEN_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_OFFSCREEN_WINDOW, GtkOffscreenWindowClass))

typedef struct _GtkOffscreenWindow      GtkOffscreenWindow;
typedef struct _GtkOffscreenWindowClass GtkOffscreenWindowClass;

struct _GtkOffscreenWindow
{
  GtkWindow parent_object;
};

struct _GtkOffscreenWindowClass
{
  GtkWindowClass parent_class;
};

GType      gtk_offscreen_window_get_type   (void) G_GNUC_CONST;

GtkWidget *gtk_offscreen_window_new        (void);
GdkPixmap *gtk_offscreen_window_get_pixmap (GtkOffscreenWindow *offscreen);
GdkPixbuf *gtk_offscreen_window_get_pixbuf (GtkOffscreenWindow *offscreen);

G_END_DECLS

#endif /* __GTK_OFFSCREEN_WINDOW_H__ */
