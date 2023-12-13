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
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtksettings.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkalias.h"


#define BORDER_SPACING  0
#define DEFAULT_IPADDING 1

/* Properties */
enum {
  PROP_0,
  PROP_PACK_DIRECTION,
  PROP_CHILD_PACK_DIRECTION
};

typedef struct _GtkMenuBarPrivate GtkMenuBarPrivate;
struct _GtkMenuBarPrivate
{
  GtkPackDirection pack_direction;
  GtkPackDirection child_pack_direction;
};

#define GTK_MENU_BAR_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_MENU_BAR, GtkMenuBarPrivate))


static void gtk_menu_bar_set_property      (GObject             *object,
					    guint                prop_id,
					    const GValue        *value,
					    GParamSpec          *pspec);
static void gtk_menu_bar_get_property      (GObject             *object,
					    guint                prop_id,
					    GValue              *value,
					    GParamSpec          *pspec);
static void gtk_menu_bar_size_request      (GtkWidget       *widget,
					    GtkRequisition  *requisition);
static void gtk_menu_bar_size_allocate     (GtkWidget       *widget,
					    GtkAllocation   *allocation);
static void gtk_menu_bar_paint             (GtkWidget       *widget,
					    GdkRectangle    *area);
static gint gtk_menu_bar_expose            (GtkWidget       *widget,
					    GdkEventExpose  *event);
static void gtk_menu_bar_hierarchy_changed (GtkWidget       *widget,
					    GtkWidget       *old_toplevel);
static gint gtk_menu_bar_get_popup_delay   (GtkMenuShell    *menu_shell);
static void gtk_menu_bar_move_current      (GtkMenuShell     *menu_shell,
                                            GtkMenuDirectionType direction);

static GtkShadowType get_shadow_type   (GtkMenuBar      *menubar);

G_DEFINE_TYPE (GtkMenuBar, gtk_menu_bar, GTK_TYPE_MENU_SHELL)

