/* gtktooltip.c
 *
 * Copyright (C) 2006-2007 Imendio AB
 * Contact: Kristian Rietveld <kris@imendio.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "gtktooltip.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkmain.h"
#include "gtklabel.h"
#include "gtkimage.h"
#include "gtkhbox.h"
#include "gtkalignment.h"

#include "gtkalias.h"

#undef DEBUG_TOOLTIP


#define GTK_TOOLTIP_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOLTIP, GtkTooltipClass))
#define GTK_IS_TOOLTIP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TOOLTIP))
#define GTK_TOOLTIP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TOOLTIP, GtkTooltipClass))

typedef struct _GtkTooltipClass   GtkTooltipClass;

struct _GtkTooltip
{
  GObject parent_instance;

  GtkWidget *window;
  GtkWidget *alignment;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *custom_widget;

  GtkWindow *current_window;
  GtkWidget *keyboard_widget;

  GtkWidget *tooltip_widget;
  GdkWindow *toplevel_window;

  gdouble last_x;
  gdouble last_y;
  GdkWindow *last_window;

  guint timeout_id;
  guint browse_mode_timeout_id;

  GdkRectangle tip_area;

  guint browse_mode_enabled : 1;
  guint keyboard_mode_enabled : 1;
  guint tip_area_set : 1;
  guint custom_was_reset : 1;
};

struct _GtkTooltipClass
{
  GObjectClass parent_class;
};

#define GTK_TOOLTIP_VISIBLE(tooltip) ((tooltip)->current_window && gtk_widget_get_visible (GTK_WIDGET((tooltip)->current_window)))


static void       gtk_tooltip_class_init           (GtkTooltipClass *klass);
static void       gtk_tooltip_init                 (GtkTooltip      *tooltip);
static void       gtk_tooltip_dispose              (GObject         *object);

static void       gtk_tooltip_window_style_set     (GtkTooltip      *tooltip);
static gboolean   gtk_tooltip_paint_window         (GtkTooltip      *tooltip);
static void       gtk_tooltip_window_hide          (GtkWidget       *widget,
						    gpointer         user_data);
static void       gtk_tooltip_display_closed       (GdkDisplay      *display,
						    gboolean         was_error,
						    GtkTooltip      *tooltip);
static void       gtk_tooltip_set_last_window      (GtkTooltip      *tooltip,
						    GdkWindow       *window);
static void       update_shape                     (GtkTooltip      *tooltip);


G_DEFINE_TYPE (GtkTooltip, gtk_tooltip, G_TYPE_OBJECT);

static void
gtk_tooltip_class_init (GtkTooltipClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_tooltip_dispose;
}

static void
on_composited_changed (GtkWidget  *window,
                       GtkTooltip *tooltip)
{
  update_shape (tooltip);
}

static void
on_screen_changed (GtkWidget  *window,
                   GdkScreen  *previous,
                   GtkTooltip *tooltip)
{
  GdkScreen *screen;
  GdkColormap *cmap;

  screen = gtk_widget_get_screen (window);

  cmap = NULL;
  if (gdk_screen_is_composited (screen))
    cmap = gdk_screen_get_rgba_colormap (screen);
  if (cmap == NULL)
    cmap = gdk_screen_get_rgb_colormap (screen);

  gtk_widget_set_colormap (window, cmap);
}

static void
on_realized (GtkWidget  *window,
             GtkTooltip *tooltip)
{
  update_shape (tooltip);
}

static void
gtk_tooltip_init (GtkTooltip *tooltip)
{
  tooltip->timeout_id = 0;
  tooltip->browse_mode_timeout_id = 0;

  tooltip->browse_mode_enabled = FALSE;
  tooltip->keyboard_mode_enabled = FALSE;

  tooltip->current_window = NULL;
  tooltip->keyboard_widget = NULL;

  tooltip->tooltip_widget = NULL;
  tooltip->toplevel_window = NULL;

  tooltip->last_window = NULL;

  tooltip->window = g_object_ref (gtk_window_new (GTK_WINDOW_POPUP));

  on_screen_changed (tooltip->window, NULL, tooltip);

  gtk_window_set_type_hint (GTK_WINDOW (tooltip->window),
			    GDK_WINDOW_TYPE_HINT_TOOLTIP);

  gtk_widget_set_app_paintable (tooltip->window, TRUE);
  gtk_window_set_resizable (GTK_WINDOW (tooltip->window), FALSE);
  gtk_widget_set_name (tooltip->window, "gtk-tooltip");
  g_signal_connect (tooltip->window, "hide",
		    G_CALLBACK (gtk_tooltip_window_hide), tooltip);

  tooltip->alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (tooltip->alignment),
			     tooltip->window->style->ythickness,
			     tooltip->window->style->ythickness,
			     tooltip->window->style->xthickness,
			     tooltip->window->style->xthickness);
  gtk_container_add (GTK_CONTAINER (tooltip->window), tooltip->alignment);
  gtk_widget_show (tooltip->alignment);

  g_signal_connect_swapped (tooltip->window, "style-set",
			    G_CALLBACK (gtk_tooltip_window_style_set), tooltip);
  g_signal_connect_swapped (tooltip->window, "expose-event",
			    G_CALLBACK (gtk_tooltip_paint_window), tooltip);

  tooltip->box = gtk_hbox_new (FALSE, tooltip->window->style->xthickness);
  gtk_container_add (GTK_CONTAINER (tooltip->alignment), tooltip->box);
  gtk_widget_show (tooltip->box);

  tooltip->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (tooltip->box), tooltip->image,
		      FALSE, FALSE, 0);

  tooltip->label = gtk_label_new ("");
  gtk_label_set_line_wrap (GTK_LABEL (tooltip->label), TRUE);
  gtk_box_pack_start (GTK_BOX (tooltip->box), tooltip->label,
		      FALSE, FALSE, 0);

  g_signal_connect (tooltip->window, "composited-changed",
                    G_CALLBACK (on_composited_changed), tooltip);
  g_signal_connect (tooltip->window, "screen-changed",
                    G_CALLBACK (on_screen_changed), tooltip);
  g_signal_connect (tooltip->window, "realize",
                    G_CALLBACK (on_realized), tooltip);

  tooltip->custom_widget = NULL;
}

static void
gtk_tooltip_dispose (GObject *object)
{
  GtkTooltip *tooltip = GTK_TOOLTIP (object);

  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  if (tooltip->browse_mode_timeout_id)
    {
      g_source_remove (tooltip->browse_mode_timeout_id);
      tooltip->browse_mode_timeout_id = 0;
    }

  gtk_tooltip_set_custom (tooltip, NULL);
  gtk_tooltip_set_last_window (tooltip, NULL);

  if (tooltip->window)
    {
      GdkDisplay *display;

      display = gtk_widget_get_display (tooltip->window);
      g_signal_handlers_disconnect_by_func (display,
					    gtk_tooltip_display_closed,
					    tooltip);
      gtk_widget_destroy (tooltip->window);
      tooltip->window = NULL;
    }

  G_OBJECT_CLASS (gtk_tooltip_parent_class)->dispose (object);
}

/* public API */

