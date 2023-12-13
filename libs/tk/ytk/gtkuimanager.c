/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <string.h>
#include "gtkaccellabel.h"
#include "gtkactivatable.h"
#include "gtkbuildable.h"
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkmenutoolbutton.h"
#include "gtkseparatormenuitem.h"
#include "gtkseparatortoolitem.h"
#include "gtktearoffmenuitem.h"
#include "gtktoolbar.h"
#include "gtkuimanager.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#undef DEBUG_UI_MANAGER

typedef enum
{
  NODE_TYPE_UNDECIDED,
  NODE_TYPE_ROOT,
  NODE_TYPE_MENUBAR,
  NODE_TYPE_MENU,
  NODE_TYPE_TOOLBAR,
  NODE_TYPE_MENU_PLACEHOLDER,
  NODE_TYPE_TOOLBAR_PLACEHOLDER,
  NODE_TYPE_POPUP,
  NODE_TYPE_MENUITEM,
  NODE_TYPE_TOOLITEM,
  NODE_TYPE_SEPARATOR,
  NODE_TYPE_ACCELERATOR
} NodeType;

typedef struct _Node Node;

struct _Node {
  NodeType type;

  gchar *name;

  GQuark action_name;
  GtkAction *action;
  GtkWidget *proxy;
  GtkWidget *extra; /* second separator for placeholders */

  GList *uifiles;

  guint dirty : 1;
  guint expand : 1;  /* used for separators */
  guint popup_accels : 1;
  guint always_show_image_set : 1; /* used for menu items */
  guint always_show_image     : 1; /* used for menu items */
};

#define GTK_UI_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_UI_MANAGER, GtkUIManagerPrivate))

struct _GtkUIManagerPrivate 
{
  GtkAccelGroup *accel_group;

  GNode *root_node;
  GList *action_groups;

  guint last_merge_id;

  guint update_tag;  

  gboolean add_tearoffs;
};

#define NODE_INFO(node) ((Node *)node->data)

typedef struct _NodeUIReference NodeUIReference;

struct _NodeUIReference 
{
  guint merge_id;
  GQuark action_quark;
};

static void        gtk_ui_manager_finalize        (GObject           *object);
static void        gtk_ui_manager_set_property    (GObject           *object,
                                                   guint              prop_id,
                                                   const GValue      *value,
                                                   GParamSpec        *pspec);
static void        gtk_ui_manager_get_property    (GObject           *object,
                                                   guint              prop_id,
                                                   GValue            *value,
                                                   GParamSpec        *pspec);
static GtkWidget * gtk_ui_manager_real_get_widget (GtkUIManager      *manager,
                                                   const gchar       *path);
static GtkAction * gtk_ui_manager_real_get_action (GtkUIManager      *manager,
                                                   const gchar       *path);
static void        queue_update                   (GtkUIManager      *self);
static void        dirty_all_nodes                (GtkUIManager      *self);
static void        mark_node_dirty                (GNode             *node);
static GNode     * get_child_node                 (GtkUIManager      *self,
                                                   GNode             *parent,
						   GNode             *sibling,
                                                   const gchar       *childname,
                                                   gint               childname_length,
                                                   NodeType           node_type,
                                                   gboolean           create,
                                                   gboolean           top);
static GNode     * get_node                       (GtkUIManager      *self,
                                                   const gchar       *path,
                                                   NodeType           node_type,
                                                   gboolean           create);
static gboolean    free_node                      (GNode             *node);
static void        node_prepend_ui_reference      (GNode             *node,
                                                   guint              merge_id,
                                                   GQuark             action_quark);
static void        node_remove_ui_reference       (GNode             *node,
                                                   guint              merge_id);

/* GtkBuildable */
static void gtk_ui_manager_buildable_init      (GtkBuildableIface *iface);
static void gtk_ui_manager_buildable_add_child (GtkBuildable  *buildable,
						GtkBuilder    *builder,
						GObject       *child,
						const gchar   *type);
static GObject* gtk_ui_manager_buildable_construct_child (GtkBuildable *buildable,
							  GtkBuilder   *builder,
							  const gchar  *name);
static gboolean gtk_ui_manager_buildable_custom_tag_start (GtkBuildable  *buildable,
							   GtkBuilder    *builder,
							   GObject       *child,
							   const gchar   *tagname,
							   GMarkupParser *parser,
							   gpointer      *data);
static void     gtk_ui_manager_buildable_custom_tag_end (GtkBuildable 	 *buildable,
							 GtkBuilder   	 *builder,
							 GObject      	 *child,
							 const gchar  	 *tagname,
							 gpointer     	 *data);



enum 
{
  ADD_WIDGET,
  ACTIONS_CHANGED,
  CONNECT_PROXY,
  DISCONNECT_PROXY,
  PRE_ACTIVATE,
  POST_ACTIVATE,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ADD_TEAROFFS,
  PROP_UI
};

static guint ui_manager_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkUIManager, gtk_ui_manager, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_ui_manager_buildable_init))

static void
gtk_ui_manager_class_init (GtkUIManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_ui_manager_finalize;
  gobject_class->set_property = gtk_ui_manager_set_property;
  gobject_class->get_property = gtk_ui_manager_get_property;
  klass->get_widget = gtk_ui_manager_real_get_widget;
  klass->get_action = gtk_ui_manager_real_get_action;

  /**
   * GtkUIManager:add-tearoffs:
   *
   * The "add-tearoffs" property controls whether generated menus 
   * have tearoff menu items. 
   *
   * Note that this only affects regular menus. Generated popup 
   * menus never have tearoff menu items.   
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ADD_TEAROFFS,
                                   g_param_spec_boolean ("add-tearoffs",
							 P_("Add tearoffs to menus"),
							 P_("Whether tearoff menu items should be added to menus"),
                                                         FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_UI,
				   g_param_spec_string ("ui",
 							P_("Merged UI definition"),
							P_("An XML string describing the merged UI"),
							"<ui>\n</ui>\n",
							GTK_PARAM_READABLE));


  /**
   * GtkUIManager::add-widget:
   * @merge: a #GtkUIManager
   * @widget: the added widget
   *
   * The add_widget signal is emitted for each generated menubar and toolbar.
   * It is not emitted for generated popup menus, which can be obtained by 
   * gtk_ui_manager_get_widget().
   *
   * Since: 2.4
   */
  ui_manager_signals[ADD_WIDGET] =
    g_signal_new (I_("add-widget"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, add_widget),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, 
		  GTK_TYPE_WIDGET);

  /**
   * GtkUIManager::actions-changed:
   * @merge: a #GtkUIManager
   *
   * The "actions-changed" signal is emitted whenever the set of actions
   * changes.
   *
   * Since: 2.4
   */
  ui_manager_signals[ACTIONS_CHANGED] =
    g_signal_new (I_("actions-changed"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, actions_changed),  
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  /**
   * GtkUIManager::connect-proxy:
   * @uimanager: the ui manager
   * @action: the action
   * @proxy: the proxy
   *
   * The connect_proxy signal is emitted after connecting a proxy to 
   * an action in the group. 
   *
   * This is intended for simple customizations for which a custom action
   * class would be too clumsy, e.g. showing tooltips for menuitems in the
   * statusbar.
   *
   * Since: 2.4
   */
  ui_manager_signals[CONNECT_PROXY] =
    g_signal_new (I_("connect-proxy"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, connect_proxy),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2, 
		  GTK_TYPE_ACTION,
		  GTK_TYPE_WIDGET);

  /**
   * GtkUIManager::disconnect-proxy:
   * @uimanager: the ui manager
   * @action: the action
   * @proxy: the proxy
   *
   * The disconnect_proxy signal is emitted after disconnecting a proxy 
   * from an action in the group. 
   *
   * Since: 2.4
   */
  ui_manager_signals[DISCONNECT_PROXY] =
    g_signal_new (I_("disconnect-proxy"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, disconnect_proxy),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_ACTION,
		  GTK_TYPE_WIDGET);

  /**
   * GtkUIManager::pre-activate:
   * @uimanager: the ui manager
   * @action: the action
   *
   * The pre_activate signal is emitted just before the @action
   * is activated.
   *
   * This is intended for applications to get notification
   * just before any action is activated.
   *
   * Since: 2.4
   */
  ui_manager_signals[PRE_ACTIVATE] =
    g_signal_new (I_("pre-activate"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, pre_activate),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ACTION);

  /**
   * GtkUIManager::post-activate:
   * @uimanager: the ui manager
   * @action: the action
   *
   * The post_activate signal is emitted just after the @action
   * is activated.
   *
   * This is intended for applications to get notification
   * just after any action is activated.
   *
   * Since: 2.4
   */
  ui_manager_signals[POST_ACTIVATE] =
    g_signal_new (I_("post-activate"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkUIManagerClass, post_activate),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ACTION);

  klass->add_widget = NULL;
  klass->actions_changed = NULL;
  klass->connect_proxy = NULL;
  klass->disconnect_proxy = NULL;
  klass->pre_activate = NULL;
  klass->post_activate = NULL;

  g_type_class_add_private (gobject_class, sizeof (GtkUIManagerPrivate));
}


static void
gtk_ui_manager_init (GtkUIManager *self)
{
  guint merge_id;
  GNode *node;

  self->private_data = GTK_UI_MANAGER_GET_PRIVATE (self);

  self->private_data->accel_group = gtk_accel_group_new ();

  self->private_data->root_node = NULL;
  self->private_data->action_groups = NULL;

  self->private_data->last_merge_id = 0;
  self->private_data->add_tearoffs = FALSE;

  merge_id = gtk_ui_manager_new_merge_id (self);
  node = get_child_node (self, NULL, NULL, "ui", 2,
			 NODE_TYPE_ROOT, TRUE, FALSE);
  node_prepend_ui_reference (node, merge_id, 0);
}

static void
gtk_ui_manager_finalize (GObject *object)
{
  GtkUIManager *self = GTK_UI_MANAGER (object);
  
  if (self->private_data->update_tag != 0)
    {
      g_source_remove (self->private_data->update_tag);
      self->private_data->update_tag = 0;
    }
  
  g_node_traverse (self->private_data->root_node, 
		   G_POST_ORDER, G_TRAVERSE_ALL, -1,
		   (GNodeTraverseFunc)free_node, NULL);
  g_node_destroy (self->private_data->root_node);
  self->private_data->root_node = NULL;
  
  g_list_foreach (self->private_data->action_groups,
                  (GFunc) g_object_unref, NULL);
  g_list_free (self->private_data->action_groups);
  self->private_data->action_groups = NULL;

  g_object_unref (self->private_data->accel_group);
  self->private_data->accel_group = NULL;

  G_OBJECT_CLASS (gtk_ui_manager_parent_class)->finalize (object);
}

static void
gtk_ui_manager_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_ui_manager_buildable_add_child;
  iface->construct_child = gtk_ui_manager_buildable_construct_child;
  iface->custom_tag_start = gtk_ui_manager_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_ui_manager_buildable_custom_tag_end;
}

