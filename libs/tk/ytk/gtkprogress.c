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
#include <math.h>
#include <string.h>

#undef GDK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#define __GTK_PROGRESS_C__

#include "gtkprogress.h" 
#include "gtkprivate.h" 
#include "gtkintl.h"
#include "gtkalias.h"

#define EPSILON  1e-5
#define DEFAULT_FORMAT "%P %%"

enum {
  PROP_0,
  PROP_ACTIVITY_MODE,
  PROP_SHOW_TEXT,
  PROP_TEXT_XALIGN,
  PROP_TEXT_YALIGN
};


static void gtk_progress_set_property    (GObject          *object,
					  guint             prop_id,
					  const GValue     *value,
					  GParamSpec       *pspec);
static void gtk_progress_get_property    (GObject          *object,
					  guint             prop_id,
					  GValue           *value,
					  GParamSpec       *pspec);
static void gtk_progress_destroy         (GtkObject        *object);
static void gtk_progress_finalize        (GObject          *object);
static void gtk_progress_realize         (GtkWidget        *widget);
static gboolean gtk_progress_expose      (GtkWidget        *widget,
				 	  GdkEventExpose   *event);
static void gtk_progress_size_allocate   (GtkWidget        *widget,
				 	  GtkAllocation    *allocation);
static void gtk_progress_create_pixmap   (GtkProgress      *progress);
static void gtk_progress_value_changed   (GtkAdjustment    *adjustment,
					  GtkProgress      *progress);
static void gtk_progress_changed         (GtkAdjustment    *adjustment,
					  GtkProgress      *progress);

G_DEFINE_ABSTRACT_TYPE (GtkProgress, gtk_progress, GTK_TYPE_WIDGET)

