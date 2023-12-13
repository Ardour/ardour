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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#undef GTK_DISABLE_DEPRECATED

#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmenuitem.h"
#include "gtkprivate.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkstyle.h"
#include "gtktooltips.h"
#include "gtkintl.h"
#include "gtkalias.h"


#define DEFAULT_DELAY 500           /* Default delay in ms */
#define STICKY_DELAY 0              /* Delay before popping up next tip
                                     * if we're sticky
                                     */
#define STICKY_REVERT_DELAY 1000    /* Delay before sticky tooltips revert
				     * to normal
                                     */
#define GTK_TOOLTIPS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TOOLTIPS, GtkTooltipsPrivate))

typedef struct _GtkTooltipsPrivate GtkTooltipsPrivate;

struct _GtkTooltipsPrivate
{
  GHashTable *tips_data_table;
};


static void gtk_tooltips_finalize          (GObject         *object);
static void gtk_tooltips_destroy           (GtkObject       *object);

static void gtk_tooltips_destroy_data      (GtkTooltipsData *tooltipsdata);

static void gtk_tooltips_widget_remove     (GtkWidget       *widget,
                                            gpointer         data);

static const gchar  tooltips_data_key[] = "_GtkTooltipsData";
static const gchar  tooltips_info_key[] = "_GtkTooltipsInfo";

G_DEFINE_TYPE (GtkTooltips, gtk_tooltips, GTK_TYPE_OBJECT)

static void
gtk_tooltips_class_init (GtkTooltipsClass *class)
{
  GtkObjectClass *object_class = (GtkObjectClass *) class;
  GObjectClass *gobject_class = (GObjectClass *) class;

  gobject_class->finalize = gtk_tooltips_finalize;

  object_class->destroy = gtk_tooltips_destroy;

  g_type_class_add_private (gobject_class, sizeof (GtkTooltipsPrivate));
}

static void
gtk_tooltips_init (GtkTooltips *tooltips)
{
  GtkTooltipsPrivate *private = GTK_TOOLTIPS_GET_PRIVATE (tooltips);

  tooltips->tip_window = NULL;
  tooltips->active_tips_data = NULL;
  tooltips->tips_data_list = NULL;

  tooltips->delay = DEFAULT_DELAY;
  tooltips->enabled = TRUE;
  tooltips->timer_tag = 0;
  tooltips->use_sticky_delay = FALSE;
  tooltips->last_popdown.tv_sec = -1;
  tooltips->last_popdown.tv_usec = -1;

  private->tips_data_table =
    g_hash_table_new_full (NULL, NULL, NULL,
                           (GDestroyNotify) gtk_tooltips_destroy_data);

  gtk_tooltips_force_window (tooltips);
}

static void
gtk_tooltips_finalize (GObject *object)
{
  GtkTooltipsPrivate *private = GTK_TOOLTIPS_GET_PRIVATE (object);

  g_hash_table_destroy (private->tips_data_table);

  G_OBJECT_CLASS (gtk_tooltips_parent_class)->finalize (object);
}

GtkTooltips *
gtk_tooltips_new (void)
{
  return g_object_new (GTK_TYPE_TOOLTIPS, NULL);
}

static void
gtk_tooltips_destroy_data (GtkTooltipsData *tooltipsdata)
{
  g_free (tooltipsdata->tip_text);
  g_free (tooltipsdata->tip_private);

  g_signal_handlers_disconnect_by_func (tooltipsdata->widget,
					gtk_tooltips_widget_remove,
					tooltipsdata);

  g_object_set_data (G_OBJECT (tooltipsdata->widget), I_(tooltips_data_key), NULL);
  g_object_unref (tooltipsdata->widget);
  g_free (tooltipsdata);
}

static void
gtk_tooltips_destroy (GtkObject *object)
{
  GtkTooltips *tooltips = GTK_TOOLTIPS (object);
  GtkTooltipsPrivate *private = GTK_TOOLTIPS_GET_PRIVATE (tooltips);

  if (tooltips->tip_window)
    {
      gtk_widget_destroy (tooltips->tip_window);
      tooltips->tip_window = NULL;
    }

  g_hash_table_remove_all (private->tips_data_table);

  GTK_OBJECT_CLASS (gtk_tooltips_parent_class)->destroy (object);
}

void
gtk_tooltips_force_window (GtkTooltips *tooltips)
{
  g_return_if_fail (GTK_IS_TOOLTIPS (tooltips));

  if (!tooltips->tip_window)
    {
      tooltips->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
      g_signal_connect (tooltips->tip_window,
			"destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&tooltips->tip_window);

      tooltips->tip_label = gtk_label_new (NULL);
      gtk_container_add (GTK_CONTAINER (tooltips->tip_window),
			 tooltips->tip_label);
    }
}

void
gtk_tooltips_enable (GtkTooltips *tooltips)
{
  g_return_if_fail (tooltips != NULL);

  tooltips->enabled = TRUE;
}

void
gtk_tooltips_disable (GtkTooltips *tooltips)
{
  g_return_if_fail (tooltips != NULL);

  tooltips->enabled = FALSE;
}

void
gtk_tooltips_set_delay (GtkTooltips *tooltips,
                        guint         delay)
{
  g_return_if_fail (tooltips != NULL);

  tooltips->delay = delay;
}

GtkTooltipsData*
gtk_tooltips_data_get (GtkWidget       *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);

  return g_object_get_data (G_OBJECT (widget), tooltips_data_key);
}


