/* gtktextdisplay.c - display layed-out text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
 *
 * This file can be used under your choice of two licenses, the LGPL
 * and the original Tk license.
 *
 * LGPL:
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
 *
 * Original Tk license:
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#define GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
#undef GDK_DISABLE_DEPRECATED

#include "config.h"
#include "gtktextdisplay.h"
#include "gtkintl.h"
#include "gtkalias.h"
/* DO NOT go putting private headers in here. This file should only
 * use the semi-public headers, as with gtktextview.c.
 */

#define GTK_TYPE_TEXT_RENDERER            (_gtk_text_renderer_get_type())
#define GTK_TEXT_RENDERER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_TEXT_RENDERER, GtkTextRenderer))
#define GTK_IS_TEXT_RENDERER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_TEXT_RENDERER))
#define GTK_TEXT_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_RENDERER, GtkTextRendererClass))
#define GTK_IS_TEXT_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_RENDERER))
#define GTK_TEXT_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_RENDERER, GtkTextRendererClass))

typedef struct _GtkTextRenderer      GtkTextRenderer;
typedef struct _GtkTextRendererClass GtkTextRendererClass;

enum {
  NORMAL,
  SELECTED,
  CURSOR
};

struct _GtkTextRenderer
{
  GdkPangoRenderer parent_instance;

  GdkScreen *screen;

  GtkWidget *widget;
  GdkDrawable *drawable;
  GdkRectangle clip_rect;
  
  GdkColor *error_color;	/* Error underline color for this widget */
  GList *widgets;		/* widgets encountered when drawing */
  
  int state;
};

struct _GtkTextRendererClass
{
  GdkPangoRendererClass parent_class;
};

G_DEFINE_TYPE (GtkTextRenderer, _gtk_text_renderer, GDK_TYPE_PANGO_RENDERER)

static GdkColor *
text_renderer_get_error_color (GtkTextRenderer *text_renderer)
{
  static const GdkColor red = { 0, 0xffff, 0, 0 };

  if (!text_renderer->error_color)
    gtk_widget_style_get (text_renderer->widget,
			  "error-underline-color", &text_renderer->error_color,
			  NULL);
  
  if (!text_renderer->error_color)
    text_renderer->error_color = gdk_color_copy (&red);

  return text_renderer->error_color;
}

static void
text_renderer_set_gdk_color (GtkTextRenderer *text_renderer,
			     PangoRenderPart  part,
			     GdkColor        *gdk_color)
{
  PangoRenderer *renderer = PANGO_RENDERER (text_renderer);

  if (gdk_color)
    {
      PangoColor color;

      color.red = gdk_color->red;
      color.green = gdk_color->green;
      color.blue = gdk_color->blue;
      
      pango_renderer_set_color (renderer, part, &color);
    }
  else
    pango_renderer_set_color (renderer, part, NULL);
	   
}

static GtkTextAppearance *
get_item_appearance (PangoItem *item)
{
  GSList *tmp_list = item->analysis.extra_attrs;

  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == gtk_text_attr_appearance_type)
	return &((GtkTextAttrAppearance *)attr)->appearance;

      tmp_list = tmp_list->next;
    }

  return NULL;
}

