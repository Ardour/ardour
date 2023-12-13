/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <stdio.h>
#include <math.h>

#undef GTK_DISABLE_DEPRECATED

#include <gdk/gdkkeysyms.h>
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkrange.h"
#include "gtkscale.h"
#include "gtkscrollbar.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define SCROLL_DELAY_FACTOR 5    /* Scroll repeat multiplier */
#define UPDATE_DELAY        300  /* Delay for queued update */

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_UPDATE_POLICY,
  PROP_ADJUSTMENT,
  PROP_INVERTED,
  PROP_LOWER_STEPPER_SENSITIVITY,
  PROP_UPPER_STEPPER_SENSITIVITY,
  PROP_SHOW_FILL_LEVEL,
  PROP_RESTRICT_TO_FILL_LEVEL,
  PROP_FILL_LEVEL,
  PROP_ROUND_DIGITS
};

enum {
  VALUE_CHANGED,
  ADJUST_BOUNDS,
  MOVE_SLIDER,
  CHANGE_VALUE,
  LAST_SIGNAL
};

typedef enum {
  MOUSE_OUTSIDE,
  MOUSE_STEPPER_A,
  MOUSE_STEPPER_B,
  MOUSE_STEPPER_C,
  MOUSE_STEPPER_D,
  MOUSE_TROUGH,
  MOUSE_SLIDER,
  MOUSE_WIDGET /* inside widget but not in any of the above GUI elements */
} MouseLocation;

typedef enum {
  STEPPER_A,
  STEPPER_B,
  STEPPER_C,
  STEPPER_D
} Stepper;

#define GTK_RANGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_RANGE, GtkRangeLayout))

struct _GtkRangeLayout
{
  /* These are in widget->window coordinates */
  GdkRectangle stepper_a;
  GdkRectangle stepper_b;
  GdkRectangle stepper_c;
  GdkRectangle stepper_d;
  /* The trough rectangle is the area the thumb can slide in, not the
   * entire range_rect
   */
  GdkRectangle trough;
  GdkRectangle slider;

  /* Layout-related state */

  MouseLocation mouse_location;
  /* last mouse coords we got, or -1 if mouse is outside the range */
  gint mouse_x;
  gint mouse_y;

  /* "grabbed" mouse location, OUTSIDE for no grab */
  MouseLocation grab_location;
  guint grab_button : 8; /* 0 if none */

  /* Stepper sensitivity */
  guint lower_sensitive : 1;
  guint upper_sensitive : 1;

  /* Fill level */
  guint show_fill_level : 1;
  guint restrict_to_fill_level : 1;

  GtkSensitivityType lower_sensitivity;
  GtkSensitivityType upper_sensitivity;
  guint repaint_id;

  gdouble fill_level;

  GQuark slider_detail_quark;
  GQuark stepper_detail_quark[4];

  gdouble *marks;
  gint *mark_pos;
  gint n_marks;
  gboolean recalc_marks;
};


static void gtk_range_set_property   (GObject          *object,
                                      guint             prop_id,
                                      const GValue     *value,
                                      GParamSpec       *pspec);
static void gtk_range_get_property   (GObject          *object,
                                      guint             prop_id,
                                      GValue           *value,
                                      GParamSpec       *pspec);
static void gtk_range_destroy        (GtkObject        *object);
static void gtk_range_size_request   (GtkWidget        *widget,
                                      GtkRequisition   *requisition);
static void gtk_range_size_allocate  (GtkWidget        *widget,
                                      GtkAllocation    *allocation);
static void gtk_range_realize        (GtkWidget        *widget);
static void gtk_range_unrealize      (GtkWidget        *widget);
static void gtk_range_map            (GtkWidget        *widget);
static void gtk_range_unmap          (GtkWidget        *widget);
static gboolean gtk_range_expose         (GtkWidget        *widget,
                                      GdkEventExpose   *event);
static gboolean gtk_range_button_press   (GtkWidget        *widget,
                                      GdkEventButton   *event);
static gboolean gtk_range_button_release (GtkWidget        *widget,
                                      GdkEventButton   *event);
static gboolean gtk_range_motion_notify  (GtkWidget        *widget,
                                      GdkEventMotion   *event);
static gboolean gtk_range_enter_notify   (GtkWidget        *widget,
                                      GdkEventCrossing *event);
static gboolean gtk_range_leave_notify   (GtkWidget        *widget,
                                      GdkEventCrossing *event);
static gboolean gtk_range_grab_broken (GtkWidget          *widget,
				       GdkEventGrabBroken *event);
static void gtk_range_grab_notify    (GtkWidget          *widget,
				      gboolean            was_grabbed);
static void gtk_range_state_changed  (GtkWidget          *widget,
				      GtkStateType        previous_state);
static gboolean gtk_range_scroll_event   (GtkWidget        *widget,
                                      GdkEventScroll   *event);
static void gtk_range_style_set      (GtkWidget        *widget,
                                      GtkStyle         *previous_style);
static void update_slider_position   (GtkRange	       *range,
				      gint              mouse_x,
				      gint              mouse_y);
static void stop_scrolling           (GtkRange         *range);

/* Range methods */

static void gtk_range_move_slider              (GtkRange         *range,
                                                GtkScrollType     scroll);

/* Internals */
static gboolean      gtk_range_scroll                   (GtkRange      *range,
                                                         GtkScrollType  scroll);
static gboolean      gtk_range_update_mouse_location    (GtkRange      *range);
static void          gtk_range_calc_layout              (GtkRange      *range,
							 gdouble	adjustment_value);
static void          gtk_range_calc_marks               (GtkRange      *range);
static void          gtk_range_get_props                (GtkRange      *range,
                                                         gint          *slider_width,
                                                         gint          *stepper_size,
                                                         gint          *focus_width,
                                                         gint          *trough_border,
                                                         gint          *stepper_spacing,
                                                         gboolean      *trough_under_steppers,
							 gint          *arrow_displacement_x,
							 gint	       *arrow_displacement_y);
static void          gtk_range_calc_request             (GtkRange      *range,
                                                         gint           slider_width,
                                                         gint           stepper_size,
                                                         gint           focus_width,
                                                         gint           trough_border,
                                                         gint           stepper_spacing,
                                                         GdkRectangle  *range_rect,
                                                         GtkBorder     *border,
                                                         gint          *n_steppers_p,
                                                         gboolean      *has_steppers_ab,
                                                         gboolean      *has_steppers_cd,
                                                         gint          *slider_length_p);
static void          gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_adjustment_changed       (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_add_step_timer           (GtkRange      *range,
                                                         GtkScrollType  step);
static void          gtk_range_remove_step_timer        (GtkRange      *range);
static void          gtk_range_reset_update_timer       (GtkRange      *range);
static void          gtk_range_remove_update_timer      (GtkRange      *range);
static GdkRectangle* get_area                           (GtkRange      *range,
                                                         MouseLocation  location);
static gboolean      gtk_range_real_change_value        (GtkRange      *range,
                                                         GtkScrollType  scroll,
                                                         gdouble        value);
static void          gtk_range_update_value             (GtkRange      *range);
static gboolean      gtk_range_key_press                (GtkWidget     *range,
							 GdkEventKey   *event);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkRange, gtk_range, GTK_TYPE_WIDGET,
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                         NULL))

static guint signals[LAST_SIGNAL];


