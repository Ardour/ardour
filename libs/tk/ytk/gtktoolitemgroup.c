/* GtkToolPalette -- A tool palette with categories and DnD support
 * Copyright (C) 2008  Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 *      Jan Arne Petersen
 */

#include "config.h"

#include "gtktoolpaletteprivate.h"

#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define ANIMATION_TIMEOUT        50
#define ANIMATION_DURATION      (ANIMATION_TIMEOUT * 4)
#define DEFAULT_ANIMATION_STATE  TRUE
#define DEFAULT_EXPANDER_SIZE    16
#define DEFAULT_HEADER_SPACING   2

#define DEFAULT_LABEL            ""
#define DEFAULT_COLLAPSED        FALSE
#define DEFAULT_ELLIPSIZE        PANGO_ELLIPSIZE_NONE

/**
 * SECTION:gtktoolitemgroup
 * @Short_description: A sub container used in a tool palette
 * @Title: GtkToolItemGroup
 *
 * A #GtkToolItemGroup is used together with #GtkToolPalette to add
 * #GtkToolItem<!-- -->s to a palette like container with different
 * categories and drag and drop support.
 *
 * Since: 2.20
 */

enum
{
  PROP_NONE,
  PROP_LABEL,
  PROP_LABEL_WIDGET,
  PROP_COLLAPSED,
  PROP_ELLIPSIZE,
  PROP_RELIEF
};

enum
{
  CHILD_PROP_NONE,
  CHILD_PROP_HOMOGENEOUS,
  CHILD_PROP_EXPAND,
  CHILD_PROP_FILL,
  CHILD_PROP_NEW_ROW,
  CHILD_PROP_POSITION,
};

typedef struct _GtkToolItemGroupChild GtkToolItemGroupChild;

struct _GtkToolItemGroupPrivate
{
  GtkWidget         *header;
  GtkWidget         *label_widget;

  GList             *children;

  gboolean           animation;
  gint64             animation_start;
  GSource           *animation_timeout;
  GtkExpanderStyle   expander_style;
  gint               expander_size;
  gint               header_spacing;
  PangoEllipsizeMode ellipsize;

  gulong             focus_set_id;
  GtkWidget         *toplevel;

  GtkSettings       *settings;
  gulong             settings_connection;

  guint              collapsed : 1;
};

struct _GtkToolItemGroupChild
{
  GtkToolItem *item;

  guint        homogeneous : 1;
  guint        expand : 1;
  guint        fill : 1;
  guint        new_row : 1;
};

static void gtk_tool_item_group_tool_shell_init (GtkToolShellIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkToolItemGroup, gtk_tool_item_group, GTK_TYPE_CONTAINER,
G_IMPLEMENT_INTERFACE (GTK_TYPE_TOOL_SHELL, gtk_tool_item_group_tool_shell_init));

static GtkWidget*
gtk_tool_item_group_get_alignment (GtkToolItemGroup *group)
{
  return gtk_bin_get_child (GTK_BIN (group->priv->header));
}

static GtkOrientation
gtk_tool_item_group_get_orientation (GtkToolShell *shell)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (shell));

  if (GTK_IS_TOOL_PALETTE (parent))
    return gtk_orientable_get_orientation (GTK_ORIENTABLE (parent));

  return GTK_ORIENTATION_VERTICAL;
}

static GtkToolbarStyle
gtk_tool_item_group_get_style (GtkToolShell *shell)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (shell));

  if (GTK_IS_TOOL_PALETTE (parent))
    return gtk_tool_palette_get_style (GTK_TOOL_PALETTE (parent));

  return GTK_TOOLBAR_ICONS;
}

static GtkIconSize
gtk_tool_item_group_get_icon_size (GtkToolShell *shell)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (shell));

  if (GTK_IS_TOOL_PALETTE (parent))
    return gtk_tool_palette_get_icon_size (GTK_TOOL_PALETTE (parent));

  return GTK_ICON_SIZE_SMALL_TOOLBAR;
}

static PangoEllipsizeMode
gtk_tool_item_group_get_ellipsize_mode (GtkToolShell *shell)
{
  return GTK_TOOL_ITEM_GROUP (shell)->priv->ellipsize;
}

static gfloat
gtk_tool_item_group_get_text_alignment (GtkToolShell *shell)
{
  if (GTK_TOOLBAR_TEXT == gtk_tool_item_group_get_style (shell) ||
      GTK_TOOLBAR_BOTH_HORIZ == gtk_tool_item_group_get_style (shell))
    return 0.0;

  return 0.5;
}

static GtkOrientation
gtk_tool_item_group_get_text_orientation (GtkToolShell *shell)
{
  return GTK_ORIENTATION_HORIZONTAL;
}

static GtkSizeGroup *
gtk_tool_item_group_get_text_size_group (GtkToolShell *shell)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (shell));

  if (GTK_IS_TOOL_PALETTE (parent))
    return _gtk_tool_palette_get_size_group (GTK_TOOL_PALETTE (parent));

  return NULL;
}

static void
animation_change_notify (GtkToolItemGroup *group)
{
  GtkSettings *settings = group->priv->settings;
  gboolean animation;

  if (settings)
    g_object_get (settings,
                  "gtk-enable-animations", &animation,
                  NULL);
  else
    animation = DEFAULT_ANIMATION_STATE;

  group->priv->animation = animation;
}

static void
gtk_tool_item_group_settings_change_notify (GtkSettings      *settings,
                                            const GParamSpec *pspec,
                                            GtkToolItemGroup *group)
{
  if (strcmp (pspec->name, "gtk-enable-animations") == 0)
    animation_change_notify (group);
}

static void
gtk_tool_item_group_screen_changed (GtkWidget *widget,
                                    GdkScreen *previous_screen)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (widget);
  GtkToolItemGroupPrivate* priv = group->priv;
  GtkSettings *old_settings = priv->settings;
  GtkSettings *settings;

  if (gtk_widget_has_screen (GTK_WIDGET (group)))
    settings = gtk_widget_get_settings (GTK_WIDGET (group));
  else
    settings = NULL;

  if (settings == old_settings)
    return;

  if (old_settings)
  {
    g_signal_handler_disconnect (old_settings, priv->settings_connection);
    g_object_unref (old_settings);
  }

  if (settings)
  {
    priv->settings_connection =
      g_signal_connect (settings, "notify",
                        G_CALLBACK (gtk_tool_item_group_settings_change_notify),
                        group);
    priv->settings = g_object_ref (settings);
  }
  else
    priv->settings = NULL;

  animation_change_notify (group);
}

static void
gtk_tool_item_group_tool_shell_init (GtkToolShellIface *iface)
{
  iface->get_icon_size = gtk_tool_item_group_get_icon_size;
  iface->get_orientation = gtk_tool_item_group_get_orientation;
  iface->get_style = gtk_tool_item_group_get_style;
  iface->get_text_alignment = gtk_tool_item_group_get_text_alignment;
  iface->get_text_orientation = gtk_tool_item_group_get_text_orientation;
  iface->get_text_size_group = gtk_tool_item_group_get_text_size_group;
  iface->get_ellipsize_mode = gtk_tool_item_group_get_ellipsize_mode;
}

static gboolean
gtk_tool_item_group_header_expose_event_cb (GtkWidget      *widget,
                                            GdkEventExpose *event,
                                            gpointer        data)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (data);
  GtkToolItemGroupPrivate* priv = group->priv;
  GtkExpanderStyle expander_style;
  GtkOrientation orientation;
  gint x, y;
  GtkTextDirection direction;

  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));
  expander_style = priv->expander_style;
  direction = gtk_widget_get_direction (widget);

  if (GTK_ORIENTATION_VERTICAL == orientation)
    {
      if (GTK_TEXT_DIR_RTL == direction)
        x = widget->allocation.x + widget->allocation.width - priv->expander_size / 2;
      else
        x = widget->allocation.x + priv->expander_size / 2;
      y = widget->allocation.y + widget->allocation.height / 2;
    }
  else
    {
      x = widget->allocation.x + widget->allocation.width / 2;
      y = widget->allocation.y + priv->expander_size / 2;

      /* Unfortunatly gtk_paint_expander() doesn't support rotated drawing
       * modes. Luckily the following shady arithmetics produce the desired
       * result. */
      expander_style = GTK_EXPANDER_EXPANDED - expander_style;
    }

  gtk_paint_expander (widget->style, widget->window,
                      priv->header->state,
                      &event->area, GTK_WIDGET (group),
                      "tool-palette-header", x, y,
                      expander_style);

  return FALSE;
}

