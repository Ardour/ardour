/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
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
#include <stdlib.h>
#include "gtkhandlebox.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

typedef struct _GtkHandleBoxPrivate GtkHandleBoxPrivate;

struct _GtkHandleBoxPrivate
{
  gint orig_x;
  gint orig_y;
};

enum {
  PROP_0,
  PROP_SHADOW,
  PROP_SHADOW_TYPE,
  PROP_HANDLE_POSITION,
  PROP_SNAP_EDGE,
  PROP_SNAP_EDGE_SET,
  PROP_CHILD_DETACHED
};

#define DRAG_HANDLE_SIZE 10
#define CHILDLESS_SIZE	25
#define GHOST_HEIGHT 3
#define TOLERANCE 5

enum {
  SIGNAL_CHILD_ATTACHED,
  SIGNAL_CHILD_DETACHED,
  SIGNAL_LAST
};

/* The algorithm for docking and redocking implemented here
 * has a couple of nice properties:
 *
 * 1) During a single drag, docking always occurs at the
 *    the same cursor position. This means that the users
 *    motions are reversible, and that you won't
 *    undock/dock oscillations.
 *
 * 2) Docking generally occurs at user-visible features.
 *    The user, once they figure out to redock, will
 *    have useful information about doing it again in
 *    the future.
 *
 * Please try to preserve these properties if you
 * change the algorithm. (And the current algorithm
 * is far from ideal). Briefly, the current algorithm
 * for deciding whether the handlebox is docked or not:
 *
 * 1) The decision is done by comparing two rectangles - the
 *    allocation if the widget at the start of the drag,
 *    and the boundary of hb->bin_window at the start of
 *    of the drag offset by the distance that the cursor
 *    has moved.
 *
 * 2) These rectangles must have one edge, the "snap_edge"
 *    of the handlebox, aligned within TOLERANCE.
 * 
 * 3) On the other dimension, the extents of one rectangle
 *    must be contained in the extents of the other,
 *    extended by tolerance. That is, either we can have:
 *
 * <-TOLERANCE-|--------bin_window--------------|-TOLERANCE->
 *         <--------float_window-------------------->
 *
 * or we can have:
 *
 * <-TOLERANCE-|------float_window--------------|-TOLERANCE->
 *          <--------bin_window-------------------->
 */