/**
 * gtk_tooltip_set_markup:
 * @tooltip: a #GtkTooltip
 * @markup: (allow-none): a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>) or %NULL
 *
 * Sets the text of the tooltip to be @markup, which is marked up
 * with the <link
 * linkend="PangoMarkupFormat">Pango text markup language</link>.
 * If @markup is %NULL, the label will be hidden.
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_markup (GtkTooltip  *tooltip,
			const gchar *markup)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_label_set_markup (GTK_LABEL (tooltip->label), markup);

  if (markup)
    gtk_widget_show (tooltip->label);
  else
    gtk_widget_hide (tooltip->label);
}

/**
 * gtk_tooltip_set_text:
 * @tooltip: a #GtkTooltip
 * @text: (allow-none): a text string or %NULL
 *
 * Sets the text of the tooltip to be @text. If @text is %NULL, the label
 * will be hidden. See also gtk_tooltip_set_markup().
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_text (GtkTooltip  *tooltip,
                      const gchar *text)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_label_set_text (GTK_LABEL (tooltip->label), text);

  if (text)
    gtk_widget_show (tooltip->label);
  else
    gtk_widget_hide (tooltip->label);
}

/**
 * gtk_tooltip_set_icon:
 * @tooltip: a #GtkTooltip
 * @pixbuf: (allow-none): a #GdkPixbuf, or %NULL
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * @pixbuf.  If @pixbuf is %NULL, the image will be hidden.
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_icon (GtkTooltip *tooltip,
		      GdkPixbuf  *pixbuf)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  if (pixbuf)
    g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  gtk_image_set_from_pixbuf (GTK_IMAGE (tooltip->image), pixbuf);

  if (pixbuf)
    gtk_widget_show (tooltip->image);
  else
    gtk_widget_hide (tooltip->image);
}

/**
 * gtk_tooltip_set_icon_from_stock:
 * @tooltip: a #GtkTooltip
 * @stock_id: (allow-none): a stock id, or %NULL
 * @size: (type int): a stock icon size
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * the stock item indicated by @stock_id with the size indicated
 * by @size.  If @stock_id is %NULL, the image will be hidden.
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_icon_from_stock (GtkTooltip  *tooltip,
				 const gchar *stock_id,
				 GtkIconSize  size)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_image_set_from_stock (GTK_IMAGE (tooltip->image), stock_id, size);

  if (stock_id)
    gtk_widget_show (tooltip->image);
  else
    gtk_widget_hide (tooltip->image);
}

/**
 * gtk_tooltip_set_icon_from_icon_name:
 * @tooltip: a #GtkTooltip
 * @icon_name: (allow-none): an icon name, or %NULL
 * @size: (type int): a stock icon size
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * the icon indicated by @icon_name with the size indicated
 * by @size.  If @icon_name is %NULL, the image will be hidden.
 *
 * Since: 2.14
 */
void
gtk_tooltip_set_icon_from_icon_name (GtkTooltip  *tooltip,
				     const gchar *icon_name,
				     GtkIconSize  size)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_image_set_from_icon_name (GTK_IMAGE (tooltip->image), icon_name, size);

  if (icon_name)
    gtk_widget_show (tooltip->image);
  else
    gtk_widget_hide (tooltip->image);
}

/**
 * gtk_tooltip_set_icon_from_gicon:
 * @tooltip: a #GtkTooltip
 * @gicon: (allow-none): a #GIcon representing the icon, or %NULL
 * @size: (type int): a stock icon size
 *
 * Sets the icon of the tooltip (which is in front of the text)
 * to be the icon indicated by @gicon with the size indicated
 * by @size. If @gicon is %NULL, the image will be hidden.
 *
 * Since: 2.20
 */
void
gtk_tooltip_set_icon_from_gicon (GtkTooltip  *tooltip,
				 GIcon       *gicon,
				 GtkIconSize  size)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_image_set_from_gicon (GTK_IMAGE (tooltip->image), gicon, size);

  if (gicon)
    gtk_widget_show (tooltip->image);
  else
    gtk_widget_hide (tooltip->image);
}

