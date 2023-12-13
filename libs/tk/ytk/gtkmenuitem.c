/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#define GTK_MENU_INTERNALS

#include "config.h"
#include <string.h>

#include "gtkaccellabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkseparatormenuitem.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "gtkactivatable.h"
#include "gtkintl.h"
#include "gtkalias.h"


typedef struct {
  GtkAction *action;
  gboolean   use_action_appearance;
} GtkMenuItemPrivate;

enum {
  ACTIVATE,
  ACTIVATE_ITEM,
  TOGGLE_SIZE_REQUEST,
  TOGGLE_SIZE_ALLOCATE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_RIGHT_JUSTIFIED,
  PROP_SUBMENU,
  PROP_ACCEL_PATH,
  PROP_LABEL,
  PROP_USE_UNDERLINE,

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE
};


static void gtk_menu_item_dispose        (GObject          *object);
static void gtk_menu_item_set_property   (GObject          *object,
					  guint             prop_id,
					  const GValue     *value,
					  GParamSpec       *pspec);
static void gtk_menu_item_get_property   (GObject          *object,
					  guint             prop_id,
					  GValue           *value,
					  GParamSpec       *pspec);
static void gtk_menu_item_destroy        (GtkObject        *object);
static void gtk_menu_item_size_request   (GtkWidget        *widget,
					  GtkRequisition   *requisition);
static void gtk_menu_item_size_allocate  (GtkWidget        *widget,
					  GtkAllocation    *allocation);
static void gtk_menu_item_realize        (GtkWidget        *widget);
static void gtk_menu_item_unrealize      (GtkWidget        *widget);
static void gtk_menu_item_map            (GtkWidget        *widget);
static void gtk_menu_item_unmap          (GtkWidget        *widget);
static void gtk_menu_item_paint          (GtkWidget        *widget,
					  GdkRectangle     *area);
static gint gtk_menu_item_expose         (GtkWidget        *widget,
					  GdkEventExpose   *event);
static void gtk_menu_item_parent_set     (GtkWidget        *widget,
					  GtkWidget        *previous_parent);


static void gtk_real_menu_item_select               (GtkItem     *item);
static void gtk_real_menu_item_deselect             (GtkItem     *item);
static void gtk_real_menu_item_activate             (GtkMenuItem *item);
static void gtk_real_menu_item_activate_item        (GtkMenuItem *item);
static void gtk_real_menu_item_toggle_size_request  (GtkMenuItem *menu_item,
						     gint        *requisition);
static void gtk_real_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
						     gint         allocation);
static gboolean gtk_menu_item_mnemonic_activate     (GtkWidget   *widget,
						     gboolean     group_cycling);

static void gtk_menu_item_ensure_label   (GtkMenuItem      *menu_item);
static gint gtk_menu_item_popup_timeout  (gpointer          data);
static void gtk_menu_item_position_menu  (GtkMenu          *menu,
					  gint             *x,
					  gint             *y,
					  gboolean         *push_in,
					  gpointer          user_data);
static void gtk_menu_item_show_all       (GtkWidget        *widget);
static void gtk_menu_item_hide_all       (GtkWidget        *widget);
static void gtk_menu_item_forall         (GtkContainer    *container,
					  gboolean         include_internals,
					  GtkCallback      callback,
					  gpointer         callback_data);
static gboolean gtk_menu_item_can_activate_accel (GtkWidget *widget,
						  guint      signal_id);

static void gtk_real_menu_item_set_label (GtkMenuItem     *menu_item,
					  const gchar     *label);
static const gchar * gtk_real_menu_item_get_label (GtkMenuItem *menu_item);


static void gtk_menu_item_buildable_interface_init (GtkBuildableIface   *iface);
static void gtk_menu_item_buildable_add_child      (GtkBuildable        *buildable,
						    GtkBuilder          *builder,
						    GObject             *child,
						    const gchar         *type);
static void gtk_menu_item_buildable_custom_finished(GtkBuildable        *buildable,
						    GtkBuilder          *builder,
						    GObject             *child,
						    const gchar         *tagname,
						    gpointer             user_data);

static void gtk_menu_item_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_menu_item_update                     (GtkActivatable       *activatable,
						      GtkAction            *action,
						      const gchar          *property_name);
static void gtk_menu_item_sync_action_properties     (GtkActivatable       *activatable,
						      GtkAction            *action);
static void gtk_menu_item_set_related_action         (GtkMenuItem          *menu_item, 
						      GtkAction            *action);
static void gtk_menu_item_set_use_action_appearance  (GtkMenuItem          *menu_item, 
						      gboolean              use_appearance);


static guint menu_item_signals[LAST_SIGNAL] = { 0 };

static GtkBuildableIface *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkMenuItem, gtk_menu_item, GTK_TYPE_ITEM,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_menu_item_buildable_interface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_menu_item_activatable_interface_init))

#define GET_PRIVATE(object)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_MENU_ITEM, GtkMenuItemPrivate))

