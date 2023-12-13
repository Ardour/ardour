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
#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkkeyhash.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtkmenushell.h"
#include "gtkmnemonichash.h"
#include "gtktearoffmenuitem.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define MENU_SHELL_TIMEOUT   500

#define PACK_DIRECTION(m)                                 \
   (GTK_IS_MENU_BAR (m)                                   \
     ? gtk_menu_bar_get_pack_direction (GTK_MENU_BAR (m)) \
     : GTK_PACK_DIRECTION_LTR)

enum {
  DEACTIVATE,
  SELECTION_DONE,
  MOVE_CURRENT,
  ACTIVATE_CURRENT,
  CANCEL,
  CYCLE_FOCUS,
  MOVE_SELECTED,
  INSERT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TAKE_FOCUS
};

/* Terminology:
 * 
 * A menu item can be "selected", this means that it is displayed
 * in the prelight state, and if it has a submenu, that submenu
 * will be popped up. 
 * 
 * A menu is "active" when it is visible onscreen and the user
 * is selecting from it. A menubar is not active until the user
 * clicks on one of its menuitems. When a menu is active,
 * passing the mouse over a submenu will pop it up.
 *
 * menu_shell->active_menu_item, is however, not an "active"
 * menu item (there is no such thing) but rather, the selected
 * menu item in that MenuShell, if there is one.
 *
 * There is also is a concept of the current menu and a current
 * menu item. The current menu item is the selected menu item
 * that is furthest down in the hierarchy. (Every active menu_shell
 * does not necessarily contain a selected menu item, but if
 * it does, then menu_shell->parent_menu_shell must also contain
 * a selected menu item. The current menu is the menu that 
 * contains the current menu_item. It will always have a GTK
 * grab and receive all key presses.
 *
 *
 * Action signals:
 *
 *  ::move_current (GtkMenuDirection *dir)
 *     Moves the current menu item in direction 'dir':
 *
 *       GTK_MENU_DIR_PARENT: To the parent menu shell
 *       GTK_MENU_DIR_CHILD: To the child menu shell (if this item has
 *          a submenu.
 *       GTK_MENU_DIR_NEXT/PREV: To the next or previous item
 *          in this menu.
 * 
 *     As a a bit of a hack to get movement between menus and
 *     menubars working, if submenu_placement is different for
 *     the menu and its MenuShell then the following apply:
 * 
 *       - For 'parent' the current menu is not just moved to
 *         the parent, but moved to the previous entry in the parent
 *       - For 'child', if there is no child, then current is
 *         moved to the next item in the parent.
 *
 *    Note that the above explanation of ::move_current was written
 *    before menus and menubars had support for RTL flipping and
 *    different packing directions, and therefore only applies for
 *    when text direction and packing direction are both left-to-right.
 * 
 *  ::activate_current (GBoolean *force_hide)
 *     Activate the current item. If 'force_hide' is true, hide
 *     the current menu item always. Otherwise, only hide
 *     it if menu_item->klass->hide_on_activate is true.
 *
 *  ::cancel ()
 *     Cancels the current selection
 */

#define GTK_MENU_SHELL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_MENU_SHELL, GtkMenuShellPrivate))

typedef struct _GtkMenuShellPrivate GtkMenuShellPrivate;

struct _GtkMenuShellPrivate
{
  GtkMnemonicHash *mnemonic_hash;
  GtkKeyHash *key_hash;

  guint take_focus : 1;
  guint activated_submenu : 1;
  /* This flag is a crutch to keep mnemonics in the same menu
   * if the user moves the mouse over an unselectable menuitem.
   */
  guint in_unselectable_item : 1;
};

static void gtk_menu_shell_set_property      (GObject           *object,
                                              guint              prop_id,
                                              const GValue      *value,
                                              GParamSpec        *pspec);
static void gtk_menu_shell_get_property      (GObject           *object,
                                              guint              prop_id,
                                              GValue            *value,
                                              GParamSpec        *pspec);