/**
 * gtk_tooltip_set_custom:
 * @tooltip: a #GtkTooltip
 * @custom_widget: (allow-none): a #GtkWidget, or %NULL to unset the old custom widget.
 *
 * Replaces the widget packed into the tooltip with
 * @custom_widget. @custom_widget does not get destroyed when the tooltip goes
 * away.
 * By default a box with a #GtkImage and #GtkLabel is embedded in 
 * the tooltip, which can be configured using gtk_tooltip_set_markup() 
 * and gtk_tooltip_set_icon().

 *
 * Since: 2.12
 */
void
gtk_tooltip_set_custom (GtkTooltip *tooltip,
			GtkWidget  *custom_widget)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  if (custom_widget)
    g_return_if_fail (GTK_IS_WIDGET (custom_widget));

  /* The custom widget has been updated from the query-tooltip
   * callback, so we do not want to reset the custom widget later on.
   */
  tooltip->custom_was_reset = TRUE;

  /* No need to do anything if the custom widget stays the same */
  if (tooltip->custom_widget == custom_widget)
    return;

  if (tooltip->custom_widget)
    {
      GtkWidget *custom = tooltip->custom_widget;
      /* Note: We must reset tooltip->custom_widget first, 
       * since gtk_container_remove() will recurse into 
       * gtk_tooltip_set_custom()
       */
      tooltip->custom_widget = NULL;
      gtk_container_remove (GTK_CONTAINER (tooltip->box), custom);
      g_object_unref (custom);
    }

  if (custom_widget)
    {
      tooltip->custom_widget = g_object_ref (custom_widget);

      gtk_container_add (GTK_CONTAINER (tooltip->box), custom_widget);
      gtk_widget_show (custom_widget);
    }
}

/**
 * gtk_tooltip_set_tip_area:
 * @tooltip: a #GtkTooltip
 * @rect: a #GdkRectangle
 *
 * Sets the area of the widget, where the contents of this tooltip apply,
 * to be @rect (in widget coordinates).  This is especially useful for
 * properly setting tooltips on #GtkTreeView rows and cells, #GtkIconViews,
 * etc.
 *
 * For setting tooltips on #GtkTreeView, please refer to the convenience
 * functions for this: gtk_tree_view_set_tooltip_row() and
 * gtk_tree_view_set_tooltip_cell().
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_tip_area (GtkTooltip         *tooltip,
			  const GdkRectangle *rect)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  if (!rect)
    tooltip->tip_area_set = FALSE;
  else
    {
      tooltip->tip_area_set = TRUE;
      tooltip->tip_area = *rect;
    }
}

/**
 * gtk_tooltip_trigger_tooltip_query:
 * @display: a #GdkDisplay
 *
 * Triggers a new tooltip query on @display, in order to update the current
 * visible tooltip, or to show/hide the current tooltip.  This function is
 * useful to call when, for example, the state of the widget changed by a
 * key press.
 *
 * Since: 2.12
 */
void
gtk_tooltip_trigger_tooltip_query (GdkDisplay *display)
{
  gint x, y;
  GdkWindow *window;
  GdkEvent event;

  /* Trigger logic as if the mouse moved */
  window = gdk_display_get_window_at_pointer (display, &x, &y);
  if (!window)
    return;

  event.type = GDK_MOTION_NOTIFY;
  event.motion.window = window;
  event.motion.x = x;
  event.motion.y = y;
  event.motion.is_hint = FALSE;

  gdk_window_get_root_coords (window, x, y, &x, &y);
  event.motion.x_root = x;
  event.motion.y_root = y;

  _gtk_tooltip_handle_event (&event);
}

/* private functions */

static void
gtk_tooltip_reset (GtkTooltip *tooltip)
{
  gtk_tooltip_set_markup (tooltip, NULL);
  gtk_tooltip_set_icon (tooltip, NULL);
  gtk_tooltip_set_tip_area (tooltip, NULL);

  /* See if the custom widget is again set from the query-tooltip
   * callback.
   */
  tooltip->custom_was_reset = FALSE;
}

static void
gtk_tooltip_window_style_set (GtkTooltip *tooltip)
{
  gtk_alignment_set_padding (GTK_ALIGNMENT (tooltip->alignment),
			     tooltip->window->style->ythickness,
			     tooltip->window->style->ythickness,
			     tooltip->window->style->xthickness,
			     tooltip->window->style->xthickness);
  gtk_box_set_spacing (GTK_BOX (tooltip->box),
		       tooltip->window->style->xthickness);

  gtk_widget_queue_draw (tooltip->window);
}

static void
draw_round_rect (cairo_t *cr,
                 gdouble  aspect,
                 gdouble  x,
                 gdouble  y,
                 gdouble  corner_radius,
                 gdouble  width,
                 gdouble  height)
{
  gdouble radius = corner_radius / aspect;

  cairo_move_to (cr, x + radius, y);

  /* top-right, left of the corner */
  cairo_line_to (cr, x + width - radius, y);

  /* top-right, below the corner */
  cairo_arc (cr,
             x + width - radius, y + radius, radius,
             -90.0f * G_PI / 180.0f, 0.0f * G_PI / 180.0f);

  /* bottom-right, above the corner */
  cairo_line_to (cr, x + width, y + height - radius);

  /* bottom-right, left of the corner */
  cairo_arc (cr,
             x + width - radius, y + height - radius, radius,
             0.0f * G_PI / 180.0f, 90.0f * G_PI / 180.0f);

  /* bottom-left, right of the corner */
  cairo_line_to (cr, x + radius, y + height);

  /* bottom-left, above the corner */
  cairo_arc (cr,
             x + radius, y + height - radius, radius,
             90.0f * G_PI / 180.0f, 180.0f * G_PI / 180.0f);

  /* top-left, below the corner */
  cairo_line_to (cr, x, y + radius);

  /* top-left, right of the corner */
  cairo_arc (cr,
             x + radius, y + radius, radius,
             180.0f * G_PI / 180.0f, 270.0f * G_PI / 180.0f);

  cairo_close_path (cr);
}

