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

#include "config.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtktogglebutton.h"
#include "gtktoggleaction.h"
#include "gtkactivatable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_INCONSISTENT,
  PROP_DRAW_INDICATOR
};


static gint gtk_toggle_button_expose        (GtkWidget            *widget,
					     GdkEventExpose       *event);
static gboolean gtk_toggle_button_mnemonic_activate  (GtkWidget            *widget,
                                                      gboolean              group_cycling);
static void gtk_toggle_button_pressed       (GtkButton            *button);
static void gtk_toggle_button_released      (GtkButton            *button);
static void gtk_toggle_button_clicked       (GtkButton            *button);
static void gtk_toggle_button_set_property  (GObject              *object,
					     guint                 prop_id,
					     const GValue         *value,
					     GParamSpec           *pspec);
static void gtk_toggle_button_get_property  (GObject              *object,
					     guint                 prop_id,
					     GValue               *value,
					     GParamSpec           *pspec);
static void gtk_toggle_button_update_state  (GtkButton            *button);


static void gtk_toggle_button_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_toggle_button_update         	     (GtkActivatable       *activatable,
					 	      GtkAction            *action,
						      const gchar          *property_name);
static void gtk_toggle_button_sync_action_properties (GtkActivatable       *activatable,
						      GtkAction            *action);

static GtkActivatableIface *parent_activatable_iface;
static guint                toggle_button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkToggleButton, gtk_toggle_button, GTK_TYPE_BUTTON,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_toggle_button_activatable_interface_init))

static void
gtk_toggle_button_class_init (GtkToggleButtonClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;

  gobject_class->set_property = gtk_toggle_button_set_property;
  gobject_class->get_property = gtk_toggle_button_get_property;

  widget_class->expose_event = gtk_toggle_button_expose;
  widget_class->mnemonic_activate = gtk_toggle_button_mnemonic_activate;

  button_class->pressed = gtk_toggle_button_pressed;
  button_class->released = gtk_toggle_button_released;
  button_class->clicked = gtk_toggle_button_clicked;
  button_class->enter = gtk_toggle_button_update_state;
  button_class->leave = gtk_toggle_button_update_state;

  class->toggled = NULL;

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
							 P_("Active"),
							 P_("If the toggle button should be pressed in or not"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INCONSISTENT,
                                   g_param_spec_boolean ("inconsistent",
							 P_("Inconsistent"),
							 P_("If the toggle button is in an \"in between\" state"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_INDICATOR,
                                   g_param_spec_boolean ("draw-indicator",
							 P_("Draw Indicator"),
							 P_("If the toggle part of the button is displayed"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  toggle_button_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToggleButtonClass, toggled),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_toggle_button_init (GtkToggleButton *toggle_button)
{
  toggle_button->active = FALSE;
  toggle_button->draw_indicator = FALSE;
  GTK_BUTTON (toggle_button)->depress_on_activate = TRUE;
}

static void
gtk_toggle_button_activatable_interface_init (GtkActivatableIface *iface)
{
  parent_activatable_iface = g_type_interface_peek_parent (iface);
  iface->update = gtk_toggle_button_update;
  iface->sync_action_properties = gtk_toggle_button_sync_action_properties;
}

static void
gtk_toggle_button_update (GtkActivatable *activatable,
			  GtkAction      *action,
			  const gchar    *property_name)
{
  GtkToggleButton *button;

  parent_activatable_iface->update (activatable, action, property_name);

  button = GTK_TOGGLE_BUTTON (activatable);

  if (strcmp (property_name, "active") == 0)
    {
      gtk_action_block_activate (action);
      gtk_toggle_button_set_active (button, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
      gtk_action_unblock_activate (action);
    }

}

static void
gtk_toggle_button_sync_action_properties (GtkActivatable *activatable,
				          GtkAction      *action)
{
  GtkToggleButton *button;

  parent_activatable_iface->sync_action_properties (activatable, action);

  if (!GTK_IS_TOGGLE_ACTION (action))
    return;

  button = GTK_TOGGLE_BUTTON (activatable);

  gtk_action_block_activate (action);
  gtk_toggle_button_set_active (button, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
  gtk_action_unblock_activate (action);
}


GtkWidget*
gtk_toggle_button_new (void)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, NULL);
}

GtkWidget*
gtk_toggle_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, "label", label, NULL);
}

/**
 * gtk_toggle_button_new_with_mnemonic:
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkToggleButton
 *
 * Creates a new #GtkToggleButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the button.
 **/
GtkWidget*
gtk_toggle_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, 
		       "label", label, 
		       "use-underline", TRUE, 
		       NULL);
}

static void
gtk_toggle_button_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_toggle_button_set_active (tb, g_value_get_boolean (value));
      break;
    case PROP_INCONSISTENT:
      gtk_toggle_button_set_inconsistent (tb, g_value_get_boolean (value));
      break;
    case PROP_DRAW_INDICATOR:
      gtk_toggle_button_set_mode (tb, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_toggle_button_get_property (GObject      *object,
				guint         prop_id,
				GValue       *value,
				GParamSpec   *pspec)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, tb->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, tb->inconsistent);
      break;
    case PROP_DRAW_INDICATOR:
      g_value_set_boolean (value, tb->draw_indicator);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_toggle_button_set_mode:
 * @toggle_button: a #GtkToggleButton
 * @draw_indicator: if %TRUE, draw the button as a separate indicator
 * and label; if %FALSE, draw the button like a normal button
 *
 * Sets whether the button is displayed as a separate indicator and label.
 * You can call this function on a checkbutton or a radiobutton with
 * @draw_indicator = %FALSE to make the button look like a normal button
 *
 * This function only affects instances of classes like #GtkCheckButton
 * and #GtkRadioButton that derive from #GtkToggleButton,
 * not instances of #GtkToggleButton itself.
 */
void
gtk_toggle_button_set_mode (GtkToggleButton *toggle_button,
			    gboolean         draw_indicator)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  draw_indicator = draw_indicator ? TRUE : FALSE;

  if (toggle_button->draw_indicator != draw_indicator)
    {
      toggle_button->draw_indicator = draw_indicator;
      GTK_BUTTON (toggle_button)->depress_on_activate = !draw_indicator;
      
      if (gtk_widget_get_visible (GTK_WIDGET (toggle_button)))
	gtk_widget_queue_resize (GTK_WIDGET (toggle_button));

      g_object_notify (G_OBJECT (toggle_button), "draw-indicator");
    }
}