static void gtk_menu_shell_realize           (GtkWidget         *widget);
static void gtk_menu_shell_finalize          (GObject           *object);
static gint gtk_menu_shell_button_press      (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_button_release    (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_key_press         (GtkWidget	        *widget,
					      GdkEventKey       *event);
static gint gtk_menu_shell_enter_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static gint gtk_menu_shell_leave_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static void gtk_menu_shell_screen_changed    (GtkWidget         *widget,
					      GdkScreen         *previous_screen);
static gboolean gtk_menu_shell_grab_broken       (GtkWidget         *widget,
					      GdkEventGrabBroken *event);
static void gtk_menu_shell_add               (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_remove            (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_forall            (GtkContainer      *container,
					      gboolean		 include_internals,
					      GtkCallback        callback,
					      gpointer           callback_data);
static void gtk_menu_shell_real_insert       (GtkMenuShell *menu_shell,
					      GtkWidget    *child,
					      gint          position);
static void gtk_real_menu_shell_deactivate   (GtkMenuShell      *menu_shell);
static gint gtk_menu_shell_is_item           (GtkMenuShell      *menu_shell,
					      GtkWidget         *child);
static GtkWidget *gtk_menu_shell_get_item    (GtkMenuShell      *menu_shell,
					      GdkEvent          *event);
static GType    gtk_menu_shell_child_type  (GtkContainer      *container);
static void gtk_menu_shell_real_select_item  (GtkMenuShell      *menu_shell,
					      GtkWidget         *menu_item);
static gboolean gtk_menu_shell_select_submenu_first (GtkMenuShell   *menu_shell); 

static void gtk_real_menu_shell_move_current (GtkMenuShell      *menu_shell,
					      GtkMenuDirectionType direction);
static void gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
						  gboolean           force_hide);
static void gtk_real_menu_shell_cancel           (GtkMenuShell      *menu_shell);
static void gtk_real_menu_shell_cycle_focus      (GtkMenuShell      *menu_shell,
						  GtkDirectionType   dir);

static void     gtk_menu_shell_reset_key_hash    (GtkMenuShell *menu_shell);
static gboolean gtk_menu_shell_activate_mnemonic (GtkMenuShell *menu_shell,
						  GdkEventKey  *event);
static gboolean gtk_menu_shell_real_move_selected (GtkMenuShell  *menu_shell, 
						   gint           distance);

static guint menu_shell_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE (GtkMenuShell, gtk_menu_shell, GTK_TYPE_CONTAINER)

static void
gtk_menu_shell_class_init (GtkMenuShellClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  GtkBindingSet *binding_set;

  object_class = (GObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  object_class->set_property = gtk_menu_shell_set_property;
  object_class->get_property = gtk_menu_shell_get_property;
  object_class->finalize = gtk_menu_shell_finalize;

  widget_class->realize = gtk_menu_shell_realize;
  widget_class->button_press_event = gtk_menu_shell_button_press;
  widget_class->button_release_event = gtk_menu_shell_button_release;
  widget_class->grab_broken_event = gtk_menu_shell_grab_broken;
  widget_class->key_press_event = gtk_menu_shell_key_press;
  widget_class->enter_notify_event = gtk_menu_shell_enter_notify;
  widget_class->leave_notify_event = gtk_menu_shell_leave_notify;
  widget_class->screen_changed = gtk_menu_shell_screen_changed;

  container_class->add = gtk_menu_shell_add;
  container_class->remove = gtk_menu_shell_remove;
  container_class->forall = gtk_menu_shell_forall;
  container_class->child_type = gtk_menu_shell_child_type;

  klass->submenu_placement = GTK_TOP_BOTTOM;
  klass->deactivate = gtk_real_menu_shell_deactivate;
  klass->selection_done = NULL;
  klass->move_current = gtk_real_menu_shell_move_current;
  klass->activate_current = gtk_real_menu_shell_activate_current;
  klass->cancel = gtk_real_menu_shell_cancel;
  klass->select_item = gtk_menu_shell_real_select_item;
  klass->insert = gtk_menu_shell_real_insert;
  klass->move_selected = gtk_menu_shell_real_move_selected;

  menu_shell_signals[DEACTIVATE] =
    g_signal_new (I_("deactivate"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkMenuShellClass, deactivate),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  menu_shell_signals[SELECTION_DONE] =
    g_signal_new (I_("selection-done"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkMenuShellClass, selection_done),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  menu_shell_signals[MOVE_CURRENT] =
    g_signal_new (I_("move-current"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuShellClass, move_current),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_MENU_DIRECTION_TYPE);

  menu_shell_signals[ACTIVATE_CURRENT] =
    g_signal_new (I_("activate-current"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuShellClass, activate_current),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);

  menu_shell_signals[CANCEL] =
    g_signal_new (I_("cancel"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuShellClass, cancel),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  menu_shell_signals[CYCLE_FOCUS] =
    g_signal_new_class_handler (I_("cycle-focus"),
                                G_OBJECT_CLASS_TYPE (object_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_real_menu_shell_cycle_focus),
                                NULL, NULL,
                                _gtk_marshal_VOID__ENUM,
                                G_TYPE_NONE, 1,
                                GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkMenuShell::move-selected:
   * @menu_shell: the object on which the signal is emitted
   * @distance: +1 to move to the next item, -1 to move to the previous
   *
   * The ::move-selected signal is emitted to move the selection to
   * another item.
   *
   * Returns: %TRUE to stop the signal emission, %FALSE to continue
   *
   * Since: 2.12
   */
  menu_shell_signals[MOVE_SELECTED] =
    g_signal_new (I_("move-selected"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkMenuShellClass, move_selected),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__INT,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_INT);

  /**
   * GtkMenuShell::insert:
   * @menu_shell: the object on which the signal is emitted
   * @child: the #GtkMenuItem that is being inserted
   * @position: the position at which the insert occurs
   *
   * The ::insert signal is emitted when a new #GtkMenuItem is added to
   * a #GtkMenuShell.  A separate signal is used instead of
   * GtkContainer::add because of the need for an additional position
   * parameter.
   *
   * The inverse of this signal is the GtkContainer::remove signal.
   *
   * Since: 2.24.15
   **/
  menu_shell_signals[INSERT] =
    g_signal_new (I_("insert"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuShellClass, insert),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_INT,
                  G_TYPE_NONE, 2, GTK_TYPE_WIDGET, G_TYPE_INT);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Escape, 0,
				"cancel", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Return, 0,
				"activate-current", 1,
				G_TYPE_BOOLEAN,
				TRUE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_ISO_Enter, 0,
				"activate-current", 1,
				G_TYPE_BOOLEAN,
				TRUE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Enter, 0,
				"activate-current", 1,
				G_TYPE_BOOLEAN,
				TRUE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_space, 0,
				"activate-current", 1,
				G_TYPE_BOOLEAN,
				FALSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Space, 0,
				"activate-current", 1,
				G_TYPE_BOOLEAN,
				FALSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_F10, 0,
				"cycle-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_F10, GDK_SHIFT_MASK,
				"cycle-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);

  /**
   * GtkMenuShell:take-focus:
   *
   * A boolean that determines whether the menu and its submenus grab the
   * keyboard focus. See gtk_menu_shell_set_take_focus() and
   * gtk_menu_shell_get_take_focus().
   *
   * Since: 2.8
   **/
  g_object_class_install_property (object_class,
                                   PROP_TAKE_FOCUS,
                                   g_param_spec_boolean ("take-focus",
							 P_("Take Focus"),
							 P_("A boolean that determines whether the menu grabs the keyboard focus"),
							 TRUE,
							 GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkMenuShellPrivate));
}

static GType
gtk_menu_shell_child_type (GtkContainer     *container)
{
  return GTK_TYPE_MENU_ITEM;
}

static void
gtk_menu_shell_init (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

  menu_shell->children = NULL;
  menu_shell->active_menu_item = NULL;
  menu_shell->parent_menu_shell = NULL;
  menu_shell->active = FALSE;
  menu_shell->have_grab = FALSE;
  menu_shell->have_xgrab = FALSE;
  menu_shell->button = 0;
  menu_shell->activate_time = 0;

  priv->mnemonic_hash = NULL;
  priv->key_hash = NULL;
  priv->take_focus = TRUE;
  priv->activated_submenu = FALSE;
}

static void
gtk_menu_shell_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);

  switch (prop_id)
    {
    case PROP_TAKE_FOCUS:
      gtk_menu_shell_set_take_focus (menu_shell, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_shell_get_property (GObject     *object,
                             guint        prop_id,
                             GValue      *value,
                             GParamSpec  *pspec)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);

  switch (prop_id)
    {
    case PROP_TAKE_FOCUS:
      g_value_set_boolean (value, gtk_menu_shell_get_take_focus (menu_shell));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_shell_finalize (GObject *object)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (object);
  GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

  if (priv->mnemonic_hash)
    _gtk_mnemonic_hash_free (priv->mnemonic_hash);
  if (priv->key_hash)
    _gtk_key_hash_free (priv->key_hash);

  G_OBJECT_CLASS (gtk_menu_shell_parent_class)->finalize (object);
}


void
gtk_menu_shell_append (GtkMenuShell *menu_shell,
		       GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, -1);
}

void
gtk_menu_shell_prepend (GtkMenuShell *menu_shell,
			GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, 0);
}

void
gtk_menu_shell_insert (GtkMenuShell *menu_shell,
		       GtkWidget    *child,
		       gint          position)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  g_signal_emit (menu_shell, menu_shell_signals[INSERT], 0, child, position);
}

static void
gtk_menu_shell_real_insert (GtkMenuShell *menu_shell,
			    GtkWidget    *child,
			    gint          position)
{
  menu_shell->children = g_list_insert (menu_shell->children, child, position);

  gtk_widget_set_parent (child, GTK_WIDGET (menu_shell));
}

void
gtk_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  g_signal_emit (menu_shell, menu_shell_signals[DEACTIVATE], 0);
}

static void
gtk_menu_shell_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_menu_shell_activate (GtkMenuShell *menu_shell)
{
  if (!menu_shell->active)
    {
      gtk_grab_add (GTK_WIDGET (menu_shell));
      menu_shell->have_grab = TRUE;
      menu_shell->active = TRUE;
    }
}

static gint
gtk_menu_shell_button_press (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  menu_shell = GTK_MENU_SHELL (widget);

  if (menu_shell->parent_menu_shell)
    return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);

  menu_item = gtk_menu_shell_get_item (menu_shell, (GdkEvent *)event);

  if (menu_item && _gtk_menu_item_is_selectable (menu_item) &&
      menu_item != GTK_MENU_SHELL (menu_item->parent)->active_menu_item)
    {
      /*  select the menu item *before* activating the shell, so submenus
       *  which might be open are closed the friendly way. If we activate
       *  (and thus grab) this menu shell first, we might get grab_broken
       *  events which will close the entire menu hierarchy. Selecting the
       *  menu item also fixes up the state as if enter_notify() would
       *  have run before (which normally selects the item).
       */
      if (GTK_MENU_SHELL_GET_CLASS (menu_item->parent)->submenu_placement != GTK_TOP_BOTTOM)
        {
          gtk_menu_shell_select_item (GTK_MENU_SHELL (menu_item->parent), menu_item);
        }
    }

  if (!menu_shell->active || !menu_shell->button)
    {
      gtk_menu_shell_activate (menu_shell);

      menu_shell->button = event->button;

      if (menu_item && _gtk_menu_item_is_selectable (menu_item) &&
	  menu_item->parent == widget &&
          menu_item != menu_shell->active_menu_item)
        {
          if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement == GTK_TOP_BOTTOM)
            {
              menu_shell->activate_time = event->time;
              gtk_menu_shell_select_item (menu_shell, menu_item);
            }
        }
    }
  else
    {
      widget = gtk_get_event_widget ((GdkEvent*) event);
      if (widget == GTK_WIDGET (menu_shell))
	{
	  gtk_menu_shell_deactivate (menu_shell);
	  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
	}
    }

  if (menu_item && _gtk_menu_item_is_selectable (menu_item) &&
      GTK_MENU_ITEM (menu_item)->submenu != NULL &&
      !gtk_widget_get_visible (GTK_MENU_ITEM (menu_item)->submenu))
    {
      GtkMenuShellPrivate *priv;

      _gtk_menu_item_popup_submenu (menu_item, FALSE);

      priv = GTK_MENU_SHELL_GET_PRIVATE (menu_item->parent);
      priv->activated_submenu = TRUE;
    }

  return TRUE;
}

static gboolean
gtk_menu_shell_grab_broken (GtkWidget          *widget,
			    GdkEventGrabBroken *event)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);

  if (menu_shell->have_xgrab && event->grab_window == NULL)
    {
      /* Unset the active menu item so gtk_menu_popdown() doesn't see it.
       */
      
      gtk_menu_shell_deselect (menu_shell);
      
      gtk_menu_shell_deactivate (menu_shell);
      g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
    }

  return TRUE;
}

