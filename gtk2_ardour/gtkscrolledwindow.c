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

#include <config.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkmarshalers.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkintl.h>
#include <gtk/gtkalias.h>


/* scrolled window policy and size requisition handling:
 *
 * gtk size requisition works as follows:
 *   a widget upon size-request reports the width and height that it finds
 *   to be best suited to display its contents, including children.
 *   the width and/or height reported from a widget upon size requisition
 *   may be overidden by the user by specifying a width and/or height
 *   other than 0 through gtk_widget_set_usize().
 *
 * a scrolled window needs (for imlementing all three policy types) to
 * request its width and height based on two different rationales.
 * 1)   the user wants the scrolled window to just fit into the space
 *      that it gets allocated for a specifc dimension.
 * 1.1) this does not apply if the user specified a concrete value
 *      value for that specific dimension by either specifying usize for the
 *      scrolled window or for its child.
 * 2)   the user wants the scrolled window to take as much space up as
 *      is desired by the child for a specifc dimension (i.e. POLICY_NEVER).
 *
 * also, kinda obvious:
 * 3)   a user would certainly not have choosen a scrolled window as a container
 *      for the child, if the resulting allocation takes up more space than the
 *      child would have allocated without the scrolled window.
 *
 * conclusions:
 * A) from 1) follows: the scrolled window shouldn't request more space for a
 *    specifc dimension than is required at minimum.
 * B) from 1.1) follows: the requisition may be overidden by usize of the scrolled
 *    window (done automatically) or by usize of the child (needs to be checked).
 * C) from 2) follows: for POLICY_NEVER, the scrolled window simply reports the
 *    child's dimension.
 * D) from 3) follows: the scrolled window child's minimum width and minimum height
 *    under A) at least correspond to the space taken up by its scrollbars.
 */

#define DEFAULT_SCROLLBAR_SPACING  3

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_WINDOW_PLACEMENT,
  PROP_SHADOW_TYPE,
  PROP_LAST
};

/* Signals */
enum
{
  SCROLL_CHILD,
  MOVE_FOCUS_OUT,
  LAST_SIGNAL
};

static void gtk_scrolled_window_class_init         (GtkScrolledWindowClass *klass);
static void gtk_scrolled_window_init               (GtkScrolledWindow      *scrolled_window);
static void gtk_scrolled_window_destroy            (GtkObject              *object);
static void gtk_scrolled_window_finalize           (GObject                *object);
static void gtk_scrolled_window_set_property       (GObject                *object,
					            guint                   prop_id,
					            const GValue           *value,
					            GParamSpec             *pspec);
static void gtk_scrolled_window_get_property       (GObject                *object,
					            guint                   prop_id,
					            GValue                 *value,
					            GParamSpec             *pspec);

static gint gtk_scrolled_window_expose             (GtkWidget              *widget,
						    GdkEventExpose         *event);
static void gtk_scrolled_window_size_request       (GtkWidget              *widget,
						    GtkRequisition         *requisition);
static void gtk_scrolled_window_size_allocate      (GtkWidget              *widget,
						    GtkAllocation          *allocation);
static gint gtk_scrolled_window_scroll_event       (GtkWidget              *widget,
						    GdkEventScroll         *event);
static gint gtk_scrolled_window_focus              (GtkWidget              *widget,
						    GtkDirectionType        direction);
static void gtk_scrolled_window_add                (GtkContainer           *container,
						    GtkWidget              *widget);
static void gtk_scrolled_window_remove             (GtkContainer           *container,
						    GtkWidget              *widget);
static void gtk_scrolled_window_forall             (GtkContainer           *container,
						    gboolean		    include_internals,
						    GtkCallback             callback,
						    gpointer                callback_data);
static void gtk_scrolled_window_scroll_child       (GtkScrolledWindow      *scrolled_window,
						    GtkScrollType           scroll,
						    gboolean                horizontal);
static void gtk_scrolled_window_move_focus_out     (GtkScrolledWindow      *scrolled_window,
						    GtkDirectionType        direction_type);

static void gtk_scrolled_window_relative_allocation(GtkWidget              *widget,
						    GtkAllocation          *allocation);
static void gtk_scrolled_window_adjustment_changed (GtkAdjustment          *adjustment,
						    gpointer                data);

static GtkContainerClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = {0};

GType
gtk_scrolled_window_get_type (void)
{
  static GType scrolled_window_type = 0;

  if (!scrolled_window_type)
    {
      static const GTypeInfo scrolled_window_info =
      {
	sizeof (GtkScrolledWindowClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_scrolled_window_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkScrolledWindow),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_scrolled_window_init,
      };

      scrolled_window_type =
	g_type_register_static (GTK_TYPE_BIN, "GtkScrolledWindow",
				&scrolled_window_info, 0);
    }

  return scrolled_window_type;
}

