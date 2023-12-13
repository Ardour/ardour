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
 * Based on nautilus-search-engine.c
 */

#include "config.h"
#include "gtksearchengine.h"
#include "gtksearchenginesimple.h"
#include "gtksearchenginequartz.h"

#include <gdkconfig.h> /* for GDK_WINDOWING_QUARTZ */

enum 
{
  HITS_ADDED,
  HITS_SUBTRACTED,
  FINISHED,
  ERROR,
  LAST_SIGNAL
}; 

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE (GtkSearchEngine, _gtk_search_engine, G_TYPE_OBJECT);

static void
finalize (GObject *object)
{
  G_OBJECT_CLASS (_gtk_search_engine_parent_class)->finalize (object);
}

static void
_gtk_search_engine_class_init (GtkSearchEngineClass *class)
{
  GObjectClass *gobject_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = finalize;
  
  signals[HITS_ADDED] =
    g_signal_new ("hits-added",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSearchEngineClass, hits_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1,
		  G_TYPE_POINTER);
  
  signals[HITS_SUBTRACTED] =
    g_signal_new ("hits-subtracted",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSearchEngineClass, hits_subtracted),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1,
		  G_TYPE_POINTER);
  
  signals[FINISHED] =
    g_signal_new ("finished",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSearchEngineClass, finished),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  signals[ERROR] =
    g_signal_new ("error",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSearchEngineClass, error),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);  
}

static void
_gtk_search_engine_init (GtkSearchEngine *engine)
{
}

GtkSearchEngine *
_gtk_search_engine_new (void)
{
  GtkSearchEngine *engine = NULL;
	
#ifdef HAVE_TRACKER
  engine = _gtk_search_engine_tracker_new ();
  if (engine)
    return engine;
#endif
  
#ifdef HAVE_BEAGLE
  engine = _gtk_search_engine_beagle_new ();
  if (engine)
    return engine;
#endif

#ifdef GDK_WINDOWING_QUARTZ
  engine = _gtk_search_engine_quartz_new ();
  if (engine)
    return engine;
#endif

  if (g_thread_supported ())
    engine = _gtk_search_engine_simple_new ();
  
  return engine;
}

void
_gtk_search_engine_set_query (GtkSearchEngine *engine, 
			      GtkQuery        *query)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  g_return_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->set_query != NULL);
  
  GTK_SEARCH_ENGINE_GET_CLASS (engine)->set_query (engine, query);
}

void
_gtk_search_engine_start (GtkSearchEngine *engine)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  g_return_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->start != NULL);
  
  GTK_SEARCH_ENGINE_GET_CLASS (engine)->start (engine);
}

void
_gtk_search_engine_stop (GtkSearchEngine *engine)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  g_return_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->stop != NULL);
  
  GTK_SEARCH_ENGINE_GET_CLASS (engine)->stop (engine);
}

gboolean
_gtk_search_engine_is_indexed (GtkSearchEngine *engine)
{
  g_return_val_if_fail (GTK_IS_SEARCH_ENGINE (engine), FALSE);
  g_return_val_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->is_indexed != NULL, FALSE);
  
  return GTK_SEARCH_ENGINE_GET_CLASS (engine)->is_indexed (engine);
}

void	       
_gtk_search_engine_hits_added (GtkSearchEngine *engine, 
			       GList           *hits)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  
  g_signal_emit (engine, signals[HITS_ADDED], 0, hits);
}


void	       
_gtk_search_engine_hits_subtracted (GtkSearchEngine *engine, 
				    GList           *hits)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  
  g_signal_emit (engine, signals[HITS_SUBTRACTED], 0, hits);
}


void	       
_gtk_search_engine_finished (GtkSearchEngine *engine)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  
  g_signal_emit (engine, signals[FINISHED], 0);
}

void
_gtk_search_engine_error (GtkSearchEngine *engine, 
			  const gchar     *error_message)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  
  g_signal_emit (engine, signals[ERROR], 0, error_message);
}