static void
gtk_progress_class_init (GtkProgressClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  gobject_class->finalize = gtk_progress_finalize;

  gobject_class->set_property = gtk_progress_set_property;
  gobject_class->get_property = gtk_progress_get_property;
  object_class->destroy = gtk_progress_destroy;

  widget_class->realize = gtk_progress_realize;
  widget_class->expose_event = gtk_progress_expose;
  widget_class->size_allocate = gtk_progress_size_allocate;

  /* to be overridden */
  class->paint = NULL;
  class->update = NULL;
  class->act_mode_enter = NULL;
  
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVITY_MODE,
                                   g_param_spec_boolean ("activity-mode",
							 P_("Activity mode"),
							 P_("If TRUE, the GtkProgress is in activity mode, meaning that it signals "
                                                            "something is happening, but not how much of the activity is finished. "
                                                            "This is used when you're doing something but don't know how long it will take."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_TEXT,
                                   g_param_spec_boolean ("show-text",
							 P_("Show text"),
							 P_("Whether the progress is shown as text."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TEXT_XALIGN,
				   g_param_spec_float ("text-xalign",
						       P_("Text x alignment"),
                                                       P_("The horizontal text alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
						       0.0, 1.0, 0.5,
						       GTK_PARAM_READWRITE));  
  g_object_class_install_property (gobject_class,
				   PROP_TEXT_YALIGN,
				   g_param_spec_float ("text-yalign",
						       P_("Text y alignment"),
                                                       P_("The vertical text alignment, from 0 (top) to 1 (bottom)."),
						       0.0, 1.0, 0.5,
						       GTK_PARAM_READWRITE));
}

static void
gtk_progress_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  GtkProgress *progress;
  
  progress = GTK_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_ACTIVITY_MODE:
      gtk_progress_set_activity_mode (progress, g_value_get_boolean (value));
      break;
    case PROP_SHOW_TEXT:
      gtk_progress_set_show_text (progress, g_value_get_boolean (value));
      break;
    case PROP_TEXT_XALIGN:
      gtk_progress_set_text_alignment (progress,
				       g_value_get_float (value),
				       progress->y_align);
      break;
    case PROP_TEXT_YALIGN:
      gtk_progress_set_text_alignment (progress,
				       progress->x_align,
				       g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_progress_get_property (GObject      *object,
			   guint         prop_id,
			   GValue       *value,
			   GParamSpec   *pspec)
{
  GtkProgress *progress;
  
  progress = GTK_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_ACTIVITY_MODE:
      g_value_set_boolean (value, (progress->activity_mode != FALSE));
      break;
    case PROP_SHOW_TEXT:
      g_value_set_boolean (value, (progress->show_text != FALSE));
      break;
    case PROP_TEXT_XALIGN:
      g_value_set_float (value, progress->x_align);
      break;
    case PROP_TEXT_YALIGN:
      g_value_set_float (value, progress->y_align);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_progress_init (GtkProgress *progress)
{
  progress->adjustment = NULL;
  progress->offscreen_pixmap = NULL;
  progress->format = g_strdup (DEFAULT_FORMAT);
  progress->x_align = 0.5;
  progress->y_align = 0.5;
  progress->show_text = FALSE;
  progress->activity_mode = FALSE;
  progress->use_text_format = TRUE;
}

static void
gtk_progress_realize (GtkWidget *widget)
{
  GtkProgress *progress = GTK_PROGRESS (widget);
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
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, progress);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

  gtk_progress_create_pixmap (progress);
}

static void
gtk_progress_destroy (GtkObject *object)
{
  GtkProgress *progress = GTK_PROGRESS (object);

  if (progress->adjustment)
    {
      g_signal_handlers_disconnect_by_func (progress->adjustment,
					    gtk_progress_value_changed,
					    progress);
      g_signal_handlers_disconnect_by_func (progress->adjustment,
					    gtk_progress_changed,
					    progress);
      g_object_unref (progress->adjustment);
      progress->adjustment = NULL;
    }

  GTK_OBJECT_CLASS (gtk_progress_parent_class)->destroy (object);
}

static void
gtk_progress_finalize (GObject *object)
{
  GtkProgress *progress = GTK_PROGRESS (object);

  if (progress->offscreen_pixmap)
    g_object_unref (progress->offscreen_pixmap);

  g_free (progress->format);

  G_OBJECT_CLASS (gtk_progress_parent_class)->finalize (object);
}

static gboolean
gtk_progress_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    gdk_draw_drawable (widget->window,
		       widget->style->black_gc,
		       GTK_PROGRESS (widget)->offscreen_pixmap,
		       event->area.x, event->area.y,
		       event->area.x, event->area.y,
		       event->area.width,
		       event->area.height);

  return FALSE;
}

static void
gtk_progress_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  widget->allocation = *allocation;

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_progress_create_pixmap (GTK_PROGRESS (widget));
    }
}

static void
gtk_progress_create_pixmap (GtkProgress *progress)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_PROGRESS (progress));

  widget = GTK_WIDGET (progress);

  if (gtk_widget_get_realized (widget))
    {
      if (progress->offscreen_pixmap)
	g_object_unref (progress->offscreen_pixmap);

      progress->offscreen_pixmap = gdk_pixmap_new (widget->window,
						   widget->allocation.width,
						   widget->allocation.height,
						   -1);

      /* clear the pixmap for transparent themes */
      gtk_paint_flat_box (widget->style,
                          progress->offscreen_pixmap,
                          GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                          NULL, widget, "trough", 0, 0, -1, -1);
      
      GTK_PROGRESS_GET_CLASS (progress)->paint (progress);
    }
}

static void
gtk_progress_changed (GtkAdjustment *adjustment,
		      GtkProgress   *progress)
{
  /* A change in the value of adjustment->upper can change
   * the size request
   */
  if (progress->use_text_format && progress->show_text)
    gtk_widget_queue_resize (GTK_WIDGET (progress));
  else
    GTK_PROGRESS_GET_CLASS (progress)->update (progress);
}

static void
gtk_progress_value_changed (GtkAdjustment *adjustment,
			    GtkProgress   *progress)
{
  GTK_PROGRESS_GET_CLASS (progress)->update (progress);
}

