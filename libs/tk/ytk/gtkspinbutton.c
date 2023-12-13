/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkSpinButton widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkspinbutton.h"
#include "gtkentryprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtksettings.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define MIN_SPIN_BUTTON_WIDTH 30
#define MAX_TIMER_CALLS       5
#define EPSILON               1e-10
#define	MAX_DIGITS            20
#define MIN_ARROW_WIDTH       6

enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_CLIMB_RATE,
  PROP_DIGITS,
  PROP_SNAP_TO_TICKS,
  PROP_NUMERIC,
  PROP_WRAP,
  PROP_UPDATE_POLICY,
  PROP_VALUE
};

/* Signals */
enum
{
  INPUT,
  OUTPUT,
  VALUE_CHANGED,
  CHANGE_VALUE,
  WRAPPED,
  LAST_SIGNAL
};

static void gtk_spin_button_editable_init  (GtkEditableClass   *iface);
static void gtk_spin_button_finalize       (GObject            *object);
static void gtk_spin_button_destroy        (GtkObject          *object);
static void gtk_spin_button_set_property   (GObject         *object,
					    guint            prop_id,
					    const GValue    *value,
					    GParamSpec      *pspec);
static void gtk_spin_button_get_property   (GObject         *object,
					    guint            prop_id,
					    GValue          *value,
					    GParamSpec      *pspec);
static void gtk_spin_button_map            (GtkWidget          *widget);
static void gtk_spin_button_unmap          (GtkWidget          *widget);
static void gtk_spin_button_realize        (GtkWidget          *widget);
static void gtk_spin_button_unrealize      (GtkWidget          *widget);
static void gtk_spin_button_size_request   (GtkWidget          *widget,
					    GtkRequisition     *requisition);
static void gtk_spin_button_size_allocate  (GtkWidget          *widget,
					    GtkAllocation      *allocation);
static gint gtk_spin_button_expose         (GtkWidget          *widget,
					    GdkEventExpose     *event);
static gint gtk_spin_button_button_press   (GtkWidget          *widget,
					    GdkEventButton     *event);
static gint gtk_spin_button_button_release (GtkWidget          *widget,
					    GdkEventButton     *event);
static gint gtk_spin_button_motion_notify  (GtkWidget          *widget,
					    GdkEventMotion     *event);
static gint gtk_spin_button_enter_notify   (GtkWidget          *widget,
					    GdkEventCrossing   *event);
static gint gtk_spin_button_leave_notify   (GtkWidget          *widget,
					    GdkEventCrossing   *event);
static gint gtk_spin_button_focus_out      (GtkWidget          *widget,
					    GdkEventFocus      *event);
static void gtk_spin_button_grab_notify    (GtkWidget          *widget,
					    gboolean            was_grabbed);
static void gtk_spin_button_state_changed  (GtkWidget          *widget,
					    GtkStateType        previous_state);
static void gtk_spin_button_style_set      (GtkWidget          *widget,
                                            GtkStyle           *previous_style);
static void gtk_spin_button_draw_arrow     (GtkSpinButton      *spin_button, 
					    GdkRectangle       *area,
					    GtkArrowType        arrow_type);
static gboolean gtk_spin_button_timer          (GtkSpinButton      *spin_button);
static void gtk_spin_button_stop_spinning  (GtkSpinButton      *spin);
static void gtk_spin_button_value_changed  (GtkAdjustment      *adjustment,
					    GtkSpinButton      *spin_button); 
static gint gtk_spin_button_key_release    (GtkWidget          *widget,
					    GdkEventKey        *event);
static gint gtk_spin_button_scroll         (GtkWidget          *widget,
					    GdkEventScroll     *event);
static void gtk_spin_button_activate       (GtkEntry           *entry);
static void gtk_spin_button_get_text_area_size (GtkEntry *entry,
						gint     *x,
						gint     *y,
						gint     *width,
						gint     *height);
static void gtk_spin_button_snap           (GtkSpinButton      *spin_button,
					    gdouble             val);
static void gtk_spin_button_insert_text    (GtkEditable        *editable,
					    const gchar        *new_text,
					    gint                new_text_length,
					    gint               *position);
static void gtk_spin_button_real_spin      (GtkSpinButton      *spin_button,
					    gdouble             step);
static void gtk_spin_button_real_change_value (GtkSpinButton   *spin,
					       GtkScrollType    scroll);

static gint gtk_spin_button_default_input  (GtkSpinButton      *spin_button,
					    gdouble            *new_val);
static gint gtk_spin_button_default_output (GtkSpinButton      *spin_button);

static gint spin_button_get_arrow_size     (GtkSpinButton      *spin_button);
static gint spin_button_get_shadow_type    (GtkSpinButton      *spin_button);

static guint spinbutton_signals[LAST_SIGNAL] = {0};

#define NO_ARROW 2

G_DEFINE_TYPE_WITH_CODE (GtkSpinButton, gtk_spin_button, GTK_TYPE_ENTRY,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
						gtk_spin_button_editable_init))

#define add_spin_binding(binding_set, keyval, mask, scroll)            \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,             \
                                "change_value", 1,                     \
                                GTK_TYPE_SCROLL_TYPE, scroll)