static void
gtk_text_renderer_prepare_run (PangoRenderer  *renderer,
			       PangoLayoutRun *run)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  GdkColor *bg_color, *fg_color, *underline_color;
  GdkPixmap *fg_stipple, *bg_stipple;
  GtkTextAppearance *appearance;

  PANGO_RENDERER_CLASS (_gtk_text_renderer_parent_class)->prepare_run (renderer, run);

  appearance = get_item_appearance (run->item);
  g_assert (appearance != NULL);

  if (appearance->draw_bg && text_renderer->state == NORMAL)
    bg_color = &appearance->bg_color;
  else
    bg_color = NULL;
  
  text_renderer_set_gdk_color (text_renderer, PANGO_RENDER_PART_BACKGROUND, bg_color);

  if (text_renderer->state == SELECTED)
    {
      if (gtk_widget_has_focus (text_renderer->widget))
	fg_color = &text_renderer->widget->style->text[GTK_STATE_SELECTED];
      else
	fg_color = &text_renderer->widget->style->text[GTK_STATE_ACTIVE];
    }
  else if (text_renderer->state == CURSOR && gtk_widget_has_focus (text_renderer->widget))
    fg_color = &text_renderer->widget->style->base[GTK_STATE_NORMAL];
  else
    fg_color = &appearance->fg_color;

  text_renderer_set_gdk_color (text_renderer, PANGO_RENDER_PART_FOREGROUND, fg_color);
  text_renderer_set_gdk_color (text_renderer, PANGO_RENDER_PART_STRIKETHROUGH, fg_color);

  if (appearance->underline == PANGO_UNDERLINE_ERROR)
    underline_color = text_renderer_get_error_color (text_renderer);
  else
    underline_color = fg_color;

  text_renderer_set_gdk_color (text_renderer, PANGO_RENDER_PART_UNDERLINE, underline_color);

  fg_stipple = appearance->fg_stipple;
  if (fg_stipple && text_renderer->screen != gdk_drawable_get_screen (fg_stipple))
    {
      g_warning ("gtk_text_renderer_prepare_run:\n"
		 "The foreground stipple bitmap has been created on the wrong screen.\n"
		 "Ignoring the stipple bitmap information.");
      fg_stipple = NULL;
    }
      
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_FOREGROUND, fg_stipple);
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_STRIKETHROUGH, fg_stipple);
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_UNDERLINE, fg_stipple);

  bg_stipple = appearance->draw_bg ? appearance->bg_stipple : NULL;
  
  if (bg_stipple && text_renderer->screen != gdk_drawable_get_screen (bg_stipple))
    {
      g_warning ("gtk_text_renderer_prepare_run:\n"
		 "The background stipple bitmap has been created on the wrong screen.\n"
		 "Ignoring the stipple bitmap information.");
      bg_stipple = NULL;
    }
  
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_BACKGROUND, bg_stipple);
}

