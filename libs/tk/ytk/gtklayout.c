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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * GtkLayout: Widget for scrolling of arbitrary-sized areas.
 *
 * Copyright Owen Taylor, 1998
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include "gdkconfig.h"

#include "gtklayout.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkalias.h"

typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
  GtkWidget *widget;
  gint x;
  gint y;
};

enum {
   PROP_0,
   PROP_HADJUSTMENT,
   PROP_VADJUSTMENT,
   PROP_WIDTH,
   PROP_HEIGHT
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_X,
  CHILD_PROP_Y
};

static void gtk_layout_get_property       (GObject        *object,
                                           guint           prop_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void gtk_layout_set_property       (GObject        *object,
                                           guint           prop_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static GObject *gtk_layout_constructor    (GType                  type,
					   guint                  n_properties,
					   GObjectConstructParam *properties);
static void gtk_layout_finalize           (GObject        *object);
static void gtk_layout_realize            (GtkWidget      *widget);
static void gtk_layout_unrealize          (GtkWidget      *widget);
static void gtk_layout_map                (GtkWidget      *widget);
static void gtk_layout_size_request       (GtkWidget      *widget,
                                           GtkRequisition *requisition);
static void gtk_layout_size_allocate      (GtkWidget      *widget,
                                           GtkAllocation  *allocation);
static gint gtk_layout_expose             (GtkWidget      *widget,
                                           GdkEventExpose *event);
static void gtk_layout_add                (GtkContainer   *container,
					   GtkWidget      *widget);
static void gtk_layout_remove             (GtkContainer   *container,
                                           GtkWidget      *widget);
static void gtk_layout_forall             (GtkContainer   *container,
                                           gboolean        include_internals,
                                           GtkCallback     callback,
                                           gpointer        callback_data);
static void gtk_layout_set_adjustments    (GtkLayout      *layout,
                                           GtkAdjustment  *hadj,
                                           GtkAdjustment  *vadj);
static void gtk_layout_set_child_property (GtkContainer   *container,
                                           GtkWidget      *child,
                                           guint           property_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static void gtk_layout_get_child_property (GtkContainer   *container,
                                           GtkWidget      *child,
                                           guint           property_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void gtk_layout_allocate_child     (GtkLayout      *layout,
                                           GtkLayoutChild *child);
static void gtk_layout_adjustment_changed (GtkAdjustment  *adjustment,
                                           GtkLayout      *layout);
static void gtk_layout_style_set          (GtkWidget      *widget,
					   GtkStyle       *old_style);

static void gtk_layout_set_adjustment_upper (GtkAdjustment *adj,
					     gdouble        upper,
					     gboolean       always_emit_changed);

G_DEFINE_TYPE (GtkLayout, gtk_layout, GTK_TYPE_CONTAINER)

/* Public interface
 */
/**
 * gtk_layout_new:
 * @hadjustment: (allow-none): horizontal scroll adjustment, or %NULL
 * @vadjustment: (allow-none): vertical scroll adjustment, or %NULL
 * 
 * Creates a new #GtkLayout. Unless you have a specific adjustment
 * you'd like the layout to use for scrolling, pass %NULL for
 * @hadjustment and @vadjustment.
 * 
 * Return value: a new #GtkLayout
 **/
  
GtkWidget*    
gtk_layout_new (GtkAdjustment *hadjustment,
		GtkAdjustment *vadjustment)
{
  GtkLayout *layout;

  layout = g_object_new (GTK_TYPE_LAYOUT,
			 "hadjustment", hadjustment,
			 "vadjustment", vadjustment,
			 NULL);

  return GTK_WIDGET (layout);
}

/**
 * gtk_layout_get_bin_window:
 * @layout: a #GtkLayout
 *
 * Retrieve the bin window of the layout used for drawing operations.
 *
 * Return value: (transfer none): a #GdkWindow
 *
 * Since: 2.14
 **/
GdkWindow*
gtk_layout_get_bin_window (GtkLayout *layout)
{
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->bin_window;
}

/**
 * gtk_layout_get_hadjustment:
 * @layout: a #GtkLayout
 *
 * This function should only be called after the layout has been
 * placed in a #GtkScrolledWindow or otherwise configured for
 * scrolling. It returns the #GtkAdjustment used for communication
 * between the horizontal scrollbar and @layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 *
 * Return value: (transfer none): horizontal scroll adjustment
 **/
GtkAdjustment*
gtk_layout_get_hadjustment (GtkLayout *layout)
{
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->hadjustment;
}
/**
 * gtk_layout_get_vadjustment:
 * @layout: a #GtkLayout
 *
 * This function should only be called after the layout has been
 * placed in a #GtkScrolledWindow or otherwise configured for
 * scrolling. It returns the #GtkAdjustment used for communication
 * between the vertical scrollbar and @layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 *
 * Return value: (transfer none): vertical scroll adjustment
 **/
GtkAdjustment*
gtk_layout_get_vadjustment (GtkLayout *layout)
{
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->vadjustment;
}

static GtkAdjustment *
new_default_adjustment (void)
{
  return GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
}

static void           
gtk_layout_set_adjustments (GtkLayout     *layout,
			    GtkAdjustment *hadj,
			    GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else if (layout->hadjustment)
    hadj = new_default_adjustment ();
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else if (layout->vadjustment)
    vadj = new_default_adjustment ();
  
  if (layout->hadjustment && (layout->hadjustment != hadj))
    {
      g_signal_handlers_disconnect_by_func (layout->hadjustment,
					    gtk_layout_adjustment_changed,
					    layout);
      g_object_unref (layout->hadjustment);
    }
  
  if (layout->vadjustment && (layout->vadjustment != vadj))
    {
      g_signal_handlers_disconnect_by_func (layout->vadjustment,
					    gtk_layout_adjustment_changed,
					    layout);
      g_object_unref (layout->vadjustment);
    }
  
  if (layout->hadjustment != hadj)
    {
      layout->hadjustment = hadj;
      g_object_ref_sink (layout->hadjustment);
      gtk_layout_set_adjustment_upper (layout->hadjustment, layout->width, FALSE);
      
      g_signal_connect (layout->hadjustment, "value-changed",
			G_CALLBACK (gtk_layout_adjustment_changed),
			layout);
      need_adjust = TRUE;
    }
  
  if (layout->vadjustment != vadj)
    {
      layout->vadjustment = vadj;
      g_object_ref_sink (layout->vadjustment);
      gtk_layout_set_adjustment_upper (layout->vadjustment, layout->height, FALSE);
      
      g_signal_connect (layout->vadjustment, "value-changed",
			G_CALLBACK (gtk_layout_adjustment_changed),
			layout);
      need_adjust = TRUE;
    }

  /* vadj or hadj can be NULL while constructing; don't emit a signal
     then */
  if (need_adjust && vadj && hadj)
    gtk_layout_adjustment_changed (NULL, layout);
}

static void
gtk_layout_finalize (GObject *object)
{
  GtkLayout *layout = GTK_LAYOUT (object);

  g_object_unref (layout->hadjustment);
  g_object_unref (layout->vadjustment);

  G_OBJECT_CLASS (gtk_layout_parent_class)->finalize (object);
}

/**
 * gtk_layout_set_hadjustment:
 * @layout: a #GtkLayout
 * @adjustment: (allow-none): new scroll adjustment
 *
 * Sets the horizontal scroll adjustment for the layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 * 
 **/
void           
gtk_layout_set_hadjustment (GtkLayout     *layout,
			    GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  gtk_layout_set_adjustments (layout, adjustment, layout->vadjustment);
  g_object_notify (G_OBJECT (layout), "hadjustment");
}
 
/**
 * gtk_layout_set_vadjustment:
 * @layout: a #GtkLayout
 * @adjustment: (allow-none): new scroll adjustment
 *
 * Sets the vertical scroll adjustment for the layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 * 
 **/
void           
gtk_layout_set_vadjustment (GtkLayout     *layout,
			    GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  
  gtk_layout_set_adjustments (layout, layout->hadjustment, adjustment);
  g_object_notify (G_OBJECT (layout), "vadjustment");
}

static GtkLayoutChild*
get_child (GtkLayout  *layout,
           GtkWidget  *widget)
{
  GList *children;
  
  children = layout->children;
  while (children)
    {
      GtkLayoutChild *child;
      
      child = children->data;
      children = children->next;

      if (child->widget == widget)
        return child;
    }

  return NULL;
}

/**
 * gtk_layout_put:
 * @layout: a #GtkLayout
 * @child_widget: child widget
 * @x: X position of child widget
 * @y: Y position of child widget
 *
 * Adds @child_widget to @layout, at position (@x,@y).
 * @layout becomes the new parent container of @child_widget.
 * 
 **/
void           
gtk_layout_put (GtkLayout     *layout, 
		GtkWidget     *child_widget, 
		gint           x, 
		gint           y)
{
  GtkLayoutChild *child;

  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));
  
  child = g_new (GtkLayoutChild, 1);

  child->widget = child_widget;
  child->x = x;
  child->y = y;

  layout->children = g_list_append (layout->children, child);
  
  if (gtk_widget_get_realized (GTK_WIDGET (layout)))
    gtk_widget_set_parent_window (child->widget, layout->bin_window);

  gtk_widget_set_parent (child_widget, GTK_WIDGET (layout));
}

static void
gtk_layout_move_internal (GtkLayout       *layout,
                          GtkWidget       *widget,
                          gboolean         change_x,
                          gint             x,
                          gboolean         change_y,
                          gint             y)
{
  GtkLayoutChild *child;

  child = get_child (layout, widget);

  g_assert (child);

  gtk_widget_freeze_child_notify (widget);
  
  if (change_x)
    {
      child->x = x;
      gtk_widget_child_notify (widget, "x");
    }

  if (change_y)
    {
      child->y = y;
      gtk_widget_child_notify (widget, "y");
    }

  gtk_widget_thaw_child_notify (widget);
  
  if (gtk_widget_get_visible (widget) &&
      gtk_widget_get_visible (GTK_WIDGET (layout)))
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_layout_move:
 * @layout: a #GtkLayout
 * @child_widget: a current child of @layout
 * @x: X position to move to
 * @y: Y position to move to
 *
 * Moves a current child of @layout to a new position.
 * 
 **/
void           
gtk_layout_move (GtkLayout     *layout, 
		 GtkWidget     *child_widget, 
		 gint           x, 
		 gint           y)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));
  g_return_if_fail (child_widget->parent == GTK_WIDGET (layout));  

  gtk_layout_move_internal (layout, child_widget, TRUE, x, TRUE, y);
}