static void
gtk_menu_bar_class_init (GtkMenuBarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkMenuShellClass *menu_shell_class;

  GtkBindingSet *binding_set;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  menu_shell_class = (GtkMenuShellClass*) class;

  gobject_class->get_property = gtk_menu_bar_get_property;
  gobject_class->set_property = gtk_menu_bar_set_property;

  widget_class->size_request = gtk_menu_bar_size_request;
  widget_class->size_allocate = gtk_menu_bar_size_allocate;
  widget_class->expose_event = gtk_menu_bar_expose;
  widget_class->hierarchy_changed = gtk_menu_bar_hierarchy_changed;
  
  menu_shell_class->submenu_placement = GTK_TOP_BOTTOM;
  menu_shell_class->get_popup_delay = gtk_menu_bar_get_popup_delay;
  menu_shell_class->move_current = gtk_menu_bar_move_current;

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Left, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Left, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Right, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Right, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Up, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Up, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Down, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Down, 0,
				"move-current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);

  /**
   * GtkMenuBar:pack-direction:
   *
   * The pack direction of the menubar. It determines how
   * menuitems are arranged in the menubar.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
				   PROP_PACK_DIRECTION,
				   g_param_spec_enum ("pack-direction",
 						      P_("Pack direction"),
 						      P_("The pack direction of the menubar"),
 						      GTK_TYPE_PACK_DIRECTION,
 						      GTK_PACK_DIRECTION_LTR,
 						      GTK_PARAM_READWRITE));
  
  /**
   * GtkMenuBar:child-pack-direction:
   *
   * The child pack direction of the menubar. It determines how
   * the widgets contained in child menuitems are arranged.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
				   PROP_CHILD_PACK_DIRECTION,
				   g_param_spec_enum ("child-pack-direction",
 						      P_("Child Pack direction"),
 						      P_("The child pack direction of the menubar"),
 						      GTK_TYPE_PACK_DIRECTION,
 						      GTK_PACK_DIRECTION_LTR,
 						      GTK_PARAM_READWRITE));
  

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("shadow-type",
                                                              P_("Shadow type"),
                                                              P_("Style of bevel around the menubar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal-padding",
							     P_("Internal padding"),
							     P_("Amount of border space between the menubar shadow and the menu items"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             GTK_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (GtkMenuBarPrivate));
}

static void
gtk_menu_bar_init (GtkMenuBar *object)
{
}

GtkWidget*
gtk_menu_bar_new (void)
{
  return g_object_new (GTK_TYPE_MENU_BAR, NULL);
}

static void
gtk_menu_bar_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (object);
  
  switch (prop_id)
    {
    case PROP_PACK_DIRECTION:
      gtk_menu_bar_set_pack_direction (menubar, g_value_get_enum (value));
      break;
    case PROP_CHILD_PACK_DIRECTION:
      gtk_menu_bar_set_child_pack_direction (menubar, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_bar_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (object);
  
  switch (prop_id)
    {
    case PROP_PACK_DIRECTION:
      g_value_set_enum (value, gtk_menu_bar_get_pack_direction (menubar));
      break;
    case PROP_CHILD_PACK_DIRECTION:
      g_value_set_enum (value, gtk_menu_bar_get_child_pack_direction (menubar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_bar_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkMenuBar *menu_bar;
  GtkMenuBarPrivate *priv;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  gint nchildren;
  GtkRequisition child_requisition;
  gint ipadding;

  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (requisition != NULL);

  requisition->width = 0;
  requisition->height = 0;
  
  if (gtk_widget_get_visible (widget))
    {
      menu_bar = GTK_MENU_BAR (widget);
      menu_shell = GTK_MENU_SHELL (widget);
      priv = GTK_MENU_BAR_GET_PRIVATE (menu_bar);

      nchildren = 0;
      children = menu_shell->children;

      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_get_visible (child))
	    {
              gint toggle_size;

	      GTK_MENU_ITEM (child)->show_submenu_indicator = FALSE;
	      gtk_widget_size_request (child, &child_requisition);
              gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
                                                 &toggle_size);

	      if (priv->child_pack_direction == GTK_PACK_DIRECTION_LTR ||
		  priv->child_pack_direction == GTK_PACK_DIRECTION_RTL)
		child_requisition.width += toggle_size;
	      else
		child_requisition.height += toggle_size;

              if (priv->pack_direction == GTK_PACK_DIRECTION_LTR ||
		  priv->pack_direction == GTK_PACK_DIRECTION_RTL)
		{
		  requisition->width += child_requisition.width;
		  requisition->height = MAX (requisition->height, child_requisition.height);
		}
	      else
		{
		  requisition->width = MAX (requisition->width, child_requisition.width);
		  requisition->height += child_requisition.height;
		}
	      nchildren += 1;
	    }
	}

      gtk_widget_style_get (widget, "internal-padding", &ipadding, NULL);
      
      requisition->width += (GTK_CONTAINER (menu_bar)->border_width +
                             ipadding + 
			     BORDER_SPACING) * 2;
      requisition->height += (GTK_CONTAINER (menu_bar)->border_width +
                              ipadding +
			      BORDER_SPACING) * 2;

      if (get_shadow_type (menu_bar) != GTK_SHADOW_NONE)
	{
	  requisition->width += widget->style->xthickness * 2;
	  requisition->height += widget->style->ythickness * 2;
	}
    }
}

static void
gtk_menu_bar_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkMenuBar *menu_bar;
  GtkMenuShell *menu_shell;
  GtkMenuBarPrivate *priv;
  GtkWidget *child;
  GList *children;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  guint offset;
  GtkTextDirection direction;
  gint ltr_x, ltr_y;
  gint ipadding;

  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (allocation != NULL);

  menu_bar = GTK_MENU_BAR (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  priv = GTK_MENU_BAR_GET_PRIVATE (menu_bar);

  direction = gtk_widget_get_direction (widget);

  widget->allocation = *allocation;
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  gtk_widget_style_get (widget, "internal-padding", &ipadding, NULL);
  
  if (menu_shell->children)
    {
      child_allocation.x = (GTK_CONTAINER (menu_bar)->border_width +
			    ipadding + 
			    BORDER_SPACING);
      child_allocation.y = (GTK_CONTAINER (menu_bar)->border_width +
			    BORDER_SPACING);
      
      if (get_shadow_type (menu_bar) != GTK_SHADOW_NONE)
	{
	  child_allocation.x += widget->style->xthickness;
	  child_allocation.y += widget->style->ythickness;
	}
      
      if (priv->pack_direction == GTK_PACK_DIRECTION_LTR ||
	  priv->pack_direction == GTK_PACK_DIRECTION_RTL)
	{
	  child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);
	  
	  offset = child_allocation.x; 	/* Window edge to menubar start */
	  ltr_x = child_allocation.x;
	  
	  children = menu_shell->children;
	  while (children)
	    {
	      gint toggle_size;          
	      
	      child = children->data;
	      children = children->next;
	      
	      gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
						 &toggle_size);
	      gtk_widget_get_child_requisition (child, &child_requisition);
	    
	      if (priv->child_pack_direction == GTK_PACK_DIRECTION_LTR ||
		  priv->child_pack_direction == GTK_PACK_DIRECTION_RTL)
		child_requisition.width += toggle_size;
	      else
		child_requisition.height += toggle_size;
	      
	      /* Support for the right justified help menu */
	      if ((children == NULL) && (GTK_IS_MENU_ITEM(child))
		  && (GTK_MENU_ITEM(child)->right_justify)) 
		{
		  ltr_x = allocation->width -
		    child_requisition.width - offset;
		}
	      if (gtk_widget_get_visible (child))
		{
		  if ((direction == GTK_TEXT_DIR_LTR) == (priv->pack_direction == GTK_PACK_DIRECTION_LTR))
		    child_allocation.x = ltr_x;
		  else
		    child_allocation.x = allocation->width -
		      child_requisition.width - ltr_x; 
		  
		  child_allocation.width = child_requisition.width;
		  
		  gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
						      toggle_size);
		  gtk_widget_size_allocate (child, &child_allocation);
		  
		  ltr_x += child_allocation.width;
		}
	    }
	}
      else
	{
	  child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2);
	  
	  offset = child_allocation.y; 	/* Window edge to menubar start */
	  ltr_y = child_allocation.y;
	  
	  children = menu_shell->children;
	  while (children)
	    {
	      gint toggle_size;          
	      
	      child = children->data;
	      children = children->next;
	      
	      gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child),
						 &toggle_size);
	      gtk_widget_get_child_requisition (child, &child_requisition);
	      
	      if (priv->child_pack_direction == GTK_PACK_DIRECTION_LTR ||
		  priv->child_pack_direction == GTK_PACK_DIRECTION_RTL)
		child_requisition.width += toggle_size;
	      else
		child_requisition.height += toggle_size;
	      
	      /* Support for the right justified help menu */
	      if ((children == NULL) && (GTK_IS_MENU_ITEM(child))
		  && (GTK_MENU_ITEM(child)->right_justify)) 
		{
		  ltr_y = allocation->height -
		    child_requisition.height - offset;
		}
	      if (gtk_widget_get_visible (child))
		{
		  if ((direction == GTK_TEXT_DIR_LTR) ==
		      (priv->pack_direction == GTK_PACK_DIRECTION_TTB))
		    child_allocation.y = ltr_y;
		  else
		    child_allocation.y = allocation->height -
		      child_requisition.height - ltr_y; 
		  child_allocation.height = child_requisition.height;
		  
		  gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
						      toggle_size);
		  gtk_widget_size_allocate (child, &child_allocation);
		  
		  ltr_y += child_allocation.height;
		}
	    }
	}
    }
}