static void
gtk_ui_manager_buildable_add_child (GtkBuildable  *buildable,
				    GtkBuilder    *builder,
				    GObject       *child,
				    const gchar   *type)
{
  GtkUIManager *self = GTK_UI_MANAGER (buildable);
  guint pos;

  g_return_if_fail (GTK_IS_ACTION_GROUP (child));

  pos = g_list_length (self->private_data->action_groups);

  g_object_ref (child);
  gtk_ui_manager_insert_action_group (self,
				      GTK_ACTION_GROUP (child),
				      pos);
}

static void
child_hierarchy_changed_cb (GtkWidget *widget,
			    GtkWidget *unused,
			    GtkUIManager *uimgr)
{
  GtkWidget *toplevel;
  GtkAccelGroup *group;
  GSList *groups;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!toplevel || !GTK_IS_WINDOW (toplevel))
    return;
  
  group = gtk_ui_manager_get_accel_group (uimgr);
  groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
  if (g_slist_find (groups, group) == NULL)
    gtk_window_add_accel_group (GTK_WINDOW (toplevel), group);

  g_signal_handlers_disconnect_by_func (widget,
					child_hierarchy_changed_cb,
					uimgr);
}

static GObject *
gtk_ui_manager_buildable_construct_child (GtkBuildable *buildable,
					  GtkBuilder   *builder,
					  const gchar  *id)
{
  GtkWidget *widget;
  char *name;

  name = g_strdup_printf ("ui/%s", id);
  widget = gtk_ui_manager_get_widget (GTK_UI_MANAGER (buildable), name);
  if (!widget)
    {
      g_error ("Unknown ui manager child: %s\n", name);
      g_free (name);
      return NULL;
    }
  g_free (name);

  g_signal_connect (widget, "hierarchy-changed",
		    G_CALLBACK (child_hierarchy_changed_cb),
		    GTK_UI_MANAGER (buildable));
  return g_object_ref (widget);
}

static void
gtk_ui_manager_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  GtkUIManager *self = GTK_UI_MANAGER (object);
 
  switch (prop_id)
    {
    case PROP_ADD_TEAROFFS:
      gtk_ui_manager_set_add_tearoffs (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_ui_manager_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  GtkUIManager *self = GTK_UI_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ADD_TEAROFFS:
      g_value_set_boolean (value, self->private_data->add_tearoffs);
      break;
    case PROP_UI:
      g_value_take_string (value, gtk_ui_manager_get_ui (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkWidget *
gtk_ui_manager_real_get_widget (GtkUIManager *self,
                                const gchar  *path)
{
  GNode *node;

  /* ensure that there are no pending updates before we get the
   * widget */
  gtk_ui_manager_ensure_update (self);

  node = get_node (self, path, NODE_TYPE_UNDECIDED, FALSE);

  if (node == NULL)
    return NULL;

  return NODE_INFO (node)->proxy;
}

static GtkAction *
gtk_ui_manager_real_get_action (GtkUIManager *self,
                                const gchar  *path)
{
  GNode *node;

  /* ensure that there are no pending updates before we get
   * the action */
  gtk_ui_manager_ensure_update (self);

  node = get_node (self, path, NODE_TYPE_UNDECIDED, FALSE);

  if (node == NULL)
    return NULL;

  return NODE_INFO (node)->action;
}


/**
 * gtk_ui_manager_new:
 * 
 * Creates a new ui manager object.
 * 
 * Return value: a new ui manager object.
 *
 * Since: 2.4
 **/
GtkUIManager*
gtk_ui_manager_new (void)
{
  return g_object_new (GTK_TYPE_UI_MANAGER, NULL);
}


/**
 * gtk_ui_manager_get_add_tearoffs:
 * @self: a #GtkUIManager
 * 
 * Returns whether menus generated by this #GtkUIManager
 * will have tearoff menu items. 
 * 
 * Return value: whether tearoff menu items are added
 *
 * Since: 2.4
 **/
gboolean 
gtk_ui_manager_get_add_tearoffs (GtkUIManager *self)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), FALSE);
  
  return self->private_data->add_tearoffs;
}


/**
 * gtk_ui_manager_set_add_tearoffs:
 * @self: a #GtkUIManager
 * @add_tearoffs: whether tearoff menu items are added
 * 
 * Sets the "add_tearoffs" property, which controls whether menus 
 * generated by this #GtkUIManager will have tearoff menu items. 
 *
 * Note that this only affects regular menus. Generated popup 
 * menus never have tearoff menu items.
 *
 * Since: 2.4
 **/
void 
gtk_ui_manager_set_add_tearoffs (GtkUIManager *self,
				 gboolean      add_tearoffs)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (self));

  add_tearoffs = add_tearoffs != FALSE;

  if (add_tearoffs != self->private_data->add_tearoffs)
    {
      self->private_data->add_tearoffs = add_tearoffs;
      
      dirty_all_nodes (self);

      g_object_notify (G_OBJECT (self), "add-tearoffs");
    }
}

static void
cb_proxy_connect_proxy (GtkActionGroup *group, 
                        GtkAction      *action,
                        GtkWidget      *proxy, 
                        GtkUIManager *self)
{
  g_signal_emit (self, ui_manager_signals[CONNECT_PROXY], 0, action, proxy);
}

static void
cb_proxy_disconnect_proxy (GtkActionGroup *group, 
                           GtkAction      *action,
                           GtkWidget      *proxy, 
                           GtkUIManager *self)
{
  g_signal_emit (self, ui_manager_signals[DISCONNECT_PROXY], 0, action, proxy);
}

static void
cb_proxy_pre_activate (GtkActionGroup *group, 
                       GtkAction      *action,
                       GtkUIManager   *self)
{
  g_signal_emit (self, ui_manager_signals[PRE_ACTIVATE], 0, action);
}

static void
cb_proxy_post_activate (GtkActionGroup *group, 
                        GtkAction      *action,
                        GtkUIManager   *self)
{
  g_signal_emit (self, ui_manager_signals[POST_ACTIVATE], 0, action);
}

/**
 * gtk_ui_manager_insert_action_group:
 * @self: a #GtkUIManager object
 * @action_group: the action group to be inserted
 * @pos: the position at which the group will be inserted.
 * 
 * Inserts an action group into the list of action groups associated 
 * with @self. Actions in earlier groups hide actions with the same 
 * name in later groups. 
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_insert_action_group (GtkUIManager   *self,
				    GtkActionGroup *action_group, 
				    gint            pos)
{
#ifdef G_ENABLE_DEBUG
  GList *l;
  const char *group_name;
#endif 

  g_return_if_fail (GTK_IS_UI_MANAGER (self));
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (g_list_find (self->private_data->action_groups, 
				 action_group) == NULL);

#ifdef G_ENABLE_DEBUG
  group_name  = gtk_action_group_get_name (action_group);

  for (l = self->private_data->action_groups; l; l = l->next) 
    {
      GtkActionGroup *group = l->data;

      if (strcmp (gtk_action_group_get_name (group), group_name) == 0)
        {
          g_warning ("Inserting action group '%s' into UI manager which "
		     "already has a group with this name\n", group_name);
          break;
        }
    }
#endif /* G_ENABLE_DEBUG */

  g_object_ref (action_group);
  self->private_data->action_groups = 
    g_list_insert (self->private_data->action_groups, action_group, pos);
  g_object_connect (action_group,
		    "object-signal::connect-proxy", G_CALLBACK (cb_proxy_connect_proxy), self,
		    "object-signal::disconnect-proxy", G_CALLBACK (cb_proxy_disconnect_proxy), self,
		    "object-signal::pre-activate", G_CALLBACK (cb_proxy_pre_activate), self,
		    "object-signal::post-activate", G_CALLBACK (cb_proxy_post_activate), self,
		    NULL);

  /* dirty all nodes, as action bindings may change */
  dirty_all_nodes (self);

  g_signal_emit (self, ui_manager_signals[ACTIONS_CHANGED], 0);
}

/**
 * gtk_ui_manager_remove_action_group:
 * @self: a #GtkUIManager object
 * @action_group: the action group to be removed
 * 
 * Removes an action group from the list of action groups associated 
 * with @self.
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_remove_action_group (GtkUIManager   *self,
				    GtkActionGroup *action_group)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (self));
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (g_list_find (self->private_data->action_groups, 
				 action_group) != NULL);

  self->private_data->action_groups =
    g_list_remove (self->private_data->action_groups, action_group);

  g_object_disconnect (action_group,
                       "any-signal::connect-proxy", G_CALLBACK (cb_proxy_connect_proxy), self,
                       "any-signal::disconnect-proxy", G_CALLBACK (cb_proxy_disconnect_proxy), self,
                       "any-signal::pre-activate", G_CALLBACK (cb_proxy_pre_activate), self,
                       "any-signal::post-activate", G_CALLBACK (cb_proxy_post_activate), self, 
                       NULL);
  g_object_unref (action_group);

  /* dirty all nodes, as action bindings may change */
  dirty_all_nodes (self);

  g_signal_emit (self, ui_manager_signals[ACTIONS_CHANGED], 0);
}

/**
 * gtk_ui_manager_get_action_groups:
 * @self: a #GtkUIManager object
 * 
 * Returns the list of action groups associated with @self.
 *
 * Return value:  (element-type GtkActionGroup) (transfer none): a #GList of
 *   action groups. The list is owned by GTK+
 *   and should not be modified.
 *
 * Since: 2.4
 **/
GList *
gtk_ui_manager_get_action_groups (GtkUIManager *self)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);

  return self->private_data->action_groups;
}

/**
 * gtk_ui_manager_get_accel_group:
 * @self: a #GtkUIManager object
 * 
 * Returns the #GtkAccelGroup associated with @self.
 *
 * Return value: (transfer none): the #GtkAccelGroup.
 *
 * Since: 2.4
 **/
GtkAccelGroup *
gtk_ui_manager_get_accel_group (GtkUIManager *self)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);

  return self->private_data->accel_group;
}