static void
gtk_range_class_init (GtkRangeClass *class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_range_set_property;
  gobject_class->get_property = gtk_range_get_property;

  object_class->destroy = gtk_range_destroy;

  widget_class->size_request = gtk_range_size_request;
  widget_class->size_allocate = gtk_range_size_allocate;
  widget_class->realize = gtk_range_realize;
  widget_class->unrealize = gtk_range_unrealize;  
  widget_class->map = gtk_range_map;
  widget_class->unmap = gtk_range_unmap;
  widget_class->expose_event = gtk_range_expose;
  widget_class->button_press_event = gtk_range_button_press;
  widget_class->button_release_event = gtk_range_button_release;
  widget_class->motion_notify_event = gtk_range_motion_notify;
  widget_class->scroll_event = gtk_range_scroll_event;
  widget_class->enter_notify_event = gtk_range_enter_notify;
  widget_class->leave_notify_event = gtk_range_leave_notify;
  widget_class->grab_broken_event = gtk_range_grab_broken;
  widget_class->grab_notify = gtk_range_grab_notify;
  widget_class->state_changed = gtk_range_state_changed;
  widget_class->style_set = gtk_range_style_set;
  widget_class->key_press_event = gtk_range_key_press;

  class->move_slider = gtk_range_move_slider;
  class->change_value = gtk_range_real_change_value;

  class->slider_detail = "slider";
  class->stepper_detail = "stepper";

  /**
   * GtkRange::value-changed:
   * @range: the #GtkRange
   *
   * Emitted when the range value changes.
   */
  signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, value_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  
  signals[ADJUST_BOUNDS] =
    g_signal_new (I_("adjust-bounds"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, adjust_bounds),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1,
                  G_TYPE_DOUBLE);
  
  /**
   * GtkRange::move-slider:
   * @range: the #GtkRange
   * @step: how to move the slider
   *
   * Virtual function that moves the slider. Used for keybindings.
   */
  signals[MOVE_SLIDER] =
    g_signal_new (I_("move-slider"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkRangeClass, move_slider),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkRange::change-value:
   * @range: the range that received the signal
   * @scroll: the type of scroll action that was performed
   * @value: the new value resulting from the scroll action
   * @returns: %TRUE to prevent other handlers from being invoked for the
   * signal, %FALSE to propagate the signal further
   *
   * The ::change-value signal is emitted when a scroll action is
   * performed on a range.  It allows an application to determine the
   * type of scroll event that occurred and the resultant new value.
   * The application can handle the event itself and return %TRUE to
   * prevent further processing.  Or, by returning %FALSE, it can pass
   * the event to other handlers until the default GTK+ handler is
   * reached.
   *
   * The value parameter is unrounded.  An application that overrides
   * the ::change-value signal is responsible for clamping the value to
   * the desired number of decimal digits; the default GTK+ handler
   * clamps the value based on #GtkRange:round_digits.
   *
   * It is not possible to use delayed update policies in an overridden
   * ::change-value handler.
   *
   * Since: 2.6
   */
  signals[CHANGE_VALUE] =
    g_signal_new (I_("change-value"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, change_value),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_DOUBLE,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_SCROLL_TYPE,
                  G_TYPE_DOUBLE);

  g_object_class_override_property (gobject_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_property (gobject_class,
                                   PROP_UPDATE_POLICY,
                                   g_param_spec_enum ("update-policy",
						      P_("Update policy"),
						      P_("How the range should be updated on the screen"),
						      GTK_TYPE_UPDATE_TYPE,
						      GTK_UPDATE_CONTINUOUS,
						      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
							P_("Adjustment"),
							P_("The GtkAdjustment that contains the current value of this range object"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_INVERTED,
                                   g_param_spec_boolean ("inverted",
							P_("Inverted"),
							P_("Invert direction slider moves to increase range value"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_LOWER_STEPPER_SENSITIVITY,
                                   g_param_spec_enum ("lower-stepper-sensitivity",
						      P_("Lower stepper sensitivity"),
						      P_("The sensitivity policy for the stepper that points to the adjustment's lower side"),
						      GTK_TYPE_SENSITIVITY_TYPE,
						      GTK_SENSITIVITY_AUTO,
						      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_UPPER_STEPPER_SENSITIVITY,
                                   g_param_spec_enum ("upper-stepper-sensitivity",
						      P_("Upper stepper sensitivity"),
						      P_("The sensitivity policy for the stepper that points to the adjustment's upper side"),
						      GTK_TYPE_SENSITIVITY_TYPE,
						      GTK_SENSITIVITY_AUTO,
						      GTK_PARAM_READWRITE));

  /**
   * GtkRange:show-fill-level:
   *
   * The show-fill-level property controls whether fill level indicator
   * graphics are displayed on the trough. See
   * gtk_range_set_show_fill_level().
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_FILL_LEVEL,
                                   g_param_spec_boolean ("show-fill-level",
                                                         P_("Show Fill Level"),
                                                         P_("Whether to display a fill level indicator graphics on trough."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkRange:restrict-to-fill-level:
   *
   * The restrict-to-fill-level property controls whether slider
   * movement is restricted to an upper boundary set by the
   * fill level. See gtk_range_set_restrict_to_fill_level().
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_RESTRICT_TO_FILL_LEVEL,
                                   g_param_spec_boolean ("restrict-to-fill-level",
                                                         P_("Restrict to Fill Level"),
                                                         P_("Whether to restrict the upper boundary to the fill level."),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkRange:fill-level:
   *
   * The fill level (e.g. prebuffering of a network stream).
   * See gtk_range_set_fill_level().
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILL_LEVEL,
                                   g_param_spec_double ("fill-level",
							P_("Fill Level"),
							P_("The fill level."),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkRange:round-digits:
   *
   * The number of digits to round the value to when
   * it changes, or -1. See #GtkRange::change-value.
   *
   * Since: 2.24
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ROUND_DIGITS,
                                   g_param_spec_int ("round-digits",
                                                     P_("Round Digits"),
                                                     P_("The number of digits to round the value to."),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("slider-width",
							     P_("Slider Width"),
							     P_("Width of scrollbar or scale thumb"),
							     0,
							     G_MAXINT,
							     14,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("trough-border",
                                                             P_("Trough Border"),
                                                             P_("Spacing between thumb/steppers and outer trough bevel"),
                                                             0,
                                                             G_MAXINT,
                                                             1,
                                                             GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("stepper-size",
							     P_("Stepper Size"),
							     P_("Length of step buttons at ends"),
							     0,
							     G_MAXINT,
							     14,
							     GTK_PARAM_READABLE));
  /**
   * GtkRange:stepper-spacing:
   *
   * The spacing between the stepper buttons and thumb. Note that
   * setting this value to anything > 0 will automatically set the
   * trough-under-steppers style property to %TRUE as well. Also,
   * stepper-spacing won't have any effect if there are no steppers.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("stepper-spacing",
							     P_("Stepper Spacing"),
							     P_("Spacing between step buttons and thumb"),
                                                             0,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("arrow-displacement-x",
							     P_("Arrow X Displacement"),
							     P_("How far in the x direction to move the arrow when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("arrow-displacement-y",
							     P_("Arrow Y Displacement"),
							     P_("How far in the y direction to move the arrow when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));

  /**
   * GtkRange:activate-slider:
   *
   * When %TRUE, sliders will be drawn active and with shadow in
   * while they are dragged.
   *
   * Deprecated: 2.22: This style property will be removed in GTK+ 3
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("activate-slider",
                                                                 P_("Draw slider ACTIVE during drag"),
							         P_("With this option set to TRUE, sliders will be drawn ACTIVE and with shadow IN while they are dragged"),
							         FALSE,
							         GTK_PARAM_READABLE));

  /**
   * GtkRange:trough-side-details:
   *
   * When %TRUE, the parts of the trough on the two sides of the 
   * slider are drawn with different details.
   *
   * Since: 2.10
   *
   * Deprecated: 2.22: This style property will be removed in GTK+ 3
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("trough-side-details",
                                                                 P_("Trough Side Details"),
                                                                 P_("When TRUE, the parts of the trough on the two sides of the slider are drawn with different details"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkRange:trough-under-steppers:
   *
   * Whether to draw the trough across the full length of the range or
   * to exclude the steppers and their spacing. Note that setting the
   * #GtkRange:stepper-spacing style property to any value > 0 will
   * automatically enable trough-under-steppers too.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("trough-under-steppers",
                                                                 P_("Trough Under Steppers"),
                                                                 P_("Whether to draw trough for full length of range or exclude the steppers and spacing"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkRange:arrow-scaling:
   *
   * The arrow size proportion relative to the scroll button size.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
							       P_("Arrow scaling"),
							       P_("Arrow scaling with regard to scroll button size"),
							       0.0, 1.0, 0.5,
							       GTK_PARAM_READABLE));

  /**
   * GtkRange:stepper-position-details:
   *
   * When %TRUE, the detail string for rendering the steppers will be
   * suffixed with information about the stepper position.
   *
   * Since: 2.22
   *
   * Deprecated: 2.22: This style property will be removed in GTK+ 3
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("stepper-position-details",
                                                                 P_("Stepper Position Details"),
                                                                 P_("When TRUE, the detail string for rendering the steppers is suffixed with position information"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  g_type_class_add_private (class, sizeof (GtkRangeLayout));
}

static void
gtk_range_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkRange *range = GTK_RANGE (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      range->orientation = g_value_get_enum (value);

      range->layout->slider_detail_quark = 0;
      range->layout->stepper_detail_quark[0] = 0;
      range->layout->stepper_detail_quark[1] = 0;
      range->layout->stepper_detail_quark[2] = 0;
      range->layout->stepper_detail_quark[3] = 0;

      gtk_widget_queue_resize (GTK_WIDGET (range));
      break;
    case PROP_UPDATE_POLICY:
      gtk_range_set_update_policy (range, g_value_get_enum (value));
      break;
    case PROP_ADJUSTMENT:
      gtk_range_set_adjustment (range, g_value_get_object (value));
      break;
    case PROP_INVERTED:
      gtk_range_set_inverted (range, g_value_get_boolean (value));
      break;
    case PROP_LOWER_STEPPER_SENSITIVITY:
      gtk_range_set_lower_stepper_sensitivity (range, g_value_get_enum (value));
      break;
    case PROP_UPPER_STEPPER_SENSITIVITY:
      gtk_range_set_upper_stepper_sensitivity (range, g_value_get_enum (value));
      break;
    case PROP_SHOW_FILL_LEVEL:
      gtk_range_set_show_fill_level (range, g_value_get_boolean (value));
      break;
    case PROP_RESTRICT_TO_FILL_LEVEL:
      gtk_range_set_restrict_to_fill_level (range, g_value_get_boolean (value));
      break;
    case PROP_FILL_LEVEL:
      gtk_range_set_fill_level (range, g_value_get_double (value));
      break;
    case PROP_ROUND_DIGITS:
      gtk_range_set_round_digits (range, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_range_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkRange *range = GTK_RANGE (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, range->orientation);
      break;
    case PROP_UPDATE_POLICY:
      g_value_set_enum (value, range->update_policy);
      break;
    case PROP_ADJUSTMENT:
      g_value_set_object (value, range->adjustment);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, range->inverted);
      break;
    case PROP_LOWER_STEPPER_SENSITIVITY:
      g_value_set_enum (value, gtk_range_get_lower_stepper_sensitivity (range));
      break;
    case PROP_UPPER_STEPPER_SENSITIVITY:
      g_value_set_enum (value, gtk_range_get_upper_stepper_sensitivity (range));
      break;
    case PROP_SHOW_FILL_LEVEL:
      g_value_set_boolean (value, gtk_range_get_show_fill_level (range));
      break;
    case PROP_RESTRICT_TO_FILL_LEVEL:
      g_value_set_boolean (value, gtk_range_get_restrict_to_fill_level (range));
      break;
    case PROP_FILL_LEVEL:
      g_value_set_double (value, gtk_range_get_fill_level (range));
      break;
    case PROP_ROUND_DIGITS:
      g_value_set_int (value, gtk_range_get_round_digits (range));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_range_init (GtkRange *range)
{
  gtk_widget_set_has_window (GTK_WIDGET (range), FALSE);

  range->orientation = GTK_ORIENTATION_HORIZONTAL;
  range->adjustment = NULL;
  range->update_policy = GTK_UPDATE_CONTINUOUS;
  range->inverted = FALSE;
  range->flippable = FALSE;
  range->min_slider_size = 1;
  range->has_stepper_a = FALSE;
  range->has_stepper_b = FALSE;
  range->has_stepper_c = FALSE;
  range->has_stepper_d = FALSE;
  range->need_recalc = TRUE;
  range->round_digits = -1;
  range->layout = GTK_RANGE_GET_PRIVATE (range);
  range->layout->mouse_location = MOUSE_OUTSIDE;
  range->layout->mouse_x = -1;
  range->layout->mouse_y = -1;
  range->layout->grab_location = MOUSE_OUTSIDE;
  range->layout->grab_button = 0;
  range->layout->lower_sensitivity = GTK_SENSITIVITY_AUTO;
  range->layout->upper_sensitivity = GTK_SENSITIVITY_AUTO;
  range->layout->lower_sensitive = TRUE;
  range->layout->upper_sensitive = TRUE;
  range->layout->show_fill_level = FALSE;
  range->layout->restrict_to_fill_level = TRUE;
  range->layout->fill_level = G_MAXDOUBLE;
  range->timer = NULL;  
}

/**
 * gtk_range_get_adjustment:
 * @range: a #GtkRange
 * 
 * Get the #GtkAdjustment which is the "model" object for #GtkRange.
 * See gtk_range_set_adjustment() for details.
 * The return value does not have a reference added, so should not
 * be unreferenced.
 * 
 * Return value: (transfer none): a #GtkAdjustment
 **/
GtkAdjustment*
gtk_range_get_adjustment (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), NULL);

  if (!range->adjustment)
    gtk_range_set_adjustment (range, NULL);

  return range->adjustment;
}

/**
 * gtk_range_set_update_policy:
 * @range: a #GtkRange
 * @policy: update policy
 *
 * Sets the update policy for the range. #GTK_UPDATE_CONTINUOUS means that
 * anytime the range slider is moved, the range value will change and the
 * value_changed signal will be emitted. #GTK_UPDATE_DELAYED means that
 * the value will be updated after a brief timeout where no slider motion
 * occurs, so updates are spaced by a short time rather than
 * continuous. #GTK_UPDATE_DISCONTINUOUS means that the value will only
 * be updated when the user releases the button and ends the slider
 * drag operation.
 *
 * Deprecated: 2.24: There is no replacement. If you require delayed
 *   updates, you need to code it yourself.
 **/
void
gtk_range_set_update_policy (GtkRange      *range,
			     GtkUpdateType  policy)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->update_policy != policy)
    {
      range->update_policy = policy;
      g_object_notify (G_OBJECT (range), "update-policy");
    }
}

/**
 * gtk_range_get_update_policy:
 * @range: a #GtkRange
 *
 * Gets the update policy of @range. See gtk_range_set_update_policy().
 *
 * Return value: the current update policy
 *
 * Deprecated: 2.24: There is no replacement. If you require delayed
 *   updates, you need to code it yourself.
 **/
GtkUpdateType
gtk_range_get_update_policy (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_UPDATE_CONTINUOUS);

  return range->update_policy;
}

/**
 * gtk_range_set_adjustment:
 * @range: a #GtkRange
 * @adjustment: a #GtkAdjustment
 *
 * Sets the adjustment to be used as the "model" object for this range
 * widget. The adjustment indicates the current range value, the
 * minimum and maximum range values, the step/page increments used
 * for keybindings and scrolling, and the page size. The page size
 * is normally 0 for #GtkScale and nonzero for #GtkScrollbar, and
 * indicates the size of the visible area of the widget being scrolled.
 * The page size affects the size of the scrollbar slider.
 **/
void
gtk_range_set_adjustment (GtkRange      *range,
			  GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  
  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (range->adjustment != adjustment)
    {
      if (range->adjustment)
	{
	  g_signal_handlers_disconnect_by_func (range->adjustment,
						gtk_range_adjustment_changed,
						range);
	  g_signal_handlers_disconnect_by_func (range->adjustment,
						gtk_range_adjustment_value_changed,
						range);
	  g_object_unref (range->adjustment);
	}

      range->adjustment = adjustment;
      g_object_ref_sink (adjustment);
      
      g_signal_connect (adjustment, "changed",
			G_CALLBACK (gtk_range_adjustment_changed),
			range);
      g_signal_connect (adjustment, "value-changed",
			G_CALLBACK (gtk_range_adjustment_value_changed),
			range);
      
      gtk_range_adjustment_changed (adjustment, range);
      g_object_notify (G_OBJECT (range), "adjustment");
    }
}

/**
 * gtk_range_set_inverted:
 * @range: a #GtkRange
 * @setting: %TRUE to invert the range
 *
 * Ranges normally move from lower to higher values as the
 * slider moves from top to bottom or left to right. Inverted
 * ranges have higher values at the top or on the right rather than
 * on the bottom or left.
 **/
void
gtk_range_set_inverted (GtkRange *range,
                        gboolean  setting)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  
  setting = setting != FALSE;

  if (setting != range->inverted)
    {
      range->inverted = setting;
      g_object_notify (G_OBJECT (range), "inverted");
      gtk_widget_queue_resize (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_inverted:
 * @range: a #GtkRange
 * 
 * Gets the value set by gtk_range_set_inverted().
 * 
 * Return value: %TRUE if the range is inverted
 **/
gboolean
gtk_range_get_inverted (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->inverted;
}

/**
 * gtk_range_set_flippable:
 * @range: a #GtkRange
 * @flippable: %TRUE to make the range flippable
 *
 * If a range is flippable, it will switch its direction if it is
 * horizontal and its direction is %GTK_TEXT_DIR_RTL.
 *
 * See gtk_widget_get_direction().
 *
 * Since: 2.18
 **/
void
gtk_range_set_flippable (GtkRange *range,
                         gboolean  flippable)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  flippable = flippable ? TRUE : FALSE;

  if (flippable != range->flippable)
    {
      range->flippable = flippable;

      gtk_widget_queue_draw (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_flippable:
 * @range: a #GtkRange
 *
 * Gets the value set by gtk_range_set_flippable().
 *
 * Return value: %TRUE if the range is flippable
 *
 * Since: 2.18
 **/
gboolean
gtk_range_get_flippable (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->flippable;
}

/**
 * gtk_range_set_slider_size_fixed:
 * @range: a #GtkRange
 * @size_fixed: %TRUE to make the slider size constant
 *
 * Sets whether the range's slider has a fixed size, or a size that
 * depends on it's adjustment's page size.
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * Since: 2.20
 **/
void
gtk_range_set_slider_size_fixed (GtkRange *range,
                                 gboolean  size_fixed)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (size_fixed != range->slider_size_fixed)
    {
      range->slider_size_fixed = size_fixed ? TRUE : FALSE;

      range->need_recalc = TRUE;
      gtk_range_calc_layout (range, range->adjustment->value);
      gtk_widget_queue_draw (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_slider_size_fixed:
 * @range: a #GtkRange
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * See gtk_range_set_slider_size_fixed().
 *
 * Return value: whether the range's slider has a fixed size.
 *
 * Since: 2.20
 **/
gboolean
gtk_range_get_slider_size_fixed (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->slider_size_fixed;
}

/**
 * gtk_range_set_min_slider_size:
 * @range: a #GtkRange
 * @min_size: The slider's minimum size
 *
 * Sets the minimum size of the range's slider.
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * Since: 2.20
 **/
void
gtk_range_set_min_slider_size (GtkRange *range,
                               gboolean  min_size)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (min_size > 0);

  if (min_size != range->min_slider_size)
    {
      range->min_slider_size = min_size;

      range->need_recalc = TRUE;
      gtk_range_calc_layout (range, range->adjustment->value);
      gtk_widget_queue_draw (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_min_slider_size:
 * @range: a #GtkRange
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * See gtk_range_set_min_slider_size().
 *
 * Return value: The minimum size of the range's slider.
 *
 * Since: 2.20
 **/
gint
gtk_range_get_min_slider_size (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->min_slider_size;
}

/**
 * gtk_range_get_range_rect:
 * @range: a #GtkRange
 * @range_rect: (out): return location for the range rectangle
 *
 * This function returns the area that contains the range's trough
 * and its steppers, in widget->window coordinates.
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * Since: 2.20
 **/
void
gtk_range_get_range_rect (GtkRange     *range,
                          GdkRectangle *range_rect)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (range_rect != NULL);

  gtk_range_calc_layout (range, range->adjustment->value);

  *range_rect = range->range_rect;
}

/**
 * gtk_range_get_slider_range:
 * @range: a #GtkRange
 * @slider_start: (out) (allow-none): return location for the slider's
 *     start, or %NULL
 * @slider_end: (out) (allow-none): return location for the slider's
 *     end, or %NULL
 *
 * This function returns sliders range along the long dimension,
 * in widget->window coordinates.
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * Since: 2.20
 **/
void
gtk_range_get_slider_range (GtkRange *range,
                            gint     *slider_start,
                            gint     *slider_end)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  gtk_range_calc_layout (range, range->adjustment->value);

  if (slider_start)
    *slider_start = range->slider_start;

  if (slider_end)
    *slider_end = range->slider_end;
}

/**
 * gtk_range_set_lower_stepper_sensitivity:
 * @range:       a #GtkRange
 * @sensitivity: the lower stepper's sensitivity policy.
 *
 * Sets the sensitivity policy for the stepper that points to the
 * 'lower' end of the GtkRange's adjustment.
 *
 * Since: 2.10
 **/
void
gtk_range_set_lower_stepper_sensitivity (GtkRange           *range,
                                         GtkSensitivityType  sensitivity)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->layout->lower_sensitivity != sensitivity)
    {
      range->layout->lower_sensitivity = sensitivity;

      range->need_recalc = TRUE;
      gtk_range_calc_layout (range, range->adjustment->value);
      gtk_widget_queue_draw (GTK_WIDGET (range));

      g_object_notify (G_OBJECT (range), "lower-stepper-sensitivity");
    }
}

/**
 * gtk_range_get_lower_stepper_sensitivity:
 * @range: a #GtkRange
 *
 * Gets the sensitivity policy for the stepper that points to the
 * 'lower' end of the GtkRange's adjustment.
 *
 * Return value: The lower stepper's sensitivity policy.
 *
 * Since: 2.10
 **/
GtkSensitivityType
gtk_range_get_lower_stepper_sensitivity (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_SENSITIVITY_AUTO);

  return range->layout->lower_sensitivity;
}

/**
 * gtk_range_set_upper_stepper_sensitivity:
 * @range:       a #GtkRange
 * @sensitivity: the upper stepper's sensitivity policy.
 *
 * Sets the sensitivity policy for the stepper that points to the
 * 'upper' end of the GtkRange's adjustment.
 *
 * Since: 2.10
 **/
void
gtk_range_set_upper_stepper_sensitivity (GtkRange           *range,
                                         GtkSensitivityType  sensitivity)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->layout->upper_sensitivity != sensitivity)
    {
      range->layout->upper_sensitivity = sensitivity;

      range->need_recalc = TRUE;
      gtk_range_calc_layout (range, range->adjustment->value);
      gtk_widget_queue_draw (GTK_WIDGET (range));

      g_object_notify (G_OBJECT (range), "upper-stepper-sensitivity");
    }
}

/**
 * gtk_range_get_upper_stepper_sensitivity:
 * @range: a #GtkRange
 *
 * Gets the sensitivity policy for the stepper that points to the
 * 'upper' end of the GtkRange's adjustment.
 *
 * Return value: The upper stepper's sensitivity policy.
 *
 * Since: 2.10
 **/
GtkSensitivityType
gtk_range_get_upper_stepper_sensitivity (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_SENSITIVITY_AUTO);

  return range->layout->upper_sensitivity;
}

/**
 * gtk_range_set_increments:
 * @range: a #GtkRange
 * @step: step size
 * @page: page size
 *
 * Sets the step and page sizes for the range.
 * The step size is used when the user clicks the #GtkScrollbar
 * arrows or moves #GtkScale via arrow keys. The page size
 * is used for example when moving via Page Up or Page Down keys.
 **/
void
gtk_range_set_increments (GtkRange *range,
                          gdouble   step,
                          gdouble   page)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  range->adjustment->step_increment = step;
  range->adjustment->page_increment = page;

  gtk_adjustment_changed (range->adjustment);
}

/**
 * gtk_range_set_range:
 * @range: a #GtkRange
 * @min: minimum range value
 * @max: maximum range value
 * 
 * Sets the allowable values in the #GtkRange, and clamps the range
 * value to be between @min and @max. (If the range has a non-zero
 * page size, it is clamped between @min and @max - page-size.)
 **/
void
gtk_range_set_range (GtkRange *range,
                     gdouble   min,
                     gdouble   max)
{
  gdouble value;
  
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (min < max);
  
  range->adjustment->lower = min;
  range->adjustment->upper = max;

  value = range->adjustment->value;

  if (range->layout->restrict_to_fill_level)
    value = MIN (value, MAX (range->adjustment->lower,
                             range->layout->fill_level));

  value = CLAMP (value, range->adjustment->lower,
                 (range->adjustment->upper - range->adjustment->page_size));

  gtk_adjustment_set_value (range->adjustment, value);
  gtk_adjustment_changed (range->adjustment);
}

/**
 * gtk_range_set_value:
 * @range: a #GtkRange
 * @value: new value of the range
 *
 * Sets the current value of the range; if the value is outside the
 * minimum or maximum range values, it will be clamped to fit inside
 * them. The range emits the #GtkRange::value-changed signal if the 
 * value changes.
 **/
void
gtk_range_set_value (GtkRange *range,
                     gdouble   value)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->layout->restrict_to_fill_level)
    value = MIN (value, MAX (range->adjustment->lower,
                             range->layout->fill_level));

  value = CLAMP (value, range->adjustment->lower,
                 (range->adjustment->upper - range->adjustment->page_size));

  gtk_adjustment_set_value (range->adjustment, value);
}