static void
gtk_tool_item_group_header_size_request_cb (GtkWidget      *widget,
                                            GtkRequisition *requisition,
                                            gpointer        data)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (data);
  requisition->height = MAX (requisition->height, group->priv->expander_size);
}

static void
gtk_tool_item_group_header_clicked_cb (GtkButton *button,
                                       gpointer   data)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (data);
  GtkToolItemGroupPrivate* priv = group->priv;
  GtkWidget *parent = gtk_widget_get_parent (data);

  if (priv->collapsed ||
      !GTK_IS_TOOL_PALETTE (parent) ||
      !gtk_tool_palette_get_exclusive (GTK_TOOL_PALETTE (parent), data))
    gtk_tool_item_group_set_collapsed (group, !priv->collapsed);
}

static void
gtk_tool_item_group_header_adjust_style (GtkToolItemGroup *group)
{
  GtkWidget *alignment = gtk_tool_item_group_get_alignment (group);
  GtkWidget *label_widget = gtk_bin_get_child (GTK_BIN (alignment));
  GtkWidget *widget = GTK_WIDGET (group);
  GtkToolItemGroupPrivate* priv = group->priv;
  gint dx = 0, dy = 0;
  GtkTextDirection direction = gtk_widget_get_direction (widget);

  gtk_widget_style_get (widget,
                        "header-spacing", &(priv->header_spacing),
                        "expander-size", &(priv->expander_size),
                        NULL);

  switch (gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group)))
    {
      case GTK_ORIENTATION_HORIZONTAL:
        dy = priv->header_spacing + priv->expander_size;

        if (GTK_IS_LABEL (label_widget))
          {
            gtk_label_set_ellipsize (GTK_LABEL (label_widget), PANGO_ELLIPSIZE_NONE);
            if (GTK_TEXT_DIR_RTL == direction)
              gtk_label_set_angle (GTK_LABEL (label_widget), -90);
            else
              gtk_label_set_angle (GTK_LABEL (label_widget), 90);
          }
       break;

      case GTK_ORIENTATION_VERTICAL:
        dx = priv->header_spacing + priv->expander_size;

        if (GTK_IS_LABEL (label_widget))
          {
            gtk_label_set_ellipsize (GTK_LABEL (label_widget), priv->ellipsize);
            gtk_label_set_angle (GTK_LABEL (label_widget), 0);
          }
        break;
    }

  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), dy, 0, dx, 0);
}

static void
gtk_tool_item_group_init (GtkToolItemGroup *group)
{
  GtkWidget *alignment;
  GtkToolItemGroupPrivate* priv;

  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (group), FALSE);

  group->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (group,
                                             GTK_TYPE_TOOL_ITEM_GROUP,
                                             GtkToolItemGroupPrivate);

  priv->children = NULL;
  priv->header_spacing = DEFAULT_HEADER_SPACING;
  priv->expander_size = DEFAULT_EXPANDER_SIZE;
  priv->expander_style = GTK_EXPANDER_EXPANDED;

  priv->label_widget = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (priv->label_widget), 0.0, 0.5);
  alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_container_add (GTK_CONTAINER (alignment), priv->label_widget);
  gtk_widget_show_all (alignment);

  gtk_widget_push_composite_child ();
  priv->header = gtk_button_new ();
  gtk_widget_set_composite_name (priv->header, "header");
  gtk_widget_pop_composite_child ();

  g_object_ref_sink (priv->header);
  gtk_button_set_focus_on_click (GTK_BUTTON (priv->header), FALSE);
  gtk_container_add (GTK_CONTAINER (priv->header), alignment);
  gtk_widget_set_parent (priv->header, GTK_WIDGET (group));

  gtk_tool_item_group_header_adjust_style (group);

  g_signal_connect_after (alignment, "expose-event",
                          G_CALLBACK (gtk_tool_item_group_header_expose_event_cb),
                          group);
  g_signal_connect_after (alignment, "size-request",
                          G_CALLBACK (gtk_tool_item_group_header_size_request_cb),
                          group);

  g_signal_connect (priv->header, "clicked",
                    G_CALLBACK (gtk_tool_item_group_header_clicked_cb),
                    group);
}

static void
gtk_tool_item_group_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (object);

  switch (prop_id)
    {
      case PROP_LABEL:
        gtk_tool_item_group_set_label (group, g_value_get_string (value));
        break;

      case PROP_LABEL_WIDGET:
        gtk_tool_item_group_set_label_widget (group, g_value_get_object (value));
	break;

      case PROP_COLLAPSED:
        gtk_tool_item_group_set_collapsed (group, g_value_get_boolean (value));
        break;

      case PROP_ELLIPSIZE:
        gtk_tool_item_group_set_ellipsize (group, g_value_get_enum (value));
        break;

      case PROP_RELIEF:
        gtk_tool_item_group_set_header_relief (group, g_value_get_enum(value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_item_group_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (object);

  switch (prop_id)
    {
      case PROP_LABEL:
        g_value_set_string (value, gtk_tool_item_group_get_label (group));
        break;

      case PROP_LABEL_WIDGET:
        g_value_set_object (value,
			    gtk_tool_item_group_get_label_widget (group));
        break;

      case PROP_COLLAPSED:
        g_value_set_boolean (value, gtk_tool_item_group_get_collapsed (group));
        break;

      case PROP_ELLIPSIZE:
        g_value_set_enum (value, gtk_tool_item_group_get_ellipsize (group));
        break;

      case PROP_RELIEF:
        g_value_set_enum (value, gtk_tool_item_group_get_header_relief (group));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_item_group_finalize (GObject *object)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (object);

  if (group->priv->children)
    {
      g_list_free (group->priv->children);
      group->priv->children = NULL;
    }

  G_OBJECT_CLASS (gtk_tool_item_group_parent_class)->finalize (object);
}

static void
gtk_tool_item_group_dispose (GObject *object)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (object);
  GtkToolItemGroupPrivate* priv = group->priv;

  if (priv->toplevel)
    {
      /* disconnect focus tracking handler */
      g_signal_handler_disconnect (priv->toplevel,
                                   priv->focus_set_id);

      priv->focus_set_id = 0;
      priv->toplevel = NULL;
    }

  G_OBJECT_CLASS (gtk_tool_item_group_parent_class)->dispose (object);
}

static void
gtk_tool_item_group_get_item_size (GtkToolItemGroup *group,
                                   GtkRequisition   *item_size,
                                   gboolean          homogeneous_only,
                                   gint             *requested_rows)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (group));

  if (GTK_IS_TOOL_PALETTE (parent))
    _gtk_tool_palette_get_item_size (GTK_TOOL_PALETTE (parent), item_size, homogeneous_only, requested_rows);
  else
    _gtk_tool_item_group_item_size_request (group, item_size, homogeneous_only, requested_rows);
}

static void
gtk_tool_item_group_size_request (GtkWidget      *widget,
                                  GtkRequisition *requisition)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (widget);
  GtkToolItemGroupPrivate* priv = group->priv;
  GtkOrientation orientation;
  GtkRequisition item_size;
  gint requested_rows;

  if (priv->children && gtk_tool_item_group_get_label_widget (group))
    {
      gtk_widget_size_request (priv->header, requisition);
      gtk_widget_show (priv->header);
    }
  else
    {
      requisition->width = requisition->height = 0;
      gtk_widget_hide (priv->header);
    }

  gtk_tool_item_group_get_item_size (group, &item_size, FALSE, &requested_rows);

  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));

  if (GTK_ORIENTATION_VERTICAL == orientation)
    requisition->width = MAX (requisition->width, item_size.width);
  else
    requisition->height = MAX (requisition->height, item_size.height * requested_rows);

  requisition->width += border_width * 2;
  requisition->height += border_width * 2;
}

static gboolean
gtk_tool_item_group_is_item_visible (GtkToolItemGroup      *group,
                                     GtkToolItemGroupChild *child)
{
  GtkToolbarStyle style;
  GtkOrientation orientation;

  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));
  style = gtk_tool_shell_get_style (GTK_TOOL_SHELL (group));

  /* horizontal tool palettes with text style support only homogeneous items */
  if (!child->homogeneous &&
      GTK_ORIENTATION_HORIZONTAL == orientation &&
      GTK_TOOLBAR_TEXT == style)
    return FALSE;

  return
    (gtk_widget_get_visible (GTK_WIDGET (child->item))) &&
    (GTK_ORIENTATION_VERTICAL == orientation ?
     gtk_tool_item_get_visible_vertical (child->item) :
     gtk_tool_item_get_visible_horizontal (child->item));
}