/**
 * gtk_ui_manager_get_widget:
 * @self: a #GtkUIManager
 * @path: a path
 * 
 * Looks up a widget by following a path. 
 * The path consists of the names specified in the XML description of the UI. 
 * separated by '/'. Elements which don't have a name or action attribute in 
 * the XML (e.g. &lt;popup&gt;) can be addressed by their XML element name 
 * (e.g. "popup"). The root element ("/ui") can be omitted in the path.
 *
 * Note that the widget found by following a path that ends in a &lt;menu&gt;
 * element is the menuitem to which the menu is attached, not the menu itself.
 *
 * Also note that the widgets constructed by a ui manager are not tied to 
 * the lifecycle of the ui manager. If you add the widgets returned by this 
 * function to some container or explicitly ref them, they will survive the
 * destruction of the ui manager.
 *
 * Return value: (transfer none): the widget found by following the path, or %NULL if no widget
 *   was found.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_ui_manager_get_widget (GtkUIManager *self,
			   const gchar  *path)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return GTK_UI_MANAGER_GET_CLASS (self)->get_widget (self, path);
}

typedef struct {
  GtkUIManagerItemType types;
  GSList *list;
} ToplevelData;

static void
collect_toplevels (GNode   *node, 
		   gpointer user_data)
{
  ToplevelData *data = user_data;

  if (NODE_INFO (node)->proxy)
    {
      switch (NODE_INFO (node)->type) 
	{
	case NODE_TYPE_MENUBAR:
	  if (data->types & GTK_UI_MANAGER_MENUBAR)
	data->list = g_slist_prepend (data->list, NODE_INFO (node)->proxy);
	  break;
	case NODE_TYPE_TOOLBAR:
      if (data->types & GTK_UI_MANAGER_TOOLBAR)
	data->list = g_slist_prepend (data->list, NODE_INFO (node)->proxy);
      break;
	case NODE_TYPE_POPUP:
	  if (data->types & GTK_UI_MANAGER_POPUP)
	    data->list = g_slist_prepend (data->list, NODE_INFO (node)->proxy);
	  break;
	default: ;
	}
    }
}

/**
 * gtk_ui_manager_get_toplevels:
 * @self: a #GtkUIManager
 * @types: specifies the types of toplevel widgets to include. Allowed
 *   types are #GTK_UI_MANAGER_MENUBAR, #GTK_UI_MANAGER_TOOLBAR and
 *   #GTK_UI_MANAGER_POPUP.
 * 
 * Obtains a list of all toplevel widgets of the requested types.
 *
 * Return value: (element-type GtkWidget) (transfer container): a newly-allocated #GSList of
 * all toplevel widgets of the requested types.  Free the returned list with g_slist_free().
 *
 * Since: 2.4
 **/
GSList *
gtk_ui_manager_get_toplevels (GtkUIManager         *self,
			      GtkUIManagerItemType  types)
{
  ToplevelData data;

  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);
  g_return_val_if_fail ((~(GTK_UI_MANAGER_MENUBAR | 
			   GTK_UI_MANAGER_TOOLBAR |
			   GTK_UI_MANAGER_POPUP) & types) == 0, NULL);
  
      
  data.types = types;
  data.list = NULL;

  g_node_children_foreach (self->private_data->root_node, 
			   G_TRAVERSE_ALL, 
			   collect_toplevels, &data);

  return data.list;
}


/**
 * gtk_ui_manager_get_action:
 * @self: a #GtkUIManager
 * @path: a path
 * 
 * Looks up an action by following a path. See gtk_ui_manager_get_widget()
 * for more information about paths.
 * 
 * Return value: (transfer none): the action whose proxy widget is found by following the path, 
 *     or %NULL if no widget was found.
 *
 * Since: 2.4
 **/
GtkAction *
gtk_ui_manager_get_action (GtkUIManager *self,
			   const gchar  *path)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return GTK_UI_MANAGER_GET_CLASS (self)->get_action (self, path);
}

static gboolean
node_is_dead (GNode *node)
{
  GNode *child;

  if (NODE_INFO (node)->uifiles != NULL)
    return FALSE;

  for (child = node->children; child != NULL; child = child->next)
    {
      if (!node_is_dead (child))
	return FALSE;
    }

  return TRUE;
}

static GNode *
get_child_node (GtkUIManager *self, 
		GNode        *parent,
		GNode        *sibling,
		const gchar  *childname, 
		gint          childname_length,
		NodeType      node_type,
		gboolean      create, 
		gboolean      top)
{
  GNode *child = NULL;

  if (parent)
    {
      if (childname)
	{
	  for (child = parent->children; child != NULL; child = child->next)
	    {
	      if (NODE_INFO (child)->name &&
		  strlen (NODE_INFO (child)->name) == childname_length &&
		  !strncmp (NODE_INFO (child)->name, childname, childname_length))
		{
		  /* if undecided about node type, set it */
		  if (NODE_INFO (child)->type == NODE_TYPE_UNDECIDED)
		    NODE_INFO (child)->type = node_type;
		  
		  /* warn about type mismatch */
		  if (NODE_INFO (child)->type != NODE_TYPE_UNDECIDED &&
		      node_type != NODE_TYPE_UNDECIDED &&
		      NODE_INFO (child)->type != node_type)
		    g_warning ("node type doesn't match %d (%s is type %d)",
			       node_type, 
			       NODE_INFO (child)->name,
			       NODE_INFO (child)->type);

                    if (node_is_dead (child))
                      {
                        /* This node was removed but is still dirty so
                         * it is still in the tree. We want to treat this
                         * as if it didn't exist, which means we move it
                         * to the position it would have been created at.
                         */
                        g_node_unlink (child);
                        goto insert_child;
                      }

		  return child;
		}
	    }
	}
      if (!child && create)
	{
	  Node *mnode;
	  
	  mnode = g_slice_new0 (Node);
	  mnode->type = node_type;
	  mnode->name = g_strndup (childname, childname_length);

	  child = g_node_new (mnode);
	insert_child:
	  if (sibling)
	    {
	      if (top)
		g_node_insert_before (parent, sibling, child);
	      else
		g_node_insert_after (parent, sibling, child);
	    }
	  else
	    {
	      if (top)
		g_node_prepend (parent, child);
	      else
		g_node_append (parent, child);
	    }

	  mark_node_dirty (child);
	}
    }
  else
    {
      /* handle root node */
      if (self->private_data->root_node)
	{
	  child = self->private_data->root_node;
	  if (strncmp (NODE_INFO (child)->name, childname, childname_length) != 0)
	    g_warning ("root node name '%s' doesn't match '%s'",
		       childname, NODE_INFO (child)->name);
	  if (NODE_INFO (child)->type != NODE_TYPE_ROOT)
	    g_warning ("base element must be of type ROOT");
	}
      else if (create)
	{
	  Node *mnode;

	  mnode = g_slice_new0 (Node);
	  mnode->type = node_type;
	  mnode->name = g_strndup (childname, childname_length);
	  mnode->dirty = TRUE;
	  
	  child = self->private_data->root_node = g_node_new (mnode);
	}
    }

  return child;
}

static GNode *
get_node (GtkUIManager *self, 
	  const gchar  *path,
	  NodeType      node_type, 
	  gboolean      create)
{
  const gchar *pos, *end;
  GNode *parent, *node;
  
  if (strncmp ("/ui", path, 3) == 0)
    path += 3;
  
  end = path + strlen (path);
  pos = path;
  parent = node = NULL;
  while (pos < end)
    {
      const gchar *slash;
      gsize length;

      slash = strchr (pos, '/');
      if (slash)
	length = slash - pos;
      else
	length = strlen (pos);

      node = get_child_node (self, parent, NULL, pos, length, NODE_TYPE_UNDECIDED,
			     create, FALSE);
      if (!node)
	return NULL;

      pos += length + 1; /* move past the node name and the slash too */
      parent = node;
    }

  if (node != NULL && NODE_INFO (node)->type == NODE_TYPE_UNDECIDED)
    NODE_INFO (node)->type = node_type;

  return node;
}

static void
node_ui_reference_free (gpointer data)
{
  g_slice_free (NodeUIReference, data);
}

static gboolean
free_node (GNode *node)
{
  Node *info = NODE_INFO (node);
  
  g_list_foreach (info->uifiles, (GFunc) node_ui_reference_free, NULL);
  g_list_free (info->uifiles);

  if (info->action)
    g_object_unref (info->action);
  if (info->proxy)
    g_object_unref (info->proxy);
  if (info->extra)
    g_object_unref (info->extra);
  g_free (info->name);
  g_slice_free (Node, info);

  return FALSE;
}

/**
 * gtk_ui_manager_new_merge_id:
 * @self: a #GtkUIManager
 * 
 * Returns an unused merge id, suitable for use with 
 * gtk_ui_manager_add_ui().
 * 
 * Return value: an unused merge id.
 *
 * Since: 2.4
 **/
guint
gtk_ui_manager_new_merge_id (GtkUIManager *self)
{
  self->private_data->last_merge_id++;

  return self->private_data->last_merge_id;
}

static void
node_prepend_ui_reference (GNode  *gnode,
			   guint   merge_id, 
			   GQuark  action_quark)
{
  Node *node = NODE_INFO (gnode);
  NodeUIReference *reference = NULL;

  if (node->uifiles && 
      ((NodeUIReference *)node->uifiles->data)->merge_id == merge_id)
    reference = node->uifiles->data;
  else
    {
      reference = g_slice_new (NodeUIReference);
      node->uifiles = g_list_prepend (node->uifiles, reference);
    }

  reference->merge_id = merge_id;
  reference->action_quark = action_quark;

  mark_node_dirty (gnode);
}

static void
node_remove_ui_reference (GNode  *gnode,
			  guint  merge_id)
{
  Node *node = NODE_INFO (gnode);
  GList *p;
  
  for (p = node->uifiles; p != NULL; p = p->next)
    {
      NodeUIReference *reference = p->data;
      
      if (reference->merge_id == merge_id)
	{
	  if (p == node->uifiles)
	    mark_node_dirty (gnode);
	  node->uifiles = g_list_delete_link (node->uifiles, p);
	  g_slice_free (NodeUIReference, reference);

	  break;
	}
    }
}

/* -------------------- The UI file parser -------------------- */

typedef enum
{
  STATE_START,
  STATE_ROOT,
  STATE_MENU,
  STATE_TOOLBAR,
  STATE_MENUITEM,
  STATE_TOOLITEM,
  STATE_ACCELERATOR,
  STATE_END
} ParseState;

typedef struct _ParseContext ParseContext;
struct _ParseContext
{
  ParseState state;
  ParseState prev_state;

  GtkUIManager *self;

  GNode *current;

  guint merge_id;
};