static void
gtk_text_renderer_draw_shape (PangoRenderer   *renderer,
			      PangoAttrShape  *attr,
			      int              x,
			      int              y)
{
  GtkTextRenderer *text_renderer = GTK_TEXT_RENDERER (renderer);
  GdkGC *fg_gc;

  if (text_renderer->state == SELECTED)
    {
      if (gtk_widget_has_focus (text_renderer->widget))
	fg_gc = text_renderer->widget->style->text_gc[GTK_STATE_SELECTED];
      else
	fg_gc = text_renderer->widget->style->text_gc[GTK_STATE_SELECTED];
    }
  else if (text_renderer->state == CURSOR && gtk_widget_has_focus (text_renderer->widget))
    fg_gc = text_renderer->widget->style->base_gc[GTK_STATE_NORMAL];
  else
    fg_gc = text_renderer->widget->style->text_gc[GTK_STATE_NORMAL];
  
  if (attr->data == NULL)
    {
      /* This happens if we have an empty widget anchor. Draw
       * something empty-looking.
       */
      GdkRectangle shape_rect, draw_rect;
      
      shape_rect.x = PANGO_PIXELS (x);
      shape_rect.y = PANGO_PIXELS (y + attr->logical_rect.y);
      shape_rect.width = PANGO_PIXELS (x + attr->logical_rect.width) - shape_rect.x;
      shape_rect.height = PANGO_PIXELS (y + attr->logical_rect.y + attr->logical_rect.height) - shape_rect.y;
      
      if (gdk_rectangle_intersect (&shape_rect, &text_renderer->clip_rect,
				   &draw_rect))
	{
	  gdk_draw_rectangle (text_renderer->drawable, fg_gc,
			      FALSE, shape_rect.x, shape_rect.y,
			      shape_rect.width, shape_rect.height);
	  
	  gdk_draw_line (text_renderer->drawable, fg_gc,
			 shape_rect.x, shape_rect.y,
			 shape_rect.x + shape_rect.width,
			 shape_rect.y + shape_rect.height);
	  
	  gdk_draw_line (text_renderer->drawable, fg_gc,
			 shape_rect.x + shape_rect.width, shape_rect.y,
			 shape_rect.x,
			 shape_rect.y + shape_rect.height);
	}
    }
  else if (GDK_IS_PIXBUF (attr->data))
    {
      gint width, height;
      GdkRectangle pixbuf_rect, draw_rect;
      GdkPixbuf *pixbuf;
      
      pixbuf = GDK_PIXBUF (attr->data);
      
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      
      pixbuf_rect.x = PANGO_PIXELS (x);
      pixbuf_rect.y = PANGO_PIXELS (y) - height;
      pixbuf_rect.width = width;
      pixbuf_rect.height = height;
      
      if (gdk_rectangle_intersect (&pixbuf_rect, &text_renderer->clip_rect,
				   &draw_rect))
	{
	  gdk_draw_pixbuf (text_renderer->drawable,
			   fg_gc,
			   pixbuf,
			   draw_rect.x - pixbuf_rect.x,
			   draw_rect.y - pixbuf_rect.y,
			   draw_rect.x, draw_rect.y,
			   draw_rect.width,
			   draw_rect.height,
			   GDK_RGB_DITHER_NORMAL,
			   0, 0);
	}
    }
  else if (GTK_IS_WIDGET (attr->data))
    {
      GtkWidget *widget;
      
      widget = GTK_WIDGET (attr->data);

      text_renderer->widgets = g_list_prepend (text_renderer->widgets,
					       g_object_ref (widget));
    }
  else
    g_assert_not_reached (); /* not a pixbuf or widget */
}

static void
gtk_text_renderer_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gtk_text_renderer_parent_class)->finalize (object);
}

static void
_gtk_text_renderer_init (GtkTextRenderer *renderer)
{
}

static void
_gtk_text_renderer_class_init (GtkTextRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);
  
  renderer_class->prepare_run = gtk_text_renderer_prepare_run;
  renderer_class->draw_shape = gtk_text_renderer_draw_shape;

  object_class->finalize = gtk_text_renderer_finalize;
}

static void
text_renderer_set_state (GtkTextRenderer *text_renderer,
			 int              state)
{
  text_renderer->state = state;
}

static void
text_renderer_begin (GtkTextRenderer *text_renderer,
		     GtkWidget       *widget,
		     GdkDrawable     *drawable,
		     GdkRectangle    *clip_rect)
{
  text_renderer->widget = widget;
  text_renderer->drawable = drawable;
  text_renderer->clip_rect = *clip_rect;

  gdk_pango_renderer_set_drawable (GDK_PANGO_RENDERER (text_renderer), drawable);
  gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer),
			     widget->style->text_gc[widget->state]);
}

/* Returns a GSList of (referenced) widgets encountered while drawing.
 */
static GList *
text_renderer_end (GtkTextRenderer *text_renderer)
{
  GList *widgets = text_renderer->widgets;

  text_renderer->widget = NULL;
  text_renderer->drawable = NULL;

  text_renderer->widgets = NULL;

  if (text_renderer->error_color)
    {
      gdk_color_free (text_renderer->error_color);
      text_renderer->error_color = NULL;
    }

  gdk_pango_renderer_set_drawable (GDK_PANGO_RENDERER (text_renderer), NULL);
  gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), NULL);

  return widgets;
}


