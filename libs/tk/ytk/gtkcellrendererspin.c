/* GtkCellRendererSpin
 * Copyright (C) 2004 Lorenzo Gil Sanchez
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
 *
 * Authors: Lorenzo Gil Sanchez    <lgs@sicem.biz>
 *          Carlos Garnacho Parro  <carlosg@gnome.org>
 */

#include "config.h"
#include <gdk/gdkkeysyms.h>
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkspinbutton.h"
#include "gtkcellrendererspin.h"
#include "gtkalias.h"

#define GTK_CELL_RENDERER_SPIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_CELL_RENDERER_SPIN, GtkCellRendererSpinPrivate))

struct _GtkCellRendererSpinPrivate
{
  GtkAdjustment *adjustment;
  gdouble climb_rate;
  guint   digits;
};

static void gtk_cell_renderer_spin_finalize   (GObject                  *object);

static void gtk_cell_renderer_spin_get_property (GObject      *object,
						 guint         prop_id,
						 GValue       *value,
						 GParamSpec   *spec);
static void gtk_cell_renderer_spin_set_property (GObject      *object,
						 guint         prop_id,
						 const GValue *value,
						 GParamSpec   *spec);

static GtkCellEditable * gtk_cell_renderer_spin_start_editing (GtkCellRenderer     *cell,
							       GdkEvent            *event,
							       GtkWidget           *widget,
							       const gchar         *path,
							       GdkRectangle        *background_area,
							       GdkRectangle        *cell_area,
							       GtkCellRendererState flags);
enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_CLIMB_RATE,
  PROP_DIGITS
};

#define GTK_CELL_RENDERER_SPIN_PATH "gtk-cell-renderer-spin-path"

G_DEFINE_TYPE (GtkCellRendererSpin, gtk_cell_renderer_spin, GTK_TYPE_CELL_RENDERER_TEXT)


static void
gtk_cell_renderer_spin_class_init (GtkCellRendererSpinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize     = gtk_cell_renderer_spin_finalize;
  object_class->get_property = gtk_cell_renderer_spin_get_property;
  object_class->set_property = gtk_cell_renderer_spin_set_property;

  cell_class->start_editing  = gtk_cell_renderer_spin_start_editing;

  /**
   * GtkCellRendererSpin:adjustment:
   *
   * The adjustment that holds the value of the spinbutton. 
   * This must be non-%NULL for the cell renderer to be editable.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
				   PROP_ADJUSTMENT,
				   g_param_spec_object ("adjustment",
							P_("Adjustment"),
							P_("The adjustment that holds the value of the spinbutton."),
							GTK_TYPE_ADJUSTMENT,
							GTK_PARAM_READWRITE));


  /**
   * GtkCellRendererSpin:climb-rate:
   *
   * The acceleration rate when you hold down a button.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
				   PROP_CLIMB_RATE,
				   g_param_spec_double ("climb-rate",
							P_("Climb rate"),
							P_("The acceleration rate when you hold down a button"),
							0.0, G_MAXDOUBLE, 0.0,
							GTK_PARAM_READWRITE));  
  /**
   * GtkCellRendererSpin:digits:
   *
   * The number of decimal places to display.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
				   PROP_DIGITS,
				   g_param_spec_uint ("digits",
						      P_("Digits"),
						      P_("The number of decimal places to display"),
						      0, 20, 0,
						      GTK_PARAM_READWRITE));  

  g_type_class_add_private (object_class, sizeof (GtkCellRendererSpinPrivate));
}

static void
gtk_cell_renderer_spin_init (GtkCellRendererSpin *self)
{
  GtkCellRendererSpinPrivate *priv;

  priv = GTK_CELL_RENDERER_SPIN_GET_PRIVATE (self);

  priv->adjustment = NULL;
  priv->climb_rate = 0.0;
  priv->digits = 0;
}

static void
gtk_cell_renderer_spin_finalize (GObject *object)
{
  GtkCellRendererSpinPrivate *priv;

  priv = GTK_CELL_RENDERER_SPIN_GET_PRIVATE (object);

  if (priv && priv->adjustment)
    g_object_unref (priv->adjustment);

  G_OBJECT_CLASS (gtk_cell_renderer_spin_parent_class)->finalize (object);
}

static void
gtk_cell_renderer_spin_get_property (GObject      *object,
				     guint         prop_id,
				     GValue       *value,
				     GParamSpec   *pspec)
{
  GtkCellRendererSpin *renderer;
  GtkCellRendererSpinPrivate *priv;

  renderer = GTK_CELL_RENDERER_SPIN (object);
  priv = GTK_CELL_RENDERER_SPIN_GET_PRIVATE (renderer);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;
    case PROP_CLIMB_RATE:
      g_value_set_double (value, priv->climb_rate);
      break;
    case PROP_DIGITS:
      g_value_set_uint (value, priv->digits);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_spin_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  GtkCellRendererSpin *renderer;
  GtkCellRendererSpinPrivate *priv;
  GObject *obj;

  renderer = GTK_CELL_RENDERER_SPIN (object);
  priv = GTK_CELL_RENDERER_SPIN_GET_PRIVATE (renderer);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      obj = g_value_get_object (value);

      if (priv->adjustment)
	{
	  g_object_unref (priv->adjustment);
	  priv->adjustment = NULL;
	}

      if (obj)
	priv->adjustment = g_object_ref_sink (obj);
      break;
    case PROP_CLIMB_RATE:
      priv->climb_rate = g_value_get_double (value);
      break;
    case PROP_DIGITS:
      priv->digits = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_cell_renderer_spin_focus_out_event (GtkWidget *widget,
					GdkEvent  *event,
					gpointer   data)
{
  const gchar *path;
  const gchar *new_text;
  gboolean canceled;

  g_object_get (widget,
                "editing-canceled", &canceled,
                NULL);

  g_signal_handlers_disconnect_by_func (widget,
					gtk_cell_renderer_spin_focus_out_event,
					data);

  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (data), canceled);

  if (!canceled)
    {
      path = g_object_get_data (G_OBJECT (widget), GTK_CELL_RENDERER_SPIN_PATH);

      new_text = gtk_entry_get_text (GTK_ENTRY (widget));
      g_signal_emit_by_name (data, "edited", path, new_text);
    }
  
  return FALSE;
}

static gboolean
gtk_cell_renderer_spin_key_press_event (GtkWidget   *widget,
					GdkEventKey *event,
					gpointer     data)
{
  if (event->state == 0)
    {
      if (event->keyval == GDK_Up)
	{
	  gtk_spin_button_spin (GTK_SPIN_BUTTON (widget), GTK_SPIN_STEP_FORWARD, 1);
	  return TRUE;
	}
      else if (event->keyval == GDK_Down)
	{
	  gtk_spin_button_spin (GTK_SPIN_BUTTON (widget), GTK_SPIN_STEP_BACKWARD, 1);
	  return TRUE;
	}
    }

  return FALSE;
}

static gboolean
gtk_cell_renderer_spin_button_press_event (GtkWidget      *widget,
                                           GdkEventButton *event,
                                           gpointer        user_data)
{
  /* Block 2BUTTON and 3BUTTON here, so that they won't be eaten
   * by tree view.
   */
  if (event->type == GDK_2BUTTON_PRESS
      || event->type == GDK_3BUTTON_PRESS)
    return TRUE;

  return FALSE;
}

