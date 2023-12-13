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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkarrow
 * @Short_description: Displays an arrow
 * @Title: GtkArrow
 * @See_also: gtk_paint_arrow()
 *
 * GtkArrow should be used to draw simple arrows that need to point in
 * one of the four cardinal directions (up, down, left, or right).  The
 * style of the arrow can be one of shadow in, shadow out, etched in, or
 * etched out.  Note that these directions and style types may be
 * ammended in versions of GTK+ to come.
 *
 * GtkArrow will fill any space alloted to it, but since it is inherited
 * from #GtkMisc, it can be padded and/or aligned, to fill exactly the
 * space the programmer desires.
 *
 * Arrows are created with a call to gtk_arrow_new().  The direction or
 * style of an arrow can be changed after creation by using gtk_arrow_set().
 */

#include "config.h"
#include <math.h>
#include "gtkarrow.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define MIN_ARROW_SIZE  15

enum {
  PROP_0,
  PROP_ARROW_TYPE,
  PROP_SHADOW_TYPE
};


static void     gtk_arrow_set_property (GObject        *object,
                                        guint           prop_id,
                                        const GValue   *value,
                                        GParamSpec     *pspec);
static void     gtk_arrow_get_property (GObject        *object,
                                        guint           prop_id,
                                        GValue         *value,
                                        GParamSpec     *pspec);
static gboolean gtk_arrow_expose       (GtkWidget      *widget,
                                        GdkEventExpose *event);


G_DEFINE_TYPE (GtkArrow, gtk_arrow, GTK_TYPE_MISC)