static void
fill_background (GtkWidget  *widget,
                 cairo_t    *cr,
                 GdkColor   *bg_color,
                 GdkColor   *border_color,
                 guchar      alpha)
{
  gint tooltip_radius;

  if (!gtk_widget_is_composited (widget))
    alpha = 255;

  gtk_widget_style_get (widget,
                        "tooltip-radius", &tooltip_radius,
                        NULL);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  draw_round_rect (cr,
                   1.0, 0.5, 0.5, tooltip_radius,
                   widget->allocation.width - 1,
                   widget->allocation.height - 1);

  cairo_set_source_rgba (cr,
                         (float) bg_color->red / 65535.0,
                         (float) bg_color->green / 65535.0,
                         (float) bg_color->blue / 65535.0,
                         (float) alpha / 255.0);
  cairo_fill_preserve (cr);

  cairo_set_source_rgba (cr,
                         (float) border_color->red / 65535.0,
                         (float) border_color->green / 65535.0,
                         (float) border_color->blue / 65535.0,
                         (float) alpha / 255.0);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);
}

static void
update_shape (GtkTooltip *tooltip)
{
  GdkBitmap *mask;
  cairo_t *cr;
  gint width, height, tooltip_radius;

  gtk_widget_style_get (tooltip->window,
                        "tooltip-radius", &tooltip_radius,
                        NULL);

  if (tooltip_radius == 0 ||
      gtk_widget_is_composited (tooltip->window))
    {
      gtk_widget_shape_combine_mask (tooltip->window, NULL, 0, 0);
      return;
    }

  gtk_window_get_size (GTK_WINDOW (tooltip->window), &width, &height);
  mask = (GdkBitmap *) gdk_pixmap_new (NULL, width, height, 1);
  cr = gdk_cairo_create (mask);

  fill_background (tooltip->window, cr,
                   &tooltip->window->style->black,
                   &tooltip->window->style->black,
                   255);
  gtk_widget_shape_combine_mask (tooltip->window, mask, 0, 0);

  cairo_destroy (cr);
  g_object_unref (mask);
}

static gboolean
gtk_tooltip_paint_window (GtkTooltip *tooltip)
{
  guchar tooltip_alpha;
  gint tooltip_radius;

  gtk_widget_style_get (tooltip->window,
                        "tooltip-alpha", &tooltip_alpha,
                        "tooltip-radius", &tooltip_radius,
                        NULL);

  if (tooltip_alpha != 255 || tooltip_radius != 0)
    {
      cairo_t *cr;

      cr = gdk_cairo_create (tooltip->window->window);
      fill_background (tooltip->window, cr,
                       &tooltip->window->style->bg [GTK_STATE_NORMAL],
                       &tooltip->window->style->bg [GTK_STATE_SELECTED],
                       tooltip_alpha);
      cairo_destroy (cr);

      update_shape (tooltip);
    }
  else
    {
      gtk_paint_flat_box (tooltip->window->style,
                          tooltip->window->window,
                          GTK_STATE_NORMAL,
                          GTK_SHADOW_OUT,
                          NULL,
                          tooltip->window,
                          "tooltip",
                          0, 0,
                          tooltip->window->allocation.width,
                          tooltip->window->allocation.height);
    }

  return FALSE;
}

static void
gtk_tooltip_window_hide (GtkWidget *widget,
			 gpointer   user_data)
{
  GtkTooltip *tooltip = GTK_TOOLTIP (user_data);

  gtk_tooltip_set_custom (tooltip, NULL);
}

/* event handling, etc */

struct ChildLocation
{
  GtkWidget *child;
  GtkWidget *container;

  gint x;
  gint y;
};

static void
child_location_foreach (GtkWidget *child,
			gpointer   data)
{
  gint x, y;
  struct ChildLocation *child_loc = data;

  /* Ignore invisible widgets */
  if (!gtk_widget_is_drawable (child))
    return;

  x = 0;
  y = 0;

  /* (child_loc->x, child_loc->y) are relative to
   * child_loc->container's allocation.
   */

  if (!child_loc->child &&
      gtk_widget_translate_coordinates (child_loc->container, child,
					child_loc->x, child_loc->y,
					&x, &y))
    {
#ifdef DEBUG_TOOLTIP
      g_print ("candidate: %s  alloc=[(%d,%d)  %dx%d]     (%d, %d)->(%d, %d)\n",
	       gtk_widget_get_name (child),
	       child->allocation.x,
	       child->allocation.y,
	       child->allocation.width,
	       child->allocation.height,
	       child_loc->x, child_loc->y,
	       x, y);
#endif /* DEBUG_TOOLTIP */

      /* (x, y) relative to child's allocation. */
      if (x >= 0 && x < child->allocation.width
	  && y >= 0 && y < child->allocation.height)
        {
	  if (GTK_IS_CONTAINER (child))
	    {
	      struct ChildLocation tmp = { NULL, NULL, 0, 0 };

	      /* Take (x, y) relative the child's allocation and
	       * recurse.
	       */
	      tmp.x = x;
	      tmp.y = y;
	      tmp.container = child;

	      gtk_container_forall (GTK_CONTAINER (child),
				    child_location_foreach, &tmp);

	      if (tmp.child)
		child_loc->child = tmp.child;
	      else
		child_loc->child = child;
	    }
	  else
	    child_loc->child = child;
	}
    }
}

/* Translates coordinates from dest_widget->window relative (src_x, src_y),
 * to allocation relative (dest_x, dest_y) of dest_widget.
 */