/**
 * gtk_toggle_button_get_mode:
 * @toggle_button: a #GtkToggleButton
 *
 * Retrieves whether the button is displayed as a separate indicator
 * and label. See gtk_toggle_button_set_mode().
 *
 * Return value: %TRUE if the togglebutton is drawn as a separate indicator
 *   and label.
 **/
gboolean
gtk_toggle_button_get_mode (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return toggle_button->draw_indicator;
}

void
gtk_toggle_button_set_active (GtkToggleButton *toggle_button,
			      gboolean         is_active)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  is_active = is_active != FALSE;

  if (toggle_button->active != is_active)
    gtk_button_clicked (GTK_BUTTON (toggle_button));
}


gboolean
gtk_toggle_button_get_active (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return (toggle_button->active) ? TRUE : FALSE;
}


void
gtk_toggle_button_toggled (GtkToggleButton *toggle_button)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  g_signal_emit (toggle_button, toggle_button_signals[TOGGLED], 0);
}

/**
 * gtk_toggle_button_set_inconsistent:
 * @toggle_button: a #GtkToggleButton
 * @setting: %TRUE if state is inconsistent
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a toggle button, and the
 * current values in that range are inconsistent, you may want to
 * display the toggle in an "in between" state. This function turns on
 * "in between" display.  Normally you would turn off the inconsistent
 * state again if the user toggles the toggle button. This has to be
 * done manually, gtk_toggle_button_set_inconsistent() only affects
 * visual appearance, it doesn't affect the semantics of the button.
 * 
 **/