static void
add_scroll_binding (GtkBindingSet  *binding_set,
		    guint           keyval,
		    GdkModifierType mask,
		    GtkScrollType   scroll,
		    gboolean        horizontal)
{
  guint keypad_keyval = keyval - GDK_Left + GDK_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keyval, mask,
                                "scroll_child", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
  gtk_binding_entry_add_signal (binding_set, keypad_keyval, mask,
                                "scroll_child", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
}

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
gtk_scrolled_window_class_init (GtkScrolledWindowClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_scrolled_window_finalize;
  gobject_class->set_property = gtk_scrolled_window_set_property;
  gobject_class->get_property = gtk_scrolled_window_get_property;

  object_class->destroy = gtk_scrolled_window_destroy;

  widget_class->expose_event = gtk_scrolled_window_expose;
  widget_class->size_request = gtk_scrolled_window_size_request;
  widget_class->size_allocate = gtk_scrolled_window_size_allocate;
  widget_class->scroll_event = gtk_scrolled_window_scroll_event;
  widget_class->focus = gtk_scrolled_window_focus;

  container_class->add = gtk_scrolled_window_add;
  container_class->remove = gtk_scrolled_window_remove;
  container_class->forall = gtk_scrolled_window_forall;

  class->scrollbar_spacing = -1;

  class->scroll_child = gtk_scrolled_window_scroll_child;
  class->move_focus_out = gtk_scrolled_window_move_focus_out;
  
  g_object_class_install_property (gobject_class,
				   PROP_HADJUSTMENT,
				   g_param_spec_object ("hadjustment",
							P_("Horizontal Adjustment"),
							P_("The GtkAdjustment for the horizontal position"),
							GTK_TYPE_ADJUSTMENT,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
				   PROP_VADJUSTMENT,
				   g_param_spec_object ("vadjustment",
							P_("Vertical Adjustment"),
							P_("The GtkAdjustment for the vertical position"),
							GTK_TYPE_ADJUSTMENT,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
                                   PROP_HSCROLLBAR_POLICY,
                                   g_param_spec_enum ("hscrollbar_policy",
                                                      P_("Horizontal Scrollbar Policy"),
                                                      P_("When the horizontal scrollbar is displayed"),
						      GTK_TYPE_POLICY_TYPE,
						      GTK_POLICY_ALWAYS,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_VSCROLLBAR_POLICY,
                                   g_param_spec_enum ("vscrollbar_policy",
                                                      P_("Vertical Scrollbar Policy"),
                                                      P_("When the vertical scrollbar is displayed"),
						      GTK_TYPE_POLICY_TYPE,
						      GTK_POLICY_ALWAYS,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_PLACEMENT,
                                   g_param_spec_enum ("window_placement",
                                                      P_("Window Placement"),
                                                      P_("Where the contents are located with respect to the scrollbars"),
						      GTK_TYPE_CORNER_TYPE,
						      GTK_CORNER_TOP_LEFT,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow_type",
                                                      P_("Shadow Type"),
                                                      P_("Style of bevel around the contents"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_NONE,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("scrollbar_spacing",
							     P_("Scrollbar spacing"),
							     P_("Number of pixels between the scrollbars and the scrolled window"),
							     0,
							     G_MAXINT,
							     DEFAULT_SCROLLBAR_SPACING,
							     G_PARAM_READABLE));

  signals[SCROLL_CHILD] =
    g_signal_new ("scroll_child",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkScrolledWindowClass, scroll_child),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_BOOLEAN,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_SCROLL_TYPE,
		  G_TYPE_BOOLEAN);
  signals[MOVE_FOCUS_OUT] =
    g_signal_new ("move_focus_out",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkScrolledWindowClass, move_focus_out),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);
  
  binding_set = gtk_binding_set_by_class (class);

  add_scroll_binding (binding_set, GDK_Left,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, TRUE);
  add_scroll_binding (binding_set, GDK_Right, GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  TRUE);
  add_scroll_binding (binding_set, GDK_Up,    GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, FALSE);
  add_scroll_binding (binding_set, GDK_Down,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  FALSE);

  add_scroll_binding (binding_set, GDK_Page_Up,   GDK_CONTROL_MASK, GTK_SCROLL_PAGE_BACKWARD, TRUE);
  add_scroll_binding (binding_set, GDK_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_FORWARD,  TRUE);
  add_scroll_binding (binding_set, GDK_Page_Up,   0,                GTK_SCROLL_PAGE_BACKWARD, FALSE);
  add_scroll_binding (binding_set, GDK_Page_Down, 0,                GTK_SCROLL_PAGE_FORWARD,  FALSE);

  add_scroll_binding (binding_set, GDK_Home, GDK_CONTROL_MASK, GTK_SCROLL_START, TRUE);
  add_scroll_binding (binding_set, GDK_End,  GDK_CONTROL_MASK, GTK_SCROLL_END,   TRUE);
  add_scroll_binding (binding_set, GDK_Home, 0,                GTK_SCROLL_START, FALSE);
  add_scroll_binding (binding_set, GDK_End,  0,                GTK_SCROLL_END,   FALSE);

  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
}

static void
gtk_scrolled_window_init (GtkScrolledWindow *scrolled_window)
{
  GTK_WIDGET_SET_FLAGS (scrolled_window, GTK_NO_WINDOW | GTK_CAN_FOCUS);

  scrolled_window->hscrollbar = NULL;
  scrolled_window->vscrollbar = NULL;
  scrolled_window->hscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->vscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->hscrollbar_visible = FALSE;
  scrolled_window->vscrollbar_visible = FALSE;
  scrolled_window->focus_out = FALSE;
  scrolled_window->window_placement = GTK_CORNER_TOP_LEFT;
  
}

GtkWidget*
gtk_scrolled_window_new (GtkAdjustment *hadjustment,
			 GtkAdjustment *vadjustment)
{
  GtkWidget *scrolled_window;

  if (hadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), NULL);

  if (vadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), NULL);

  scrolled_window = gtk_widget_new (GTK_TYPE_SCROLLED_WINDOW,
				    "hadjustment", hadjustment,
				    "vadjustment", vadjustment,
				    NULL);

  return scrolled_window;
}

void
gtk_scrolled_window_set_hadjustment (GtkScrolledWindow *scrolled_window,
				     GtkAdjustment     *hadjustment)
{
  GtkBin *bin;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  bin = GTK_BIN (scrolled_window);

  if (!scrolled_window->hscrollbar)
    {
      gtk_widget_push_composite_child ();
      scrolled_window->hscrollbar = gtk_hscrollbar_new (hadjustment);
      gtk_widget_set_composite_name (scrolled_window->hscrollbar, "hscrollbar");
      gtk_widget_pop_composite_child ();

      gtk_widget_set_parent (scrolled_window->hscrollbar, GTK_WIDGET (scrolled_window));
      g_object_ref (scrolled_window->hscrollbar);
      gtk_widget_show (scrolled_window->hscrollbar);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
      if (old_adjustment == hadjustment)
	return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
					    gtk_scrolled_window_adjustment_changed,
					    scrolled_window);
      gtk_range_set_adjustment (GTK_RANGE (scrolled_window->hscrollbar),
				hadjustment);
    }
  hadjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
  g_signal_connect (hadjustment,
		    "changed",
		    G_CALLBACK (gtk_scrolled_window_adjustment_changed),
		    scrolled_window);
  gtk_scrolled_window_adjustment_changed (hadjustment, scrolled_window);
  
  if (bin->child)
    gtk_widget_set_scroll_adjustments (bin->child,
				       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
				       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)));

  g_object_notify (G_OBJECT (scrolled_window), "hadjustment");
}