static gchar *
gtk_progress_build_string (GtkProgress *progress,
			   gdouble      value,
			   gdouble      percentage)
{
  gchar buf[256] = { 0 };
  gchar tmp[256] = { 0 };
  gchar *src;
  gchar *dest;
  gchar fmt[10];

  src = progress->format;

  /* This is the new supported version of this function */
  if (!progress->use_text_format)
    return g_strdup (src);

  /* And here's all the deprecated goo. */
  
  dest = buf;
 
  while (src && *src)
    {
      if (*src != '%')
	{
	  *dest = *src;
	  dest++;
	}
      else
	{
	  gchar c;
	  gint digits;

	  c = *(src + sizeof(gchar));
	  digits = 0;

	  if (c >= '0' && c <= '2')
	    {
	      digits = (gint) (c - '0');
	      src++;
	      c = *(src + sizeof(gchar));
	    }

	  switch (c)
	    {
	    case '%':
	      *dest = '%';
	      src++;
	      dest++;
	      break;
	    case 'p':
	    case 'P':
	      if (digits)
		{
		  g_snprintf (fmt, sizeof (fmt), "%%.%df", digits);
		  g_snprintf (tmp, sizeof (tmp), fmt, 100 * percentage);
		}
	      else
		g_snprintf (tmp, sizeof (tmp), "%.0f", 100 * percentage);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    case 'v':
	    case 'V':
	      if (digits)
		{
		  g_snprintf (fmt, sizeof (fmt), "%%.%df", digits);
		  g_snprintf (tmp, sizeof (tmp), fmt, value);
		}
	      else
		g_snprintf (tmp, sizeof (tmp), "%.0f", value);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    case 'l':
	    case 'L':
	      if (digits)
		{
		  g_snprintf (fmt, sizeof (fmt), "%%.%df", digits);
		  g_snprintf (tmp, sizeof (tmp), fmt, progress->adjustment->lower);
		}
	      else
		g_snprintf (tmp, sizeof (tmp), "%.0f", progress->adjustment->lower);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    case 'u':
	    case 'U':
	      if (digits)
		{
		  g_snprintf (fmt, sizeof (fmt), "%%.%df", digits);
		  g_snprintf (tmp, sizeof (tmp), fmt, progress->adjustment->upper);
		}
	      else
		g_snprintf (tmp, sizeof (tmp), "%.0f", progress->adjustment->upper);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    default:
	      break;
	    }
	}
      src++;
    }

  return g_strdup (buf);
}

/***************************************************************/

void
gtk_progress_set_adjustment (GtkProgress   *progress,
			     GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  else
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0, 0, 100, 0, 0, 0);

  if (progress->adjustment != adjustment)
    {
      if (progress->adjustment)
        {
	  g_signal_handlers_disconnect_by_func (progress->adjustment,
						gtk_progress_changed,
						progress);
	  g_signal_handlers_disconnect_by_func (progress->adjustment,
						gtk_progress_value_changed,
						progress);
          g_object_unref (progress->adjustment);
        }
      progress->adjustment = adjustment;
      if (adjustment)
        {
          g_object_ref_sink (adjustment);
          g_signal_connect (adjustment, "changed",
			    G_CALLBACK (gtk_progress_changed),
			    progress);
          g_signal_connect (adjustment, "value-changed",
			    G_CALLBACK (gtk_progress_value_changed),
			    progress);
        }

      gtk_progress_changed (adjustment, progress);
    }
}

void
gtk_progress_configure (GtkProgress *progress,
			gdouble      value,
			gdouble      min,
			gdouble      max)
{
  GtkAdjustment *adj;
  gboolean changed = FALSE;

  g_return_if_fail (GTK_IS_PROGRESS (progress));
  g_return_if_fail (min <= max);
  g_return_if_fail (value >= min && value <= max);

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);
  adj = progress->adjustment;

  if (fabs (adj->lower - min) > EPSILON || fabs (adj->upper - max) > EPSILON)
    changed = TRUE;

  adj->value = value;
  adj->lower = min;
  adj->upper = max;

  gtk_adjustment_value_changed (adj);
  if (changed)
    gtk_adjustment_changed (adj);
}

