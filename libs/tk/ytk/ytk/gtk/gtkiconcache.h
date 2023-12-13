/* gtkiconcache.h
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
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
#ifndef __GTK_ICON_CACHE_H__
#define __GTK_ICON_CACHE_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

typedef struct _GtkIconCache GtkIconCache;
typedef struct _GtkIconData GtkIconData;

struct _GtkIconData
{
  gboolean has_embedded_rect;
  gint x0, y0, x1, y1;
  
  GdkPoint *attach_points;
  gint n_attach_points;

  gchar *display_name;
};

GtkIconCache *_gtk_icon_cache_new            (const gchar  *data);
GtkIconCache *_gtk_icon_cache_new_for_path   (const gchar  *path);
gint          _gtk_icon_cache_get_directory_index  (GtkIconCache *cache,
					            const gchar  *directory);
gboolean      _gtk_icon_cache_has_icon       (GtkIconCache *cache,
					      const gchar  *icon_name);
gboolean      _gtk_icon_cache_has_icon_in_directory (GtkIconCache *cache,
					             const gchar  *icon_name,
					             const gchar  *directory);
void	      _gtk_icon_cache_add_icons      (GtkIconCache *cache,
					      const gchar  *directory,
					      GHashTable   *hash_table);

gint          _gtk_icon_cache_get_icon_flags (GtkIconCache *cache,
					      const gchar  *icon_name,
					      gint          directory_index);
GdkPixbuf    *_gtk_icon_cache_get_icon       (GtkIconCache *cache,
					      const gchar  *icon_name,
					      gint          directory_index);
GtkIconData  *_gtk_icon_cache_get_icon_data  (GtkIconCache *cache,
 					      const gchar  *icon_name,
 					      gint          directory_index);

GtkIconCache *_gtk_icon_cache_ref            (GtkIconCache *cache);
void          _gtk_icon_cache_unref          (GtkIconCache *cache);


#endif /* __GTK_ICON_CACHE_H__ */