static void
gtk_spin_button_class_init (GtkSpinButtonClass *class)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass   *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (class);
  GtkEntryClass    *entry_class = GTK_ENTRY_CLASS (class);
  GtkBindingSet    *binding_set;

  gobject_class->finalize = gtk_spin_button_finalize;

  gobject_class->set_property = gtk_spin_button_set_property;
  gobject_class->get_property = gtk_spin_button_get_property;

  object_class->destroy = gtk_spin_button_destroy;

  widget_class->map = gtk_spin_button_map;
  widget_class->unmap = gtk_spin_button_unmap;
  widget_class->realize = gtk_spin_button_realize;
  widget_class->unrealize = gtk_spin_button_unrealize;
  widget_class->size_request = gtk_spin_button_size_request;
  widget_class->size_allocate = gtk_spin_button_size_allocate;
  widget_class->expose_event = gtk_spin_button_expose;
  widget_class->scroll_event = gtk_spin_button_scroll;
  widget_class->button_press_event = gtk_spin_button_button_press;
  widget_class->button_release_event = gtk_spin_button_button_release;
  widget_class->motion_notify_event = gtk_spin_button_motion_notify;
  widget_class->key_release_event = gtk_spin_button_key_release;
  widget_class->enter_notify_event = gtk_spin_button_enter_notify;
  widget_class->leave_notify_event = gtk_spin_button_leave_notify;
  widget_class->focus_out_event = gtk_spin_button_focus_out;
  widget_class->grab_notify = gtk_spin_button_grab_notify;
  widget_class->state_changed = gtk_spin_button_state_changed;
  widget_class->style_set = gtk_spin_button_style_set;

  entry_class->activate = gtk_spin_button_activate;
  entry_class->get_text_area_size = gtk_spin_button_get_text_area_size;

  class->input = NULL;
  class->output = NULL;
  class->change_value = gtk_spin_button_real_change_value;

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        P_("Adjustment"),
                                                        P_("The adjustment that holds the value of the spinbutton"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_CLIMB_RATE,
                                   g_param_spec_double ("climb-rate",
							P_("Climb Rate"),
							P_("The acceleration rate when you hold down a button"),
							0.0,
							G_MAXDOUBLE,
							0.0,
							GTK_PARAM_READWRITE));  
  
  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_uint ("digits",
						      P_("Digits"),
						      P_("The number of decimal places to display"),
						      0,
						      MAX_DIGITS,
						      0,
						      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_TO_TICKS,
                                   g_param_spec_boolean ("snap-to-ticks",
							 P_("Snap to Ticks"),
							 P_("Whether erroneous values are automatically changed to a spin button's nearest step increment"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_NUMERIC,
                                   g_param_spec_boolean ("numeric",
							 P_("Numeric"),
							 P_("Whether non-numeric characters should be ignored"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
							 P_("Wrap"),
							 P_("Whether a spin button should wrap upon reaching its limits"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_UPDATE_POLICY,
                                   g_param_spec_enum ("update-policy",
						      P_("Update Policy"),
						      P_("Whether the spin button should update always, or only when the value is legal"),
						      GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
						      GTK_UPDATE_ALWAYS,
						      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
							P_("Value"),
							P_("Reads the current value, or sets a new value"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0.0,
							GTK_PARAM_READWRITE));  
  
  gtk_widget_class_install_style_property_parser (widget_class,
						  g_param_spec_enum ("shadow-type", 
								     "Shadow Type", 
								     P_("Style of bevel around the spin button"),
								     GTK_TYPE_SHADOW_TYPE,
								     GTK_SHADOW_IN,
								     GTK_PARAM_READABLE),
						  gtk_rc_property_parse_enum);
  spinbutton_signals[INPUT] =
    g_signal_new (I_("input"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSpinButtonClass, input),
		  NULL, NULL,
		  _gtk_marshal_INT__POINTER,
		  G_TYPE_INT, 1,
		  G_TYPE_POINTER);

  /**
   * GtkSpinButton::output:
   * @spin_button: the object which received the signal
   * 
   * The ::output signal can be used to change to formatting
   * of the value that is displayed in the spin buttons entry.
   * |[
   * /&ast; show leading zeros &ast;/
   * static gboolean
   * on_output (GtkSpinButton *spin,
   *            gpointer       data)
   * {
   *    GtkAdjustment *adj;
   *    gchar *text;
   *    int value;
   *    
   *    adj = gtk_spin_button_get_adjustment (spin);
   *    value = (int)gtk_adjustment_get_value (adj);
   *    text = g_strdup_printf ("%02d", value);
   *    gtk_entry_set_text (GTK_ENTRY (spin), text);
   *    g_free (text);
   *    
   *    return TRUE;
   * }
   * ]|
   *
   * Returns: %TRUE if the value has been displayed.
   */
  spinbutton_signals[OUTPUT] =
    g_signal_new (I_("output"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSpinButtonClass, output),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  spinbutton_signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSpinButtonClass, value_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkSpinButton::wrapped:
   * @spinbutton: the object which received the signal
   *
   * The wrapped signal is emitted right after the spinbutton wraps
   * from its maximum to minimum value or vice-versa.
   *
   * Since: 2.10
   */
  spinbutton_signals[WRAPPED] =
    g_signal_new (I_("wrapped"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSpinButtonClass, wrapped),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /* Action signals */
  spinbutton_signals[CHANGE_VALUE] =
    g_signal_new (I_("change-value"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, change_value),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);
  
  binding_set = gtk_binding_set_by_class (class);
  
  add_spin_binding (binding_set, GDK_Up, 0, GTK_SCROLL_STEP_UP);
  add_spin_binding (binding_set, GDK_KP_Up, 0, GTK_SCROLL_STEP_UP);
  add_spin_binding (binding_set, GDK_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_spin_binding (binding_set, GDK_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_spin_binding (binding_set, GDK_Page_Up, 0, GTK_SCROLL_PAGE_UP);
  add_spin_binding (binding_set, GDK_Page_Down, 0, GTK_SCROLL_PAGE_DOWN);
  add_spin_binding (binding_set, GDK_Page_Up, GDK_CONTROL_MASK, GTK_SCROLL_END);
  add_spin_binding (binding_set, GDK_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_START);
}

static void
gtk_spin_button_editable_init (GtkEditableClass *iface)
{
  iface->insert_text = gtk_spin_button_insert_text;
}

static void
gtk_spin_button_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);

  switch (prop_id)
    {
      GtkAdjustment *adjustment;

    case PROP_ADJUSTMENT:
      adjustment = GTK_ADJUSTMENT (g_value_get_object (value));
      if (!adjustment)
	adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      gtk_spin_button_set_adjustment (spin_button, adjustment);
      break;
    case PROP_CLIMB_RATE:
      gtk_spin_button_configure (spin_button,
				 spin_button->adjustment,
				 g_value_get_double (value),
				 spin_button->digits);
      break;
    case PROP_DIGITS:
      gtk_spin_button_configure (spin_button,
				 spin_button->adjustment,
				 spin_button->climb_rate,
				 g_value_get_uint (value));
      break;
    case PROP_SNAP_TO_TICKS:
      gtk_spin_button_set_snap_to_ticks (spin_button, g_value_get_boolean (value));
      break;
    case PROP_NUMERIC:
      gtk_spin_button_set_numeric (spin_button, g_value_get_boolean (value));
      break;
    case PROP_WRAP:
      gtk_spin_button_set_wrap (spin_button, g_value_get_boolean (value));
      break;
    case PROP_UPDATE_POLICY:
      gtk_spin_button_set_update_policy (spin_button, g_value_get_enum (value));
      break;
    case PROP_VALUE:
      gtk_spin_button_set_value (spin_button, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_spin_button_get_property (GObject      *object,
			      guint         prop_id,
			      GValue       *value,
			      GParamSpec   *pspec)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, spin_button->adjustment);
      break;
    case PROP_CLIMB_RATE:
      g_value_set_double (value, spin_button->climb_rate);
      break;
    case PROP_DIGITS:
      g_value_set_uint (value, spin_button->digits);
      break;
    case PROP_SNAP_TO_TICKS:
      g_value_set_boolean (value, spin_button->snap_to_ticks);
      break;
    case PROP_NUMERIC:
      g_value_set_boolean (value, spin_button->numeric);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, spin_button->wrap);
      break;
    case PROP_UPDATE_POLICY:
      g_value_set_enum (value, spin_button->update_policy);
      break;
     case PROP_VALUE:
       g_value_set_double (value, spin_button->adjustment->value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  spin_button->adjustment = NULL;
  spin_button->panel = NULL;
  spin_button->timer = 0;
  spin_button->climb_rate = 0.0;
  spin_button->timer_step = 0.0;
  spin_button->update_policy = GTK_UPDATE_ALWAYS;
  spin_button->in_child = NO_ARROW;
  spin_button->click_child = NO_ARROW;
  spin_button->button = 0;
  spin_button->need_timer = FALSE;
  spin_button->timer_calls = 0;
  spin_button->digits = 0;
  spin_button->numeric = FALSE;
  spin_button->wrap = FALSE;
  spin_button->snap_to_ticks = FALSE;

  gtk_spin_button_set_adjustment (spin_button,
	  (GtkAdjustment*) gtk_adjustment_new (0, 0, 0, 0, 0, 0));
}

static void
gtk_spin_button_finalize (GObject *object)
{
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (object), NULL);
  
  G_OBJECT_CLASS (gtk_spin_button_parent_class)->finalize (object);
}

static void
gtk_spin_button_destroy (GtkObject *object)
{
  gtk_spin_button_stop_spinning (GTK_SPIN_BUTTON (object));
  
  GTK_OBJECT_CLASS (gtk_spin_button_parent_class)->destroy (object);
}

static void
gtk_spin_button_map (GtkWidget *widget)
{
  if (gtk_widget_get_realized (widget) && !gtk_widget_get_mapped (widget))
    {
      GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->map (widget);
      gdk_window_show (GTK_SPIN_BUTTON (widget)->panel);
    }
}

static void
gtk_spin_button_unmap (GtkWidget *widget)
{
  if (gtk_widget_get_mapped (widget))
    {
      gtk_spin_button_stop_spinning (GTK_SPIN_BUTTON (widget));

      gdk_window_hide (GTK_SPIN_BUTTON (widget)->panel);
      GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->unmap (widget);
    }
}

static void
gtk_spin_button_realize (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;
  gboolean return_val;
  gint arrow_size;

  arrow_size = spin_button_get_arrow_size (spin_button);

  gtk_widget_set_events (widget, gtk_widget_get_events (widget) |
			 GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->realize (widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK 
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK 
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  attributes.x = (widget->allocation.width - arrow_size -
		  2 * widget->style->xthickness);
  attributes.y = (widget->allocation.height -
					 widget->requisition.height) / 2;
  attributes.width = arrow_size + 2 * widget->style->xthickness;
  attributes.height = widget->requisition.height;
  
  spin_button->panel = gdk_window_new (widget->window, 
				       &attributes, attributes_mask);
  gdk_window_set_user_data (spin_button->panel, widget);

  gtk_style_set_background (widget->style, spin_button->panel, GTK_STATE_NORMAL);

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);

  /* If output wasn't processed explicitly by the method connected to the
   * 'output' signal; and if we don't have any explicit 'text' set initially,
   * fallback to the default output. */
  if (!return_val &&
      (spin_button->numeric || gtk_entry_get_text (GTK_ENTRY (spin_button)) == NULL))
    gtk_spin_button_default_output (spin_button);

  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

static void
gtk_spin_button_unrealize (GtkWidget *widget)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  gtk_spin_button_stop_spinning (GTK_SPIN_BUTTON (widget));

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->unrealize (widget);

  if (spin->panel)
    {
      gdk_window_set_user_data (spin->panel, NULL);
      gdk_window_destroy (spin->panel);
      spin->panel = NULL;
    }
}

static int
compute_double_length (double val, int digits)
{
  int a;
  int extra;

  a = 1;
  if (fabs (val) > 1.0)
    a = floor (log10 (fabs (val))) + 1;  

  extra = 0;
  
  /* The dot: */
  if (digits > 0)
    extra++;

  /* The sign: */
  if (val < 0)
    extra++;

  return a + digits + extra;
}

static void
gtk_spin_button_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkEntry *entry = GTK_ENTRY (widget);
  gint arrow_size;

  arrow_size = spin_button_get_arrow_size (spin_button);

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->size_request (widget, requisition);

  if (entry->width_chars < 0)
    {
      PangoContext *context;
      PangoFontMetrics *metrics;
      gint width;
      gint w;
      gint string_len;
      gint max_string_len;
      gint digit_width;
      gboolean interior_focus;
      gint focus_width;
      gint xborder, yborder;
      GtkBorder inner_border;

      gtk_widget_style_get (widget,
			    "interior-focus", &interior_focus,
			    "focus-line-width", &focus_width,
			    NULL);

      context = gtk_widget_get_pango_context (widget);
      metrics = pango_context_get_metrics (context,
					   widget->style->font_desc,
					   pango_context_get_language (context));

      digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
      digit_width = PANGO_SCALE *
        ((digit_width + PANGO_SCALE - 1) / PANGO_SCALE);

      pango_font_metrics_unref (metrics);
      
      /* Get max of MIN_SPIN_BUTTON_WIDTH, size of upper, size of lower */
      
      width = MIN_SPIN_BUTTON_WIDTH;
      max_string_len = MAX (10, compute_double_length (1e9 * spin_button->adjustment->step_increment,
                                                       spin_button->digits));

      string_len = compute_double_length (spin_button->adjustment->upper,
                                          spin_button->digits);
      w = PANGO_PIXELS (MIN (string_len, max_string_len) * digit_width);
      width = MAX (width, w);
      string_len = compute_double_length (spin_button->adjustment->lower,
					  spin_button->digits);
      w = PANGO_PIXELS (MIN (string_len, max_string_len) * digit_width);
      width = MAX (width, w);
      
      _gtk_entry_get_borders (entry, &xborder, &yborder);
      _gtk_entry_effective_inner_border (entry, &inner_border);

      requisition->width = width + xborder * 2 + inner_border.left + inner_border.right;
    }

  requisition->width += arrow_size + 2 * widget->style->xthickness;
}

static void
gtk_spin_button_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkAllocation panel_allocation;
  gint arrow_size;
  gint panel_width;

  arrow_size = spin_button_get_arrow_size (spin);
  panel_width = arrow_size + 2 * widget->style->xthickness;
  
  widget->allocation = *allocation;
  
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    panel_allocation.x = 0;
  else
    panel_allocation.x = allocation->width - panel_width;

  panel_allocation.width = panel_width;
  panel_allocation.height = MIN (widget->requisition.height, allocation->height);

  panel_allocation.y = 0;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (GTK_SPIN_BUTTON (widget)->panel, 
			      panel_allocation.x,
			      panel_allocation.y,
			      panel_allocation.width,
			      panel_allocation.height); 
    }

  gtk_widget_queue_draw (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (gtk_widget_is_drawable (widget))
    {
      if (event->window == spin->panel)
	{
	  GtkShadowType shadow_type;

	  shadow_type = spin_button_get_shadow_type (spin);

	  if (shadow_type != GTK_SHADOW_NONE)
	    {
	      gint width, height;
              gboolean state_hint;
              GtkStateType state;

              gtk_widget_style_get (widget, "state-hint", &state_hint, NULL);
              if (state_hint)
                state = gtk_widget_has_focus (widget) ?
                  GTK_STATE_ACTIVE : gtk_widget_get_state (widget);
              else
                state = GTK_STATE_NORMAL;

              width = gdk_window_get_width (spin->panel);
              height = gdk_window_get_height (spin->panel);

              if (gtk_entry_get_has_frame (GTK_ENTRY (spin)))
                gtk_paint_box (widget->style, spin->panel,
                               state, shadow_type,
                               &event->area, widget, "spinbutton",
                               0, 0, width, height);
	    }

	  gtk_spin_button_draw_arrow (spin, &event->area, GTK_ARROW_UP);
	  gtk_spin_button_draw_arrow (spin, &event->area, GTK_ARROW_DOWN);
	}
      else
        {
          if (event->window == widget->window)
            {
              gint text_x, text_y, text_width, text_height, slice_x;

              /* Since we reuse xthickness for the buttons panel on one side, and GtkEntry
               * always sizes its background to (allocation->width - 2 * xthickness), we
               * have to manually render the missing slice of the background on the panel
               * side.
               */
              GTK_ENTRY_GET_CLASS (spin)->get_text_area_size (GTK_ENTRY (spin),
                                                              &text_x, &text_y,
                                                              &text_width, &text_height);

              if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
                slice_x = text_x - widget->style->xthickness;
              else
                slice_x = text_x + text_width;

              gtk_paint_flat_box (widget->style, widget->window,
                                  gtk_widget_get_state (widget), GTK_SHADOW_NONE,
                                  &event->area, widget, "entry_bg",
                                  slice_x, text_y,
                                  widget->style->xthickness, text_height);
            }

          GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->expose_event (widget, event);
        }
    }
  
  return FALSE;
}

static gboolean
spin_button_at_limit (GtkSpinButton *spin_button,
                     GtkArrowType   arrow)
{
  GtkArrowType effective_arrow;

  if (spin_button->wrap)
    return FALSE;

  if (spin_button->adjustment->step_increment > 0)
    effective_arrow = arrow;
  else
    effective_arrow = arrow == GTK_ARROW_UP ? GTK_ARROW_DOWN : GTK_ARROW_UP; 
  
  if (effective_arrow == GTK_ARROW_UP &&
      (spin_button->adjustment->upper - spin_button->adjustment->value <= EPSILON))
    return TRUE;
  
  if (effective_arrow == GTK_ARROW_DOWN &&
      (spin_button->adjustment->value - spin_button->adjustment->lower <= EPSILON))
    return TRUE;
  
  return FALSE;
}

static void
gtk_spin_button_draw_arrow (GtkSpinButton *spin_button, 
			    GdkRectangle  *area,
			    GtkArrowType   arrow_type)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GtkWidget *widget;
  gint x;
  gint y;
  gint height;
  gint width;
  gint h, w;

  g_return_if_fail (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN);

  widget = GTK_WIDGET (spin_button);

  if (gtk_widget_is_drawable (widget))
    {
      width = spin_button_get_arrow_size (spin_button) + 2 * widget->style->xthickness;

      if (arrow_type == GTK_ARROW_UP)
	{
	  x = 0;
	  y = 0;

	  height = widget->requisition.height / 2;
	}
      else
	{
	  x = 0;
	  y = widget->requisition.height / 2;

	  height = (widget->requisition.height + 1) / 2;
	}

      if (spin_button_at_limit (spin_button, arrow_type))
	{
	  shadow_type = GTK_SHADOW_OUT;
	  state_type = GTK_STATE_INSENSITIVE;
	}
      else
	{
	  if (spin_button->click_child == arrow_type)
	    {
	      state_type = GTK_STATE_ACTIVE;
	      shadow_type = GTK_SHADOW_IN;
	    }
	  else
	    {
	      if (spin_button->in_child == arrow_type &&
		  spin_button->click_child == NO_ARROW)
		{
		  state_type = GTK_STATE_PRELIGHT;
		}
	      else
		{
		  state_type = gtk_widget_get_state (widget);
		}
	      
	      shadow_type = GTK_SHADOW_OUT;
	    }
	}
      
      gtk_paint_box (widget->style, spin_button->panel,
		     state_type, shadow_type,
		     area, widget,
		     (arrow_type == GTK_ARROW_UP)? "spinbutton_up" : "spinbutton_down",
		     x, y, width, height);

      height = widget->requisition.height;

      if (arrow_type == GTK_ARROW_DOWN)
	{
	  y = height / 2;
	  height = height - y - 2;
	}
      else
	{
	  y = 2;
	  height = height / 2 - 2;
	}

      width -= 3;

      if (widget && gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	x = 2;
      else
	x = 1;

      w = width / 2;
      w -= w % 2 - 1; /* force odd */
      h = (w + 1) / 2;
      
      x += (width - w) / 2;
      y += (height - h) / 2;
      
      height = h;
      width = w;

      gtk_paint_arrow (widget->style, spin_button->panel,
		       state_type, shadow_type, 
		       area, widget, "spinbutton",
		       arrow_type, TRUE, 
		       x, y, width, height);
    }
}

static gint
gtk_spin_button_enter_notify (GtkWidget        *widget,
			      GdkEventCrossing *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (event->window == spin->panel)
    {
      gint x;
      gint y;

      gdk_window_get_pointer (spin->panel, &x, &y, NULL);

      if (y <= widget->requisition.height / 2)
	spin->in_child = GTK_ARROW_UP;
      else
	spin->in_child = GTK_ARROW_DOWN;

      gtk_widget_queue_draw (GTK_WIDGET (spin));
    }
 
  if (GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->enter_notify_event)
    return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->enter_notify_event (widget, event);

  return FALSE;
}

static gint
gtk_spin_button_leave_notify (GtkWidget        *widget,
			      GdkEventCrossing *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  spin->in_child = NO_ARROW;
  gtk_widget_queue_draw (GTK_WIDGET (spin));
 
  if (GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->leave_notify_event)
    return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->leave_notify_event (widget, event);

  return FALSE;
}

static gint
gtk_spin_button_focus_out (GtkWidget     *widget,
			   GdkEventFocus *event)
{
  if (GTK_ENTRY (widget)->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (widget));

  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->focus_out_event (widget, event);
}

static void
gtk_spin_button_grab_notify (GtkWidget *widget,
			     gboolean   was_grabbed)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (!was_grabbed)
    {
      gtk_spin_button_stop_spinning (spin);
      gtk_widget_queue_draw (GTK_WIDGET (spin));
    }
}

static void
gtk_spin_button_state_changed (GtkWidget    *widget,
			       GtkStateType  previous_state)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (!gtk_widget_is_sensitive (widget))
    {
      gtk_spin_button_stop_spinning (spin);    
      gtk_widget_queue_draw (GTK_WIDGET (spin));
    }
}

static void
gtk_spin_button_style_set (GtkWidget *widget,
		           GtkStyle  *previous_style)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (previous_style && gtk_widget_get_realized (widget))
    gtk_style_set_background (widget->style, spin->panel, GTK_STATE_NORMAL);

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->style_set (widget, previous_style);
}


static gint
gtk_spin_button_scroll (GtkWidget      *widget,
			GdkEventScroll *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (event->direction == GDK_SCROLL_UP)
    {
      if (!gtk_widget_has_focus (widget))
	gtk_widget_grab_focus (widget);
      gtk_spin_button_real_spin (spin, spin->adjustment->step_increment);
    }
  else if (event->direction == GDK_SCROLL_DOWN)
    {
      if (!gtk_widget_has_focus (widget))
	gtk_widget_grab_focus (widget);
      gtk_spin_button_real_spin (spin, -spin->adjustment->step_increment); 
    }
  else
    return FALSE;

  return TRUE;
}

static void
gtk_spin_button_stop_spinning (GtkSpinButton *spin)
{
  if (spin->timer)
    {
      g_source_remove (spin->timer);
      spin->timer = 0;
      spin->timer_calls = 0;
      spin->need_timer = FALSE;
    }

  spin->button = 0;
  spin->timer = 0;
  spin->timer_step = spin->adjustment->step_increment;
  spin->timer_calls = 0;

  spin->click_child = NO_ARROW;
  spin->button = 0;
}

static void
start_spinning (GtkSpinButton *spin,
		GtkArrowType   click_child,
		gdouble        step)
{
  g_return_if_fail (click_child == GTK_ARROW_UP || click_child == GTK_ARROW_DOWN);
  
  spin->click_child = click_child;
  
  if (!spin->timer)
    {
      GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (spin));
      guint        timeout;

      g_object_get (settings, "gtk-timeout-initial", &timeout, NULL);

      spin->timer_step = step;
      spin->need_timer = TRUE;
      spin->timer = gdk_threads_add_timeout (timeout,
				   (GSourceFunc) gtk_spin_button_timer,
				   (gpointer) spin);
    }
  gtk_spin_button_real_spin (spin, click_child == GTK_ARROW_UP ? step : -step);

  gtk_widget_queue_draw (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_button_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (!spin->button)
    {
      if (event->window == spin->panel)
	{
	  if (!gtk_widget_has_focus (widget))
	    gtk_widget_grab_focus (widget);
	  spin->button = event->button;
	  
	  if (GTK_ENTRY (widget)->editable)
	    gtk_spin_button_update (spin);
	  
	  if (event->y <= widget->requisition.height / 2)
	    {
	      if (event->button == 1)
		start_spinning (spin, GTK_ARROW_UP, spin->adjustment->step_increment);
	      else if (event->button == 2)
		start_spinning (spin, GTK_ARROW_UP, spin->adjustment->page_increment);
	      else
		spin->click_child = GTK_ARROW_UP;
	    }
	  else 
	    {
	      if (event->button == 1)
		start_spinning (spin, GTK_ARROW_DOWN, spin->adjustment->step_increment);
	      else if (event->button == 2)
		start_spinning (spin, GTK_ARROW_DOWN, spin->adjustment->page_increment);
	      else
		spin->click_child = GTK_ARROW_DOWN;
	    }
	  return TRUE;
	}
      else
	return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->button_press_event (widget, event);
    }
  return FALSE;
}

static gint
gtk_spin_button_button_release (GtkWidget      *widget,
				GdkEventButton *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  gint arrow_size;

  arrow_size = spin_button_get_arrow_size (spin);

  if (event->button == spin->button)
    {
      int click_child = spin->click_child;

      gtk_spin_button_stop_spinning (spin);

      if (event->button == 3)
	{
	  if (event->y >= 0 && event->x >= 0 && 
	      event->y <= widget->requisition.height &&
	      event->x <= arrow_size + 2 * widget->style->xthickness)
	    {
	      if (click_child == GTK_ARROW_UP &&
		  event->y <= widget->requisition.height / 2)
		{
		  gdouble diff;

		  diff = spin->adjustment->upper - spin->adjustment->value;
		  if (diff > EPSILON)
		    gtk_spin_button_real_spin (spin, diff);
		}
	      else if (click_child == GTK_ARROW_DOWN &&
		       event->y > widget->requisition.height / 2)
		{
		  gdouble diff;

		  diff = spin->adjustment->value - spin->adjustment->lower;
		  if (diff > EPSILON)
		    gtk_spin_button_real_spin (spin, -diff);
		}
	    }
	}		  
      gtk_widget_queue_draw (GTK_WIDGET (spin));

      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->button_release_event (widget, event);
}

static gint
gtk_spin_button_motion_notify (GtkWidget      *widget,
			       GdkEventMotion *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (spin->button)
    return FALSE;

  if (event->window == spin->panel)
    {
      gint y = event->y;

      gdk_event_request_motions (event);
  
      if (y <= widget->requisition.height / 2 && 
	  spin->in_child == GTK_ARROW_DOWN)
	{
	  spin->in_child = GTK_ARROW_UP;
	  gtk_widget_queue_draw (GTK_WIDGET (spin));
	}
      else if (y > widget->requisition.height / 2 && 
	  spin->in_child == GTK_ARROW_UP)
	{
	  spin->in_child = GTK_ARROW_DOWN;
	  gtk_widget_queue_draw (GTK_WIDGET (spin));
	}
      
      return FALSE;
    }
	  
  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->motion_notify_event (widget, event);
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  gboolean retval = FALSE;
  
  if (spin_button->timer)
    {
      if (spin_button->click_child == GTK_ARROW_UP)
	gtk_spin_button_real_spin (spin_button,	spin_button->timer_step);
      else
	gtk_spin_button_real_spin (spin_button,	-spin_button->timer_step);

      if (spin_button->need_timer)
	{
          GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (spin_button));
          guint        timeout;

          g_object_get (settings, "gtk-timeout-repeat", &timeout, NULL);

	  spin_button->need_timer = FALSE;
	  spin_button->timer = gdk_threads_add_timeout (timeout,
					      (GSourceFunc) gtk_spin_button_timer, 
					      (gpointer) spin_button);
	}
      else 
	{
	  if (spin_button->climb_rate > 0.0 && spin_button->timer_step 
	      < spin_button->adjustment->page_increment)
	    {
	      if (spin_button->timer_calls < MAX_TIMER_CALLS)
		spin_button->timer_calls++;
	      else 
		{
		  spin_button->timer_calls = 0;
		  spin_button->timer_step += spin_button->climb_rate;
		}
	    }
	  retval = TRUE;
	}
    }

  return retval;
}

static void
gtk_spin_button_value_changed (GtkAdjustment *adjustment,
			       GtkSpinButton *spin_button)
{
  gboolean return_val;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);
  if (return_val == FALSE)
    gtk_spin_button_default_output (spin_button);

  g_signal_emit (spin_button, spinbutton_signals[VALUE_CHANGED], 0);

  gtk_widget_queue_draw (GTK_WIDGET (spin_button));
  
  g_object_notify (G_OBJECT (spin_button), "value");
}

static void
gtk_spin_button_real_change_value (GtkSpinButton *spin,
				   GtkScrollType  scroll)
{
  gdouble old_value;

  /* When the key binding is activated, there may be an outstanding
   * value, so we first have to commit what is currently written in
   * the spin buttons text entry. See #106574
   */
  gtk_spin_button_update (spin);

  old_value = spin->adjustment->value;

  /* We don't test whether the entry is editable, since
   * this key binding conceptually corresponds to changing
   * the value with the buttons using the mouse, which
   * we allow for non-editable spin buttons.
   */
  switch (scroll)
    {
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_DOWN:
    case GTK_SCROLL_STEP_LEFT:
      gtk_spin_button_real_spin (spin, -spin->timer_step);
      
      if (spin->climb_rate > 0.0 && spin->timer_step
	  < spin->adjustment->page_increment)
	{
	  if (spin->timer_calls < MAX_TIMER_CALLS)
	    spin->timer_calls++;
	  else 
	    {
	      spin->timer_calls = 0;
	      spin->timer_step += spin->climb_rate;
	    }
	}
      break;
      
    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_STEP_UP:
    case GTK_SCROLL_STEP_RIGHT:
      gtk_spin_button_real_spin (spin, spin->timer_step);
      
      if (spin->climb_rate > 0.0 && spin->timer_step
	  < spin->adjustment->page_increment)
	{
	  if (spin->timer_calls < MAX_TIMER_CALLS)
	    spin->timer_calls++;
	  else 
	    {
	      spin->timer_calls = 0;
	      spin->timer_step += spin->climb_rate;
	    }
	}
      break;
      
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_DOWN:
    case GTK_SCROLL_PAGE_LEFT:
      gtk_spin_button_real_spin (spin, -spin->adjustment->page_increment);
      break;
      
    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_RIGHT:
      gtk_spin_button_real_spin (spin, spin->adjustment->page_increment);
      break;
      
    case GTK_SCROLL_START:
      {
	gdouble diff = spin->adjustment->value - spin->adjustment->lower;
	if (diff > EPSILON)
	  gtk_spin_button_real_spin (spin, -diff);
	break;
      }
      
    case GTK_SCROLL_END:
      {
	gdouble diff = spin->adjustment->upper - spin->adjustment->value;
	if (diff > EPSILON)
	  gtk_spin_button_real_spin (spin, diff);
	break;
      }
      
    default:
      g_warning ("Invalid scroll type %d for GtkSpinButton::change-value", scroll);
      break;
    }
  
  gtk_spin_button_update (spin);

  if (spin->adjustment->value == old_value)
    gtk_widget_error_bell (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_key_release (GtkWidget   *widget,
			     GdkEventKey *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  /* We only get a release at the end of a key repeat run, so reset the timer_step */
  spin->timer_step = spin->adjustment->step_increment;
  spin->timer_calls = 0;
  
  return TRUE;
}

static void
gtk_spin_button_snap (GtkSpinButton *spin_button,
		      gdouble        val)
{
  gdouble inc;
  gdouble tmp;

  inc = spin_button->adjustment->step_increment;
  if (inc == 0)
    return;
  
  tmp = (val - spin_button->adjustment->lower) / inc;
  if (tmp - floor (tmp) < ceil (tmp) - tmp)
    val = spin_button->adjustment->lower + floor (tmp) * inc;
  else
    val = spin_button->adjustment->lower + ceil (tmp) * inc;

  gtk_spin_button_set_value (spin_button, val);
}

static void
gtk_spin_button_activate (GtkEntry *entry)
{
  if (entry->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (entry));

  /* Chain up so that entry->activates_default is honored */
  GTK_ENTRY_CLASS (gtk_spin_button_parent_class)->activate (entry);
}

static void
gtk_spin_button_get_text_area_size (GtkEntry *entry,
				    gint     *x,
				    gint     *y,
				    gint     *width,
				    gint     *height)
{
  gint arrow_size;
  gint panel_width;

  GTK_ENTRY_CLASS (gtk_spin_button_parent_class)->get_text_area_size (entry, x, y, width, height);

  arrow_size = spin_button_get_arrow_size (GTK_SPIN_BUTTON (entry));
  panel_width = arrow_size + 2 * GTK_WIDGET (entry)->style->xthickness;

  if (width)
    *width -= panel_width;

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL && x)
    *x += panel_width;
}

static void
gtk_spin_button_insert_text (GtkEditable *editable,
			     const gchar *new_text,
			     gint         new_text_length,
			     gint        *position)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkSpinButton *spin = GTK_SPIN_BUTTON (editable);
  GtkEditableClass *parent_editable_iface = g_type_interface_peek (gtk_spin_button_parent_class, GTK_TYPE_EDITABLE);
 
  if (spin->numeric)
    {
      struct lconv *lc;
      gboolean sign;
      gint dotpos = -1;
      gint i;
      GdkWChar pos_sign;
      GdkWChar neg_sign;
      gint entry_length;
      const gchar *entry_text;

      entry_length = gtk_entry_get_text_length (entry);
      entry_text = gtk_entry_get_text (entry);

      lc = localeconv ();

      if (*(lc->negative_sign))
	neg_sign = *(lc->negative_sign);
      else 
	neg_sign = '-';

      if (*(lc->positive_sign))
	pos_sign = *(lc->positive_sign);
      else 
	pos_sign = '+';

#ifdef G_OS_WIN32
      /* Workaround for bug caused by some Windows application messing
       * up the positive sign of the current locale, more specifically
       * HKEY_CURRENT_USER\Control Panel\International\sPositiveSign.
       * See bug #330743 and for instance
       * http://www.msnewsgroups.net/group/microsoft.public.dotnet.languages.csharp/topic36024.aspx
       *
       * I don't know if the positive sign always gets bogusly set to
       * a digit when the above Registry value is corrupted as
       * described. (In my test case, it got set to "8", and in the
       * bug report above it presumably was set ot "0".) Probably it
       * might get set to almost anything? So how to distinguish a
       * bogus value from some correct one for some locale? That is
       * probably hard, but at least we should filter out the
       * digits...
       */
      if (pos_sign >= '0' && pos_sign <= '9')
	pos_sign = '+';
#endif

      for (sign=0, i=0; i<entry_length; i++)
	if ((entry_text[i] == neg_sign) ||
	    (entry_text[i] == pos_sign))
	  {
	    sign = 1;
	    break;
	  }

      if (sign && !(*position))
	return;

      for (dotpos=-1, i=0; i<entry_length; i++)
	if (entry_text[i] == *(lc->decimal_point))
	  {
	    dotpos = i;
	    break;
	  }

      if (dotpos > -1 && *position > dotpos &&
	  (gint)spin->digits - entry_length
	    + dotpos - new_text_length + 1 < 0)
	return;

      for (i = 0; i < new_text_length; i++)
	{
	  if (new_text[i] == neg_sign || new_text[i] == pos_sign)
	    {
	      if (sign || (*position) || i)
		return;
	      sign = TRUE;
	    }
	  else if (new_text[i] == *(lc->decimal_point))
	    {
	      if (!spin->digits || dotpos > -1 || 
 		  (new_text_length - 1 - i + entry_length
		    - *position > (gint)spin->digits)) 
		return;
	      dotpos = *position + i;
	    }
	  else if (new_text[i] < 0x30 || new_text[i] > 0x39)
	    return;
	}
    }

  parent_editable_iface->insert_text (editable, new_text,
				      new_text_length, position);
}

static void
gtk_spin_button_real_spin (GtkSpinButton *spin_button,
			   gdouble        increment)
{
  GtkAdjustment *adj;
  gdouble new_value = 0.0;
  gboolean wrapped = FALSE;
  
  adj = spin_button->adjustment;

  new_value = adj->value + increment;

  if (increment > 0)
    {
      if (spin_button->wrap)
	{
	  if (fabs (adj->value - adj->upper) < EPSILON)
	    {
	      new_value = adj->lower;
	      wrapped = TRUE;
	    }
	  else if (new_value > adj->upper)
	    new_value = adj->upper;
	}
      else
	new_value = MIN (new_value, adj->upper);
    }
  else if (increment < 0) 
    {
      if (spin_button->wrap)
	{
	  if (fabs (adj->value - adj->lower) < EPSILON)
	    {
	      new_value = adj->upper;
	      wrapped = TRUE;
	    }
	  else if (new_value < adj->lower)
	    new_value = adj->lower;
	}
      else
	new_value = MAX (new_value, adj->lower);
    }

  if (fabs (new_value - adj->value) > EPSILON)
    gtk_adjustment_set_value (adj, new_value);

  if (wrapped)
    g_signal_emit (spin_button, spinbutton_signals[WRAPPED], 0);

  gtk_widget_queue_draw (GTK_WIDGET (spin_button));
}

static gint
gtk_spin_button_default_input (GtkSpinButton *spin_button,
			       gdouble       *new_val)
{
  gchar *err = NULL;

  *new_val = g_strtod (gtk_entry_get_text (GTK_ENTRY (spin_button)), &err);
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return FALSE;
}

static gint
gtk_spin_button_default_output (GtkSpinButton *spin_button)
{
  gchar *buf = g_strdup_printf ("%0.*f", spin_button->digits, spin_button->adjustment->value);

  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  g_free (buf);
  return FALSE;
}


/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/


/**
 * gtk_spin_button_configure:
 * @spin_button: a #GtkSpinButton
 * @adjustment: (allow-none):  a #GtkAdjustment.
 * @climb_rate: the new climb rate.
 * @digits: the number of decimal places to display in the spin button.
 *
 * Changes the properties of an existing spin button. The adjustment, climb rate,
 * and number of decimal places are all changed accordingly, after this function call.
 */
void
gtk_spin_button_configure (GtkSpinButton  *spin_button,
			   GtkAdjustment  *adjustment,
			   gdouble         climb_rate,
			   guint           digits)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (adjustment)
    gtk_spin_button_set_adjustment (spin_button, adjustment);
  else
    adjustment = spin_button->adjustment;

  g_object_freeze_notify (G_OBJECT (spin_button));
  if (spin_button->digits != digits) 
    {
      spin_button->digits = digits;
      g_object_notify (G_OBJECT (spin_button), "digits");
    }

  if (spin_button->climb_rate != climb_rate)
    {
      spin_button->climb_rate = climb_rate;
      g_object_notify (G_OBJECT (spin_button), "climb-rate");
    }
  g_object_thaw_notify (G_OBJECT (spin_button));

  gtk_adjustment_value_changed (adjustment);
}

GtkWidget *
gtk_spin_button_new (GtkAdjustment *adjustment,
		     gdouble        climb_rate,
		     guint          digits)
{
  GtkSpinButton *spin;

  if (adjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  spin = g_object_new (GTK_TYPE_SPIN_BUTTON, NULL);

  gtk_spin_button_configure (spin, adjustment, climb_rate, digits);

  return GTK_WIDGET (spin);
}

/**
 * gtk_spin_button_new_with_range:
 * @min: Minimum allowable value
 * @max: Maximum allowable value
 * @step: Increment added or subtracted by spinning the widget
 * 
 * This is a convenience constructor that allows creation of a numeric 
 * #GtkSpinButton without manually creating an adjustment. The value is 
 * initially set to the minimum value and a page increment of 10 * @step
 * is the default. The precision of the spin button is equivalent to the 
 * precision of @step. 
 * 
 * Note that the way in which the precision is derived works best if @step 
 * is a power of ten. If the resulting precision is not suitable for your 
 * needs, use gtk_spin_button_set_digits() to correct it.
 * 
 * Return value: The new spin button as a #GtkWidget.
 **/
GtkWidget *
gtk_spin_button_new_with_range (gdouble min,
				gdouble max,
				gdouble step)
{
  GtkObject *adj;
  GtkSpinButton *spin;
  gint digits;

  g_return_val_if_fail (min <= max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  spin = g_object_new (GTK_TYPE_SPIN_BUTTON, NULL);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  if (fabs (step) >= 1.0 || step == 0.0)
    digits = 0;
  else {
    digits = abs ((gint) floor (log10 (fabs (step))));
    if (digits > MAX_DIGITS)
      digits = MAX_DIGITS;
  }

  gtk_spin_button_configure (spin, GTK_ADJUSTMENT (adj), step, digits);

  gtk_spin_button_set_numeric (spin, TRUE);

  return GTK_WIDGET (spin);
}

static void
warn_nonzero_page_size (GtkAdjustment *adjustment)
{
  if (gtk_adjustment_get_page_size (adjustment) != 0.0)
    g_warning ("GtkSpinButton: setting an adjustment with non-zero page size is deprecated");
}

/* Callback used when the spin button's adjustment changes.  We need to redraw
 * the arrows when the adjustment's range changes, and reevaluate our size request.
 */
static void
adjustment_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (data);

  spin_button->timer_step = spin_button->adjustment->step_increment;
  warn_nonzero_page_size (adjustment);
  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

/**
 * gtk_spin_button_set_adjustment:
 * @spin_button: a #GtkSpinButton
 * @adjustment: a #GtkAdjustment to replace the existing adjustment
 * 
 * Replaces the #GtkAdjustment associated with @spin_button.
 **/
void
gtk_spin_button_set_adjustment (GtkSpinButton *spin_button,
				GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->adjustment != adjustment)
    {
      if (spin_button->adjustment)
        {
	  g_signal_handlers_disconnect_by_func (spin_button->adjustment,
						gtk_spin_button_value_changed,
						spin_button);
	  g_signal_handlers_disconnect_by_func (spin_button->adjustment,
						adjustment_changed_cb,
						spin_button);
	  g_object_unref (spin_button->adjustment);
        }
      spin_button->adjustment = adjustment;
      if (adjustment)
        {
	  g_object_ref_sink (adjustment);
	  g_signal_connect (adjustment, "value-changed",
			    G_CALLBACK (gtk_spin_button_value_changed),
			    spin_button);
	  g_signal_connect (adjustment, "changed",
			    G_CALLBACK (adjustment_changed_cb),
			    spin_button);
	  spin_button->timer_step = spin_button->adjustment->step_increment;
          warn_nonzero_page_size (adjustment);
        }

      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }

  g_object_notify (G_OBJECT (spin_button), "adjustment");
}

/**
 * gtk_spin_button_get_adjustment:
 * @spin_button: a #GtkSpinButton
 * 
 * Get the adjustment associated with a #GtkSpinButton
 * 
 * Return value: (transfer none): the #GtkAdjustment of @spin_button
 **/
GtkAdjustment *
gtk_spin_button_get_adjustment (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), NULL);

  return spin_button->adjustment;
}

