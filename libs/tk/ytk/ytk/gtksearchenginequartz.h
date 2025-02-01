/*
 * Copyright (C) 2007  Kristian Rietveld  <kris@gtk.org>
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
 */

#ifndef __GTK_SEARCH_ENGINE_QUARTZ_H__
#define __GTK_SEARCH_ENGINE_QUARTZ_H__

#include "gtksearchengine.h"

G_BEGIN_DECLS

#define GTK_TYPE_SEARCH_ENGINE_QUARTZ			(_gtk_search_engine_quartz_get_type ())
#define GTK_SEARCH_ENGINE_QUARTZ(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SEARCH_ENGINE_QUARTZ, GtkSearchEngineQuartz))
#define GTK_SEARCH_ENGINE_QUARTZ_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SEARCH_ENGINE_QUARTZ, GtkSearchEngineQuartzClass))
#define GTK_IS_SEARCH_ENGINE_QUARTZ(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SEARCH_ENGINE_QUARTZ))
#define GTK_IS_SEARCH_ENGINE_QUARTZ_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SEARCH_ENGINE_QUARTZ))
#define GTK_SEARCH_ENGINE_QUARTZ_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SEARCH_ENGINE_QUARTZ, GtkSearchEngineQuartzClass))

typedef struct _GtkSearchEngineQuartz GtkSearchEngineQuartz;
typedef struct _GtkSearchEngineQuartzClass GtkSearchEngineQuartzClass;
typedef struct _GtkSearchEngineQuartzPrivate GtkSearchEngineQuartzPrivate;

struct _GtkSearchEngineQuartz
{
  GtkSearchEngine parent;

  GtkSearchEngineQuartzPrivate *priv;
};

struct _GtkSearchEngineQuartzClass
{
  GtkSearchEngineClass parent_class;
};

GType            _gtk_search_engine_quartz_get_type (void);
GtkSearchEngine *_gtk_search_engine_quartz_new      (void);

G_END_DECLS

#endif /* __GTK_SEARCH_ENGINE_QUARTZ_H__ */