static void
gtk_menu_item_class_init (GtkMenuItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkItemClass *item_class = GTK_ITEM_CLASS (klass);

  gobject_class->dispose      = gtk_menu_item_dispose;
  gobject_class->set_property = gtk_menu_item_set_property;
  gobject_class->get_property = gtk_menu_item_get_property;

  object_class->destroy = gtk_menu_item_destroy;

  widget_class->size_request = gtk_menu_item_size_request;
  widget_class->size_allocate = gtk_menu_item_size_allocate;
  widget_class->expose_event = gtk_menu_item_expose;
  widget_class->realize = gtk_menu_item_realize;
  widget_class->unrealize = gtk_menu_item_unrealize;
  widget_class->map = gtk_menu_item_map;
  widget_class->unmap = gtk_menu_item_unmap;
  widget_class->show_all = gtk_menu_item_show_all;
  widget_class->hide_all = gtk_menu_item_hide_all;
  widget_class->mnemonic_activate = gtk_menu_item_mnemonic_activate;
  widget_class->parent_set = gtk_menu_item_parent_set;
  widget_class->can_activate_accel = gtk_menu_item_can_activate_accel;
  
  container_class->forall = gtk_menu_item_forall;

  item_class->select      = gtk_real_menu_item_select;
  item_class->deselect    = gtk_real_menu_item_deselect;

  klass->activate             = gtk_real_menu_item_activate;
  klass->activate_item        = gtk_real_menu_item_activate_item;
  klass->toggle_size_request  = gtk_real_menu_item_toggle_size_request;
  klass->toggle_size_allocate = gtk_real_menu_item_toggle_size_allocate;
  klass->set_label            = gtk_real_menu_item_set_label;
  klass->get_label            = gtk_real_menu_item_get_label;

  klass->hide_on_activate = TRUE;

  menu_item_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuItemClass, activate),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = menu_item_signals[ACTIVATE];

  menu_item_signals[ACTIVATE_ITEM] =
    g_signal_new (I_("activate-item"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkMenuItemClass, activate_item),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  menu_item_signals[TOGGLE_SIZE_REQUEST] =
    g_signal_new (I_("toggle-size-request"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkMenuItemClass, toggle_size_request),
		  NULL, NULL,
		  _gtk_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1,
		  G_TYPE_POINTER);

  menu_item_signals[TOGGLE_SIZE_ALLOCATE] =
    g_signal_new (I_("toggle-size-allocate"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
 		  G_STRUCT_OFFSET (GtkMenuItemClass, toggle_size_allocate),
		  NULL, NULL,
		  _gtk_marshal_VOID__INT,
		  G_TYPE_NONE, 1,
		  G_TYPE_INT);

  /**
   * GtkMenuItem:right-justified:
   *
   * Sets whether the menu item appears justified at the right side of a menu bar.
   *
   * Since: 2.14
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_RIGHT_JUSTIFIED,
                                   g_param_spec_boolean ("right-justified",
                                                         P_("Right Justified"),
                                                         P_("Sets whether the menu item appears justified at the right side of a menu bar"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkMenuItem:submenu:
   *
   * The submenu attached to the menu item, or NULL if it has none.
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SUBMENU,
                                   g_param_spec_object ("submenu",
                                                        P_("Submenu"),
                                                        P_("The submenu attached to the menu item, or NULL if it has none"),
                                                        GTK_TYPE_MENU,
                                                        GTK_PARAM_READWRITE));
  

  /**
   * GtkMenuItem:accel-path:
   *
   * Sets the accelerator path of the menu item, through which runtime
   * changes of the menu item's accelerator caused by the user can be
   * identified and saved to persistant storage.
   *
   * Since: 2.14
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_PATH,
                                   g_param_spec_string ("accel-path",
                                                        P_("Accel Path"),
                                                        P_("Sets the accelerator path of the menu item"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMenuItem:label:
   *
   * The text for the child label.
   *
   * Since: 2.16
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
							P_("Label"),
							P_("The text for the child label"),
							"",
							GTK_PARAM_READWRITE));

  /**
   * GtkMenuItem:use-underline:
   *
   * %TRUE if underlines in the text indicate mnemonics  
   *
   * Since: 2.16
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
							 P_("Use underline"),
							 P_("If set, an underline in the text indicates "
							    "the next character should be used for the "
							    "mnemonic accelerator key"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");

  gtk_widget_class_install_style_property_parser (widget_class,
						  g_param_spec_enum ("selected-shadow-type",
								     "Selected Shadow Type",
								     "Shadow type when item is selected",
								     GTK_TYPE_SHADOW_TYPE,
								     GTK_SHADOW_NONE,
								     GTK_PARAM_READABLE),
						  gtk_rc_property_parse_enum);

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("horizontal-padding",
							     "Horizontal Padding",
							     "Padding to left and right of the menu item",
							     0,
							     G_MAXINT,
							     3,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("toggle-spacing",
							     "Icon Spacing",
							     "Space between icon and label",
							     0,
							     G_MAXINT,
							     5,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("arrow-spacing",
							     "Arrow Spacing",
							     "Space between label and arrow",
							     0,
							     G_MAXINT,
							     10,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("Amount of space used up by arrow, relative to the menu item's font size"),
                                                               0.0, 2.0, 0.8,
                                                               GTK_PARAM_READABLE));

  /**
   * GtkMenuItem:width-chars:
   *
   * The minimum desired width of the menu item in characters.
   *
   * Since: 2.14
   **/
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("width-chars",
                                                             P_("Width in Characters"),
                                                             P_("The minimum desired width of the menu item in characters"),
                                                             0, G_MAXINT, 12,
                                                             GTK_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (GtkMenuItemPrivate));
}

static void
gtk_menu_item_init (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);

  gtk_widget_set_has_window (GTK_WIDGET (menu_item), FALSE);

  priv->action = NULL;
  priv->use_action_appearance = TRUE;
  
  menu_item->submenu = NULL;
  menu_item->toggle_size = 0;
  menu_item->accelerator_width = 0;
  menu_item->show_submenu_indicator = FALSE;
  if (gtk_widget_get_direction (GTK_WIDGET (menu_item)) == GTK_TEXT_DIR_RTL)
    menu_item->submenu_direction = GTK_DIRECTION_LEFT;
  else
    menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
  menu_item->submenu_placement = GTK_TOP_BOTTOM;
  menu_item->right_justify = FALSE;

  menu_item->timer = 0;
}

GtkWidget*
gtk_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_MENU_ITEM, NULL);
}

GtkWidget*
gtk_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_MENU_ITEM, 
		       "label", label,
		       NULL);
}


/**
 * gtk_menu_item_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkMenuItem
 *
 * Creates a new #GtkMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 **/
GtkWidget*
gtk_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_MENU_ITEM, 
		       "use-underline", TRUE,
		       "label", label,
		       NULL);
}

static void
gtk_menu_item_dispose (GObject *object)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);
  GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);

  if (priv->action)
    {
      gtk_action_disconnect_accelerator (priv->action);
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (menu_item), NULL);
      
      priv->action = NULL;
    }
  G_OBJECT_CLASS (gtk_menu_item_parent_class)->dispose (object);
}

static void 
gtk_menu_item_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_RIGHT_JUSTIFIED:
      gtk_menu_item_set_right_justified (menu_item, g_value_get_boolean (value));
      break;
    case PROP_SUBMENU:
      gtk_menu_item_set_submenu (menu_item, g_value_get_object (value));
      break;
    case PROP_ACCEL_PATH:
      gtk_menu_item_set_accel_path (menu_item, g_value_get_string (value));
      break;
    case PROP_LABEL:
      gtk_menu_item_set_label (menu_item, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_menu_item_set_use_underline (menu_item, g_value_get_boolean (value));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      gtk_menu_item_set_related_action (menu_item, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      gtk_menu_item_set_use_action_appearance (menu_item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_menu_item_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);
  GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);
  
  switch (prop_id)
    {
    case PROP_RIGHT_JUSTIFIED:
      g_value_set_boolean (value, gtk_menu_item_get_right_justified (menu_item));
      break;
    case PROP_SUBMENU:
      g_value_set_object (value, gtk_menu_item_get_submenu (menu_item));
      break;
    case PROP_ACCEL_PATH:
      g_value_set_string (value, gtk_menu_item_get_accel_path (menu_item));
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_menu_item_get_label (menu_item));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_menu_item_get_use_underline (menu_item));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, priv->action);
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, priv->use_action_appearance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_item_destroy (GtkObject *object)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (object);

  if (menu_item->submenu)
    gtk_widget_destroy (menu_item->submenu);

  GTK_OBJECT_CLASS (gtk_menu_item_parent_class)->destroy (object);
}

static void
gtk_menu_item_detacher (GtkWidget *widget,
			GtkMenu   *menu)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);

  g_return_if_fail (menu_item->submenu == (GtkWidget*) menu);

  menu_item->submenu = NULL;
}

static void
gtk_menu_item_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_menu_item_buildable_add_child;
  iface->custom_finished = gtk_menu_item_buildable_custom_finished;
}

static void 
gtk_menu_item_buildable_add_child (GtkBuildable *buildable,
				   GtkBuilder   *builder,
				   GObject      *child,
				   const gchar  *type)
{
  if (type && strcmp (type, "submenu") == 0)
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (buildable),
				   GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}


