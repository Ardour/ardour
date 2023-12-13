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
 * Based on nautilus-query.c
 */

#include "config.h"
#include <string.h>

#include "gtkquery.h"

struct _GtkQueryPrivate 
{
  gchar *text;
  gchar *location_uri;
  GList *mime_types;
};

G_DEFINE_TYPE (GtkQuery, _gtk_query, G_TYPE_OBJECT);

static void
finalize (GObject *object)
{
  GtkQuery *query;
  
  query = GTK_QUERY (object);
  
  g_free (query->priv->text);

  G_OBJECT_CLASS (_gtk_query_parent_class)->finalize (object);
}

static void
_gtk_query_class_init (GtkQueryClass *class)
{
  GObjectClass *gobject_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = finalize;

  g_type_class_add_private (gobject_class, sizeof (GtkQueryPrivate));  
}

static void
_gtk_query_init (GtkQuery *query)
{
  query->priv = G_TYPE_INSTANCE_GET_PRIVATE (query, GTK_TYPE_QUERY, GtkQueryPrivate);
}

GtkQuery *
_gtk_query_new (void)
{
  return g_object_new (GTK_TYPE_QUERY,  NULL);
}


gchar *
_gtk_query_get_text (GtkQuery *query)
{
  return g_strdup (query->priv->text);
}

void 
_gtk_query_set_text (GtkQuery    *query, 
		    const gchar *text)
{
  g_free (query->priv->text);
  query->priv->text = g_strdup (text);
}

gchar *
_gtk_query_get_location (GtkQuery *query)
{
  return g_strdup (query->priv->location_uri);
}
	
void
_gtk_query_set_location (GtkQuery    *query, 
			const gchar *uri)
{
  g_free (query->priv->location_uri);
  query->priv->location_uri = g_strdup (uri);
}

GList *
_gtk_query_get_mime_types (GtkQuery *query)
{
  GList *list, *l;
  gchar *mime_type;

  list = NULL;
  for (l = query->priv->mime_types; l; l = l->next)
    {
      mime_type = (gchar*)l->data;
      list = g_list_prepend (list, g_strdup (mime_type));
    }

  return list;
}

void
_gtk_query_set_mime_types (GtkQuery *query, 
			   GList    *mime_types)
{
  GList *l;
  gchar *mime_type;

  g_list_foreach (query->priv->mime_types, (GFunc)g_free, NULL);
  g_list_free (query->priv->mime_types);
  query->priv->mime_types = NULL;

  for (l = mime_types; l; l = l->next)
    {
      mime_type = (gchar*)l->data;
      query->priv->mime_types = g_list_prepend (query->priv->mime_types, g_strdup (mime_type));
    }
}

void
_gtk_query_add_mime_type (GtkQuery    *query, 
			  const gchar *mime_type)
{
  query->priv->mime_types = g_list_prepend (query->priv->mime_types,
					    g_strdup (mime_type));
}