static void
window_to_alloc (GtkWidget *dest_widget,
		 gint       src_x,
		 gint       src_y,
		 gint      *dest_x,
		 gint      *dest_y)
{
  /* Translate from window relative to allocation relative */
  if (gtk_widget_get_has_window (dest_widget) && dest_widget->parent)
    {
      gint wx, wy;
      gdk_window_get_position (dest_widget->window, &wx, &wy);

      /* Offset coordinates if widget->window is smaller than
       * widget->allocation.
       */
      src_x += wx - dest_widget->allocation.x;
      src_y += wy - dest_widget->allocation.y;
    }
  else
    {
      src_x -= dest_widget->allocation.x;
      src_y -= dest_widget->allocation.y;
    }

  if (dest_x)
    *dest_x = src_x;
  if (dest_y)
    *dest_y = src_y;
}

/* Translates coordinates from window relative (x, y) to
 * allocation relative (x, y) of the returned widget.
 */
GtkWidget *
_gtk_widget_find_at_coords (GdkWindow *window,
                            gint       window_x,
                            gint       window_y,
                            gint      *widget_x,
                            gint      *widget_y)
{
  GtkWidget *event_widget;
  struct ChildLocation child_loc = { NULL, NULL, 0, 0 };

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  gdk_window_get_user_data (window, (void **)&event_widget);

  if (!event_widget)
    return NULL;

#ifdef DEBUG_TOOLTIP
  g_print ("event window %p (belonging to %p (%s))  (%d, %d)\n",
	   window, event_widget, gtk_widget_get_name (event_widget),
	   window_x, window_y);
#endif

  /* Coordinates are relative to event window */
  child_loc.x = window_x;
  child_loc.y = window_y;

  /* We go down the window hierarchy to the widget->window,
   * coordinates stay relative to the current window.
   * We end up with window == widget->window, coordinates relative to that.
   */
  while (window && window != event_widget->window)
    {
      gdouble px, py;

      gdk_window_coords_to_parent (window,
                                   child_loc.x, child_loc.y,
                                   &px, &py);
      child_loc.x = px;
      child_loc.y = py;

      window = gdk_window_get_effective_parent (window);
    }

  /* Failing to find widget->window can happen for e.g. a detached handle box;
   * chaining ::query-tooltip up to its parent probably makes little sense,
   * and users better implement tooltips on handle_box->child.
   * so we simply ignore the event for tooltips here.
   */
  if (!window)
    return NULL;

  /* Convert the window relative coordinates to allocation
   * relative coordinates.
   */
  window_to_alloc (event_widget,
		   child_loc.x, child_loc.y,
		   &child_loc.x, &child_loc.y);

  if (GTK_IS_CONTAINER (event_widget))
    {
      GtkWidget *container = event_widget;

      child_loc.container = event_widget;
      child_loc.child = NULL;

      gtk_container_forall (GTK_CONTAINER (event_widget),
			    child_location_foreach, &child_loc);

      /* Here we have a widget, with coordinates relative to
       * child_loc.container's allocation.
       */

      if (child_loc.child)
	event_widget = child_loc.child;
      else if (child_loc.container)
	event_widget = child_loc.container;

      /* Translate to event_widget's allocation */
      gtk_widget_translate_coordinates (container, event_widget,
					child_loc.x, child_loc.y,
					&child_loc.x, &child_loc.y);
    }

  /* We return (x, y) relative to the allocation of event_widget. */
  if (widget_x)
    *widget_x = child_loc.x;
  if (widget_y)
    *widget_y = child_loc.y;

  return event_widget;
}

/* Ignores (x, y) on input, translates event coordinates to
 * allocation relative (x, y) of the returned widget.
 */
static GtkWidget *
find_topmost_widget_coords_from_event (GdkEvent *event,
				       gint     *x,
				       gint     *y)
{
  gint tx, ty;
  gdouble dx, dy;
  GtkWidget *tmp;

  gdk_event_get_coords (event, &dx, &dy);

  /* Returns coordinates relative to tmp's allocation. */
  tmp = _gtk_widget_find_at_coords (event->any.window, dx, dy, &tx, &ty);

  if (!tmp)
    return NULL;

  /* Make sure the pointer can actually be on the widget returned. */
  if (tx < 0 || tx >= tmp->allocation.width ||
      ty < 0 || ty >= tmp->allocation.height)
    return NULL;

  if (x)
    *x = tx;
  if (y)
    *y = ty;

  return tmp;
}

static gint
tooltip_browse_mode_expired (gpointer data)
{
  GtkTooltip *tooltip;

  tooltip = GTK_TOOLTIP (data);

  tooltip->browse_mode_enabled = FALSE;
  tooltip->browse_mode_timeout_id = 0;

  /* destroy tooltip */
  g_object_set_data (G_OBJECT (gtk_widget_get_display (tooltip->window)),
		     "gdk-display-current-tooltip", NULL);

  return FALSE;
}

static void
gtk_tooltip_display_closed (GdkDisplay *display,
			    gboolean    was_error,
			    GtkTooltip *tooltip)
{
  g_object_set_data (G_OBJECT (display), "gdk-display-current-tooltip", NULL);
}

static void
gtk_tooltip_set_last_window (GtkTooltip *tooltip,
			     GdkWindow  *window)
{
  if (tooltip->last_window == window)
    return;

  if (tooltip->last_window)
    g_object_remove_weak_pointer (G_OBJECT (tooltip->last_window),
				  (gpointer *) &tooltip->last_window);

  tooltip->last_window = window;

  if (window)
    g_object_add_weak_pointer (G_OBJECT (tooltip->last_window),
			       (gpointer *) &tooltip->last_window);
}