static void
start_element_handler (GMarkupParseContext *context,
		       const gchar         *element_name,
		       const gchar        **attribute_names,
		       const gchar        **attribute_values,
		       gpointer             user_data,
		       GError             **error)
{
  ParseContext *ctx = user_data;
  GtkUIManager *self = ctx->self;

  gint i;
  const gchar *node_name;
  const gchar *action;
  GQuark action_quark;
  gboolean top;
  gboolean expand = FALSE;
  gboolean accelerators = FALSE;
  gboolean always_show_image_set = FALSE, always_show_image = FALSE;

  gboolean raise_error = TRUE;

  node_name = NULL;
  action = NULL;
  action_quark = 0;
  top = FALSE;

  for (i = 0; attribute_names[i] != NULL; i++)
    {
      if (!strcmp (attribute_names[i], "name"))
	{
	  node_name = attribute_values[i];
	}
      else if (!strcmp (attribute_names[i], "action"))
	{
	  action = attribute_values[i];
	  action_quark = g_quark_from_string (attribute_values[i]);
	}
      else if (!strcmp (attribute_names[i], "position"))
	{
	  top = !strcmp (attribute_values[i], "top");
	}
      else if (!strcmp (attribute_names[i], "expand"))
	{
	  expand = !strcmp (attribute_values[i], "true");
	}
      else if (!strcmp (attribute_names[i], "accelerators"))
        {
          accelerators = !strcmp (attribute_values[i], "true");
        }
      else if (!strcmp (attribute_names[i], "always-show-image"))
        {
          always_show_image_set = TRUE;
          always_show_image = !strcmp (attribute_values[i], "true");
        }
      /*  else silently skip unknown attributes to be compatible with
       *  future additional attributes.
       */
    }

  /* Work out a name for this node.  Either the name attribute, or
   * the action, or the element name */
  if (node_name == NULL) 
    {
      if (action != NULL)
	node_name = action;
      else 
	node_name = element_name;
    }

  switch (element_name[0])
    {
    case 'a':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "accelerator"))
	{
	  ctx->state = STATE_ACCELERATOR;
	  ctx->current = get_child_node (self, ctx->current, NULL,
					 node_name, strlen (node_name),
					 NODE_TYPE_ACCELERATOR,
					 TRUE, FALSE);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;

	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);

	  raise_error = FALSE;
	}
      break;
    case 'u':
      if (ctx->state == STATE_START && !strcmp (element_name, "ui"))
	{
	  ctx->state = STATE_ROOT;
	  ctx->current = self->private_data->root_node;
	  raise_error = FALSE;

	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	}
      break;
    case 'm':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "menubar"))
	{
	  ctx->state = STATE_MENU;
	  ctx->current = get_child_node (self, ctx->current, NULL,
					 node_name, strlen (node_name),
					 NODE_TYPE_MENUBAR,
					 TRUE, FALSE);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;

	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	  mark_node_dirty (ctx->current);

	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_MENU && !strcmp (element_name, "menu"))
	{
	  ctx->current = get_child_node (self, ctx->current, NULL,
					 node_name, strlen (node_name),
					 NODE_TYPE_MENU,
					 TRUE, top);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;

	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_TOOLITEM && !strcmp (element_name, "menu"))
	{
	  ctx->state = STATE_MENU;
	  
	  ctx->current = get_child_node (self, g_node_last_child (ctx->current), NULL,
					 node_name, strlen (node_name),
					 NODE_TYPE_MENU,
					 TRUE, top);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;

	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_MENU && !strcmp (element_name, "menuitem"))
	{
	  GNode *node;

	  ctx->state = STATE_MENUITEM;
	  node = get_child_node (self, ctx->current, NULL,
				 node_name, strlen (node_name),
				 NODE_TYPE_MENUITEM,
				 TRUE, top);
	  if (NODE_INFO (node)->action_name == 0)
	    NODE_INFO (node)->action_name = action_quark;

	  NODE_INFO (node)->always_show_image_set = always_show_image_set;
	  NODE_INFO (node)->always_show_image = always_show_image;

	  node_prepend_ui_reference (node, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      break;
    case 'p':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "popup"))
	{
	  ctx->state = STATE_MENU;
	  ctx->current = get_child_node (self, ctx->current, NULL,
					 node_name, strlen (node_name),
					 NODE_TYPE_POPUP,
					 TRUE, FALSE);

          NODE_INFO (ctx->current)->popup_accels = accelerators;

	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;
	  
	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      else if ((ctx->state == STATE_MENU || ctx->state == STATE_TOOLBAR) &&
	       !strcmp (element_name, "placeholder"))
	{
	  if (ctx->state == STATE_TOOLBAR)
	    ctx->current = get_child_node (self, ctx->current, NULL,
					   node_name, strlen (node_name),
					   NODE_TYPE_TOOLBAR_PLACEHOLDER,
					   TRUE, top);
	  else
	    ctx->current = get_child_node (self, ctx->current, NULL,
					   node_name, strlen (node_name),
					   NODE_TYPE_MENU_PLACEHOLDER,
					   TRUE, top);
	  
	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      break;
    case 's':
      if ((ctx->state == STATE_MENU || ctx->state == STATE_TOOLBAR) &&
	  !strcmp (element_name, "separator"))
	{
	  GNode *node;
	  gint length;

	  if (ctx->state == STATE_TOOLBAR)
	    ctx->state = STATE_TOOLITEM;
	  else
	    ctx->state = STATE_MENUITEM;
	  if (!strcmp (node_name, "separator"))
	    {
	      node_name = NULL;
	      length = 0;
	    }
	  else
	    length = strlen (node_name);
	  node = get_child_node (self, ctx->current, NULL,
				 node_name, length,
				 NODE_TYPE_SEPARATOR,
				 TRUE, top);

	  NODE_INFO (node)->expand = expand;

	  if (NODE_INFO (node)->action_name == 0)
	    NODE_INFO (node)->action_name = action_quark;

	  node_prepend_ui_reference (node, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      break;
    case 't':
      if (ctx->state == STATE_ROOT && !strcmp (element_name, "toolbar"))
	{
	  ctx->state = STATE_TOOLBAR;
	  ctx->current = get_child_node (self, ctx->current, NULL,
					 node_name, strlen (node_name),
					 NODE_TYPE_TOOLBAR,
					 TRUE, FALSE);
	  if (NODE_INFO (ctx->current)->action_name == 0)
	    NODE_INFO (ctx->current)->action_name = action_quark;
	  
	  node_prepend_ui_reference (ctx->current, ctx->merge_id, action_quark);
	  
	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_TOOLBAR && !strcmp (element_name, "toolitem"))
	{
	  GNode *node;

	  ctx->state = STATE_TOOLITEM;
	  node = get_child_node (self, ctx->current, NULL,
				node_name, strlen (node_name),
				 NODE_TYPE_TOOLITEM,
				 TRUE, top);
	  if (NODE_INFO (node)->action_name == 0)
	    NODE_INFO (node)->action_name = action_quark;
	  
	  node_prepend_ui_reference (node, ctx->merge_id, action_quark);

	  raise_error = FALSE;
	}
      break;
    default:
      break;
    }
  if (raise_error)
    {
      gint line_number, char_number;
 
      g_markup_parse_context_get_position (context,
					   &line_number, &char_number);
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		   _("Unexpected start tag '%s' on line %d char %d"),
		   element_name,
		   line_number, char_number);
    }
}

static void
end_element_handler (GMarkupParseContext *context,
		     const gchar         *element_name,
		     gpointer             user_data,
		     GError             **error)
{
  ParseContext *ctx = user_data;

  switch (ctx->state)
    {
    case STATE_START:
    case STATE_END:
      /* no need to GError here, GMarkup already catches this */
      break;
    case STATE_ROOT:
      ctx->current = NULL;
      ctx->state = STATE_END;
      break;
    case STATE_MENU:
    case STATE_TOOLBAR:
    case STATE_ACCELERATOR:
      ctx->current = ctx->current->parent;
      if (NODE_INFO (ctx->current)->type == NODE_TYPE_ROOT) 
	ctx->state = STATE_ROOT;
      else if (NODE_INFO (ctx->current)->type == NODE_TYPE_TOOLITEM)
	{
	  ctx->current = ctx->current->parent;
	  ctx->state = STATE_TOOLITEM;
	}
      /* else, stay in same state */
      break;
    case STATE_MENUITEM:
      ctx->state = STATE_MENU;
      break;
    case STATE_TOOLITEM:
      ctx->state = STATE_TOOLBAR;
      break;
    }
}

static void
cleanup (GMarkupParseContext *context,
	 GError              *error,
	 gpointer             user_data)
{
  ParseContext *ctx = user_data;

  ctx->current = NULL;
  /* should also walk through the tree and get rid of nodes related to
   * this UI file's tag */

  gtk_ui_manager_remove_ui (ctx->self, ctx->merge_id);
}

static gboolean
xml_isspace (char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void 
text_handler (GMarkupParseContext *context,
	      const gchar         *text,
	      gsize                text_len,  
	      gpointer             user_data,
	      GError             **error)
{
  const gchar *p;
  const gchar *end;

  p = text;
  end = text + text_len;
  while (p != end && xml_isspace (*p))
    ++p;
  
  if (p != end)
    {
      gint line_number, char_number;
      
      g_markup_parse_context_get_position (context,
					   &line_number, &char_number);
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_INVALID_CONTENT,
		   _("Unexpected character data on line %d char %d"),
		   line_number, char_number);
    }
}


static const GMarkupParser ui_parser = {
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  cleanup
};

static guint
add_ui_from_string (GtkUIManager *self,
		    const gchar  *buffer, 
		    gssize        length,
		    gboolean      needs_root,
		    GError      **error)
{
  ParseContext ctx = { 0 };
  GMarkupParseContext *context;

  ctx.state = STATE_START;
  ctx.self = self;
  ctx.current = NULL;
  ctx.merge_id = gtk_ui_manager_new_merge_id (self);

  context = g_markup_parse_context_new (&ui_parser, 0, &ctx, NULL);

  if (needs_root)
    if (!g_markup_parse_context_parse (context, "<ui>", -1, error))
      goto out;

  if (!g_markup_parse_context_parse (context, buffer, length, error))
    goto out;

  if (needs_root)
    if (!g_markup_parse_context_parse (context, "</ui>", -1, error))
      goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  g_markup_parse_context_free (context);

  queue_update (self);

  g_object_notify (G_OBJECT (self), "ui");

  return ctx.merge_id;

 out:

  g_markup_parse_context_free (context);

  return 0;
}

/**
 * gtk_ui_manager_add_ui_from_string:
 * @self: a #GtkUIManager object
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: return location for an error
 * 
 * Parses a string containing a <link linkend="XML-UI">UI definition</link> and 
 * merges it with the current contents of @self. An enclosing &lt;ui&gt; 
 * element is added if it is missing.
 * 
 * Return value: The merge id for the merged UI. The merge id can be used
 *   to unmerge the UI with gtk_ui_manager_remove_ui(). If an error occurred,
 *   the return value is 0.
 *
 * Since: 2.4
 **/
guint
gtk_ui_manager_add_ui_from_string (GtkUIManager *self,
				   const gchar  *buffer, 
				   gssize        length,
				   GError      **error)
{
  gboolean needs_root = TRUE;
  const gchar *p;
  const gchar *end;

  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), 0);
  g_return_val_if_fail (buffer != NULL, 0);

  if (length < 0)
    length = strlen (buffer);

  p = buffer;
  end = buffer + length;
  while (p != end && xml_isspace (*p))
    ++p;

  if (end - p >= 4 && strncmp (p, "<ui>", 4) == 0)
    needs_root = FALSE;
  
  return add_ui_from_string (self, buffer, length, needs_root, error);
}