static void 
gtk_menu_item_buildable_custom_finished (GtkBuildable        *buildable,
					 GtkBuilder          *builder,
					 GObject             *child,
					 const gchar         *tagname,
					 gpointer             user_data)
{
  GtkWidget *toplevel;

  if (strcmp (tagname, "accelerator") == 0)
    {
      GtkMenuShell *menu_shell = (GtkMenuShell *) GTK_WIDGET (buildable)->parent;
      GtkWidget *attach;

      if (menu_shell)
	{
	  while (GTK_IS_MENU (menu_shell) &&
		 (attach = gtk_menu_get_attach_widget (GTK_MENU (menu_shell))) != NULL)
	    menu_shell = (GtkMenuShell *)attach->parent;
	  
	  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu_shell));
	}
      else
	{
	  /* Fall back to something ... */
	  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (buildable));

	  g_warning ("found a GtkMenuItem '%s' without a parent GtkMenuShell, assigned accelerators wont work.",
		     gtk_buildable_get_name (buildable));
	}

      /* Feed the correct toplevel to the GtkWidget accelerator parsing code */
      _gtk_widget_buildable_finish_accelerator (GTK_WIDGET (buildable), toplevel, user_data);
    }
  else
    parent_buildable_iface->custom_finished (buildable, builder, child, tagname, user_data);
}


static void
gtk_menu_item_activatable_interface_init (GtkActivatableIface *iface)
{
  iface->update = gtk_menu_item_update;
  iface->sync_action_properties = gtk_menu_item_sync_action_properties;
}

static void
activatable_update_label (GtkMenuItem *menu_item, GtkAction *action)
{
  GtkWidget *child = GTK_BIN (menu_item)->child;

  if (GTK_IS_LABEL (child))
    {
      const gchar *label;

      label = gtk_action_get_label (action);
      gtk_menu_item_set_label (menu_item, label);
    }
}

gboolean _gtk_menu_is_empty (GtkWidget *menu);

static void
gtk_menu_item_update (GtkActivatable *activatable,
		      GtkAction      *action,
		      const gchar    *property_name)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (activatable);
  GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);

  if (strcmp (property_name, "visible") == 0)
    _gtk_action_sync_menu_visible (action, GTK_WIDGET (menu_item), 
				   _gtk_menu_is_empty (gtk_menu_item_get_submenu (menu_item)));
  else if (strcmp (property_name, "sensitive") == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (menu_item), gtk_action_is_sensitive (action));
  else if (priv->use_action_appearance)
    {
      if (strcmp (property_name, "label") == 0)
	activatable_update_label (menu_item, action);
    }
}

static void
gtk_menu_item_sync_action_properties (GtkActivatable *activatable,
				      GtkAction      *action)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (activatable);
  GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);

  if (!priv->use_action_appearance || !action)
    {
      GtkWidget *label = GTK_BIN (menu_item)->child;

      label = GTK_BIN (menu_item)->child;

      if (GTK_IS_ACCEL_LABEL (label))
        gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), GTK_WIDGET (menu_item));
    }

  if (!action)
    return;

  _gtk_action_sync_menu_visible (action, GTK_WIDGET (menu_item),
				 _gtk_menu_is_empty (gtk_menu_item_get_submenu (menu_item)));

  gtk_widget_set_sensitive (GTK_WIDGET (menu_item), gtk_action_is_sensitive (action));

  if (priv->use_action_appearance)
    {
      GtkWidget *label = GTK_BIN (menu_item)->child;

      /* make sure label is a label */
      if (label && !GTK_IS_LABEL (label))
	{
	  gtk_container_remove (GTK_CONTAINER (menu_item), label);
	  label = NULL;
	}

      gtk_menu_item_ensure_label (menu_item);
      gtk_menu_item_set_use_underline (menu_item, TRUE);

      label = GTK_BIN (menu_item)->child;

      if (GTK_IS_ACCEL_LABEL (label) && gtk_action_get_accel_path (action))
        {
          gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), NULL);
          gtk_accel_label_set_accel_closure (GTK_ACCEL_LABEL (label),
                                             gtk_action_get_accel_closure (action));
        }

      activatable_update_label (menu_item, action);
    }
}

static void
gtk_menu_item_set_related_action (GtkMenuItem *menu_item,
				  GtkAction   *action)
{
    GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);

    if (priv->action == action)
      return;

    if (priv->action)
      {
	gtk_action_disconnect_accelerator (priv->action);
      }

    if (action)
      {
	const gchar *accel_path;
	
	accel_path = gtk_action_get_accel_path (action);
	if (accel_path)
	  {
	    gtk_action_connect_accelerator (action);
	    gtk_menu_item_set_accel_path (menu_item, accel_path);
	  }
      }

    gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (menu_item), action);

    priv->action = action;
}

static void
gtk_menu_item_set_use_action_appearance (GtkMenuItem *menu_item,
					 gboolean     use_appearance)
{
    GtkMenuItemPrivate *priv = GET_PRIVATE (menu_item);

    if (priv->use_action_appearance != use_appearance)
      {
	priv->use_action_appearance = use_appearance;

	gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (menu_item), priv->action);
      }
}


/**
 * gtk_menu_item_set_submenu:
 * @menu_item: a #GtkMenuItem
 * @submenu: (allow-none): the submenu, or %NULL
 *
 * Sets or replaces the menu item's submenu, or removes it when a %NULL
 * submenu is passed.
 **/
void
gtk_menu_item_set_submenu (GtkMenuItem *menu_item,
			   GtkWidget   *submenu)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (submenu == NULL || GTK_IS_MENU (submenu));
  
  if (menu_item->submenu != submenu)
    {
      if (menu_item->submenu)
	gtk_menu_detach (GTK_MENU (menu_item->submenu));

      if (submenu)
	{
	  menu_item->submenu = submenu;
	  gtk_menu_attach_to_widget (GTK_MENU (submenu),
				     GTK_WIDGET (menu_item),
				     gtk_menu_item_detacher);
	}

      if (GTK_WIDGET (menu_item)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (menu_item));

      g_object_notify (G_OBJECT (menu_item), "submenu");
    }
}

/**
 * gtk_menu_item_get_submenu:
 * @menu_item: a #GtkMenuItem
 *
 * Gets the submenu underneath this menu item, if any.
 * See gtk_menu_item_set_submenu().
 *
 * Return value: (transfer none): submenu for this menu item, or %NULL if none.
 **/
GtkWidget *
gtk_menu_item_get_submenu (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return menu_item->submenu;
}

/**
 * gtk_menu_item_remove_submenu:
 * @menu_item: a #GtkMenuItem
 *
 * Removes the widget's submenu.
 *
 * Deprecated: 2.12: gtk_menu_item_remove_submenu() is deprecated and
 *                   should not be used in newly written code. Use
 *                   gtk_menu_item_set_submenu() instead.
 **/
void
gtk_menu_item_remove_submenu (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_menu_item_set_submenu (menu_item, NULL);
}

void _gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
				   GtkSubmenuPlacement  placement);

void
_gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
			     GtkSubmenuPlacement  placement)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->submenu_placement = placement;
}

void
gtk_menu_item_select (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_item_select (GTK_ITEM (menu_item));

  /* Enable themeing of the parent menu item depending on whether
   * something is selected in its submenu
   */
  if (GTK_IS_MENU (GTK_WIDGET (menu_item)->parent))
    {
      GtkMenu *menu = GTK_MENU (GTK_WIDGET (menu_item)->parent);

      if (menu->parent_menu_item)
        gtk_widget_queue_draw (GTK_WIDGET (menu->parent_menu_item));
    }
}

