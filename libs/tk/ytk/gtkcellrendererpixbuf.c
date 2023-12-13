/* gtkcellrendererpixbuf.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
#include <stdlib.h>
#include "gtkcellrendererpixbuf.h"
#include "gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkalias.h"

static void gtk_cell_renderer_pixbuf_get_property  (GObject                    *object,
						    guint                       param_id,
						    GValue                     *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_pixbuf_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_pixbuf_finalize   (GObject                    *object);
static void gtk_cell_renderer_pixbuf_create_stock_pixbuf (GtkCellRendererPixbuf *cellpixbuf,
							  GtkWidget             *widget);
static void gtk_cell_renderer_pixbuf_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
						 GdkRectangle               *rectangle,
						 gint                       *x_offset,
						 gint                       *y_offset,
						 gint                       *width,
						 gint                       *height);
static void gtk_cell_renderer_pixbuf_render     (GtkCellRenderer            *cell,
						 GdkDrawable                *window,
						 GtkWidget                  *widget,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 GdkRectangle               *expose_area,
						 GtkCellRendererState        flags);


enum {
  PROP_0,
  PROP_PIXBUF,
  PROP_PIXBUF_EXPANDER_OPEN,
  PROP_PIXBUF_EXPANDER_CLOSED,
  PROP_STOCK_ID,
  PROP_STOCK_SIZE,
  PROP_STOCK_DETAIL,
  PROP_FOLLOW_STATE,
  PROP_ICON_NAME,
  PROP_GICON
};


#define GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_CELL_RENDERER_PIXBUF, GtkCellRendererPixbufPrivate))

typedef struct _GtkCellRendererPixbufPrivate GtkCellRendererPixbufPrivate;
struct _GtkCellRendererPixbufPrivate
{
  gchar *stock_id;
  GtkIconSize stock_size;
  gchar *stock_detail;
  gboolean follow_state;
  gchar *icon_name;
  GIcon *gicon;
};

G_DEFINE_TYPE (GtkCellRendererPixbuf, gtk_cell_renderer_pixbuf, GTK_TYPE_CELL_RENDERER)

static void
gtk_cell_renderer_pixbuf_init (GtkCellRendererPixbuf *cellpixbuf)
{
  GtkCellRendererPixbufPrivate *priv;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (cellpixbuf);
  priv->stock_size = GTK_ICON_SIZE_MENU;
}

static void
gtk_cell_renderer_pixbuf_class_init (GtkCellRendererPixbufClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->finalize = gtk_cell_renderer_pixbuf_finalize;

  object_class->get_property = gtk_cell_renderer_pixbuf_get_property;
  object_class->set_property = gtk_cell_renderer_pixbuf_set_property;

  cell_class->get_size = gtk_cell_renderer_pixbuf_get_size;
  cell_class->render = gtk_cell_renderer_pixbuf_render;

  g_object_class_install_property (object_class,
				   PROP_PIXBUF,
				   g_param_spec_object ("pixbuf",
							P_("Pixbuf Object"),
							P_("The pixbuf to render"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_OPEN,
				   g_param_spec_object ("pixbuf-expander-open",
							P_("Pixbuf Expander Open"),
							P_("Pixbuf for open expander"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_CLOSED,
				   g_param_spec_object ("pixbuf-expander-closed",
							P_("Pixbuf Expander Closed"),
							P_("Pixbuf for closed expander"),
							GDK_TYPE_PIXBUF,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_STOCK_ID,
				   g_param_spec_string ("stock-id",
							P_("Stock ID"),
							P_("The stock ID of the stock icon to render"),
							NULL,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_STOCK_SIZE,
				   g_param_spec_uint ("stock-size",
						      P_("Size"),
						      P_("The GtkIconSize value that specifies the size of the rendered icon"),
						      0,
						      G_MAXUINT,
						      GTK_ICON_SIZE_MENU,
						      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_STOCK_DETAIL,
				   g_param_spec_string ("stock-detail",
							P_("Detail"),
							P_("Render detail to pass to the theme engine"),
							NULL,
							GTK_PARAM_READWRITE));

  
  /**
   * GtkCellRendererPixbuf:icon-name:
   *
   * The name of the themed icon to display.
   * This property only has an effect if not overridden by "stock_id" 
   * or "pixbuf" properties.
   *
   * Since: 2.8 
   */
  g_object_class_install_property (object_class,
				   PROP_ICON_NAME,
				   g_param_spec_string ("icon-name",
							P_("Icon Name"),
							P_("The name of the icon from the icon theme"),
							NULL,
							GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererPixbuf:follow-state:
   *
   * Specifies whether the rendered pixbuf should be colorized
   * according to the #GtkCellRendererState.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
				   PROP_FOLLOW_STATE,
				   g_param_spec_boolean ("follow-state",
 							 P_("Follow State"),
 							 P_("Whether the rendered pixbuf should be "
							    "colorized according to the state"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererPixbuf:gicon:
   *
   * The GIcon representing the icon to display.
   * If the icon theme is changed, the image will be updated
   * automatically.
   *
   * Since: 2.14
   */
  g_object_class_install_property (object_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon",
                                                        P_("Icon"),
                                                        P_("The GIcon being displayed"),
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));



  g_type_class_add_private (object_class, sizeof (GtkCellRendererPixbufPrivate));
}

static void
gtk_cell_renderer_pixbuf_finalize (GObject *object)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  GtkCellRendererPixbufPrivate *priv;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (object);
  
  if (cellpixbuf->pixbuf)
    g_object_unref (cellpixbuf->pixbuf);
  if (cellpixbuf->pixbuf_expander_open)
    g_object_unref (cellpixbuf->pixbuf_expander_open);
  if (cellpixbuf->pixbuf_expander_closed)
    g_object_unref (cellpixbuf->pixbuf_expander_closed);

  g_free (priv->stock_id);
  g_free (priv->stock_detail);
  g_free (priv->icon_name);

  if (priv->gicon)
    g_object_unref (priv->gicon);

  G_OBJECT_CLASS (gtk_cell_renderer_pixbuf_parent_class)->finalize (object);
}

static void
gtk_cell_renderer_pixbuf_get_property (GObject        *object,
				       guint           param_id,
				       GValue         *value,
				       GParamSpec     *pspec)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  GtkCellRendererPixbufPrivate *priv;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (object);
  
  switch (param_id)
    {
    case PROP_PIXBUF:
      g_value_set_object (value, cellpixbuf->pixbuf);
      break;
    case PROP_PIXBUF_EXPANDER_OPEN:
      g_value_set_object (value, cellpixbuf->pixbuf_expander_open);
      break;
    case PROP_PIXBUF_EXPANDER_CLOSED:
      g_value_set_object (value, cellpixbuf->pixbuf_expander_closed);
      break;
    case PROP_STOCK_ID:
      g_value_set_string (value, priv->stock_id);
      break;
    case PROP_STOCK_SIZE:
      g_value_set_uint (value, priv->stock_size);
      break;
    case PROP_STOCK_DETAIL:
      g_value_set_string (value, priv->stock_detail);
      break;
    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, priv->follow_state);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, priv->icon_name);
      break;
    case PROP_GICON:
      g_value_set_object (value, priv->gicon);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_pixbuf_set_property (GObject      *object,
				       guint         param_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  GtkCellRendererPixbufPrivate *priv;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (object);
  
  switch (param_id)
    {
    case PROP_PIXBUF:
      if (cellpixbuf->pixbuf)
	g_object_unref (cellpixbuf->pixbuf);
      cellpixbuf->pixbuf = (GdkPixbuf*) g_value_dup_object (value);
      if (cellpixbuf->pixbuf)
        {
          if (priv->stock_id)
            {
              g_free (priv->stock_id);
              priv->stock_id = NULL;
              g_object_notify (object, "stock-id");
            }
          if (priv->icon_name)
            {
              g_free (priv->icon_name);
              priv->icon_name = NULL;
              g_object_notify (object, "icon-name");
            }
          if (priv->gicon)
            {
              g_object_unref (priv->gicon);
              priv->gicon = NULL;
              g_object_notify (object, "gicon");
            }
        }
      break;
    case PROP_PIXBUF_EXPANDER_OPEN:
      if (cellpixbuf->pixbuf_expander_open)
	g_object_unref (cellpixbuf->pixbuf_expander_open);
      cellpixbuf->pixbuf_expander_open = (GdkPixbuf*) g_value_dup_object (value);
      break;
    case PROP_PIXBUF_EXPANDER_CLOSED:
      if (cellpixbuf->pixbuf_expander_closed)
	g_object_unref (cellpixbuf->pixbuf_expander_closed);
      cellpixbuf->pixbuf_expander_closed = (GdkPixbuf*) g_value_dup_object (value);
      break;
    case PROP_STOCK_ID:
      if (priv->stock_id)
        {
          if (cellpixbuf->pixbuf)
            {
              g_object_unref (cellpixbuf->pixbuf);
              cellpixbuf->pixbuf = NULL;
              g_object_notify (object, "pixbuf");
            }
          g_free (priv->stock_id);
        }
      priv->stock_id = g_value_dup_string (value);
      if (priv->stock_id)
        {
          if (cellpixbuf->pixbuf)
            {
              g_object_unref (cellpixbuf->pixbuf);
              cellpixbuf->pixbuf = NULL;
              g_object_notify (object, "pixbuf");
            }
          if (priv->icon_name)
            {
              g_free (priv->icon_name);
              priv->icon_name = NULL;
              g_object_notify (object, "icon-name");
            }
          if (priv->gicon)
            {
              g_object_unref (priv->gicon);
              priv->gicon = NULL;
              g_object_notify (object, "gicon");
            }
        }
      break;
    case PROP_STOCK_SIZE:
      priv->stock_size = g_value_get_uint (value);
      break;
    case PROP_STOCK_DETAIL:
      g_free (priv->stock_detail);
      priv->stock_detail = g_value_dup_string (value);
      break;
    case PROP_ICON_NAME:
      if (priv->icon_name)
	{
	  if (cellpixbuf->pixbuf)
	    {
	      g_object_unref (cellpixbuf->pixbuf);
	      cellpixbuf->pixbuf = NULL;
              g_object_notify (object, "pixbuf");
	    }
	  g_free (priv->icon_name);
	}
      priv->icon_name = g_value_dup_string (value);
      if (priv->icon_name)
        {
	  if (cellpixbuf->pixbuf)
	    {
              g_object_unref (cellpixbuf->pixbuf);
              cellpixbuf->pixbuf = NULL;
              g_object_notify (object, "pixbuf");
	    }
          if (priv->stock_id)
            {
              g_free (priv->stock_id);
              priv->stock_id = NULL;
              g_object_notify (object, "stock-id");
            }
          if (priv->gicon)
            {
              g_object_unref (priv->gicon);
              priv->gicon = NULL;
              g_object_notify (object, "gicon");
            }
        }
      break;
    case PROP_FOLLOW_STATE:
      priv->follow_state = g_value_get_boolean (value);
      break;
    case PROP_GICON:
      if (priv->gicon)
	{
	  if (cellpixbuf->pixbuf)
	    {
	      g_object_unref (cellpixbuf->pixbuf);
	      cellpixbuf->pixbuf = NULL;
              g_object_notify (object, "pixbuf");
	    }
	  g_object_unref (priv->gicon);
	}
      priv->gicon = (GIcon *) g_value_dup_object (value);
      if (priv->gicon)
        {
	  if (cellpixbuf->pixbuf)
	    {
              g_object_unref (cellpixbuf->pixbuf);
              cellpixbuf->pixbuf = NULL;
              g_object_notify (object, "pixbuf");
	    }
          if (priv->stock_id)
            {
              g_free (priv->stock_id);
              priv->stock_id = NULL;
              g_object_notify (object, "stock-id");
            }
          if (priv->icon_name)
            {
              g_free (priv->icon_name);
              priv->icon_name = NULL;
              g_object_notify (object, "icon-name");
            }
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_pixbuf_new:
 * 
 * Creates a new #GtkCellRendererPixbuf. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "pixbuf" property on the cell renderer to a pixbuf value
 * in the model, thus rendering a different image in each row of the
 * #GtkTreeView.
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_pixbuf_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF, NULL);
}

static void
gtk_cell_renderer_pixbuf_create_stock_pixbuf (GtkCellRendererPixbuf *cellpixbuf,
                                              GtkWidget             *widget)
{
  GtkCellRendererPixbufPrivate *priv;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (cellpixbuf);

  if (cellpixbuf->pixbuf)
    g_object_unref (cellpixbuf->pixbuf);

  cellpixbuf->pixbuf = gtk_widget_render_icon (widget,
                                               priv->stock_id,
                                               priv->stock_size,
                                               priv->stock_detail);

  g_object_notify (G_OBJECT (cellpixbuf), "pixbuf");
}

static void 
gtk_cell_renderer_pixbuf_create_themed_pixbuf (GtkCellRendererPixbuf *cellpixbuf,
					       GtkWidget             *widget)
{
  GtkCellRendererPixbufPrivate *priv;
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  gint width, height;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (cellpixbuf);

  if (cellpixbuf->pixbuf)
    {
      g_object_unref (cellpixbuf->pixbuf);
      cellpixbuf->pixbuf = NULL;
    }

  screen = gtk_widget_get_screen (GTK_WIDGET (widget));
  icon_theme = gtk_icon_theme_get_for_screen (screen);
  settings = gtk_settings_get_for_screen (screen);

  if (!gtk_icon_size_lookup_for_settings (settings,
					  priv->stock_size,
					  &width, &height))
    {
      g_warning ("Invalid icon size %u\n", priv->stock_size);
      width = height = 24;
    }

  if (priv->icon_name)
    cellpixbuf->pixbuf = gtk_icon_theme_load_icon (icon_theme,
			                           priv->icon_name,
			                           MIN (width, height), 
                                                   GTK_ICON_LOOKUP_USE_BUILTIN,
                                                   NULL);
  else if (priv->gicon)
    {
      GtkIconInfo *info;

      info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                             priv->gicon,
			                     MIN (width, height), 
                                             GTK_ICON_LOOKUP_USE_BUILTIN);
      if (info)
        {
          cellpixbuf->pixbuf = gtk_icon_info_load_icon (info, NULL);
          gtk_icon_info_free (info);
        }
    }

  g_object_notify (G_OBJECT (cellpixbuf), "pixbuf");
}