/**
 * gtk_spin_button_set_digits:
 * @spin_button: a #GtkSpinButton
 * @digits: the number of digits after the decimal point to be displayed for the spin button's value
 * 
 * Set the precision to be displayed by @spin_button. Up to 20 digit precision
 * is allowed.
 **/
void
gtk_spin_button_set_digits (GtkSpinButton *spin_button,
			    guint          digits)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->digits != digits)
    {
      spin_button->digits = digits;
      gtk_spin_button_value_changed (spin_button->adjustment, spin_button);
      g_object_notify (G_OBJECT (spin_button), "digits");
      
      /* since lower/upper may have changed */
      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }
}

/**
 * gtk_spin_button_get_digits:
 * @spin_button: a #GtkSpinButton
 *
 * Fetches the precision of @spin_button. See gtk_spin_button_set_digits().
 *
 * Returns: the current precision
 **/
guint
gtk_spin_button_get_digits (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  return spin_button->digits;
}

/**
 * gtk_spin_button_set_increments:
 * @spin_button: a #GtkSpinButton
 * @step: increment applied for a button 1 press.
 * @page: increment applied for a button 2 press.
 * 
 * Sets the step and page increments for spin_button.  This affects how 
 * quickly the value changes when the spin button's arrows are activated.
 **/