/**
 * gtk_ui_manager_add_ui_from_file:
 * @self: a #GtkUIManager object
 * @filename: the name of the file to parse 
 * @error: return location for an error
 * 
 * Parses a file containing a <link linkend="XML-UI">UI definition</link> and 
 * merges it with the current contents of @self. 
 * 
 * Return value: The merge id for the merged UI. The merge id can be used
 *   to unmerge the UI with gtk_ui_manager_remove_ui(). If an error occurred,
 *   the return value is 0.
 *
 * Since: 2.4
 **/
guint
gtk_ui_manager_add_ui_from_file (GtkUIManager *self,
				 const gchar  *filename,
				 GError      **error)
{
  gchar *buffer;
  gsize length;
  guint res;

  g_return_val_if_fail (GTK_IS_UI_MANAGER (self), 0);

  if (!g_file_get_contents (filename, &buffer, &length, error))
    return 0;

  res = add_ui_from_string (self, buffer, length, FALSE, error);
  g_free (buffer);

  return res;
}

/**
 * gtk_ui_manager_add_ui:
 * @self: a #GtkUIManager
 * @merge_id: the merge id for the merged UI, see gtk_ui_manager_new_merge_id()
 * @path: a path
 * @name: the name for the added UI element
 * @action: (allow-none): the name of the action to be proxied, or %NULL to add a separator
 * @type: the type of UI element to add.
 * @top: if %TRUE, the UI element is added before its siblings, otherwise it
 *   is added after its siblings.
 *
 * Adds a UI element to the current contents of @self. 
 *
 * If @type is %GTK_UI_MANAGER_AUTO, GTK+ inserts a menuitem, toolitem or 
 * separator if such an element can be inserted at the place determined by 
 * @path. Otherwise @type must indicate an element that can be inserted at 
 * the place determined by @path.
 *
 * If @path points to a menuitem or toolitem, the new element will be inserted
 * before or after this item, depending on @top.
 * 
 * Since: 2.4
 **/
void
gtk_ui_manager_add_ui (GtkUIManager        *self,
		       guint                merge_id,
		       const gchar         *path,
		       const gchar         *name,
		       const gchar         *action,
		       GtkUIManagerItemType type,
		       gboolean             top)
{
  GNode *node;
  GNode *sibling;
  GNode *child;
  NodeType node_type;
  GQuark action_quark = 0;

  g_return_if_fail (GTK_IS_UI_MANAGER (self));  
  g_return_if_fail (merge_id > 0);
  g_return_if_fail (name != NULL || type == GTK_UI_MANAGER_SEPARATOR);

  node = get_node (self, path, NODE_TYPE_UNDECIDED, FALSE);
  sibling = NULL;

  if (node == NULL)
    return;

  node_type = NODE_TYPE_UNDECIDED;

 reswitch:
  switch (NODE_INFO (node)->type) 
    {
    case NODE_TYPE_SEPARATOR:
    case NODE_TYPE_MENUITEM:
    case NODE_TYPE_TOOLITEM:
      sibling = node;
      node = node->parent;
      goto reswitch;
    case NODE_TYPE_MENUBAR:
    case NODE_TYPE_MENU:
    case NODE_TYPE_POPUP:
    case NODE_TYPE_MENU_PLACEHOLDER:
      switch (type) 
	{
	case GTK_UI_MANAGER_AUTO:
	  if (action != NULL)
	      node_type = NODE_TYPE_MENUITEM;
	  else
	      node_type = NODE_TYPE_SEPARATOR;
	  break;
	case GTK_UI_MANAGER_MENU:
	  node_type = NODE_TYPE_MENU;
	  break;
	case GTK_UI_MANAGER_MENUITEM:
	  node_type = NODE_TYPE_MENUITEM;
	  break;
	case GTK_UI_MANAGER_SEPARATOR:
	  node_type = NODE_TYPE_SEPARATOR;
	  break;
	case GTK_UI_MANAGER_PLACEHOLDER:
	  node_type = NODE_TYPE_MENU_PLACEHOLDER;
	  break;
	default: ;
	  /* do nothing */
	}
      break;
    case NODE_TYPE_TOOLBAR:
    case NODE_TYPE_TOOLBAR_PLACEHOLDER:
      switch (type) 
	{
	case GTK_UI_MANAGER_AUTO:
	  if (action != NULL)
	      node_type = NODE_TYPE_TOOLITEM;
	  else
	      node_type = NODE_TYPE_SEPARATOR;
	  break;
	case GTK_UI_MANAGER_TOOLITEM:
	  node_type = NODE_TYPE_TOOLITEM;
	  break;
	case GTK_UI_MANAGER_SEPARATOR:
	  node_type = NODE_TYPE_SEPARATOR;
	  break;
	case GTK_UI_MANAGER_PLACEHOLDER:
	  node_type = NODE_TYPE_TOOLBAR_PLACEHOLDER;
	  break;
	default: ;
	  /* do nothing */
	}
      break;
    case NODE_TYPE_ROOT:
      switch (type) 
	{
	case GTK_UI_MANAGER_MENUBAR:
	  node_type = NODE_TYPE_MENUBAR;
	  break;
	case GTK_UI_MANAGER_TOOLBAR:
	  node_type = NODE_TYPE_TOOLBAR;
	  break;
	case GTK_UI_MANAGER_POPUP:
	case GTK_UI_MANAGER_POPUP_WITH_ACCELS:
	  node_type = NODE_TYPE_POPUP;
	  break;
	case GTK_UI_MANAGER_ACCELERATOR:
	  node_type = NODE_TYPE_ACCELERATOR;
	  break;
	default: ;
	  /* do nothing */
	}
      break;
    default: ;
      /* do nothing */
    }

  if (node_type == NODE_TYPE_UNDECIDED)
    {
      g_warning ("item type %d not suitable for adding at '%s'", 
		 type, path);
      return;
    }
   
  child = get_child_node (self, node, sibling,
			  name, name ? strlen (name) : 0,
			  node_type, TRUE, top);

  if (type == GTK_UI_MANAGER_POPUP_WITH_ACCELS)
    NODE_INFO (child)->popup_accels = TRUE;

  if (action != NULL)
    action_quark = g_quark_from_string (action);

  node_prepend_ui_reference (child, merge_id, action_quark);

  if (NODE_INFO (child)->action_name == 0)
    NODE_INFO (child)->action_name = action_quark;

  queue_update (self);

  g_object_notify (G_OBJECT (self), "ui");      
}

static gboolean
remove_ui (GNode   *node, 
	   gpointer user_data)
{
  guint merge_id = GPOINTER_TO_UINT (user_data);

  node_remove_ui_reference (node, merge_id);

  return FALSE; /* continue */
}

/**
 * gtk_ui_manager_remove_ui:
 * @self: a #GtkUIManager object
 * @merge_id: a merge id as returned by gtk_ui_manager_add_ui_from_string()
 * 
 * Unmerges the part of @self<!-- -->s content identified by @merge_id.
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_remove_ui (GtkUIManager *self, 
			  guint         merge_id)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (self));

  g_node_traverse (self->private_data->root_node, 
		   G_POST_ORDER, G_TRAVERSE_ALL, -1,
		   remove_ui, GUINT_TO_POINTER (merge_id));

  queue_update (self);

  g_object_notify (G_OBJECT (self), "ui");      
}

/* -------------------- Updates -------------------- */


static GtkAction *
get_action_by_name (GtkUIManager *merge, 
		    const gchar  *action_name)
{
  GList *tmp;

  if (!action_name)
    return NULL;
  
  /* lookup name */
  for (tmp = merge->private_data->action_groups; tmp != NULL; tmp = tmp->next)
    {
      GtkActionGroup *action_group = tmp->data;
      GtkAction *action;
      
      action = gtk_action_group_get_action (action_group, action_name);

      if (action)
	return action;
    }

  return NULL;
}

static gboolean
find_menu_position (GNode      *node, 
		    GtkWidget **menushell_p, 
		    gint       *pos_p)
{
  GtkWidget *menushell;
  gint pos = 0;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (NODE_INFO (node)->type == NODE_TYPE_MENU ||
		        NODE_INFO (node)->type == NODE_TYPE_POPUP ||
		        NODE_INFO (node)->type == NODE_TYPE_MENU_PLACEHOLDER ||
		        NODE_INFO (node)->type == NODE_TYPE_MENUITEM ||
		        NODE_INFO (node)->type == NODE_TYPE_SEPARATOR,
                        FALSE);

  /* first sibling -- look at parent */
  if (node->prev == NULL)
    {
      GNode *parent;
      GList *siblings;

      parent = node->parent;
      switch (NODE_INFO (parent)->type)
	{
	case NODE_TYPE_MENUBAR:
	case NODE_TYPE_POPUP:
	  menushell = NODE_INFO (parent)->proxy;
	  pos = 0;
	  break;
	case NODE_TYPE_MENU:
	  menushell = NODE_INFO (parent)->proxy;
	  if (GTK_IS_MENU_ITEM (menushell))
	    menushell = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menushell));
	  siblings = gtk_container_get_children (GTK_CONTAINER (menushell));
	  if (siblings != NULL && GTK_IS_TEAROFF_MENU_ITEM (siblings->data))
	    pos = 1;
	  else
	    pos = 0;
	  g_list_free (siblings);
	  break;
	case NODE_TYPE_MENU_PLACEHOLDER:
	  menushell = gtk_widget_get_parent (NODE_INFO (parent)->proxy);
	  g_return_val_if_fail (GTK_IS_MENU_SHELL (menushell), FALSE);
	  pos = g_list_index (GTK_MENU_SHELL (menushell)->children,
			      NODE_INFO (parent)->proxy) + 1;
	  break;
	default:
	  g_warning ("%s: bad parent node type %d", G_STRLOC,
		     NODE_INFO (parent)->type);
	  return FALSE;
	}
    }
  else
    {
      GtkWidget *prev_child;
      GNode *sibling;

      sibling = node->prev;
      if (NODE_INFO (sibling)->type == NODE_TYPE_MENU_PLACEHOLDER)
	prev_child = NODE_INFO (sibling)->extra; /* second Separator */
      else
	prev_child = NODE_INFO (sibling)->proxy;

      if (!GTK_IS_WIDGET (prev_child))
        return FALSE;

      menushell = gtk_widget_get_parent (prev_child);
      if (!GTK_IS_MENU_SHELL (menushell))
        return FALSE;

      pos = g_list_index (GTK_MENU_SHELL (menushell)->children, prev_child) + 1;
    }

  if (menushell_p)
    *menushell_p = menushell;
  if (pos_p)
    *pos_p = pos;

  return TRUE;
}