/**
 * gtk_range_get_value:
 * @range: a #GtkRange
 * 
 * Gets the current value of the range.
 * 
 * Return value: current value of the range.
 **/
gdouble
gtk_range_get_value (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return range->adjustment->value;
}

/**
 * gtk_range_set_show_fill_level:
 * @range:           A #GtkRange
 * @show_fill_level: Whether a fill level indicator graphics is shown.
 *
 * Sets whether a graphical fill level is show on the trough. See
 * gtk_range_set_fill_level() for a general description of the fill
 * level concept.
 *
 * Since: 2.12
 **/
void
gtk_range_set_show_fill_level (GtkRange *range,
                               gboolean  show_fill_level)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  show_fill_level = show_fill_level ? TRUE : FALSE;

  if (show_fill_level != range->layout->show_fill_level)
    {
      range->layout->show_fill_level = show_fill_level;
      g_object_notify (G_OBJECT (range), "show-fill-level");
      gtk_widget_queue_draw (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_show_fill_level:
 * @range: A #GtkRange
 *
 * Gets whether the range displays the fill level graphically.
 *
 * Return value: %TRUE if @range shows the fill level.
 *
 * Since: 2.12
 **/
gboolean
gtk_range_get_show_fill_level (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->layout->show_fill_level;
}

/**
 * gtk_range_set_restrict_to_fill_level:
 * @range:                  A #GtkRange
 * @restrict_to_fill_level: Whether the fill level restricts slider movement.
 *
 * Sets whether the slider is restricted to the fill level. See
 * gtk_range_set_fill_level() for a general description of the fill
 * level concept.
 *
 * Since: 2.12
 **/
void
gtk_range_set_restrict_to_fill_level (GtkRange *range,
                                      gboolean  restrict_to_fill_level)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  restrict_to_fill_level = restrict_to_fill_level ? TRUE : FALSE;

  if (restrict_to_fill_level != range->layout->restrict_to_fill_level)
    {
      range->layout->restrict_to_fill_level = restrict_to_fill_level;
      g_object_notify (G_OBJECT (range), "restrict-to-fill-level");

      gtk_range_set_value (range, gtk_range_get_value (range));
    }
}

/**
 * gtk_range_get_restrict_to_fill_level:
 * @range: A #GtkRange
 *
 * Gets whether the range is restricted to the fill level.
 *
 * Return value: %TRUE if @range is restricted to the fill level.
 *
 * Since: 2.12
 **/
gboolean
gtk_range_get_restrict_to_fill_level (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->layout->restrict_to_fill_level;
}

/**
 * gtk_range_set_fill_level:
 * @range: a #GtkRange
 * @fill_level: the new position of the fill level indicator
 *
 * Set the new position of the fill level indicator.
 *
 * The "fill level" is probably best described by its most prominent
 * use case, which is an indicator for the amount of pre-buffering in
 * a streaming media player. In that use case, the value of the range
 * would indicate the current play position, and the fill level would
 * be the position up to which the file/stream has been downloaded.
 *
 * This amount of prebuffering can be displayed on the range's trough
 * and is themeable separately from the trough. To enable fill level
 * display, use gtk_range_set_show_fill_level(). The range defaults
 * to not showing the fill level.
 *
 * Additionally, it's possible to restrict the range's slider position
 * to values which are smaller than the fill level. This is controller
 * by gtk_range_set_restrict_to_fill_level() and is by default
 * enabled.
 *
 * Since: 2.12
 **/
void
gtk_range_set_fill_level (GtkRange *range,
                          gdouble   fill_level)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (fill_level != range->layout->fill_level)
    {
      range->layout->fill_level = fill_level;
      g_object_notify (G_OBJECT (range), "fill-level");

      if (range->layout->show_fill_level)
        gtk_widget_queue_draw (GTK_WIDGET (range));

      if (range->layout->restrict_to_fill_level)
        gtk_range_set_value (range, gtk_range_get_value (range));
    }
}

/**
 * gtk_range_get_fill_level:
 * @range: A #GtkRange
 *
 * Gets the current position of the fill level indicator.
 *
 * Return value: The current fill level
 *
 * Since: 2.12
 **/
gdouble
gtk_range_get_fill_level (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return range->layout->fill_level;
}

static gboolean
should_invert (GtkRange *range)
{  
  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
    return
      (range->inverted && !range->flippable) ||
      (range->inverted && range->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_LTR) ||
      (!range->inverted && range->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_RTL);
  else
    return range->inverted;
}

static void
gtk_range_destroy (GtkObject *object)
{
  GtkRange *range = GTK_RANGE (object);

  gtk_range_remove_step_timer (range);
  gtk_range_remove_update_timer (range);

  if (range->layout->repaint_id)
    g_source_remove (range->layout->repaint_id);
  range->layout->repaint_id = 0;

  if (range->adjustment)
    {
      g_signal_handlers_disconnect_by_func (range->adjustment,
					    gtk_range_adjustment_changed,
					    range);
      g_signal_handlers_disconnect_by_func (range->adjustment,
					    gtk_range_adjustment_value_changed,
					    range);
      g_object_unref (range->adjustment);
      range->adjustment = NULL;
    }

  if (range->layout->n_marks)
    {
      g_free (range->layout->marks);
      range->layout->marks = NULL;
      g_free (range->layout->mark_pos);
      range->layout->mark_pos = NULL;
      range->layout->n_marks = 0;
    }

  GTK_OBJECT_CLASS (gtk_range_parent_class)->destroy (object);
}

static void
gtk_range_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
  GtkRange *range;
  gint slider_width, stepper_size, focus_width, trough_border, stepper_spacing;
  GdkRectangle range_rect;
  GtkBorder border;
  
  range = GTK_RANGE (widget);
  
  gtk_range_get_props (range,
                       &slider_width, &stepper_size,
                       &focus_width, &trough_border,
                       &stepper_spacing, NULL,
                       NULL, NULL);

  gtk_range_calc_request (range, 
                          slider_width, stepper_size,
                          focus_width, trough_border, stepper_spacing,
                          &range_rect, &border, NULL, NULL, NULL, NULL);

  requisition->width = range_rect.width + border.left + border.right;
  requisition->height = range_rect.height + border.top + border.bottom;
}

