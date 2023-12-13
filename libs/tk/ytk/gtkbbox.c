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
#include "gtkbbox.h"
#include "gtkhbbox.h"
#include "gtkvbbox.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum {
  PROP_0,
  PROP_LAYOUT_STYLE
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_SECONDARY
};

static void gtk_button_box_set_property       (GObject           *object,
					       guint              prop_id,
					       const GValue      *value,
					       GParamSpec        *pspec);
static void gtk_button_box_get_property       (GObject           *object,
					       guint              prop_id,
					       GValue            *value,
					       GParamSpec        *pspec);
static void gtk_button_box_size_request       (GtkWidget         *widget,
                                               GtkRequisition    *requisition);
static void gtk_button_box_size_allocate      (GtkWidget         *widget,
                                               GtkAllocation     *allocation);
static void gtk_button_box_set_child_property (GtkContainer      *container,
					       GtkWidget         *child,
					       guint              property_id,
					       const GValue      *value,
					       GParamSpec        *pspec);
static void gtk_button_box_get_child_property (GtkContainer      *container,
					       GtkWidget         *child,
					       guint              property_id,
					       GValue            *value,
					       GParamSpec        *pspec);

#define DEFAULT_CHILD_MIN_WIDTH 85
#define DEFAULT_CHILD_MIN_HEIGHT 27
#define DEFAULT_CHILD_IPAD_X 4
#define DEFAULT_CHILD_IPAD_Y 0

G_DEFINE_ABSTRACT_TYPE (GtkButtonBox, gtk_button_box, GTK_TYPE_BOX)

static void
gtk_button_box_class_init (GtkButtonBoxClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_button_box_set_property;
  gobject_class->get_property = gtk_button_box_get_property;

  widget_class->size_request = gtk_button_box_size_request;
  widget_class->size_allocate = gtk_button_box_size_allocate;

  container_class->set_child_property = gtk_button_box_set_child_property;
  container_class->get_child_property = gtk_button_box_get_child_property;

  /* FIXME we need to override the "spacing" property on GtkBox once
   * libgobject allows that.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-min-width",
							     P_("Minimum child width"),
							     P_("Minimum width of buttons inside the box"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_MIN_WIDTH,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-min-height",
							     P_("Minimum child height"),
							     P_("Minimum height of buttons inside the box"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_MIN_HEIGHT,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-internal-pad-x",
							     P_("Child internal width padding"),
							     P_("Amount to increase child's size on either side"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_X,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-internal-pad-y",
							     P_("Child internal height padding"),
							     P_("Amount to increase child's size on the top and bottom"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_Y,
							     GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_STYLE,
                                   g_param_spec_enum ("layout-style",
                                                      P_("Layout style"),
                                                      P_("How to lay out the buttons in the box. Possible values are: default, spread, edge, start and end"),
						      GTK_TYPE_BUTTON_BOX_STYLE,
						      GTK_BUTTONBOX_DEFAULT_STYLE,
                                                      GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_SECONDARY,
					      g_param_spec_boolean ("secondary", 
								    P_("Secondary"),
								    P_("If TRUE, the child appears in a secondary group of children, suitable for, e.g., help buttons"),
								    FALSE,
								    GTK_PARAM_READWRITE));
}

static void
gtk_button_box_init (GtkButtonBox *button_box)
{
  GTK_BOX (button_box)->spacing = 0;
  button_box->child_min_width = GTK_BUTTONBOX_DEFAULT;
  button_box->child_min_height = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_x = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_y = GTK_BUTTONBOX_DEFAULT;
  button_box->layout_style = GTK_BUTTONBOX_DEFAULT_STYLE;
}

static void
gtk_button_box_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_STYLE:
      gtk_button_box_set_layout (GTK_BUTTON_BOX (object),
				 g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_STYLE:
      g_value_set_enum (value, GTK_BUTTON_BOX (object)->layout_style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_set_child_property (GtkContainer    *container,
				   GtkWidget       *child,
				   guint            property_id,
				   const GValue    *value,
				   GParamSpec      *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_SECONDARY:
      gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (container), child,
					  g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_child_property (GtkContainer *container,
				   GtkWidget    *child,
				   guint         property_id,
				   GValue       *value,
				   GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_SECONDARY:
      g_value_set_boolean (value, 
			   gtk_button_box_get_child_secondary (GTK_BUTTON_BOX (container), 
							       child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

/* set per widget values for spacing, child size and child internal padding */

void 
gtk_button_box_set_child_size (GtkButtonBox *widget, 
                               gint width, gint height)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  widget->child_min_width = width;
  widget->child_min_height = height;
}

