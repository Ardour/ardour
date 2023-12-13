/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2009 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2009 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkcellrendererspinner.h"
#include "gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkalias.h"


/**
 * SECTION:gtkcellrendererspinner
 * @Short_description: Renders a spinning animation in a cell
 * @Title: GtkCellRendererSpinner
 * @See_also: #GtkSpinner, #GtkCellRendererProgress
 *
 * GtkCellRendererSpinner renders a spinning animation in a cell, very
 * similar to #GtkSpinner. It can often be used as an alternative
 * to a #GtkCellRendererProgress for displaying indefinite activity,
 * instead of actual progress.
 *
 * To start the animation in a cell, set the #GtkCellRendererSpinner:active
 * property to %TRUE and increment the #GtkCellRendererSpinner:pulse property
 * at regular intervals. The usual way to set the cell renderer properties
 * for each cell is to bind them to columns in your tree model using e.g.
 * gtk_tree_view_column_add_attribute().
 */


enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_PULSE,
  PROP_SIZE
};

struct _GtkCellRendererSpinnerPrivate
{
  gboolean active;
  guint pulse;
  GtkIconSize icon_size, old_icon_size;
  gint size;
};

#define GTK_CELL_RENDERER_SPINNER_GET_PRIVATE(object)        \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object),        \
                        GTK_TYPE_CELL_RENDERER_SPINNER, \
                        GtkCellRendererSpinnerPrivate))

static void gtk_cell_renderer_spinner_get_property (GObject         *object,
                                                    guint            param_id,
                                                    GValue          *value,
                                                    GParamSpec      *pspec);
static void gtk_cell_renderer_spinner_set_property (GObject         *object,
                                                    guint            param_id,
                                                    const GValue    *value,
                                                    GParamSpec      *pspec);
static void gtk_cell_renderer_spinner_get_size     (GtkCellRenderer *cell,
                                                    GtkWidget       *widget,
                                                    GdkRectangle    *cell_area,
                                                    gint            *x_offset,
                                                    gint            *y_offset,
                                                    gint            *width,
                                                    gint            *height);
static void gtk_cell_renderer_spinner_render       (GtkCellRenderer *cell,
                                                    GdkWindow       *window,
                                                    GtkWidget       *widget,
                                                    GdkRectangle    *background_area,
                                                    GdkRectangle    *cell_area,
                                                    GdkRectangle    *expose_area,
                                                    guint            flags);

G_DEFINE_TYPE (GtkCellRendererSpinner, gtk_cell_renderer_spinner, GTK_TYPE_CELL_RENDERER)

static void
gtk_cell_renderer_spinner_class_init (GtkCellRendererSpinnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->get_property = gtk_cell_renderer_spinner_get_property;
  object_class->set_property = gtk_cell_renderer_spinner_set_property;

  cell_class->get_size = gtk_cell_renderer_spinner_get_size;
  cell_class->render = gtk_cell_renderer_spinner_render;

  /* GtkCellRendererSpinner:active:
   *
   * Whether the spinner is active (ie. shown) in the cell
   *
   * Since: 2.20
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the spinner is active (ie. shown) in the cell"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  /**
   * GtkCellRendererSpinner:pulse:
   *
   * Pulse of the spinner. Increment this value to draw the next frame of the
   * spinner animation. Usually, you would update this value in a timeout.
   *
   * The #GtkSpinner widget draws one full cycle of the animation per second by default.
   * You can learn about the number of frames used by the theme
   * by looking at the #GtkSpinner:num-steps style property and the duration
   * of the cycle by looking at #GtkSpinner:cycle-duration.
   *
   * Since: 2.20
   */
  g_object_class_install_property (object_class,
                                   PROP_PULSE,
                                   g_param_spec_uint ("pulse",
                                                      P_("Pulse"),
                                                      P_("Pulse of the spinner"),
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE));
  /**
   * GtkCellRendererSpinner:size:
   *
   * The #GtkIconSize value that specifies the size of the rendered spinner.
   *
   * Since: 2.20
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_enum ("size",
                                                      P_("Size"),
                                                      P_("The GtkIconSize value that specifies the size of the rendered spinner"),
                                                      GTK_TYPE_ICON_SIZE, GTK_ICON_SIZE_MENU,
                                                      G_PARAM_READWRITE));


  g_type_class_add_private (object_class, sizeof (GtkCellRendererSpinnerPrivate));
}

static void
gtk_cell_renderer_spinner_init (GtkCellRendererSpinner *cell)
{
  cell->priv = GTK_CELL_RENDERER_SPINNER_GET_PRIVATE (cell);
  cell->priv->pulse = 0;
  cell->priv->old_icon_size = GTK_ICON_SIZE_INVALID;
  cell->priv->icon_size = GTK_ICON_SIZE_MENU;
}

/**
 * gtk_cell_renderer_spinner_new
 *
 * Returns a new cell renderer which will show a spinner to indicate
 * activity.
 *
 * Return value: a new #GtkCellRenderer
 *
 * Since: 2.20
 */
GtkCellRenderer *
gtk_cell_renderer_spinner_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_SPINNER, NULL);
}

