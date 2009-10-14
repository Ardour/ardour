/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* modified by andreas meyer <hexx3000@gmx.de> */

#ifndef __GTK_CUSTOM_HRULER_H__
#define __GTK_CUSTOM_HRULER_H__


#include <gdk/gdk.h>
#include "gtk-custom-ruler.h"


G_BEGIN_DECLS


#define GTK_CUSTOM_HRULER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gtk_custom_hruler_get_type (), GtkCustomHRuler)
#define GTK_CUSTOM_HRULER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gtk_custom_hruler_get_type (), GtkCustomHRulerClass)
#define GTK_IS_CUSTOM_HRULER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gtk_custom_hruler_get_type ())


typedef struct _GtkCustomHRuler       GtkCustomHRuler;
typedef struct _GtkCustomHRulerClass  GtkCustomHRulerClass;

struct _GtkCustomHRuler
{
  GtkCustomRuler ruler;
};

struct _GtkCustomHRulerClass
{
  GtkCustomRulerClass parent_class;
};


GType      gtk_custom_hruler_get_type (void);
GtkWidget* gtk_custom_hruler_new      (void);


G_END_DECLS


#endif /* __GTK_CUSTOM_HRULER_H__ */