static gint
gtk_menu_shell_button_release (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);
  GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (widget);

  if (menu_shell->active)
    {
      GtkWidget *menu_item;
      gboolean   deactivate = TRUE;

      if (menu_shell->button && (event->button != menu_shell->button))
	{
	  menu_shell->button = 0;
	  if (menu_shell->parent_menu_shell)
	    return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}

      menu_shell->button = 0;
      menu_item = gtk_menu_shell_get_item (menu_shell, (GdkEvent*) event);

      if ((event->time - menu_shell->activate_time) > MENU_SHELL_TIMEOUT)
        {
          if (menu_item && (menu_shell->active_menu_item == menu_item) &&
              _gtk_menu_item_is_selectable (menu_item))
            {
              GtkWidget *submenu = GTK_MENU_ITEM (menu_item)->submenu;

              if (submenu == NULL)
                {
                  gtk_menu_shell_activate_item (menu_shell, menu_item, TRUE);

                  deactivate = FALSE;
                }
              else if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement != GTK_TOP_BOTTOM ||
                       priv->activated_submenu)
                {
                  gint popdown_delay;
                  GTimeVal *popup_time;
                  gint64 usec_since_popup = 0;

                  g_object_get (gtk_widget_get_settings (widget),
                                "gtk-menu-popdown-delay", &popdown_delay,
                                NULL);

                  popup_time = g_object_get_data (G_OBJECT (submenu),
                                                  "gtk-menu-exact-popup-time");

                  if (popup_time)
                    {
                      GTimeVal current_time;

                      g_get_current_time (&current_time);

                      usec_since_popup = ((gint64) current_time.tv_sec * 1000 * 1000 +
                                          (gint64) current_time.tv_usec -
                                          (gint64) popup_time->tv_sec * 1000 * 1000 -
                                          (gint64) popup_time->tv_usec);

                      g_object_set_data (G_OBJECT (submenu),
                                         "gtk-menu-exact-popup-time", NULL);
                    }

                  /*  only close the submenu on click if we opened the
                   *  menu explicitely (usec_since_popup == 0) or
                   *  enough time has passed since it was opened by
                   *  GtkMenuItem's timeout (usec_since_popup > delay).
                   */
                  if (!priv->activated_submenu &&
                      (usec_since_popup == 0 ||
                       usec_since_popup > popdown_delay * 1000))
                    {
                      _gtk_menu_item_popdown_submenu (menu_item);
                    }
                  else
                    {
                      gtk_menu_item_select (GTK_MENU_ITEM (menu_item));
                    }

                  deactivate = FALSE;
                }
            }
          else if (menu_item &&
                   !_gtk_menu_item_is_selectable (menu_item) &&
                   GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement != GTK_TOP_BOTTOM)
            {
              deactivate = FALSE;
            }
          else if (menu_shell->parent_menu_shell)
            {
              menu_shell->active = TRUE;
              gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
              deactivate = FALSE;
            }

          /* If we ended up on an item with a submenu, leave the menu up.
           */
          if (menu_item && (menu_shell->active_menu_item == menu_item) &&
              GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement != GTK_TOP_BOTTOM)
            {
              deactivate = FALSE;
            }
        }
      else /* a very fast press-release */
        {
          /* We only ever want to prevent deactivation on the first
           * press/release. Setting the time to zero is a bit of a
           * hack, since we could be being triggered in the first
           * few fractions of a second after a server time wraparound.
           * the chances of that happening are ~1/10^6, without
           * serious harm if we lose.
           */
          menu_shell->activate_time = 0;
          deactivate = FALSE;
        }

      if (deactivate)
        {
          gtk_menu_shell_deactivate (menu_shell);
          g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
        }

      priv->activated_submenu = FALSE;
    }

  return TRUE;
}