static void
gtk_range_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkRange *range;

  range = GTK_RANGE (widget);

  widget->allocation = *allocation;
  
  range->layout->recalc_marks = TRUE;

  range->need_recalc = TRUE;
  gtk_range_calc_layout (range, range->adjustment->value);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (range->event_window,
			    widget->allocation.x,
			    widget->allocation.y,
			    widget->allocation.width,
			    widget->allocation.height);
}

static void
gtk_range_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;  

  range = GTK_RANGE (widget);

  gtk_range_calc_layout (range, range->adjustment->value);
  
  gtk_widget_set_realized (widget, TRUE);

  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  range->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					&attributes, attributes_mask);
  gdk_window_set_user_data (range->event_window, range);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_range_unrealize (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);

  gtk_range_remove_step_timer (range);
  gtk_range_remove_update_timer (range);
  
  gdk_window_set_user_data (range->event_window, NULL);
  gdk_window_destroy (range->event_window);
  range->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_range_parent_class)->unrealize (widget);
}

static void
gtk_range_map (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  
  gdk_window_show (range->event_window);

  GTK_WIDGET_CLASS (gtk_range_parent_class)->map (widget);
}

static void
gtk_range_unmap (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
    
  stop_scrolling (range);

  gdk_window_hide (range->event_window);

  GTK_WIDGET_CLASS (gtk_range_parent_class)->unmap (widget);
}

static const gchar *
gtk_range_get_slider_detail (GtkRange *range)
{
  const gchar *slider_detail;

  if (range->layout->slider_detail_quark)
    return g_quark_to_string (range->layout->slider_detail_quark);

  slider_detail = GTK_RANGE_GET_CLASS (range)->slider_detail;

  if (slider_detail && slider_detail[0] == 'X')
    {
      gchar *detail = g_strdup (slider_detail);

      detail[0] = range->orientation == GTK_ORIENTATION_HORIZONTAL ? 'h' : 'v';

      range->layout->slider_detail_quark = g_quark_from_string (detail);

      g_free (detail);

      return g_quark_to_string (range->layout->slider_detail_quark);
    }

  return slider_detail;
}

static const gchar *
gtk_range_get_stepper_detail (GtkRange *range,
                              Stepper   stepper)
{
  const gchar *stepper_detail;
  gboolean need_orientation;
  gboolean need_position;

  if (range->layout->stepper_detail_quark[stepper])
    return g_quark_to_string (range->layout->stepper_detail_quark[stepper]);

  stepper_detail = GTK_RANGE_GET_CLASS (range)->stepper_detail;

  need_orientation = stepper_detail && stepper_detail[0] == 'X';

  gtk_widget_style_get (GTK_WIDGET (range),
                        "stepper-position-details", &need_position,
                        NULL);

  if (need_orientation || need_position)
    {
      gchar *detail;
      const gchar *position = NULL;

      if (need_position)
        {
          switch (stepper)
            {
            case STEPPER_A:
              position = "_start";
              break;
            case STEPPER_B:
              if (range->has_stepper_a)
                position = "_middle";
              else
                position = "_start";
              break;
            case STEPPER_C:
              if (range->has_stepper_d)
                position = "_middle";
              else
                position = "_end";
              break;
            case STEPPER_D:
              position = "_end";
              break;
            default:
              g_assert_not_reached ();
            }
        }

      detail = g_strconcat (stepper_detail, position, NULL);

      if (need_orientation)
        detail[0] = range->orientation == GTK_ORIENTATION_HORIZONTAL ? 'h' : 'v';

      range->layout->stepper_detail_quark[stepper] = g_quark_from_string (detail);

      g_free (detail);

      return g_quark_to_string (range->layout->stepper_detail_quark[stepper]);
    }

  return stepper_detail;
}

static void
draw_stepper (GtkRange     *range,
              Stepper       stepper,
              GtkArrowType  arrow_type,
              gboolean      clicked,
              gboolean      prelighted,
              GdkRectangle *area)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GdkRectangle intersection;
  GtkWidget *widget = GTK_WIDGET (range);
  gfloat arrow_scaling;
  GdkRectangle *rect;
  gint arrow_x;
  gint arrow_y;
  gint arrow_width;
  gint arrow_height;
  gboolean arrow_sensitive = TRUE;

  switch (stepper)
    {
    case STEPPER_A:
      rect = &range->layout->stepper_a;
      break;
    case STEPPER_B:
      rect = &range->layout->stepper_b;
      break;
    case STEPPER_C:
      rect = &range->layout->stepper_c;
      break;
    case STEPPER_D:
      rect = &range->layout->stepper_d;
      break;
    default:
      g_assert_not_reached ();
    };

  /* More to get the right clip region than for efficiency */
  if (!gdk_rectangle_intersect (area, rect, &intersection))
    return;

  intersection.x += widget->allocation.x;
  intersection.y += widget->allocation.y;

  if ((!range->inverted && (arrow_type == GTK_ARROW_DOWN ||
                            arrow_type == GTK_ARROW_RIGHT)) ||
      (range->inverted  && (arrow_type == GTK_ARROW_UP ||
                            arrow_type == GTK_ARROW_LEFT)))
    {
      arrow_sensitive = range->layout->upper_sensitive;
    }
  else
    {
      arrow_sensitive = range->layout->lower_sensitive;
    }

  if (!gtk_widget_is_sensitive (GTK_WIDGET (range)) || !arrow_sensitive)
    state_type = GTK_STATE_INSENSITIVE;
  else if (clicked)
    state_type = GTK_STATE_ACTIVE;
  else if (prelighted)
    state_type = GTK_STATE_PRELIGHT;
  else 
    state_type = GTK_STATE_NORMAL;

  if (clicked && arrow_sensitive)
    shadow_type = GTK_SHADOW_IN;
  else
    shadow_type = GTK_SHADOW_OUT;

  gtk_paint_box (widget->style,
		 widget->window,
		 state_type, shadow_type,
		 &intersection, widget,
		 gtk_range_get_stepper_detail (range, stepper),
		 widget->allocation.x + rect->x,
		 widget->allocation.y + rect->y,
		 rect->width,
		 rect->height);

  gtk_widget_style_get (widget, "arrow-scaling", &arrow_scaling, NULL);

  arrow_width = rect->width * arrow_scaling;
  arrow_height = rect->height * arrow_scaling;
  arrow_x = widget->allocation.x + rect->x + (rect->width - arrow_width) / 2;
  arrow_y = widget->allocation.y + rect->y + (rect->height - arrow_height) / 2;

  if (clicked && arrow_sensitive)
    {
      gint arrow_displacement_x;
      gint arrow_displacement_y;

      gtk_range_get_props (GTK_RANGE (widget),
                           NULL, NULL, NULL, NULL, NULL, NULL,
			   &arrow_displacement_x, &arrow_displacement_y);
      
      arrow_x += arrow_displacement_x;
      arrow_y += arrow_displacement_y;
    }
  
  gtk_paint_arrow (widget->style,
                   widget->window,
                   state_type, shadow_type,
                   &intersection, widget,
                   gtk_range_get_stepper_detail (range, stepper),
                   arrow_type,
                   TRUE,
		   arrow_x, arrow_y, arrow_width, arrow_height);
}

static gboolean
gtk_range_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkRange *range = GTK_RANGE (widget);
  gboolean sensitive;
  GtkStateType state;
  GtkShadowType shadow_type;
  GdkRectangle expose_area;	/* Relative to widget->allocation */
  GdkRectangle area;
  gint focus_line_width = 0;
  gint focus_padding = 0;
  gboolean touchscreen;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-touchscreen-mode", &touchscreen,
                NULL);
  if (gtk_widget_get_can_focus (GTK_WIDGET (range)))
    gtk_widget_style_get (GTK_WIDGET (range),
                          "focus-line-width", &focus_line_width,
                          "focus-padding", &focus_padding,
                          NULL);

  /* we're now exposing, so there's no need to force early repaints */
  if (range->layout->repaint_id)
    g_source_remove (range->layout->repaint_id);
  range->layout->repaint_id = 0;

  expose_area = event->area;
  expose_area.x -= widget->allocation.x;
  expose_area.y -= widget->allocation.y;
  
  gtk_range_calc_marks (range);
  gtk_range_calc_layout (range, range->adjustment->value);

  sensitive = gtk_widget_is_sensitive (widget);

  /* Just to be confusing, we draw the trough for the whole
   * range rectangle, not the trough rectangle (the trough
   * rectangle is just for hit detection)
   */
  /* The gdk_rectangle_intersect is more to get the right
   * clip region (limited to range_rect) than for efficiency
   */
  if (gdk_rectangle_intersect (&expose_area, &range->range_rect,
                               &area))
    {
      gint     x      = (widget->allocation.x + range->range_rect.x +
                         focus_line_width + focus_padding);
      gint     y      = (widget->allocation.y + range->range_rect.y +
                         focus_line_width + focus_padding);
      gint     width  = (range->range_rect.width -
                         2 * (focus_line_width + focus_padding));
      gint     height = (range->range_rect.height -
                         2 * (focus_line_width + focus_padding));
      gboolean trough_side_details;
      gboolean trough_under_steppers;
      gint     stepper_size;
      gint     stepper_spacing;

      area.x += widget->allocation.x;
      area.y += widget->allocation.y;

      gtk_widget_style_get (GTK_WIDGET (range),
                            "trough-side-details",   &trough_side_details,
                            "trough-under-steppers", &trough_under_steppers,
                            "stepper-size",          &stepper_size,
                            "stepper-spacing",       &stepper_spacing,
                            NULL);

      if (stepper_spacing > 0)
        trough_under_steppers = FALSE;

      if (! trough_under_steppers)
        {
          gint offset  = 0;
          gint shorter = 0;

          if (range->has_stepper_a)
            offset += stepper_size;

          if (range->has_stepper_b)
            offset += stepper_size;

          shorter += offset;

          if (range->has_stepper_c)
            shorter += stepper_size;

          if (range->has_stepper_d)
            shorter += stepper_size;

          if (range->has_stepper_a || range->has_stepper_b)
            {
              offset  += stepper_spacing;
              shorter += stepper_spacing;
            }

          if (range->has_stepper_c || range->has_stepper_d)
            {
              shorter += stepper_spacing;
            }

          if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              x     += offset;
              width -= shorter;
            }
          else
            {
              y      += offset;
              height -= shorter;
            }
	}

      if (! trough_side_details)
        {
          gtk_paint_box (widget->style,
                         widget->window,
                         sensitive ? GTK_STATE_ACTIVE : GTK_STATE_INSENSITIVE,
                         GTK_SHADOW_IN,
                         &area, GTK_WIDGET(range), "trough",
                         x, y,
                         width, height);
        }
      else
        {
	  gint trough_change_pos_x = width;
	  gint trough_change_pos_y = height;

	  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
	    trough_change_pos_x = (range->layout->slider.x +
                                   range->layout->slider.width / 2 -
                                   (x - widget->allocation.x));
	  else
	    trough_change_pos_y = (range->layout->slider.y +
                                   range->layout->slider.height / 2 -
                                   (y - widget->allocation.y));

          gtk_paint_box (widget->style,
                         widget->window,
                         sensitive ? GTK_STATE_ACTIVE : GTK_STATE_INSENSITIVE,
                         GTK_SHADOW_IN,
                         &area, GTK_WIDGET (range),
                         should_invert (range) ? "trough-upper" : "trough-lower",
                         x, y,
                         trough_change_pos_x, trough_change_pos_y);

	  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
	    trough_change_pos_y = 0;
	  else
	    trough_change_pos_x = 0;

          gtk_paint_box (widget->style,
                         widget->window,
                         sensitive ? GTK_STATE_ACTIVE : GTK_STATE_INSENSITIVE,
                         GTK_SHADOW_IN,
                         &area, GTK_WIDGET (range),
                         should_invert (range) ? "trough-lower" : "trough-upper",
                         x + trough_change_pos_x, y + trough_change_pos_y,
                         width - trough_change_pos_x,
                         height - trough_change_pos_y);
        }

      if (range->layout->show_fill_level &&
          range->adjustment->upper - range->adjustment->page_size -
          range->adjustment->lower != 0)
	{
          gdouble  fill_level  = range->layout->fill_level;
	  gint     fill_x      = x;
	  gint     fill_y      = y;
	  gint     fill_width  = width;
	  gint     fill_height = height;
	  gchar   *fill_detail;

          fill_level = CLAMP (fill_level, range->adjustment->lower,
                              range->adjustment->upper -
                              range->adjustment->page_size);

	  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      fill_x     = widget->allocation.x + range->layout->trough.x;
	      fill_width = (range->layout->slider.width +
                            (fill_level - range->adjustment->lower) /
                            (range->adjustment->upper -
                             range->adjustment->lower -
                             range->adjustment->page_size) *
                            (range->layout->trough.width -
                             range->layout->slider.width));

              if (should_invert (range))
                fill_x += range->layout->trough.width - fill_width;
	    }
	  else
	    {
	      fill_y      = widget->allocation.y + range->layout->trough.y;
	      fill_height = (range->layout->slider.height +
                             (fill_level - range->adjustment->lower) /
                             (range->adjustment->upper -
                              range->adjustment->lower -
                              range->adjustment->page_size) *
                             (range->layout->trough.height -
                              range->layout->slider.height));

              if (should_invert (range))
                fill_y += range->layout->trough.height - fill_height;
	    }

	  if (fill_level < range->adjustment->upper - range->adjustment->page_size)
	    fill_detail = "trough-fill-level-full";
	  else
	    fill_detail = "trough-fill-level";

          gtk_paint_box (widget->style,
                         widget->window,
                         sensitive ? GTK_STATE_ACTIVE : GTK_STATE_INSENSITIVE,
                         GTK_SHADOW_OUT,
                         &area, GTK_WIDGET (range), fill_detail,
                         fill_x, fill_y,
                         fill_width, fill_height);
	}

      if (sensitive && gtk_widget_has_focus (widget))
        gtk_paint_focus (widget->style, widget->window, gtk_widget_get_state (widget),
                         &area, widget, "trough",
                         widget->allocation.x + range->range_rect.x,
                         widget->allocation.y + range->range_rect.y,
                         range->range_rect.width,
                         range->range_rect.height);
    }

  shadow_type = GTK_SHADOW_OUT;

  if (!sensitive)
    state = GTK_STATE_INSENSITIVE;
  else if (!touchscreen && range->layout->mouse_location == MOUSE_SLIDER)
    state = GTK_STATE_PRELIGHT;
  else
    state = GTK_STATE_NORMAL;

  if (range->layout->grab_location == MOUSE_SLIDER)
    {
      gboolean activate_slider;

      gtk_widget_style_get (widget, "activate-slider", &activate_slider, NULL);

      if (activate_slider)
        {
          state = GTK_STATE_ACTIVE;
          shadow_type = GTK_SHADOW_IN;
        }
    }

  if (gdk_rectangle_intersect (&expose_area,
                               &range->layout->slider,
                               &area))
    {
      area.x += widget->allocation.x;
      area.y += widget->allocation.y;
      
      gtk_paint_slider (widget->style,
                        widget->window,
                        state,
                        shadow_type,
                        &area,
                        widget,
                        gtk_range_get_slider_detail (range),
                        widget->allocation.x + range->layout->slider.x,
                        widget->allocation.y + range->layout->slider.y,
                        range->layout->slider.width,
                        range->layout->slider.height,
                        range->orientation);
    }
  
  if (range->has_stepper_a)
    draw_stepper (range, STEPPER_A,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_UP : GTK_ARROW_LEFT,
                  range->layout->grab_location == MOUSE_STEPPER_A,
                  !touchscreen && range->layout->mouse_location == MOUSE_STEPPER_A,
                  &expose_area);

  if (range->has_stepper_b)
    draw_stepper (range, STEPPER_B,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  range->layout->grab_location == MOUSE_STEPPER_B,
                  !touchscreen && range->layout->mouse_location == MOUSE_STEPPER_B,
                  &expose_area);

  if (range->has_stepper_c)
    draw_stepper (range, STEPPER_C,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_UP : GTK_ARROW_LEFT,
                  range->layout->grab_location == MOUSE_STEPPER_C,
                  !touchscreen && range->layout->mouse_location == MOUSE_STEPPER_C,
                  &expose_area);

  if (range->has_stepper_d)
    draw_stepper (range, STEPPER_D,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  range->layout->grab_location == MOUSE_STEPPER_D,
                  !touchscreen && range->layout->mouse_location == MOUSE_STEPPER_D,
                  &expose_area);
  
  return FALSE;
}