static gboolean
find_toolbar_position (GNode      *node, 
		       GtkWidget **toolbar_p, 
		       gint       *pos_p)
{
  GtkWidget *toolbar;
  gint pos;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (NODE_INFO (node)->type == NODE_TYPE_TOOLBAR ||
		        NODE_INFO (node)->type == NODE_TYPE_TOOLBAR_PLACEHOLDER ||
		        NODE_INFO (node)->type == NODE_TYPE_TOOLITEM ||
		        NODE_INFO (node)->type == NODE_TYPE_SEPARATOR,
                        FALSE);
  
  /* first sibling -- look at parent */
  if (node->prev == NULL)
    {
      GNode *parent;

      parent = node->parent;
      switch (NODE_INFO (parent)->type)
	{
	case NODE_TYPE_TOOLBAR:
	  toolbar = NODE_INFO (parent)->proxy;
	  pos = 0;
	  break;
	case NODE_TYPE_TOOLBAR_PLACEHOLDER:
	  toolbar = gtk_widget_get_parent (NODE_INFO (parent)->proxy);
	  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);
	  pos = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar),
					    GTK_TOOL_ITEM (NODE_INFO (parent)->proxy)) + 1;
	  break;
	default:
	  g_warning ("%s: bad parent node type %d", G_STRLOC,
		     NODE_INFO (parent)->type);
	  return FALSE;
	}
    }
  else
    {
      GtkWidget *prev_child;
      GNode *sibling;

      sibling = node->prev;
      if (NODE_INFO (sibling)->type == NODE_TYPE_TOOLBAR_PLACEHOLDER)
	prev_child = NODE_INFO (sibling)->extra; /* second Separator */
      else
	prev_child = NODE_INFO (sibling)->proxy;

      if (!GTK_IS_WIDGET (prev_child))
        return FALSE;

      toolbar = gtk_widget_get_parent (prev_child);
      if (!GTK_IS_TOOLBAR (toolbar))
        return FALSE;

      pos = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar),
					GTK_TOOL_ITEM (prev_child)) + 1;
    }
  
  if (toolbar_p)
    *toolbar_p = toolbar;
  if (pos_p)
    *pos_p = pos;

  return TRUE;
}

/**
 * _gtk_menu_is_empty:
 * @menu: (allow-none): a #GtkMenu or %NULL
 * 
 * Determines whether @menu is empty. A menu is considered empty if it
 * the only visible children are tearoff menu items or "filler" menu 
 * items which were inserted to mark the menu as empty.
 * 
 * This function is used by #GtkAction.
 *
 * Return value: whether @menu is empty.
 **/
gboolean
_gtk_menu_is_empty (GtkWidget *menu)
{
  GList *children, *cur;
  gboolean result = TRUE;

  g_return_val_if_fail (menu == NULL || GTK_IS_MENU (menu), TRUE);

  if (!menu)
    return FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  cur = children;
  while (cur) 
    {
      if (gtk_widget_get_visible (cur->data))
	{
	  if (!GTK_IS_TEAROFF_MENU_ITEM (cur->data) &&
	      !g_object_get_data (cur->data, "gtk-empty-menu-item"))
            {
	      result = FALSE;
              break;
            }
	}
      cur = cur->next;
    }
  g_list_free (children);

  return result;
}

enum {
  SEPARATOR_MODE_SMART,
  SEPARATOR_MODE_VISIBLE,
  SEPARATOR_MODE_HIDDEN
};

static void
update_smart_separators (GtkWidget *proxy)
{
  GtkWidget *parent = NULL;
  
  if (GTK_IS_MENU (proxy) || GTK_IS_TOOLBAR (proxy))
    parent = proxy;
  else if (GTK_IS_MENU_ITEM (proxy) || GTK_IS_TOOL_ITEM (proxy))
    parent = gtk_widget_get_parent (proxy);

  if (parent) 
    {
      gboolean visible;
      gboolean empty;
      GList *children, *cur, *last;
      GtkWidget *filler;

      children = gtk_container_get_children (GTK_CONTAINER (parent));
      
      visible = FALSE;
      last = NULL;
      empty = TRUE;
      filler = NULL;

      cur = children;
      while (cur) 
	{
	  if (g_object_get_data (cur->data, "gtk-empty-menu-item"))
	    {
	      filler = cur->data;
	    }
	  else if (GTK_IS_SEPARATOR_MENU_ITEM (cur->data) ||
		   GTK_IS_SEPARATOR_TOOL_ITEM (cur->data))
	    {
	      gint mode = 
		GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cur->data), 
						    "gtk-separator-mode"));
	      switch (mode) 
		{
		case SEPARATOR_MODE_VISIBLE:
		  gtk_widget_show (GTK_WIDGET (cur->data));
		  last = NULL;
		  visible = FALSE;
		  break;
		case SEPARATOR_MODE_HIDDEN:
		  gtk_widget_hide (GTK_WIDGET (cur->data));
		  break;
		case SEPARATOR_MODE_SMART:
		  if (visible)
		    {
		      gtk_widget_show (GTK_WIDGET (cur->data));
		      last = cur;
		      visible = FALSE;
		    }
		  else 
		    gtk_widget_hide (GTK_WIDGET (cur->data));
		  break;
		}
	    }
	  else if (gtk_widget_get_visible (cur->data))
	    {
	      last = NULL;
	      if (GTK_IS_TEAROFF_MENU_ITEM (cur->data) || cur->data == filler)
		visible = FALSE;
	      else 
		{
		  visible = TRUE;
		  empty = FALSE;
		}
	    }
	  
	  cur = cur->next;
	}

      if (last)
	gtk_widget_hide (GTK_WIDGET (last->data));

      if (GTK_IS_MENU (parent)) 
	{
	  GtkWidget *item;

	  item = gtk_menu_get_attach_widget (GTK_MENU (parent));
	  if (GTK_IS_MENU_ITEM (item))
	    _gtk_action_sync_menu_visible (NULL, item, empty);
	  if (GTK_IS_WIDGET (filler))
	    {
	      if (empty)
		gtk_widget_show (filler);
	      else
		gtk_widget_hide (filler);
	    }
	}

      g_list_free (children);
    }
}