void
gtk_toggle_button_set_inconsistent (GtkToggleButton *toggle_button,
                                    gboolean         setting)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));
  
  setting = setting != FALSE;

  if (setting != toggle_button->inconsistent)
    {
      toggle_button->inconsistent = setting;
      
      gtk_toggle_button_update_state (GTK_BUTTON (toggle_button));
      gtk_widget_queue_draw (GTK_WIDGET (toggle_button));

      g_object_notify (G_OBJECT (toggle_button), "inconsistent");      
    }
}

/**
 * gtk_toggle_button_get_inconsistent:
 * @toggle_button: a #GtkToggleButton
 * 
 * Gets the value set by gtk_toggle_button_set_inconsistent().
 * 
 * Return value: %TRUE if the button is displayed as inconsistent, %FALSE otherwise
 **/
gboolean
gtk_toggle_button_get_inconsistent (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return toggle_button->inconsistent;
}

static gint
gtk_toggle_button_expose (GtkWidget      *widget,
			  GdkEventExpose *event)
{
  if (gtk_widget_is_drawable (widget))
    {
      GtkWidget *child = GTK_BIN (widget)->child;
      GtkButton *button = GTK_BUTTON (widget);
      GtkStateType state_type;
      GtkShadowType shadow_type;

      state_type = gtk_widget_get_state (widget);
      
      if (GTK_TOGGLE_BUTTON (widget)->inconsistent)
        {
          if (state_type == GTK_STATE_ACTIVE)
            state_type = GTK_STATE_NORMAL;
          shadow_type = GTK_SHADOW_ETCHED_IN;
        }
      else
	shadow_type = button->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

      _gtk_button_paint (button, &event->area, state_type, shadow_type,
			 "togglebutton", "togglebuttondefault");

      if (child)
	gtk_container_propagate_expose (GTK_CONTAINER (widget), child, event);
    }
  
  return FALSE;
}

static gboolean
gtk_toggle_button_mnemonic_activate (GtkWidget *widget,
                                     gboolean   group_cycling)
{
  /*
   * We override the standard implementation in 
   * gtk_widget_real_mnemonic_activate() in order to focus the widget even
   * if there is no mnemonic conflict.
   */
  if (gtk_widget_get_can_focus (widget))
    gtk_widget_grab_focus (widget);

  if (!group_cycling)
    gtk_widget_activate (widget);

  return TRUE;
}

static void
gtk_toggle_button_pressed (GtkButton *button)
{
  button->button_down = TRUE;

  gtk_toggle_button_update_state (button);
  gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
gtk_toggle_button_released (GtkButton *button)
{
  if (button->button_down)
    {
      button->button_down = FALSE;

      if (button->in_button)
	gtk_button_clicked (button);

      gtk_toggle_button_update_state (button);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}

static void
gtk_toggle_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  toggle_button->active = !toggle_button->active;

  gtk_toggle_button_toggled (toggle_button);

  gtk_toggle_button_update_state (button);

  g_object_notify (G_OBJECT (toggle_button), "active");

  if (GTK_BUTTON_CLASS (gtk_toggle_button_parent_class)->clicked)
    GTK_BUTTON_CLASS (gtk_toggle_button_parent_class)->clicked (button);
}

static void
gtk_toggle_button_update_state (GtkButton *button)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  gboolean depressed, touchscreen;
  GtkStateType new_state;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (button)),
                "gtk-touchscreen-mode", &touchscreen,
                NULL);

  if (toggle_button->inconsistent)
    depressed = FALSE;
  else if (button->in_button && button->button_down)
    depressed = TRUE;
  else
    depressed = toggle_button->active;
      
  if (!touchscreen && button->in_button && (!button->button_down || toggle_button->draw_indicator))
    new_state = GTK_STATE_PRELIGHT;
  else
    new_state = depressed ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL;

  _gtk_button_set_depressed (button, depressed); 
  gtk_widget_set_state (GTK_WIDGET (toggle_button), new_state);
}

#define __GTK_TOGGLE_BUTTON_C__
#include "gtkaliasdef.c"