static void
gtk_layout_set_adjustment_upper (GtkAdjustment *adj,
				 gdouble        upper,
				 gboolean       always_emit_changed)
{
  gboolean changed = FALSE;
  gboolean value_changed = FALSE;
  
  gdouble min = MAX (0., upper - adj->page_size);

  if (upper != adj->upper)
    {
      adj->upper = upper;
      changed = TRUE;
    }
      
  if (adj->value > min)
    {
      adj->value = min;
      value_changed = TRUE;
    }
  
  if (changed || always_emit_changed)
    gtk_adjustment_changed (adj);
  if (value_changed)
    gtk_adjustment_value_changed (adj);
}

/**
 * gtk_layout_set_size:
 * @layout: a #GtkLayout
 * @width: width of entire scrollable area
 * @height: height of entire scrollable area
 *
 * Sets the size of the scrollable area of the layout.
 * 
 **/
void
gtk_layout_set_size (GtkLayout     *layout, 
		     guint          width,
		     guint          height)
{
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  
  widget = GTK_WIDGET (layout);
  
  g_object_freeze_notify (G_OBJECT (layout));
  if (width != layout->width)
     {
	layout->width = width;
	g_object_notify (G_OBJECT (layout), "width");
     }
  if (height != layout->height)
     {
	layout->height = height;
	g_object_notify (G_OBJECT (layout), "height");
     }
  g_object_thaw_notify (G_OBJECT (layout));

  if (layout->hadjustment)
    gtk_layout_set_adjustment_upper (layout->hadjustment, layout->width, FALSE);
  if (layout->vadjustment)
    gtk_layout_set_adjustment_upper (layout->vadjustment, layout->height, FALSE);

  if (gtk_widget_get_realized (widget))
    {
      width = MAX (width, widget->allocation.width);
      height = MAX (height, widget->allocation.height);
      gdk_window_resize (layout->bin_window, width, height);
    }
}