static void
gtk_arrow_class_init (GtkArrowClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_arrow_set_property;
  gobject_class->get_property = gtk_arrow_get_property;

  widget_class->expose_event = gtk_arrow_expose;

  g_object_class_install_property (gobject_class,
                                   PROP_ARROW_TYPE,
                                   g_param_spec_enum ("arrow-type",
                                                      P_("Arrow direction"),
                                                      P_("The direction the arrow should point"),
						      GTK_TYPE_ARROW_TYPE,
						      GTK_ARROW_RIGHT,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Arrow shadow"),
                                                      P_("Appearance of the shadow surrounding the arrow"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("Amount of space used up by arrow"),
                                                               0.0, 1.0, 0.7,
                                                               GTK_PARAM_READABLE));
}

static void
gtk_arrow_set_property (GObject         *object,
			guint            prop_id,
			const GValue    *value,
			GParamSpec      *pspec)
{
  GtkArrow *arrow = GTK_ARROW (object);

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      gtk_arrow_set (arrow,
		     g_value_get_enum (value),
		     arrow->shadow_type);
      break;
    case PROP_SHADOW_TYPE:
      gtk_arrow_set (arrow,
		     arrow->arrow_type,
		     g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_arrow_get_property (GObject         *object,
			guint            prop_id,
			GValue          *value,
			GParamSpec      *pspec)
{
  GtkArrow *arrow = GTK_ARROW (object);

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      g_value_set_enum (value, arrow->arrow_type);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, arrow->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_arrow_init (GtkArrow *arrow)
{
  gtk_widget_set_has_window (GTK_WIDGET (arrow), FALSE);

  GTK_WIDGET (arrow)->requisition.width = MIN_ARROW_SIZE + GTK_MISC (arrow)->xpad * 2;
  GTK_WIDGET (arrow)->requisition.height = MIN_ARROW_SIZE + GTK_MISC (arrow)->ypad * 2;

  arrow->arrow_type = GTK_ARROW_RIGHT;
  arrow->shadow_type = GTK_SHADOW_OUT;
}

/**
 * gtk_arrow_new:
 * @arrow_type: a valid #GtkArrowType.
 * @shadow_type: a valid #GtkShadowType.
 *
 * Creates a new #GtkArrow widget.
 *
 * Returns: the new #GtkArrow widget.
 */
GtkWidget*
gtk_arrow_new (GtkArrowType  arrow_type,
	       GtkShadowType shadow_type)
{
  GtkArrow *arrow;

  arrow = g_object_new (GTK_TYPE_ARROW, NULL);

  arrow->arrow_type = arrow_type;
  arrow->shadow_type = shadow_type;

  return GTK_WIDGET (arrow);
}

/**
 * gtk_arrow_set:
 * @arrow: a widget of type #GtkArrow.
 * @arrow_type: a valid #GtkArrowType.
 * @shadow_type: a valid #GtkShadowType.
 *
 * Sets the direction and style of the #GtkArrow, @arrow.
 */
void
gtk_arrow_set (GtkArrow      *arrow,
	       GtkArrowType   arrow_type,
	       GtkShadowType  shadow_type)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_ARROW (arrow));

  if (   ((GtkArrowType) arrow->arrow_type != arrow_type)
      || ((GtkShadowType) arrow->shadow_type != shadow_type))
    {
      g_object_freeze_notify (G_OBJECT (arrow));

      if ((GtkArrowType) arrow->arrow_type != arrow_type)
        {
          arrow->arrow_type = arrow_type;
          g_object_notify (G_OBJECT (arrow), "arrow-type");
        }

      if ((GtkShadowType) arrow->shadow_type != shadow_type)
        {
          arrow->shadow_type = shadow_type;
          g_object_notify (G_OBJECT (arrow), "shadow-type");
        }

      g_object_thaw_notify (G_OBJECT (arrow));

      widget = GTK_WIDGET (arrow);
      if (gtk_widget_is_drawable (widget))
	gtk_widget_queue_draw (widget);
    }
}


static gboolean
gtk_arrow_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  if (gtk_widget_is_drawable (widget))
    {
      GtkArrow *arrow = GTK_ARROW (widget);
      GtkMisc *misc = GTK_MISC (widget);
      GtkShadowType shadow_type;
      gint width, height;
      gint x, y;
      gint extent;
      gfloat xalign;
      GtkArrowType effective_arrow_type;
      gfloat arrow_scaling;

      gtk_widget_style_get (widget, "arrow-scaling", &arrow_scaling, NULL);

      width = widget->allocation.width - misc->xpad * 2;
      height = widget->allocation.height - misc->ypad * 2;
      extent = MIN (width, height) * arrow_scaling;
      effective_arrow_type = arrow->arrow_type;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	{
	  xalign = 1.0 - misc->xalign;
	  if (arrow->arrow_type == GTK_ARROW_LEFT)
	    effective_arrow_type = GTK_ARROW_RIGHT;
	  else if (arrow->arrow_type == GTK_ARROW_RIGHT)
	    effective_arrow_type = GTK_ARROW_LEFT;
	}

      x = floor (widget->allocation.x + misc->xpad
		 + ((widget->allocation.width - extent) * xalign));
      y = floor (widget->allocation.y + misc->ypad
		 + ((widget->allocation.height - extent) * misc->yalign));

      shadow_type = arrow->shadow_type;

      if (widget->state == GTK_STATE_ACTIVE)
	{
          if (shadow_type == GTK_SHADOW_IN)
            shadow_type = GTK_SHADOW_OUT;
          else if (shadow_type == GTK_SHADOW_OUT)
            shadow_type = GTK_SHADOW_IN;
          else if (shadow_type == GTK_SHADOW_ETCHED_IN)
            shadow_type = GTK_SHADOW_ETCHED_OUT;
          else if (shadow_type == GTK_SHADOW_ETCHED_OUT)
            shadow_type = GTK_SHADOW_ETCHED_IN;
	}

      gtk_paint_arrow (widget->style, widget->window,
		       widget->state, shadow_type,
		       &event->area, widget, "arrow",
		       effective_arrow_type, TRUE,
		       x, y, extent, extent);
    }

  return FALSE;
}

#define __GTK_ARROW_C__
#include "gtkaliasdef.c"