static inline unsigned
udiv (unsigned x,
      unsigned y)
{
  return (x + y - 1) / y;
}

static void
gtk_tool_item_group_real_size_query (GtkWidget      *widget,
                                     GtkAllocation  *allocation,
                                     GtkRequisition *inquery)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (widget);
  GtkToolItemGroupPrivate* priv = group->priv;

  GtkRequisition item_size;
  GtkAllocation item_area;

  GtkOrientation orientation;
  GtkToolbarStyle style;

  gint min_rows;

  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));
  style = gtk_tool_shell_get_style (GTK_TOOL_SHELL (group));

  /* figure out the size of homogeneous items */
  gtk_tool_item_group_get_item_size (group, &item_size, TRUE, &min_rows);

  if (GTK_ORIENTATION_VERTICAL == orientation)
    item_size.width = MIN (item_size.width, allocation->width);
  else
    item_size.height = MIN (item_size.height, allocation->height);

  item_size.width  = MAX (item_size.width, 1);
  item_size.height = MAX (item_size.height, 1);

  item_area.width = 0;
  item_area.height = 0;

  /* figure out the required columns (n_columns) and rows (n_rows) to place all items */
  if (!priv->collapsed || !priv->animation || priv->animation_timeout)
    {
      guint n_columns;
      gint n_rows;
      GList *it;

      if (GTK_ORIENTATION_VERTICAL == orientation)
        {
          gboolean new_row = FALSE;
          gint row = -1;
          guint col = 0;

          item_area.width = allocation->width - 2 * border_width;
          n_columns = MAX (item_area.width / item_size.width, 1);

          /* calculate required rows for n_columns columns */
          for (it = priv->children; it != NULL; it = it->next)
            {
              GtkToolItemGroupChild *child = it->data;

              if (!gtk_tool_item_group_is_item_visible (group, child))
                continue;

              if (new_row || child->new_row)
                {
                  new_row = FALSE;
                  row++;
                  col = 0;
                }

              if (child->expand)
                new_row = TRUE;

              if (child->homogeneous)
                {
                  col++;
                  if (col >= n_columns)
                    new_row = TRUE;
                }
              else
                {
                  GtkRequisition req = {0, 0};
                  guint width;

                  gtk_widget_size_request (GTK_WIDGET (child->item), &req);

                  width = udiv (req.width, item_size.width);
                  col += width;

                  if (col > n_columns)
                    row++;

                  col = width;

                  if (col >= n_columns)
                    new_row = TRUE;
                }
            }
          n_rows = row + 2;
        }
      else
        {
          guint *row_min_width;
          gint row = -1;
          gboolean new_row = TRUE;
          guint col = 0, min_col, max_col = 0, all_items = 0;
          gint i;

          item_area.height = allocation->height - 2 * border_width;
          n_rows = MAX (item_area.height / item_size.height, min_rows);

          row_min_width = g_new0 (guint, n_rows);

          /* calculate minimal and maximal required cols and minimal required rows */
          for (it = priv->children; it != NULL; it = it->next)
            {
              GtkToolItemGroupChild *child = it->data;

              if (!gtk_tool_item_group_is_item_visible (group, child))
                continue;

              if (new_row || child->new_row)
                {
                  new_row = FALSE;
                  row++;
                  col = 0;
                  row_min_width[row] = 1;
                }

              if (child->expand)
                new_row = TRUE;

              if (child->homogeneous)
                {
                  col++;
                  all_items++;
                }
              else
                {
                  GtkRequisition req = {0, 0};
                  guint width;

                  gtk_widget_size_request (GTK_WIDGET (child->item), &req);

                  width = udiv (req.width, item_size.width);

                  col += width;
                  all_items += width;

                  row_min_width[row] = MAX (row_min_width[row], width);
                }

              max_col = MAX (max_col, col);
            }

          /* calculate minimal required cols */
          min_col = udiv (all_items, n_rows);

          for (i = 0; i <= row; i++)
            {
              min_col = MAX (min_col, row_min_width[i]);
            }

          /* simple linear search for minimal required columns for the given maximal number of rows (n_rows) */
          for (n_columns = min_col; n_columns < max_col; n_columns ++)
            {
              new_row = TRUE;
              row = -1;
              /* calculate required rows for n_columns columns */
              for (it = priv->children; it != NULL; it = it->next)
                {
                  GtkToolItemGroupChild *child = it->data;

                  if (!gtk_tool_item_group_is_item_visible (group, child))
                    continue;

                  if (new_row || child->new_row)
                    {
                      new_row = FALSE;
                      row++;
                      col = 0;
                    }

                  if (child->expand)
                    new_row = TRUE;

                  if (child->homogeneous)
                    {
                      col++;
                      if (col >= n_columns)
                        new_row = TRUE;
                    }
                  else
                    {
                      GtkRequisition req = {0, 0};
                      guint width;

                      gtk_widget_size_request (GTK_WIDGET (child->item), &req);

                      width = udiv (req.width, item_size.width);
                      col += width;

                      if (col > n_columns)
                        row++;

                      col = width;

                      if (col >= n_columns)
                        new_row = TRUE;
                    }
                }

              if (row < n_rows)
                break;
            }
        }

      item_area.width = item_size.width * n_columns;
      item_area.height = item_size.height * n_rows;
    }

  inquery->width = 0;
  inquery->height = 0;

  /* figure out header widget size */
  if (gtk_widget_get_visible (priv->header))
    {
      GtkRequisition child_requisition;

      gtk_widget_size_request (priv->header, &child_requisition);

      if (GTK_ORIENTATION_VERTICAL == orientation)
        inquery->height += child_requisition.height;
      else
        inquery->width += child_requisition.width;
    }

  /* report effective widget size */
  inquery->width += item_area.width + 2 * border_width;
  inquery->height += item_area.height + 2 * border_width;
}

