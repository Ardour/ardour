/* gtkrecentchooserutils.h - Private utility functions for implementing a
 *                           GtkRecentChooser interface
 *
 * Copyright (C) 2006 Emmanuele Bassi
 *
 * All rights reserved
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
 *
 * Based on gtkfilechooserutils.c:
 *	Copyright (C) 2003 Red Hat, Inc.
 */

#include "config.h"

#include "gtkrecentchooserutils.h"
#include "gtkalias.h"

/* Methods */
static void      delegate_set_sort_func              (GtkRecentChooser  *chooser,
						      GtkRecentSortFunc  sort_func,
						      gpointer           sort_data,
						      GDestroyNotify     data_destroy);
static void      delegate_add_filter                 (GtkRecentChooser  *chooser,
						      GtkRecentFilter   *filter);
static void      delegate_remove_filter              (GtkRecentChooser  *chooser,
						      GtkRecentFilter   *filter);
static GSList   *delegate_list_filters               (GtkRecentChooser  *chooser);
static gboolean  delegate_select_uri                 (GtkRecentChooser  *chooser,
						      const gchar       *uri,
						      GError           **error);
static void      delegate_unselect_uri               (GtkRecentChooser  *chooser,
						      const gchar       *uri);
static GList    *delegate_get_items                  (GtkRecentChooser  *chooser);
static GtkRecentManager *delegate_get_recent_manager (GtkRecentChooser  *chooser);
static void      delegate_select_all                 (GtkRecentChooser  *chooser);
static void      delegate_unselect_all               (GtkRecentChooser  *chooser);
static gboolean  delegate_set_current_uri            (GtkRecentChooser  *chooser,
						      const gchar       *uri,
						      GError           **error);
static gchar *   delegate_get_current_uri            (GtkRecentChooser  *chooser);

/* Signals */
static void      delegate_notify            (GObject          *object,
					     GParamSpec       *pspec,
					     gpointer          user_data);
static void      delegate_selection_changed (GtkRecentChooser *receiver,
					     gpointer          user_data);
static void      delegate_item_activated    (GtkRecentChooser *receiver,
					     gpointer          user_data);

/**
 * _gtk_recent_chooser_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 *
 * Installs the necessary properties for a class implementing
 * #GtkRecentChooser. A #GtkParamSpecOverride property is installed
 * for each property, using the values from the #GtkRecentChooserProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don't collide with some other property values they
 * are using.
 */
void
_gtk_recent_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER,
  				    "recent-manager");  				    
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE,
  				    "show-private");
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_SHOW_TIPS,
  				    "show-tips");
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_SHOW_ICONS,
  				    "show-icons");
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND,
  				    "show-not-found");
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE,
  				    "select-multiple");
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_LIMIT,
  				    "limit");
  g_object_class_override_property (klass,
		  		    GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY,
				    "local-only");
  g_object_class_override_property (klass,
		  		    GTK_RECENT_CHOOSER_PROP_SORT_TYPE,
				    "sort-type");
  g_object_class_override_property (klass,
  				    GTK_RECENT_CHOOSER_PROP_FILTER,
  				    "filter");
}

/**
 * _gtk_recent_chooser_delegate_iface_init:
 * @iface: a #GtkRecentChooserIface
 *
 * An interface-initialization function for use in cases where
 * an object is simply delegating the methods, signals of
 * the #GtkRecentChooser interface to another object.
 * _gtk_recent_chooser_set_delegate() must be called on each
 * instance of the object so that the delegate object can
 * be found.
 */
void
_gtk_recent_chooser_delegate_iface_init (GtkRecentChooserIface *iface)
{
  iface->set_current_uri = delegate_set_current_uri;
  iface->get_current_uri = delegate_get_current_uri;
  iface->select_uri = delegate_select_uri;
  iface->unselect_uri = delegate_unselect_uri;
  iface->select_all = delegate_select_all;
  iface->unselect_all = delegate_unselect_all;
  iface->get_items = delegate_get_items;
  iface->get_recent_manager = delegate_get_recent_manager;
  iface->set_sort_func = delegate_set_sort_func;
  iface->add_filter = delegate_add_filter;
  iface->remove_filter = delegate_remove_filter;
  iface->list_filters = delegate_list_filters;
}