void
_gtk_menu_shell_set_keyboard_mode (GtkMenuShell *menu_shell,
                                   gboolean      keyboard_mode)
{
  menu_shell->keyboard_mode = keyboard_mode;
}

gboolean
_gtk_menu_shell_get_keyboard_mode (GtkMenuShell *menu_shell)
{
  return menu_shell->keyboard_mode;
}

void
_gtk_menu_shell_update_mnemonics (GtkMenuShell *menu_shell)
{
  GtkMenuShell *target;
  gboolean auto_mnemonics;
  gboolean found;
  gboolean mnemonics_visible;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu_shell)),
                "gtk-auto-mnemonics", &auto_mnemonics, NULL);

  if (!auto_mnemonics)
    return;

  target = menu_shell;
  found = FALSE;
  while (target)
    {
      GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (target);
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (target));

      /* The idea with keyboard mode is that once you start using
       * the keyboard to navigate the menus, we show mnemonics
       * until the menu navigation is over. To that end, we spread
       * the keyboard mode upwards in the menu hierarchy here.
       * Also see gtk_menu_popup, where we inherit it downwards.
       */
      if (menu_shell->keyboard_mode)
        target->keyboard_mode = TRUE;

      /* While navigating menus, the first parent menu with an active
       * item is the one where mnemonics are effective, as can be seen
       * in gtk_menu_shell_key_press below.
       * We also show mnemonics in context menus. The grab condition is
       * necessary to ensure we remove underlines from menu bars when
       * dismissing menus.
       */
      mnemonics_visible = target->keyboard_mode &&
                          (((target->active_menu_item || priv->in_unselectable_item) && !found) ||
                           (target == menu_shell &&
                            !target->parent_menu_shell &&
                            gtk_widget_has_grab (GTK_WIDGET (target))));

      /* While menus are up, only show underlines inside the menubar,
       * not in the entire window.
       */
      if (GTK_IS_MENU_BAR (target))
        {
          gtk_window_set_mnemonics_visible (GTK_WINDOW (toplevel), FALSE);
          _gtk_label_mnemonics_visible_apply_recursively (GTK_WIDGET (target),
                                                          mnemonics_visible);
        }
      else
        gtk_window_set_mnemonics_visible (GTK_WINDOW (toplevel), mnemonics_visible);

      if (target->active_menu_item || priv->in_unselectable_item)
        found = TRUE;

      target = GTK_MENU_SHELL (target->parent_menu_shell);
    }
}

static gint
gtk_menu_shell_key_press (GtkWidget   *widget,
			  GdkEventKey *event)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);
  GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);
  gboolean enable_mnemonics;

  menu_shell->keyboard_mode = TRUE;

  if (!(menu_shell->active_menu_item || priv->in_unselectable_item) && menu_shell->parent_menu_shell)
    return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent *)event);

  if (gtk_bindings_activate_event (GTK_OBJECT (widget), event))
    return TRUE;

  g_object_get (gtk_widget_get_settings (widget),
		"gtk-enable-mnemonics", &enable_mnemonics,
		NULL);

  if (enable_mnemonics)
    return gtk_menu_shell_activate_mnemonic (menu_shell, event);

  return FALSE;
}

