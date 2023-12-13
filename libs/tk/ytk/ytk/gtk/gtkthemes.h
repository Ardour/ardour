/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_THEMES_H__
#define __GTK_THEMES_H__


#include <gtk/gtkstyle.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_THEME_ENGINE             (gtk_theme_engine_get_type ())
#define GTK_THEME_ENGINE(theme_engine)    (G_TYPE_CHECK_INSTANCE_CAST ((theme_engine), GTK_TYPE_THEME_ENGINE, GtkThemeEngine))
#define GTK_IS_THEME_ENGINE(theme_engine) (G_TYPE_CHECK_INSTANCE_TYPE ((theme_engine), GTK_TYPE_THEME_ENGINE))

GType           gtk_theme_engine_get_type        (void) G_GNUC_CONST;
GtkThemeEngine *gtk_theme_engine_get             (const gchar     *name);
GtkRcStyle     *gtk_theme_engine_create_rc_style (GtkThemeEngine  *engine);

G_END_DECLS

#endif /* __GTK_THEMES_H__ */