void
gtk_menu_item_deselect (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_item_deselect (GTK_ITEM (menu_item));

  /* Enable themeing of the parent menu item depending on whether
   * something is selected in its submenu
   */
  if (GTK_IS_MENU (GTK_WIDGET (menu_item)->parent))
    {
      GtkMenu *menu = GTK_MENU (GTK_WIDGET (menu_item)->parent);

      if (menu->parent_menu_item)
        gtk_widget_queue_draw (GTK_WIDGET (menu->parent_menu_item));
    }
}

void
gtk_menu_item_activate (GtkMenuItem *menu_item)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[ACTIVATE], 0);
}

void
gtk_menu_item_toggle_size_request (GtkMenuItem *menu_item,
				   gint        *requisition)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[TOGGLE_SIZE_REQUEST], 0, requisition);
}

void
gtk_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
				    gint         allocation)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  g_signal_emit (menu_item, menu_item_signals[TOGGLE_SIZE_ALLOCATE], 0, allocation);
}

static void
gtk_menu_item_accel_width_foreach (GtkWidget *widget,
				   gpointer data)
{
  guint *width = data;

  if (GTK_IS_ACCEL_LABEL (widget))
    {
      guint w;

      w = gtk_accel_label_get_accel_width (GTK_ACCEL_LABEL (widget));
      *width = MAX (*width, w);
    }
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
			   gtk_menu_item_accel_width_foreach,
			   data);
}

static gint
get_minimum_width (GtkWidget *widget)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint width;
  gint width_chars;

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context,
				       widget->style->font_desc,
				       pango_context_get_language (context));

  width = pango_font_metrics_get_approximate_char_width (metrics);

  pango_font_metrics_unref (metrics);

  gtk_widget_style_get (widget, "width-chars", &width_chars, NULL);

  return PANGO_PIXELS (width_chars * width);
}

static void
gtk_menu_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkMenuItem *menu_item;
  GtkBin *bin;
  guint accel_width;
  guint horizontal_padding;
  GtkPackDirection pack_dir;
  GtkPackDirection child_pack_dir;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  gtk_widget_style_get (widget,
 			"horizontal-padding", &horizontal_padding,
			NULL);
  
  bin = GTK_BIN (widget);
  menu_item = GTK_MENU_ITEM (widget);

  if (GTK_IS_MENU_BAR (widget->parent))
    {
      pack_dir = gtk_menu_bar_get_pack_direction (GTK_MENU_BAR (widget->parent));
      child_pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (widget->parent));
    }
  else
    {
      pack_dir = GTK_PACK_DIRECTION_LTR;
      child_pack_dir = GTK_PACK_DIRECTION_LTR;
    }

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->xthickness) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->ythickness) * 2;

  if ((pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL) &&
      (child_pack_dir == GTK_PACK_DIRECTION_LTR || child_pack_dir == GTK_PACK_DIRECTION_RTL))
    requisition->width += 2 * horizontal_padding;
  else if ((pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT) &&
      (child_pack_dir == GTK_PACK_DIRECTION_TTB || child_pack_dir == GTK_PACK_DIRECTION_BTT))
    requisition->height += 2 * horizontal_padding;

  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;

      if (menu_item->submenu && menu_item->show_submenu_indicator)
	{
	  guint arrow_spacing;
	  
	  gtk_widget_style_get (widget,
				"arrow-spacing", &arrow_spacing,
				NULL);

	  requisition->width += child_requisition.height;
	  requisition->width += arrow_spacing;

	  requisition->width = MAX (requisition->width, get_minimum_width (widget));
	}
    }
  else /* separator item */
    {
      gboolean wide_separators;
      gint     separator_height;

      gtk_widget_style_get (widget,
                            "wide-separators",  &wide_separators,
                            "separator-height", &separator_height,
                            NULL);

      if (wide_separators)
        requisition->height += separator_height + widget->style->ythickness;
      else
        requisition->height += widget->style->ythickness * 2;
    }

  accel_width = 0;
  gtk_container_foreach (GTK_CONTAINER (menu_item),
			 gtk_menu_item_accel_width_foreach,
			 &accel_width);
  menu_item->accelerator_width = accel_width;
}

static void
gtk_menu_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkMenuItem *menu_item;
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkTextDirection direction;
  GtkPackDirection pack_dir;
  GtkPackDirection child_pack_dir;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  menu_item = GTK_MENU_ITEM (widget);
  bin = GTK_BIN (widget);
  
  direction = gtk_widget_get_direction (widget);

  if (GTK_IS_MENU_BAR (widget->parent))
    {
      pack_dir = gtk_menu_bar_get_pack_direction (GTK_MENU_BAR (widget->parent));
      child_pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (widget->parent));
    }
  else
    {
      pack_dir = GTK_PACK_DIRECTION_LTR;
      child_pack_dir = GTK_PACK_DIRECTION_LTR;
    }
    
  widget->allocation = *allocation;

  if (bin->child)
    {
      GtkRequisition child_requisition;
      guint horizontal_padding;

      gtk_widget_style_get (widget,
			    "horizontal-padding", &horizontal_padding,
			    NULL);

      child_allocation.x = GTK_CONTAINER (widget)->border_width + widget->style->xthickness;
      child_allocation.y = GTK_CONTAINER (widget)->border_width + widget->style->ythickness;

      if ((pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL) &&
	  (child_pack_dir == GTK_PACK_DIRECTION_LTR || child_pack_dir == GTK_PACK_DIRECTION_RTL))
	child_allocation.x += horizontal_padding;
      else if ((pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT) &&
	       (child_pack_dir == GTK_PACK_DIRECTION_TTB || child_pack_dir == GTK_PACK_DIRECTION_BTT))
	child_allocation.y += horizontal_padding;
      
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);

      if (child_pack_dir == GTK_PACK_DIRECTION_LTR ||
	  child_pack_dir == GTK_PACK_DIRECTION_RTL)
	{
	  if ((direction == GTK_TEXT_DIR_LTR) == (child_pack_dir != GTK_PACK_DIRECTION_RTL))
	    child_allocation.x += GTK_MENU_ITEM (widget)->toggle_size;
	  child_allocation.width -= GTK_MENU_ITEM (widget)->toggle_size;
	}
      else
	{
	  if ((direction == GTK_TEXT_DIR_LTR) == (child_pack_dir != GTK_PACK_DIRECTION_BTT))
	    child_allocation.y += GTK_MENU_ITEM (widget)->toggle_size;
	  child_allocation.height -= GTK_MENU_ITEM (widget)->toggle_size;
	}

      child_allocation.x += widget->allocation.x;
      child_allocation.y += widget->allocation.y;

      gtk_widget_get_child_requisition (bin->child, &child_requisition);
      if (menu_item->submenu && menu_item->show_submenu_indicator) 
	{
	  if (direction == GTK_TEXT_DIR_RTL)
	    child_allocation.x += child_requisition.height;
	  child_allocation.width -= child_requisition.height;
	}
      
      if (child_allocation.width < 1)
	child_allocation.width = 1;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (menu_item->event_window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  if (menu_item->submenu)
    gtk_menu_reposition (GTK_MENU (menu_item->submenu));
}

static void
gtk_menu_item_realize (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);
  
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_POINTER_MOTION_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;
  menu_item->event_window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (menu_item->event_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_menu_item_unrealize (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);

  gdk_window_set_user_data (menu_item->event_window, NULL);
  gdk_window_destroy (menu_item->event_window);
  menu_item->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->unrealize (widget);
}