static gint
gtk_menu_shell_enter_notify (GtkWidget        *widget,
			     GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);

  if (event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_UNGRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return TRUE;

  if (menu_shell->active)
    {
      GtkWidget *menu_item;

      menu_item = gtk_get_event_widget ((GdkEvent*) event);

      if (!menu_item)
        return TRUE;

      if (GTK_IS_MENU_ITEM (menu_item) &&
          !_gtk_menu_item_is_selectable (menu_item))
        {
          GtkMenuShellPrivate *priv;

          priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);
          priv->in_unselectable_item = TRUE;

          return TRUE;
        }

      if (menu_item->parent == widget &&
	  GTK_IS_MENU_ITEM (menu_item))
	{
	  if (menu_shell->ignore_enter)
	    return TRUE;

	  if (event->detail != GDK_NOTIFY_INFERIOR)
            {
	      if (gtk_widget_get_state (menu_item) != GTK_STATE_PRELIGHT)
                gtk_menu_shell_select_item (menu_shell, menu_item);

              /* If any mouse button is down, and there is a submenu
               * that is not yet visible, activate it. It's sufficient
               * to check for any button's mask (not only the one
               * matching menu_shell->button), because there is no
               * situation a mouse button could be pressed while
               * entering a menu item where we wouldn't want to show
               * its submenu.
               */
              if ((event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) &&
                  GTK_MENU_ITEM (menu_item)->submenu != NULL)
                {
                  GtkMenuShellPrivate *priv;

                  priv = GTK_MENU_SHELL_GET_PRIVATE (menu_item->parent);
                  priv->activated_submenu = TRUE;

                  if (!gtk_widget_get_visible (GTK_MENU_ITEM (menu_item)->submenu))
                    {
                      gboolean touchscreen_mode;

                      g_object_get (gtk_widget_get_settings (widget),
                                    "gtk-touchscreen-mode", &touchscreen_mode,
                                    NULL);

                      if (touchscreen_mode)
                        _gtk_menu_item_popup_submenu (menu_item, TRUE);
                    }
                }
	    }
	}
      else if (menu_shell->parent_menu_shell)
	{
	  gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}
    }

  return TRUE;
}

static gint
gtk_menu_shell_leave_notify (GtkWidget        *widget,
			     GdkEventCrossing *event)
{
  if (event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_GTK_GRAB ||
      event->mode == GDK_CROSSING_STATE_CHANGED)
    return TRUE;

  if (gtk_widget_get_visible (widget))
    {
      GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);
      GtkWidget *event_widget = gtk_get_event_widget ((GdkEvent*) event);
      GtkMenuItem *menu_item;

      if (!event_widget || !GTK_IS_MENU_ITEM (event_widget))
	return TRUE;

      menu_item = GTK_MENU_ITEM (event_widget);

      if (!_gtk_menu_item_is_selectable (event_widget))
        {
          GtkMenuShellPrivate *priv;

          priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);
          priv->in_unselectable_item = TRUE;

          return TRUE;
        }

      if ((menu_shell->active_menu_item == event_widget) &&
	  (menu_item->submenu == NULL))
	{
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (gtk_widget_get_state (GTK_WIDGET (menu_item)) != GTK_STATE_NORMAL))
	    {
	      gtk_menu_shell_deselect (menu_shell);
	    }
	}
      else if (menu_shell->parent_menu_shell)
	{
	  gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}
    }

  return TRUE;
}

static void
gtk_menu_shell_screen_changed (GtkWidget *widget,
			       GdkScreen *previous_screen)
{
  gtk_menu_shell_reset_key_hash (GTK_MENU_SHELL (widget));
}

static void
gtk_menu_shell_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (container), widget);
}

static void
gtk_menu_shell_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (container);
  gint was_visible;

  was_visible = gtk_widget_get_visible (widget);
  menu_shell->children = g_list_remove (menu_shell->children, widget);
  
  if (widget == menu_shell->active_menu_item)
    {
      gtk_item_deselect (GTK_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }

  gtk_widget_unparent (widget);
  
  /* queue resize regardless of gtk_widget_get_visible (container),
   * since that's what is needed by toplevels.
   */
  if (was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_menu_shell_forall (GtkContainer *container,
		       gboolean      include_internals,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  GtkMenuShell *menu_shell = GTK_MENU_SHELL (container);
  GtkWidget *child;
  GList *children;

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child, callback_data);
    }
}


static void
gtk_real_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  if (menu_shell->active)
    {
      menu_shell->button = 0;
      menu_shell->active = FALSE;
      menu_shell->activate_time = 0;

      if (menu_shell->active_menu_item)
	{
	  gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
	  menu_shell->active_menu_item = NULL;
	}

      if (menu_shell->have_grab)
	{
	  menu_shell->have_grab = FALSE;
	  gtk_grab_remove (GTK_WIDGET (menu_shell));
	}
      if (menu_shell->have_xgrab)
	{
	  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (menu_shell));

	  menu_shell->have_xgrab = FALSE;
	  gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
	  gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
	}

      menu_shell->keyboard_mode = FALSE;

      _gtk_menu_shell_update_mnemonics (menu_shell);
    }
}

static gint
gtk_menu_shell_is_item (GtkMenuShell *menu_shell,
			GtkWidget    *child)
{
  GtkWidget *parent;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), FALSE);
  g_return_val_if_fail (child != NULL, FALSE);

  parent = child->parent;
  while (GTK_IS_MENU_SHELL (parent))
    {
      if (parent == (GtkWidget*) menu_shell)
	return TRUE;
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }

  return FALSE;
}

static GtkWidget*
gtk_menu_shell_get_item (GtkMenuShell *menu_shell,
			 GdkEvent     *event)
{
  GtkWidget *menu_item;

  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  
  while (menu_item && !GTK_IS_MENU_ITEM (menu_item))
    menu_item = menu_item->parent;

  if (menu_item && gtk_menu_shell_is_item (menu_shell, menu_item))
    return menu_item;
  else
    return NULL;
}

/* Handlers for action signals */

void
gtk_menu_shell_select_item (GtkMenuShell *menu_shell,
			    GtkWidget    *menu_item)
{
  GtkMenuShellClass *class;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  class = GTK_MENU_SHELL_GET_CLASS (menu_shell);

  if (class->select_item &&
      !(menu_shell->active &&
	menu_shell->active_menu_item == menu_item))
    class->select_item (menu_shell, menu_item);
}

void _gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
				   GtkSubmenuPlacement  placement);