static void
gtk_tool_item_group_real_size_allocate (GtkWidget     *widget,
                                        GtkAllocation *allocation)
{
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (widget);
  GtkToolItemGroupPrivate* priv = group->priv;
  GtkRequisition child_requisition;
  GtkAllocation child_allocation;

  GtkRequisition item_size;
  GtkAllocation item_area;

  GtkOrientation orientation;
  GtkToolbarStyle style;

  GList *it;

  gint n_columns, n_rows = 1;
  gint min_rows;

  GtkTextDirection direction = gtk_widget_get_direction (widget);

  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));
  style = gtk_tool_shell_get_style (GTK_TOOL_SHELL (group));

  /* chain up */
  GTK_WIDGET_CLASS (gtk_tool_item_group_parent_class)->size_allocate (widget, allocation);

  child_allocation.x = border_width;
  child_allocation.y = border_width;

  /* place the header widget */
  if (gtk_widget_get_visible (priv->header))
    {
      gtk_widget_size_request (priv->header, &child_requisition);

      if (GTK_ORIENTATION_VERTICAL == orientation)
        {
          child_allocation.width = allocation->width;
          child_allocation.height = child_requisition.height;
        }
      else
        {
          child_allocation.width = child_requisition.width;
          child_allocation.height = allocation->height;

          if (GTK_TEXT_DIR_RTL == direction)
            child_allocation.x = allocation->width - border_width - child_allocation.width;
        }

      gtk_widget_size_allocate (priv->header, &child_allocation);

      if (GTK_ORIENTATION_VERTICAL == orientation)
        child_allocation.y += child_allocation.height;
      else if (GTK_TEXT_DIR_RTL != direction)
        child_allocation.x += child_allocation.width;
      else
        child_allocation.x = border_width;
    }
  else
    child_requisition.width = child_requisition.height = 0;

  /* figure out the size of homogeneous items */
  gtk_tool_item_group_get_item_size (group, &item_size, TRUE, &min_rows);

  item_size.width  = MAX (item_size.width, 1);
  item_size.height = MAX (item_size.height, 1);

  /* figure out the available columns and size of item_area */
  if (GTK_ORIENTATION_VERTICAL == orientation)
    {
      item_size.width = MIN (item_size.width, allocation->width);

      item_area.width = allocation->width - 2 * border_width;
      item_area.height = allocation->height - 2 * border_width - child_requisition.height;

      n_columns = MAX (item_area.width / item_size.width, 1);

      item_size.width = item_area.width / n_columns;
    }
  else
    {
      item_size.height = MIN (item_size.height, allocation->height);

      item_area.width = allocation->width - 2 * border_width - child_requisition.width;
      item_area.height = allocation->height - 2 * border_width;

      n_columns = MAX (item_area.width / item_size.width, 1);
      n_rows = MAX (item_area.height / item_size.height, min_rows);

      item_size.height = item_area.height / n_rows;
    }

  item_area.x = child_allocation.x;
  item_area.y = child_allocation.y;

  /* when expanded or in transition, place the tool items in a grid like layout */
  if (!priv->collapsed || !priv->animation || priv->animation_timeout)
    {
      gint col = 0, row = 0;

      for (it = priv->children; it != NULL; it = it->next)
        {
          GtkToolItemGroupChild *child = it->data;
          gint col_child;

          if (!gtk_tool_item_group_is_item_visible (group, child))
            {
              gtk_widget_set_child_visible (GTK_WIDGET (child->item), FALSE);

              continue;
            }

          /* for non homogeneous widgets request the required size */
          child_requisition.width = 0;

          if (!child->homogeneous)
            {
              gtk_widget_size_request (GTK_WIDGET (child->item), &child_requisition);
              child_requisition.width = MIN (child_requisition.width, item_area.width);
            }

          /* select next row if at end of row */
          if (col > 0 && (child->new_row || (col * item_size.width) + MAX (child_requisition.width, item_size.width) > item_area.width))
            {
              row++;
              col = 0;
              child_allocation.y += child_allocation.height;
            }

          col_child = col;

          /* calculate the position and size of the item */
          if (!child->homogeneous)
            {
              gint col_width;
              gint width;

              if (!child->expand)
                col_width = udiv (child_requisition.width, item_size.width);
              else
                col_width = n_columns - col;

              width = col_width * item_size.width;

              if (GTK_TEXT_DIR_RTL == direction)
                col_child = (n_columns - col - col_width);

              if (child->fill)
                {
                  child_allocation.x = item_area.x + col_child * item_size.width;
                  child_allocation.width = width;
                }
              else
                {
                  child_allocation.x =
                    (item_area.x + col_child * item_size.width +
                    (width - child_requisition.width) / 2);
                  child_allocation.width = child_requisition.width;
                }

              col += col_width;
            }
          else
            {
              if (GTK_TEXT_DIR_RTL == direction)
                col_child = (n_columns - col - 1);

              child_allocation.x = item_area.x + col_child * item_size.width;
              child_allocation.width = item_size.width;

              col++;
            }

          child_allocation.height = item_size.height;

          gtk_widget_size_allocate (GTK_WIDGET (child->item), &child_allocation);
          gtk_widget_set_child_visible (GTK_WIDGET (child->item), TRUE);
        }

      child_allocation.y += item_size.height;
    }

  /* or just hide all items, when collapsed */

  else
    {
      for (it = priv->children; it != NULL; it = it->next)
        {
          GtkToolItemGroupChild *child = it->data;

          gtk_widget_set_child_visible (GTK_WIDGET (child->item), FALSE);
        }
    }
}

static void
gtk_tool_item_group_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  gtk_tool_item_group_real_size_allocate (widget, allocation);

  if (gtk_widget_get_mapped (widget))
    gdk_window_invalidate_rect (widget->window, NULL, FALSE);
}

static void
gtk_tool_item_group_set_focus_cb (GtkWidget *window,
                                  GtkWidget *widget,
                                  gpointer   user_data)
{
  GtkAdjustment *adjustment;
  GtkWidget *p;

  /* Find this group's parent widget in the focused widget's anchestry. */
  for (p = widget; p; p = gtk_widget_get_parent (p))
    if (p == user_data)
      {
        p = gtk_widget_get_parent (p);
        break;
      }

  if (GTK_IS_TOOL_PALETTE (p))
    {
      /* Check that the focused widgets is fully visible within
       * the group's parent widget and make it visible otherwise. */

      adjustment = gtk_tool_palette_get_hadjustment (GTK_TOOL_PALETTE (p));
      adjustment = gtk_tool_palette_get_vadjustment (GTK_TOOL_PALETTE (p));

      if (adjustment)
        {
          int y;

          /* Handle vertical adjustment. */
          if (gtk_widget_translate_coordinates
                (widget, p, 0, 0, NULL, &y) && y < 0)
            {
              y += adjustment->value;
              gtk_adjustment_clamp_page (adjustment, y, y + widget->allocation.height);
            }
          else if (gtk_widget_translate_coordinates
                      (widget, p, 0, widget->allocation.height, NULL, &y) &&
                   y > p->allocation.height)
            {
              y += adjustment->value;
              gtk_adjustment_clamp_page (adjustment, y - widget->allocation.height, y);
            }
        }

      adjustment = gtk_tool_palette_get_hadjustment (GTK_TOOL_PALETTE (p));

      if (adjustment)
        {
          int x;

          /* Handle horizontal adjustment. */
          if (gtk_widget_translate_coordinates
                (widget, p, 0, 0, &x, NULL) && x < 0)
            {
              x += adjustment->value;
              gtk_adjustment_clamp_page (adjustment, x, x + widget->allocation.width);
            }
          else if (gtk_widget_translate_coordinates
                      (widget, p, widget->allocation.width, 0, &x, NULL) &&
                   x > p->allocation.width)
            {
              x += adjustment->value;
              gtk_adjustment_clamp_page (adjustment, x - widget->allocation.width, x);
            }

          return;
        }
    }
}

static void
gtk_tool_item_group_set_toplevel_window (GtkToolItemGroup *group,
                                         GtkWidget        *toplevel)
{
  GtkToolItemGroupPrivate* priv = group->priv;

  if (toplevel != priv->toplevel)
    {
      if (priv->toplevel)
        {
          /* Disconnect focus tracking handler. */
          g_signal_handler_disconnect (priv->toplevel,
                                       priv->focus_set_id);

          priv->focus_set_id = 0;
          priv->toplevel = NULL;
        }

      if (toplevel)
        {
          /* Install focus tracking handler. We connect to the window's
           * set-focus signal instead of connecting to the focus signal of
           * each child to:
           *
           * 1) Reduce the number of signal handlers used.
           * 2) Avoid special handling for group headers.
           * 3) Catch focus grabs not only for direct children,
           *    but also for nested widgets.
           */
          priv->focus_set_id =
            g_signal_connect (toplevel, "set-focus",
                              G_CALLBACK (gtk_tool_item_group_set_focus_cb),
                              group);

          priv->toplevel = toplevel;
        }
    }
}

static void
gtk_tool_item_group_realize (GtkWidget *widget)
{
  GtkWidget *toplevel_window;
  const gint border_width = GTK_CONTAINER (widget)->border_width;
  gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  GdkWindowAttr attributes;
  GdkDisplay *display;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget)
                         | GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK
                         | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                         | GDK_BUTTON_MOTION_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, attributes_mask);

  display = gdk_window_get_display (widget->window);

  if (gdk_display_supports_composite (display))
    gdk_window_set_composited (widget->window, TRUE);

  gdk_window_set_user_data (widget->window, widget);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_widget_set_realized (widget, TRUE);

  gtk_container_forall (GTK_CONTAINER (widget),
                        (GtkCallback) gtk_widget_set_parent_window,
                        widget->window);

  gtk_widget_queue_resize_no_redraw (widget);

  toplevel_window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
  gtk_tool_item_group_set_toplevel_window (GTK_TOOL_ITEM_GROUP (widget),
                                           toplevel_window);
}

static void
gtk_tool_item_group_unrealize (GtkWidget *widget)
{
  gtk_tool_item_group_set_toplevel_window (GTK_TOOL_ITEM_GROUP (widget), NULL);
  GTK_WIDGET_CLASS (gtk_tool_item_group_parent_class)->unrealize (widget);
}

static void
gtk_tool_item_group_style_set (GtkWidget *widget,
                               GtkStyle  *previous_style)
{
  gtk_tool_item_group_header_adjust_style (GTK_TOOL_ITEM_GROUP (widget));
  GTK_WIDGET_CLASS (gtk_tool_item_group_parent_class)->style_set (widget, previous_style);
}

static void
gtk_tool_item_group_add (GtkContainer *container,
                         GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM (widget));

  gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (container),
                              GTK_TOOL_ITEM (widget), -1);
}