void 
gtk_button_box_set_child_ipadding (GtkButtonBox *widget,
                                   gint ipad_x, gint ipad_y)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  widget->child_ipad_x = ipad_x;
  widget->child_ipad_y = ipad_y;
}

void
gtk_button_box_set_layout (GtkButtonBox      *widget, 
                           GtkButtonBoxStyle  layout_style)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (layout_style >= GTK_BUTTONBOX_DEFAULT_STYLE &&
		    layout_style <= GTK_BUTTONBOX_CENTER);

  if (widget->layout_style != layout_style)
    {
      widget->layout_style = layout_style;
      g_object_notify (G_OBJECT (widget), "layout-style");
      gtk_widget_queue_resize (GTK_WIDGET (widget));
    }
}


/* get per widget values for spacing, child size and child internal padding */

void 
gtk_button_box_get_child_size (GtkButtonBox *widget,
                               gint *width, gint *height)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (width != NULL);
  g_return_if_fail (height != NULL);

  *width  = widget->child_min_width;
  *height = widget->child_min_height;
}

void
gtk_button_box_get_child_ipadding (GtkButtonBox *widget,
                                   gint* ipad_x, gint *ipad_y)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (ipad_x != NULL);
  g_return_if_fail (ipad_y != NULL);

  *ipad_x = widget->child_ipad_x;
  *ipad_y = widget->child_ipad_y;
}

GtkButtonBoxStyle 
gtk_button_box_get_layout (GtkButtonBox *widget)
{
  g_return_val_if_fail (GTK_IS_BUTTON_BOX (widget), GTK_BUTTONBOX_SPREAD);
  
  return widget->layout_style;
}

/**
 * gtk_button_box_get_child_secondary:
 * @widget: a #GtkButtonBox
 * @child: a child of @widget 
 * 
 * Returns whether @child should appear in a secondary group of children.
 *
 * Return value: whether @child should appear in a secondary group of children.
 *
 * Since: 2.4
 **/