static void
update_node (GtkUIManager *self, 
	     GNode        *node,
	     gboolean      in_popup,
             gboolean      popup_accels)
{
  Node *info;
  GNode *child;
  GtkAction *action;
  const gchar *action_name;
  NodeUIReference *ref;
  
#ifdef DEBUG_UI_MANAGER
  GList *tmp;
#endif

  g_return_if_fail (node != NULL);
  g_return_if_fail (NODE_INFO (node) != NULL);

  info = NODE_INFO (node);
  
  if (!info->dirty)
    return;

  if (info->type == NODE_TYPE_POPUP)
    {
      in_popup = TRUE;
      popup_accels = info->popup_accels;
    }

#ifdef DEBUG_UI_MANAGER
  g_print ("update_node name=%s dirty=%d popup %d (", 
	   info->name, info->dirty, in_popup);
  for (tmp = info->uifiles; tmp != NULL; tmp = tmp->next)
    {
      NodeUIReference *ref = tmp->data;
      g_print("%s:%u", g_quark_to_string (ref->action_quark), ref->merge_id);
      if (tmp->next)
	g_print (", ");
    }
  g_print (")\n");
#endif

  if (info->uifiles == NULL) {
    /* We may need to remove this node.
     * This must be done in post order
     */
    goto recurse_children;
  }
  
  ref = info->uifiles->data;
  action_name = g_quark_to_string (ref->action_quark);
  action = get_action_by_name (self, action_name);
  
  info->dirty = FALSE;
  
  /* Check if the node doesn't have an action and must have an action */
  if (action == NULL &&
      info->type != NODE_TYPE_ROOT &&
      info->type != NODE_TYPE_MENUBAR &&
      info->type != NODE_TYPE_TOOLBAR &&
      info->type != NODE_TYPE_POPUP &&
      info->type != NODE_TYPE_SEPARATOR &&
      info->type != NODE_TYPE_MENU_PLACEHOLDER &&
      info->type != NODE_TYPE_TOOLBAR_PLACEHOLDER)
    {
      g_warning ("%s: missing action %s", info->name, action_name);
      
      return;
    }
  
  if (action)
    gtk_action_set_accel_group (action, self->private_data->accel_group);
  
  /* If the widget already has a proxy and the action hasn't changed, then
   * we only have to update the tearoff menu items.
   */
  if (info->proxy != NULL && action == info->action)
    {
      if (info->type == NODE_TYPE_MENU) 
	{
	  GtkWidget *menu;
	  GList *siblings;
	  
	  if (GTK_IS_MENU (info->proxy))
	    menu = info->proxy;
	  else
	    menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy));
	  siblings = gtk_container_get_children (GTK_CONTAINER (menu));
	  if (siblings != NULL && GTK_IS_TEAROFF_MENU_ITEM (siblings->data))
	    {
	      if (self->private_data->add_tearoffs && !in_popup)
		gtk_widget_show (GTK_WIDGET (siblings->data));
	      else
		gtk_widget_hide (GTK_WIDGET (siblings->data));
	    }
	  g_list_free (siblings);
	}
      
      goto recurse_children;
    }
  
  switch (info->type)
    {
    case NODE_TYPE_MENUBAR:
      if (info->proxy == NULL)
	{
	  info->proxy = gtk_menu_bar_new ();
	  g_object_ref_sink (info->proxy);
	  gtk_widget_set_name (info->proxy, info->name);
	  gtk_widget_show (info->proxy);
	  g_signal_emit (self, ui_manager_signals[ADD_WIDGET], 0, info->proxy);
	}
      break;
    case NODE_TYPE_POPUP:
      if (info->proxy == NULL) 
	{
	  info->proxy = gtk_menu_new ();
	  g_object_ref_sink (info->proxy);
	}
      gtk_widget_set_name (info->proxy, info->name);
      break;
    case NODE_TYPE_MENU:
      {
	GtkWidget *prev_submenu = NULL;
	GtkWidget *menu = NULL;
	GList *siblings;

	/* remove the proxy if it is of the wrong type ... */
	if (info->proxy &&  
	    G_OBJECT_TYPE (info->proxy) != GTK_ACTION_GET_CLASS (action)->menu_item_type)
	  {
	    if (GTK_IS_MENU_ITEM (info->proxy))
	      {
		prev_submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy));
		if (prev_submenu)
		  {
		    g_object_ref (prev_submenu);
		    gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy), NULL);
		}
	      }

            gtk_activatable_set_related_action (GTK_ACTIVATABLE (info->proxy), NULL);
	    gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				  info->proxy);
	    g_object_unref (info->proxy);
	    info->proxy = NULL;
	  }

	/* create proxy if needed ... */
	if (info->proxy == NULL)
	  {
            /* ... if the action already provides a menu, then use
             * that menu instead of creating an empty one
             */
            if ((NODE_INFO (node->parent)->type == NODE_TYPE_TOOLITEM ||
                 NODE_INFO (node->parent)->type == NODE_TYPE_MENUITEM) &&
                GTK_ACTION_GET_CLASS (action)->create_menu)
              {
                menu = gtk_action_create_menu (action);
              }

            if (!menu)
              {
                GtkWidget *tearoff;
                GtkWidget *filler;
	    
                menu = gtk_menu_new ();
                gtk_widget_set_name (menu, info->name);
                tearoff = gtk_tearoff_menu_item_new ();
                gtk_widget_set_no_show_all (tearoff, TRUE);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), tearoff);
                filler = gtk_menu_item_new_with_label (_("Empty"));
                g_object_set_data (G_OBJECT (filler),
                                   I_("gtk-empty-menu-item"),
                                   GINT_TO_POINTER (TRUE));
                gtk_widget_set_sensitive (filler, FALSE);
                gtk_widget_set_no_show_all (filler, TRUE);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), filler);
              }
	    
            if (NODE_INFO (node->parent)->type == NODE_TYPE_TOOLITEM)
	      {
		info->proxy = menu;
		g_object_ref_sink (info->proxy);
		gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (NODE_INFO (node->parent)->proxy),
					       menu);
	      }
	    else
	      {
		GtkWidget *menushell;
		gint pos;
		
		if (find_menu_position (node, &menushell, &pos))
                  {
		     info->proxy = gtk_action_create_menu_item (action);
		     g_object_ref_sink (info->proxy);
		     g_signal_connect (info->proxy, "notify::visible",
		   		       G_CALLBACK (update_smart_separators), NULL);
		     gtk_widget_set_name (info->proxy, info->name);
		
		     gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy), menu);
		     gtk_menu_shell_insert (GTK_MENU_SHELL (menushell), info->proxy, pos);
                 }
	      }
	  }
	else
          gtk_activatable_set_related_action (GTK_ACTIVATABLE (info->proxy), action);
	
	if (prev_submenu)
	  {
	    gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy),
				       prev_submenu);
	    g_object_unref (prev_submenu);
	  }
	
	if (GTK_IS_MENU (info->proxy))
	  menu = info->proxy;
	else
	  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy));

	siblings = gtk_container_get_children (GTK_CONTAINER (menu));
	if (siblings != NULL && GTK_IS_TEAROFF_MENU_ITEM (siblings->data))
	  {
	    if (self->private_data->add_tearoffs && !in_popup)
	      gtk_widget_show (GTK_WIDGET (siblings->data));
	    else
	      gtk_widget_hide (GTK_WIDGET (siblings->data));
	  }
	g_list_free (siblings);
      }
      break;
    case NODE_TYPE_UNDECIDED:
      g_warning ("found undecided node!");
      break;
    case NODE_TYPE_ROOT:
      break;
    case NODE_TYPE_TOOLBAR:
      if (info->proxy == NULL)
	{
	  info->proxy = gtk_toolbar_new ();
	  g_object_ref_sink (info->proxy);
	  gtk_widget_set_name (info->proxy, info->name);
	  gtk_widget_show (info->proxy);
	  g_signal_emit (self, ui_manager_signals[ADD_WIDGET], 0, info->proxy);
	}
      break;
    case NODE_TYPE_MENU_PLACEHOLDER:
      /* create menu items for placeholders if necessary ... */
      if (!GTK_IS_SEPARATOR_MENU_ITEM (info->proxy) ||
	  !GTK_IS_SEPARATOR_MENU_ITEM (info->extra))
	{
	  if (info->proxy)
	    {
	      gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				    info->proxy);
	      g_object_unref (info->proxy);
	      info->proxy = NULL;
	    }
	  if (info->extra)
	    {
	      gtk_container_remove (GTK_CONTAINER (info->extra->parent),
				    info->extra);
	      g_object_unref (info->extra);
	      info->extra = NULL;
	    }
	}
      if (info->proxy == NULL)
	{
	  GtkWidget *menushell;
	  gint pos;
	  
	  if (find_menu_position (node, &menushell, &pos))
            {
	      info->proxy = gtk_separator_menu_item_new ();
	      g_object_ref_sink (info->proxy);
	      g_object_set_data (G_OBJECT (info->proxy),
	  		         I_("gtk-separator-mode"),
			         GINT_TO_POINTER (SEPARATOR_MODE_HIDDEN));
	      gtk_widget_set_no_show_all (info->proxy, TRUE);
	      gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
	 			     NODE_INFO (node)->proxy, pos);
	  
	      info->extra = gtk_separator_menu_item_new ();
	      g_object_ref_sink (info->extra);
	      g_object_set_data (G_OBJECT (info->extra),
			         I_("gtk-separator-mode"),
			         GINT_TO_POINTER (SEPARATOR_MODE_HIDDEN));
	      gtk_widget_set_no_show_all (info->extra, TRUE);
	      gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
				     NODE_INFO (node)->extra, pos + 1);
            }
	}
      break;
    case NODE_TYPE_TOOLBAR_PLACEHOLDER:
      /* create toolbar items for placeholders if necessary ... */
      if (!GTK_IS_SEPARATOR_TOOL_ITEM (info->proxy) ||
	  !GTK_IS_SEPARATOR_TOOL_ITEM (info->extra))
	{
	  if (info->proxy)
	    {
	      gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				    info->proxy);
	      g_object_unref (info->proxy);
	      info->proxy = NULL;
	    }
	  if (info->extra)
	    {
	      gtk_container_remove (GTK_CONTAINER (info->extra->parent),
				    info->extra);
	      g_object_unref (info->extra);
	      info->extra = NULL;
	    }
	}
      if (info->proxy == NULL)
	{
	  GtkWidget *toolbar;
	  gint pos;
	  GtkToolItem *item;    
	  
	  if (find_toolbar_position (node, &toolbar, &pos))
            {
	      item = gtk_separator_tool_item_new ();
	      gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, pos);
	      info->proxy = GTK_WIDGET (item);
	      g_object_ref_sink (info->proxy);
	      g_object_set_data (G_OBJECT (info->proxy),
			         I_("gtk-separator-mode"),
			         GINT_TO_POINTER (SEPARATOR_MODE_HIDDEN));
	      gtk_widget_set_no_show_all (info->proxy, TRUE);
	  
	      item = gtk_separator_tool_item_new ();
	      gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, pos+1);
	      info->extra = GTK_WIDGET (item);
	      g_object_ref_sink (info->extra);
	      g_object_set_data (G_OBJECT (info->extra),
			         I_("gtk-separator-mode"),
			         GINT_TO_POINTER (SEPARATOR_MODE_HIDDEN));
	      gtk_widget_set_no_show_all (info->extra, TRUE);
            }
	}
      break;
    case NODE_TYPE_MENUITEM:
      /* remove the proxy if it is of the wrong type ... */
      if (info->proxy &&  
	  G_OBJECT_TYPE (info->proxy) != GTK_ACTION_GET_CLASS (action)->menu_item_type)
	{
	  g_signal_handlers_disconnect_by_func (info->proxy,
						G_CALLBACK (update_smart_separators),
						NULL);  
          gtk_activatable_set_related_action (GTK_ACTIVATABLE (info->proxy), NULL);
	  gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				info->proxy);
	  g_object_unref (info->proxy);
	  info->proxy = NULL;
	}
      /* create proxy if needed ... */
      if (info->proxy == NULL)
	{
	  GtkWidget *menushell;
	  gint pos;
	  
	  if (find_menu_position (node, &menushell, &pos))
            {
	      info->proxy = gtk_action_create_menu_item (action);
	      g_object_ref_sink (info->proxy);
	      gtk_widget_set_name (info->proxy, info->name);

              if (info->always_show_image_set &&
                  GTK_IS_IMAGE_MENU_ITEM (info->proxy))
                gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (info->proxy),
                                                           info->always_show_image);

	      gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
				     info->proxy, pos);
           }
	}
      else
	{
	  g_signal_handlers_disconnect_by_func (info->proxy,
						G_CALLBACK (update_smart_separators),
						NULL);
	  gtk_menu_item_set_submenu (GTK_MENU_ITEM (info->proxy), NULL);
          gtk_activatable_set_related_action (GTK_ACTIVATABLE (info->proxy), action);
	}

      if (info->proxy)
        {
          g_signal_connect (info->proxy, "notify::visible",
			    G_CALLBACK (update_smart_separators), NULL);
          if (in_popup && !popup_accels)
	    {
	      /* don't show accels in popups */
	      GtkWidget *child = gtk_bin_get_child (GTK_BIN (info->proxy));
	      if (GTK_IS_ACCEL_LABEL (child))
	        g_object_set (child, "accel-closure", NULL, NULL);
	    }
        }
      
      break;
    case NODE_TYPE_TOOLITEM:
      /* remove the proxy if it is of the wrong type ... */
      if (info->proxy && 
	  G_OBJECT_TYPE (info->proxy) != GTK_ACTION_GET_CLASS (action)->toolbar_item_type)
	{
	  g_signal_handlers_disconnect_by_func (info->proxy,
						G_CALLBACK (update_smart_separators),
						NULL);
          gtk_activatable_set_related_action (GTK_ACTIVATABLE (info->proxy), NULL);
	  gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				info->proxy);
	  g_object_unref (info->proxy);
	  info->proxy = NULL;
	}
      /* create proxy if needed ... */
      if (info->proxy == NULL)
	{
	  GtkWidget *toolbar;
	  gint pos;
	  
	  if (find_toolbar_position (node, &toolbar, &pos))
            {
	      info->proxy = gtk_action_create_tool_item (action);
	      g_object_ref_sink (info->proxy);
	      gtk_widget_set_name (info->proxy, info->name);
	      
	      gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
	  		          GTK_TOOL_ITEM (info->proxy), pos);
            }
	}
      else
	{
	  g_signal_handlers_disconnect_by_func (info->proxy,
						G_CALLBACK (update_smart_separators),
						NULL);
	  gtk_activatable_set_related_action (GTK_ACTIVATABLE (info->proxy), action);
	}

      if (info->proxy)
        {
          g_signal_connect (info->proxy, "notify::visible",
			    G_CALLBACK (update_smart_separators), NULL);
        }
      break;
    case NODE_TYPE_SEPARATOR:
      if (NODE_INFO (node->parent)->type == NODE_TYPE_TOOLBAR ||
	  NODE_INFO (node->parent)->type == NODE_TYPE_TOOLBAR_PLACEHOLDER)
	{
	  GtkWidget *toolbar;
	  gint pos;
	  gint separator_mode;
	  GtkToolItem *item;

	  if (GTK_IS_SEPARATOR_TOOL_ITEM (info->proxy))
	    {
	      gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				    info->proxy);
	      g_object_unref (info->proxy);
	      info->proxy = NULL;
	    }
	  
	  if (find_toolbar_position (node, &toolbar, &pos))
            {
	      item  = gtk_separator_tool_item_new ();
	      gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, pos);
	      info->proxy = GTK_WIDGET (item);
	      g_object_ref_sink (info->proxy);
	      gtk_widget_set_no_show_all (info->proxy, TRUE);
	      if (info->expand)
	        {
	          gtk_tool_item_set_expand (GTK_TOOL_ITEM (item), TRUE);
	          gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
	          separator_mode = SEPARATOR_MODE_VISIBLE;
	        }
	      else
	        separator_mode = SEPARATOR_MODE_SMART;
	  
	      g_object_set_data (G_OBJECT (info->proxy),
			         I_("gtk-separator-mode"),
			         GINT_TO_POINTER (separator_mode));
	      gtk_widget_show (info->proxy);
            }
	}
      else
	{
	  GtkWidget *menushell;
	  gint pos;
	  
	  if (GTK_IS_SEPARATOR_MENU_ITEM (info->proxy))
	    {
	      gtk_container_remove (GTK_CONTAINER (info->proxy->parent),
				    info->proxy);
	      g_object_unref (info->proxy);
	      info->proxy = NULL;
	    }
	  
	  if (find_menu_position (node, &menushell, &pos))
	    {
              info->proxy = gtk_separator_menu_item_new ();
	      g_object_ref_sink (info->proxy);
	      gtk_widget_set_no_show_all (info->proxy, TRUE);
	      g_object_set_data (G_OBJECT (info->proxy),
			         I_("gtk-separator-mode"),
			         GINT_TO_POINTER (SEPARATOR_MODE_SMART));
	      gtk_menu_shell_insert (GTK_MENU_SHELL (menushell),
				     info->proxy, pos);
	      gtk_widget_show (info->proxy);
            }
	}
      break;
    case NODE_TYPE_ACCELERATOR:
      gtk_action_connect_accelerator (action);
      break;
    }
  
  if (action)
    g_object_ref (action);
  if (info->action)
    g_object_unref (info->action);
  info->action = action;

 recurse_children:
  /* process children */
  child = node->children;
  while (child)
    {
      GNode *current;
      
      current = child;
      child = current->next;
      update_node (self, current, in_popup, popup_accels);
    }
  
  if (info->proxy) 
    {
      if (info->type == NODE_TYPE_MENU && GTK_IS_MENU_ITEM (info->proxy)) 
	update_smart_separators (gtk_menu_item_get_submenu (GTK_MENU_ITEM (info->proxy)));
      else if (info->type == NODE_TYPE_MENU || 
	       info->type == NODE_TYPE_TOOLBAR || 
	       info->type == NODE_TYPE_POPUP) 
	update_smart_separators (info->proxy);
    }
  
  /* handle cleanup of dead nodes */
  if (node->children == NULL && info->uifiles == NULL)
    {
      if (info->proxy)
	gtk_widget_destroy (info->proxy);
      if (info->extra)
	gtk_widget_destroy (info->extra);
      if (info->type == NODE_TYPE_ACCELERATOR && info->action != NULL)
	gtk_action_disconnect_accelerator (info->action);
      free_node (node);
      g_node_destroy (node);
    }
}