void
gtk_scrolled_window_set_vadjustment (GtkScrolledWindow *scrolled_window,
				     GtkAdjustment     *vadjustment)
{
  GtkBin *bin;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  bin = GTK_BIN (scrolled_window);

  if (!scrolled_window->vscrollbar)
    {
      gtk_widget_push_composite_child ();
      scrolled_window->vscrollbar = gtk_vscrollbar_new (vadjustment);
      gtk_widget_set_composite_name (scrolled_window->vscrollbar, "vscrollbar");
      gtk_widget_pop_composite_child ();

      gtk_widget_set_parent (scrolled_window->vscrollbar, GTK_WIDGET (scrolled_window));
      g_object_ref (scrolled_window->vscrollbar);
      gtk_widget_show (scrolled_window->vscrollbar);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
      if (old_adjustment == vadjustment)
	return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
					    gtk_scrolled_window_adjustment_changed,
					    scrolled_window);
      gtk_range_set_adjustment (GTK_RANGE (scrolled_window->vscrollbar),
				vadjustment);
    }
  vadjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
  g_signal_connect (vadjustment,
		    "changed",
		    G_CALLBACK (gtk_scrolled_window_adjustment_changed),
		    scrolled_window);
  gtk_scrolled_window_adjustment_changed (vadjustment, scrolled_window);

  if (bin->child)
    gtk_widget_set_scroll_adjustments (bin->child,
				       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
				       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)));

  g_object_notify (G_OBJECT (scrolled_window), "vadjustment");
}