static gboolean
gtk_tooltip_run_requery (GtkWidget  **widget,
			 GtkTooltip  *tooltip,
			 gint        *x,
			 gint        *y)
{
  gboolean has_tooltip = FALSE;
  gboolean return_value = FALSE;

  gtk_tooltip_reset (tooltip);

  do
    {
      g_object_get (*widget,
		    "has-tooltip", &has_tooltip,
		    NULL);

      if (has_tooltip)
	g_signal_emit_by_name (*widget,
			       "query-tooltip",
			       *x, *y,
			       tooltip->keyboard_mode_enabled,
			       tooltip,
			       &return_value);

      if (!return_value)
        {
	  GtkWidget *parent = (*widget)->parent;

	  if (parent)
	    gtk_widget_translate_coordinates (*widget, parent, *x, *y, x, y);

	  *widget = parent;
	}
      else
	break;
    }
  while (*widget);

  /* If the custom widget was not reset in the query-tooltip
   * callback, we clear it here.
   */
  if (!tooltip->custom_was_reset)
    gtk_tooltip_set_custom (tooltip, NULL);

  return return_value;
}

static void
get_bounding_box (GtkWidget    *widget,
                  GdkRectangle *bounds)
{
  GdkWindow *window;
  gint x, y;
  gint w, h;
  gint x1, y1;
  gint x2, y2;
  gint x3, y3;
  gint x4, y4;

  window = gtk_widget_get_parent_window (widget);

  x = widget->allocation.x;
  y = widget->allocation.y;
  w = widget->allocation.width;
  h = widget->allocation.height;

  gdk_window_get_root_coords (window, x, y, &x1, &y1);
  gdk_window_get_root_coords (window, x + w, y, &x2, &y2);
  gdk_window_get_root_coords (window, x, y + h, &x3, &y3);
  gdk_window_get_root_coords (window, x + w, y + h, &x4, &y4);

#define MIN4(a,b,c,d) MIN(MIN(a,b),MIN(c,d))
#define MAX4(a,b,c,d) MAX(MAX(a,b),MAX(c,d))

  bounds->x = floor (MIN4 (x1, x2, x3, x4));
  bounds->y = floor (MIN4 (y1, y2, y3, y4));
  bounds->width = ceil (MAX4 (x1, x2, x3, x4)) - bounds->x;
  bounds->height = ceil (MAX4 (y1, y2, y3, y4)) - bounds->y;
}

static void
gtk_tooltip_position (GtkTooltip *tooltip,
		      GdkDisplay *display,
		      GtkWidget  *new_tooltip_widget)
{
  gint x, y;
  GdkScreen *screen;

  tooltip->tooltip_widget = new_tooltip_widget;

  /* Position the tooltip */
  /* FIXME: should we swap this when RTL is enabled? */
  if (tooltip->keyboard_mode_enabled)
    {
      GdkRectangle bounds;

      get_bounding_box (new_tooltip_widget, &bounds);

      /* For keyboard mode we position the tooltip below the widget,
       * right of the center of the widget.
       */
      x = bounds.x + bounds.width / 2;
      y = bounds.y + bounds.height + 4;
    }
  else
    {
      guint cursor_size;

      x = tooltip->last_x;
      y = tooltip->last_y;

      /* For mouse mode, we position the tooltip right of the cursor,
       * a little below the cursor's center.
       */
      cursor_size = gdk_display_get_default_cursor_size (display);
      x += cursor_size / 2;
      y += cursor_size / 2;
    }

  screen = gtk_widget_get_screen (new_tooltip_widget);

  /* Show it */
  if (tooltip->current_window)
    {
      gint monitor_num;
      GdkRectangle monitor;
      GtkRequisition requisition;

      gtk_widget_size_request (GTK_WIDGET (tooltip->current_window),
                               &requisition);

      monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);
      gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

      if (x + requisition.width > monitor.x + monitor.width)
        x -= x - (monitor.x + monitor.width) + requisition.width;
      else if (x < monitor.x)
        x = monitor.x;

      if (y + requisition.height > monitor.y + monitor.height)
        y -= y - (monitor.y + monitor.height) + requisition.height;
  
      if (!tooltip->keyboard_mode_enabled)
        {
          /* don't pop up under the pointer */
          if (x <= tooltip->last_x && tooltip->last_x < x + requisition.width &&
              y <= tooltip->last_y && tooltip->last_y < y + requisition.height)
            y = tooltip->last_y - requisition.height - 2;
        }
  
      gtk_window_move (GTK_WINDOW (tooltip->current_window), x, y);
      gtk_widget_show (GTK_WIDGET (tooltip->current_window));
    }
}

static void
gtk_tooltip_show_tooltip (GdkDisplay *display)
{
  gint x, y;
  GdkScreen *screen;

  GdkWindow *window;
  GtkWidget *tooltip_widget;
  GtkWidget *pointer_widget;
  GtkTooltip *tooltip;
  gboolean has_tooltip;
  gboolean return_value = FALSE;

  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (tooltip->keyboard_mode_enabled)
    {
      x = y = -1;
      pointer_widget = tooltip_widget = tooltip->keyboard_widget;
    }
  else
    {
      gint tx, ty;

      window = tooltip->last_window;

      if (!GDK_IS_WINDOW (window))
	return;

      gdk_window_get_pointer (window, &x, &y, NULL);

      gdk_window_get_root_coords (window, x, y, &tx, &ty);
      tooltip->last_x = tx;
      tooltip->last_y = ty;

      pointer_widget = tooltip_widget = _gtk_widget_find_at_coords (window,
                                                                    x, y,
                                                                    &x, &y);
    }

  if (!tooltip_widget)
    return;

  g_object_get (tooltip_widget, "has-tooltip", &has_tooltip, NULL);

  return_value = gtk_tooltip_run_requery (&tooltip_widget, tooltip, &x, &y);
  if (!return_value)
    return;

  if (!tooltip->current_window)
    {
      if (gtk_widget_get_tooltip_window (tooltip_widget))
	tooltip->current_window = gtk_widget_get_tooltip_window (tooltip_widget);
      else
	tooltip->current_window = GTK_WINDOW (GTK_TOOLTIP (tooltip)->window);
    }

  screen = gtk_widget_get_screen (tooltip_widget);

  /* FIXME: should use tooltip->current_window iso tooltip->window */
  if (screen != gtk_widget_get_screen (tooltip->window))
    {
      g_signal_handlers_disconnect_by_func (display,
					    gtk_tooltip_display_closed,
					    tooltip);

      gtk_window_set_screen (GTK_WINDOW (tooltip->window), screen);

      g_signal_connect (display, "closed",
			G_CALLBACK (gtk_tooltip_display_closed), tooltip);
    }

  gtk_tooltip_position (tooltip, display, tooltip_widget);

  /* Now a tooltip is visible again on the display, make sure browse
   * mode is enabled.
   */
  tooltip->browse_mode_enabled = TRUE;
  if (tooltip->browse_mode_timeout_id)
    {
      g_source_remove (tooltip->browse_mode_timeout_id);
      tooltip->browse_mode_timeout_id = 0;
    }
}