static void
gtk_menu_shell_real_select_item (GtkMenuShell *menu_shell,
				 GtkWidget    *menu_item)
{
  GtkPackDirection pack_dir = PACK_DIRECTION (menu_shell);

  if (menu_shell->active_menu_item)
    {
      gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }

  if (!_gtk_menu_item_is_selectable (menu_item))
    {
      GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

      priv->in_unselectable_item = TRUE;
      _gtk_menu_shell_update_mnemonics (menu_shell);

      return;
    }

  gtk_menu_shell_activate (menu_shell);

  menu_shell->active_menu_item = menu_item;
  if (pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT)
    _gtk_menu_item_set_placement (GTK_MENU_ITEM (menu_shell->active_menu_item),
				  GTK_LEFT_RIGHT);
  else
    _gtk_menu_item_set_placement (GTK_MENU_ITEM (menu_shell->active_menu_item),
				  GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement);
  gtk_menu_item_select (GTK_MENU_ITEM (menu_shell->active_menu_item));

  _gtk_menu_shell_update_mnemonics (menu_shell);

  /* This allows the bizarre radio buttons-with-submenus-display-history
   * behavior
   */
  if (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
    gtk_widget_activate (menu_shell->active_menu_item);
}

void
gtk_menu_shell_deselect (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (menu_shell->active_menu_item)
    {
      gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
      _gtk_menu_shell_update_mnemonics (menu_shell);
    }
}

void
gtk_menu_shell_activate_item (GtkMenuShell      *menu_shell,
			      GtkWidget         *menu_item,
			      gboolean           force_deactivate)
{
  GSList *slist, *shells = NULL;
  gboolean deactivate = force_deactivate;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (!deactivate)
    deactivate = GTK_MENU_ITEM_GET_CLASS (menu_item)->hide_on_activate;

  g_object_ref (menu_shell);
  g_object_ref (menu_item);

  if (deactivate)
    {
      GtkMenuShell *parent_menu_shell = menu_shell;

      do
	{
	  g_object_ref (parent_menu_shell);
	  shells = g_slist_prepend (shells, parent_menu_shell);
	  parent_menu_shell = (GtkMenuShell*) parent_menu_shell->parent_menu_shell;
	}
      while (parent_menu_shell);
      shells = g_slist_reverse (shells);

      gtk_menu_shell_deactivate (menu_shell);
  
      /* flush the x-queue, so any grabs are removed and
       * the menu is actually taken down
       */
      gdk_display_sync (gtk_widget_get_display (menu_item));
    }

  gtk_widget_activate (menu_item);

  for (slist = shells; slist; slist = slist->next)
    {
      g_signal_emit (slist->data, menu_shell_signals[SELECTION_DONE], 0);
      g_object_unref (slist->data);
    }
  g_slist_free (shells);

  g_object_unref (menu_shell);
  g_object_unref (menu_item);
}

/* Distance should be +/- 1 */
static gboolean
gtk_menu_shell_real_move_selected (GtkMenuShell  *menu_shell, 
				   gint           distance)
{
  if (menu_shell->active_menu_item)
    {
      GList *node = g_list_find (menu_shell->children,
				 menu_shell->active_menu_item);
      GList *start_node = node;
      gboolean wrap_around;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu_shell)),
                    "gtk-keynav-wrap-around", &wrap_around,
                    NULL);

      if (distance > 0)
	{
	  node = node->next;
	  while (node != start_node && 
		 (!node || !_gtk_menu_item_is_selectable (node->data)))
	    {
	      if (node)
		node = node->next;
              else if (wrap_around)
		node = menu_shell->children;
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (menu_shell));
                  break;
                }
	    }
	}
      else
	{
	  node = node->prev;
	  while (node != start_node &&
		 (!node || !_gtk_menu_item_is_selectable (node->data)))
	    {
	      if (node)
		node = node->prev;
              else if (wrap_around)
		node = g_list_last (menu_shell->children);
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (menu_shell));
                  break;
                }
	    }
	}
      
      if (node)
	gtk_menu_shell_select_item (menu_shell, node->data);
    }

  return TRUE;
}

/* Distance should be +/- 1 */
static void
gtk_menu_shell_move_selected (GtkMenuShell  *menu_shell, 
			      gint           distance)
{
  gboolean handled = FALSE;

  g_signal_emit (menu_shell, menu_shell_signals[MOVE_SELECTED], 0,
		 distance, &handled);
}

/**
 * gtk_menu_shell_select_first:
 * @menu_shell: a #GtkMenuShell
 * @search_sensitive: if %TRUE, search for the first selectable
 *                    menu item, otherwise select nothing if
 *                    the first item isn't sensitive. This
 *                    should be %FALSE if the menu is being
 *                    popped up initially.
 * 
 * Select the first visible or selectable child of the menu shell;
 * don't select tearoff items unless the only item is a tearoff
 * item.
 *
 * Since: 2.2
 **/
void
gtk_menu_shell_select_first (GtkMenuShell *menu_shell,
			     gboolean      search_sensitive)
{
  GtkWidget *to_select = NULL;
  GList *tmp_list;

  tmp_list = menu_shell->children;
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;
      
      if ((!search_sensitive && gtk_widget_get_visible (child)) ||
	  _gtk_menu_item_is_selectable (child))
	{
	  to_select = child;
	  if (!GTK_IS_TEAROFF_MENU_ITEM (child))
	    break;
	}
      
      tmp_list = tmp_list->next;
    }

  if (to_select)
    gtk_menu_shell_select_item (menu_shell, to_select);
}

void
_gtk_menu_shell_select_last (GtkMenuShell *menu_shell,
			     gboolean      search_sensitive)
{
  GtkWidget *to_select = NULL;
  GList *tmp_list;

  tmp_list = g_list_last (menu_shell->children);
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;
      
      if ((!search_sensitive && gtk_widget_get_visible (child)) ||
	  _gtk_menu_item_is_selectable (child))
	{
	  to_select = child;
	  if (!GTK_IS_TEAROFF_MENU_ITEM (child))
	    break;
	}
      
      tmp_list = tmp_list->prev;
    }

  if (to_select)
    gtk_menu_shell_select_item (menu_shell, to_select);
}

static gboolean
gtk_menu_shell_select_submenu_first (GtkMenuShell     *menu_shell)
{
  GtkMenuItem *menu_item;

  if (menu_shell->active_menu_item == NULL)
    return FALSE;

  menu_item = GTK_MENU_ITEM (menu_shell->active_menu_item); 
  
  if (menu_item->submenu)
    {
      _gtk_menu_item_popup_submenu (GTK_WIDGET (menu_item), FALSE);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (menu_item->submenu), TRUE);
      if (GTK_MENU_SHELL (menu_item->submenu)->active_menu_item)
	return TRUE;
    }

  return FALSE;
}

