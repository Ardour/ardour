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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>
#include "gtkthemes.h"
#include "gtkrc.h"
#include "gtkintl.h"
#include "gtkalias.h"

typedef struct _GtkThemeEngineClass GtkThemeEngineClass;

struct _GtkThemeEngine
{
  GTypeModule parent_instance;
  
  GModule *library;

  void (*init) (GTypeModule *);
  void (*exit) (void);
  GtkRcStyle *(*create_rc_style) ();

  gchar *name;
};

struct _GtkThemeEngineClass
{
  GTypeModuleClass parent_class;
};

static GHashTable *engine_hash = NULL;

static gboolean
gtk_theme_engine_load (GTypeModule *module)
{
  GtkThemeEngine *engine = GTK_THEME_ENGINE (module);
  
  gchar *engine_path;
      
  engine_path = gtk_rc_find_module_in_path (engine->name);
  
  if (!engine_path)
    {
      g_warning (_("Unable to locate theme engine in module_path: \"%s\","),
		 engine->name);
      return FALSE;
    }
    
  /* load the lib */
  
  GTK_NOTE (MISC, g_message ("Loading Theme %s\n", engine_path));
       
  engine->library = g_module_open (engine_path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_free(engine_path);
  if (!engine->library)
    {
      g_warning ("%s", g_module_error());
      return FALSE;
    }
  
  /* extract symbols from the lib */
  if (!g_module_symbol (engine->library, "theme_init",
			(gpointer *)&engine->init) ||
      !g_module_symbol (engine->library, "theme_exit", 
			(gpointer *)&engine->exit) ||
      !g_module_symbol (engine->library, "theme_create_rc_style", 
			(gpointer *)&engine->create_rc_style))
    {
      g_warning ("%s", g_module_error());
      g_module_close (engine->library);
      
      return FALSE;
    }
	    
  /* call the theme's init (theme_init) function to let it */
  /* setup anything it needs to set up. */
  engine->init (module);

  return TRUE;
}

static void
gtk_theme_engine_unload (GTypeModule *module)
{
  GtkThemeEngine *engine = GTK_THEME_ENGINE (module);

  engine->exit();

  g_module_close (engine->library);
  engine->library = NULL;

  engine->init = NULL;
  engine->exit = NULL;
  engine->create_rc_style = NULL;
}

static void
gtk_theme_engine_class_init (GtkThemeEngineClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

  module_class->load = gtk_theme_engine_load;
  module_class->unload = gtk_theme_engine_unload;
}

GType
gtk_theme_engine_get_type (void)
{
  static GType theme_engine_type = 0;

  if (!theme_engine_type)
    {
      const GTypeInfo theme_engine_info = {
        sizeof (GtkThemeEngineClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gtk_theme_engine_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkThemeEngine),
        0,              /* n_preallocs */
        NULL,           /* instance_init */
      };

      theme_engine_type =
	g_type_register_static (G_TYPE_TYPE_MODULE, I_("GtkThemeEngine"),
				&theme_engine_info, 0);
    }
  
  return theme_engine_type;
}

GtkThemeEngine*
gtk_theme_engine_get (const gchar *name)
{
  GtkThemeEngine *result;
  
  if (!engine_hash)
    engine_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* get the library name for the theme
   */
  result = g_hash_table_lookup (engine_hash, name);

  if (!result)
    {
      result = g_object_new (GTK_TYPE_THEME_ENGINE, NULL);
      g_type_module_set_name (G_TYPE_MODULE (result), name);
      result->name = g_strdup (name);

      g_hash_table_insert (engine_hash, result->name, result);
    }

  if (!g_type_module_use (G_TYPE_MODULE (result)))
    return NULL;

  return result;
}

GtkRcStyle *
gtk_theme_engine_create_rc_style (GtkThemeEngine *engine)
{
  g_return_val_if_fail (engine != NULL, NULL);

  return engine->create_rc_style ();
}

#define __GTK_THEMES_C__
#include "gtkaliasdef.c"