static void
gtk_cell_renderer_spinner_update_size (GtkCellRendererSpinner *cell,
                                       GtkWidget              *widget)
{
  GtkCellRendererSpinnerPrivate *priv = cell->priv;
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;

  if (cell->priv->old_icon_size == cell->priv->icon_size)
    return;

  screen = gtk_widget_get_screen (GTK_WIDGET (widget));
  icon_theme = gtk_icon_theme_get_for_screen (screen);
  settings = gtk_settings_get_for_screen (screen);

  if (!gtk_icon_size_lookup_for_settings (settings, priv->icon_size, &priv->size, NULL))
    {
      g_warning ("Invalid icon size %u\n", priv->icon_size);
      priv->size = 24;
    }
}

static void
gtk_cell_renderer_spinner_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (object);
  GtkCellRendererSpinnerPrivate *priv = cell->priv;

  switch (param_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, priv->active);
        break;
      case PROP_PULSE:
        g_value_set_uint (value, priv->pulse);
        break;
      case PROP_SIZE:
        g_value_set_enum (value, priv->icon_size);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_spinner_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (object);
  GtkCellRendererSpinnerPrivate *priv = cell->priv;

  switch (param_id)
    {
      case PROP_ACTIVE:
        priv->active = g_value_get_boolean (value);
        break;
      case PROP_PULSE:
        priv->pulse = g_value_get_uint (value);
        break;
      case PROP_SIZE:
        priv->old_icon_size = priv->icon_size;
        priv->icon_size = g_value_get_enum (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_spinner_get_size (GtkCellRenderer *cellr,
                                    GtkWidget       *widget,
                                    GdkRectangle    *cell_area,
                                    gint            *x_offset,
                                    gint            *y_offset,
                                    gint            *width,
                                    gint            *height)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (cellr);
  GtkCellRendererSpinnerPrivate *priv = cell->priv;
  gdouble align;
  gint w, h;
  gint xpad, ypad;
  gfloat xalign, yalign;
  gboolean rtl;

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  gtk_cell_renderer_spinner_update_size (cell, widget);

  g_object_get (cellr,
                "xpad", &xpad,
                "ypad", &ypad,
                "xalign", &xalign,
                "yalign", &yalign,
                NULL);
  w = h = priv->size;

  if (cell_area)
    {
      if (x_offset)
        {
          align = rtl ? 1.0 - xalign : xalign;
          *x_offset = align * (cell_area->width - w);
          *x_offset = MAX (*x_offset, 0);
        }
      if (y_offset)
        {
          align = rtl ? 1.0 - yalign : yalign;
          *y_offset = align * (cell_area->height - h);
          *y_offset = MAX (*y_offset, 0);
        }
    }
  else
    {
      if (x_offset)
        *x_offset = 0;
      if (y_offset)
        *y_offset = 0;
    }

  if (width)
    *width = w;
  if (height)
    *height = h;
}

static void
gtk_cell_renderer_spinner_render (GtkCellRenderer *cellr,
                                  GdkWindow       *window,
                                  GtkWidget       *widget,
                                  GdkRectangle    *background_area,
                                  GdkRectangle    *cell_area,
                                  GdkRectangle    *expose_area,
                                  guint            flags)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (cellr);
  GtkCellRendererSpinnerPrivate *priv = cell->priv;
  GtkStateType state;
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;
  gint xpad, ypad;

  if (!priv->active)
    return;

  gtk_cell_renderer_spinner_get_size (cellr, widget, cell_area,
                                      &pix_rect.x, &pix_rect.y,
                                      &pix_rect.width, &pix_rect.height);

  g_object_get (cellr,
                "xpad", &xpad,
                "ypad", &ypad,
                NULL);
  pix_rect.x += cell_area->x + xpad;
  pix_rect.y += cell_area->y + ypad;
  pix_rect.width -= xpad * 2;
  pix_rect.height -= ypad * 2;

  if (!gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect) ||
      !gdk_rectangle_intersect (expose_area, &pix_rect, &draw_rect))
    {
      return;
    }

  state = GTK_STATE_NORMAL;
  if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE || !cellr->sensitive)
    {
      state = GTK_STATE_INSENSITIVE;
    }
  else
    {
      if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
        {
          if (gtk_widget_has_focus (widget))
            state = GTK_STATE_SELECTED;
          else
            state = GTK_STATE_ACTIVE;
        }
      else
        state = GTK_STATE_PRELIGHT;
    }

  gtk_paint_spinner (widget->style,
                     window,
                     state,
                     expose_area,
                     widget,
                     "cell",
                     priv->pulse,
                     draw_rect.x, draw_rect.y,
                     draw_rect.width, draw_rect.height);
}

#define __GTK_CELL_RENDERER_SPINNER_C__
#include "gtkaliasdef.c"