void
gtk_spin_button_set_increments (GtkSpinButton *spin_button,
				gdouble        step,
				gdouble        page)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->adjustment->step_increment = step;
  spin_button->adjustment->page_increment = page;
}

/**
 * gtk_spin_button_get_increments:
 * @spin_button: a #GtkSpinButton
 * @step: (out) (allow-none): location to store step increment, or %NULL
 * @page: (out) (allow-none): location to store page increment, or %NULL
 *
 * Gets the current step and page the increments used by @spin_button. See
 * gtk_spin_button_set_increments().
 **/
void
gtk_spin_button_get_increments (GtkSpinButton *spin_button,
				gdouble       *step,
				gdouble       *page)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (step)
    *step = spin_button->adjustment->step_increment;
  if (page)
    *page = spin_button->adjustment->page_increment;
}

/**
 * gtk_spin_button_set_range:
 * @spin_button: a #GtkSpinButton
 * @min: minimum allowable value
 * @max: maximum allowable value
 * 
 * Sets the minimum and maximum allowable values for @spin_button
 **/
void
gtk_spin_button_set_range (GtkSpinButton *spin_button,
			   gdouble        min,
			   gdouble        max)
{
  gdouble value;
  
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->adjustment->lower = min;
  spin_button->adjustment->upper = max;

  value = CLAMP (spin_button->adjustment->value,
                 spin_button->adjustment->lower,
                 (spin_button->adjustment->upper - spin_button->adjustment->page_size));

  if (value != spin_button->adjustment->value)
    gtk_spin_button_set_value (spin_button, value);

  gtk_adjustment_changed (spin_button->adjustment);
}

