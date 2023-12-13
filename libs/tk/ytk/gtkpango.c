/* gtktextdisplay.c - display layed-out text
 *
 * Copyright (c) 2010 Red Hat, Inc.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <pango/pangocairo.h>
#include "gtkintl.h"

#define GTK_TYPE_FILL_LAYOUT_RENDERER            (_gtk_fill_layout_renderer_get_type())
#define GTK_FILL_LAYOUT_RENDERER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_FILL_LAYOUT_RENDERER, GtkFillLayoutRenderer))
#define GTK_IS_FILL_LAYOUT_RENDERER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_FILL_LAYOUT_RENDERER))
#define GTK_FILL_LAYOUT_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILL_LAYOUT_RENDERER, GtkFillLayoutRendererClass))
#define GTK_IS_FILL_LAYOUT_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILL_LAYOUT_RENDERER))
#define GTK_FILL_LAYOUT_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILL_LAYOUT_RENDERER, GtkFillLayoutRendererClass))

typedef struct _GtkFillLayoutRenderer      GtkFillLayoutRenderer;
typedef struct _GtkFillLayoutRendererClass GtkFillLayoutRendererClass;

struct _GtkFillLayoutRenderer
{
  PangoRenderer parent_instance;

  cairo_t *cr;
};

struct _GtkFillLayoutRendererClass
{
  PangoRendererClass parent_class;
};

G_DEFINE_TYPE (GtkFillLayoutRenderer, _gtk_fill_layout_renderer, PANGO_TYPE_RENDERER)

static void
gtk_fill_layout_renderer_draw_glyphs (PangoRenderer     *renderer,
                                      PangoFont         *font,
                                      PangoGlyphString  *glyphs,
                                      int                x,
                                      int                y)
{
  GtkFillLayoutRenderer *text_renderer = GTK_FILL_LAYOUT_RENDERER (renderer);

  cairo_move_to (text_renderer->cr, (double)x / PANGO_SCALE, (double)y / PANGO_SCALE);
  pango_cairo_show_glyph_string (text_renderer->cr, font, glyphs);
}

static void
gtk_fill_layout_renderer_draw_glyph_item (PangoRenderer     *renderer,
                                          const char        *text,
                                          PangoGlyphItem    *glyph_item,
                                          int                x,
                                          int                y)
{
  GtkFillLayoutRenderer *text_renderer = GTK_FILL_LAYOUT_RENDERER (renderer);

  cairo_move_to (text_renderer->cr, (double)x / PANGO_SCALE, (double)y / PANGO_SCALE);
  pango_cairo_show_glyph_item (text_renderer->cr, text, glyph_item);
}

static void
gtk_fill_layout_renderer_draw_rectangle (PangoRenderer     *renderer,
                                         PangoRenderPart    part,
                                         int                x,
                                         int                y,
                                         int                width,
                                         int                height)
{
  GtkFillLayoutRenderer *text_renderer = GTK_FILL_LAYOUT_RENDERER (renderer);

  if (part == PANGO_RENDER_PART_BACKGROUND)
    return;

  cairo_rectangle (text_renderer->cr,
                   (double)x / PANGO_SCALE, (double)y / PANGO_SCALE,
		   (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);
  cairo_fill (text_renderer->cr);
}

static void
gtk_fill_layout_renderer_draw_trapezoid (PangoRenderer     *renderer,
                                         PangoRenderPart    part,
                                         double             y1_,
                                         double             x11,
                                         double             x21,
                                         double             y2,
                                         double             x12,
                                         double             x22)
{
  GtkFillLayoutRenderer *text_renderer = GTK_FILL_LAYOUT_RENDERER (renderer);
  cairo_matrix_t matrix;
  cairo_t *cr;

  cr = text_renderer->cr;

  cairo_save (cr);

  /* use identity scale, but keep translation */
  cairo_get_matrix (cr, &matrix);
  matrix.xx = matrix.yy = 1;
  matrix.xy = matrix.yx = 0;
  cairo_set_matrix (cr, &matrix);

  cairo_move_to (cr, x11, y1_);
  cairo_line_to (cr, x21, y1_);
  cairo_line_to (cr, x22, y2);
  cairo_line_to (cr, x12, y2);
  cairo_close_path (cr);

  cairo_fill (cr);

  cairo_restore (cr);
}

static void
gtk_fill_layout_renderer_draw_error_underline (PangoRenderer *renderer,
                                               int            x,
                                               int            y,
                                               int            width,
                                               int            height)
{
  GtkFillLayoutRenderer *text_renderer = GTK_FILL_LAYOUT_RENDERER (renderer);

  pango_cairo_show_error_underline (text_renderer->cr,
                                    (double)x / PANGO_SCALE, (double)y / PANGO_SCALE,
                                    (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);
}

static void
gtk_fill_layout_renderer_draw_shape (PangoRenderer   *renderer,
                                     PangoAttrShape  *attr,
                                     int              x,
                                     int              y)
{
  GtkFillLayoutRenderer *text_renderer = GTK_FILL_LAYOUT_RENDERER (renderer);
  cairo_t *cr = text_renderer->cr;
  PangoLayout *layout;
  PangoCairoShapeRendererFunc shape_renderer;
  gpointer                    shape_renderer_data;

  layout = pango_renderer_get_layout (renderer);

  if (!layout)
  	return;

  shape_renderer = pango_cairo_context_get_shape_renderer (pango_layout_get_context (layout),
							   &shape_renderer_data);

  if (!shape_renderer)
    return;

  cairo_save (cr);

  cairo_move_to (cr, (double)x / PANGO_SCALE, (double)y / PANGO_SCALE);

  shape_renderer (cr, attr, FALSE, shape_renderer_data);

  cairo_restore (cr);
}

static void
gtk_fill_layout_renderer_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gtk_fill_layout_renderer_parent_class)->finalize (object);
}

static void
_gtk_fill_layout_renderer_init (GtkFillLayoutRenderer *renderer)
{
}

static void
_gtk_fill_layout_renderer_class_init (GtkFillLayoutRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);
  
  renderer_class->draw_glyphs = gtk_fill_layout_renderer_draw_glyphs;
  renderer_class->draw_glyph_item = gtk_fill_layout_renderer_draw_glyph_item;
  renderer_class->draw_rectangle = gtk_fill_layout_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = gtk_fill_layout_renderer_draw_trapezoid;
  renderer_class->draw_error_underline = gtk_fill_layout_renderer_draw_error_underline;
  renderer_class->draw_shape = gtk_fill_layout_renderer_draw_shape;

  object_class->finalize = gtk_fill_layout_renderer_finalize;
}

void
_gtk_pango_fill_layout (cairo_t     *cr,
                        PangoLayout *layout)
{
  static GtkFillLayoutRenderer *renderer = NULL;
  gboolean has_current_point;
  double current_x, current_y;

  has_current_point = cairo_has_current_point (cr);
  cairo_get_current_point (cr, &current_x, &current_y);

  if (renderer == NULL)
    renderer = g_object_new (GTK_TYPE_FILL_LAYOUT_RENDERER, NULL);

  cairo_save (cr);
  cairo_translate (cr, current_x, current_y);

  renderer->cr = cr;
  pango_renderer_draw_layout (PANGO_RENDERER (renderer), layout, 0, 0);

  cairo_restore (cr);

  if (has_current_point)
    cairo_move_to (cr, current_x, current_y);
}