GtkAdjustment*
gtk_scrolled_window_get_hadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return (scrolled_window->hscrollbar ?
	  gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)) :
	  NULL);
}

GtkAdjustment*
gtk_scrolled_window_get_vadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return (scrolled_window->vscrollbar ?
	  gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)) :
	  NULL);
}

void
gtk_scrolled_window_set_policy (GtkScrolledWindow *scrolled_window,
				GtkPolicyType      hscrollbar_policy,
				GtkPolicyType      vscrollbar_policy)
{
  GObject *object = G_OBJECT (scrolled_window);
  
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if ((scrolled_window->hscrollbar_policy != hscrollbar_policy) ||
      (scrolled_window->vscrollbar_policy != vscrollbar_policy))
    {
      scrolled_window->hscrollbar_policy = hscrollbar_policy;
      scrolled_window->vscrollbar_policy = vscrollbar_policy;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_freeze_notify (object);
      g_object_notify (object, "hscrollbar-policy");
      g_object_notify (object, "vscrollbar-policy");
      g_object_thaw_notify (object);
    }
}

/**
 * gtk_scrolled_window_get_policy:
 * @scrolled_window: a #GtkScrolledWindow
 * @hscrollbar_policy: location to store the policy for the horizontal scrollbar, or %NULL.
 * @vscrollbar_policy: location to store the policy for the horizontal scrollbar, or %NULL.
 * 
 * Retrieves the current policy values for the horizontal and vertical
 * scrollbars. See gtk_scrolled_window_set_policy().
 **/
void
gtk_scrolled_window_get_policy (GtkScrolledWindow *scrolled_window,
				GtkPolicyType     *hscrollbar_policy,
				GtkPolicyType     *vscrollbar_policy)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (hscrollbar_policy)
    *hscrollbar_policy = scrolled_window->hscrollbar_policy;
  if (vscrollbar_policy)
    *vscrollbar_policy = scrolled_window->vscrollbar_policy;
}

void
gtk_scrolled_window_set_placement (GtkScrolledWindow *scrolled_window,
				   GtkCornerType      window_placement)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (scrolled_window->window_placement != window_placement)
    {
      scrolled_window->window_placement = window_placement;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
      
      g_object_notify (G_OBJECT (scrolled_window), "window-placement");
    }
}

/**
 * gtk_scrolled_window_get_placement:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Gets the placement of the scrollbars for the scrolled window. See 
 * gtk_scrolled_window_set_placement().
 *
 * Return value: the current placement value.
 **/
GtkCornerType
gtk_scrolled_window_get_placement (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), GTK_CORNER_TOP_LEFT);

  return scrolled_window->window_placement;
}

/**
 * gtk_scrolled_window_set_shadow_type:
 * @scrolled_window: a #GtkScrolledWindow
 * @type: kind of shadow to draw around scrolled window contents
 *
 * Changes the type of shadow drawn around the contents of
 * @scrolled_window.
 * 
 **/
void
gtk_scrolled_window_set_shadow_type (GtkScrolledWindow *scrolled_window,
				     GtkShadowType      type)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (type >= GTK_SHADOW_NONE && type <= GTK_SHADOW_ETCHED_OUT);
  
  if (scrolled_window->shadow_type != type)
    {
      scrolled_window->shadow_type = type;

      if (GTK_WIDGET_DRAWABLE (scrolled_window))
	gtk_widget_queue_draw (GTK_WIDGET (scrolled_window));

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify (G_OBJECT (scrolled_window), "shadow-type");
    }
}

/**
 * gtk_scrolled_window_get_shadow_type:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Gets the shadow type of the scrolled window. See 
 * gtk_scrolled_window_set_shadow_type().
 *
 * Return value: the current shadow type
 **/
GtkShadowType
gtk_scrolled_window_get_shadow_type (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_NONE);

  return scrolled_window->shadow_type;
}