static void
gtk_tool_item_group_remove (GtkContainer *container,
                            GtkWidget    *child)
{
  GtkToolItemGroup *group;
  GtkToolItemGroupPrivate* priv;
  GList *it;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (container));
  group = GTK_TOOL_ITEM_GROUP (container);
  priv = group->priv;

  for (it = priv->children; it != NULL; it = it->next)
    {
      GtkToolItemGroupChild *child_info = it->data;

      if ((GtkWidget *)child_info->item == child)
        {
          g_object_unref (child);
          gtk_widget_unparent (child);

          g_free (child_info);
          priv->children = g_list_delete_link (priv->children, it);

          gtk_widget_queue_resize (GTK_WIDGET (container));
          break;
        }
    }
}

static void
gtk_tool_item_group_forall (GtkContainer *container,
                            gboolean      internals,
                            GtkCallback   callback,
                            gpointer      callback_data)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (container);
  GtkToolItemGroupPrivate* priv = group->priv;
  GList *children;

  if (internals && priv->header)
    callback (priv->header, callback_data);

  children = priv->children;
  while (children)
    {
      GtkToolItemGroupChild *child = children->data;
      children = children->next; /* store pointer before call to callback
				    because the child pointer is invalid if the
				    child->item is removed from the item group
				    in callback */

      callback (GTK_WIDGET (child->item), callback_data);
    }
}

static GType
gtk_tool_item_group_child_type (GtkContainer *container)
{
  return GTK_TYPE_TOOL_ITEM;
}

static GtkToolItemGroupChild *
gtk_tool_item_group_get_child (GtkToolItemGroup  *group,
                               GtkToolItem       *item,
                               gint              *position,
                               GList            **link)
{
  guint i;
  GList *it;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), NULL);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (item), NULL);

  for (it = group->priv->children, i = 0; it != NULL; it = it->next, ++i)
    {
      GtkToolItemGroupChild *child = it->data;

      if (child->item == item)
        {
          if (position)
            *position = i;

          if (link)
            *link = it;

          return child;
        }
    }

  return NULL;
}

static void
gtk_tool_item_group_get_item_packing (GtkToolItemGroup *group,
                                      GtkToolItem      *item,
                                      gboolean         *homogeneous,
                                      gboolean         *expand,
                                      gboolean         *fill,
                                      gboolean         *new_row)
{
  GtkToolItemGroupChild *child;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));

  child = gtk_tool_item_group_get_child (group, item, NULL, NULL);
  if (!child)
    return;

  if (expand)
    *expand = child->expand;

  if (homogeneous)
    *homogeneous = child->homogeneous;

  if (fill)
    *fill = child->fill;

  if (new_row)
    *new_row = child->new_row;
}

static void
gtk_tool_item_group_set_item_packing (GtkToolItemGroup *group,
                                      GtkToolItem      *item,
                                      gboolean          homogeneous,
                                      gboolean          expand,
                                      gboolean          fill,
                                      gboolean          new_row)
{
  GtkToolItemGroupChild *child;
  gboolean changed = FALSE;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));

  child = gtk_tool_item_group_get_child (group, item, NULL, NULL);
  if (!child)
    return;

  gtk_widget_freeze_child_notify (GTK_WIDGET (item));

  if (child->homogeneous != homogeneous)
    {
      child->homogeneous = homogeneous;
      changed = TRUE;
      gtk_widget_child_notify (GTK_WIDGET (item), "homogeneous");
    }
  if (child->expand != expand)
    {
      child->expand = expand;
      changed = TRUE;
      gtk_widget_child_notify (GTK_WIDGET (item), "expand");
    }
  if (child->fill != fill)
    {
      child->fill = fill;
      changed = TRUE;
      gtk_widget_child_notify (GTK_WIDGET (item), "fill");
    }
  if (child->new_row != new_row)
    {
      child->new_row = new_row;
      changed = TRUE;
      gtk_widget_child_notify (GTK_WIDGET (item), "new-row");
    }

  gtk_widget_thaw_child_notify (GTK_WIDGET (item));

  if (changed
      && gtk_widget_get_visible (GTK_WIDGET (group))
      && gtk_widget_get_visible (GTK_WIDGET (item)))
    gtk_widget_queue_resize (GTK_WIDGET (group));
}