static void
gtk_tooltip_hide_tooltip (GtkTooltip *tooltip)
{
  if (!tooltip)
    return;

  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  if (!GTK_TOOLTIP_VISIBLE (tooltip))
    return;

  tooltip->tooltip_widget = NULL;

  if (!tooltip->keyboard_mode_enabled)
    {
      guint timeout;
      GtkSettings *settings;

      settings = gtk_widget_get_settings (GTK_WIDGET (tooltip->window));

      g_object_get (settings,
		    "gtk-tooltip-browse-mode-timeout", &timeout,
		    NULL);

      /* The tooltip is gone, after (by default, should be configurable) 500ms
       * we want to turn off browse mode
       */
      if (!tooltip->browse_mode_timeout_id)
	tooltip->browse_mode_timeout_id =
	  gdk_threads_add_timeout_full (0, timeout,
					tooltip_browse_mode_expired,
					g_object_ref (tooltip),
					g_object_unref);
    }
  else
    {
      if (tooltip->browse_mode_timeout_id)
        {
	  g_source_remove (tooltip->browse_mode_timeout_id);
	  tooltip->browse_mode_timeout_id = 0;
	}
    }

  if (tooltip->current_window)
    {
      gtk_widget_hide (GTK_WIDGET (tooltip->current_window));
      tooltip->current_window = NULL;
    }
}

static gint
tooltip_popup_timeout (gpointer data)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = GDK_DISPLAY_OBJECT (data);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  /* This usually does not happen.  However, it does occur in language
   * bindings were reference counting of objects behaves differently.
   */
  if (!tooltip)
    return FALSE;

  gtk_tooltip_show_tooltip (display);

  tooltip->timeout_id = 0;

  return FALSE;
}

static void
gtk_tooltip_start_delay (GdkDisplay *display)
{
  guint timeout;
  GtkTooltip *tooltip;
  GtkSettings *settings;

  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (!tooltip || GTK_TOOLTIP_VISIBLE (tooltip))
    return;

  if (tooltip->timeout_id)
    g_source_remove (tooltip->timeout_id);

  settings = gtk_widget_get_settings (GTK_WIDGET (tooltip->window));

  if (tooltip->browse_mode_enabled)
    g_object_get (settings, "gtk-tooltip-browse-timeout", &timeout, NULL);
  else
    g_object_get (settings, "gtk-tooltip-timeout", &timeout, NULL);

  tooltip->timeout_id = gdk_threads_add_timeout_full (0, timeout,
						      tooltip_popup_timeout,
						      g_object_ref (display),
						      g_object_unref);
}

void
_gtk_tooltip_focus_in (GtkWidget *widget)
{
  gint x, y;
  gboolean return_value = FALSE;
  GdkDisplay *display;
  GtkTooltip *tooltip;

  /* Get current tooltip for this display */
  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  /* Check if keyboard mode is enabled at this moment */
  if (!tooltip || !tooltip->keyboard_mode_enabled)
    return;

  if (tooltip->keyboard_widget)
    g_object_unref (tooltip->keyboard_widget);

  tooltip->keyboard_widget = g_object_ref (widget);

  gdk_window_get_pointer (widget->window, &x, &y, NULL);

  return_value = gtk_tooltip_run_requery (&widget, tooltip, &x, &y);
  if (!return_value)
    {
      gtk_tooltip_hide_tooltip (tooltip);
      return;
    }

  if (!tooltip->current_window)
    {
      if (gtk_widget_get_tooltip_window (widget))
	tooltip->current_window = gtk_widget_get_tooltip_window (widget);
      else
	tooltip->current_window = GTK_WINDOW (GTK_TOOLTIP (tooltip)->window);
    }

  gtk_tooltip_show_tooltip (display);
}

void
_gtk_tooltip_focus_out (GtkWidget *widget)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  /* Get current tooltip for this display */
  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (!tooltip || !tooltip->keyboard_mode_enabled)
    return;

  if (tooltip->keyboard_widget)
    {
      g_object_unref (tooltip->keyboard_widget);
      tooltip->keyboard_widget = NULL;
    }

  gtk_tooltip_hide_tooltip (tooltip);
}