static void     gtk_handle_box_set_property  (GObject        *object,
                                              guint           param_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void     gtk_handle_box_get_property  (GObject        *object,
                                              guint           param_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);
static void     gtk_handle_box_map           (GtkWidget      *widget);
static void     gtk_handle_box_unmap         (GtkWidget      *widget);
static void     gtk_handle_box_realize       (GtkWidget      *widget);
static void     gtk_handle_box_unrealize     (GtkWidget      *widget);
static void     gtk_handle_box_style_set     (GtkWidget      *widget,
                                              GtkStyle       *previous_style);
static void     gtk_handle_box_size_request  (GtkWidget      *widget,
                                              GtkRequisition *requisition);
static void     gtk_handle_box_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *real_allocation);
static void     gtk_handle_box_add           (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_remove        (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_draw_ghost    (GtkHandleBox   *hb);
static void     gtk_handle_box_paint         (GtkWidget      *widget,
                                              GdkEventExpose *event,
                                              GdkRectangle   *area);
static gboolean gtk_handle_box_expose        (GtkWidget      *widget,
                                              GdkEventExpose *event);
static gboolean gtk_handle_box_button_press  (GtkWidget      *widget,
                                              GdkEventButton *event);
static gboolean gtk_handle_box_motion        (GtkWidget      *widget,
                                              GdkEventMotion *event);
static gboolean gtk_handle_box_delete_event  (GtkWidget      *widget,
                                              GdkEventAny    *event);
static void     gtk_handle_box_reattach      (GtkHandleBox   *hb);
static void     gtk_handle_box_end_drag      (GtkHandleBox   *hb,
                                              guint32         time);

static guint handle_box_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GtkHandleBox, gtk_handle_box, GTK_TYPE_BIN)

static void
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  gobject_class->set_property = gtk_handle_box_set_property;
  gobject_class->get_property = gtk_handle_box_get_property;
  
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW,
                                   g_param_spec_enum ("shadow", NULL,
                                                      P_("Deprecated property, use shadow_type instead"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Shadow type"),
                                                      P_("Appearance of the shadow that surrounds the container"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_HANDLE_POSITION,
                                   g_param_spec_enum ("handle-position",
                                                      P_("Handle position"),
                                                      P_("Position of the handle relative to the child widget"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_LEFT,
                                                      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_EDGE,
                                   g_param_spec_enum ("snap-edge",
                                                      P_("Snap edge"),
                                                      P_("Side of the handlebox that's lined up with the docking point to dock the handlebox"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_TOP,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_EDGE_SET,
                                   g_param_spec_boolean ("snap-edge-set",
							 P_("Snap edge set"),
							 P_("Whether to use the value from the snap_edge property or a value derived from handle_position"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CHILD_DETACHED,
                                   g_param_spec_boolean ("child-detached",
							 P_("Child Detached"),
							 P_("A boolean value indicating whether the handlebox's child is attached or detached."),
							 FALSE,
							 GTK_PARAM_READABLE));

  widget_class->map = gtk_handle_box_map;
  widget_class->unmap = gtk_handle_box_unmap;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->unrealize = gtk_handle_box_unrealize;
  widget_class->style_set = gtk_handle_box_style_set;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_press;
  widget_class->delete_event = gtk_handle_box_delete_event;

  container_class->add = gtk_handle_box_add;
  container_class->remove = gtk_handle_box_remove;

  class->child_attached = NULL;
  class->child_detached = NULL;

  handle_box_signals[SIGNAL_CHILD_ATTACHED] =
    g_signal_new (I_("child-attached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_attached),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  handle_box_signals[SIGNAL_CHILD_DETACHED] =
    g_signal_new (I_("child-detached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_detached),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  g_type_class_add_private (gobject_class, sizeof (GtkHandleBoxPrivate));    
}

static GtkHandleBoxPrivate *
gtk_handle_box_get_private (GtkHandleBox *hb)
{
  return G_TYPE_INSTANCE_GET_PRIVATE (hb, GTK_TYPE_HANDLE_BOX, GtkHandleBoxPrivate);
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  gtk_widget_set_has_window (GTK_WIDGET (handle_box), TRUE);

  handle_box->bin_window = NULL;
  handle_box->float_window = NULL;
  handle_box->shadow_type = GTK_SHADOW_OUT;
  handle_box->handle_position = GTK_POS_LEFT;
  handle_box->float_window_mapped = FALSE;
  handle_box->child_detached = FALSE;
  handle_box->in_drag = FALSE;
  handle_box->shrink_on_detach = TRUE;
  handle_box->snap_edge = -1;
}

static void 
gtk_handle_box_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);

  switch (prop_id)
    {
    case PROP_SHADOW:
    case PROP_SHADOW_TYPE:
      gtk_handle_box_set_shadow_type (handle_box, g_value_get_enum (value));
      break;
    case PROP_HANDLE_POSITION:
      gtk_handle_box_set_handle_position (handle_box, g_value_get_enum (value));
      break;
    case PROP_SNAP_EDGE:
      gtk_handle_box_set_snap_edge (handle_box, g_value_get_enum (value));
      break;
    case PROP_SNAP_EDGE_SET:
      if (!g_value_get_boolean (value))
	gtk_handle_box_set_snap_edge (handle_box, (GtkPositionType)-1);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_handle_box_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);
  
  switch (prop_id)
    {
    case PROP_SHADOW:
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, handle_box->shadow_type);
      break;
    case PROP_HANDLE_POSITION:
      g_value_set_enum (value, handle_box->handle_position);
      break;
    case PROP_SNAP_EDGE:
      g_value_set_enum (value,
			(handle_box->snap_edge == -1 ?
			 GTK_POS_TOP : handle_box->snap_edge));
      break;
    case PROP_SNAP_EDGE_SET:
      g_value_set_boolean (value, handle_box->snap_edge != -1);
      break;
    case PROP_CHILD_DETACHED:
      g_value_set_boolean (value, handle_box->child_detached);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
 
GtkWidget*
gtk_handle_box_new (void)
{
  return g_object_new (GTK_TYPE_HANDLE_BOX, NULL);
}

static void
gtk_handle_box_map (GtkWidget *widget)
{
  GtkBin *bin;
  GtkHandleBox *hb;

  gtk_widget_set_mapped (widget, TRUE);

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  if (bin->child &&
      gtk_widget_get_visible (bin->child) &&
      !gtk_widget_get_mapped (bin->child))
    gtk_widget_map (bin->child);

  if (hb->child_detached && !hb->float_window_mapped)
    {
      gdk_window_show (hb->float_window);
      hb->float_window_mapped = TRUE;
    }

  gdk_window_show (hb->bin_window);
  gdk_window_show (widget->window);
}

static void
gtk_handle_box_unmap (GtkWidget *widget)
{
  GtkHandleBox *hb;

  gtk_widget_set_mapped (widget, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  gdk_window_hide (widget->window);
  if (hb->float_window_mapped)
    {
      gdk_window_hide (hb->float_window);
      hb->float_window_mapped = FALSE;
    }
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkHandleBox *hb;

  hb = GTK_HANDLE_BOX (widget);

  gtk_widget_set_realized (widget, TRUE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget)
			   | GDK_EXPOSURE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON1_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK |
			   GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  hb->bin_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (hb->bin_window, widget);
  if (GTK_BIN (hb)->child)
    gtk_widget_set_parent_window (GTK_BIN (hb)->child, hb->bin_window);
  
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->requisition.width;
  attributes.height = widget->requisition.height;
  attributes.window_type = GDK_WINDOW_TOPLEVEL;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_KEY_PRESS_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_FOCUS_CHANGE_MASK |
			   GDK_STRUCTURE_MASK);
  attributes.type_hint = GDK_WINDOW_TYPE_HINT_TOOLBAR;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_TYPE_HINT;
  hb->float_window = gdk_window_new (gtk_widget_get_root_window (widget),
				     &attributes, attributes_mask);
  gdk_window_set_user_data (hb->float_window, widget);
  gdk_window_set_decorations (hb->float_window, 0);
  gdk_window_set_type_hint (hb->float_window, GDK_WINDOW_TYPE_HINT_TOOLBAR);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, gtk_widget_get_state (widget));
  gtk_style_set_background (widget->style, hb->bin_window, gtk_widget_get_state (widget));
  gtk_style_set_background (widget->style, hb->float_window, gtk_widget_get_state (widget));
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
}

static void
gtk_handle_box_unrealize (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);

  gdk_window_set_user_data (hb->bin_window, NULL);
  gdk_window_destroy (hb->bin_window);
  hb->bin_window = NULL;
  gdk_window_set_user_data (hb->float_window, NULL);
  gdk_window_destroy (hb->float_window);
  hb->float_window = NULL;

  GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->unrealize (widget);
}

static void
gtk_handle_box_style_set (GtkWidget *widget,
			  GtkStyle  *previous_style)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);

  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
    {
      gtk_style_set_background (widget->style, widget->window,
				widget->state);
      gtk_style_set_background (widget->style, hb->bin_window, widget->state);
      gtk_style_set_background (widget->style, hb->float_window, widget->state);
    }
}

static int
effective_handle_position (GtkHandleBox *hb)
{
  int handle_position;

  if (gtk_widget_get_direction (GTK_WIDGET (hb)) == GTK_TEXT_DIR_LTR)
    handle_position = hb->handle_position;
  else
    {
      switch (hb->handle_position) 
	{
	case GTK_POS_LEFT:
	  handle_position = GTK_POS_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  handle_position = GTK_POS_LEFT;
	  break;
	default:
	  handle_position = hb->handle_position;
	  break;
	}
    }

  return handle_position;
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GtkRequisition child_requisition;
  gint handle_position;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  handle_position = effective_handle_position (hb);

  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      requisition->width = DRAG_HANDLE_SIZE;
      requisition->height = 0;
    }
  else
    {
      requisition->width = 0;
      requisition->height = DRAG_HANDLE_SIZE;
    }

  /* if our child is not visible, we still request its size, since we
   * won't have any useful hint for our size otherwise.
   */
  if (bin->child)
    gtk_widget_size_request (bin->child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      

  if (hb->child_detached)
    {
      /* FIXME: This doesn't work currently */
      if (!hb->shrink_on_detach)
	{
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += child_requisition.height;
	  else
	    requisition->width += child_requisition.width;
	}
      else
	{
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += widget->style->ythickness;
	  else
	    requisition->width += widget->style->xthickness;
	}
    }
  else
    {
      requisition->width += GTK_CONTAINER (widget)->border_width * 2;
      requisition->height += GTK_CONTAINER (widget)->border_width * 2;
      
      if (bin->child)
	{
	  requisition->width += child_requisition.width;
	  requisition->height += child_requisition.height;
	}
      else
	{
	  requisition->width += CHILDLESS_SIZE;
	  requisition->height += CHILDLESS_SIZE;
	}
    }
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GtkRequisition child_requisition;
  gint handle_position;
  
  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);
  
  handle_position = effective_handle_position (hb);

  if (bin->child)
    gtk_widget_get_child_requisition (bin->child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      
      
  widget->allocation = *allocation;

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (widget->window,
			    widget->allocation.x,
			    widget->allocation.y,
			    widget->allocation.width,
			    widget->allocation.height);


  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      GtkAllocation child_allocation;
      guint border_width;

      border_width = GTK_CONTAINER (widget)->border_width;

      child_allocation.x = border_width;
      child_allocation.y = border_width;
      if (handle_position == GTK_POS_LEFT)
	child_allocation.x += DRAG_HANDLE_SIZE;
      else if (handle_position == GTK_POS_TOP)
	child_allocation.y += DRAG_HANDLE_SIZE;

      if (hb->child_detached)
	{
	  guint float_width;
	  guint float_height;
	  
	  child_allocation.width = child_requisition.width;
	  child_allocation.height = child_requisition.height;
	  
	  float_width = child_allocation.width + 2 * border_width;
	  float_height = child_allocation.height + 2 * border_width;
	  
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    float_width += DRAG_HANDLE_SIZE;
	  else
	    float_height += DRAG_HANDLE_SIZE;

	  if (gtk_widget_get_realized (widget))
	    {
	      gdk_window_resize (hb->float_window,
				 float_width,
				 float_height);
	      gdk_window_move_resize (hb->bin_window,
				      0,
				      0,
				      float_width,
				      float_height);
	    }
	}
      else
	{
	  child_allocation.width = MAX (1, (gint)widget->allocation.width - 2 * border_width);
	  child_allocation.height = MAX (1, (gint)widget->allocation.height - 2 * border_width);

	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    child_allocation.width -= DRAG_HANDLE_SIZE;
	  else
	    child_allocation.height -= DRAG_HANDLE_SIZE;
	  
	  if (gtk_widget_get_realized (widget))
	    gdk_window_move_resize (hb->bin_window,
				    0,
				    0,
				    widget->allocation.width,
				    widget->allocation.height);
	}

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
gtk_handle_box_draw_ghost (GtkHandleBox *hb)
{
  GtkWidget *widget;
  guint x;
  guint y;
  guint width;
  guint height;
  gint handle_position;

  widget = GTK_WIDGET (hb);
  
  handle_position = effective_handle_position (hb);
  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      x = handle_position == GTK_POS_LEFT ? 0 : widget->allocation.width - DRAG_HANDLE_SIZE;
      y = 0;
      width = DRAG_HANDLE_SIZE;
      height = widget->allocation.height;
    }
  else
    {
      x = 0;
      y = handle_position == GTK_POS_TOP ? 0 : widget->allocation.height - DRAG_HANDLE_SIZE;
      width = widget->allocation.width;
      height = DRAG_HANDLE_SIZE;
    }
  gtk_paint_shadow (widget->style,
		    widget->window,
		    gtk_widget_get_state (widget),
		    GTK_SHADOW_ETCHED_IN,
		    NULL, widget, "handle",
		    x,
		    y,
		    width,
		    height);
   if (handle_position == GTK_POS_LEFT ||
       handle_position == GTK_POS_RIGHT)
     gtk_paint_hline (widget->style,
		      widget->window,
		      gtk_widget_get_state (widget),
		      NULL, widget, "handlebox",
		      handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		      handle_position == GTK_POS_LEFT ? widget->allocation.width : widget->allocation.width - DRAG_HANDLE_SIZE,
		      widget->allocation.height / 2);
   else
     gtk_paint_vline (widget->style,
		      widget->window,
		      gtk_widget_get_state (widget),
		      NULL, widget, "handlebox",
		      handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		      handle_position == GTK_POS_TOP ? widget->allocation.height : widget->allocation.height - DRAG_HANDLE_SIZE,
		      widget->allocation.width / 2);
}

static void
draw_textured_frame (GtkWidget *widget, GdkWindow *window, GdkRectangle *rect, GtkShadowType shadow,
		     GdkRectangle *clip, GtkOrientation orientation)
{
   gtk_paint_handle (widget->style, window, GTK_STATE_NORMAL, shadow,
		     clip, widget, "handlebox",
		     rect->x, rect->y, rect->width, rect->height, 
		     orientation);
}

void
gtk_handle_box_set_shadow_type (GtkHandleBox  *handle_box,
				GtkShadowType  type)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if ((GtkShadowType) handle_box->shadow_type != type)
    {
      handle_box->shadow_type = type;
      g_object_notify (G_OBJECT (handle_box), "shadow-type");
      gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

/**
 * gtk_handle_box_get_shadow_type:
 * @handle_box: a #GtkHandleBox
 * 
 * Gets the type of shadow drawn around the handle box. See
 * gtk_handle_box_set_shadow_type().
 *
 * Return value: the type of shadow currently drawn around the handle box.
 **/
GtkShadowType
gtk_handle_box_get_shadow_type (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_SHADOW_ETCHED_OUT);

  return handle_box->shadow_type;
}

void        
gtk_handle_box_set_handle_position  (GtkHandleBox    *handle_box,
				     GtkPositionType  position)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if ((GtkPositionType) handle_box->handle_position != position)
    {
      handle_box->handle_position = position;
      g_object_notify (G_OBJECT (handle_box), "handle-position");
      gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

/**
 * gtk_handle_box_get_handle_position:
 * @handle_box: a #GtkHandleBox
 *
 * Gets the handle position of the handle box. See
 * gtk_handle_box_set_handle_position().
 *
 * Return value: the current handle position.
 **/
GtkPositionType
gtk_handle_box_get_handle_position (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_POS_LEFT);

  return handle_box->handle_position;
}

void        
gtk_handle_box_set_snap_edge        (GtkHandleBox    *handle_box,
				     GtkPositionType  edge)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if (handle_box->snap_edge != edge)
    {
      handle_box->snap_edge = edge;
      
      g_object_freeze_notify (G_OBJECT (handle_box));
      g_object_notify (G_OBJECT (handle_box), "snap-edge");
      g_object_notify (G_OBJECT (handle_box), "snap-edge-set");
      g_object_thaw_notify (G_OBJECT (handle_box));
    }
}

/**
 * gtk_handle_box_get_snap_edge:
 * @handle_box: a #GtkHandleBox
 * 
 * Gets the edge used for determining reattachment of the handle box. See
 * gtk_handle_box_set_snap_edge().
 *
 * Return value: the edge used for determining reattachment, or (GtkPositionType)-1 if this
 *               is determined (as per default) from the handle position. 
 **/
GtkPositionType
gtk_handle_box_get_snap_edge (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), (GtkPositionType)-1);

  return handle_box->snap_edge;
}

/**
 * gtk_handle_box_get_child_detached:
 * @handle_box: a #GtkHandleBox
 *
 * Whether the handlebox's child is currently detached.
 *
 * Return value: %TRUE if the child is currently detached, otherwise %FALSE
 *
 * Since: 2.14
 **/
gboolean
gtk_handle_box_get_child_detached (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), FALSE);

  return handle_box->child_detached;
}

static void
gtk_handle_box_paint (GtkWidget      *widget,
                      GdkEventExpose *event,
		      GdkRectangle   *area)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  gint width, height;
  GdkRectangle rect;
  GdkRectangle dest;
  gint handle_position;
  GtkOrientation handle_orientation;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  handle_position = effective_handle_position (hb);

  width = gdk_window_get_width (hb->bin_window);
  height = gdk_window_get_height (hb->bin_window);
  
  if (!event)
    gtk_paint_box (widget->style,
		   hb->bin_window,
		   gtk_widget_get_state (widget),
		   hb->shadow_type,
		   area, widget, "handlebox_bin",
		   0, 0, -1, -1);
  else
   gtk_paint_box (widget->style,
		  hb->bin_window,
		  gtk_widget_get_state (widget),
		  hb->shadow_type,
		  &event->area, widget, "handlebox_bin",
		  0, 0, -1, -1);

/* We currently draw the handle _above_ the relief of the handlebox.
 * it could also be drawn on the same level...

		 hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		 hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		 width,
		 height);*/

  switch (handle_position)
    {
    case GTK_POS_LEFT:
      rect.x = 0;
      rect.y = 0; 
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      handle_orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_POS_RIGHT:
      rect.x = width - DRAG_HANDLE_SIZE; 
      rect.y = 0;
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      handle_orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_POS_TOP:
      rect.x = 0;
      rect.y = 0; 
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      handle_orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    case GTK_POS_BOTTOM:
      rect.x = 0;
      rect.y = height - DRAG_HANDLE_SIZE;
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      handle_orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    default: 
      g_assert_not_reached ();
      break;
    }

  if (gdk_rectangle_intersect (event ? &event->area : area, &rect, &dest))
    draw_textured_frame (widget, hb->bin_window, &rect,
			 GTK_SHADOW_OUT,
			 event ? &event->area : area,
			 handle_orientation);

  if (bin->child && gtk_widget_get_visible (bin->child))
    GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->expose_event (widget, event);
}

static gboolean
gtk_handle_box_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  GtkHandleBox *hb;

  if (gtk_widget_is_drawable (widget))
    {
      hb = GTK_HANDLE_BOX (widget);

      if (event->window == widget->window)
	{
	  if (hb->child_detached)
	    gtk_handle_box_draw_ghost (hb);
	}
      else
	gtk_handle_box_paint (widget, event, NULL);
    }
  
  return FALSE;
}