static void
gtk_menu_item_map (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  
  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->map (widget);

  gdk_window_show (menu_item->event_window);
}

static void
gtk_menu_item_unmap (GtkWidget *widget)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
    
  gdk_window_hide (menu_item->event_window);

  GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->unmap (widget);
}

static void
gtk_menu_item_paint (GtkWidget    *widget,
		     GdkRectangle *area)
{
  GtkMenuItem *menu_item;
  GtkStateType state_type;
  GtkShadowType shadow_type, selected_shadow_type;
  gint width, height;
  gint x, y;
  gint border_width = GTK_CONTAINER (widget)->border_width;

  if (gtk_widget_is_drawable (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);

      state_type = widget->state;
      
      x = widget->allocation.x + border_width;
      y = widget->allocation.y + border_width;
      width = widget->allocation.width - border_width * 2;
      height = widget->allocation.height - border_width * 2;
      
      if ((state_type == GTK_STATE_PRELIGHT) &&
	  (GTK_BIN (menu_item)->child))
	{
	  gtk_widget_style_get (widget,
				"selected-shadow-type", &selected_shadow_type,
				NULL);
	  gtk_paint_box (widget->style,
			 widget->window,
			 GTK_STATE_PRELIGHT,
			 selected_shadow_type,
			 area, widget, "menuitem",
			 x, y, width, height);
	}
  
      if (menu_item->submenu && menu_item->show_submenu_indicator)
	{
	  gint arrow_x, arrow_y;
	  gint arrow_size;
	  gint arrow_extent;
	  guint horizontal_padding;
          gfloat arrow_scaling;
	  GtkTextDirection direction;
	  GtkArrowType arrow_type;
	  PangoContext *context;
	  PangoFontMetrics *metrics;

	  direction = gtk_widget_get_direction (widget);
      
 	  gtk_widget_style_get (widget,
 				"horizontal-padding", &horizontal_padding,
                                "arrow-scaling", &arrow_scaling,
 				NULL);
 	  
	  context = gtk_widget_get_pango_context (GTK_BIN (menu_item)->child);
	  metrics = pango_context_get_metrics (context, 
					       GTK_WIDGET (GTK_BIN (menu_item)->child)->style->font_desc,
					       pango_context_get_language (context));

	  arrow_size = (PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                                      pango_font_metrics_get_descent (metrics)));

	  pango_font_metrics_unref (metrics);

	  arrow_extent = arrow_size * arrow_scaling;

	  shadow_type = GTK_SHADOW_OUT;
	  if (state_type == GTK_STATE_PRELIGHT)
	    shadow_type = GTK_SHADOW_IN;

	  if (direction == GTK_TEXT_DIR_LTR)
	    {
	      arrow_x = x + width - horizontal_padding - arrow_extent;
	      arrow_type = GTK_ARROW_RIGHT;
	    }
	  else
	    {
	      arrow_x = x + horizontal_padding;
	      arrow_type = GTK_ARROW_LEFT;
	    }

	  arrow_y = y + (height - arrow_extent) / 2;

	  gtk_paint_arrow (widget->style, widget->window,
			   state_type, shadow_type, 
			   area, widget, "menuitem", 
			   arrow_type, TRUE,
			   arrow_x, arrow_y,
			   arrow_extent, arrow_extent);
	}
      else if (!GTK_BIN (menu_item)->child)
	{
          gboolean wide_separators;
          gint     separator_height;
	  guint    horizontal_padding;

	  gtk_widget_style_get (widget,
                                "wide-separators",    &wide_separators,
                                "separator-height",   &separator_height,
                                "horizontal-padding", &horizontal_padding,
                                NULL);

          if (wide_separators)
            gtk_paint_box (widget->style, widget->window,
                           GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                           area, widget, "hseparator",
                           widget->allocation.x + horizontal_padding + widget->style->xthickness,
                           widget->allocation.y + (widget->allocation.height -
                                                   separator_height -
                                                   widget->style->ythickness) / 2,
                           widget->allocation.width -
                           2 * (horizontal_padding + widget->style->xthickness),
                           separator_height);
          else
            gtk_paint_hline (widget->style, widget->window,
                             GTK_STATE_NORMAL, area, widget, "menuitem",
                             widget->allocation.x + horizontal_padding + widget->style->xthickness,
                             widget->allocation.x + widget->allocation.width - horizontal_padding - widget->style->xthickness - 1,
                             widget->allocation.y + (widget->allocation.height -
                                                     widget->style->ythickness) / 2);
	}
    }
}

static gint
gtk_menu_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gtk_widget_is_drawable (widget))
    {
      gtk_menu_item_paint (widget, &event->area);

      GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static void
gtk_real_menu_item_select (GtkItem *item)
{
  GtkMenuItem *menu_item;
  gboolean touchscreen_mode;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  menu_item = GTK_MENU_ITEM (item);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (item)),
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  if (!touchscreen_mode &&
      menu_item->submenu &&
      (!gtk_widget_get_mapped (menu_item->submenu) ||
       GTK_MENU (menu_item->submenu)->tearoff_active))
    {
      _gtk_menu_item_popup_submenu (GTK_WIDGET (menu_item), TRUE);
    }

  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_PRELIGHT);
  gtk_widget_queue_draw (GTK_WIDGET (menu_item));
}

static void
gtk_real_menu_item_deselect (GtkItem *item)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  menu_item = GTK_MENU_ITEM (item);

  if (menu_item->submenu)
    _gtk_menu_item_popdown_submenu (GTK_WIDGET (menu_item));

  gtk_widget_set_state (GTK_WIDGET (menu_item), GTK_STATE_NORMAL);
  gtk_widget_queue_draw (GTK_WIDGET (menu_item));
}

static gboolean
gtk_menu_item_mnemonic_activate (GtkWidget *widget,
				 gboolean   group_cycling)
{
  if (GTK_IS_MENU_SHELL (widget->parent))
    _gtk_menu_shell_set_keyboard_mode (GTK_MENU_SHELL (widget->parent), TRUE);

  if (group_cycling &&
      widget->parent &&
      GTK_IS_MENU_SHELL (widget->parent) &&
      GTK_MENU_SHELL (widget->parent)->active)
    {
      gtk_menu_shell_select_item (GTK_MENU_SHELL (widget->parent),
				  widget);
    }
  else
    g_signal_emit (widget, menu_item_signals[ACTIVATE_ITEM], 0);
  
  return TRUE;
}

static void 
gtk_real_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv;

  priv = GET_PRIVATE (menu_item);

  if (priv->action)
    gtk_action_activate (priv->action);
}


static void
gtk_real_menu_item_activate_item (GtkMenuItem *menu_item)
{
  GtkMenuItemPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  priv   = GET_PRIVATE (menu_item);
  widget = GTK_WIDGET (menu_item);
  
  if (widget->parent &&
      GTK_IS_MENU_SHELL (widget->parent))
    {
      if (menu_item->submenu == NULL)
	gtk_menu_shell_activate_item (GTK_MENU_SHELL (widget->parent),
				      widget, TRUE);
      else
	{
	  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget->parent);

	  gtk_menu_shell_select_item (GTK_MENU_SHELL (widget->parent), widget);
	  _gtk_menu_item_popup_submenu (widget, FALSE);

	  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu_item->submenu), TRUE);
	}
    }
}