static void
gtk_menu_bar_paint (GtkWidget    *widget,
                    GdkRectangle *area)
{
  g_return_if_fail (GTK_IS_MENU_BAR (widget));

  if (gtk_widget_is_drawable (widget))
    {
      gint border;

      border = GTK_CONTAINER (widget)->border_width;
      
      gtk_paint_box (widget->style,
		     widget->window,
                     gtk_widget_get_state (widget),
                     get_shadow_type (GTK_MENU_BAR (widget)),
		     area, widget, "menubar",
		     border, border,
		     widget->allocation.width - border * 2,
                     widget->allocation.height - border * 2);
    }
}

static gint
gtk_menu_bar_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_MENU_BAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gtk_widget_is_drawable (widget))
    {
      gtk_menu_bar_paint (widget, &event->area);

      GTK_WIDGET_CLASS (gtk_menu_bar_parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static GList *
get_menu_bars (GtkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "gtk-menu-bar-list");
}

static GList *
get_viewable_menu_bars (GtkWindow *window)
{
  GList *menu_bars;
  GList *viewable_menu_bars = NULL;

  for (menu_bars = get_menu_bars (window);
       menu_bars;
       menu_bars = menu_bars->next)
    {
      GtkWidget *widget = menu_bars->data;
      gboolean viewable = TRUE;
      
      while (widget)
	{
	  if (!gtk_widget_get_mapped (widget))
	    viewable = FALSE;
	  
	  widget = widget->parent;
	}

      if (viewable)
	viewable_menu_bars = g_list_prepend (viewable_menu_bars, menu_bars->data);
    }

  return g_list_reverse (viewable_menu_bars);
}

static void
set_menu_bars (GtkWindow *window,
	       GList     *menubars)
{
  g_object_set_data (G_OBJECT (window), I_("gtk-menu-bar-list"), menubars);
}

static gboolean
window_key_press_handler (GtkWidget   *widget,
                          GdkEventKey *event,
                          gpointer     data)
{
  gchar *accel = NULL;
  gboolean retval = FALSE;
  
  g_object_get (gtk_widget_get_settings (widget),
                "gtk-menu-bar-accel", &accel,
                NULL);

  if (accel && *accel)
    {
      guint keyval = 0;
      GdkModifierType mods = 0;

      gtk_accelerator_parse (accel, &keyval, &mods);

      if (keyval == 0)
        g_warning ("Failed to parse menu bar accelerator '%s'\n", accel);

      /* FIXME this is wrong, needs to be in the global accel resolution
       * thing, to properly consider i18n etc., but that probably requires
       * AccelGroup changes etc.
       */
      if (event->keyval == keyval &&
          ((event->state & gtk_accelerator_get_default_mod_mask ()) ==
	   (mods & gtk_accelerator_get_default_mod_mask ())))
        {
	  GList *tmp_menubars = get_viewable_menu_bars (GTK_WINDOW (widget));
	  GList *menubars;

	  menubars = _gtk_container_focus_sort (GTK_CONTAINER (widget), tmp_menubars,
						GTK_DIR_TAB_FORWARD, NULL);
	  g_list_free (tmp_menubars);
	  
	  if (menubars)
	    {
	      GtkMenuShell *menu_shell = GTK_MENU_SHELL (menubars->data);

              _gtk_menu_shell_set_keyboard_mode (menu_shell, TRUE);
	      gtk_menu_shell_select_first (menu_shell, FALSE);
	      
	      g_list_free (menubars);
	      
	      retval = TRUE;	      
	    }
        }
    }

  g_free (accel);

  return retval;
}

static void
add_to_window (GtkWindow  *window,
               GtkMenuBar *menubar)
{
  GList *menubars = get_menu_bars (window);

  if (!menubars)
    {
      g_signal_connect (window,
			"key-press-event",
			G_CALLBACK (window_key_press_handler),
			NULL);
    }

  set_menu_bars (window, g_list_prepend (menubars, menubar));
}

static void
remove_from_window (GtkWindow  *window,
                    GtkMenuBar *menubar)
{
  GList *menubars = get_menu_bars (window);

  menubars = g_list_remove (menubars, menubar);

  if (!menubars)
    {
      g_signal_handlers_disconnect_by_func (window,
					    window_key_press_handler,
					    NULL);
    }

  set_menu_bars (window, menubars);
}

static void
gtk_menu_bar_hierarchy_changed (GtkWidget *widget,
				GtkWidget *old_toplevel)
{
  GtkWidget *toplevel;  
  GtkMenuBar *menubar;

  menubar = GTK_MENU_BAR (widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (old_toplevel)
    remove_from_window (GTK_WINDOW (old_toplevel), menubar);
  
  if (gtk_widget_is_toplevel (toplevel))
    add_to_window (GTK_WINDOW (toplevel), menubar);
}

/**
 * _gtk_menu_bar_cycle_focus:
 * @menubar: a #GtkMenuBar
 * @dir: direction in which to cycle the focus
 * 
 * Move the focus between menubars in the toplevel.
 **/
void
_gtk_menu_bar_cycle_focus (GtkMenuBar       *menubar,
			   GtkDirectionType  dir)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menubar));
  GtkMenuItem *to_activate = NULL;

  if (gtk_widget_is_toplevel (toplevel))
    {
      GList *tmp_menubars = get_viewable_menu_bars (GTK_WINDOW (toplevel));
      GList *menubars;
      GList *current;

      menubars = _gtk_container_focus_sort (GTK_CONTAINER (toplevel), tmp_menubars,
					    dir, GTK_WIDGET (menubar));
      g_list_free (tmp_menubars);

      if (menubars)
	{
	  current = g_list_find (menubars, menubar);

	  if (current && current->next)
	    {
	      GtkMenuShell *new_menushell = GTK_MENU_SHELL (current->next->data);
	      if (new_menushell->children)
		to_activate = new_menushell->children->data;
	    }
	}
	  
      g_list_free (menubars);
    }

  gtk_menu_shell_cancel (GTK_MENU_SHELL (menubar));

  if (to_activate)
    g_signal_emit_by_name (to_activate, "activate_item");
}