static void
gtk_tool_item_group_set_child_property (GtkContainer *container,
                                        GtkWidget    *child,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (container);
  GtkToolItem *item = GTK_TOOL_ITEM (child);
  gboolean homogeneous, expand, fill, new_row;

  if (prop_id != CHILD_PROP_POSITION)
    gtk_tool_item_group_get_item_packing (group, item,
                                          &homogeneous,
                                          &expand,
                                          &fill,
                                          &new_row);

  switch (prop_id)
    {
      case CHILD_PROP_HOMOGENEOUS:
        gtk_tool_item_group_set_item_packing (group, item,
                                              g_value_get_boolean (value),
                                              expand,
                                              fill,
                                              new_row);
        break;

      case CHILD_PROP_EXPAND:
        gtk_tool_item_group_set_item_packing (group, item,
                                              homogeneous,
                                              g_value_get_boolean (value),
                                              fill,
                                              new_row);
        break;

      case CHILD_PROP_FILL:
        gtk_tool_item_group_set_item_packing (group, item,
                                              homogeneous,
                                              expand,
                                              g_value_get_boolean (value),
                                              new_row);
        break;

      case CHILD_PROP_NEW_ROW:
        gtk_tool_item_group_set_item_packing (group, item,
                                              homogeneous,
                                              expand,
                                              fill,
                                              g_value_get_boolean (value));
        break;

      case CHILD_PROP_POSITION:
        gtk_tool_item_group_set_item_position (group, item, g_value_get_int (value));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_item_group_get_child_property (GtkContainer *container,
                                        GtkWidget    *child,
                                        guint         prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (container);
  GtkToolItem *item = GTK_TOOL_ITEM (child);
  gboolean homogeneous, expand, fill, new_row;

  if (prop_id != CHILD_PROP_POSITION)
    gtk_tool_item_group_get_item_packing (group, item,
                                          &homogeneous,
                                          &expand,
                                          &fill,
                                          &new_row);

  switch (prop_id)
    {
      case CHILD_PROP_HOMOGENEOUS:
        g_value_set_boolean (value, homogeneous);
        break;

       case CHILD_PROP_EXPAND:
        g_value_set_boolean (value, expand);
        break;

       case CHILD_PROP_FILL:
        g_value_set_boolean (value, fill);
        break;

       case CHILD_PROP_NEW_ROW:
        g_value_set_boolean (value, new_row);
        break;

     case CHILD_PROP_POSITION:
        g_value_set_int (value, gtk_tool_item_group_get_item_position (group, item));
        break;

      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
        break;
    }
}

static void
gtk_tool_item_group_class_init (GtkToolItemGroupClass *cls)
{
  GObjectClass       *oclass = G_OBJECT_CLASS (cls);
  GtkWidgetClass     *wclass = GTK_WIDGET_CLASS (cls);
  GtkContainerClass  *cclass = GTK_CONTAINER_CLASS (cls);

  oclass->set_property       = gtk_tool_item_group_set_property;
  oclass->get_property       = gtk_tool_item_group_get_property;
  oclass->finalize           = gtk_tool_item_group_finalize;
  oclass->dispose            = gtk_tool_item_group_dispose;

  wclass->size_request       = gtk_tool_item_group_size_request;
  wclass->size_allocate      = gtk_tool_item_group_size_allocate;
  wclass->realize            = gtk_tool_item_group_realize;
  wclass->unrealize          = gtk_tool_item_group_unrealize;
  wclass->style_set          = gtk_tool_item_group_style_set;
  wclass->screen_changed     = gtk_tool_item_group_screen_changed;

  cclass->add                = gtk_tool_item_group_add;
  cclass->remove             = gtk_tool_item_group_remove;
  cclass->forall             = gtk_tool_item_group_forall;
  cclass->child_type         = gtk_tool_item_group_child_type;
  cclass->set_child_property = gtk_tool_item_group_set_child_property;
  cclass->get_child_property = gtk_tool_item_group_get_child_property;

  g_object_class_install_property (oclass, PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("The human-readable title of this item group"),
                                                        DEFAULT_LABEL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (oclass, PROP_LABEL_WIDGET,
                                   g_param_spec_object  ("label-widget",
                                                        P_("Label widget"),
                                                        P_("A widget to display in place of the usual label"),
                                                        GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (oclass, PROP_COLLAPSED,
                                   g_param_spec_boolean ("collapsed",
                                                         P_("Collapsed"),
                                                         P_("Whether the group has been collapsed and items are hidden"),
                                                         DEFAULT_COLLAPSED,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (oclass, PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
                                                      P_("ellipsize"),
                                                      P_("Ellipsize for item group headers"),
                                                      PANGO_TYPE_ELLIPSIZE_MODE, DEFAULT_ELLIPSIZE,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (oclass, PROP_RELIEF,
                                   g_param_spec_enum ("header-relief",
                                                      P_("Header Relief"),
                                                      P_("Relief of the group header button"),
                                                      GTK_TYPE_RELIEF_STYLE, GTK_RELIEF_NORMAL,
                                                      GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (wclass,
                                           g_param_spec_int ("expander-size",
                                                             P_("Expander Size"),
                                                             P_("Size of the expander arrow"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_EXPANDER_SIZE,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (wclass,
                                           g_param_spec_int ("header-spacing",
                                                             P_("Header Spacing"),
                                                             P_("Spacing between expander arrow and caption"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_HEADER_SPACING,
                                                             GTK_PARAM_READABLE));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_HOMOGENEOUS,
                                              g_param_spec_boolean ("homogeneous",
                                                                    P_("Homogeneous"),
                                                                    P_("Whether the item should be the same size as other homogeneous items"),
                                                                    TRUE,
                                                                    GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_EXPAND,
                                              g_param_spec_boolean ("expand",
                                                                    P_("Expand"),
                                                                    P_("Whether the item should receive extra space when the group grows"),
                                                                    FALSE,
                                                                    GTK_PARAM_READWRITE)); 

  gtk_container_class_install_child_property (cclass, CHILD_PROP_FILL,
                                              g_param_spec_boolean ("fill",
                                                                    P_("Fill"),
                                                                    P_("Whether the item should fill the available space"),
                                                                    TRUE,
                                                                    GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_NEW_ROW,
                                              g_param_spec_boolean ("new-row",
                                                                    P_("New Row"),
                                                                    P_("Whether the item should start a new row"),
                                                                    FALSE,
                                                                    GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (cclass, CHILD_PROP_POSITION,
                                              g_param_spec_int ("position",
                                                                P_("Position"),
                                                                P_("Position of the item within this group"),
                                                                0,
                                                                G_MAXINT,
                                                                0,
                                                                GTK_PARAM_READWRITE));

  g_type_class_add_private (cls, sizeof (GtkToolItemGroupPrivate));
}

/**
 * gtk_tool_item_group_new:
 * @label: the label of the new group
 *
 * Creates a new tool item group with label @label.
 *
 * Returns: a new #GtkToolItemGroup.
 *
 * Since: 2.20
 */
GtkWidget*
gtk_tool_item_group_new (const gchar *label)
{
  return g_object_new (GTK_TYPE_TOOL_ITEM_GROUP, "label", label, NULL);
}

/**
 * gtk_tool_item_group_set_label:
 * @group: a #GtkToolItemGroup
 * @label: the new human-readable label of of the group
 *
 * Sets the label of the tool item group. The label is displayed in the header
 * of the group.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_set_label (GtkToolItemGroup *group,
                               const gchar      *label)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  if (!label)
    gtk_tool_item_group_set_label_widget (group, NULL);
  else
    {
      GtkWidget *child = gtk_label_new (label);
      gtk_widget_show (child);

      gtk_tool_item_group_set_label_widget (group, child);
    }

  g_object_notify (G_OBJECT (group), "label");
}

/**
 * gtk_tool_item_group_set_label_widget:
 * @group: a #GtkToolItemGroup
 * @label_widget: the widget to be displayed in place of the usual label
 *
 * Sets the label of the tool item group.
 * The label widget is displayed in the header of the group, in place
 * of the usual label.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_set_label_widget (GtkToolItemGroup *group,
                                      GtkWidget        *label_widget)
{
  GtkToolItemGroupPrivate* priv;
  GtkWidget *alignment;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || label_widget->parent == NULL);

  priv = group->priv;

  if (priv->label_widget == label_widget)
    return;

  alignment = gtk_tool_item_group_get_alignment (group);

  if (priv->label_widget)
    {
      gtk_widget_set_state (priv->label_widget, GTK_STATE_NORMAL);
      gtk_container_remove (GTK_CONTAINER (alignment), priv->label_widget);
    }


  if (label_widget)
      gtk_container_add (GTK_CONTAINER (alignment), label_widget);

  priv->label_widget = label_widget;

  if (gtk_widget_get_visible (GTK_WIDGET (group)))
    gtk_widget_queue_resize (GTK_WIDGET (group));

  /* Only show the header widget if the group has children: */
  if (label_widget && priv->children)
    gtk_widget_show (priv->header);
  else
    gtk_widget_hide (priv->header);

  g_object_freeze_notify (G_OBJECT (group));
  g_object_notify (G_OBJECT (group), "label-widget");
  g_object_notify (G_OBJECT (group), "label");
  g_object_thaw_notify (G_OBJECT (group));
}

/**
 * gtk_tool_item_group_set_header_relief:
 * @group: a #GtkToolItemGroup
 * @style: the #GtkReliefStyle
 *
 * Set the button relief of the group header.
 * See gtk_button_set_relief() for details.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_set_header_relief (GtkToolItemGroup *group,
                                       GtkReliefStyle    style)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  gtk_button_set_relief (GTK_BUTTON (group->priv->header), style);
}

static gint64
gtk_tool_item_group_get_animation_timestamp (GtkToolItemGroup *group)
{
  return (g_source_get_time (group->priv->animation_timeout) -
          group->priv->animation_start) / 1000;
}

static void
gtk_tool_item_group_force_expose (GtkToolItemGroup *group)
{
  GtkToolItemGroupPrivate* priv = group->priv;
  GtkWidget *widget = GTK_WIDGET (group);

  if (gtk_widget_get_realized (priv->header))
    {
      GtkWidget *alignment = gtk_tool_item_group_get_alignment (group);
      GdkRectangle area;

      /* Find the header button's arrow area... */
      area.x = alignment->allocation.x;
      area.y = alignment->allocation.y + (alignment->allocation.height - priv->expander_size) / 2;
      area.height = priv->expander_size;
      area.width = priv->expander_size;

      /* ... and invalidated it to get it animated. */
      gdk_window_invalidate_rect (priv->header->window, &area, TRUE);
    }

  if (gtk_widget_get_realized (widget))
    {
      GtkWidget *parent = gtk_widget_get_parent (widget);
      int x, y, width, height;

      /* Find the tool item area button's arrow area... */
      width = widget->allocation.width;
      height = widget->allocation.height;

      gtk_widget_translate_coordinates (widget, parent, 0, 0, &x, &y);

      if (gtk_widget_get_visible (priv->header))
        {
          height -= priv->header->allocation.height;
          y += priv->header->allocation.height;
        }

      /* ... and invalidated it to get it animated. */
      gtk_widget_queue_draw_area (parent, x, y, width, height);
    }
}

static gboolean
gtk_tool_item_group_animation_cb (gpointer data)
{
  GtkToolItemGroup *group = GTK_TOOL_ITEM_GROUP (data);
  GtkToolItemGroupPrivate* priv = group->priv;
  gint64 timestamp = gtk_tool_item_group_get_animation_timestamp (group);
  gboolean retval;

  GDK_THREADS_ENTER ();

  /* Enque this early to reduce number of expose events. */
  gtk_widget_queue_resize_no_redraw (GTK_WIDGET (group));

  /* Figure out current style of the expander arrow. */
  if (priv->collapsed)
    {
      if (priv->expander_style == GTK_EXPANDER_EXPANDED)
        priv->expander_style = GTK_EXPANDER_SEMI_COLLAPSED;
      else
        priv->expander_style = GTK_EXPANDER_COLLAPSED;
    }
  else
    {
      if (priv->expander_style == GTK_EXPANDER_COLLAPSED)
        priv->expander_style = GTK_EXPANDER_SEMI_EXPANDED;
      else
        priv->expander_style = GTK_EXPANDER_EXPANDED;
    }

  gtk_tool_item_group_force_expose (group);

  /* Finish animation when done. */
  if (timestamp >= ANIMATION_DURATION)
    priv->animation_timeout = NULL;

  retval = (priv->animation_timeout != NULL);

  GDK_THREADS_LEAVE ();

  return retval;
}

/**
 * gtk_tool_item_group_set_collapsed:
 * @group: a #GtkToolItemGroup
 * @collapsed: whether the @group should be collapsed or expanded
 *
 * Sets whether the @group should be collapsed or expanded.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_set_collapsed (GtkToolItemGroup *group,
                                   gboolean          collapsed)
{
  GtkWidget *parent;
  GtkToolItemGroupPrivate* priv;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  priv = group->priv;

  parent = gtk_widget_get_parent (GTK_WIDGET (group));
  if (GTK_IS_TOOL_PALETTE (parent) && !collapsed)
    _gtk_tool_palette_set_expanding_child (GTK_TOOL_PALETTE (parent),
                                           GTK_WIDGET (group));
  if (collapsed != priv->collapsed)
    {
      if (priv->animation)
        {
          if (priv->animation_timeout)
            g_source_destroy (priv->animation_timeout);

          priv->animation_start = g_get_monotonic_time ();
          priv->animation_timeout = g_timeout_source_new (ANIMATION_TIMEOUT);

          g_source_set_callback (priv->animation_timeout,
                                 gtk_tool_item_group_animation_cb,
                                 group, NULL);

          g_source_attach (priv->animation_timeout, NULL);
        }
        else
        {
          priv->expander_style = GTK_EXPANDER_COLLAPSED;
          gtk_tool_item_group_force_expose (group);
        }

      priv->collapsed = collapsed;
      g_object_notify (G_OBJECT (group), "collapsed");
    }
}

/**
 * gtk_tool_item_group_set_ellipsize:
 * @group: a #GtkToolItemGroup
 * @ellipsize: the #PangoEllipsizeMode labels in @group should use
 *
 * Sets the ellipsization mode which should be used by labels in @group.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_set_ellipsize (GtkToolItemGroup   *group,
                                   PangoEllipsizeMode  ellipsize)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));

  if (ellipsize != group->priv->ellipsize)
    {
      group->priv->ellipsize = ellipsize;
      gtk_tool_item_group_header_adjust_style (group);
      g_object_notify (G_OBJECT (group), "ellipsize");
      _gtk_tool_item_group_palette_reconfigured (group);
    }
}

/**
 * gtk_tool_item_group_get_label:
 * @group: a #GtkToolItemGroup
 *
 * Gets the label of @group.
 *
 * Returns: the label of @group. The label is an internal string of @group
 *     and must not be modified. Note that %NULL is returned if a custom
 *     label has been set with gtk_tool_item_group_set_label_widget()
 *
 * Since: 2.20
 */
const gchar*
gtk_tool_item_group_get_label (GtkToolItemGroup *group)
{
  GtkToolItemGroupPrivate *priv;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), NULL);

  priv = group->priv;

  if (GTK_IS_LABEL (priv->label_widget))
    return gtk_label_get_label (GTK_LABEL (priv->label_widget));
  else
    return NULL;
}

/**
 * gtk_tool_item_group_get_label_widget:
 * @group: a #GtkToolItemGroup
 *
 * Gets the label widget of @group.
 * See gtk_tool_item_group_set_label_widget().
 *
 * Returns: (transfer none): the label widget of @group
 *
 * Since: 2.20
 */
GtkWidget*
gtk_tool_item_group_get_label_widget (GtkToolItemGroup *group)
{
  GtkWidget *alignment = gtk_tool_item_group_get_alignment (group);

  return gtk_bin_get_child (GTK_BIN (alignment));
}

/**
 * gtk_tool_item_group_get_collapsed:
 * @group: a GtkToolItemGroup
 *
 * Gets whether @group is collapsed or expanded.
 *
 * Returns: %TRUE if @group is collapsed, %FALSE if it is expanded
 *
 * Since: 2.20
 */
gboolean
gtk_tool_item_group_get_collapsed (GtkToolItemGroup *group)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_COLLAPSED);

  return group->priv->collapsed;
}

/**
 * gtk_tool_item_group_get_ellipsize:
 * @group: a #GtkToolItemGroup
 *
 * Gets the ellipsization mode of @group.
 *
 * Returns: the #PangoEllipsizeMode of @group
 *
 * Since: 2.20
 */
PangoEllipsizeMode
gtk_tool_item_group_get_ellipsize (GtkToolItemGroup *group)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), DEFAULT_ELLIPSIZE);

  return group->priv->ellipsize;
}

/**
 * gtk_tool_item_group_get_header_relief:
 * @group: a #GtkToolItemGroup
 *
 * Gets the relief mode of the header button of @group.
 *
 * Returns: the #GtkReliefStyle
 *
 * Since: 2.20
 */
GtkReliefStyle
gtk_tool_item_group_get_header_relief (GtkToolItemGroup   *group)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), GTK_RELIEF_NORMAL);

  return gtk_button_get_relief (GTK_BUTTON (group->priv->header));
}

/**
 * gtk_tool_item_group_insert:
 * @group: a #GtkToolItemGroup
 * @item: the #GtkToolItem to insert into @group
 * @position: the position of @item in @group, starting with 0.
 *     The position -1 means end of list.
 *
 * Inserts @item at @position in the list of children of @group.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_insert (GtkToolItemGroup *group,
                            GtkToolItem      *item,
                            gint              position)
{
  GtkWidget *parent, *child_widget;
  GtkToolItemGroupChild *child;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));
  g_return_if_fail (position >= -1);

  parent = gtk_widget_get_parent (GTK_WIDGET (group));

  child = g_new (GtkToolItemGroupChild, 1);
  child->item = g_object_ref_sink (item);
  child->homogeneous = TRUE;
  child->expand = FALSE;
  child->fill = TRUE;
  child->new_row = FALSE;

  group->priv->children = g_list_insert (group->priv->children, child, position);

  if (GTK_IS_TOOL_PALETTE (parent))
    _gtk_tool_palette_child_set_drag_source (GTK_WIDGET (item), parent);

  child_widget = gtk_bin_get_child (GTK_BIN (item));

  if (GTK_IS_BUTTON (child_widget))
    gtk_button_set_focus_on_click (GTK_BUTTON (child_widget), TRUE);

  gtk_widget_set_parent (GTK_WIDGET (item), GTK_WIDGET (group));
}

/**
 * gtk_tool_item_group_set_item_position:
 * @group: a #GtkToolItemGroup
 * @item: the #GtkToolItem to move to a new position, should
 *     be a child of @group.
 * @position: the new position of @item in @group, starting with 0.
 *     The position -1 means end of list.
 *
 * Sets the position of @item in the list of children of @group.
 *
 * Since: 2.20
 */
void
gtk_tool_item_group_set_item_position (GtkToolItemGroup *group,
                                       GtkToolItem      *item,
                                       gint              position)
{
  gint old_position;
  GList *link;
  GtkToolItemGroupChild *child;
  GtkToolItemGroupPrivate* priv;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (GTK_IS_TOOL_ITEM (item));
  g_return_if_fail (position >= -1);

  child = gtk_tool_item_group_get_child (group, item, &old_position, &link);
  priv = group->priv;

  g_return_if_fail (child != NULL);

  if (position == old_position)
    return;

  priv->children = g_list_delete_link (priv->children, link);
  priv->children = g_list_insert (priv->children, child, position);

  gtk_widget_child_notify (GTK_WIDGET (item), "position");
  if (gtk_widget_get_visible (GTK_WIDGET (group)) &&
      gtk_widget_get_visible (GTK_WIDGET (item)))
    gtk_widget_queue_resize (GTK_WIDGET (group));
}

/**
 * gtk_tool_item_group_get_item_position:
 * @group: a #GtkToolItemGroup
 * @item: a #GtkToolItem
 *
 * Gets the position of @item in @group as index.
 *
 * Returns: the index of @item in @group or -1 if @item is no child of @group
 *
 * Since: 2.20
 */
gint
gtk_tool_item_group_get_item_position (GtkToolItemGroup *group,
                                       GtkToolItem      *item)
{
  gint position;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), -1);
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (item), -1);

  if (gtk_tool_item_group_get_child (group, item, &position, NULL))
    return position;

  return -1;
}

/**
 * gtk_tool_item_group_get_n_items:
 * @group: a #GtkToolItemGroup
 *
 * Gets the number of tool items in @group.
 *
 * Returns: the number of tool items in @group
 *
 * Since: 2.20
 */
guint
gtk_tool_item_group_get_n_items (GtkToolItemGroup *group)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), 0);

  return g_list_length (group->priv->children);
}