gboolean 
gtk_button_box_get_child_secondary (GtkButtonBox *widget,
				    GtkWidget    *child)
{
  GList *list;
  GtkBoxChild *child_info;

  g_return_val_if_fail (GTK_IS_BUTTON_BOX (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  child_info = NULL;
  list = GTK_BOX (widget)->children;
  while (list)
    {
      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  g_return_val_if_fail (list != NULL, FALSE);

  return child_info->is_secondary;
}

/**
 * gtk_button_box_set_child_secondary
 * @widget: a #GtkButtonBox
 * @child: a child of @widget
 * @is_secondary: if %TRUE, the @child appears in a secondary group of the
 *                button box.
 *
 * Sets whether @child should appear in a secondary group of children.
 * A typical use of a secondary child is the help button in a dialog.
 *
 * This group appears after the other children if the style
 * is %GTK_BUTTONBOX_START, %GTK_BUTTONBOX_SPREAD or
 * %GTK_BUTTONBOX_EDGE, and before the other children if the style
 * is %GTK_BUTTONBOX_END. For horizontal button boxes, the definition
 * of before/after depends on direction of the widget (see
 * gtk_widget_set_direction()). If the style is %GTK_BUTTONBOX_START
 * or %GTK_BUTTONBOX_END, then the secondary children are aligned at
 * the other end of the button box from the main children. For the
 * other styles, they appear immediately next to the main children.
 **/
void 
gtk_button_box_set_child_secondary (GtkButtonBox *widget, 
				    GtkWidget    *child,
				    gboolean      is_secondary)
{
  GList *list;
  
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (widget));

  list = GTK_BOX (widget)->children;
  while (list)
    {
      GtkBoxChild *child_info = list->data;
      if (child_info->widget == child)
	{
	  child_info->is_secondary = is_secondary;
	  break;
	}

      list = list->next;
    }

  gtk_widget_child_notify (child, "secondary");

  if (gtk_widget_get_visible (GTK_WIDGET (widget))
      && gtk_widget_get_visible (child))
    gtk_widget_queue_resize (child);
}

/* Ask children how much space they require and round up 
   to match minimum size and internal padding.
   Returns the size each single child should have. */
void
_gtk_button_box_child_requisition (GtkWidget *widget,
                                   int       *nvis_children,
				   int       *nvis_secondaries,
                                   int       *width,
                                   int       *height)
{
  GtkButtonBox *bbox;
  GtkBoxChild *child;
  GList *children;
  gint nchildren;
  gint nsecondaries;
  gint needed_width;
  gint needed_height;
  GtkRequisition child_requisition;
  gint ipad_w;
  gint ipad_h;
  gint width_default;
  gint height_default;
  gint ipad_x_default;
  gint ipad_y_default;
  
  gint child_min_width;
  gint child_min_height;
  gint ipad_x;
  gint ipad_y;
  
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  bbox = GTK_BUTTON_BOX (widget);

  gtk_widget_style_get (widget,
                        "child-min-width", &width_default,
                        "child-min-height", &height_default,
                        "child-internal-pad-x", &ipad_x_default,
                        "child-internal-pad-y", &ipad_y_default, 
			NULL);
  
  child_min_width = bbox->child_min_width   != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_width : width_default;
  child_min_height = bbox->child_min_height !=GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_height : height_default;
  ipad_x = bbox->child_ipad_x != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_x : ipad_x_default;
  ipad_y = bbox->child_ipad_y != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_y : ipad_y_default;

  nchildren = 0;
  nsecondaries = 0;
  children = GTK_BOX(bbox)->children;
  needed_width = child_min_width;
  needed_height = child_min_height;  
  ipad_w = ipad_x * 2;
  ipad_h = ipad_y * 2;
  
  while (children)
    {
      child = children->data;
      children = children->next;

      if (gtk_widget_get_visible (child->widget))
	{
	  nchildren += 1;
	  gtk_widget_size_request (child->widget, &child_requisition);

	  if (child_requisition.width + ipad_w > needed_width)
	    needed_width = child_requisition.width + ipad_w;
	  if (child_requisition.height + ipad_h > needed_height)
	    needed_height = child_requisition.height + ipad_h;
	  if (child->is_secondary)
	    nsecondaries++;
	}
    }

  if (nvis_children)
    *nvis_children = nchildren;
  if (nvis_secondaries)
    *nvis_secondaries = nsecondaries;
  if (width)
    *width = needed_width;
  if (height)
    *height = needed_height;
}

/* this is a kludge function to support the deprecated
 * gtk_[vh]button_box_set_layout_default() just in case anyone is still
 * using it (why?)
 */
static GtkButtonBoxStyle
gtk_button_box_kludge_get_layout_default (GtkButtonBox *widget)
{
  GtkOrientation orientation;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return _gtk_hbutton_box_get_layout_default ();
  else
    return _gtk_vbutton_box_get_layout_default ();
}

static void
gtk_button_box_size_request (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  GtkBox *box;
  GtkButtonBox *bbox;
  gint nvis_children;
  gint child_width;
  gint child_height;
  gint spacing;
  GtkButtonBoxStyle layout;
  GtkOrientation orientation;

  box = GTK_BOX (widget);
  bbox = GTK_BUTTON_BOX (widget);

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
  spacing = box->spacing;
  layout = bbox->layout_style != GTK_BUTTONBOX_DEFAULT_STYLE
	  ? bbox->layout_style : gtk_button_box_kludge_get_layout_default (GTK_BUTTON_BOX (widget));

  _gtk_button_box_child_requisition (widget,
                                     &nvis_children,
				     NULL,
                                     &child_width,
                                     &child_height);

  if (nvis_children == 0)
    {
      requisition->width = 0;
      requisition->height = 0;
    }
  else
    {
      switch (layout)
        {
          case GTK_BUTTONBOX_SPREAD:
            if (orientation == GTK_ORIENTATION_HORIZONTAL)
              requisition->width =
                      nvis_children*child_width + ((nvis_children+1)*spacing);
            else
              requisition->height =
                      nvis_children*child_height + ((nvis_children+1)*spacing);

            break;
          case GTK_BUTTONBOX_EDGE:
          case GTK_BUTTONBOX_START:
          case GTK_BUTTONBOX_END:
          case GTK_BUTTONBOX_CENTER:
            if (orientation == GTK_ORIENTATION_HORIZONTAL)
              requisition->width =
                      nvis_children*child_width + ((nvis_children-1)*spacing);
            else
              requisition->height =
                      nvis_children*child_height + ((nvis_children-1)*spacing);

            break;
          default:
            g_assert_not_reached ();
            break;
        }

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        requisition->height = child_height;
      else
        requisition->width = child_width;
    }

  requisition->width += GTK_CONTAINER (box)->border_width * 2;
  requisition->height += GTK_CONTAINER (box)->border_width * 2;
}

static void
gtk_button_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkBox *base_box;
  GtkButtonBox *box;
  GtkBoxChild *child;
  GList *children;
  GtkAllocation child_allocation;
  gint nvis_children;
  gint n_secondaries;
  gint child_width;
  gint child_height;
  gint x = 0;
  gint y = 0;
  gint secondary_x = 0;
  gint secondary_y = 0;
  gint width = 0;
  gint height = 0;
  gint childspace;
  gint childspacing = 0;
  GtkButtonBoxStyle layout;
  gint spacing;
  GtkOrientation orientation;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
  base_box = GTK_BOX (widget);
  box = GTK_BUTTON_BOX (widget);
  spacing = base_box->spacing;
  layout = box->layout_style != GTK_BUTTONBOX_DEFAULT_STYLE
	  ? box->layout_style : gtk_button_box_kludge_get_layout_default (GTK_BUTTON_BOX (widget));
  _gtk_button_box_child_requisition (widget,
                                     &nvis_children,
                                     &n_secondaries,
                                     &child_width,
                                     &child_height);
  widget->allocation = *allocation;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    width = allocation->width - GTK_CONTAINER (box)->border_width*2;
  else
    height = allocation->height - GTK_CONTAINER (box)->border_width*2;

  switch (layout)
    {
      case GTK_BUTTONBOX_SPREAD:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = (width - (nvis_children * child_width))
                    / (nvis_children + 1);
            x = allocation->x + GTK_CONTAINER (box)->border_width
                    + childspacing;
            secondary_x = x + ((nvis_children - n_secondaries)
                            * (child_width + childspacing));
          }
        else
          {
            childspacing = (height - (nvis_children * child_height))
                    / (nvis_children + 1);
            y = allocation->y + GTK_CONTAINER (box)->border_width
                    + childspacing;
            secondary_y = y + ((nvis_children - n_secondaries)
                            * (child_height + childspacing));
          }

        break;

      case GTK_BUTTONBOX_EDGE:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            if (nvis_children >= 2)
              {
                childspacing = (width - (nvis_children * child_width))
                      / (nvis_children - 1);
                x = allocation->x + GTK_CONTAINER (box)->border_width;
                secondary_x = x + ((nvis_children - n_secondaries)
                                   * (child_width + childspacing));
              }
            else
              {
                /* one or zero children, just center */
                childspacing = width;
                x = secondary_x = allocation->x
                      + (allocation->width - child_width) / 2;
              }
          }
        else
          {
            if (nvis_children >= 2)
              {
                childspacing = (height - (nvis_children*child_height))
                        / (nvis_children-1);
                y = allocation->y + GTK_CONTAINER (box)->border_width;
                secondary_y = y + ((nvis_children - n_secondaries)
                                * (child_height + childspacing));
              }
            else
              {
                /* one or zero children, just center */
                childspacing = height;
                y = secondary_y = allocation->y
                        + (allocation->height - child_height) / 2;
              }
          }

        break;

      case GTK_BUTTONBOX_START:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = spacing;
            x = allocation->x + GTK_CONTAINER (box)->border_width;
            secondary_x = allocation->x + allocation->width
              - child_width * n_secondaries
              - spacing * (n_secondaries - 1)
              - GTK_CONTAINER (box)->border_width;
          }
        else
          {
            childspacing = spacing;
            y = allocation->y + GTK_CONTAINER (box)->border_width;
            secondary_y = allocation->y + allocation->height
              - child_height * n_secondaries
              - spacing * (n_secondaries - 1)
              - GTK_CONTAINER (box)->border_width;
          }

        break;

      case GTK_BUTTONBOX_END:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = spacing;
            x = allocation->x + allocation->width
              - child_width * (nvis_children - n_secondaries)
              - spacing * (nvis_children - n_secondaries - 1)
              - GTK_CONTAINER (box)->border_width;
            secondary_x = allocation->x + GTK_CONTAINER (box)->border_width;
          }
        else
          {
            childspacing = spacing;
            y = allocation->y + allocation->height
              - child_height * (nvis_children - n_secondaries)
              - spacing * (nvis_children - n_secondaries - 1)
              - GTK_CONTAINER (box)->border_width;
            secondary_y = allocation->y + GTK_CONTAINER (box)->border_width;
          }

        break;

      case GTK_BUTTONBOX_CENTER:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = spacing;
            x = allocation->x +
              (allocation->width
               - (child_width * (nvis_children - n_secondaries)
               + spacing * (nvis_children - n_secondaries - 1))) / 2
              + (n_secondaries * child_width + n_secondaries * spacing) / 2;
            secondary_x = allocation->x + GTK_CONTAINER (box)->border_width;
          }
        else
          {
            childspacing = spacing;
            y = allocation->y +
              (allocation->height
               - (child_height * (nvis_children - n_secondaries)
                  + spacing * (nvis_children - n_secondaries - 1))) / 2
              + (n_secondaries * child_height + n_secondaries * spacing) / 2;
            secondary_y = allocation->y + GTK_CONTAINER (box)->border_width;
          }

        break;

      default:
        g_assert_not_reached ();
        break;
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
      {
        y = allocation->y + (allocation->height - child_height) / 2;
        childspace = child_width + childspacing;
      }
    else
      {
        x = allocation->x + (allocation->width - child_width) / 2;
        childspace = child_height + childspacing;
      }

  children = GTK_BOX (box)->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (gtk_widget_get_visible (child->widget))
        {
          child_allocation.width = child_width;
          child_allocation.height = child_height;

          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              child_allocation.y = y;

              if (child->is_secondary)
                {
                  child_allocation.x = secondary_x;
                  secondary_x += childspace;
                }
              else
                {
                  child_allocation.x = x;
                  x += childspace;
                }

              if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
                  child_allocation.x = (allocation->x + allocation->width)
                          - (child_allocation.x + child_width - allocation->x);
            }
          else
            {
              child_allocation.x = x;

              if (child->is_secondary)
                {
                  child_allocation.y = secondary_y;
                  secondary_y += childspace;
                }
              else
                {
                  child_allocation.y = y;
                  y += childspace;
                }
            }

          gtk_widget_size_allocate (child->widget, &child_allocation);
        }
    }
}

#define __GTK_BUTTON_BOX_C__
#include "gtkaliasdef.c"