static GtkShadowType
get_shadow_type (GtkMenuBar *menubar)
{
  GtkShadowType shadow_type = GTK_SHADOW_OUT;
  
  gtk_widget_style_get (GTK_WIDGET (menubar),
			"shadow-type", &shadow_type,
			NULL);

  return shadow_type;
}

static gint
gtk_menu_bar_get_popup_delay (GtkMenuShell *menu_shell)
{
  gint popup_delay;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (menu_shell)),
		"gtk-menu-bar-popup-delay", &popup_delay,
		NULL);

  return popup_delay;
}

static void
gtk_menu_bar_move_current (GtkMenuShell         *menu_shell,
			   GtkMenuDirectionType  direction)
{
  GtkMenuBar *menubar = GTK_MENU_BAR (menu_shell);
  GtkTextDirection text_dir;
  GtkPackDirection pack_dir;

  text_dir = gtk_widget_get_direction (GTK_WIDGET (menubar));
  pack_dir = gtk_menu_bar_get_pack_direction (menubar);
  
  if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
     {
      if ((text_dir == GTK_TEXT_DIR_RTL) == (pack_dir == GTK_PACK_DIRECTION_LTR))
	{
	  switch (direction) 
	    {      
	    case GTK_MENU_DIR_PREV:
	      direction = GTK_MENU_DIR_NEXT;
	      break;
	    case GTK_MENU_DIR_NEXT:
	      direction = GTK_MENU_DIR_PREV;
	      break;
	    default: ;
	    }
	}
    }
  else
    {
      switch (direction) 
	{
	case GTK_MENU_DIR_PARENT:
	  if ((text_dir == GTK_TEXT_DIR_LTR) == (pack_dir == GTK_PACK_DIRECTION_TTB))
	    direction = GTK_MENU_DIR_PREV;
	  else
	    direction = GTK_MENU_DIR_NEXT;
	  break;
	case GTK_MENU_DIR_CHILD:
	  if ((text_dir == GTK_TEXT_DIR_LTR) == (pack_dir == GTK_PACK_DIRECTION_TTB))
	    direction = GTK_MENU_DIR_NEXT;
	  else
	    direction = GTK_MENU_DIR_PREV;
	  break;
	case GTK_MENU_DIR_PREV:
	  if (text_dir == GTK_TEXT_DIR_RTL)	  
	    direction = GTK_MENU_DIR_CHILD;
	  else
	    direction = GTK_MENU_DIR_PARENT;
	  break;
	case GTK_MENU_DIR_NEXT:
	  if (text_dir == GTK_TEXT_DIR_RTL)	  
	    direction = GTK_MENU_DIR_PARENT;
	  else
	    direction = GTK_MENU_DIR_CHILD;
	  break;
	default: ;
	}
    }
  
  GTK_MENU_SHELL_CLASS (gtk_menu_bar_parent_class)->move_current (menu_shell, direction);
}