static void
gtk_scrolled_window_destroy (GtkObject *object)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (object));

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  gtk_widget_unparent (scrolled_window->hscrollbar);
  gtk_widget_unparent (scrolled_window->vscrollbar);
  gtk_widget_destroy (scrolled_window->hscrollbar);
  gtk_widget_destroy (scrolled_window->vscrollbar);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_scrolled_window_finalize (GObject *object)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);

  g_object_unref (scrolled_window->hscrollbar);
  g_object_unref (scrolled_window->vscrollbar);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_scrolled_window_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_scrolled_window_set_hadjustment (scrolled_window,
					   g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_scrolled_window_set_vadjustment (scrolled_window,
					   g_value_get_object (value));
      break;
    case PROP_HSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      g_value_get_enum (value),
				      scrolled_window->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      scrolled_window->hscrollbar_policy,
				      g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT:
      gtk_scrolled_window_set_placement (scrolled_window,
					 g_value_get_enum (value));
      break;
    case PROP_SHADOW_TYPE:
      gtk_scrolled_window_set_shadow_type (scrolled_window,
					   g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrolled_window_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value,
			  G_OBJECT (gtk_scrolled_window_get_hadjustment (scrolled_window)));
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value,
			  G_OBJECT (gtk_scrolled_window_get_vadjustment (scrolled_window)));
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, scrolled_window->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, scrolled_window->vscrollbar_policy);
      break;
    case PROP_WINDOW_PLACEMENT:
      g_value_set_enum (value, scrolled_window->window_placement);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, scrolled_window->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrolled_window_paint (GtkWidget    *widget,
			   GdkRectangle *area)
{
  GtkAllocation relative_allocation;
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);

  if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
    {
      gtk_scrolled_window_relative_allocation (widget, &relative_allocation);
      
      relative_allocation.x -= widget->style->xthickness;
      relative_allocation.y -= widget->style->ythickness;
      relative_allocation.width += 2 * widget->style->xthickness;
      relative_allocation.height += 2 * widget->style->ythickness;
      
      gtk_paint_shadow (widget->style, widget->window,
			GTK_STATE_NORMAL, scrolled_window->shadow_type,
			area, widget, "scrolled_window",
			widget->allocation.x + relative_allocation.x,
			widget->allocation.y + relative_allocation.y,
			relative_allocation.width,
			relative_allocation.height);
    }
}

static gint
gtk_scrolled_window_expose (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_scrolled_window_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static void
gtk_scrolled_window_forall (GtkContainer *container,
			    gboolean	  include_internals,
			    GtkCallback   callback,
			    gpointer      callback_data)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (callback != NULL);

  GTK_CONTAINER_CLASS (parent_class)->forall (container,
					      include_internals,
					      callback,
					      callback_data);
  if (include_internals)
    {
      GtkScrolledWindow *scrolled_window;

      scrolled_window = GTK_SCROLLED_WINDOW (container);
      
      if (scrolled_window->vscrollbar)
	callback (scrolled_window->vscrollbar, callback_data);
      if (scrolled_window->hscrollbar)
	callback (scrolled_window->hscrollbar, callback_data);
    }
}

static void
gtk_scrolled_window_scroll_child (GtkScrolledWindow *scrolled_window,
				  GtkScrollType      scroll,
				  gboolean           horizontal)
{
  GtkAdjustment *adjustment = NULL;
  
  switch (scroll)
    {
    case GTK_SCROLL_STEP_UP:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_DOWN:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_UP:
      scroll = GTK_SCROLL_PAGE_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_DOWN:
      scroll = GTK_SCROLL_PAGE_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_START:
    case GTK_SCROLL_END:
      break;
    default:
      g_warning ("Invalid scroll type %d for GtkSpinButton::change-value", scroll);
      return;
    }

  if (horizontal)
    {
      if (scrolled_window->hscrollbar)
	adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
    }
  else
    {
      if (scrolled_window->vscrollbar)
	adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
    }

  if (adjustment)
    {
      gdouble value = adjustment->value;
      
      switch (scroll)
	{
	case GTK_SCROLL_STEP_FORWARD:
	  value += adjustment->step_increment;
	  break;
	case GTK_SCROLL_STEP_BACKWARD:
	  value -= adjustment->step_increment;
	  break;
	case GTK_SCROLL_PAGE_FORWARD:
	  value += adjustment->page_increment;
	  break;
	case GTK_SCROLL_PAGE_BACKWARD:
	  value -= adjustment->page_increment;
	  break;
	case GTK_SCROLL_START:
	  value = adjustment->lower;
	  break;
	case GTK_SCROLL_END:
	  value = adjustment->upper;
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}

      value = CLAMP (value, adjustment->lower, adjustment->upper - adjustment->page_size);
      
      gtk_adjustment_set_value (adjustment, value);
    }
}