/**
 * _gtk_recent_chooser_set_delegate:
 * @receiver: a #GObject implementing #GtkRecentChooser
 * @delegate: another #GObject implementing #GtkRecentChooser
 *
 * Establishes that calls on @receiver for #GtkRecentChooser
 * methods should be delegated to @delegate, and that
 * #GtkRecentChooser signals emitted on @delegate should be
 * forwarded to @receiver. Must be used in conjunction with
 * _gtk_recent_chooser_delegate_iface_init().
 */
void
_gtk_recent_chooser_set_delegate (GtkRecentChooser *receiver,
				  GtkRecentChooser *delegate)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (receiver));
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (delegate));
  
  g_object_set_data (G_OBJECT (receiver),
  		    "gtk-recent-chooser-delegate", delegate);
  
  g_signal_connect (delegate, "notify",
  		    G_CALLBACK (delegate_notify), receiver);
  g_signal_connect (delegate, "selection-changed",
  		    G_CALLBACK (delegate_selection_changed), receiver);
  g_signal_connect (delegate, "item-activated",
  		    G_CALLBACK (delegate_item_activated), receiver);
}

GQuark
_gtk_recent_chooser_delegate_get_quark (void)
{
  static GQuark quark = 0;
  
  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gtk-recent-chooser-delegate");
  
  return quark;
}

static GtkRecentChooser *
get_delegate (GtkRecentChooser *receiver)
{
  return g_object_get_qdata (G_OBJECT (receiver),
  			     GTK_RECENT_CHOOSER_DELEGATE_QUARK);
}

static void
delegate_set_sort_func (GtkRecentChooser  *chooser,
			GtkRecentSortFunc  sort_func,
			gpointer           sort_data,
			GDestroyNotify     data_destroy)
{
  gtk_recent_chooser_set_sort_func (get_delegate (chooser),
		  		    sort_func,
				    sort_data,
				    data_destroy);
}

static void
delegate_add_filter (GtkRecentChooser *chooser,
		     GtkRecentFilter  *filter)
{
  gtk_recent_chooser_add_filter (get_delegate (chooser), filter);
}

static void
delegate_remove_filter (GtkRecentChooser *chooser,
			GtkRecentFilter  *filter)
{
  gtk_recent_chooser_remove_filter (get_delegate (chooser), filter);
}

static GSList *
delegate_list_filters (GtkRecentChooser *chooser)
{
  return gtk_recent_chooser_list_filters (get_delegate (chooser));
}

static gboolean
delegate_select_uri (GtkRecentChooser  *chooser,
		     const gchar       *uri,
		     GError           **error)
{
  return gtk_recent_chooser_select_uri (get_delegate (chooser), uri, error);
}

static void
delegate_unselect_uri (GtkRecentChooser *chooser,
		       const gchar      *uri)
{
 gtk_recent_chooser_unselect_uri (get_delegate (chooser), uri);
}

static GList *
delegate_get_items (GtkRecentChooser *chooser)
{
  return gtk_recent_chooser_get_items (get_delegate (chooser));
}

static GtkRecentManager *
delegate_get_recent_manager (GtkRecentChooser *chooser)
{
  return _gtk_recent_chooser_get_recent_manager (get_delegate (chooser));
}

static void
delegate_select_all (GtkRecentChooser *chooser)
{
  gtk_recent_chooser_select_all (get_delegate (chooser));
}

static void
delegate_unselect_all (GtkRecentChooser *chooser)
{
  gtk_recent_chooser_unselect_all (get_delegate (chooser));
}