/**
 * gtk_layout_get_size:
 * @layout: a #GtkLayout
 * @width: (out) (allow-none): location to store the width set on
 *     @layout, or %NULL
 * @height: (out) (allow-none): location to store the height set on
 *     @layout, or %NULL
 *
 * Gets the size that has been set on the layout, and that determines
 * the total extents of the layout's scrollbar area. See
 * gtk_layout_set_size ().
 **/
void
gtk_layout_get_size (GtkLayout *layout,
		     guint     *width,
		     guint     *height)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (width)
    *width = layout->width;
  if (height)
    *height = layout->height;
}

/**
 * gtk_layout_freeze:
 * @layout: a #GtkLayout
 * 
 * This is a deprecated function, it doesn't do anything useful.
 **/
void
gtk_layout_freeze (GtkLayout *layout)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  layout->freeze_count++;
}

/**
 * gtk_layout_thaw:
 * @layout: a #GtkLayout
 * 
 * This is a deprecated function, it doesn't do anything useful.
 **/
void
gtk_layout_thaw (GtkLayout *layout)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (layout->freeze_count)
    {
      if (!(--layout->freeze_count))
	{
	  gtk_widget_queue_draw (GTK_WIDGET (layout));
	  gdk_window_process_updates (GTK_WIDGET (layout)->window, TRUE);
	}
    }

}