/**
 * gtk_spin_button_get_range:
 * @spin_button: a #GtkSpinButton
 * @min: (out) (allow-none): location to store minimum allowed value, or %NULL
 * @max: (out) (allow-none): location to store maximum allowed value, or %NULL
 *
 * Gets the range allowed for @spin_button. See
 * gtk_spin_button_set_range().
 **/
void
gtk_spin_button_get_range (GtkSpinButton *spin_button,
			   gdouble       *min,
			   gdouble       *max)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (min)
    *min = spin_button->adjustment->lower;
  if (max)
    *max = spin_button->adjustment->upper;
}

/**
 * gtk_spin_button_get_value:
 * @spin_button: a #GtkSpinButton
 * 
 * Get the value in the @spin_button.
 * 
 * Return value: the value of @spin_button
 **/
gdouble
gtk_spin_button_get_value (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0.0);

  return spin_button->adjustment->value;
}

/**
 * gtk_spin_button_get_value_as_int:
 * @spin_button: a #GtkSpinButton
 * 
 * Get the value @spin_button represented as an integer.
 * 
 * Return value: the value of @spin_button
 **/
gint
gtk_spin_button_get_value_as_int (GtkSpinButton *spin_button)
{
  gdouble val;

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  val = spin_button->adjustment->value;
  if (val - floor (val) < ceil (val) - val)
    return floor (val);
  else
    return ceil (val);
}