static void
range_grab_add (GtkRange      *range,
                MouseLocation  location,
                gint           button)
{
  /* we don't actually gtk_grab, since a button is down */

  gtk_grab_add (GTK_WIDGET (range));
  
  range->layout->grab_location = location;
  range->layout->grab_button = button;
  
  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (GTK_WIDGET (range));
}

static void
range_grab_remove (GtkRange *range)
{
  MouseLocation location;

  gtk_grab_remove (GTK_WIDGET (range));
 
  location = range->layout->grab_location; 
  range->layout->grab_location = MOUSE_OUTSIDE;
  range->layout->grab_button = 0;

  if (gtk_range_update_mouse_location (range) ||
      location != MOUSE_OUTSIDE)
    gtk_widget_queue_draw (GTK_WIDGET (range));
}

static GtkScrollType
range_get_scroll_for_grab (GtkRange      *range)
{ 
  gboolean invert;

  invert = should_invert (range);
  switch (range->layout->grab_location)
    {
      /* Backward stepper */
    case MOUSE_STEPPER_A:
    case MOUSE_STEPPER_C:
      switch (range->layout->grab_button)
        {
        case 1:
          return invert ? GTK_SCROLL_STEP_FORWARD : GTK_SCROLL_STEP_BACKWARD;
          break;
        case 2:
          return invert ? GTK_SCROLL_PAGE_FORWARD : GTK_SCROLL_PAGE_BACKWARD;
          break;
        case 3:
          return invert ? GTK_SCROLL_END : GTK_SCROLL_START;
          break;
        }
      break;

      /* Forward stepper */
    case MOUSE_STEPPER_B:
    case MOUSE_STEPPER_D:
      switch (range->layout->grab_button)
        {
        case 1:
          return invert ? GTK_SCROLL_STEP_BACKWARD : GTK_SCROLL_STEP_FORWARD;
          break;
        case 2:
          return invert ? GTK_SCROLL_PAGE_BACKWARD : GTK_SCROLL_PAGE_FORWARD;
          break;
        case 3:
          return invert ? GTK_SCROLL_START : GTK_SCROLL_END;
          break;
       }
      break;

      /* In the trough */
    case MOUSE_TROUGH:
      {
        if (range->trough_click_forward)
	  return GTK_SCROLL_PAGE_FORWARD;
        else
	  return GTK_SCROLL_PAGE_BACKWARD;
      }
      break;

    case MOUSE_OUTSIDE:
    case MOUSE_SLIDER:
    case MOUSE_WIDGET:
      break;
    }

  return GTK_SCROLL_NONE;
}

static gdouble
coord_to_value (GtkRange *range,
                gint      coord)
{
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    trough_start;
  gint    slider_length;
  gint    trough_border;
  gint    trough_under_steppers;

  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      trough_length = range->layout->trough.height;
      trough_start  = range->layout->trough.y;
      slider_length = range->layout->slider.height;
    }
  else
    {
      trough_length = range->layout->trough.width;
      trough_start  = range->layout->trough.x;
      slider_length = range->layout->slider.width;
    }

  gtk_range_get_props (range, NULL, NULL, NULL, &trough_border, NULL,
                       &trough_under_steppers, NULL, NULL);

  if (! trough_under_steppers)
    {
      trough_start += trough_border;
      trough_length -= 2 * trough_border;
    }

  if (trough_length == slider_length)
    frac = 1.0;
  else
    frac = (MAX (0, coord - trough_start) /
            (gdouble) (trough_length - slider_length));

  if (should_invert (range))
    frac = 1.0 - frac;

  value = range->adjustment->lower + frac * (range->adjustment->upper -
                                             range->adjustment->lower -
                                             range->adjustment->page_size);

  return value;
}

static gboolean
gtk_range_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkRange *range = GTK_RANGE (widget);

  if (event->keyval == GDK_Escape &&
      range->layout->grab_location != MOUSE_OUTSIDE)
    {
      stop_scrolling (range);

      update_slider_position (range,
			      range->slide_initial_coordinate,
			      range->slide_initial_coordinate);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_range_parent_class)->key_press_event (widget, event);
}

static gint
gtk_range_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkRange *range = GTK_RANGE (widget);
  gboolean primary_warps;
  gint page_increment_button, warp_button;
  
  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  /* ignore presses when we're already doing something else. */
  if (range->layout->grab_location != MOUSE_OUTSIDE)
    return FALSE;

  range->layout->mouse_x = event->x;
  range->layout->mouse_y = event->y;
  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-primary-button-warps-slider", &primary_warps,
                NULL);
  if (primary_warps)
    {
      warp_button = 1;
      page_increment_button = 3;
    }
  else
    {
      warp_button = 2;
      page_increment_button = 1;
    }

  if (range->layout->mouse_location == MOUSE_TROUGH  &&
      event->button == page_increment_button)
    {
      /* this button steps by page increment, as with button 2 on a stepper
       */
      GtkScrollType scroll;
      gdouble click_value;
      
      click_value = coord_to_value (range,
                                    range->orientation == GTK_ORIENTATION_VERTICAL ?
                                    event->y : event->x);
      
      range->trough_click_forward = click_value > range->adjustment->value;
      range_grab_add (range, MOUSE_TROUGH, event->button);
      
      scroll = range_get_scroll_for_grab (range);
      
      gtk_range_add_step_timer (range, scroll);

      return TRUE;
    }
  else if ((range->layout->mouse_location == MOUSE_STEPPER_A ||
            range->layout->mouse_location == MOUSE_STEPPER_B ||
            range->layout->mouse_location == MOUSE_STEPPER_C ||
            range->layout->mouse_location == MOUSE_STEPPER_D) &&
           (event->button == 1 || event->button == 2 || event->button == 3))
    {
      GdkRectangle *stepper_area;
      GtkScrollType scroll;
      
      range_grab_add (range, range->layout->mouse_location, event->button);

      stepper_area = get_area (range, range->layout->mouse_location);
      gtk_widget_queue_draw_area (widget,
                                  widget->allocation.x + stepper_area->x,
                                  widget->allocation.y + stepper_area->y,
                                  stepper_area->width,
                                  stepper_area->height);

      scroll = range_get_scroll_for_grab (range);
      if (scroll != GTK_SCROLL_NONE)
        gtk_range_add_step_timer (range, scroll);
      
      return TRUE;
    }
  else if ((range->layout->mouse_location == MOUSE_TROUGH &&
            event->button == warp_button) ||
           range->layout->mouse_location == MOUSE_SLIDER)
    {
      gboolean need_value_update = FALSE;
      gboolean activate_slider;

      /* Any button can be used to drag the slider, but you can start
       * dragging the slider with a trough click using the warp button;
       * we warp the slider to mouse position, then begin the slider drag.
       */
      if (range->layout->mouse_location != MOUSE_SLIDER)
        {
          gdouble slider_low_value, slider_high_value, new_value;
          
          slider_high_value =
            coord_to_value (range,
                            range->orientation == GTK_ORIENTATION_VERTICAL ?
                            event->y : event->x);
          slider_low_value =
            coord_to_value (range,
                            range->orientation == GTK_ORIENTATION_VERTICAL ?
                            event->y - range->layout->slider.height :
                            event->x - range->layout->slider.width);
          
          /* compute new value for warped slider */
          new_value = slider_low_value + (slider_high_value - slider_low_value) / 2;

	  /* recalc slider, so we can set slide_initial_slider_position
           * properly
           */
	  range->need_recalc = TRUE;
          gtk_range_calc_layout (range, new_value);

	  /* defer adjustment updates to update_slider_position() in order
	   * to keep pixel quantisation
	   */
	  need_value_update = TRUE;
        }
      
      if (range->orientation == GTK_ORIENTATION_VERTICAL)
        {
          range->slide_initial_slider_position = range->layout->slider.y;
          range->slide_initial_coordinate = event->y;
        }
      else
        {
          range->slide_initial_slider_position = range->layout->slider.x;
          range->slide_initial_coordinate = event->x;
        }

      range_grab_add (range, MOUSE_SLIDER, event->button);

      gtk_widget_style_get (widget, "activate-slider", &activate_slider, NULL);

      /* force a redraw, if the active slider is drawn differently to the
       * prelight one
       */
      if (activate_slider)
        gtk_widget_queue_draw (widget);

      if (need_value_update)
        update_slider_position (range, event->x, event->y);

      return TRUE;
    }
  
  return FALSE;
}

/* During a slide, move the slider as required given new mouse position */
static void
update_slider_position (GtkRange *range,
                        gint      mouse_x,
                        gint      mouse_y)
{
  gint delta;
  gint c;
  gdouble new_value;
  gboolean handled;
  gdouble next_value;
  gdouble mark_value;
  gdouble mark_delta;
  gint i;

  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - range->slide_initial_coordinate;
  else
    delta = mouse_x - range->slide_initial_coordinate;

  c = range->slide_initial_slider_position + delta;

  new_value = coord_to_value (range, c);
  next_value = coord_to_value (range, c + 1);
  mark_delta = fabs (next_value - new_value); 

  for (i = 0; i < range->layout->n_marks; i++)
    {
      mark_value = range->layout->marks[i];

      if (fabs (range->adjustment->value - mark_value) < 3 * mark_delta)
        {
          if (fabs (new_value - mark_value) < (range->slider_end - range->slider_start) * 0.5 * mark_delta)
            {
              new_value = mark_value;
              break;
            }
        }
    }  

  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_JUMP, new_value,
                 &handled);
}