static void
gtk_real_menu_item_toggle_size_request (GtkMenuItem *menu_item,
					gint        *requisition)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  *requisition = 0;
}

static void
gtk_real_menu_item_toggle_size_allocate (GtkMenuItem *menu_item,
					 gint         allocation)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  menu_item->toggle_size = allocation;
}

static void
gtk_real_menu_item_set_label (GtkMenuItem *menu_item,
			      const gchar *label)
{
  gtk_menu_item_ensure_label (menu_item);

  if (GTK_IS_LABEL (GTK_BIN (menu_item)->child))
    {
      gtk_label_set_label (GTK_LABEL (GTK_BIN (menu_item)->child), label ? label : "");
      
      g_object_notify (G_OBJECT (menu_item), "label");
    }
}

static const gchar *
gtk_real_menu_item_get_label (GtkMenuItem *menu_item)
{
  gtk_menu_item_ensure_label (menu_item);

  if (GTK_IS_LABEL (GTK_BIN (menu_item)->child))
    return gtk_label_get_label (GTK_LABEL (GTK_BIN (menu_item)->child));

  return NULL;
}

static void
free_timeval (GTimeVal *val)
{
  g_slice_free (GTimeVal, val);
}

static void
gtk_menu_item_real_popup_submenu (GtkWidget *widget,
                                  gboolean   remember_exact_time)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);

  if (gtk_widget_is_sensitive (menu_item->submenu) && widget->parent)
    {
      gboolean take_focus;
      GtkMenuPositionFunc menu_position_func;

      take_focus = gtk_menu_shell_get_take_focus (GTK_MENU_SHELL (widget->parent));
      gtk_menu_shell_set_take_focus (GTK_MENU_SHELL (menu_item->submenu),
                                     take_focus);

      if (remember_exact_time)
        {
          GTimeVal *popup_time = g_slice_new0 (GTimeVal);

          g_get_current_time (popup_time);

          g_object_set_data_full (G_OBJECT (menu_item->submenu),
                                  "gtk-menu-exact-popup-time", popup_time,
                                  (GDestroyNotify) free_timeval);
        }
      else
        {
          g_object_set_data (G_OBJECT (menu_item->submenu),
                             "gtk-menu-exact-popup-time", NULL);
        }

      /* gtk_menu_item_position_menu positions the submenu from the
       * menuitems position. If the menuitem doesn't have a window,
       * that doesn't work. In that case we use the default
       * positioning function instead which places the submenu at the
       * mouse cursor.
       */
      if (widget->window)
        menu_position_func = gtk_menu_item_position_menu;
      else
        menu_position_func = NULL;

      gtk_menu_popup (GTK_MENU (menu_item->submenu),
                      widget->parent,
                      widget,
                      menu_position_func,
                      menu_item,
                      GTK_MENU_SHELL (widget->parent)->button,
                      0);
    }

  /* Enable themeing of the parent menu item depending on whether
   * its submenu is shown or not.
   */
  gtk_widget_queue_draw (widget);
}

static gint
gtk_menu_item_popup_timeout (gpointer data)
{
  GtkMenuItem *menu_item;
  GtkWidget *parent;
  
  menu_item = GTK_MENU_ITEM (data);

  parent = GTK_WIDGET (menu_item)->parent;

  if ((GTK_IS_MENU_SHELL (parent) && GTK_MENU_SHELL (parent)->active) || 
      (GTK_IS_MENU (parent) && GTK_MENU (parent)->torn_off))
    {
      gtk_menu_item_real_popup_submenu (GTK_WIDGET (menu_item), TRUE);
      if (menu_item->timer_from_keypress && menu_item->submenu)
	GTK_MENU_SHELL (menu_item->submenu)->ignore_enter = TRUE;
    }

  menu_item->timer = 0;

  return FALSE;  
}

static gint
get_popup_delay (GtkWidget *widget)
{
  if (GTK_IS_MENU_SHELL (widget->parent))
    {
      return _gtk_menu_shell_get_popup_delay (GTK_MENU_SHELL (widget->parent));
    }
  else
    {
      gint popup_delay;

      g_object_get (gtk_widget_get_settings (widget),
		    "gtk-menu-popup-delay", &popup_delay,
		    NULL);

      return popup_delay;
    }
}

void
_gtk_menu_item_popup_submenu (GtkWidget *widget,
                              gboolean   with_delay)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);

  if (menu_item->timer)
    {
      g_source_remove (menu_item->timer);
      menu_item->timer = 0;
      with_delay = FALSE;
    }

  if (with_delay)
    {
      gint popup_delay = get_popup_delay (widget);

      if (popup_delay > 0)
	{
	  GdkEvent *event = gtk_get_current_event ();

	  menu_item->timer = gdk_threads_add_timeout (popup_delay,
                                                      gtk_menu_item_popup_timeout,
                                                      menu_item);

	  if (event &&
	      event->type != GDK_BUTTON_PRESS &&
	      event->type != GDK_ENTER_NOTIFY)
	    menu_item->timer_from_keypress = TRUE;
	  else
	    menu_item->timer_from_keypress = FALSE;

	  if (event)
	    gdk_event_free (event);

          return;
        }
    }

  gtk_menu_item_real_popup_submenu (widget, FALSE);
}

void
_gtk_menu_item_popdown_submenu (GtkWidget *widget)
{
  GtkMenuItem *menu_item;

  menu_item = GTK_MENU_ITEM (widget);

  if (menu_item->submenu)
    {
      g_object_set_data (G_OBJECT (menu_item->submenu),
                         "gtk-menu-exact-popup-time", NULL);

      if (menu_item->timer)
        {
          g_source_remove (menu_item->timer);
          menu_item->timer = 0;
        }
      else
        gtk_menu_popdown (GTK_MENU (menu_item->submenu));

      gtk_widget_queue_draw (widget);
    }
}

static void
get_offsets (GtkMenu *menu,
	     gint    *horizontal_offset,
	     gint    *vertical_offset)
{
  gint vertical_padding;
  gint horizontal_padding;
  
  gtk_widget_style_get (GTK_WIDGET (menu),
			"horizontal-offset", horizontal_offset,
			"vertical-offset", vertical_offset,
			"horizontal-padding", &horizontal_padding,
			"vertical-padding", &vertical_padding,
			NULL);

  *vertical_offset -= GTK_WIDGET (menu)->style->ythickness;
  *vertical_offset -= vertical_padding;
  *horizontal_offset += horizontal_padding;
}