static GtkWidget *
gtk_handle_box_get_invisible (void)
{
  static GtkWidget *handle_box_invisible = NULL;

  if (!handle_box_invisible)
    {
      handle_box_invisible = gtk_invisible_new ();
      gtk_widget_show (handle_box_invisible);
    }
  
  return handle_box_invisible;
}

static gboolean
gtk_handle_box_grab_event (GtkWidget    *widget,
			   GdkEvent     *event,
			   GtkHandleBox *hb)
{
  switch (event->type)
    {
    case GDK_BUTTON_RELEASE:
      if (hb->in_drag)		/* sanity check */
	{
	  gtk_handle_box_end_drag (hb, event->button.time);
	  return TRUE;
	}
      break;

    case GDK_MOTION_NOTIFY:
      return gtk_handle_box_motion (GTK_WIDGET (hb), (GdkEventMotion *)event);
      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
gtk_handle_box_button_press (GtkWidget      *widget,
                             GdkEventButton *event)
{
  GtkHandleBox *hb;
  gboolean event_handled;
  GdkCursor *fleur;
  gint handle_position;

  hb = GTK_HANDLE_BOX (widget);

  handle_position = effective_handle_position (hb);

  event_handled = FALSE;
  if ((event->button == 1) && 
      (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS))
    {
      GtkWidget *child;
      gboolean in_handle;
      
      if (event->window != hb->bin_window)
	return FALSE;

      child = GTK_BIN (hb)->child;

      if (child)
	{
	  switch (handle_position)
	    {
	    case GTK_POS_LEFT:
	      in_handle = event->x < DRAG_HANDLE_SIZE;
	      break;
	    case GTK_POS_TOP:
	      in_handle = event->y < DRAG_HANDLE_SIZE;
	      break;
	    case GTK_POS_RIGHT:
	      in_handle = event->x > 2 * GTK_CONTAINER (hb)->border_width + child->allocation.width;
	      break;
	    case GTK_POS_BOTTOM:
	      in_handle = event->y > 2 * GTK_CONTAINER (hb)->border_width + child->allocation.height;
	      break;
	    default:
	      in_handle = FALSE;
	      break;
	    }
	}
      else
	{
	  in_handle = FALSE;
	  event_handled = TRUE;
	}
      
      if (in_handle)
	{
	  if (event->type == GDK_BUTTON_PRESS) /* Start a drag */
	    {
	      GtkHandleBoxPrivate *private = gtk_handle_box_get_private (hb);
	      GtkWidget *invisible = gtk_handle_box_get_invisible ();
	      gint desk_x, desk_y;
	      gint root_x, root_y;
	      gint width, height;

              gtk_invisible_set_screen (GTK_INVISIBLE (invisible),
                                        gtk_widget_get_screen (GTK_WIDGET (hb)));
	      gdk_window_get_deskrelative_origin (hb->bin_window, &desk_x, &desk_y);
	      gdk_window_get_origin (hb->bin_window, &root_x, &root_y);
	      width = gdk_window_get_width (hb->bin_window);
	      height = gdk_window_get_height (hb->bin_window);
		  
	      private->orig_x = event->x_root;
	      private->orig_y = event->y_root;
		  
	      hb->float_allocation.x = root_x - event->x_root;
	      hb->float_allocation.y = root_y - event->y_root;
	      hb->float_allocation.width = width;
	      hb->float_allocation.height = height;
	      
	      hb->deskoff_x = desk_x - root_x;
	      hb->deskoff_y = desk_y - root_y;
	      
	      if (gdk_window_is_viewable (widget->window))
		{
		  gdk_window_get_origin (widget->window, &root_x, &root_y);
		  width = gdk_window_get_width (widget->window);
		  height = gdk_window_get_height (widget->window);
	      
		  hb->attach_allocation.x = root_x;
		  hb->attach_allocation.y = root_y;
		  hb->attach_allocation.width = width;
		  hb->attach_allocation.height = height;
		}
	      else
		{
		  hb->attach_allocation.x = -1;
		  hb->attach_allocation.y = -1;
		  hb->attach_allocation.width = 0;
		  hb->attach_allocation.height = 0;
		}
	      hb->in_drag = TRUE;
	      fleur = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						  GDK_FLEUR);
	      if (gdk_pointer_grab (invisible->window,
				    FALSE,
				    (GDK_BUTTON1_MOTION_MASK |
				     GDK_POINTER_MOTION_HINT_MASK |
				     GDK_BUTTON_RELEASE_MASK),
				    NULL,
				    fleur,
				    event->time) != 0)
		{
		  hb->in_drag = FALSE;
		}
	      else
		{
		  gtk_grab_add (invisible);
		  g_signal_connect (invisible, "event",
				    G_CALLBACK (gtk_handle_box_grab_event), hb);
		}
	      
	      gdk_cursor_unref (fleur);
	      event_handled = TRUE;
	    }
	  else if (hb->child_detached) /* Double click */
	    {
	      gtk_handle_box_reattach (hb);
	    }
	}
    }
  
  return event_handled;
}

static gboolean
gtk_handle_box_motion (GtkWidget      *widget,
		       GdkEventMotion *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  gint new_x, new_y;
  gint snap_edge;
  gboolean is_snapped = FALSE;
  gint handle_position;
  GdkGeometry geometry;
  GdkScreen *screen, *pointer_screen;

  if (!hb->in_drag)
    return FALSE;
  handle_position = effective_handle_position (hb);

  /* Calculate the attachment point on the float, if the float
   * were detached
   */
  new_x = 0;
  new_y = 0;
  screen = gtk_widget_get_screen (widget);
  gdk_display_get_pointer (gdk_screen_get_display (screen),
			   &pointer_screen, 
			   &new_x, &new_y, NULL);
  if (pointer_screen != screen)
    {
      GtkHandleBoxPrivate *private = gtk_handle_box_get_private (hb);

      new_x = private->orig_x;
      new_y = private->orig_y;
    }
  
  new_x += hb->float_allocation.x;
  new_y += hb->float_allocation.y;

  snap_edge = hb->snap_edge;
  if (snap_edge == -1)
    snap_edge = (handle_position == GTK_POS_LEFT ||
		 handle_position == GTK_POS_RIGHT) ?
      GTK_POS_TOP : GTK_POS_LEFT;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
    switch (snap_edge) 
      {
      case GTK_POS_LEFT:
	snap_edge = GTK_POS_RIGHT;
	break;
      case GTK_POS_RIGHT:
	snap_edge = GTK_POS_LEFT;
	break;
      default:
	break;
      }

  /* First, check if the snapped edge is aligned
   */
  switch (snap_edge)
    {
    case GTK_POS_TOP:
      is_snapped = abs (hb->attach_allocation.y - new_y) < TOLERANCE;
      break;
    case GTK_POS_BOTTOM:
      is_snapped = abs (hb->attach_allocation.y + (gint)hb->attach_allocation.height -
			new_y - (gint)hb->float_allocation.height) < TOLERANCE;
      break;
    case GTK_POS_LEFT:
      is_snapped = abs (hb->attach_allocation.x - new_x) < TOLERANCE;
      break;
    case GTK_POS_RIGHT:
      is_snapped = abs (hb->attach_allocation.x + (gint)hb->attach_allocation.width -
			new_x - (gint)hb->float_allocation.width) < TOLERANCE;
      break;
    }

  /* Next, check if coordinates in the other direction are sufficiently
   * aligned
   */
  if (is_snapped)
    {
      gint float_pos1 = 0;	/* Initialize to suppress warnings */
      gint float_pos2 = 0;
      gint attach_pos1 = 0;
      gint attach_pos2 = 0;
      
      switch (snap_edge)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  attach_pos1 = hb->attach_allocation.x;
	  attach_pos2 = hb->attach_allocation.x + hb->attach_allocation.width;
	  float_pos1 = new_x;
	  float_pos2 = new_x + hb->float_allocation.width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  attach_pos1 = hb->attach_allocation.y;
	  attach_pos2 = hb->attach_allocation.y + hb->attach_allocation.height;
	  float_pos1 = new_y;
	  float_pos2 = new_y + hb->float_allocation.height;
	  break;
	}

      is_snapped = ((attach_pos1 - TOLERANCE < float_pos1) && 
		    (attach_pos2 + TOLERANCE > float_pos2)) ||
	           ((float_pos1 - TOLERANCE < attach_pos1) &&
		    (float_pos2 + TOLERANCE > attach_pos2));
    }

  if (is_snapped)
    {
      if (hb->child_detached)
	{
	  hb->child_detached = FALSE;
	  gdk_window_hide (hb->float_window);
	  gdk_window_reparent (hb->bin_window, widget->window, 0, 0);
	  hb->float_window_mapped = FALSE;
	  g_signal_emit (hb,
			 handle_box_signals[SIGNAL_CHILD_ATTACHED],
			 0,
			 GTK_BIN (hb)->child);
	  
	  gtk_widget_queue_resize (widget);
	}
    }
  else
    {
      gint width, height;

      width = gdk_window_get_width (hb->float_window);
      height = gdk_window_get_height (hb->float_window);
      new_x += hb->deskoff_x;
      new_y += hb->deskoff_y;

      switch (handle_position)
	{
	case GTK_POS_LEFT:
	  new_y += ((gint)hb->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_RIGHT:
	  new_x += (gint)hb->float_allocation.width - width;
	  new_y += ((gint)hb->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_TOP:
	  new_x += ((gint)hb->float_allocation.width - width) / 2;
	  break;
	case GTK_POS_BOTTOM:
	  new_x += ((gint)hb->float_allocation.width - width) / 2;
	  new_y += (gint)hb->float_allocation.height - height;
	  break;
	}

      if (hb->child_detached)
	{
	  gdk_window_move (hb->float_window, new_x, new_y);
	  gdk_window_raise (hb->float_window);
	}
      else
	{
	  gint width;
	  gint height;
	  GtkRequisition child_requisition;

	  hb->child_detached = TRUE;

	  if (GTK_BIN (hb)->child)
	    gtk_widget_get_child_requisition (GTK_BIN (hb)->child, &child_requisition);
	  else
	    {
	      child_requisition.width = 0;
	      child_requisition.height = 0;
	    }      

	  width = child_requisition.width + 2 * GTK_CONTAINER (hb)->border_width;
	  height = child_requisition.height + 2 * GTK_CONTAINER (hb)->border_width;

	  if (handle_position == GTK_POS_LEFT || handle_position == GTK_POS_RIGHT)
	    width += DRAG_HANDLE_SIZE;
	  else
	    height += DRAG_HANDLE_SIZE;
	  
	  gdk_window_move_resize (hb->float_window, new_x, new_y, width, height);
	  gdk_window_reparent (hb->bin_window, hb->float_window, 0, 0);
	  gdk_window_set_geometry_hints (hb->float_window, &geometry, GDK_HINT_POS);
	  gdk_window_show (hb->float_window);
	  hb->float_window_mapped = TRUE;
#if	0
	  /* this extra move is necessary if we use decorations, or our
	   * window manager insists on decorations.
	   */
	  gdk_display_sync (gtk_widget_get_display (widget));
	  gdk_window_move (hb->float_window, new_x, new_y);
	  gdk_display_sync (gtk_widget_get_display (widget));
#endif	/* 0 */
	  g_signal_emit (hb,
			 handle_box_signals[SIGNAL_CHILD_DETACHED],
			 0,
			 GTK_BIN (hb)->child);
	  gtk_handle_box_draw_ghost (hb);
	  
	  gtk_widget_queue_resize (widget);
	}
    }

  return TRUE;
}

static void
gtk_handle_box_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  gtk_widget_set_parent_window (widget, GTK_HANDLE_BOX (container)->bin_window);
  GTK_CONTAINER_CLASS (gtk_handle_box_parent_class)->add (container, widget);
}

static void
gtk_handle_box_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (gtk_handle_box_parent_class)->remove (container, widget);

  gtk_handle_box_reattach (GTK_HANDLE_BOX (container));
}