static void
gtk_scrolled_window_move_focus_out (GtkScrolledWindow *scrolled_window,
				    GtkDirectionType   direction_type)
{
  GtkWidget *toplevel;
  
  /* Focus out of the scrolled window entirely. We do this by setting
   * a flag, then propagating the focus motion to the notebook.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (scrolled_window));
  if (!GTK_WIDGET_TOPLEVEL (toplevel))
    return;

  g_object_ref (scrolled_window);
  
  scrolled_window->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move_focus", direction_type);
  scrolled_window->focus_out = FALSE;
  
  g_object_unref (scrolled_window);
}

static void
gtk_scrolled_window_size_request (GtkWidget      *widget,
				  GtkRequisition *requisition)
{
  GtkScrolledWindow *scrolled_window;
  GtkBin *bin;
  gint extra_width;
  gint extra_height;
  gint scrollbar_spacing;
  GtkRequisition hscrollbar_requisition;
  GtkRequisition vscrollbar_requisition;
  GtkRequisition child_requisition;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  bin = GTK_BIN (scrolled_window);

  scrollbar_spacing = _gtk_scrolled_window_get_scrollbar_spacing (scrolled_window);

  extra_width = 0;
  extra_height = 0;
  requisition->width = 0;
  requisition->height = 0;
  
  gtk_widget_size_request (scrolled_window->hscrollbar,
			   &hscrollbar_requisition);
  gtk_widget_size_request (scrolled_window->vscrollbar,
			   &vscrollbar_requisition);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      if (scrolled_window->hscrollbar_policy == GTK_POLICY_NEVER)
	requisition->width += child_requisition.width;
      else
	{
	  GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (bin->child, FALSE);

	  if (aux_info && aux_info->width > 0)
	    {
	      requisition->width += aux_info->width;
	      extra_width = -1;
	    }
	  else
	    requisition->width += vscrollbar_requisition.width;
	}

//      if (scrolled_window->vscrollbar_policy == GTK_POLICY_NEVER)
//	requisition->height += child_requisition.height;
//      else
	{
	  GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (bin->child, FALSE);

	  if (aux_info && aux_info->height > 0)
	    {
	      requisition->height += aux_info->height;
	      extra_height = -1;
	    }
	  else
	    requisition->height += hscrollbar_requisition.height;
	}
    }

  if (scrolled_window->hscrollbar_policy == GTK_POLICY_AUTOMATIC ||
      scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
    {
      requisition->width = MAX (requisition->width, hscrollbar_requisition.width);
      if (!extra_height || scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
	extra_height = scrollbar_spacing + hscrollbar_requisition.height;
    }

  if (scrolled_window->vscrollbar_policy == GTK_POLICY_AUTOMATIC ||
      scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
    {
      requisition->height = MAX (requisition->height, vscrollbar_requisition.height);
      if (!extra_height || scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
	extra_width = scrollbar_spacing + vscrollbar_requisition.width;
    }

  requisition->width += GTK_CONTAINER (widget)->border_width * 2 + MAX (0, extra_width);
  requisition->height += GTK_CONTAINER (widget)->border_width * 2 + MAX (0, extra_height);

  if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
    {
      requisition->width += 2 * widget->style->xthickness;
      requisition->height += 2 * widget->style->ythickness;
    }
}

static void
gtk_scrolled_window_relative_allocation (GtkWidget     *widget,
					 GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;
  gint scrollbar_spacing;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  scrollbar_spacing = _gtk_scrolled_window_get_scrollbar_spacing (scrolled_window);

  allocation->x = GTK_CONTAINER (widget)->border_width;
  allocation->y = GTK_CONTAINER (widget)->border_width;

  if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
    {
      allocation->x += widget->style->xthickness;
      allocation->y += widget->style->ythickness;
    }
  
  allocation->width = MAX (1, (gint)widget->allocation.width - allocation->x * 2);
  allocation->height = MAX (1, (gint)widget->allocation.height - allocation->y * 2);

  if (scrolled_window->vscrollbar_visible)
    {
      GtkRequisition vscrollbar_requisition;
      gboolean is_rtl;

      gtk_widget_get_child_requisition (scrolled_window->vscrollbar,
					&vscrollbar_requisition);
      is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  
      if ((!is_rtl && 
	   (scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT ||
	    scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
	  (is_rtl && 
	   (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	    scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT)))
	allocation->x += (vscrollbar_requisition.width +  scrollbar_spacing);

      allocation->width = MAX (1, allocation->width - (vscrollbar_requisition.width + scrollbar_spacing));
    }
  if (scrolled_window->hscrollbar_visible)
    {
      GtkRequisition hscrollbar_requisition;
      gtk_widget_get_child_requisition (scrolled_window->hscrollbar,
					&hscrollbar_requisition);
  
      if (scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)
	allocation->y += (hscrollbar_requisition.height + scrollbar_spacing);

      allocation->height = MAX (1, allocation->height - (hscrollbar_requisition.height + scrollbar_spacing));
    }
}

static void
gtk_scrolled_window_size_allocate (GtkWidget     *widget,
				   GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;
  GtkBin *bin;
  GtkAllocation relative_allocation;
  GtkAllocation child_allocation;
  gint scrollbar_spacing;
  
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  bin = GTK_BIN (scrolled_window);

  scrollbar_spacing = _gtk_scrolled_window_get_scrollbar_spacing (scrolled_window);

  widget->allocation = *allocation;

  if (scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->hscrollbar_visible = TRUE;
  else if (scrolled_window->hscrollbar_policy == GTK_POLICY_NEVER)
    scrolled_window->hscrollbar_visible = FALSE;
  if (scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->vscrollbar_visible = TRUE;
  else if (scrolled_window->vscrollbar_policy == GTK_POLICY_NEVER)
    scrolled_window->vscrollbar_visible = FALSE;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gboolean previous_hvis;
      gboolean previous_vvis;
      guint count = 0;
      
      do
	{
	  gtk_scrolled_window_relative_allocation (widget, &relative_allocation);
	  
	  child_allocation.x = relative_allocation.x + allocation->x;
	  child_allocation.y = relative_allocation.y + allocation->y;
	  child_allocation.width = relative_allocation.width;
	  child_allocation.height = relative_allocation.height;
	  
	  previous_hvis = scrolled_window->hscrollbar_visible;
	  previous_vvis = scrolled_window->vscrollbar_visible;
	  
	  gtk_widget_size_allocate (bin->child, &child_allocation);

	  /* If, after the first iteration, the hscrollbar and the
	   * vscrollbar flip visiblity, then we need both.
	   */
	  if (count &&
	      previous_hvis != scrolled_window->hscrollbar_visible &&
	      previous_vvis != scrolled_window->vscrollbar_visible)
	    {
	      scrolled_window->hscrollbar_visible = TRUE;
	      scrolled_window->vscrollbar_visible = TRUE;

	      /* a new resize is already queued at this point,
	       * so we will immediatedly get reinvoked
	       */
	      return;
	    }
	  
	  count++;
	}
      while (previous_hvis != scrolled_window->hscrollbar_visible ||
	     previous_vvis != scrolled_window->vscrollbar_visible);
    }
  else
    gtk_scrolled_window_relative_allocation (widget, &relative_allocation);
  
  if (scrolled_window->hscrollbar_visible)
    {
      GtkRequisition hscrollbar_requisition;
      gtk_widget_get_child_requisition (scrolled_window->hscrollbar,
					&hscrollbar_requisition);
  
      if (!GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
	gtk_widget_show (scrolled_window->hscrollbar);

      child_allocation.x = relative_allocation.x;
      if (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT)
	child_allocation.y = (relative_allocation.y +
			      relative_allocation.height +
			      scrollbar_spacing +
			      (scrolled_window->shadow_type == GTK_SHADOW_NONE ?
			       0 : widget->style->ythickness));
      else
	child_allocation.y = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.width = relative_allocation.width;
      child_allocation.height = hscrollbar_requisition.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
	{
	  child_allocation.x -= widget->style->xthickness;
	  child_allocation.width += 2 * widget->style->xthickness;
	}

      gtk_widget_size_allocate (scrolled_window->hscrollbar, &child_allocation);
    }
  else if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    gtk_widget_hide (scrolled_window->hscrollbar);

  if (scrolled_window->vscrollbar_visible)
    {
      GtkRequisition vscrollbar_requisition;
      if (!GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
	gtk_widget_show (scrolled_window->vscrollbar);

      gtk_widget_get_child_requisition (scrolled_window->vscrollbar,
					&vscrollbar_requisition);

      if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL && 
	   (scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT ||
	    scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
	  (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR && 
	   (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	    scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT)))
	child_allocation.x = (relative_allocation.x +
			      relative_allocation.width +
			      scrollbar_spacing +
			      (scrolled_window->shadow_type == GTK_SHADOW_NONE ?
			       0 : widget->style->xthickness));
      else
	child_allocation.x = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.y = relative_allocation.y;
      child_allocation.width = vscrollbar_requisition.width;
      child_allocation.height = relative_allocation.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      if (scrolled_window->shadow_type != GTK_SHADOW_NONE)
	{
	  child_allocation.y -= widget->style->ythickness;
	  child_allocation.height += 2 * widget->style->ythickness;
	}

      gtk_widget_size_allocate (scrolled_window->vscrollbar, &child_allocation);
    }
  else if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    gtk_widget_hide (scrolled_window->vscrollbar);
}

static gint
gtk_scrolled_window_scroll_event (GtkWidget *widget,
				  GdkEventScroll *event)
{
  GtkWidget *range;

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);  

  if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN)
    range = GTK_SCROLLED_WINDOW (widget)->vscrollbar;
  else
    range = GTK_SCROLLED_WINDOW (widget)->hscrollbar;

  if (range && GTK_WIDGET_VISIBLE (range))
    {
      GtkAdjustment *adj = GTK_RANGE (range)->adjustment;
      gdouble delta, new_value;

      delta = _gtk_range_get_wheel_delta (GTK_RANGE (range), event->direction);

      new_value = CLAMP (adj->value + delta, adj->lower, adj->upper - adj->page_size);
      
      gtk_adjustment_set_value (adj, new_value);

      return TRUE;
    }

  return FALSE;
}