static gboolean
delegate_set_current_uri (GtkRecentChooser  *chooser,
			  const gchar       *uri,
			  GError           **error)
{
  return gtk_recent_chooser_set_current_uri (get_delegate (chooser), uri, error);
}

static gchar *
delegate_get_current_uri (GtkRecentChooser *chooser)
{
  return gtk_recent_chooser_get_current_uri (get_delegate (chooser));
}

static void
delegate_notify (GObject    *object,
		 GParamSpec *pspec,
		 gpointer    user_data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)),
				 gtk_recent_chooser_get_type ());
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify (user_data, pspec->name);
}

static void
delegate_selection_changed (GtkRecentChooser *receiver,
			    gpointer          user_data)
{
  _gtk_recent_chooser_selection_changed (GTK_RECENT_CHOOSER (user_data));
}

static void
delegate_item_activated (GtkRecentChooser *receiver,
			 gpointer          user_data)
{
  _gtk_recent_chooser_item_activated (GTK_RECENT_CHOOSER (user_data));
}

static gint
sort_recent_items_mru (GtkRecentInfo *a,
		       GtkRecentInfo *b,
		       gpointer       unused)
{
  g_assert (a != NULL && b != NULL);
  
  return gtk_recent_info_get_modified (b) - gtk_recent_info_get_modified (a);
}

static gint
sort_recent_items_lru (GtkRecentInfo *a,
		       GtkRecentInfo *b,
		       gpointer       unused)
{
  g_assert (a != NULL && b != NULL);
  
  return -1 * (gtk_recent_info_get_modified (b) - gtk_recent_info_get_modified (a));
}

typedef struct
{
  GtkRecentSortFunc func;
  gpointer data;
} SortRecentData;

/* our proxy sorting function */
static gint
sort_recent_items_proxy (gpointer *a,
                         gpointer *b,
                         gpointer  user_data)
{
  GtkRecentInfo *info_a = (GtkRecentInfo *) a;
  GtkRecentInfo *info_b = (GtkRecentInfo *) b;
  SortRecentData *sort_recent = user_data;

  if (sort_recent->func)
    return (* sort_recent->func) (info_a, info_b, sort_recent->data);
  
  /* fallback */
  return 0;
}

static gboolean
get_is_recent_filtered (GtkRecentFilter *filter,
			GtkRecentInfo   *info)
{
  GtkRecentFilterInfo filter_info;
  GtkRecentFilterFlags needed;
  gboolean retval;

  g_assert (info != NULL);
  
  needed = gtk_recent_filter_get_needed (filter);
  
  filter_info.contains = GTK_RECENT_FILTER_URI | GTK_RECENT_FILTER_MIME_TYPE;
  
  filter_info.uri = gtk_recent_info_get_uri (info);
  filter_info.mime_type = gtk_recent_info_get_mime_type (info);
  
  if (needed & GTK_RECENT_FILTER_DISPLAY_NAME)
    {
      filter_info.display_name = gtk_recent_info_get_display_name (info);
      filter_info.contains |= GTK_RECENT_FILTER_DISPLAY_NAME;
    }
  else
    filter_info.uri = NULL;
  
  if (needed & GTK_RECENT_FILTER_APPLICATION)
    {
      filter_info.applications = (const gchar **) gtk_recent_info_get_applications (info, NULL);
      filter_info.contains |= GTK_RECENT_FILTER_APPLICATION;
    }
  else
    filter_info.applications = NULL;

  if (needed & GTK_RECENT_FILTER_GROUP)
    {
      filter_info.groups = (const gchar **) gtk_recent_info_get_groups (info, NULL);
      filter_info.contains |= GTK_RECENT_FILTER_GROUP;
    }
  else
    filter_info.groups = NULL;

  if (needed & GTK_RECENT_FILTER_AGE)
    {
      filter_info.age = gtk_recent_info_get_age (info);
      filter_info.contains |= GTK_RECENT_FILTER_AGE;
    }
  else
    filter_info.age = -1;
  
  retval = gtk_recent_filter_filter (filter, &filter_info);
  
  /* these we own */
  if (filter_info.applications)
    g_strfreev ((gchar **) filter_info.applications);
  if (filter_info.groups)
    g_strfreev ((gchar **) filter_info.groups);
  
  return !retval;
}