static gint
gtk_handle_box_delete_event (GtkWidget *widget,
			     GdkEventAny  *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);

  if (event->window == hb->float_window)
    {
      gtk_handle_box_reattach (hb);
      
      return TRUE;
    }

  return FALSE;
}

static void
gtk_handle_box_reattach (GtkHandleBox *hb)
{
  GtkWidget *widget = GTK_WIDGET (hb);
  
  if (hb->child_detached)
    {
      hb->child_detached = FALSE;
      if (gtk_widget_get_realized (widget))
	{
	  gdk_window_hide (hb->float_window);
	  gdk_window_reparent (hb->bin_window, widget->window, 0, 0);

	  if (GTK_BIN (hb)->child)
	    g_signal_emit (hb,
			   handle_box_signals[SIGNAL_CHILD_ATTACHED],
			   0,
			   GTK_BIN (hb)->child);

	}
      hb->float_window_mapped = FALSE;
    }
  if (hb->in_drag)
    gtk_handle_box_end_drag (hb, GDK_CURRENT_TIME);

  gtk_widget_queue_resize (GTK_WIDGET (hb));
}

static void
gtk_handle_box_end_drag (GtkHandleBox *hb,
			 guint32       time)
{
  GtkWidget *invisible = gtk_handle_box_get_invisible ();
		
  hb->in_drag = FALSE;

  gtk_grab_remove (invisible);
  gdk_pointer_ungrab (time);
  g_signal_handlers_disconnect_by_func (invisible,
					G_CALLBACK (gtk_handle_box_grab_event),
					hb);
}

#define __GTK_HANDLE_BOX_C__
#include "gtkaliasdef.c"