/**
 * gtk_tool_item_group_get_nth_item:
 * @group: a #GtkToolItemGroup
 * @index: the index
 *
 * Gets the tool item at @index in group.
 *
 * Returns: (transfer none): the #GtkToolItem at index
 *
 * Since: 2.20
 */
GtkToolItem*
gtk_tool_item_group_get_nth_item (GtkToolItemGroup *group,
                                  guint             index)
{
  GtkToolItemGroupChild *child;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), NULL);

  child = g_list_nth_data (group->priv->children, index);

  return child != NULL ? child->item : NULL;
}

/**
 * gtk_tool_item_group_get_drop_item:
 * @group: a #GtkToolItemGroup
 * @x: the x position
 * @y: the y position
 *
 * Gets the tool item at position (x, y).
 *
 * Returns: (transfer none): the #GtkToolItem at position (x, y)
 *
 * Since: 2.20
 */
GtkToolItem*
gtk_tool_item_group_get_drop_item (GtkToolItemGroup *group,
                                   gint              x,
                                   gint              y)
{
  GtkAllocation *allocation;
  GtkOrientation orientation;
  GList *it;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM_GROUP (group), NULL);

  allocation = &GTK_WIDGET (group)->allocation;
  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));

  g_return_val_if_fail (x >= 0 && x < allocation->width, NULL);
  g_return_val_if_fail (y >= 0 && y < allocation->height, NULL);

  for (it = group->priv->children; it != NULL; it = it->next)
    {
      GtkToolItemGroupChild *child = it->data;
      GtkToolItem *item = child->item;
      gint x0, y0;

      if (!item || !gtk_tool_item_group_is_item_visible (group, child))
        continue;

      allocation = &GTK_WIDGET (item)->allocation;

      x0 = x - allocation->x;
      y0 = y - allocation->y;

      if (x0 >= 0 && x0 < allocation->width &&
          y0 >= 0 && y0 < allocation->height)
        return item;
    }

  return NULL;
}