/**
 * gtk_spin_button_set_value:
 * @spin_button: a #GtkSpinButton
 * @value: the new value
 * 
 * Set the value of @spin_button.
 **/
void 
gtk_spin_button_set_value (GtkSpinButton *spin_button, 
			   gdouble        value)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (fabs (value - spin_button->adjustment->value) > EPSILON)
    gtk_adjustment_set_value (spin_button->adjustment, value);
  else
    {
      gint return_val = FALSE;
      g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);
      if (return_val == FALSE)
	gtk_spin_button_default_output (spin_button);
    }
}

/**
 * gtk_spin_button_set_update_policy:
 * @spin_button: a #GtkSpinButton 
 * @policy: a #GtkSpinButtonUpdatePolicy value
 * 
 * Sets the update behavior of a spin button. This determines whether the
 * spin button is always updated or only when a valid value is set.
 **/
void
gtk_spin_button_set_update_policy (GtkSpinButton             *spin_button,
				   GtkSpinButtonUpdatePolicy  policy)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->update_policy != policy)
    {
      spin_button->update_policy = policy;
      g_object_notify (G_OBJECT (spin_button), "update-policy");
    }
}

/**
 * gtk_spin_button_get_update_policy:
 * @spin_button: a #GtkSpinButton
 *
 * Gets the update behavior of a spin button. See
 * gtk_spin_button_set_update_policy().
 *
 * Return value: the current update policy
 **/