/**
 * gtk_tooltips_set_tip:
 * @tooltips: a #GtkTooltips.
 * @widget: the #GtkWidget you wish to associate the tip with.
 * @tip_text: (allow-none): a string containing the tip itself.
 * @tip_private: (allow-none): a string of any further information that may be useful if the user gets stuck.
 *
 * Adds a tooltip containing the message @tip_text to the specified #GtkWidget.
 * Deprecated: 2.12:
 */
void
gtk_tooltips_set_tip (GtkTooltips *tooltips,
		      GtkWidget   *widget,
		      const gchar *tip_text,
		      const gchar *tip_private)
{
  GtkTooltipsData *tooltipsdata;

  g_return_if_fail (GTK_IS_TOOLTIPS (tooltips));
  g_return_if_fail (widget != NULL);

  tooltipsdata = gtk_tooltips_data_get (widget);

  if (!tip_text)
    {
      if (tooltipsdata)
	gtk_tooltips_widget_remove (tooltipsdata->widget, tooltipsdata);
      return;
    }
  
  if (tooltips->active_tips_data 
      && tooltipsdata
      && tooltips->active_tips_data->widget == widget
      && GTK_WIDGET_DRAWABLE (tooltips->active_tips_data->widget))
    {
      g_free (tooltipsdata->tip_text);
      g_free (tooltipsdata->tip_private);

      tooltipsdata->tip_text = g_strdup (tip_text);
      tooltipsdata->tip_private = g_strdup (tip_private);
    }
  else 
    {
      g_object_ref (widget);
      
      if (tooltipsdata)
        gtk_tooltips_widget_remove (tooltipsdata->widget, tooltipsdata);
      
      tooltipsdata = g_new0 (GtkTooltipsData, 1);
      
      tooltipsdata->tooltips = tooltips;
      tooltipsdata->widget = widget;

      tooltipsdata->tip_text = g_strdup (tip_text);
      tooltipsdata->tip_private = g_strdup (tip_private);

      g_hash_table_insert (GTK_TOOLTIPS_GET_PRIVATE (tooltips)->tips_data_table,
                           widget, tooltipsdata);

      g_object_set_data (G_OBJECT (widget), I_(tooltips_data_key),
                         tooltipsdata);

      g_signal_connect (widget, "destroy",
                        G_CALLBACK (gtk_tooltips_widget_remove),
			tooltipsdata);
    }

  gtk_widget_set_tooltip_text (widget, tip_text);
}

static void
gtk_tooltips_widget_remove (GtkWidget *widget,
			    gpointer   data)
{
  GtkTooltipsData *tooltipsdata = (GtkTooltipsData*) data;
  GtkTooltips *tooltips = tooltipsdata->tooltips;
  GtkTooltipsPrivate *private = GTK_TOOLTIPS_GET_PRIVATE (tooltips);

  g_hash_table_remove (private->tips_data_table, tooltipsdata->widget);
}

/**
 * gtk_tooltips_get_info_from_tip_window:
 * @tip_window: a #GtkWindow 
 * @tooltips: the return location for the tooltips which are displayed 
 *    in @tip_window, or %NULL
 * @current_widget: the return location for the widget whose tooltips 
 *    are displayed, or %NULL
 * 
 * Determines the tooltips and the widget they belong to from the window in 
 * which they are displayed. 
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 * 
 * Return value: %TRUE if @tip_window is displaying tooltips, otherwise %FALSE.
 *
 * Since: 2.4
 *
 * Deprecated: 2.12:
 **/
gboolean
gtk_tooltips_get_info_from_tip_window (GtkWindow    *tip_window,
                                       GtkTooltips **tooltips,
                                       GtkWidget   **current_widget)
{
  GtkTooltips  *current_tooltips;  
  gboolean has_tips;

  g_return_val_if_fail (GTK_IS_WINDOW (tip_window), FALSE);

  current_tooltips = g_object_get_data (G_OBJECT (tip_window), tooltips_info_key);

  has_tips = current_tooltips != NULL;

  if (tooltips)
    *tooltips = current_tooltips;
  if (current_widget)
    *current_widget = (has_tips && current_tooltips->active_tips_data) ? current_tooltips->active_tips_data->widget : NULL;

  return has_tips;
}

#define __GTK_TOOLTIPS_C__
#include "gtkaliasdef.c"