void
_gtk_tool_item_group_item_size_request (GtkToolItemGroup *group,
                                        GtkRequisition   *item_size,
                                        gboolean          homogeneous_only,
                                        gint             *requested_rows)
{
  GtkRequisition child_requisition;
  GList *it;
  gint rows = 0;
  gboolean new_row = TRUE;
  GtkOrientation orientation;
  GtkToolbarStyle style;

  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (group));
  g_return_if_fail (NULL != item_size);

  orientation = gtk_tool_shell_get_orientation (GTK_TOOL_SHELL (group));
  style = gtk_tool_shell_get_style (GTK_TOOL_SHELL (group));

  item_size->width = item_size->height = 0;

  for (it = group->priv->children; it != NULL; it = it->next)
    {
      GtkToolItemGroupChild *child = it->data;

      if (!gtk_tool_item_group_is_item_visible (group, child))
        continue;

      if (child->new_row || new_row)
        {
          rows++;
          new_row = FALSE;
        }

      if (!child->homogeneous && child->expand)
          new_row = TRUE;

      gtk_widget_size_request (GTK_WIDGET (child->item), &child_requisition);

      if (!homogeneous_only || child->homogeneous)
        item_size->width = MAX (item_size->width, child_requisition.width);
      item_size->height = MAX (item_size->height, child_requisition.height);
    }

  if (requested_rows)
    *requested_rows = rows;
}

void
_gtk_tool_item_group_paint (GtkToolItemGroup *group,
                            cairo_t          *cr)
{
  GtkWidget *widget = GTK_WIDGET (group);
  GtkToolItemGroupPrivate* priv = group->priv;

  gdk_cairo_set_source_pixmap (cr, widget->window,
                               widget->allocation.x,
                               widget->allocation.y);

  if (priv->animation_timeout)
    {
      GtkOrientation orientation = gtk_tool_item_group_get_orientation (GTK_TOOL_SHELL (group));
      cairo_pattern_t *mask;
      gdouble v0, v1;

      if (GTK_ORIENTATION_VERTICAL == orientation)
        v1 = widget->allocation.height;
      else
        v1 = widget->allocation.width;

      v0 = v1 - 256;

      if (!gtk_widget_get_visible (priv->header))
        v0 = MAX (v0, 0);
      else if (GTK_ORIENTATION_VERTICAL == orientation)
        v0 = MAX (v0, priv->header->allocation.height);
      else
        v0 = MAX (v0, priv->header->allocation.width);

      v1 = MIN (v0 + 256, v1);

      if (GTK_ORIENTATION_VERTICAL == orientation)
        {
          v0 += widget->allocation.y;
          v1 += widget->allocation.y;

          mask = cairo_pattern_create_linear (0.0, v0, 0.0, v1);
        }
      else
        {
          v0 += widget->allocation.x;
          v1 += widget->allocation.x;

          mask = cairo_pattern_create_linear (v0, 0.0, v1, 0.0);
        }

      cairo_pattern_add_color_stop_rgba (mask, 0.00, 0.0, 0.0, 0.0, 1.00);
      cairo_pattern_add_color_stop_rgba (mask, 0.25, 0.0, 0.0, 0.0, 0.25);
      cairo_pattern_add_color_stop_rgba (mask, 0.50, 0.0, 0.0, 0.0, 0.10);
      cairo_pattern_add_color_stop_rgba (mask, 0.75, 0.0, 0.0, 0.0, 0.01);
      cairo_pattern_add_color_stop_rgba (mask, 1.00, 0.0, 0.0, 0.0, 0.00);

      cairo_mask (cr, mask);
      cairo_pattern_destroy (mask);
    }
  else
    cairo_paint (cr);
}

gint
_gtk_tool_item_group_get_size_for_limit (GtkToolItemGroup *group,
                                         gint              limit,
                                         gboolean          vertical,
                                         gboolean          animation)
{
  GtkRequisition requisition;
  GtkToolItemGroupPrivate* priv = group->priv;

  gtk_widget_size_request (GTK_WIDGET (group), &requisition);

  if (!priv->collapsed || priv->animation_timeout)
    {
      GtkAllocation allocation = { 0, 0, requisition.width, requisition.height };
      GtkRequisition inquery;

      if (vertical)
        allocation.width = limit;
      else
        allocation.height = limit;

      gtk_tool_item_group_real_size_query (GTK_WIDGET (group),
                                           &allocation, &inquery);

      if (vertical)
        inquery.height -= requisition.height;
      else
        inquery.width -= requisition.width;

      if (priv->animation_timeout && animation)
        {
          gint64 timestamp = gtk_tool_item_group_get_animation_timestamp (group);

          timestamp = MIN (timestamp, ANIMATION_DURATION);

          if (priv->collapsed)
            timestamp = ANIMATION_DURATION - timestamp;

          if (vertical)
            {
              inquery.height *= timestamp;
              inquery.height /= ANIMATION_DURATION;
            }
          else
            {
              inquery.width *= timestamp;
              inquery.width /= ANIMATION_DURATION;
            }
        }

      if (vertical)
        requisition.height += inquery.height;
      else
        requisition.width += inquery.width;
    }

  return (vertical ? requisition.height : requisition.width);
}

gint
_gtk_tool_item_group_get_height_for_width (GtkToolItemGroup *group,
                                           gint              width)
{
  return _gtk_tool_item_group_get_size_for_limit (group, width, TRUE, group->priv->animation);
}

gint
_gtk_tool_item_group_get_width_for_height (GtkToolItemGroup *group,
                                           gint              height)
{
  return _gtk_tool_item_group_get_size_for_limit (group, height, FALSE, TRUE);
}

static void
gtk_tool_palette_reconfigured_foreach_item (GtkWidget *child,
                                            gpointer   data)
{
  if (GTK_IS_TOOL_ITEM (child))
    gtk_tool_item_toolbar_reconfigured (GTK_TOOL_ITEM (child));
}


void
_gtk_tool_item_group_palette_reconfigured (GtkToolItemGroup *group)
{
  gtk_container_foreach (GTK_CONTAINER (group),
                         gtk_tool_palette_reconfigured_foreach_item,
                         NULL);

  gtk_tool_item_group_header_adjust_style (group);
}


#define __GTK_TOOL_ITEM_GROUP_C__
#include "gtkaliasdef.c"