static gint
gtk_scrolled_window_focus (GtkWidget        *widget,
			   GtkDirectionType  direction)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  gboolean had_focus_child = GTK_CONTAINER (widget)->focus_child != NULL;
  
  if (scrolled_window->focus_out)
    {
      scrolled_window->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }
  
  if (gtk_widget_is_focus (widget))
    return FALSE;

  /* We only put the scrolled window itself in the focus chain if it
   * isn't possible to focus any children.
   */
  if (GTK_BIN (widget)->child)
    {
      if (gtk_widget_child_focus (GTK_BIN (widget)->child, direction))
	return TRUE;
    }

  if (!had_focus_child)
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
					gpointer       data)
{
  GtkScrolledWindow *scrolled_win;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  scrolled_win = GTK_SCROLLED_WINDOW (data);

  if (scrolled_win->hscrollbar &&
      adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->hscrollbar)))
    {
      if (scrolled_win->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  gboolean visible;
	  
	  visible = scrolled_win->hscrollbar_visible;
	  scrolled_win->hscrollbar_visible = (adjustment->upper - adjustment->lower >
					      adjustment->page_size);
	  if (scrolled_win->hscrollbar_visible != visible)
	    gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
	}
    }
  else if (scrolled_win->vscrollbar &&
	   adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->vscrollbar)))
    {
      if (scrolled_win->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  gboolean visible;

	  visible = scrolled_win->vscrollbar_visible;
	  scrolled_win->vscrollbar_visible = (adjustment->upper - adjustment->lower >
					      adjustment->page_size);
	  if (scrolled_win->vscrollbar_visible != visible)
	    gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
	}
    }
}