static GdkPixbuf *
create_colorized_pixbuf (GdkPixbuf *src, 
			 GdkColor  *new_color)
{
  gint i, j;
  gint width, height, has_alpha, src_row_stride, dst_row_stride;
  gint red_value, green_value, blue_value;
  guchar *target_pixels;
  guchar *original_pixels;
  guchar *pixsrc;
  guchar *pixdest;
  GdkPixbuf *dest;
  
  red_value = new_color->red / 255.0;
  green_value = new_color->green / 255.0;
  blue_value = new_color->blue / 255.0;
  
  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			 gdk_pixbuf_get_has_alpha (src),
			 gdk_pixbuf_get_bits_per_sample (src),
			 gdk_pixbuf_get_width (src),
			 gdk_pixbuf_get_height (src));
  
  has_alpha = gdk_pixbuf_get_has_alpha (src);
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  src_row_stride = gdk_pixbuf_get_rowstride (src);
  dst_row_stride = gdk_pixbuf_get_rowstride (dest);
  target_pixels = gdk_pixbuf_get_pixels (dest);
  original_pixels = gdk_pixbuf_get_pixels (src);
  
  for (i = 0; i < height; i++) {
    pixdest = target_pixels + i*dst_row_stride;
    pixsrc = original_pixels + i*src_row_stride;
    for (j = 0; j < width; j++) {		
      *pixdest++ = (*pixsrc++ * red_value) >> 8;
      *pixdest++ = (*pixsrc++ * green_value) >> 8;
      *pixdest++ = (*pixsrc++ * blue_value) >> 8;
      if (has_alpha) {
	*pixdest++ = *pixsrc++;
      }
    }
  }
  return dest;
}