static GtkCellEditable *
gtk_cell_renderer_spin_start_editing (GtkCellRenderer     *cell,
				      GdkEvent            *event,
				      GtkWidget           *widget,
				      const gchar         *path,
				      GdkRectangle        *background_area,
				      GdkRectangle        *cell_area,
				      GtkCellRendererState flags)
{
  GtkCellRendererSpinPrivate *priv;
  GtkCellRendererText *cell_text;
  GtkWidget *spin;

  cell_text = GTK_CELL_RENDERER_TEXT (cell);
  priv = GTK_CELL_RENDERER_SPIN_GET_PRIVATE (cell);

  if (!cell_text->editable)
    return NULL;

  if (!priv->adjustment)
    return NULL;

  spin = gtk_spin_button_new (priv->adjustment,
			      priv->climb_rate, priv->digits);

  g_signal_connect (spin, "button-press-event",
                    G_CALLBACK (gtk_cell_renderer_spin_button_press_event),
                    NULL);

  if (cell_text->text)
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin),
			       g_ascii_strtod (cell_text->text, NULL));

  g_object_set_data_full (G_OBJECT (spin), GTK_CELL_RENDERER_SPIN_PATH,
			  g_strdup (path), g_free);

  g_signal_connect (G_OBJECT (spin), "focus-out-event",
		    G_CALLBACK (gtk_cell_renderer_spin_focus_out_event),
		    cell);
  g_signal_connect (G_OBJECT (spin), "key-press-event",
		    G_CALLBACK (gtk_cell_renderer_spin_key_press_event),
		    cell);

  gtk_widget_show (spin);

  return GTK_CELL_EDITABLE (spin);
}

/**
 * gtk_cell_renderer_spin_new:
 *
 * Creates a new #GtkCellRendererSpin. 
 *
 * Returns: a new #GtkCellRendererSpin
 *
 * Since: 2.10
 */
GtkCellRenderer *
gtk_cell_renderer_spin_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_SPIN, NULL);
}


#define __GTK_CELL_RENDERER_SPIN_C__
#include  "gtkaliasdef.c"