void
_gtk_tooltip_toggle_keyboard_mode (GtkWidget *widget)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (!tooltip)
    {
      tooltip = g_object_new (GTK_TYPE_TOOLTIP, NULL);
      g_object_set_data_full (G_OBJECT (display),
			      "gdk-display-current-tooltip",
			      tooltip, g_object_unref);
      g_signal_connect (display, "closed",
			G_CALLBACK (gtk_tooltip_display_closed),
			tooltip);
    }

  tooltip->keyboard_mode_enabled ^= 1;

  if (tooltip->keyboard_mode_enabled)
    {
      tooltip->keyboard_widget = g_object_ref (widget);
      _gtk_tooltip_focus_in (widget);
    }
  else
    {
      if (tooltip->keyboard_widget)
        {
	  g_object_unref (tooltip->keyboard_widget);
	  tooltip->keyboard_widget = NULL;
	}

      gtk_tooltip_hide_tooltip (tooltip);
    }
}

void
_gtk_tooltip_hide (GtkWidget *widget)
{
  GtkWidget *toplevel;
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (!tooltip || !GTK_TOOLTIP_VISIBLE (tooltip) || !tooltip->tooltip_widget)
    return;

  toplevel = gtk_widget_get_toplevel (widget);

  if (widget == tooltip->tooltip_widget
      || toplevel->window == tooltip->toplevel_window)
    gtk_tooltip_hide_tooltip (tooltip);
}

static gboolean
tooltips_enabled (GdkWindow *window)
{
  gboolean enabled;
  gboolean touchscreen;
  GdkScreen *screen;
  GtkSettings *settings;

  screen = gdk_window_get_screen (window);
  settings = gtk_settings_get_for_screen (screen);

  g_object_get (settings,
		"gtk-touchscreen-mode", &touchscreen,
		"gtk-enable-tooltips", &enabled,
		NULL);

  return (!touchscreen && enabled);
}

void
_gtk_tooltip_handle_event (GdkEvent *event)
{
  gint x, y;
  gboolean return_value = FALSE;
  GtkWidget *has_tooltip_widget = NULL;
  GdkDisplay *display;
  GtkTooltip *current_tooltip;

  if (!tooltips_enabled (event->any.window))
    return;

  /* Returns coordinates relative to has_tooltip_widget's allocation. */
  has_tooltip_widget = find_topmost_widget_coords_from_event (event, &x, &y);
  display = gdk_window_get_display (event->any.window);
  current_tooltip = g_object_get_data (G_OBJECT (display),
				       "gdk-display-current-tooltip");

  if (current_tooltip)
    {
      gtk_tooltip_set_last_window (current_tooltip, event->any.window);
    }

  if (current_tooltip && current_tooltip->keyboard_mode_enabled)
    {
      has_tooltip_widget = current_tooltip->keyboard_widget;
      if (!has_tooltip_widget)
	return;

      return_value = gtk_tooltip_run_requery (&has_tooltip_widget,
					      current_tooltip,
					      &x, &y);

      if (!return_value)
	gtk_tooltip_hide_tooltip (current_tooltip);
      else
	gtk_tooltip_start_delay (display);

      return;
    }

#ifdef DEBUG_TOOLTIP
  if (has_tooltip_widget)
    g_print ("%p (%s) at (%d, %d) %dx%d     pointer: (%d, %d)\n",
	     has_tooltip_widget, gtk_widget_get_name (has_tooltip_widget),
	     has_tooltip_widget->allocation.x,
	     has_tooltip_widget->allocation.y,
	     has_tooltip_widget->allocation.width,
	     has_tooltip_widget->allocation.height,
	     x, y);
#endif /* DEBUG_TOOLTIP */

  /* Always poll for a next motion event */
  gdk_event_request_motions (&event->motion);

  /* Hide the tooltip when there's no new tooltip widget */
  if (!has_tooltip_widget)
    {
      if (current_tooltip)
	gtk_tooltip_hide_tooltip (current_tooltip);

      return;
    }

  switch (event->type)
    {
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_KEY_PRESS:
      case GDK_DRAG_ENTER:
      case GDK_GRAB_BROKEN:
	gtk_tooltip_hide_tooltip (current_tooltip);
	break;

      case GDK_MOTION_NOTIFY:
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
      case GDK_SCROLL:
	if (current_tooltip)
	  {
	    gboolean tip_area_set;
	    GdkRectangle tip_area;
	    gboolean hide_tooltip;

	    tip_area_set = current_tooltip->tip_area_set;
	    tip_area = current_tooltip->tip_area;

	    return_value = gtk_tooltip_run_requery (&has_tooltip_widget,
						    current_tooltip,
						    &x, &y);

	    /* Requested to be hidden? */
	    hide_tooltip = !return_value;

	    /* Leave notify should override the query function */
	    hide_tooltip = (event->type == GDK_LEAVE_NOTIFY);

	    /* Is the pointer above another widget now? */
	    if (GTK_TOOLTIP_VISIBLE (current_tooltip))
	      hide_tooltip |= has_tooltip_widget != current_tooltip->tooltip_widget;

	    /* Did the pointer move out of the previous "context area"? */
	    if (tip_area_set)
	      hide_tooltip |= (x <= tip_area.x
			       || x >= tip_area.x + tip_area.width
			       || y <= tip_area.y
			       || y >= tip_area.y + tip_area.height);

	    if (hide_tooltip)
	      gtk_tooltip_hide_tooltip (current_tooltip);
	    else
	      gtk_tooltip_start_delay (display);
	  }
	else
	  {
	    /* Need a new tooltip for this display */
	    current_tooltip = g_object_new (GTK_TYPE_TOOLTIP, NULL);
	    g_object_set_data_full (G_OBJECT (display),
				    "gdk-display-current-tooltip",
				    current_tooltip, g_object_unref);
	    g_signal_connect (display, "closed",
			      G_CALLBACK (gtk_tooltip_display_closed),
			      current_tooltip);

	    gtk_tooltip_set_last_window (current_tooltip, event->any.window);

	    gtk_tooltip_start_delay (display);
	  }
	break;

      default:
	break;
    }
}


#define __GTK_TOOLTIP_C__
#include "gtkaliasdef.c"
