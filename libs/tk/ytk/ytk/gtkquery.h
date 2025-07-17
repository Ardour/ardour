/*
 * Copyright (C) 2005 Novell, Inc.
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
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 * Based on nautilus-query.h
 */

#ifndef __GTK_QUERY_H__
#define __GTK_QUERY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GTK_TYPE_QUERY		(_gtk_query_get_type ())
#define GTK_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_QUERY, GtkQuery))
#define GTK_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_QUERY, GtkQueryClass))
#define GTK_IS_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_QUERY))
#define GTK_IS_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_QUERY))
#define GTK_QUERY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_QUERY, GtkQueryClass))

typedef struct _GtkQuery GtkQuery;
typedef struct _GtkQueryClass GtkQueryClass;
typedef struct _GtkQueryPrivate GtkQueryPrivate;

struct _GtkQuery 
{
  GObject parent;

  GtkQueryPrivate *priv;
};

struct _GtkQueryClass
{
  GObjectClass parent_class;
};

GType     _gtk_query_get_type       (void);
gboolean  _gtk_query_enabled        (void);

GtkQuery* _gtk_query_new            (void);

gchar*    _gtk_query_get_text       (GtkQuery    *query);
void      _gtk_query_set_text       (GtkQuery    *query, 
				     const gchar *text);

gchar*    _gtk_query_get_location   (GtkQuery    *query);
void      _gtk_query_set_location   (GtkQuery    *query, 
				     const gchar *uri);

GList*    _gtk_query_get_mime_types (GtkQuery    *query);
void      _gtk_query_set_mime_types (GtkQuery    *query, 
				     GList       *mime_types);
void      _gtk_query_add_mime_type  (GtkQuery    *query, 
				     const gchar *mime_type);

G_END_DECLS

#endif /* __GTK_QUERY_H__ */