static void
gtk_real_menu_shell_move_current (GtkMenuShell         *menu_shell,
				  GtkMenuDirectionType  direction)
{
  GtkMenuShellPrivate *priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);
  GtkMenuShell *parent_menu_shell = NULL;
  gboolean had_selection;
  gboolean touchscreen_mode;

  priv->in_unselectable_item = FALSE;

  had_selection = menu_shell->active_menu_item != NULL;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu_shell)),
                "gtk-touchscreen-mode", &touchscreen_mode,
                NULL);

  if (menu_shell->parent_menu_shell)
    parent_menu_shell = GTK_MENU_SHELL (menu_shell->parent_menu_shell);

  switch (direction)
    {
    case GTK_MENU_DIR_PARENT:
      if (touchscreen_mode &&
          menu_shell->active_menu_item &&
          GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu &&
          gtk_widget_get_visible (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu))
        {
          /* if we are on a menu item that has an open submenu but the
           * focus is not in that submenu (e.g. because it's empty or
           * has only insensitive items), close that submenu instead
           * of running into the code below which would close *this*
           * menu.
           */
          _gtk_menu_item_popdown_submenu (menu_shell->active_menu_item);
          _gtk_menu_shell_update_mnemonics (menu_shell);
        }
      else if (parent_menu_shell)
	{
          if (touchscreen_mode)
            {
              /* close menu when returning from submenu. */
              _gtk_menu_item_popdown_submenu (GTK_MENU (menu_shell)->parent_menu_item);
              _gtk_menu_shell_update_mnemonics (parent_menu_shell);
              break;
            }

	  if (GTK_MENU_SHELL_GET_CLASS (parent_menu_shell)->submenu_placement ==
              GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement)
	    gtk_menu_shell_deselect (menu_shell);
	  else
	    {
	      if (PACK_DIRECTION (parent_menu_shell) == GTK_PACK_DIRECTION_LTR)
		gtk_menu_shell_move_selected (parent_menu_shell, -1);
	      else
		gtk_menu_shell_move_selected (parent_menu_shell, 1);
	      gtk_menu_shell_select_submenu_first (parent_menu_shell);
	    }
	}
      /* If there is no parent and the submenu is in the opposite direction
       * to the menu, then make the PARENT direction wrap around to
       * the bottom of the submenu.
       */
      else if (menu_shell->active_menu_item &&
	       _gtk_menu_item_is_selectable (menu_shell->active_menu_item) &&
	       GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
	{
	  GtkMenuShell *submenu = GTK_MENU_SHELL (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu);

	  if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement !=
	      GTK_MENU_SHELL_GET_CLASS (submenu)->submenu_placement)
	    _gtk_menu_shell_select_last (submenu, TRUE);
	}
      break;

    case GTK_MENU_DIR_CHILD:
      if (menu_shell->active_menu_item &&
	  _gtk_menu_item_is_selectable (menu_shell->active_menu_item) &&
	  GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
	{
	  if (gtk_menu_shell_select_submenu_first (menu_shell))
	    break;
	}

      /* Try to find a menu running the opposite direction */
      while (parent_menu_shell &&
	     (GTK_MENU_SHELL_GET_CLASS (parent_menu_shell)->submenu_placement ==
	      GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement))
	{
	  parent_menu_shell = GTK_MENU_SHELL (parent_menu_shell->parent_menu_shell);
	}

      if (parent_menu_shell)
	{
	  if (PACK_DIRECTION (parent_menu_shell) == GTK_PACK_DIRECTION_LTR)
	    gtk_menu_shell_move_selected (parent_menu_shell, 1);
	  else
	    gtk_menu_shell_move_selected (parent_menu_shell, -1);

	  gtk_menu_shell_select_submenu_first (parent_menu_shell);
	}
      break;

    case GTK_MENU_DIR_PREV:
      gtk_menu_shell_move_selected (menu_shell, -1);
      if (!had_selection &&
	  !menu_shell->active_menu_item &&
	  menu_shell->children)
	_gtk_menu_shell_select_last (menu_shell, TRUE);
      break;

    case GTK_MENU_DIR_NEXT:
      gtk_menu_shell_move_selected (menu_shell, 1);
      if (!had_selection &&
	  !menu_shell->active_menu_item &&
	  menu_shell->children)
	gtk_menu_shell_select_first (menu_shell, TRUE);
      break;
    }
}

static void
gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
				      gboolean           force_hide)
{
  if (menu_shell->active_menu_item &&
      _gtk_menu_item_is_selectable (menu_shell->active_menu_item))
  {
    if (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu == NULL)
      gtk_menu_shell_activate_item (menu_shell,
				    menu_shell->active_menu_item,
				    force_hide);
    else
      gtk_menu_shell_select_submenu_first (menu_shell);
  }
}

static void
gtk_real_menu_shell_cancel (GtkMenuShell      *menu_shell)
{
  /* Unset the active menu item so gtk_menu_popdown() doesn't see it.
   */
  gtk_menu_shell_deselect (menu_shell);
  
  gtk_menu_shell_deactivate (menu_shell);
  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
}

static void
gtk_real_menu_shell_cycle_focus (GtkMenuShell      *menu_shell,
				 GtkDirectionType   dir)
{
  while (menu_shell && !GTK_IS_MENU_BAR (menu_shell))
    {
      if (menu_shell->parent_menu_shell)
	menu_shell = GTK_MENU_SHELL (menu_shell->parent_menu_shell);
      else
	menu_shell = NULL;
    }

  if (menu_shell)
    _gtk_menu_bar_cycle_focus (GTK_MENU_BAR (menu_shell), dir);
}

gint
_gtk_menu_shell_get_popup_delay (GtkMenuShell *menu_shell)
{
  GtkMenuShellClass *klass = GTK_MENU_SHELL_GET_CLASS (menu_shell);
  
  if (klass->get_popup_delay)
    {
      return klass->get_popup_delay (menu_shell);
    }
  else
    {
      gint popup_delay;
      GtkWidget *widget = GTK_WIDGET (menu_shell);
      
      g_object_get (gtk_widget_get_settings (widget),
		    "gtk-menu-popup-delay", &popup_delay,
		    NULL);
      
      return popup_delay;
    }
}