/* Basic Object handling procedures
 */
static void
gtk_layout_class_init (GtkLayoutClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_layout_set_property;
  gobject_class->get_property = gtk_layout_get_property;
  gobject_class->finalize = gtk_layout_finalize;
  gobject_class->constructor = gtk_layout_constructor;

  container_class->set_child_property = gtk_layout_set_child_property;
  container_class->get_child_property = gtk_layout_get_child_property;

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_X,
					      g_param_spec_int ("x",
                                                                P_("X position"),
                                                                P_("X position of child widget"),
                                                                G_MININT,
                                                                G_MAXINT,
                                                                0,
                                                                GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_Y,
					      g_param_spec_int ("y",
                                                                P_("Y position"),
                                                                P_("Y position of child widget"),
                                                                G_MININT,
                                                                G_MAXINT,
                                                                0,
                                                                GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
				   PROP_HADJUSTMENT,
				   g_param_spec_object ("hadjustment",
							P_("Horizontal adjustment"),
							P_("The GtkAdjustment for the horizontal position"),
							GTK_TYPE_ADJUSTMENT,
							GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
				   PROP_VADJUSTMENT,
				   g_param_spec_object ("vadjustment",
							P_("Vertical adjustment"),
							P_("The GtkAdjustment for the vertical position"),
							GTK_TYPE_ADJUSTMENT,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_WIDTH,
				   g_param_spec_uint ("width",
						     P_("Width"),
						     P_("The width of the layout"),
						     0,
						     G_MAXINT,
						     100,
						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HEIGHT,
				   g_param_spec_uint ("height",
						     P_("Height"),
						     P_("The height of the layout"),
						     0,
						     G_MAXINT,
						     100,
						     GTK_PARAM_READWRITE));
  widget_class->realize = gtk_layout_realize;
  widget_class->unrealize = gtk_layout_unrealize;
  widget_class->map = gtk_layout_map;
  widget_class->size_request = gtk_layout_size_request;
  widget_class->size_allocate = gtk_layout_size_allocate;
  widget_class->expose_event = gtk_layout_expose;
  widget_class->style_set = gtk_layout_style_set;

  container_class->add = gtk_layout_add;
  container_class->remove = gtk_layout_remove;
  container_class->forall = gtk_layout_forall;

  class->set_scroll_adjustments = gtk_layout_set_adjustments;

  /**
   * GtkLayout::set-scroll-adjustments
   * @horizontal: the horizontal #GtkAdjustment
   * @vertical: the vertical #GtkAdjustment
   *
   * Set the scroll adjustments for the layout. Usually scrolled containers
   * like #GtkScrolledWindow will emit this signal to connect two instances
   * of #GtkScrollbar to the scroll directions of the #GtkLayout.
   */
  widget_class->set_scroll_adjustments_signal =
    g_signal_new (I_("set-scroll-adjustments"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkLayoutClass, set_scroll_adjustments),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_ADJUSTMENT,
		  GTK_TYPE_ADJUSTMENT);
}

static void
gtk_layout_get_property (GObject     *object,
			 guint        prop_id,
			 GValue      *value,
			 GParamSpec  *pspec)
{
  GtkLayout *layout = GTK_LAYOUT (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, layout->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, layout->vadjustment);
      break;
    case PROP_WIDTH:
      g_value_set_uint (value, layout->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, layout->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_layout_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GtkLayout *layout = GTK_LAYOUT (object);
  
  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_layout_set_hadjustment (layout, 
				  (GtkAdjustment*) g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_layout_set_vadjustment (layout, 
				  (GtkAdjustment*) g_value_get_object (value));
      break;
    case PROP_WIDTH:
      gtk_layout_set_size (layout, g_value_get_uint (value),
			   layout->height);
      break;
    case PROP_HEIGHT:
      gtk_layout_set_size (layout, layout->width,
			   g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_layout_set_child_property (GtkContainer    *container,
                               GtkWidget       *child,
                               guint            property_id,
                               const GValue    *value,
                               GParamSpec      *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_X:
      gtk_layout_move_internal (GTK_LAYOUT (container),
                                child,
                                TRUE, g_value_get_int (value),
                                FALSE, 0);
      break;
    case CHILD_PROP_Y:
      gtk_layout_move_internal (GTK_LAYOUT (container),
                                child,
                                FALSE, 0,
                                TRUE, g_value_get_int (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_layout_get_child_property (GtkContainer *container,
                               GtkWidget    *child,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkLayoutChild *layout_child;

  layout_child = get_child (GTK_LAYOUT (container), child);
  
  switch (property_id)
    {
    case CHILD_PROP_X:
      g_value_set_int (value, layout_child->x);
      break;
    case CHILD_PROP_Y:
      g_value_set_int (value, layout_child->y);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_layout_init (GtkLayout *layout)
{
  layout->children = NULL;

  layout->width = 100;
  layout->height = 100;

  layout->hadjustment = NULL;
  layout->vadjustment = NULL;

  layout->bin_window = NULL;

  layout->scroll_x = 0;
  layout->scroll_y = 0;
  layout->visibility = GDK_VISIBILITY_PARTIAL;

  layout->freeze_count = 0;
}

static GObject *
gtk_layout_constructor (GType                  type,
			guint                  n_properties,
			GObjectConstructParam *properties)
{
  GtkLayout *layout;
  GObject *object;
  GtkAdjustment *hadj, *vadj;
  
  object = G_OBJECT_CLASS (gtk_layout_parent_class)->constructor (type,
								  n_properties,
								  properties);

  layout = GTK_LAYOUT (object);

  hadj = layout->hadjustment ? layout->hadjustment : new_default_adjustment ();
  vadj = layout->vadjustment ? layout->vadjustment : new_default_adjustment ();

  if (!layout->hadjustment || !layout->vadjustment)
    gtk_layout_set_adjustments (layout, hadj, vadj);

  return object;
}

/* Widget methods
 */

static void 
gtk_layout_realize (GtkWidget *widget)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GList *tmp_list;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = - layout->hadjustment->value,
  attributes.y = - layout->vadjustment->value;
  attributes.width = MAX (layout->width, widget->allocation.width);
  attributes.height = MAX (layout->height, widget->allocation.height);
  attributes.event_mask = GDK_EXPOSURE_MASK | GDK_SCROLL_MASK | 
                          gtk_widget_get_events (widget);

  layout->bin_window = gdk_window_new (widget->window,
					&attributes, attributes_mask);
  gdk_window_set_user_data (layout->bin_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, layout->bin_window, GTK_STATE_NORMAL);

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_widget_set_parent_window (child->widget, layout->bin_window);
    }
}

static void
gtk_layout_style_set (GtkWidget *widget,
                      GtkStyle  *old_style)
{
  GTK_WIDGET_CLASS (gtk_layout_parent_class)->style_set (widget, old_style);

  if (gtk_widget_get_realized (widget))
    {
      gtk_style_set_background (widget->style, GTK_LAYOUT (widget)->bin_window, GTK_STATE_NORMAL);
    }
}

static void
gtk_layout_map (GtkWidget *widget)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GList *tmp_list;

  gtk_widget_set_mapped (widget, TRUE);

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (gtk_widget_get_visible (child->widget))
	{
	  if (!gtk_widget_get_mapped (child->widget))
	    gtk_widget_map (child->widget);
	}
    }

  gdk_window_show (layout->bin_window);
  gdk_window_show (widget->window);
}

static void 
gtk_layout_unrealize (GtkWidget *widget)
{
  GtkLayout *layout = GTK_LAYOUT (widget);

  gdk_window_set_user_data (layout->bin_window, NULL);
  gdk_window_destroy (layout->bin_window);
  layout->bin_window = NULL;

  GTK_WIDGET_CLASS (gtk_layout_parent_class)->unrealize (widget);
}

static void     
gtk_layout_size_request (GtkWidget     *widget,
			 GtkRequisition *requisition)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GList *tmp_list;

  requisition->width = 0;
  requisition->height = 0;

  tmp_list = layout->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      GtkRequisition child_requisition;
      
      tmp_list = tmp_list->next;

      gtk_widget_size_request (child->widget, &child_requisition);
    }
}

static void     
gtk_layout_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GList *tmp_list;

  widget->allocation = *allocation;

  tmp_list = layout->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_layout_allocate_child (layout, child);
    }

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_resize (layout->bin_window,
			 MAX (layout->width, allocation->width),
			 MAX (layout->height, allocation->height));
    }

  layout->hadjustment->page_size = allocation->width;
  layout->hadjustment->page_increment = allocation->width * 0.9;
  layout->hadjustment->lower = 0;
  /* set_adjustment_upper() emits ::changed */
  gtk_layout_set_adjustment_upper (layout->hadjustment, MAX (allocation->width, layout->width), TRUE);

  layout->vadjustment->page_size = allocation->height;
  layout->vadjustment->page_increment = allocation->height * 0.9;
  layout->vadjustment->lower = 0;
  layout->vadjustment->upper = MAX (allocation->height, layout->height);
  gtk_layout_set_adjustment_upper (layout->vadjustment, MAX (allocation->height, layout->height), TRUE);
}

static gint 
gtk_layout_expose (GtkWidget      *widget,
                   GdkEventExpose *event)
{
  GtkLayout *layout = GTK_LAYOUT (widget);

  if (event->window != layout->bin_window)
    return FALSE;

  GTK_WIDGET_CLASS (gtk_layout_parent_class)->expose_event (widget, event);

  return FALSE;
}

/* Container methods
 */
static void
gtk_layout_add (GtkContainer *container,
		GtkWidget    *widget)
{
  gtk_layout_put (GTK_LAYOUT (container), widget, 0, 0);
}

static void
gtk_layout_remove (GtkContainer *container, 
		   GtkWidget    *widget)
{
  GtkLayout *layout = GTK_LAYOUT (container);
  GList *tmp_list;
  GtkLayoutChild *child = NULL;

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)
    {
      gtk_widget_unparent (widget);

      layout->children = g_list_remove_link (layout->children, tmp_list);
      g_list_free_1 (tmp_list);
      g_free (child);
    }
}

static void
gtk_layout_forall (GtkContainer *container,
		   gboolean      include_internals,
		   GtkCallback   callback,
		   gpointer      callback_data)
{
  GtkLayout *layout = GTK_LAYOUT (container);
  GtkLayoutChild *child;
  GList *tmp_list;

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
}

/* Operations on children
 */

static void
gtk_layout_allocate_child (GtkLayout      *layout,
			   GtkLayoutChild *child)
{
  GtkAllocation allocation;
  GtkRequisition requisition;

  allocation.x = child->x;
  allocation.y = child->y;
  gtk_widget_get_child_requisition (child->widget, &requisition);
  allocation.width = requisition.width;
  allocation.height = requisition.height;
  
  gtk_widget_size_allocate (child->widget, &allocation);
}

/* Callbacks */

static void
gtk_layout_adjustment_changed (GtkAdjustment *adjustment,
			       GtkLayout     *layout)
{
  if (layout->freeze_count)
    return;

  if (gtk_widget_get_realized (GTK_WIDGET (layout)))
    {
      gdk_window_move (layout->bin_window,
		       - layout->hadjustment->value,
		       - layout->vadjustment->value);
      
      gdk_window_process_updates (layout->bin_window, TRUE);
    }
}

#define __GTK_LAYOUT_C__
#include "gtkaliasdef.c"