GtkSpinButtonUpdatePolicy
gtk_spin_button_get_update_policy (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), GTK_UPDATE_ALWAYS);

  return spin_button->update_policy;
}

/**
 * gtk_spin_button_set_numeric:
 * @spin_button: a #GtkSpinButton 
 * @numeric: flag indicating if only numeric entry is allowed. 
 * 
 * Sets the flag that determines if non-numeric text can be typed into
 * the spin button.
 **/
void
gtk_spin_button_set_numeric (GtkSpinButton  *spin_button,
			     gboolean        numeric)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  numeric = numeric != FALSE;

  if (spin_button->numeric != numeric)
    {
       spin_button->numeric = numeric;
       g_object_notify (G_OBJECT (spin_button), "numeric");
    }
}

/**
 * gtk_spin_button_get_numeric:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether non-numeric text can be typed into the spin button.
 * See gtk_spin_button_set_numeric().
 *
 * Return value: %TRUE if only numeric text can be entered
 **/
gboolean
gtk_spin_button_get_numeric (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->numeric;
}

/**
 * gtk_spin_button_set_wrap:
 * @spin_button: a #GtkSpinButton 
 * @wrap: a flag indicating if wrapping behavior is performed.
 * 
 * Sets the flag that determines if a spin button value wraps around to the
 * opposite limit when the upper or lower limit of the range is exceeded.
 **/
