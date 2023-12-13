/*
 * Copyright (C) 2005 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 * Based on nautilus-search-engine-simple.h
 */

#ifndef __GTK_SEARCH_ENGINE_SIMPLE_H__
#define __GTK_SEARCH_ENGINE_SIMPLE_H__

#include "gtksearchengine.h"

G_BEGIN_DECLS

#define GTK_TYPE_SEARCH_ENGINE_SIMPLE		(_gtk_search_engine_simple_get_type ())
#define GTK_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SEARCH_ENGINE_SIMPLE, GtkSearchEngineSimple))
#define GTK_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SEARCH_ENGINE_SIMPLE, GtkSearchEngineSimpleClass))
#define GTK_IS_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SEARCH_ENGINE_SIMPLE))
#define GTK_IS_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SEARCH_ENGINE_SIMPLE))
#define GTK_SEARCH_ENGINE_SIMPLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SEARCH_ENGINE_SIMPLE, GtkSearchEngineSimpleClass))

typedef struct _GtkSearchEngineSimple GtkSearchEngineSimple;
typedef struct _GtkSearchEngineSimpleClass GtkSearchEngineSimpleClass;
typedef struct _GtkSearchEngineSimplePrivate GtkSearchEngineSimplePrivate;

struct _GtkSearchEngineSimple 
{
  GtkSearchEngine parent;

  GtkSearchEngineSimplePrivate *priv;
};

struct _GtkSearchEngineSimpleClass
{
  GtkSearchEngineClass parent_class;
};

GType            _gtk_search_engine_simple_get_type (void);

GtkSearchEngine* _gtk_search_engine_simple_new      (void);

G_END_DECLS

#endif /* __GTK_SEARCH_ENGINE_SIMPLE_H__ */
