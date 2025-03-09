/* GTK - The GIMP Toolkit
 * gtkfilefilter.h: Filters for selecting a file subset
 * Copyright (C) 2003, Red Hat, Inc.
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

#ifndef __GTK_FILE_FILTER_H__
#define __GTK_FILE_FILTER_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_FILTER              (gtk_file_filter_get_type ())
#define GTK_FILE_FILTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_FILTER, GtkFileFilter))
#define GTK_IS_FILE_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_FILTER))

typedef struct _GtkFileFilter     GtkFileFilter;
typedef struct _GtkFileFilterInfo GtkFileFilterInfo;

typedef enum {
  GTK_FILE_FILTER_FILENAME     = 1 << 0,
  GTK_FILE_FILTER_URI          = 1 << 1,
  GTK_FILE_FILTER_DISPLAY_NAME = 1 << 2,
  GTK_FILE_FILTER_MIME_TYPE    = 1 << 3
} GtkFileFilterFlags;

typedef gboolean (*GtkFileFilterFunc) (const GtkFileFilterInfo *filter_info,
				       gpointer                 data);

struct _GtkFileFilterInfo
{
  GtkFileFilterFlags contains;

  const gchar *filename;
  const gchar *uri;
  const gchar *display_name;
  const gchar *mime_type;
};

GType gtk_file_filter_get_type (void) G_GNUC_CONST;

GtkFileFilter *       gtk_file_filter_new      (void);
void                  gtk_file_filter_set_name (GtkFileFilter *filter,
						const gchar   *name);
const gchar *         gtk_file_filter_get_name (GtkFileFilter *filter);

void gtk_file_filter_add_mime_type      (GtkFileFilter      *filter,
					 const gchar        *mime_type);
void gtk_file_filter_add_pattern        (GtkFileFilter      *filter,
					 const gchar        *pattern);
void gtk_file_filter_add_pixbuf_formats (GtkFileFilter      *filter);
void gtk_file_filter_add_custom         (GtkFileFilter      *filter,
					 GtkFileFilterFlags  needed,
					 GtkFileFilterFunc   func,
					 gpointer            data,
					 GDestroyNotify      notify);

GtkFileFilterFlags gtk_file_filter_get_needed (GtkFileFilter           *filter);
gboolean           gtk_file_filter_filter     (GtkFileFilter           *filter,
					       const GtkFileFilterInfo *filter_info);

G_END_DECLS

#endif /* __GTK_FILE_FILTER_H__ */