static GdkRegion *
get_selected_clip (GtkTextRenderer    *text_renderer,
                   PangoLayout        *layout,
                   PangoLayoutLine    *line,
                   int                 x,
                   int                 y,
                   int                 height,
                   int                 start_index,
                   int                 end_index)
{
  gint *ranges;
  gint n_ranges, i;
  GdkRegion *clip_region = gdk_region_new ();
  GdkRegion *tmp_region;

  pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

  for (i=0; i < n_ranges; i++)
    {
      GdkRectangle rect;

      rect.x = x + PANGO_PIXELS (ranges[2*i]);
      rect.y = y;
      rect.width = PANGO_PIXELS (ranges[2*i + 1]) - PANGO_PIXELS (ranges[2*i]);
      rect.height = height;
      
      gdk_region_union_with_rect (clip_region, &rect);
    }

  tmp_region = gdk_region_rectangle (&text_renderer->clip_rect);
  gdk_region_intersect (clip_region, tmp_region);
  gdk_region_destroy (tmp_region);

  g_free (ranges);
  return clip_region;
}

static void
render_para (GtkTextRenderer    *text_renderer,
             GtkTextLineDisplay *line_display,
             /* Top-left corner of paragraph including all margins */
             int                 x,
             int                 y,
             int                 selection_start_index,
             int                 selection_end_index)
{
  PangoLayout *layout = line_display->layout;
  int byte_offset = 0;
  PangoLayoutIter *iter;
  PangoRectangle layout_logical;
  int screen_width;
  GdkGC *selection_gc, *fg_gc;
  gint state;
  
  gboolean first = TRUE;

  iter = pango_layout_get_iter (layout);

  pango_layout_iter_get_layout_extents (iter, NULL, &layout_logical);

  /* Adjust for margins */
  
  layout_logical.x += line_display->x_offset * PANGO_SCALE;
  layout_logical.y += line_display->top_margin * PANGO_SCALE;

  screen_width = line_display->total_width;
  
  if (gtk_widget_has_focus (text_renderer->widget))
    state = GTK_STATE_SELECTED;
  else
    state = GTK_STATE_ACTIVE;

  selection_gc = text_renderer->widget->style->base_gc [state];
  fg_gc = text_renderer->widget->style->text_gc[text_renderer->widget->state];

  do
    {
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly (iter);
      int selection_y, selection_height;
      int first_y, last_y;
      PangoRectangle line_rect;
      int baseline;
      gboolean at_last_line;
      
      pango_layout_iter_get_line_extents (iter, NULL, &line_rect);
      baseline = pango_layout_iter_get_baseline (iter);
      pango_layout_iter_get_line_yrange (iter, &first_y, &last_y);
      
      /* Adjust for margins */

      line_rect.x += line_display->x_offset * PANGO_SCALE;
      line_rect.y += line_display->top_margin * PANGO_SCALE;
      baseline += line_display->top_margin * PANGO_SCALE;

      /* Selection is the height of the line, plus top/bottom
       * margin if we're the first/last line
       */
      selection_y = y + PANGO_PIXELS (first_y) + line_display->top_margin;
      selection_height = PANGO_PIXELS (last_y) - PANGO_PIXELS (first_y);

      if (first)
        {
          selection_y -= line_display->top_margin;
          selection_height += line_display->top_margin;
        }

      at_last_line = pango_layout_iter_at_last_line (iter);
      if (at_last_line)
        selection_height += line_display->bottom_margin;
      
      first = FALSE;

      if (selection_start_index < byte_offset &&
          selection_end_index > line->length + byte_offset) /* All selected */
        {
          gdk_draw_rectangle (text_renderer->drawable,
                              selection_gc,
                              TRUE,
                              x + line_display->left_margin,
                              selection_y,
                              screen_width,
                              selection_height);

	  text_renderer_set_state (text_renderer, SELECTED);
	  pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
					   line, 
					   PANGO_SCALE * x + line_rect.x,
					   PANGO_SCALE * y + baseline);
        }
      else
        {
          if (line_display->pg_bg_color)
            {
              GdkGC *bg_gc;
              
              bg_gc = gdk_gc_new (text_renderer->drawable);
              gdk_gc_set_fill (bg_gc, GDK_SOLID);
              gdk_gc_set_rgb_fg_color (bg_gc, line_display->pg_bg_color);
            
              gdk_draw_rectangle (text_renderer->drawable,
                                  bg_gc,
                                  TRUE,
                                  x + line_display->left_margin,
                                  selection_y,
                                  screen_width,
                                  selection_height);
              
              g_object_unref (bg_gc);
            }
        
	  text_renderer_set_state (text_renderer, NORMAL);
	  pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
					   line, 
					   PANGO_SCALE * x + line_rect.x,
					   PANGO_SCALE * y + baseline);

	  /* Check if some part of the line is selected; the newline
	   * that is after line->length for the last line of the
	   * paragraph counts as part of the line for this
	   */
          if ((selection_start_index < byte_offset + line->length ||
	       (selection_start_index == byte_offset + line->length && pango_layout_iter_at_last_line (iter))) &&
	      selection_end_index > byte_offset)
            {
              GdkRegion *clip_region = get_selected_clip (text_renderer, layout, line,
                                                          x + line_display->x_offset,
                                                          selection_y,
                                                          selection_height,
                                                          selection_start_index, selection_end_index);

	      /* When we change the clip on the foreground GC, we have to set
	       * it on the rendererer again, since the rendererer might have
	       * copied the GC to change attributes.
	       */
	      gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), NULL);
              gdk_gc_set_clip_region (selection_gc, clip_region);
              gdk_gc_set_clip_region (fg_gc, clip_region);
	      gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), fg_gc);

              gdk_draw_rectangle (text_renderer->drawable,
                                  selection_gc,
                                  TRUE,
                                  x + PANGO_PIXELS (line_rect.x),
                                  selection_y,
                                  PANGO_PIXELS (line_rect.width),
                                  selection_height);

	      text_renderer_set_state (text_renderer, SELECTED);
	      pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
					       line, 
					       PANGO_SCALE * x + line_rect.x,
					       PANGO_SCALE * y + baseline);

	      gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), NULL);
              gdk_gc_set_clip_region (selection_gc, NULL);
              gdk_gc_set_clip_region (fg_gc, NULL);
	      gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), fg_gc);
	      
              gdk_region_destroy (clip_region);

              /* Paint in the ends of the line */
              if (line_rect.x > line_display->left_margin * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_start_index < byte_offset) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_end_index > byte_offset + line->length)))
                {
                  gdk_draw_rectangle (text_renderer->drawable,
                                      selection_gc,
                                      TRUE,
                                      x + line_display->left_margin,
                                      selection_y,
                                      PANGO_PIXELS (line_rect.x) - line_display->left_margin,
                                      selection_height);
                }

              if (line_rect.x + line_rect.width <
                  (screen_width + line_display->left_margin) * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_end_index > byte_offset + line->length) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_start_index < byte_offset)))
                {
                  int nonlayout_width;

                  nonlayout_width =
                    line_display->left_margin + screen_width -
                    PANGO_PIXELS (line_rect.x) - PANGO_PIXELS (line_rect.width);

                  gdk_draw_rectangle (text_renderer->drawable,
                                      selection_gc,
                                      TRUE,
                                      x + PANGO_PIXELS (line_rect.x) + PANGO_PIXELS (line_rect.width),
                                      selection_y,
                                      nonlayout_width,
                                      selection_height);
                }
            }
	  else if (line_display->has_block_cursor &&
		   gtk_widget_has_focus (text_renderer->widget) &&
		   byte_offset <= line_display->insert_index &&
		   (line_display->insert_index < byte_offset + line->length ||
		    (at_last_line && line_display->insert_index == byte_offset + line->length)))
	    {
	      GdkRectangle cursor_rect;
	      GdkGC *cursor_gc;

	      /* we draw text using base color on filled cursor rectangle of cursor color
	       * (normally white on black) */
	      cursor_gc = _gtk_widget_get_cursor_gc (text_renderer->widget);

	      cursor_rect.x = x + line_display->x_offset + line_display->block_cursor.x;
	      cursor_rect.y = y + line_display->block_cursor.y + line_display->top_margin;
	      cursor_rect.width = line_display->block_cursor.width;
	      cursor_rect.height = line_display->block_cursor.height;

	      gdk_gc_set_clip_rectangle (cursor_gc, &cursor_rect);

              gdk_draw_rectangle (text_renderer->drawable,
                                  cursor_gc,
                                  TRUE,
                                  cursor_rect.x,
                                  cursor_rect.y,
                                  cursor_rect.width,
                                  cursor_rect.height);

              gdk_gc_set_clip_region (cursor_gc, NULL);

	      /* draw text under the cursor if any */
	      if (!line_display->cursor_at_line_end)
		{
		  GdkGC *cursor_text_gc;

		  cursor_text_gc = text_renderer->widget->style->base_gc[text_renderer->widget->state];
		  gdk_gc_set_clip_rectangle (cursor_text_gc, &cursor_rect);

		  gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), cursor_text_gc);
		  text_renderer_set_state (text_renderer, CURSOR);

		  pango_renderer_draw_layout_line (PANGO_RENDERER (text_renderer),
						   line,
						   PANGO_SCALE * x + line_rect.x,
						   PANGO_SCALE * y + baseline);

		  gdk_pango_renderer_set_gc (GDK_PANGO_RENDERER (text_renderer), fg_gc);
		  gdk_gc_set_clip_region (cursor_text_gc, NULL);
		}
	    }
        }

      byte_offset += line->length;
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
}

