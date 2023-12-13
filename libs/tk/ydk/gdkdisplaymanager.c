/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkscreen.h"
#include "gdkdisplay.h"
#include "gdkdisplaymanager.h"

#include "gdkinternals.h"
#include "gdkmarshalers.h"

#include "gdkintl.h"

#include "gdkalias.h"

struct _GdkDisplayManager
{
  GObject parent_instance;
};

enum {
  PROP_0,

  PROP_DEFAULT_DISPLAY
};

enum {
  DISPLAY_OPENED,
  LAST_SIGNAL
};

static void gdk_display_manager_class_init   (GdkDisplayManagerClass *klass);
static void gdk_display_manager_set_property (GObject                *object,
					      guint                   prop_id,
					      const GValue           *value,
					      GParamSpec             *pspec);
static void gdk_display_manager_get_property (GObject                *object,
					      guint                   prop_id,
					      GValue                 *value,
					      GParamSpec             *pspec);

static guint signals[LAST_SIGNAL] = { 0 };

static GdkDisplay *default_display = NULL;

G_DEFINE_TYPE (GdkDisplayManager, gdk_display_manager, G_TYPE_OBJECT)

static void
gdk_display_manager_class_init (GdkDisplayManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_display_manager_set_property;
  object_class->get_property = gdk_display_manager_get_property;

  /**
   * GdkDisplayManager::display-opened:
   * @display_manager: the object on which the signal is emitted
   * @display: the opened display
   *
   * The ::display_opened signal is emitted when a display is opened.
   *
   * Since: 2.2
   */
  signals[DISPLAY_OPENED] =
    g_signal_new (g_intern_static_string ("display-opened"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayManagerClass, display_opened),
		  NULL, NULL,
		  _gdk_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1,
		  GDK_TYPE_DISPLAY);

  g_object_class_install_property (object_class,
				   PROP_DEFAULT_DISPLAY,
				   g_param_spec_object ("default-display",
 							P_("Default Display"),
 							P_("The default display for GDK"),
							GDK_TYPE_DISPLAY,
 							G_PARAM_READWRITE|G_PARAM_STATIC_NAME|
							G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
gdk_display_manager_init (GdkDisplayManager *manager)
{
}

static void
gdk_display_manager_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      gdk_display_manager_set_default_display (GDK_DISPLAY_MANAGER (object),
					       g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_display_manager_get_property (GObject      *object,
				  guint         prop_id,
				  GValue       *value,
				  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      g_value_set_object (value, default_display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_display_manager_get:
 *
 * Gets the singleton #GdkDisplayManager object.
 *
 * Returns: (transfer none): The global #GdkDisplayManager singleton; gdk_parse_pargs(),
 * gdk_init(), or gdk_init_check() must have been called first.
 *
 * Since: 2.2
 **/
GdkDisplayManager*
gdk_display_manager_get (void)
{
  static GdkDisplayManager *display_manager = NULL;

  if (!display_manager)
    display_manager = g_object_new (GDK_TYPE_DISPLAY_MANAGER, NULL);

  return display_manager;
}

/**
 * gdk_display_manager_get_default_display:
 * @display_manager: a #GdkDisplayManager 
 *
 * Gets the default #GdkDisplay.
 *
 * Returns: (transfer none): a #GdkDisplay, or %NULL if there is no default
 *   display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager *display_manager)
{
  return default_display;
}

/**
 * gdk_display_get_default:
 *
 * Gets the default #GdkDisplay. This is a convenience
 * function for
 * <literal>gdk_display_manager_get_default_display (gdk_display_manager_get ())</literal>.
 *
 * Returns: (transfer none): a #GdkDisplay, or %NULL if there is no default
 *   display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_get_default (void)
{
  return default_display;
}

/**
 * gdk_screen_get_default:
 *
 * Gets the default screen for the default display. (See
 * gdk_display_get_default ()).
 *
 * Returns: (transfer none): a #GdkScreen, or %NULL if there is no default display.
 *
 * Since: 2.2
 */
GdkScreen *
gdk_screen_get_default (void)
{
  if (default_display)
    return gdk_display_get_default_screen (default_display);
  else
    return NULL;
}

/**
 * gdk_display_manager_set_default_display:
 * @display_manager: a #GdkDisplayManager
 * @display: a #GdkDisplay
 * 
 * Sets @display as the default display.
 *
 * Since: 2.2
 **/
void
gdk_display_manager_set_default_display (GdkDisplayManager *display_manager,
					 GdkDisplay        *display)
{
  default_display = display;

  _gdk_windowing_set_default_display (display);

  g_object_notify (G_OBJECT (display_manager), "default-display");
}

/**
 * gdk_display_manager_list_displays:
 * @display_manager: a #GdkDisplayManager 
 *
 * List all currently open displays.
 * 
 * Return value: (transfer container) (element-type GdkDisplay): a newly allocated
 * #GSList of #GdkDisplay objects. Free this list with g_slist_free() when you
 * are done with it.
 *
 * Since: 2.2
 **/
GSList *
gdk_display_manager_list_displays (GdkDisplayManager *display_manager)
{
  return g_slist_copy (_gdk_displays);
}

#define __GDK_DISPLAY_MANAGER_C__
#include "gdkaliasdef.c"