void
gtk_progress_set_percentage (GtkProgress *progress,
			     gdouble      percentage)
{
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  g_return_if_fail (percentage >= 0 && percentage <= 1.0);

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);
  gtk_progress_set_value (progress, progress->adjustment->lower + percentage * 
		 (progress->adjustment->upper - progress->adjustment->lower));
}

gdouble
gtk_progress_get_current_percentage (GtkProgress *progress)
{
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);

  return gtk_progress_get_percentage_from_value (progress, progress->adjustment->value);
}

gdouble
gtk_progress_get_percentage_from_value (GtkProgress *progress,
					gdouble      value)
{
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);

  if (progress->adjustment->lower < progress->adjustment->upper &&
      value >= progress->adjustment->lower &&
      value <= progress->adjustment->upper)
    return (value - progress->adjustment->lower) /
      (progress->adjustment->upper - progress->adjustment->lower);
  else
    return 0.0;
}

void
gtk_progress_set_value (GtkProgress *progress,
			gdouble      value)
{
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);

  if (fabs (progress->adjustment->value - value) > EPSILON)
    gtk_adjustment_set_value (progress->adjustment, value);
}

gdouble
gtk_progress_get_value (GtkProgress *progress)
{
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  return progress->adjustment ? progress->adjustment->value : 0;
}

void
gtk_progress_set_show_text (GtkProgress *progress,
			    gboolean     show_text)
{
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (progress->show_text != show_text)
    {
      progress->show_text = show_text;

      gtk_widget_queue_resize (GTK_WIDGET (progress));

      g_object_notify (G_OBJECT (progress), "show-text");
    }
}

void
gtk_progress_set_text_alignment (GtkProgress *progress,
				 gfloat       x_align,
				 gfloat       y_align)
{
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  g_return_if_fail (x_align >= 0.0 && x_align <= 1.0);
  g_return_if_fail (y_align >= 0.0 && y_align <= 1.0);

  if (progress->x_align != x_align || progress->y_align != y_align)
    {
      g_object_freeze_notify (G_OBJECT (progress));
      if (progress->x_align != x_align)
	{
	  progress->x_align = x_align;
	  g_object_notify (G_OBJECT (progress), "text-xalign");
	}

      if (progress->y_align != y_align)
	{
	  progress->y_align = y_align;
	  g_object_notify (G_OBJECT (progress), "text-yalign");
	}
      g_object_thaw_notify (G_OBJECT (progress));

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (progress)))
	gtk_widget_queue_resize (GTK_WIDGET (progress));
    }
}

void
gtk_progress_set_format_string (GtkProgress *progress,
				const gchar *format)
{
  gchar *old_format;
  
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  /* Turn on format, in case someone called
   * gtk_progress_bar_set_text() and turned it off.
   */
  progress->use_text_format = TRUE;

  old_format = progress->format;

  if (!format)
    format = DEFAULT_FORMAT;

  progress->format = g_strdup (format);
  g_free (old_format);
  
  gtk_widget_queue_resize (GTK_WIDGET (progress));
}

gchar *
gtk_progress_get_current_text (GtkProgress *progress)
{
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), NULL);

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);

  return gtk_progress_build_string (progress, progress->adjustment->value,
				    gtk_progress_get_current_percentage (progress));
}

gchar *
gtk_progress_get_text_from_value (GtkProgress *progress,
				  gdouble      value)
{
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), NULL);

  if (!progress->adjustment)
    gtk_progress_set_adjustment (progress, NULL);

  return gtk_progress_build_string (progress, value,
				    gtk_progress_get_percentage_from_value (progress, value));
}

void
gtk_progress_set_activity_mode (GtkProgress *progress,
				gboolean     activity_mode)
{
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (progress->activity_mode != (activity_mode != FALSE))
    {
      progress->activity_mode = (activity_mode != FALSE);

      if (progress->activity_mode)
	GTK_PROGRESS_GET_CLASS (progress)->act_mode_enter (progress);

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (progress)))
	gtk_widget_queue_resize (GTK_WIDGET (progress));

      g_object_notify (G_OBJECT (progress), "activity-mode");
    }
}

#include "gtkaliasdef.c"