static void
gtk_scrolled_window_add (GtkContainer *container,
			 GtkWidget    *child)
{
  GtkScrolledWindow *scrolled_window;
  GtkBin *bin;

  bin = GTK_BIN (container);
  g_return_if_fail (bin->child == NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);

  bin->child = child;
  gtk_widget_set_parent (child, GTK_WIDGET (bin));

  /* this is a temporary message */
  if (!gtk_widget_set_scroll_adjustments (child,
					  gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
					  gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar))))
    g_warning ("gtk_scrolled_window_add(): cannot add non scrollable widget "
	       "use gtk_scrolled_window_add_with_viewport() instead");
}

static void
gtk_scrolled_window_remove (GtkContainer *container,
			    GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_BIN (container)->child == child);
  
  gtk_widget_set_scroll_adjustments (child, NULL, NULL);

  /* chain parent class handler to remove child */
  GTK_CONTAINER_CLASS (parent_class)->remove (container, child);
}

void
gtk_scrolled_window_add_with_viewport (GtkScrolledWindow *scrolled_window,
				       GtkWidget         *child)
{
  GtkBin *bin;
  GtkWidget *viewport;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == NULL);

  bin = GTK_BIN (scrolled_window);

  if (bin->child != NULL)
    {
      g_return_if_fail (GTK_IS_VIEWPORT (bin->child));
      g_return_if_fail (GTK_BIN (bin->child)->child == NULL);

      viewport = bin->child;
    }
  else
    {
      viewport =
        gtk_viewport_new (gtk_scrolled_window_get_hadjustment (scrolled_window),
			  gtk_scrolled_window_get_vadjustment (scrolled_window));
      gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);
    }

  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), child);
}

/**
 * _gtk_scrolled_window_get_spacing:
 * @scrolled_window: a scrolled window
 * 
 * Gets the spacing between the scrolled window's scrollbars and
 * the scrolled widget. Used by GtkCombo
 * 
 * Return value: the spacing, in pixels.
 **/
gint
_gtk_scrolled_window_get_scrollbar_spacing (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowClass *class;
    
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), 0);

  class = GTK_SCROLLED_WINDOW_GET_CLASS (scrolled_window);

  if (class->scrollbar_spacing >= 0)
    return class->scrollbar_spacing;
  else
    {
      gint scrollbar_spacing;
      
      gtk_widget_style_get (GTK_WIDGET (scrolled_window),
			    "scrollbar_spacing", &scrollbar_spacing,
			    NULL);

      return scrollbar_spacing;
    }
}

#define __GTK_SCROLLED_WINDOW_C__
#include "gtkaliasdef.c"