static void 
stop_scrolling (GtkRange *range)
{
  range_grab_remove (range);
  gtk_range_remove_step_timer (range);
  /* Flush any pending discontinuous/delayed updates */
  gtk_range_update_value (range);
}

static gboolean
gtk_range_grab_broken (GtkWidget          *widget,
		       GdkEventGrabBroken *event)
{
  GtkRange *range = GTK_RANGE (widget);

  if (range->layout->grab_location != MOUSE_OUTSIDE)
    {
      if (range->layout->grab_location == MOUSE_SLIDER)
	update_slider_position (range, range->layout->mouse_x, range->layout->mouse_y);
      
      stop_scrolling (range);
      
      return TRUE;
    }
  
  return FALSE;
}

static gint
gtk_range_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkRange *range = GTK_RANGE (widget);

  if (event->window == range->event_window)
    {
      range->layout->mouse_x = event->x;
      range->layout->mouse_y = event->y;
    }
  else
    {
      gdk_window_get_pointer (range->event_window,
			      &range->layout->mouse_x,
			      &range->layout->mouse_y,
			      NULL);
    }
  
  if (range->layout->grab_button == event->button)
    {
      if (range->layout->grab_location == MOUSE_SLIDER)
        update_slider_position (range, range->layout->mouse_x, range->layout->mouse_y);

      stop_scrolling (range);
      
      return TRUE;
    }

  return FALSE;
}

/**
 * _gtk_range_get_wheel_delta:
 * @range: a #GtkRange
 * @event: A #GdkEventScroll
 * 
 * Returns a good step value for the mouse wheel.
 * 
 * Return value: A good step value for the mouse wheel. 
 * 
 * Since: 2.4
 **/
gdouble
_gtk_range_get_wheel_delta (GtkRange       *range,
                            GdkEventScroll *event)
{
  GtkAdjustment *adj = range->adjustment;
  gdouble dx, dy;
  gdouble delta;

  if (gdk_event_get_scroll_deltas ((GdkEvent *) event, &dx, &dy))
    {
      GtkAllocation allocation;

      gtk_widget_get_allocation (GTK_WIDGET (range), &allocation);

      if (gtk_orientable_get_orientation (GTK_ORIENTABLE (range)) == GTK_ORIENTATION_HORIZONTAL)
        {
          if (GTK_IS_SCROLLBAR (range) && adj->page_size > 0)
            delta = dx * adj->page_size / allocation.width;
          else
            delta = dx * (adj->upper - adj->lower) / allocation.width;
        }
      else
        {
          if (GTK_IS_SCROLLBAR (range) && adj->page_size > 0)
            delta = dy * adj->page_size / allocation.height;
          else
            delta = dy * (adj->upper - adj->lower) / allocation.height;
        }
    }
  else
    {
      if (GTK_IS_SCROLLBAR (range))
        delta = pow (adj->page_size, 2.0 / 3.0);
      else
        delta = adj->step_increment * 2;

      if (event->direction == GDK_SCROLL_UP ||
          event->direction == GDK_SCROLL_LEFT)
        delta = - delta;
    }

  if (range->inverted)
    delta = - delta;

  return delta;
}

static gboolean
gtk_range_scroll_event (GtkWidget      *widget,
			GdkEventScroll *event)
{
  GtkRange *range = GTK_RANGE (widget);

  if (gtk_widget_get_realized (widget))
    {
      GtkAdjustment *adj = GTK_RANGE (range)->adjustment;
      gdouble delta;
      gboolean handled;

      delta = _gtk_range_get_wheel_delta (range, event);

      g_signal_emit (range, signals[CHANGE_VALUE], 0,
                     GTK_SCROLL_JUMP, adj->value + delta,
                     &handled);
      
      /* Policy DELAYED makes sense with scroll events,
       * but DISCONTINUOUS doesn't, so we update immediately
       * for DISCONTINUOUS
       */
      if (range->update_policy == GTK_UPDATE_DISCONTINUOUS)
        gtk_range_update_value (range);
    }

  return TRUE;
}

static gboolean
gtk_range_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkRange *range;

  range = GTK_RANGE (widget);

  gdk_event_request_motions (event);
  
  range->layout->mouse_x = event->x;
  range->layout->mouse_y = event->y;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);

  if (range->layout->grab_location == MOUSE_SLIDER)
    update_slider_position (range, event->x, event->y);

  /* We handled the event if the mouse was in the range_rect */
  return range->layout->mouse_location != MOUSE_OUTSIDE;
}

static gboolean
gtk_range_enter_notify (GtkWidget        *widget,
			GdkEventCrossing *event)
{
  GtkRange *range = GTK_RANGE (widget);

  range->layout->mouse_x = event->x;
  range->layout->mouse_y = event->y;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);
  
  return TRUE;
}

static gboolean
gtk_range_leave_notify (GtkWidget        *widget,
			GdkEventCrossing *event)
{
  GtkRange *range = GTK_RANGE (widget);

  range->layout->mouse_x = -1;
  range->layout->mouse_y = -1;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);
  
  return TRUE;
}

static void
gtk_range_grab_notify (GtkWidget *widget,
		       gboolean   was_grabbed)
{
  if (!was_grabbed)
    stop_scrolling (GTK_RANGE (widget));
}

static void
gtk_range_state_changed (GtkWidget    *widget,
			 GtkStateType  previous_state)
{
  if (!gtk_widget_is_sensitive (widget))
    stop_scrolling (GTK_RANGE (widget));
}

#define check_rectangle(rectangle1, rectangle2)              \
  {                                                          \
    if (rectangle1.x != rectangle2.x) return TRUE;           \
    if (rectangle1.y != rectangle2.y) return TRUE;           \
    if (rectangle1.width  != rectangle2.width)  return TRUE; \
    if (rectangle1.height != rectangle2.height) return TRUE; \
  }

static gboolean
layout_changed (GtkRangeLayout *layout1, 
		GtkRangeLayout *layout2)
{
  check_rectangle (layout1->slider, layout2->slider);
  check_rectangle (layout1->trough, layout2->trough);
  check_rectangle (layout1->stepper_a, layout2->stepper_a);
  check_rectangle (layout1->stepper_d, layout2->stepper_d);
  check_rectangle (layout1->stepper_b, layout2->stepper_b);
  check_rectangle (layout1->stepper_c, layout2->stepper_c);

  if (layout1->upper_sensitive != layout2->upper_sensitive) return TRUE;
  if (layout1->lower_sensitive != layout2->lower_sensitive) return TRUE;

  return FALSE;
}

static void
gtk_range_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GtkRange *range = GTK_RANGE (data);
  /* create a copy of the layout */
  GtkRangeLayout layout = *range->layout;

  range->layout->recalc_marks = TRUE;
  range->need_recalc = TRUE;
  gtk_range_calc_layout (range, range->adjustment->value);
  
  /* now check whether the layout changed  */
  if (layout_changed (range->layout, &layout))
    gtk_widget_queue_draw (GTK_WIDGET (range));

  /* Note that we don't round off to range->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */
}

static gboolean
force_repaint (gpointer data)
{
  GtkRange *range = GTK_RANGE (data);

  range->layout->repaint_id = 0;
  if (gtk_widget_is_drawable (GTK_WIDGET (range)))
    gdk_window_process_updates (GTK_WIDGET (range)->window, FALSE);

  return FALSE;
}

static void
gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GtkRange *range = GTK_RANGE (data);
  /* create a copy of the layout */
  GtkRangeLayout layout = *range->layout;

  range->need_recalc = TRUE;
  gtk_range_calc_layout (range, range->adjustment->value);
  
  /* now check whether the layout changed  */
  if (layout_changed (range->layout, &layout) ||
      (GTK_IS_SCALE (range) && GTK_SCALE (range)->draw_value))
    {
      gtk_widget_queue_draw (GTK_WIDGET (range));
      /* setup a timer to ensure the range isn't lagging too much behind the scroll position */
      if (!range->layout->repaint_id)
        range->layout->repaint_id = gdk_threads_add_timeout_full (GDK_PRIORITY_EVENTS, 181, force_repaint, range, NULL);
    }
  
  /* Note that we don't round off to range->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */

  g_signal_emit (range, signals[VALUE_CHANGED], 0);
}

static void
gtk_range_style_set (GtkWidget *widget,
                     GtkStyle  *previous_style)
{
  GtkRange *range = GTK_RANGE (widget);

  range->need_recalc = TRUE;

  GTK_WIDGET_CLASS (gtk_range_parent_class)->style_set (widget, previous_style);
}

static void
apply_marks (GtkRange *range, 
             gdouble   oldval,
             gdouble  *newval)
{
  gint i;
  gdouble mark;

  for (i = 0; i < range->layout->n_marks; i++)
    {
      mark = range->layout->marks[i];
      if ((oldval < mark && mark < *newval) ||
          (oldval > mark && mark > *newval))
        {
          *newval = mark;
          return;
        }
    }
}

static void
step_back (GtkRange *range)
{
  gdouble newval;
  gboolean handled;
  
  newval = range->adjustment->value - range->adjustment->step_increment;
  apply_marks (range, range->adjustment->value, &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_STEP_BACKWARD, newval, &handled);
}

static void
step_forward (GtkRange *range)
{
  gdouble newval;
  gboolean handled;

  newval = range->adjustment->value + range->adjustment->step_increment;
  apply_marks (range, range->adjustment->value, &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_STEP_FORWARD, newval, &handled);
}


static void
page_back (GtkRange *range)
{
  gdouble newval;
  gboolean handled;

  newval = range->adjustment->value - range->adjustment->page_increment;
  apply_marks (range, range->adjustment->value, &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_PAGE_BACKWARD, newval, &handled);
}

static void
page_forward (GtkRange *range)
{
  gdouble newval;
  gboolean handled;

  newval = range->adjustment->value + range->adjustment->page_increment;
  apply_marks (range, range->adjustment->value, &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_PAGE_FORWARD, newval, &handled);
}

static void
scroll_begin (GtkRange *range)
{
  gboolean handled;
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_START, range->adjustment->lower,
                 &handled);
}

static void
scroll_end (GtkRange *range)
{
  gdouble newval;
  gboolean handled;

  newval = range->adjustment->upper - range->adjustment->page_size;
  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_END, newval,
                 &handled);
}

static gboolean
gtk_range_scroll (GtkRange     *range,
                  GtkScrollType scroll)
{
  gdouble old_value = range->adjustment->value;

  switch (scroll)
    {
    case GTK_SCROLL_STEP_LEFT:
      if (should_invert (range))
        step_forward (range);
      else
        step_back (range);
      break;
                    
    case GTK_SCROLL_STEP_UP:
      if (should_invert (range))
        step_forward (range);
      else
        step_back (range);
      break;

    case GTK_SCROLL_STEP_RIGHT:
      if (should_invert (range))
        step_back (range);
      else
        step_forward (range);
      break;
                    
    case GTK_SCROLL_STEP_DOWN:
      if (should_invert (range))
        step_back (range);
      else
        step_forward (range);
      break;
                  
    case GTK_SCROLL_STEP_BACKWARD:
      step_back (range);
      break;
                  
    case GTK_SCROLL_STEP_FORWARD:
      step_forward (range);
      break;

    case GTK_SCROLL_PAGE_LEFT:
      if (should_invert (range))
        page_forward (range);
      else
        page_back (range);
      break;
                    
    case GTK_SCROLL_PAGE_UP:
      if (should_invert (range))
        page_forward (range);
      else
        page_back (range);
      break;

    case GTK_SCROLL_PAGE_RIGHT:
      if (should_invert (range))
        page_back (range);
      else
        page_forward (range);
      break;
                    
    case GTK_SCROLL_PAGE_DOWN:
      if (should_invert (range))
        page_back (range);
      else
        page_forward (range);
      break;
                  
    case GTK_SCROLL_PAGE_BACKWARD:
      page_back (range);
      break;
                  
    case GTK_SCROLL_PAGE_FORWARD:
      page_forward (range);
      break;

    case GTK_SCROLL_START:
      scroll_begin (range);
      break;

    case GTK_SCROLL_END:
      scroll_end (range);
      break;

    case GTK_SCROLL_JUMP:
      /* Used by CList, range doesn't use it. */
      break;

    case GTK_SCROLL_NONE:
      break;
    }

  return range->adjustment->value != old_value;
}

static void
gtk_range_move_slider (GtkRange     *range,
                       GtkScrollType scroll)
{
  gboolean cursor_only;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (range)),
                "gtk-keynav-cursor-only", &cursor_only,
                NULL);

  if (cursor_only)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (range));

      if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (scroll == GTK_SCROLL_STEP_UP ||
              scroll == GTK_SCROLL_STEP_DOWN)
            {
              if (toplevel)
                gtk_widget_child_focus (toplevel,
                                        scroll == GTK_SCROLL_STEP_UP ?
                                        GTK_DIR_UP : GTK_DIR_DOWN);
              return;
            }
        }
      else
        {
          if (scroll == GTK_SCROLL_STEP_LEFT ||
              scroll == GTK_SCROLL_STEP_RIGHT)
            {
              if (toplevel)
                gtk_widget_child_focus (toplevel,
                                        scroll == GTK_SCROLL_STEP_LEFT ?
                                        GTK_DIR_LEFT : GTK_DIR_RIGHT);
              return;
            }
        }
    }

  if (! gtk_range_scroll (range, scroll))
    gtk_widget_error_bell (GTK_WIDGET (range));

  /* Policy DELAYED makes sense with key events,
   * but DISCONTINUOUS doesn't, so we update immediately
   * for DISCONTINUOUS
   */
  if (range->update_policy == GTK_UPDATE_DISCONTINUOUS)
    gtk_range_update_value (range);
}