static gboolean
do_updates (GtkUIManager *self)
{
  /* this function needs to check through the tree for dirty nodes.
   * For such nodes, it needs to do the following:
   *
   * 1) check if they are referenced by any loaded UI files anymore.
   *    In which case, the proxy widget should be destroyed, unless
   *    there are any subnodes.
   *
   * 2) lookup the action for this node again.  If it is different to
   *    the current one (or if no previous action has been looked up),
   *    the proxy is reconnected to the new action (or a new proxy widget
   *    is created and added to the parent container).
   */
  update_node (self, self->private_data->root_node, FALSE, FALSE);

  self->private_data->update_tag = 0;

  return FALSE;
}

static gboolean
do_updates_idle (GtkUIManager *self)
{
  do_updates (self);

  return FALSE;
}

static void
queue_update (GtkUIManager *self)
{
  if (self->private_data->update_tag != 0)
    return;

  self->private_data->update_tag = gdk_threads_add_idle (
					       (GSourceFunc)do_updates_idle, 
					       self);
}


/**
 * gtk_ui_manager_ensure_update:
 * @self: a #GtkUIManager
 * 
 * Makes sure that all pending updates to the UI have been completed.
 *
 * This may occasionally be necessary, since #GtkUIManager updates the 
 * UI in an idle function. A typical example where this function is
 * useful is to enforce that the menubar and toolbar have been added to 
 * the main window before showing it:
 * |[
 * gtk_container_add (GTK_CONTAINER (window), vbox); 
 * g_signal_connect (merge, "add-widget", 
 *                   G_CALLBACK (add_widget), vbox);
 * gtk_ui_manager_add_ui_from_file (merge, "my-menus");
 * gtk_ui_manager_add_ui_from_file (merge, "my-toolbars");
 * gtk_ui_manager_ensure_update (merge);  
 * gtk_widget_show (window);
 * ]|
 *
 * Since: 2.4
 **/
void
gtk_ui_manager_ensure_update (GtkUIManager *self)
{
  if (self->private_data->update_tag != 0)
    {
      g_source_remove (self->private_data->update_tag);
      do_updates (self);
    }
}

static gboolean
dirty_traverse_func (GNode   *node,
		     gpointer data)
{
  NODE_INFO (node)->dirty = TRUE;
  return FALSE;
}

static void
dirty_all_nodes (GtkUIManager *self)
{
  g_node_traverse (self->private_data->root_node,
		   G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		   dirty_traverse_func, NULL);
  queue_update (self);
}

static void
mark_node_dirty (GNode *node)
{
  GNode *p;

  /* FIXME could optimize this */
  for (p = node; p; p = p->parent)
    NODE_INFO (p)->dirty = TRUE;  
}

static const gchar *
open_tag_format (NodeType type)
{
  switch (type)
    {
    case NODE_TYPE_UNDECIDED: return "%*s<UNDECIDED"; 
    case NODE_TYPE_ROOT: return "%*s<ui"; 
    case NODE_TYPE_MENUBAR: return "%*s<menubar";
    case NODE_TYPE_MENU: return "%*s<menu";
    case NODE_TYPE_TOOLBAR: return "%*s<toolbar";
    case NODE_TYPE_MENU_PLACEHOLDER:
    case NODE_TYPE_TOOLBAR_PLACEHOLDER: return "%*s<placeholder";
    case NODE_TYPE_POPUP: return "%*s<popup";
    case NODE_TYPE_MENUITEM: return "%*s<menuitem";
    case NODE_TYPE_TOOLITEM: return "%*s<toolitem";
    case NODE_TYPE_SEPARATOR: return "%*s<separator";
    case NODE_TYPE_ACCELERATOR: return "%*s<accelerator";
    default: return NULL;
    }
}

static const gchar *
close_tag_format (NodeType type)
{
  switch (type)
    {
    case NODE_TYPE_UNDECIDED: return "%*s</UNDECIDED>\n";
    case NODE_TYPE_ROOT: return "%*s</ui>\n";
    case NODE_TYPE_MENUBAR: return "%*s</menubar>\n";
    case NODE_TYPE_MENU: return "%*s</menu>\n";
    case NODE_TYPE_TOOLBAR: return "%*s</toolbar>\n";
    case NODE_TYPE_MENU_PLACEHOLDER:
    case NODE_TYPE_TOOLBAR_PLACEHOLDER: return "%*s</placeholder>\n";
    case NODE_TYPE_POPUP: return "%*s</popup>\n";
    default: return NULL;
    }
}

static void
print_node (GtkUIManager *self,
	    GNode        *node,
	    gint          indent_level,
	    GString      *buffer)
{
  Node  *mnode;
  GNode *child;
  const gchar *open_fmt;
  const gchar *close_fmt;

  mnode = node->data;

  open_fmt = open_tag_format (mnode->type);
  close_fmt = close_tag_format (mnode->type);

  g_string_append_printf (buffer, open_fmt, indent_level, "");

  if (mnode->type != NODE_TYPE_ROOT)
    {
      if (mnode->name)
	g_string_append_printf (buffer, " name=\"%s\"", mnode->name);
      
      if (mnode->action_name)
	g_string_append_printf (buffer, " action=\"%s\"",
				g_quark_to_string (mnode->action_name));
    }

  g_string_append (buffer, close_fmt ? ">\n" : "/>\n");

  for (child = node->children; child != NULL; child = child->next)
    print_node (self, child, indent_level + 2, buffer);

  if (close_fmt)
    g_string_append_printf (buffer, close_fmt, indent_level, "");
}

static gboolean
gtk_ui_manager_buildable_custom_tag_start (GtkBuildable  *buildable,
					   GtkBuilder    *builder,
					   GObject       *child,
					   const gchar   *tagname,
					   GMarkupParser *parser,
					   gpointer      *data)
{
  if (child)
    return FALSE;

  if (strcmp (tagname, "ui") == 0)
    {
      ParseContext *ctx;

      ctx = g_new0 (ParseContext, 1);
      ctx->state = STATE_START;
      ctx->self = GTK_UI_MANAGER (buildable);
      ctx->current = NULL;
      ctx->merge_id = gtk_ui_manager_new_merge_id (GTK_UI_MANAGER (buildable));

      *data = ctx;
      *parser = ui_parser;

      return TRUE;
    }

  return FALSE;

}

static void
gtk_ui_manager_buildable_custom_tag_end (GtkBuildable *buildable,
					 GtkBuilder   *builder,
					 GObject      *child,
					 const gchar  *tagname,
					 gpointer     *data)
{
  queue_update (GTK_UI_MANAGER (buildable));
  g_object_notify (G_OBJECT (buildable), "ui");
  g_free (data);
}

/**
 * gtk_ui_manager_get_ui:
 * @self: a #GtkUIManager
 * 
 * Creates a <link linkend="XML-UI">UI definition</link> of the merged UI.
 * 
 * Return value: A newly allocated string containing an XML representation of 
 * the merged UI.
 *
 * Since: 2.4
 **/
gchar *
gtk_ui_manager_get_ui (GtkUIManager *self)
{
  GString *buffer;

  buffer = g_string_new (NULL);

  gtk_ui_manager_ensure_update (self); 
 
  print_node (self, self->private_data->root_node, 0, buffer);  

  return g_string_free (buffer, FALSE);
}

#if defined (G_OS_WIN32) && !defined (_WIN64)

#undef gtk_ui_manager_add_ui_from_file

guint
gtk_ui_manager_add_ui_from_file (GtkUIManager *self,
				 const gchar  *filename,
				 GError      **error)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, error);
  guint retval;

  if (utf8_filename == NULL)
    return 0;
  
  retval = gtk_ui_manager_add_ui_from_file_utf8 (self, utf8_filename, error);

  g_free (utf8_filename);

  return retval;
}

#endif

#define __GTK_UI_MANAGER_C__
#include "gtkaliasdef.c"