static void
gtk_menu_item_position_menu (GtkMenu  *menu,
			     gint     *x,
			     gint     *y,
			     gboolean *push_in,
			     gpointer  user_data)
{
  GtkMenuItem *menu_item;
  GtkWidget *widget;
  GtkMenuItem *parent_menu_item;
  GdkScreen *screen;
  gint twidth, theight;
  gint tx, ty;
  GtkTextDirection direction;
  GdkRectangle monitor;
  gint monitor_num;
  gint horizontal_offset;
  gint vertical_offset;
  gint parent_xthickness;
  gint available_left, available_right;

  g_return_if_fail (menu != NULL);
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  menu_item = GTK_MENU_ITEM (user_data);
  widget = GTK_WIDGET (user_data);

  if (push_in)
    *push_in = FALSE;

  direction = gtk_widget_get_direction (widget);

  twidth = GTK_WIDGET (menu)->requisition.width;
  theight = GTK_WIDGET (menu)->requisition.height;

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, menu_item->event_window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  if (!gdk_window_get_origin (widget->window, &tx, &ty))
    {
      g_warning ("Menu not on screen");
      return;
    }

  tx += widget->allocation.x;
  ty += widget->allocation.y;

  get_offsets (menu, &horizontal_offset, &vertical_offset);

  available_left = tx - monitor.x;
  available_right = monitor.x + monitor.width - (tx + widget->allocation.width);

  if (GTK_IS_MENU_BAR (widget->parent))
    {
      menu_item->from_menubar = TRUE;
    }
  else if (GTK_IS_MENU (widget->parent))
    {
      if (GTK_MENU (widget->parent)->parent_menu_item)
	menu_item->from_menubar = GTK_MENU_ITEM (GTK_MENU (widget->parent)->parent_menu_item)->from_menubar;
      else
	menu_item->from_menubar = FALSE;
    }
  else
    {
      menu_item->from_menubar = FALSE;
    }
  
  switch (menu_item->submenu_placement)
    {
    case GTK_TOP_BOTTOM:
      if (direction == GTK_TEXT_DIR_LTR)
	menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
      else 
	{
	  menu_item->submenu_direction = GTK_DIRECTION_LEFT;
	  tx += widget->allocation.width - twidth;
	}
      if ((ty + widget->allocation.height + theight) <= monitor.y + monitor.height)
	ty += widget->allocation.height;
      else if ((ty - theight) >= monitor.y)
	ty -= theight;
      else if (monitor.y + monitor.height - (ty + widget->allocation.height) > ty)
	ty += widget->allocation.height;
      else
	ty -= theight;
      break;

    case GTK_LEFT_RIGHT:
      if (GTK_IS_MENU (widget->parent))
	parent_menu_item = GTK_MENU_ITEM (GTK_MENU (widget->parent)->parent_menu_item);
      else
	parent_menu_item = NULL;
      
      parent_xthickness = widget->parent->style->xthickness;

      if (parent_menu_item && !GTK_MENU (widget->parent)->torn_off)
	{
	  menu_item->submenu_direction = parent_menu_item->submenu_direction;
	}
      else
	{
	  if (direction == GTK_TEXT_DIR_LTR)
	    menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
	  else
	    menu_item->submenu_direction = GTK_DIRECTION_LEFT;
	}

      switch (menu_item->submenu_direction)
	{
	case GTK_DIRECTION_LEFT:
	  if (tx - twidth - parent_xthickness - horizontal_offset >= monitor.x ||
	      available_left >= available_right)
	    tx -= twidth + parent_xthickness + horizontal_offset;
	  else
	    {
	      menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
	      tx += widget->allocation.width + parent_xthickness + horizontal_offset;
	    }
	  break;

	case GTK_DIRECTION_RIGHT:
	  if (tx + widget->allocation.width + parent_xthickness + horizontal_offset + twidth <= monitor.x + monitor.width ||
	      available_right >= available_left)
	    tx += widget->allocation.width + parent_xthickness + horizontal_offset;
	  else
	    {
	      menu_item->submenu_direction = GTK_DIRECTION_LEFT;
	      tx -= twidth + parent_xthickness + horizontal_offset;
	    }
	  break;
	}

      ty += vertical_offset;
      
      /* If the height of the menu doesn't fit we move it upward. */
      ty = CLAMP (ty, monitor.y, MAX (monitor.y, monitor.y + monitor.height - theight));
      break;
    }

  /* If we have negative, tx, here it is because we can't get
   * the menu all the way on screen. Favor the left portion.
   */
  *x = CLAMP (tx, monitor.x, MAX (monitor.x, monitor.x + monitor.width - twidth));
  *y = ty;

  gtk_menu_set_monitor (menu, monitor_num);

  if (!gtk_widget_get_visible (menu->toplevel))
    {
      gtk_window_set_type_hint (GTK_WINDOW (menu->toplevel), menu_item->from_menubar?
				GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU : GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    }
}

/**
 * gtk_menu_item_set_right_justified:
 * @menu_item: a #GtkMenuItem.
 * @right_justified: if %TRUE the menu item will appear at the 
 *   far right if added to a menu bar.
 * 
 * Sets whether the menu item appears justified at the right
 * side of a menu bar. This was traditionally done for "Help" menu
 * items, but is now considered a bad idea. (If the widget
 * layout is reversed for a right-to-left language like Hebrew
 * or Arabic, right-justified-menu-items appear at the left.)
 **/
void
gtk_menu_item_set_right_justified (GtkMenuItem *menu_item,
				   gboolean     right_justified)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  right_justified = right_justified != FALSE;

  if (right_justified != menu_item->right_justify)
    {
      menu_item->right_justify = right_justified;
      gtk_widget_queue_resize (GTK_WIDGET (menu_item));
    }
}

/**
 * gtk_menu_item_get_right_justified:
 * @menu_item: a #GtkMenuItem
 * 
 * Gets whether the menu item appears justified at the right
 * side of the menu bar.
 * 
 * Return value: %TRUE if the menu item will appear at the
 *   far right if added to a menu bar.
 **/
gboolean
gtk_menu_item_get_right_justified (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);
  
  return menu_item->right_justify;
}


static void
gtk_menu_item_show_all (GtkWidget *widget)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  menu_item = GTK_MENU_ITEM (widget);

  /* show children including submenu */
  if (menu_item->submenu)
    gtk_widget_show_all (menu_item->submenu);
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);

  gtk_widget_show (widget);
}

static void
gtk_menu_item_hide_all (GtkWidget *widget)
{
  GtkMenuItem *menu_item;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  gtk_widget_hide (widget);

  menu_item = GTK_MENU_ITEM (widget);

  /* hide children including submenu */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_hide_all, NULL);
  if (menu_item->submenu)
    gtk_widget_hide_all (menu_item->submenu);
}

static gboolean
gtk_menu_item_can_activate_accel (GtkWidget *widget,
				  guint      signal_id)
{
  /* Chain to the parent GtkMenu for further checks */
  return (gtk_widget_is_sensitive (widget) && gtk_widget_get_visible (widget) &&
          widget->parent && gtk_widget_can_activate_accel (widget->parent, signal_id));
}

static void
gtk_menu_item_accel_name_foreach (GtkWidget *widget,
				  gpointer data)
{
  const gchar **path_p = data;

  if (!*path_p)
    {
      if (GTK_IS_LABEL (widget))
	{
	  *path_p = gtk_label_get_text (GTK_LABEL (widget));
	  if (*path_p && (*path_p)[0] == 0)
	    *path_p = NULL;
	}
      else if (GTK_IS_CONTAINER (widget))
	gtk_container_foreach (GTK_CONTAINER (widget),
			       gtk_menu_item_accel_name_foreach,
			       data);
    }
}

static void
gtk_menu_item_parent_set (GtkWidget *widget,
			  GtkWidget *previous_parent)
{
  GtkMenuItem *menu_item = GTK_MENU_ITEM (widget);
  GtkMenu *menu = GTK_IS_MENU (widget->parent) ? GTK_MENU (widget->parent) : NULL;

  if (menu)
    _gtk_menu_item_refresh_accel_path (menu_item,
				       menu->accel_path,
				       menu->accel_group,
				       TRUE);

  if (GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->parent_set)
    GTK_WIDGET_CLASS (gtk_menu_item_parent_class)->parent_set (widget, previous_parent);
}