static void
gtk_range_get_props (GtkRange  *range,
                     gint      *slider_width,
                     gint      *stepper_size,
                     gint      *focus_width,
                     gint      *trough_border,
                     gint      *stepper_spacing,
                     gboolean  *trough_under_steppers,
		     gint      *arrow_displacement_x,
		     gint      *arrow_displacement_y)
{
  GtkWidget *widget =  GTK_WIDGET (range);
  gint tmp_slider_width, tmp_stepper_size, tmp_focus_width, tmp_trough_border;
  gint tmp_stepper_spacing, tmp_trough_under_steppers;
  gint tmp_arrow_displacement_x, tmp_arrow_displacement_y;
  
  gtk_widget_style_get (widget,
                        "slider-width", &tmp_slider_width,
                        "trough-border", &tmp_trough_border,
                        "stepper-size", &tmp_stepper_size,
                        "stepper-spacing", &tmp_stepper_spacing,
                        "trough-under-steppers", &tmp_trough_under_steppers,
			"arrow-displacement-x", &tmp_arrow_displacement_x,
			"arrow-displacement-y", &tmp_arrow_displacement_y,
                        NULL);

  if (tmp_stepper_spacing > 0)
    tmp_trough_under_steppers = FALSE;

  if (gtk_widget_get_can_focus (GTK_WIDGET (range)))
    {
      gint focus_line_width;
      gint focus_padding;
      
      gtk_widget_style_get (GTK_WIDGET (range),
			    "focus-line-width", &focus_line_width,
			    "focus-padding", &focus_padding,
			    NULL);

      tmp_focus_width = focus_line_width + focus_padding;
    }
  else
    {
      tmp_focus_width = 0;
    }
  
  if (slider_width)
    *slider_width = tmp_slider_width;

  if (focus_width)
    *focus_width = tmp_focus_width;

  if (trough_border)
    *trough_border = tmp_trough_border;

  if (stepper_size)
    *stepper_size = tmp_stepper_size;

  if (stepper_spacing)
    *stepper_spacing = tmp_stepper_spacing;

  if (trough_under_steppers)
    *trough_under_steppers = tmp_trough_under_steppers;

  if (arrow_displacement_x)
    *arrow_displacement_x = tmp_arrow_displacement_x;

  if (arrow_displacement_y)
    *arrow_displacement_y = tmp_arrow_displacement_y;
}

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

/* Update mouse location, return TRUE if it changes */
static gboolean
gtk_range_update_mouse_location (GtkRange *range)
{
  gint x, y;
  MouseLocation old;
  GtkWidget *widget;

  widget = GTK_WIDGET (range);
  
  old = range->layout->mouse_location;
  
  x = range->layout->mouse_x;
  y = range->layout->mouse_y;

  if (range->layout->grab_location != MOUSE_OUTSIDE)
    range->layout->mouse_location = range->layout->grab_location;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_a))
    range->layout->mouse_location = MOUSE_STEPPER_A;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_b))
    range->layout->mouse_location = MOUSE_STEPPER_B;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_c))
    range->layout->mouse_location = MOUSE_STEPPER_C;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_d))
    range->layout->mouse_location = MOUSE_STEPPER_D;
  else if (POINT_IN_RECT (x, y, range->layout->slider))
    range->layout->mouse_location = MOUSE_SLIDER;
  else if (POINT_IN_RECT (x, y, range->layout->trough))
    range->layout->mouse_location = MOUSE_TROUGH;
  else if (POINT_IN_RECT (x, y, widget->allocation))
    range->layout->mouse_location = MOUSE_WIDGET;
  else
    range->layout->mouse_location = MOUSE_OUTSIDE;

  return old != range->layout->mouse_location;
}

/* Clamp rect, border inside widget->allocation, such that we prefer
 * to take space from border not rect in all directions, and prefer to
 * give space to border over rect in one direction.
 */
static void
clamp_dimensions (GtkWidget    *widget,
                  GdkRectangle *rect,
                  GtkBorder    *border,
                  gboolean      border_expands_horizontally)
{
  gint extra, shortage;
  
  g_return_if_fail (rect->x == 0);
  g_return_if_fail (rect->y == 0);  
  g_return_if_fail (rect->width >= 0);
  g_return_if_fail (rect->height >= 0);

  /* Width */
  
  extra = widget->allocation.width - border->left - border->right - rect->width;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          border->left += extra / 2;
          border->right += extra / 2 + extra % 2;
        }
      else
        {
          rect->width += extra;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = rect->width - widget->allocation.width;
  if (shortage > 0)
    {
      rect->width = widget->allocation.width;
      /* lose the border */
      border->left = 0;
      border->right = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->width + border->left + border->right -
        widget->allocation.width;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->left -= shortage / 2;
          border->right -= shortage / 2 + shortage % 2;
        }
    }

  /* Height */
  
  extra = widget->allocation.height - border->top - border->bottom - rect->height;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          /* don't expand border vertically */
          rect->height += extra;
        }
      else
        {
          border->top += extra / 2;
          border->bottom += extra / 2 + extra % 2;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = rect->height - widget->allocation.height;
  if (shortage > 0)
    {
      rect->height = widget->allocation.height;
      /* lose the border */
      border->top = 0;
      border->bottom = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->height + border->top + border->bottom -
        widget->allocation.height;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->top -= shortage / 2;
          border->bottom -= shortage / 2 + shortage % 2;
        }
    }
}

static void
gtk_range_calc_request (GtkRange      *range,
                        gint           slider_width,
                        gint           stepper_size,
                        gint           focus_width,
                        gint           trough_border,
                        gint           stepper_spacing,
                        GdkRectangle  *range_rect,
                        GtkBorder     *border,
                        gint          *n_steppers_p,
                        gboolean      *has_steppers_ab,
                        gboolean      *has_steppers_cd,
                        gint          *slider_length_p)
{
  gint slider_length;
  gint n_steppers;
  gint n_steppers_ab;
  gint n_steppers_cd;

  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (GTK_RANGE_GET_CLASS (range)->get_range_border)
    GTK_RANGE_GET_CLASS (range)->get_range_border (range, border);

  n_steppers_ab = 0;
  n_steppers_cd = 0;

  if (range->has_stepper_a)
    n_steppers_ab += 1;
  if (range->has_stepper_b)
    n_steppers_ab += 1;
  if (range->has_stepper_c)
    n_steppers_cd += 1;
  if (range->has_stepper_d)
    n_steppers_cd += 1;

  n_steppers = n_steppers_ab + n_steppers_cd;

  slider_length = range->min_slider_size;

  range_rect->x = 0;
  range_rect->y = 0;
  
  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      range_rect->width = (focus_width + trough_border) * 2 + slider_width;
      range_rect->height = stepper_size * n_steppers + (focus_width + trough_border) * 2 + slider_length;

      if (n_steppers_ab > 0)
        range_rect->height += stepper_spacing;

      if (n_steppers_cd > 0)
        range_rect->height += stepper_spacing;
    }
  else
    {
      range_rect->width = stepper_size * n_steppers + (focus_width + trough_border) * 2 + slider_length;
      range_rect->height = (focus_width + trough_border) * 2 + slider_width;

      if (n_steppers_ab > 0)
        range_rect->width += stepper_spacing;

      if (n_steppers_cd > 0)
        range_rect->width += stepper_spacing;
    }

  if (n_steppers_p)
    *n_steppers_p = n_steppers;

  if (has_steppers_ab)
    *has_steppers_ab = (n_steppers_ab > 0);

  if (has_steppers_cd)
    *has_steppers_cd = (n_steppers_cd > 0);

  if (slider_length_p)
    *slider_length_p = slider_length;
}