void
gtk_spin_button_set_wrap (GtkSpinButton  *spin_button,
			  gboolean        wrap)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  wrap = wrap != FALSE; 

  if (spin_button->wrap != wrap)
    {
       spin_button->wrap = (wrap != 0);
  
       g_object_notify (G_OBJECT (spin_button), "wrap");
    }
}

/**
 * gtk_spin_button_get_wrap:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether the spin button's value wraps around to the
 * opposite limit when the upper or lower limit of the range is
 * exceeded. See gtk_spin_button_set_wrap().
 *
 * Return value: %TRUE if the spin button wraps around
 **/
gboolean
gtk_spin_button_get_wrap (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->wrap;
}

static gint
spin_button_get_arrow_size (GtkSpinButton *spin_button)
{
  gint size = pango_font_description_get_size (GTK_WIDGET (spin_button)->style->font_desc);
  gint arrow_size;

  arrow_size = MAX (PANGO_PIXELS (size), MIN_ARROW_WIDTH);

  return arrow_size - arrow_size % 2; /* force even */
}

/**
 * spin_button_get_shadow_type:
 * @spin_button: a #GtkSpinButton 
 * 
 * Convenience function to Get the shadow type from the underlying widget's
 * style.
 * 
 * Return value: the #GtkShadowType
 **/
static gint
spin_button_get_shadow_type (GtkSpinButton *spin_button)
{
  GtkShadowType rc_shadow_type;

  gtk_widget_style_get (GTK_WIDGET (spin_button), "shadow-type", &rc_shadow_type, NULL);

  return rc_shadow_type;
}

/**
 * gtk_spin_button_set_snap_to_ticks:
 * @spin_button: a #GtkSpinButton 
 * @snap_to_ticks: a flag indicating if invalid values should be corrected.
 * 
 * Sets the policy as to whether values are corrected to the nearest step 
 * increment when a spin button is activated after providing an invalid value.
 **/
void
gtk_spin_button_set_snap_to_ticks (GtkSpinButton *spin_button,
				   gboolean       snap_to_ticks)
{
  guint new_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  new_val = (snap_to_ticks != 0);

  if (new_val != spin_button->snap_to_ticks)
    {
      spin_button->snap_to_ticks = new_val;
      if (new_val && GTK_ENTRY (spin_button)->editable)
	gtk_spin_button_update (spin_button);
      
      g_object_notify (G_OBJECT (spin_button), "snap-to-ticks");
    }
}

/**
 * gtk_spin_button_get_snap_to_ticks:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether the values are corrected to the nearest step. See
 * gtk_spin_button_set_snap_to_ticks().
 *
 * Return value: %TRUE if values are snapped to the nearest step.
 **/
gboolean
gtk_spin_button_get_snap_to_ticks (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->snap_to_ticks;
}

/**
 * gtk_spin_button_spin:
 * @spin_button: a #GtkSpinButton 
 * @direction: a #GtkSpinType indicating the direction to spin.
 * @increment: step increment to apply in the specified direction.
 * 
 * Increment or decrement a spin button's value in a specified direction
 * by a specified amount. 
 **/
void
gtk_spin_button_spin (GtkSpinButton *spin_button,
		      GtkSpinType    direction,
		      gdouble        increment)
{
  GtkAdjustment *adj;
  gdouble diff;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  adj = spin_button->adjustment;

  /* for compatibility with the 1.0.x version of this function */
  if (increment != 0 && increment != adj->step_increment &&
      (direction == GTK_SPIN_STEP_FORWARD ||
       direction == GTK_SPIN_STEP_BACKWARD))
    {
      if (direction == GTK_SPIN_STEP_BACKWARD && increment > 0)
	increment = -increment;
      direction = GTK_SPIN_USER_DEFINED;
    }

  switch (direction)
    {
    case GTK_SPIN_STEP_FORWARD:

      gtk_spin_button_real_spin (spin_button, adj->step_increment);
      break;

    case GTK_SPIN_STEP_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -adj->step_increment);
      break;

    case GTK_SPIN_PAGE_FORWARD:

      gtk_spin_button_real_spin (spin_button, adj->page_increment);
      break;

    case GTK_SPIN_PAGE_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -adj->page_increment);
      break;

    case GTK_SPIN_HOME:

      diff = adj->value - adj->lower;
      if (diff > EPSILON)
	gtk_spin_button_real_spin (spin_button, -diff);
      break;

    case GTK_SPIN_END:

      diff = adj->upper - adj->value;
      if (diff > EPSILON)
	gtk_spin_button_real_spin (spin_button, diff);
      break;

    case GTK_SPIN_USER_DEFINED:

      if (increment != 0)
	gtk_spin_button_real_spin (spin_button, increment);
      break;

    default:
      break;
    }
}

/**
 * gtk_spin_button_update:
 * @spin_button: a #GtkSpinButton 
 * 
 * Manually force an update of the spin button.
 **/
void 
gtk_spin_button_update (GtkSpinButton *spin_button)
{
  gdouble val;
  gint error = 0;
  gint return_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[INPUT], 0, &val, &return_val);
  if (return_val == FALSE)
    {
      return_val = gtk_spin_button_default_input (spin_button, &val);
      error = (return_val == GTK_INPUT_ERROR);
    }
  else if (return_val == GTK_INPUT_ERROR)
    error = 1;

  gtk_widget_queue_draw (GTK_WIDGET (spin_button));

  if (spin_button->update_policy == GTK_UPDATE_ALWAYS)
    {
      if (val < spin_button->adjustment->lower)
	val = spin_button->adjustment->lower;
      else if (val > spin_button->adjustment->upper)
	val = spin_button->adjustment->upper;
    }
  else if ((spin_button->update_policy == GTK_UPDATE_IF_VALID) && 
	   (error ||
	   val < spin_button->adjustment->lower ||
	   val > spin_button->adjustment->upper))
    {
      gtk_spin_button_value_changed (spin_button->adjustment, spin_button);
      return;
    }

  if (spin_button->snap_to_ticks)
    gtk_spin_button_snap (spin_button, val);
  else
    gtk_spin_button_set_value (spin_button, val);
}

#define __GTK_SPIN_BUTTON_C__
#include "gtkaliasdef.c"