/*
 * _gtk_recent_chooser_get_items:
 * @chooser: a #GtkRecentChooser
 * @filter: a #GtkRecentFilter
 * @sort_func: (allow-none): sorting function, or %NULL
 * @sort_data: (allow-none): sorting function data, or %NULL
 *
 * Default implementation for getting the filtered, sorted and
 * clamped list of recently used resources from a #GtkRecentChooser.
 * This function should be used by implementations of the
 * #GtkRecentChooser interface inside the GtkRecentChooser::get_items
 * vfunc.
 *
 * Return value: a list of #GtkRecentInfo objects
 */
GList *
_gtk_recent_chooser_get_items (GtkRecentChooser  *chooser,
                               GtkRecentFilter   *filter,
                               GtkRecentSortFunc  sort_func,
                               gpointer           sort_data)
{
  GtkRecentManager *manager;
  gint limit;
  GtkRecentSortType sort_type;
  GList *items;
  GCompareDataFunc compare_func;
  gint length;

  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);

  manager = _gtk_recent_chooser_get_recent_manager (chooser);
  if (!manager)
    return NULL;

  items = gtk_recent_manager_get_items (manager);
  if (!items)
    return NULL;

  limit = gtk_recent_chooser_get_limit (chooser);
  if (limit == 0)
    return NULL;

  if (filter)
    {
      GList *filter_items, *l;
      gboolean local_only = FALSE;
      gboolean show_private = FALSE;
      gboolean show_not_found = FALSE;

      g_object_get (G_OBJECT (chooser),
                    "local-only", &local_only,
                    "show-private", &show_private,
                    "show-not-found", &show_not_found,
                    NULL);

      filter_items = NULL;
      for (l = items; l != NULL; l = l->next)
        {
          GtkRecentInfo *info = l->data;
          gboolean remove_item = FALSE;

          if (get_is_recent_filtered (filter, info))
            remove_item = TRUE;
          
          if (local_only && !gtk_recent_info_is_local (info))
            remove_item = TRUE;

          if (!show_private && gtk_recent_info_get_private_hint (info))
            remove_item = TRUE;

          if (!show_not_found && !gtk_recent_info_exists (info))
            remove_item = TRUE;
          
          if (!remove_item)
            filter_items = g_list_prepend (filter_items, info);
          else
            gtk_recent_info_unref (info);
        }
      
      g_list_free (items);
      items = filter_items;
    }

  if (!items)
    return NULL;

  sort_type = gtk_recent_chooser_get_sort_type (chooser);
  switch (sort_type)
    {
    case GTK_RECENT_SORT_NONE:
      compare_func = NULL;
      break;
    case GTK_RECENT_SORT_MRU:
      compare_func = (GCompareDataFunc) sort_recent_items_mru;
      break;
    case GTK_RECENT_SORT_LRU:
      compare_func = (GCompareDataFunc) sort_recent_items_lru;
      break;
    case GTK_RECENT_SORT_CUSTOM:
      compare_func = (GCompareDataFunc) sort_recent_items_proxy;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  if (compare_func)
    {
      SortRecentData sort_recent;

      sort_recent.func = sort_func;
      sort_recent.data = sort_data;

      items = g_list_sort_with_data (items, compare_func, &sort_recent);
    }
  
  length = g_list_length (items);
  if ((limit != -1) && (length > limit))
    {
      GList *clamp, *l;
      
      clamp = g_list_nth (items, limit - 1);
      if (!clamp)
        return items;
      
      l = clamp->next;
      clamp->next = NULL;
    
      g_list_foreach (l, (GFunc) gtk_recent_info_unref, NULL);
      g_list_free (l);
    }

  return items;
}