static void
gtk_range_calc_layout (GtkRange *range,
		       gdouble   adjustment_value)
{
  gint slider_width, stepper_size, focus_width, trough_border, stepper_spacing;
  gint slider_length;
  GtkBorder border;
  gint n_steppers;
  gboolean has_steppers_ab;
  gboolean has_steppers_cd;
  gboolean trough_under_steppers;
  GdkRectangle range_rect;
  GtkRangeLayout *layout;
  GtkWidget *widget;
  
  if (!range->need_recalc)
    return;

  /* If we have a too-small allocation, we prefer the steppers over
   * the trough/slider, probably the steppers are a more useful
   * feature in small spaces.
   *
   * Also, we prefer to draw the range itself rather than the border
   * areas if there's a conflict, since the borders will be decoration
   * not controls. Though this depends on subclasses cooperating by
   * not drawing on range->range_rect.
   */

  widget = GTK_WIDGET (range);
  layout = range->layout;
  
  gtk_range_get_props (range,
                       &slider_width, &stepper_size,
                       &focus_width, &trough_border,
                       &stepper_spacing, &trough_under_steppers,
		       NULL, NULL);

  gtk_range_calc_request (range, 
                          slider_width, stepper_size,
                          focus_width, trough_border, stepper_spacing,
                          &range_rect, &border, &n_steppers,
                          &has_steppers_ab, &has_steppers_cd, &slider_length);
  
  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      clamp_dimensions (widget, &range_rect, &border, TRUE);
    }
  else
    {
      clamp_dimensions (widget, &range_rect, &border, FALSE);
    }
  
  range_rect.x = border.left;
  range_rect.y = border.top;
  
  range->range_rect = range_rect;
  
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint stepper_width, stepper_height;

      /* Steppers are the width of the range, and stepper_size in
       * height, or if we don't have enough height, divided equally
       * among available space.
       */
      stepper_width = range_rect.width - focus_width * 2;

      if (trough_under_steppers)
        stepper_width -= trough_border * 2;

      if (stepper_width < 1)
        stepper_width = range_rect.width; /* screw the trough border */

      if (n_steppers == 0)
        stepper_height = 0; /* avoid divide by n_steppers */
      else
        stepper_height = MIN (stepper_size, (range_rect.height / n_steppers));

      /* Stepper A */
      
      layout->stepper_a.x = range_rect.x + focus_width + trough_border * trough_under_steppers;
      layout->stepper_a.y = range_rect.y + focus_width + trough_border * trough_under_steppers;

      if (range->has_stepper_a)
        {
          layout->stepper_a.width = stepper_width;
          layout->stepper_a.height = stepper_height;
        }
      else
        {
          layout->stepper_a.width = 0;
          layout->stepper_a.height = 0;
        }

      /* Stepper B */
      
      layout->stepper_b.x = layout->stepper_a.x;
      layout->stepper_b.y = layout->stepper_a.y + layout->stepper_a.height;

      if (range->has_stepper_b)
        {
          layout->stepper_b.width = stepper_width;
          layout->stepper_b.height = stepper_height;
        }
      else
        {
          layout->stepper_b.width = 0;
          layout->stepper_b.height = 0;
        }

      /* Stepper D */

      if (range->has_stepper_d)
        {
          layout->stepper_d.width = stepper_width;
          layout->stepper_d.height = stepper_height;
        }
      else
        {
          layout->stepper_d.width = 0;
          layout->stepper_d.height = 0;
        }
      
      layout->stepper_d.x = layout->stepper_a.x;
      layout->stepper_d.y = range_rect.y + range_rect.height - layout->stepper_d.height - focus_width - trough_border * trough_under_steppers;

      /* Stepper C */

      if (range->has_stepper_c)
        {
          layout->stepper_c.width = stepper_width;
          layout->stepper_c.height = stepper_height;
        }
      else
        {
          layout->stepper_c.width = 0;
          layout->stepper_c.height = 0;
        }
      
      layout->stepper_c.x = layout->stepper_a.x;
      layout->stepper_c.y = layout->stepper_d.y - layout->stepper_c.height;

      /* Now the trough is the remaining space between steppers B and C,
       * if any, minus spacing
       */
      layout->trough.x = range_rect.x;
      layout->trough.y = layout->stepper_b.y + layout->stepper_b.height + stepper_spacing * has_steppers_ab;
      layout->trough.width = range_rect.width;
      layout->trough.height = layout->stepper_c.y - layout->trough.y - stepper_spacing * has_steppers_cd;

      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      layout->slider.x = layout->trough.x + focus_width + trough_border;
      layout->slider.width = layout->trough.width - (focus_width + trough_border) * 2;

      /* Compute slider position/length */
      {
        gint y, bottom, top, height;
        
        top = layout->trough.y;
        bottom = layout->trough.y + layout->trough.height;

        if (! trough_under_steppers)
          {
            top += trough_border;
            bottom -= trough_border;
          }

        /* slider height is the fraction (page_size /
         * total_adjustment_range) times the trough height in pixels
         */

	if (range->adjustment->upper - range->adjustment->lower != 0)
	  height = ((bottom - top) * (range->adjustment->page_size /
				       (range->adjustment->upper - range->adjustment->lower)));
	else
	  height = range->min_slider_size;
        
        if (height < range->min_slider_size ||
            range->slider_size_fixed)
          height = range->min_slider_size;

        height = MIN (height, layout->trough.height);
        
        y = top;
        
	if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
	  y += (bottom - top - height) * ((adjustment_value - range->adjustment->lower) /
					  (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));
        
        y = CLAMP (y, top, bottom);
        
        if (should_invert (range))
          y = bottom - (y - top + height);
        
        layout->slider.y = y;
        layout->slider.height = height;

        /* These are publically exported */
        range->slider_start = layout->slider.y;
        range->slider_end = layout->slider.y + layout->slider.height;
      }
    }
  else
    {
      gint stepper_width, stepper_height;

      /* Steppers are the height of the range, and stepper_size in
       * width, or if we don't have enough width, divided equally
       * among available space.
       */
      stepper_height = range_rect.height + focus_width * 2;

      if (trough_under_steppers)
        stepper_height -= trough_border * 2;

      if (stepper_height < 1)
        stepper_height = range_rect.height; /* screw the trough border */

      if (n_steppers == 0)
        stepper_width = 0; /* avoid divide by n_steppers */
      else
        stepper_width = MIN (stepper_size, (range_rect.width / n_steppers));

      /* Stepper A */
      
      layout->stepper_a.x = range_rect.x + focus_width + trough_border * trough_under_steppers;
      layout->stepper_a.y = range_rect.y + focus_width + trough_border * trough_under_steppers;

      if (range->has_stepper_a)
        {
          layout->stepper_a.width = stepper_width;
          layout->stepper_a.height = stepper_height;
        }
      else
        {
          layout->stepper_a.width = 0;
          layout->stepper_a.height = 0;
        }

      /* Stepper B */
      
      layout->stepper_b.x = layout->stepper_a.x + layout->stepper_a.width;
      layout->stepper_b.y = layout->stepper_a.y;

      if (range->has_stepper_b)
        {
          layout->stepper_b.width = stepper_width;
          layout->stepper_b.height = stepper_height;
        }
      else
        {
          layout->stepper_b.width = 0;
          layout->stepper_b.height = 0;
        }

      /* Stepper D */

      if (range->has_stepper_d)
        {
          layout->stepper_d.width = stepper_width;
          layout->stepper_d.height = stepper_height;
        }
      else
        {
          layout->stepper_d.width = 0;
          layout->stepper_d.height = 0;
        }

      layout->stepper_d.x = range_rect.x + range_rect.width - layout->stepper_d.width - focus_width - trough_border * trough_under_steppers;
      layout->stepper_d.y = layout->stepper_a.y;


      /* Stepper C */

      if (range->has_stepper_c)
        {
          layout->stepper_c.width = stepper_width;
          layout->stepper_c.height = stepper_height;
        }
      else
        {
          layout->stepper_c.width = 0;
          layout->stepper_c.height = 0;
        }
      
      layout->stepper_c.x = layout->stepper_d.x - layout->stepper_c.width;
      layout->stepper_c.y = layout->stepper_a.y;

      /* Now the trough is the remaining space between steppers B and C,
       * if any
       */
      layout->trough.x = layout->stepper_b.x + layout->stepper_b.width + stepper_spacing * has_steppers_ab;
      layout->trough.y = range_rect.y;

      layout->trough.width = layout->stepper_c.x - layout->trough.x - stepper_spacing * has_steppers_cd;
      layout->trough.height = range_rect.height;

      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      layout->slider.y = layout->trough.y + focus_width + trough_border;
      layout->slider.height = layout->trough.height - (focus_width + trough_border) * 2;

      /* Compute slider position/length */
      {
        gint x, left, right, width;
        
        left = layout->trough.x;
        right = layout->trough.x + layout->trough.width;

        if (! trough_under_steppers)
          {
            left += trough_border;
            right -= trough_border;
          }

        /* slider width is the fraction (page_size /
         * total_adjustment_range) times the trough width in pixels
         */
	
	if (range->adjustment->upper - range->adjustment->lower != 0)
	  width = ((right - left) * (range->adjustment->page_size /
                                   (range->adjustment->upper - range->adjustment->lower)));
	else
	  width = range->min_slider_size;
        
        if (width < range->min_slider_size ||
            range->slider_size_fixed)
          width = range->min_slider_size;
        
        width = MIN (width, layout->trough.width);
        
        x = left;
        
	if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
          x += (right - left - width) * ((adjustment_value - range->adjustment->lower) /
                                         (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));
        
        x = CLAMP (x, left, right);
        
        if (should_invert (range))
          x = right - (x - left + width);
        
        layout->slider.x = x;
        layout->slider.width = width;

        /* These are publically exported */
        range->slider_start = layout->slider.x;
        range->slider_end = layout->slider.x + layout->slider.width;
      }
    }
  
  gtk_range_update_mouse_location (range);

  switch (range->layout->upper_sensitivity)
    {
    case GTK_SENSITIVITY_AUTO:
      range->layout->upper_sensitive =
        (range->adjustment->value <
         (range->adjustment->upper - range->adjustment->page_size));
      break;

    case GTK_SENSITIVITY_ON:
      range->layout->upper_sensitive = TRUE;
      break;

    case GTK_SENSITIVITY_OFF:
      range->layout->upper_sensitive = FALSE;
      break;
    }

  switch (range->layout->lower_sensitivity)
    {
    case GTK_SENSITIVITY_AUTO:
      range->layout->lower_sensitive =
        (range->adjustment->value > range->adjustment->lower);
      break;

    case GTK_SENSITIVITY_ON:
      range->layout->lower_sensitive = TRUE;
      break;

    case GTK_SENSITIVITY_OFF:
      range->layout->lower_sensitive = FALSE;
      break;
    }
}

static GdkRectangle*
get_area (GtkRange     *range,
          MouseLocation location)
{
  switch (location)
    {
    case MOUSE_STEPPER_A:
      return &range->layout->stepper_a;
    case MOUSE_STEPPER_B:
      return &range->layout->stepper_b;
    case MOUSE_STEPPER_C:
      return &range->layout->stepper_c;
    case MOUSE_STEPPER_D:
      return &range->layout->stepper_d;
    case MOUSE_TROUGH:
      return &range->layout->trough;
    case MOUSE_SLIDER:
      return &range->layout->slider;
    case MOUSE_WIDGET:
    case MOUSE_OUTSIDE:
      break;
    }

  g_warning (G_STRLOC": bug");
  return NULL;
}

static void
gtk_range_calc_marks (GtkRange *range)
{
  gint i;
  
  if (!range->layout->recalc_marks)
    return;

  range->layout->recalc_marks = FALSE;

  for (i = 0; i < range->layout->n_marks; i++)
    {
      range->need_recalc = TRUE;
      gtk_range_calc_layout (range, range->layout->marks[i]);
      if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
        range->layout->mark_pos[i] = range->layout->slider.x + range->layout->slider.width / 2;
      else
        range->layout->mark_pos[i] = range->layout->slider.y + range->layout->slider.height / 2;
    }

  range->need_recalc = TRUE;
}

static gboolean
gtk_range_real_change_value (GtkRange     *range,
                             GtkScrollType scroll,
                             gdouble       value)
{
  /* potentially adjust the bounds _before we clamp */
  g_signal_emit (range, signals[ADJUST_BOUNDS], 0, value);

  if (range->layout->restrict_to_fill_level)
    value = MIN (value, MAX (range->adjustment->lower,
                             range->layout->fill_level));

  value = CLAMP (value, range->adjustment->lower,
                 (range->adjustment->upper - range->adjustment->page_size));

  if (range->round_digits >= 0)
    {
      gdouble power;
      gint i;

      i = range->round_digits;
      power = 1;
      while (i--)
        power *= 10;
      
      value = floor ((value * power) + 0.5) / power;
    }
  
  if (range->adjustment->value != value)
    {
      range->need_recalc = TRUE;

      gtk_widget_queue_draw (GTK_WIDGET (range));
      
      switch (range->update_policy)
        {
        case GTK_UPDATE_CONTINUOUS:
          gtk_adjustment_set_value (range->adjustment, value);
          break;

          /* Delayed means we update after a period of inactivity */
        case GTK_UPDATE_DELAYED:
          gtk_range_reset_update_timer (range);
          /* FALL THRU */

          /* Discontinuous means we update on button release */
        case GTK_UPDATE_DISCONTINUOUS:
          /* don't emit value_changed signal */
          range->adjustment->value = value;
          range->update_pending = TRUE;
          break;
        }
    }
  return FALSE;
}

static void
gtk_range_update_value (GtkRange *range)
{
  gtk_range_remove_update_timer (range);
  
  if (range->update_pending)
    {
      gtk_adjustment_value_changed (range->adjustment);

      range->update_pending = FALSE;
    }
}

struct _GtkRangeStepTimer
{
  guint timeout_id;
  GtkScrollType step;
};

static gboolean
second_timeout (gpointer data)
{
  GtkRange *range;

  range = GTK_RANGE (data);
  gtk_range_scroll (range, range->timer->step);
  
  return TRUE;
}

static gboolean
initial_timeout (gpointer data)
{
  GtkRange    *range;
  GtkSettings *settings;
  guint        timeout;

  settings = gtk_widget_get_settings (GTK_WIDGET (data));
  g_object_get (settings, "gtk-timeout-repeat", &timeout, NULL);

  range = GTK_RANGE (data);
  range->timer->timeout_id = gdk_threads_add_timeout (timeout * SCROLL_DELAY_FACTOR,
                                            second_timeout,
                                            range);
  /* remove self */
  return FALSE;
}

static void
gtk_range_add_step_timer (GtkRange      *range,
                          GtkScrollType  step)
{
  GtkSettings *settings;
  guint        timeout;

  g_return_if_fail (range->timer == NULL);
  g_return_if_fail (step != GTK_SCROLL_NONE);

  settings = gtk_widget_get_settings (GTK_WIDGET (range));
  g_object_get (settings, "gtk-timeout-initial", &timeout, NULL);

  range->timer = g_new (GtkRangeStepTimer, 1);

  range->timer->timeout_id = gdk_threads_add_timeout (timeout,
                                            initial_timeout,
                                            range);
  range->timer->step = step;

  gtk_range_scroll (range, range->timer->step);
}

static void
gtk_range_remove_step_timer (GtkRange *range)
{
  if (range->timer)
    {
      if (range->timer->timeout_id != 0)
        g_source_remove (range->timer->timeout_id);

      g_free (range->timer);

      range->timer = NULL;
    }
}

static gboolean
update_timeout (gpointer data)
{
  GtkRange *range;

  range = GTK_RANGE (data);
  gtk_range_update_value (range);
  range->update_timeout_id = 0;

  /* self-remove */
  return FALSE;
}

static void
gtk_range_reset_update_timer (GtkRange *range)
{
  gtk_range_remove_update_timer (range);

  range->update_timeout_id = gdk_threads_add_timeout (UPDATE_DELAY,
                                            update_timeout,
                                            range);
}

static void
gtk_range_remove_update_timer (GtkRange *range)
{
  if (range->update_timeout_id != 0)
    {
      g_source_remove (range->update_timeout_id);
      range->update_timeout_id = 0;
    }
}

void
_gtk_range_set_stop_values (GtkRange *range,
                            gdouble  *values,
                            gint      n_values)
{
  gint i;

  g_free (range->layout->marks);
  range->layout->marks = g_new (gdouble, n_values);

  g_free (range->layout->mark_pos);
  range->layout->mark_pos = g_new (gint, n_values);

  range->layout->n_marks = n_values;

  for (i = 0; i < n_values; i++) 
    range->layout->marks[i] = values[i];

  range->layout->recalc_marks = TRUE;
}

gint
_gtk_range_get_stop_positions (GtkRange  *range,
                               gint     **values)
{
  gtk_range_calc_marks (range);

  if (values)
    *values = g_memdup (range->layout->mark_pos, range->layout->n_marks * sizeof (gint));

  return range->layout->n_marks;
}

/**
 * gtk_range_set_round_digits:
 * @range: a #GtkRange
 * @round_digits: the precision in digits, or -1
 *
 * Sets the number of digits to round the value to when
 * it changes. See #GtkRange::change-value.
 *
 * Since: 2.24
 */
void
gtk_range_set_round_digits (GtkRange *range,
                            gint      round_digits)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (round_digits >= -1);

  range->round_digits = round_digits;

  g_object_notify (G_OBJECT (range), "round-digits");
}

/**
 * gtk_range_get_round_digits:
 * @range: a #GtkRange
 *
 * Gets the number of digits to round the value to when
 * it changes. See #GtkRange::change-value.
 *
 * Return value: the number of digits to round to
 *
 * Since: 2.24
 */
gint
gtk_range_get_round_digits (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), -1);

  return range->round_digits;
}


#define __GTK_RANGE_C__
#include "gtkaliasdef.c"