/**
 * gtk_menu_shell_cancel:
 * @menu_shell: a #GtkMenuShell
 * 
 * Cancels the selection within the menu shell.  
 * 
 * Since: 2.4
 */
void
gtk_menu_shell_cancel (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  g_signal_emit (menu_shell, menu_shell_signals[CANCEL], 0);
}

static GtkMnemonicHash *
gtk_menu_shell_get_mnemonic_hash (GtkMenuShell *menu_shell,
				  gboolean      create)
{
  GtkMenuShellPrivate *private = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

  if (!private->mnemonic_hash && create)
    private->mnemonic_hash = _gtk_mnemonic_hash_new ();
  
  return private->mnemonic_hash;
}

static void
menu_shell_add_mnemonic_foreach (guint    keyval,
				 GSList  *targets,
				 gpointer data)
{
  GtkKeyHash *key_hash = data;

  _gtk_key_hash_add_entry (key_hash, keyval, 0, GUINT_TO_POINTER (keyval));
}

static GtkKeyHash *
gtk_menu_shell_get_key_hash (GtkMenuShell *menu_shell,
			     gboolean      create)
{
  GtkMenuShellPrivate *private = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);
  GtkWidget *widget = GTK_WIDGET (menu_shell);

  if (!private->key_hash && create && gtk_widget_has_screen (widget))
    {
      GtkMnemonicHash *mnemonic_hash = gtk_menu_shell_get_mnemonic_hash (menu_shell, FALSE);
      GdkScreen *screen = gtk_widget_get_screen (widget);
      GdkKeymap *keymap = gdk_keymap_get_for_display (gdk_screen_get_display (screen));

      if (!mnemonic_hash)
	return NULL;
      
      private->key_hash = _gtk_key_hash_new (keymap, NULL);

      _gtk_mnemonic_hash_foreach (mnemonic_hash,
				  menu_shell_add_mnemonic_foreach,
				  private->key_hash);
    }
  
  return private->key_hash;
}

static void
gtk_menu_shell_reset_key_hash (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *private = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

  if (private->key_hash)
    {
      _gtk_key_hash_free (private->key_hash);
      private->key_hash = NULL;
    }
}

static gboolean
gtk_menu_shell_activate_mnemonic (GtkMenuShell *menu_shell,
				  GdkEventKey  *event)
{
  GtkMnemonicHash *mnemonic_hash;
  GtkKeyHash *key_hash;
  GSList *entries;
  gboolean result = FALSE;

  mnemonic_hash = gtk_menu_shell_get_mnemonic_hash (menu_shell, FALSE);
  if (!mnemonic_hash)
    return FALSE;

  key_hash = gtk_menu_shell_get_key_hash (menu_shell, TRUE);
  if (!key_hash)
    return FALSE;
  
  entries = _gtk_key_hash_lookup (key_hash,
				  event->hardware_keycode,
				  event->state,
				  gtk_accelerator_get_default_mod_mask (),
				  event->group);

  if (entries)
    result = _gtk_mnemonic_hash_activate (mnemonic_hash,
					  GPOINTER_TO_UINT (entries->data));

  return result;
}

void
_gtk_menu_shell_add_mnemonic (GtkMenuShell *menu_shell,
			      guint      keyval,
			      GtkWidget *target)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_WIDGET (target));

  _gtk_mnemonic_hash_add (gtk_menu_shell_get_mnemonic_hash (menu_shell, TRUE),
			  keyval, target);
  gtk_menu_shell_reset_key_hash (menu_shell);
}

void
_gtk_menu_shell_remove_mnemonic (GtkMenuShell *menu_shell,
				 guint      keyval,
				 GtkWidget *target)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_WIDGET (target));
  
  _gtk_mnemonic_hash_remove (gtk_menu_shell_get_mnemonic_hash (menu_shell, TRUE),
			     keyval, target);
  gtk_menu_shell_reset_key_hash (menu_shell);
}

/**
 * gtk_menu_shell_get_take_focus:
 * @menu_shell: a #GtkMenuShell
 *
 * Returns %TRUE if the menu shell will take the keyboard focus on popup.
 *
 * Returns: %TRUE if the menu shell will take the keyboard focus on popup.
 *
 * Since: 2.8
 **/
gboolean
gtk_menu_shell_get_take_focus (GtkMenuShell *menu_shell)
{
  GtkMenuShellPrivate *priv;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), FALSE);

  priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

  return priv->take_focus;
}

/**
 * gtk_menu_shell_set_take_focus:
 * @menu_shell: a #GtkMenuShell
 * @take_focus: %TRUE if the menu shell should take the keyboard focus on popup.
 *
 * If @take_focus is %TRUE (the default) the menu shell will take the keyboard 
 * focus so that it will receive all keyboard events which is needed to enable
 * keyboard navigation in menus.
 *
 * Setting @take_focus to %FALSE is useful only for special applications
 * like virtual keyboard implementations which should not take keyboard
 * focus.
 *
 * The @take_focus state of a menu or menu bar is automatically propagated
 * to submenus whenever a submenu is popped up, so you don't have to worry
 * about recursively setting it for your entire menu hierarchy. Only when
 * programmatically picking a submenu and popping it up manually, the
 * @take_focus property of the submenu needs to be set explicitely.
 *
 * Note that setting it to %FALSE has side-effects:
 *
 * If the focus is in some other app, it keeps the focus and keynav in
 * the menu doesn't work. Consequently, keynav on the menu will only
 * work if the focus is on some toplevel owned by the onscreen keyboard.
 *
 * To avoid confusing the user, menus with @take_focus set to %FALSE
 * should not display mnemonics or accelerators, since it cannot be
 * guaranteed that they will work.
 *
 * See also gdk_keyboard_grab()
 *
 * Since: 2.8
 **/
void
gtk_menu_shell_set_take_focus (GtkMenuShell *menu_shell,
                               gboolean      take_focus)
{
  GtkMenuShellPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  priv = GTK_MENU_SHELL_GET_PRIVATE (menu_shell);

  if (priv->take_focus != take_focus)
    {
      priv->take_focus = take_focus;
      g_object_notify (G_OBJECT (menu_shell), "take-focus");
    }
}

#define __GTK_MENU_SHELL_C__
#include "gtkaliasdef.c"
