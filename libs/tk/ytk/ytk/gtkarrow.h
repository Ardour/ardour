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

#ifndef __GTK_ARROW_H__
#define __GTK_ARROW_H__


#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <ytk/gtkmisc.h>


G_BEGIN_DECLS


#define GTK_TYPE_ARROW                  (gtk_arrow_get_type ())
#define GTK_ARROW(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ARROW, GtkArrow))
#define GTK_ARROW_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ARROW, GtkArrowClass))
#define GTK_IS_ARROW(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ARROW))
#define GTK_IS_ARROW_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ARROW))
#define GTK_ARROW_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ARROW, GtkArrowClass))


typedef struct _GtkArrow       GtkArrow;
typedef struct _GtkArrowClass  GtkArrowClass;

struct _GtkArrow
{
  GtkMisc misc;

  gint16 GSEAL (arrow_type);
  gint16 GSEAL (shadow_type);
};

struct _GtkArrowClass
{
  GtkMiscClass parent_class;
};


GType      gtk_arrow_get_type   (void) G_GNUC_CONST;
GtkWidget* gtk_arrow_new        (GtkArrowType   arrow_type,
				 GtkShadowType  shadow_type);
void       gtk_arrow_set        (GtkArrow      *arrow,
				 GtkArrowType   arrow_type,
				 GtkShadowType  shadow_type);


G_END_DECLS


#endif /* __GTK_ARROW_H__ */