/**
 * gtk_menu_bar_get_pack_direction:
 * @menubar: a #GtkMenuBar
 * 
 * Retrieves the current pack direction of the menubar. 
 * See gtk_menu_bar_set_pack_direction().
 *
 * Return value: the pack direction
 *
 * Since: 2.8
 */
GtkPackDirection
gtk_menu_bar_get_pack_direction (GtkMenuBar *menubar)
{
  GtkMenuBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_MENU_BAR (menubar), 
			GTK_PACK_DIRECTION_LTR);
  
  priv = GTK_MENU_BAR_GET_PRIVATE (menubar);

  return priv->pack_direction;
}

/**
 * gtk_menu_bar_set_pack_direction:
 * @menubar: a #GtkMenuBar
 * @pack_dir: a new #GtkPackDirection
 * 
 * Sets how items should be packed inside a menubar.
 * 
 * Since: 2.8
 */
void
gtk_menu_bar_set_pack_direction (GtkMenuBar       *menubar,
                                 GtkPackDirection  pack_dir)
{
  GtkMenuBarPrivate *priv;
  GList *l;

  g_return_if_fail (GTK_IS_MENU_BAR (menubar));

  priv = GTK_MENU_BAR_GET_PRIVATE (menubar);

  if (priv->pack_direction != pack_dir)
    {
      priv->pack_direction = pack_dir;

      gtk_widget_queue_resize (GTK_WIDGET (menubar));

      for (l = GTK_MENU_SHELL (menubar)->children; l; l = l->next)
	gtk_widget_queue_resize (GTK_WIDGET (l->data));

      g_object_notify (G_OBJECT (menubar), "pack-direction");
    }
}