void
_gtk_menu_item_refresh_accel_path (GtkMenuItem   *menu_item,
				   const gchar   *prefix,
				   GtkAccelGroup *accel_group,
				   gboolean       group_changed)
{
  const gchar *path;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (!accel_group || GTK_IS_ACCEL_GROUP (accel_group));

  widget = GTK_WIDGET (menu_item);

  if (!accel_group)
    {
      gtk_widget_set_accel_path (widget, NULL, NULL);
      return;
    }

  path = _gtk_widget_get_accel_path (widget, NULL);
  if (!path)					/* no active accel_path yet */
    {
      path = menu_item->accel_path;
      if (!path && prefix)
	{
	  const gchar *postfix = NULL;
          gchar *new_path;

	  /* try to construct one from label text */
	  gtk_container_foreach (GTK_CONTAINER (menu_item),
				 gtk_menu_item_accel_name_foreach,
				 &postfix);
          if (postfix)
            {
              new_path = g_strconcat (prefix, "/", postfix, NULL);
              path = menu_item->accel_path = (char*)g_intern_string (new_path);
              g_free (new_path);
            }
	}
      if (path)
	gtk_widget_set_accel_path (widget, path, accel_group);
    }
  else if (group_changed)			/* reinstall accelerators */
    gtk_widget_set_accel_path (widget, path, accel_group);
}

/**
 * gtk_menu_item_set_accel_path
 * @menu_item:  a valid #GtkMenuItem
 * @accel_path: (allow-none): accelerator path, corresponding to this menu item's
 *              functionality, or %NULL to unset the current path.
 *
 * Set the accelerator path on @menu_item, through which runtime changes of the
 * menu item's accelerator caused by the user can be identified and saved to
 * persistant storage (see gtk_accel_map_save() on this).
 * To setup a default accelerator for this menu item, call
 * gtk_accel_map_add_entry() with the same @accel_path.
 * See also gtk_accel_map_add_entry() on the specifics of accelerator paths,
 * and gtk_menu_set_accel_path() for a more convenient variant of this function.
 *
 * This function is basically a convenience wrapper that handles calling
 * gtk_widget_set_accel_path() with the appropriate accelerator group for
 * the menu item.
 *
 * Note that you do need to set an accelerator on the parent menu with
 * gtk_menu_set_accel_group() for this to work.
 *
 * Note that @accel_path string will be stored in a #GQuark. Therefore, if you
 * pass a static string, you can save some memory by interning it first with 
 * g_intern_static_string().
 */
void
gtk_menu_item_set_accel_path (GtkMenuItem *menu_item,
			      const gchar *accel_path)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (accel_path == NULL ||
		    (accel_path[0] == '<' && strchr (accel_path, '/')));

  widget = GTK_WIDGET (menu_item);

  /* store new path */
  menu_item->accel_path = (char*)g_intern_string (accel_path);

  /* forget accelerators associated with old path */
  gtk_widget_set_accel_path (widget, NULL, NULL);

  /* install accelerators associated with new path */
  if (GTK_IS_MENU (widget->parent))
    {
      GtkMenu *menu = GTK_MENU (widget->parent);

      if (menu->accel_group)
	_gtk_menu_item_refresh_accel_path (GTK_MENU_ITEM (widget),
					   NULL,
					   menu->accel_group,
					   FALSE);
    }
}

/**
 * gtk_menu_item_get_accel_path
 * @menu_item:  a valid #GtkMenuItem
 *
 * Retrieve the accelerator path that was previously set on @menu_item.
 *
 * See gtk_menu_item_set_accel_path() for details.
 *
 * Returns: the accelerator path corresponding to this menu item's
 *              functionality, or %NULL if not set
 *
 * Since: 2.14
 */
const gchar *
gtk_menu_item_get_accel_path (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return menu_item->accel_path;
}

static void
gtk_menu_item_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkBin *bin;

  g_return_if_fail (GTK_IS_MENU_ITEM (container));
  g_return_if_fail (callback != NULL);

  bin = GTK_BIN (container);

  if (bin->child)
    callback (bin->child, callback_data);
}

gboolean
_gtk_menu_item_is_selectable (GtkWidget *menu_item)
{
  if ((!GTK_BIN (menu_item)->child &&
       G_OBJECT_TYPE (menu_item) == GTK_TYPE_MENU_ITEM) ||
      GTK_IS_SEPARATOR_MENU_ITEM (menu_item) ||
      !gtk_widget_is_sensitive (menu_item) ||
      !gtk_widget_get_visible (menu_item))
    return FALSE;

  return TRUE;
}

static void
gtk_menu_item_ensure_label (GtkMenuItem *menu_item)
{
  GtkWidget *accel_label;

  if (!GTK_BIN (menu_item)->child)
    {
      accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
      gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

      gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
      gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), 
					GTK_WIDGET (menu_item));
      gtk_widget_show (accel_label);
    }
}

/**
 * gtk_menu_item_set_label:
 * @menu_item: a #GtkMenuItem
 * @label: the text you want to set
 *
 * Sets @text on the @menu_item label
 *
 * Since: 2.16
 **/
void
gtk_menu_item_set_label (GtkMenuItem *menu_item,
			 const gchar *label)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  GTK_MENU_ITEM_GET_CLASS (menu_item)->set_label (menu_item, label);
}

/**
 * gtk_menu_item_get_label:
 * @menu_item: a #GtkMenuItem
 *
 * Sets @text on the @menu_item label
 *
 * Returns: The text in the @menu_item label. This is the internal
 *   string used by the label, and must not be modified.
 *
 * Since: 2.16
 **/
const gchar *
gtk_menu_item_get_label (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), NULL);

  return GTK_MENU_ITEM_GET_CLASS (menu_item)->get_label (menu_item);
}

/**
 * gtk_menu_item_set_use_underline:
 * @menu_item: a #GtkMenuItem
 * @setting: %TRUE if underlines in the text indicate mnemonics  
 *
 * If true, an underline in the text indicates the next character should be
 * used for the mnemonic accelerator key.
 *
 * Since: 2.16
 **/
void
gtk_menu_item_set_use_underline (GtkMenuItem *menu_item,
				 gboolean     setting)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_menu_item_ensure_label (menu_item);

  if (GTK_IS_LABEL (GTK_BIN (menu_item)->child))
    {
      gtk_label_set_use_underline (GTK_LABEL (GTK_BIN (menu_item)->child), setting);

      g_object_notify (G_OBJECT (menu_item), "use-underline");
    }
}

/**
 * gtk_menu_item_get_use_underline:
 * @menu_item: a #GtkMenuItem
 *
 * Checks if an underline in the text indicates the next character should be
 * used for the mnemonic accelerator key.
 *
 * Return value: %TRUE if an embedded underline in the label indicates
 *               the mnemonic accelerator key.
 *
 * Since: 2.16
 **/
gboolean
gtk_menu_item_get_use_underline (GtkMenuItem *menu_item)
{
  g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);

  gtk_menu_item_ensure_label (menu_item);
  
  if (GTK_IS_LABEL (GTK_BIN (menu_item)->child))
    return gtk_label_get_use_underline (GTK_LABEL (GTK_BIN (menu_item)->child));

  return FALSE;
}



#define __GTK_MENU_ITEM_C__
#include "gtkaliasdef.c"