static void
on_renderer_display_closed (GdkDisplay       *display,
                            gboolean          is_error,
			    GtkTextRenderer  *text_renderer)
{
  g_signal_handlers_disconnect_by_func (text_renderer->screen,
					(gpointer)on_renderer_display_closed,
					text_renderer);
  g_object_set_data (G_OBJECT (text_renderer->screen), I_("gtk-text-renderer"), NULL);
}

static GtkTextRenderer *
get_text_renderer (GdkScreen *screen)
{
  GtkTextRenderer *text_renderer;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  
  text_renderer = g_object_get_data (G_OBJECT (screen), "gtk-text-renderer");
  if (!text_renderer)
    {
      text_renderer = g_object_new (GTK_TYPE_TEXT_RENDERER, "screen", screen, NULL);
      text_renderer->screen = screen;
      
      g_object_set_data_full (G_OBJECT (screen), I_("gtk-text-renderer"), text_renderer,
			      (GDestroyNotify)g_object_unref);

      g_signal_connect_object (gdk_screen_get_display (screen), "closed",
                               G_CALLBACK (on_renderer_display_closed),
                               text_renderer, 0);
    }

  return text_renderer;
}

void
gtk_text_layout_draw (GtkTextLayout *layout,
                      GtkWidget *widget,
                      GdkDrawable *drawable,
		      GdkGC       *cursor_gc,
                      /* Location of the drawable
                         in layout coordinates */
                      gint x_offset,
                      gint y_offset,
                      /* Region of the layout to
                         render */
                      gint x,
                      gint y,
                      gint width,
                      gint height,
                      /* widgets to expose */
                      GList **widgets)
{
  GdkRectangle clip;
  gint current_y;
  GSList *cursor_list;
  GtkTextRenderer *text_renderer;
  GtkTextIter selection_start, selection_end;
  gboolean have_selection = FALSE;
  GSList *line_list;
  GSList *tmp_list;
  GList *tmp_widgets;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  if (width == 0 || height == 0)
    return;

  line_list =  gtk_text_layout_get_lines (layout, y + y_offset, y + y_offset + height, &current_y);
  current_y -= y_offset;

  if (line_list == NULL)
    return; /* nothing on the screen */

  clip.x = x;
  clip.y = y;
  clip.width = width;
  clip.height = height;

  text_renderer = get_text_renderer (gdk_drawable_get_screen (drawable));

  text_renderer_begin (text_renderer, widget, drawable, &clip);

  gtk_text_layout_wrap_loop_start (layout);

  if (gtk_text_buffer_get_selection_bounds (layout->buffer,
                                            &selection_start,
                                            &selection_end))
    have_selection = TRUE;

  tmp_list = line_list;
  while (tmp_list != NULL)
    {
      GtkTextLineDisplay *line_display;
      gint selection_start_index = -1;
      gint selection_end_index = -1;
      gboolean have_strong;
      gboolean have_weak;

      GtkTextLine *line = tmp_list->data;

      line_display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (line_display->height > 0)
        {
          g_assert (line_display->layout != NULL);
          
          if (have_selection)
            {
              GtkTextIter line_start, line_end;
              gint byte_count;
              
              gtk_text_layout_get_iter_at_line (layout,
                                                &line_start,
                                                line, 0);
              line_end = line_start;
	      if (!gtk_text_iter_ends_line (&line_end))
		gtk_text_iter_forward_to_line_end (&line_end);
              byte_count = gtk_text_iter_get_visible_line_index (&line_end);     

              if (gtk_text_iter_compare (&selection_start, &line_end) <= 0 &&
                  gtk_text_iter_compare (&selection_end, &line_start) >= 0)
                {
                  if (gtk_text_iter_compare (&selection_start, &line_start) >= 0)
                    selection_start_index = gtk_text_iter_get_visible_line_index (&selection_start);
                  else
                    selection_start_index = -1;

                  if (gtk_text_iter_compare (&selection_end, &line_end) <= 0)
                    selection_end_index = gtk_text_iter_get_visible_line_index (&selection_end);
                  else
                    selection_end_index = byte_count + 1; /* + 1 to flag past-the-end */
                }
            }

          render_para (text_renderer, line_display,
                       - x_offset,
                       current_y,
                       selection_start_index, selection_end_index);

          /* We paint the cursors last, because they overlap another chunk
         and need to appear on top. */

 	  have_strong = FALSE;
 	  have_weak = FALSE;
	  
	  cursor_list = line_display->cursors;
	  while (cursor_list)
	    {
	      GtkTextCursorDisplay *cursor = cursor_list->data;
 	      if (cursor->is_strong)
 		have_strong = TRUE;
 	      else
 		have_weak = TRUE;
	      
	      cursor_list = cursor_list->next;
 	    }
	  
          cursor_list = line_display->cursors;
          while (cursor_list)
            {
              GtkTextCursorDisplay *cursor = cursor_list->data;
	      GtkTextDirection dir;
 	      GdkRectangle cursor_location;

              dir = line_display->direction;
 	      if (have_strong && have_weak)
 		{
 		  if (!cursor->is_strong)
 		    dir = (dir == GTK_TEXT_DIR_RTL) ? GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
 		}
 
 	      cursor_location.x = line_display->x_offset + cursor->x - x_offset;
 	      cursor_location.y = current_y + line_display->top_margin + cursor->y;
 	      cursor_location.width = 0;
 	      cursor_location.height = cursor->height;

	      gtk_draw_insertion_cursor (widget, drawable, &clip, &cursor_location,
					 cursor->is_strong,
					 dir, have_strong && have_weak);

              cursor_list = cursor_list->next;
            }
        } /* line_display->height > 0 */
          
      current_y += line_display->height;
      gtk_text_layout_free_line_display (layout, line_display);
      
      tmp_list = g_slist_next (tmp_list);
    }

  gtk_text_layout_wrap_loop_end (layout);

  tmp_widgets = text_renderer_end (text_renderer);
  if (widgets)
    *widgets = tmp_widgets;
  else
    {
      g_list_foreach (tmp_widgets, (GFunc)g_object_unref, NULL);
      g_list_free (tmp_widgets);
    }

  g_slist_free (line_list);
}

#define __GTK_TEXT_DISPLAY_C__
#include "gtkaliasdef.c"