/**
 * gtk_menu_bar_get_child_pack_direction:
 * @menubar: a #GtkMenuBar
 * 
 * Retrieves the current child pack direction of the menubar.
 * See gtk_menu_bar_set_child_pack_direction().
 *
 * Return value: the child pack direction
 *
 * Since: 2.8
 */
GtkPackDirection
gtk_menu_bar_get_child_pack_direction (GtkMenuBar *menubar)
{
  GtkMenuBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_MENU_BAR (menubar), 
			GTK_PACK_DIRECTION_LTR);
  
  priv = GTK_MENU_BAR_GET_PRIVATE (menubar);

  return priv->child_pack_direction;
}

/**
 * gtk_menu_bar_set_child_pack_direction:
 * @menubar: a #GtkMenuBar
 * @child_pack_dir: a new #GtkPackDirection
 * 
 * Sets how widgets should be packed inside the children of a menubar.
 * 
 * Since: 2.8
 */
void
gtk_menu_bar_set_child_pack_direction (GtkMenuBar       *menubar,
                                       GtkPackDirection  child_pack_dir)
{
  GtkMenuBarPrivate *priv;
  GList *l;

  g_return_if_fail (GTK_IS_MENU_BAR (menubar));

  priv = GTK_MENU_BAR_GET_PRIVATE (menubar);

  if (priv->child_pack_direction != child_pack_dir)
    {
      priv->child_pack_direction = child_pack_dir;

      gtk_widget_queue_resize (GTK_WIDGET (menubar));

      for (l = GTK_MENU_SHELL (menubar)->children; l; l = l->next)
	gtk_widget_queue_resize (GTK_WIDGET (l->data));

      g_object_notify (G_OBJECT (menubar), "child-pack-direction");
    }
}

#define __GTK_MENU_BAR_C__
#include "gtkaliasdef.c"