static void
gtk_cell_renderer_pixbuf_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   GdkRectangle    *cell_area,
				   gint            *x_offset,
				   gint            *y_offset,
				   gint            *width,
				   gint            *height)
{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
  GtkCellRendererPixbufPrivate *priv;
  gint pixbuf_width  = 0;
  gint pixbuf_height = 0;
  gint calc_width;
  gint calc_height;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (cell);

  if (!cellpixbuf->pixbuf)
    {
      if (priv->stock_id)
	gtk_cell_renderer_pixbuf_create_stock_pixbuf (cellpixbuf, widget);
      else if (priv->icon_name || priv->gicon)
	gtk_cell_renderer_pixbuf_create_themed_pixbuf (cellpixbuf, widget);
    }
  
  if (cellpixbuf->pixbuf)
    {
      pixbuf_width  = gdk_pixbuf_get_width (cellpixbuf->pixbuf);
      pixbuf_height = gdk_pixbuf_get_height (cellpixbuf->pixbuf);
    }
  if (cellpixbuf->pixbuf_expander_open)
    {
      pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_open));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_open));
    }
  if (cellpixbuf->pixbuf_expander_closed)
    {
      pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_closed));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_closed));
    }
  
  calc_width  = (gint) cell->xpad * 2 + pixbuf_width;
  calc_height = (gint) cell->ypad * 2 + pixbuf_height;
  
  if (cell_area && pixbuf_width > 0 && pixbuf_height > 0)
    {
      if (x_offset)
	{
	  *x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
                        (1.0 - cell->xalign) : cell->xalign) * 
                       (cell_area->width - calc_width));
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = (cell->yalign *
                       (cell_area->height - calc_height));
          *y_offset = MAX (*y_offset, 0);
	}
    }
  else
    {
      if (x_offset) *x_offset = 0;
      if (y_offset) *y_offset = 0;
    }

  if (width)
    *width = calc_width;
  
  if (height)
    *height = calc_height;
}

static void
gtk_cell_renderer_pixbuf_render (GtkCellRenderer      *cell,
				 GdkWindow            *window,
				 GtkWidget            *widget,
				 GdkRectangle         *background_area,
				 GdkRectangle         *cell_area,
				 GdkRectangle         *expose_area,
				 GtkCellRendererState  flags)

{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
  GtkCellRendererPixbufPrivate *priv;
  GdkPixbuf *pixbuf;
  GdkPixbuf *invisible = NULL;
  GdkPixbuf *colorized = NULL;
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;
  cairo_t *cr;

  priv = GTK_CELL_RENDERER_PIXBUF_GET_PRIVATE (cell);

  gtk_cell_renderer_pixbuf_get_size (cell, widget, cell_area,
				     &pix_rect.x,
				     &pix_rect.y,
				     &pix_rect.width,
				     &pix_rect.height);

  pix_rect.x += cell_area->x + cell->xpad;
  pix_rect.y += cell_area->y + cell->ypad;
  pix_rect.width  -= cell->xpad * 2;
  pix_rect.height -= cell->ypad * 2;

  if (!gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect) ||
      !gdk_rectangle_intersect (expose_area, &draw_rect, &draw_rect))
    return;

  pixbuf = cellpixbuf->pixbuf;

  if (cell->is_expander)
    {
      if (cell->is_expanded &&
	  cellpixbuf->pixbuf_expander_open != NULL)
	pixbuf = cellpixbuf->pixbuf_expander_open;
      else if (!cell->is_expanded &&
	       cellpixbuf->pixbuf_expander_closed != NULL)
	pixbuf = cellpixbuf->pixbuf_expander_closed;
    }

  if (!pixbuf)
    return;

  if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE || !cell->sensitive)
    {
      GtkIconSource *source;
      
      source = gtk_icon_source_new ();
      gtk_icon_source_set_pixbuf (source, pixbuf);
      /* The size here is arbitrary; since size isn't
       * wildcarded in the source, it isn't supposed to be
       * scaled by the engine function
       */
      gtk_icon_source_set_size (source, GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_icon_source_set_size_wildcarded (source, FALSE);
      
     invisible = gtk_style_render_icon (widget->style,
					source,
					gtk_widget_get_direction (widget),
					GTK_STATE_INSENSITIVE,
					/* arbitrary */
					(GtkIconSize)-1,
					widget,
					"gtkcellrendererpixbuf");
     
     gtk_icon_source_free (source);
     
     pixbuf = invisible;
    }
  else if (priv->follow_state && 
	   (flags & (GTK_CELL_RENDERER_SELECTED|GTK_CELL_RENDERER_PRELIT)) != 0)
    {
      GtkStateType state;

      if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
	{
	  if (gtk_widget_has_focus (widget))
	    state = GTK_STATE_SELECTED;
	  else
	    state = GTK_STATE_ACTIVE;
	}
      else
	state = GTK_STATE_PRELIGHT;

      colorized = create_colorized_pixbuf (pixbuf,
					   &widget->style->base[state]);

      pixbuf = colorized;
    }

  cr = gdk_cairo_create (window);
  
  gdk_cairo_set_source_pixbuf (cr, pixbuf, pix_rect.x, pix_rect.y);
  gdk_cairo_rectangle (cr, &draw_rect);
  cairo_fill (cr);

  cairo_destroy (cr);
  
  if (invisible)
    g_object_unref (invisible);

  if (colorized)
    g_object_unref (colorized);
}

#define __GTK_CELL_RENDERER_PIXBUF_C__
#include "gtkaliasdef.c"
