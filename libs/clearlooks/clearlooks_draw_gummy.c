/* Clearlooks Gummy style
 * Copyright (C) 2007 Andrea Cimitan
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
 *
 * Written by Andrea Cimitan <andrea.cimitan@gmail.com>
 */

#include "clearlooks_draw.h"
#include "clearlooks_style.h"
#include "clearlooks_types.h"

#include "support.h"
#include <ge-support.h>

#include <cairo.h>

/* Normal shadings */
#define SHADE_TOP 1.08
#define SHADE_CENTER_TOP 1.02
#define SHADE_BOTTOM 0.94

/* Listview */
#define LISTVIEW_SHADE_TOP 1.06
#define LISTVIEW_SHADE_CENTER_TOP 1.02
#define LISTVIEW_SHADE_BOTTOM 0.96

/* Toolbar */
#define TOOLBAR_SHADE_TOP 1.04
#define TOOLBAR_SHADE_CENTER_TOP 1.01
#define TOOLBAR_SHADE_BOTTOM 0.97

static void
clearlooks_draw_gummy_gradient (cairo_t          *cr,
                                double x, double y, int width, int height,
                                const CairoColor *color,
                                gboolean disabled, gboolean radius, CairoCorners corners)
{
	CairoColor fill;
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pt;

	ge_shade_color (color, disabled? 1.04 : SHADE_TOP, &shade1);
	ge_shade_color (color, disabled? 1.01 : SHADE_CENTER_TOP, &shade2);
	ge_shade_color (color, disabled? 0.99 : 1.0, &fill);
	ge_shade_color (color, disabled? 0.96 : SHADE_BOTTOM, &shade3);

	pt = cairo_pattern_create_linear (x, y, x, y+height);
	cairo_pattern_add_color_stop_rgb (pt, 0.0, shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pt, 0.5, shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pt, 0.5, fill.r, fill.g, fill.b);
	cairo_pattern_add_color_stop_rgb (pt, 1.0, shade3.r, shade3.g, shade3.b);

	cairo_set_source (cr, pt);
	ge_cairo_rounded_rectangle (cr, x, y, width, height, radius, corners);
	cairo_fill (cr);

	cairo_pattern_destroy (pt);
}

static void
clearlooks_set_mixed_color (cairo_t          *cr,
                            const CairoColor *color1,
                            const CairoColor *color2,
                            gdouble mix_factor)
{
	CairoColor composite;

	ge_mix_color (color1, color2, mix_factor, &composite);
	ge_cairo_set_color (cr, &composite);
}

static void
clearlooks_gummy_draw_highlight_and_shade (cairo_t                *cr,
                                           const CairoColor       *bg_color,
                                           const ShadowParameters *params,
                                           int width, int height, gdouble radius)
{
	CairoColor shadow;
	CairoColor highlight;
	uint8 corners = params->corners;
	double x = 1.0;
	double y = 1.0;

	/* not really sure of shading ratios... we will think */
	ge_shade_color (bg_color, 0.8, &shadow);
	ge_shade_color (bg_color, 1.2, &highlight);

	cairo_save (cr);

	/* Top/Left highlight */
	if (corners & CR_CORNER_BOTTOMLEFT)
		cairo_move_to (cr, x, y+height-radius);
	else
		cairo_move_to (cr, x, y+height);

	ge_cairo_rounded_corner (cr, x, y, radius, corners & CR_CORNER_TOPLEFT);

	if (corners & CR_CORNER_TOPRIGHT)
		cairo_line_to (cr, x+width-radius, y);
	else
		cairo_line_to (cr, x+width, y);

	if (params->shadow & CL_SHADOW_OUT)
		cairo_set_source_rgba (cr, highlight.r, highlight.g, highlight.b, 0.5);
	else
		cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, 0.5);

	cairo_stroke (cr);

	/* Bottom/Right highlight -- this includes the corners */
	cairo_move_to (cr, x+width-radius, y); /* topright and by radius to the left */
	ge_cairo_rounded_corner (cr, x+width, y, radius, corners & CR_CORNER_TOPRIGHT);
	ge_cairo_rounded_corner (cr, x+width, y+height, radius, corners & CR_CORNER_BOTTOMRIGHT);
	ge_cairo_rounded_corner (cr, x, y+height, radius, corners & CR_CORNER_BOTTOMLEFT);

	if (params->shadow & CL_SHADOW_OUT)
		cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, 0.5);
	else
		cairo_set_source_rgba (cr, highlight.r, highlight.g, highlight.b, 0.5);

	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_gummy_draw_button (cairo_t                *cr,
                              const ClearlooksColors *colors,
                              const WidgetParameters *params,
                              int x, int y, int width, int height)
{
	double xoffset = 0, yoffset = 0;
	CairoColor fill            = colors->bg[params->state_type];
	CairoColor border_normal   = colors->shade[6];
	CairoColor border_disabled = colors->shade[4];
	double radius;

	cairo_pattern_t *pattern;

	cairo_save (cr);
	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1.0);

	if (params->xthickness == 3)
		xoffset = 1;
	if (params->ythickness == 3)
		yoffset = 1;

	radius = MIN (params->radius, MIN ((width - 2.0 - 2*xoffset) / 2.0, (height - 2.0 - 2*yoffset) / 2.0));

	if (params->xthickness == 3 || params->ythickness == 3)
	{
		if (params->enable_shadow && !params->active && !params->disabled && !params->is_default)
		{
			CairoColor shadow;

			radius = MIN (params->radius, MIN ((width - 2.0 - 2*xoffset) / 2.0 - 1.0, (height - 2.0 - 2*yoffset) / 2.0 - 1.0));

			ge_cairo_inner_rounded_rectangle (cr, 0, 0, width, height, radius+1, params->corners);
			ge_shade_color (&params->parentbg, 0.97, &shadow);
			ge_cairo_set_color (cr, &shadow);
			cairo_stroke (cr);

			ge_cairo_inner_rounded_rectangle (cr, 1, 1, width-1, height-1, radius+1, params->corners);
			ge_shade_color (&params->parentbg, 0.93, &shadow);
			ge_cairo_set_color (cr, &shadow);
			cairo_stroke (cr);
		}

		if (params->is_default)
		{
			CairoColor shadow = colors->spot[1];

			radius = MIN (params->radius, MIN ((width - 2.0 - 2*xoffset) / 2.0 - 1.0, (height - 2.0 - 2*yoffset) / 2.0 - 1.0));

			ge_cairo_inner_rounded_rectangle (cr, 0, 0, width, height, radius+1, params->corners);
			clearlooks_set_mixed_color (cr, &params->parentbg, &shadow, 0.5);
			cairo_stroke (cr);
		}

		if (!(params->enable_shadow && !params->active && !params->disabled))
			params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, width, height, params->radius+1, params->corners);
	}

	clearlooks_draw_gummy_gradient (cr, xoffset+1, yoffset+1,
	                                width-(xoffset*2)-2, height-(yoffset*2)-2,
	                                &fill, params->disabled, radius, params->corners);

	/* Pressed button shadow */
	if (params->active)
	{
		CairoColor shadow;
		ge_shade_color (&fill, 0.92, &shadow);

		cairo_save (cr);

		ge_cairo_rounded_rectangle (cr, xoffset+1, yoffset+1, width-(xoffset*2)-2, height, radius,
		                            params->corners & (CR_CORNER_TOPLEFT | CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMLEFT));
		cairo_clip (cr);
		cairo_rectangle (cr, xoffset+1, yoffset+1, width-(xoffset*2)-2, 3);

		pattern = cairo_pattern_create_linear (xoffset+1, yoffset+1, xoffset+1, yoffset+4);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.58);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);

		cairo_rectangle (cr, xoffset+1, yoffset+1, 3, height-(yoffset*2)-2);

		pattern = cairo_pattern_create_linear (xoffset+1, yoffset+1, xoffset+4, yoffset+1);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.58);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);

		cairo_restore (cr);
	}

	/* Border */
	if (params->is_default) /* || (params->prelight && params->enable_shadow)) */
		border_normal = colors->spot[2];
	if (params->disabled)
		ge_cairo_set_color (cr, &border_disabled);
	else
		clearlooks_set_mixed_color (cr, &border_normal, &fill, 0.2);
	ge_cairo_rounded_rectangle (cr, xoffset + 0.5, yoffset + 0.5,
	                            width-(xoffset*2)-1, height-(yoffset*2)-1,
	                            radius, params->corners);
	cairo_stroke (cr);

	if (!params->active)
	{
		params->style_functions->draw_top_left_highlight (cr, &fill, params, 1+xoffset, 1+xoffset,
		                                                  width-(1+xoffset)*2, height-(1+xoffset)*2,
		                                                  radius, params->corners);
	}
	cairo_restore (cr);
}

static void
clearlooks_gummy_draw_entry (cairo_t                *cr,
                             const ClearlooksColors *colors,
                             const WidgetParameters *params,
                             int x, int y, int width, int height)
{
	const CairoColor *base = &colors->base[params->state_type];
	CairoColor border = colors->shade[params->disabled ? 4 : 6];
	double radius = MIN (params->radius, MIN ((width - 4.0) / 2.0, (height - 4.0) / 2.0));

	if (params->focus)
		border = colors->spot[2];

	cairo_save (cr);

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1.0);

	/* Now fill the area we want to be base[NORMAL]. */
	ge_cairo_rounded_rectangle (cr, 2, 2, width-4, height-4, MAX(0, radius-1), params->corners);
	ge_cairo_set_color (cr, base);
	cairo_fill (cr);

	params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, width, height, radius+1, params->corners);

	/* Draw the inner shadow */
	if (params->focus)
	{
		clearlooks_set_mixed_color (cr, base, &colors->spot[1], 0.5);
		ge_cairo_inner_rounded_rectangle (cr, 2, 2, width-4, height-4, MAX(0, radius-1), params->corners);
		cairo_stroke (cr);
	}
	else
	{
		CairoColor shadow;
		ge_shade_color (&border, 0.92, &shadow);

		cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, params->disabled ? 0.09 : 0.18);

		cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
		cairo_move_to (cr, 2.5, height-radius);
		cairo_arc (cr, 2.5+MAX(0, radius-1), 2.5+MAX(0, radius-1), MAX(0, radius-1), G_PI, 270*(G_PI/180));
		cairo_line_to (cr, width-radius, 2.5);
		cairo_stroke (cr);
	}

	ge_cairo_inner_rounded_rectangle (cr, 1, 1, width-2, height-2, radius, params->corners);
	ge_cairo_set_color (cr, &border);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_gummy_draw_progressbar_trough (cairo_t                *cr,
                                          const ClearlooksColors *colors,
                                          const WidgetParameters *params,
                                          int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[7];
	CairoColor        shadow;
	cairo_pattern_t  *pattern;
	double            radius = MIN (params->radius, MIN ((height-2.0) / 2.0, (width-2.0) / 2.0));

	cairo_save (cr);

	cairo_set_line_width (cr, 1.0);

	/* Create trough box */
	ge_cairo_rounded_rectangle (cr, x+1, y+1, width-2, height-2, radius, params->corners);
	ge_cairo_set_color (cr, &colors->shade[2]);
	cairo_fill (cr);

	/* Draw border */
	ge_cairo_rounded_rectangle (cr, x+0.5, y+0.5, width-1, height-1, radius, params->corners);
	clearlooks_set_mixed_color (cr, border, &colors->shade[2], 0.3);
	cairo_stroke (cr);

	/* clip the corners of the shadows */
	ge_cairo_rounded_rectangle (cr, x+1, y+1, width-2, height-2, radius, params->corners);
	cairo_clip (cr);

	ge_shade_color (border, 0.92, &shadow);

	/* Top shadow */
	cairo_rectangle (cr, x+1, y+1, width-2, 4);
	pattern = cairo_pattern_create_linear (x, y, x, y+4);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.3);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	/* Left shadow */
	cairo_rectangle (cr, x+1, y+1, 4, height-2);
	pattern = cairo_pattern_create_linear (x, y, x+4, y);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.3);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	cairo_restore (cr);
}

static void
clearlooks_gummy_draw_progressbar_fill (cairo_t                     *cr,
                                        const ClearlooksColors      *colors,
                                        const WidgetParameters      *params,
                                        const ProgressBarParameters *progressbar,
                                        int x, int y, int width, int height, gint offset)
{
	boolean is_horizontal = progressbar->orientation < 2;
	double  tile_pos = 0;
	double  stroke_width;
	double  radius;
	int     x_step;

	cairo_pattern_t *pattern;
	CairoColor       shade1, shade2, shade3;
	CairoColor       border;
	CairoColor       shadow;

	radius = MAX (0, params->radius - params->xthickness);

	cairo_save (cr);

	if (!is_horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	if ((progressbar->orientation == CL_ORIENTATION_RIGHT_TO_LEFT) || (progressbar->orientation == CL_ORIENTATION_BOTTOM_TO_TOP))
		ge_cairo_mirror (cr, CR_MIRROR_HORIZONTAL, &x, &y, &width, &height);

	/* Clamp the radius so that the _height_ fits ... */
	radius = MIN (radius, height / 2.0);

	stroke_width = height*2;
	x_step = (((float)stroke_width/10)*offset); /* This looks weird ... */

	cairo_translate (cr, x, y);

	cairo_save (cr);
	/* This is kind of nasty ... Clip twice from each side in case the length
	 * of the fill is smaller than twice the radius. */
	ge_cairo_rounded_rectangle (cr, 0, 0, width + radius, height, radius, CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT);
	cairo_clip (cr);
	ge_cairo_rounded_rectangle (cr, -radius, 0, width + radius, height, radius, CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT);
	cairo_clip (cr);

	/* Draw the background gradient */
	ge_shade_color (&colors->spot[1], SHADE_TOP, &shade1);
	ge_shade_color (&colors->spot[1], SHADE_CENTER_TOP, &shade2);
	ge_shade_color (&colors->spot[1], SHADE_BOTTOM, &shade3);
	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, colors->spot[1].r, colors->spot[1].g, colors->spot[1].b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, shade3.r, shade3.g, shade3.b);
	cairo_set_source (cr, pattern);
	cairo_paint (cr);
	cairo_pattern_destroy (pattern);

	/* Draw the strokes */
	while (tile_pos <= width+x_step)
	{
		cairo_move_to (cr, stroke_width/2-x_step, 0);
		cairo_line_to (cr, stroke_width-x_step,   0);
		cairo_line_to (cr, stroke_width/2-x_step, height);
		cairo_line_to (cr, -x_step, height);

		cairo_translate (cr, stroke_width, 0);
		tile_pos += stroke_width;
	}

	cairo_set_source_rgba (cr, colors->spot[2].r,
	                           colors->spot[2].g,
	                           colors->spot[2].b,
	                           0.15);

	cairo_fill (cr);
	cairo_restore (cr); /* rounded clip region */

	/* inner highlight border
	 * This is again kinda ugly. Draw once from each side, clipping away the other. */
	cairo_set_source_rgba (cr, colors->spot[0].r, colors->spot[0].g, colors->spot[0].b, 0.2);

	/* left side */
	cairo_save (cr);
	cairo_rectangle (cr, 0, 0, width / 2, height);
	cairo_clip (cr);

	if (progressbar->pulsing)
		ge_cairo_rounded_rectangle (cr, 1.5, 0.5, width + radius, height - 1, radius, CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT);
	else
		ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width + radius, height - 1, radius, CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT);

	cairo_stroke (cr);
	cairo_restore (cr); /* clip */

	/* right side */
	cairo_save (cr);
	cairo_rectangle (cr, width / 2, 0, (width+1) / 2, height);
	cairo_clip (cr);

	if (progressbar->value < 1.0 || progressbar->pulsing)
		ge_cairo_rounded_rectangle (cr, -1.5 - radius, 0.5, width + radius, height - 1, radius, CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT);
	else
		ge_cairo_rounded_rectangle (cr, -0.5 - radius, 0.5, width + radius, height - 1, radius, CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT);

	cairo_stroke (cr);
	cairo_restore (cr); /* clip */


	/* Draw the dark lines and the shadow */
	cairo_save (cr);
	/* Again, this weird clip area. */
	ge_cairo_rounded_rectangle (cr, -1.0, 0, width + radius + 2.0, height, radius, CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT);
	cairo_clip (cr);
	ge_cairo_rounded_rectangle (cr, -radius - 1.0, 0, width + radius + 2.0, height, radius, CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT);
	cairo_clip (cr);

	border = colors->spot[2];
	border.a = 0.6;
	ge_shade_color (&colors->shade[7], 0.92, &shadow);
	shadow.a = 0.2;

	if (progressbar->pulsing)
	{
		/* At the beginning of the bar. */
		cairo_move_to (cr, 0.5 + radius, height + 0.5);
		ge_cairo_rounded_corner (cr, 0.5, height + 0.5, radius + 1, CR_CORNER_BOTTOMLEFT);
		ge_cairo_rounded_corner (cr, 0.5, -0.5, radius + 1, CR_CORNER_TOPLEFT);
		ge_cairo_set_color (cr, &border);
		cairo_stroke (cr);

		cairo_move_to (cr, -0.5 + radius, height + 0.5);
		ge_cairo_rounded_corner (cr, -0.5, height + 0.5, radius + 1, CR_CORNER_BOTTOMLEFT);
		ge_cairo_rounded_corner (cr, -0.5, -0.5, radius + 1, CR_CORNER_TOPLEFT);
		ge_cairo_set_color (cr, &shadow);
		cairo_stroke (cr);
	}
	if (progressbar->value < 1.0 || progressbar->pulsing)
	{
		/* At the end of the bar. */
		cairo_move_to (cr, width - 0.5 - radius, -0.5);
		ge_cairo_rounded_corner (cr, width - 0.5, -0.5, radius + 1, CR_CORNER_TOPRIGHT);
		ge_cairo_rounded_corner (cr, width - 0.5, height + 0.5, radius + 1, CR_CORNER_BOTTOMRIGHT);
		ge_cairo_set_color (cr, &border);
		cairo_stroke (cr);

		cairo_move_to (cr, width + 0.5 - radius, -0.5);
		ge_cairo_rounded_corner (cr, width + 0.5, -0.5, radius + 1, CR_CORNER_TOPRIGHT);
		ge_cairo_rounded_corner (cr, width + 0.5, height + 0.5, radius + 1, CR_CORNER_BOTTOMRIGHT);
		ge_cairo_set_color (cr, &shadow);
		cairo_stroke (cr);
	}

	cairo_restore (cr);

	cairo_restore (cr); /* rotation, mirroring */
}

static void
clearlooks_gummy_scale_draw_gradient (cairo_t          *cr,
                                      const CairoColor *fill,
                                      const CairoColor *border,
                                      int x, int y, int width, int height,
                                      gboolean horizontal, gboolean in)
{
	cairo_pattern_t *pattern;

	CairoColor f1, f2;

	ge_shade_color (fill, in? 0.95 : 1.1, &f1);
	ge_shade_color (fill, in? 1.05 : 0.9, &f2);

	pattern = cairo_pattern_create_linear (0.5, 0.5, horizontal ? 0.5 :  width + 1.0, horizontal ? height + 1.0 : 0.5);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, f1.r, f1.g, f1.b, f1.a);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, f2.r, f2.g, f2.b, f2.a);

	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	clearlooks_set_mixed_color (cr, border, fill, 0.2);
	ge_cairo_inner_rectangle (cr, x, y, width, height);
	cairo_stroke (cr);
}

#define TROUGH_SIZE 7
static void
clearlooks_gummy_draw_scale_trough (cairo_t                *cr,
                                    const ClearlooksColors *colors,
                                    const WidgetParameters *params,
                                    const SliderParameters *slider,
                                    int x, int y, int width, int height)
{
	int    trough_width, trough_height;
	double translate_x, translate_y;
	CairoColor fill, border;
	gboolean in;

	cairo_save (cr);

	if (slider->horizontal)
	{
		trough_width  = width;
		trough_height = TROUGH_SIZE;

		translate_x   = x;
		translate_y   = y + (height/2) - (TROUGH_SIZE/2);
	}
	else
	{
		trough_width  = TROUGH_SIZE;
		trough_height = height;

		translate_x   = x + (width/2) - (TROUGH_SIZE/2);
		translate_y   = y;
	}

	cairo_set_line_width (cr, 1.0);
	cairo_translate (cr, translate_x, translate_y);

	if (!slider->fill_level)
		params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, trough_width, trough_height, 0, 0);

	if (!slider->lower && !slider->fill_level)
	{
		ge_shade_color (&params->parentbg, 0.896, &fill);
		border = colors->shade[6];
		in = TRUE;
	}
	else if (!slider->fill_level)
	{
		fill = colors->spot[1];
		border = colors->spot[2];
		in = FALSE;
	}
	else
	{
		fill = colors->spot[1];
		border = colors->spot[2];

		fill.a = 0.25;
		border.a = 0.25;
		
		in = FALSE;
	}

	clearlooks_gummy_scale_draw_gradient (cr,
	                                      &fill,
	                                      &border,
	                                      1, 1, trough_width - 2, trough_height - 2,
	                                      slider->horizontal, in);

	cairo_restore (cr);
}

static void
clearlooks_gummy_draw_tab (cairo_t                *cr,
                           const ClearlooksColors *colors,
                           const WidgetParameters *params,
                           const TabParameters    *tab,
                           int x, int y, int width, int height)
{
	const CairoColor *border        = &colors->shade[5];
	const CairoColor *stripe_fill   = &colors->spot[1];
	const CairoColor *stripe_border = &colors->spot[2];
	const CairoColor *fill;

	cairo_pattern_t  *pattern = NULL;

	double            radius;
	double            stripe_size = 2.0;
	double            stripe_fill_size;
	double            stripe_border_pos;

	gboolean horizontal = FALSE;

	radius = MIN (params->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));

	/* Set clip */
	cairo_rectangle      (cr, x, y, width, height);
	cairo_clip           (cr);
	cairo_new_path       (cr);

	/* Translate and set line width */
	cairo_set_line_width (cr, 1.0);
	cairo_translate      (cr, x+0.5, y+0.5);

	/* Make the tabs slightly bigger than they should be, to create a gap */
	/* And calculate the strip size too, while you're at it */
	if (tab->gap_side == CL_GAP_TOP || tab->gap_side == CL_GAP_BOTTOM)
	{
		if (params->ythickness == 3)
			stripe_size = 3.0;

		height += 3.0;
		stripe_fill_size = (tab->gap_side == CL_GAP_TOP ? stripe_size/height : stripe_size/(height-2));
		stripe_border_pos = (tab->gap_side == CL_GAP_TOP ? (stripe_size+1.0)/height : (stripe_size+1.0)/(height-2));

		horizontal = TRUE;

		if (tab->gap_side == CL_GAP_TOP)
			cairo_translate (cr, 0.0, -3.0); /* gap at the other side */
	}
	else
	{
		if (params->xthickness == 3)
			stripe_size = 3.0;

		width += 3.0;
		stripe_fill_size = (tab->gap_side == CL_GAP_LEFT ? stripe_size/width : stripe_size/(width-2));
		stripe_border_pos = (tab->gap_side == CL_GAP_LEFT ? (stripe_size+1.0)/width : (stripe_size+1.0)/(width-2));

		if (tab->gap_side == CL_GAP_LEFT)
			cairo_translate (cr, -3.0, 0.0); /* gap at the other side */
	}

	/* Set the fill color */
	fill = &colors->bg[params->state_type];

	/* Set tab shape */
	ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1,
	                            radius, params->corners);

	/* Draw fill */
	ge_cairo_set_color (cr, fill);
	cairo_fill  (cr);

	/* Draw highlight */
	if (!params->active)
	{
		ShadowParameters shadow;

		shadow.shadow  = CL_SHADOW_OUT;
		shadow.corners = params->corners;

		clearlooks_gummy_draw_highlight_and_shade (cr, &colors->bg[0], &shadow,
		                                           width, height, radius);
	}

	if (params->active)
	{
		CairoColor hilight;
		CairoColor shade1, shade2, shade3;

		ge_shade_color (fill, 1.15, &hilight);
		ge_shade_color (fill, SHADE_TOP, &shade1);
		ge_shade_color (fill, SHADE_CENTER_TOP, &shade2);
		ge_shade_color (fill, SHADE_BOTTOM, &shade3);

		switch	(tab->gap_side)
		{
			case CL_GAP_TOP:
				pattern = cairo_pattern_create_linear (0, height-2, 0, 0);
				break;
			case CL_GAP_BOTTOM:
				pattern = cairo_pattern_create_linear (0, 1, 0, height);
				break;
			case CL_GAP_LEFT:
				pattern = cairo_pattern_create_linear (width-2, 0, 1, 0);
				break;
			case CL_GAP_RIGHT:
				pattern = cairo_pattern_create_linear (1, 0, width-2, 0);
				break;
		}

		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);

		cairo_pattern_add_color_stop_rgb (pattern, 0.0, hilight.r, hilight.g, hilight.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0/(horizontal ? height : width), hilight.r, hilight.g, hilight.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0/(horizontal ? height : width), shade1.r, shade1.g, shade1.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.45, shade2.r, shade2.g, shade2.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.45, fill->r, fill->g, fill->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, shade3.r, shade3.g, shade3.b);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}
	else
	{
		CairoColor shade1;

		ge_shade_color (fill, SHADE_TOP, &shade1);

		switch	(tab->gap_side)
		{
			case CL_GAP_TOP:
				pattern = cairo_pattern_create_linear (0, height-2, 0, 0);
				break;
			case CL_GAP_BOTTOM:
				pattern = cairo_pattern_create_linear (0, 0, 0, height);
				break;
			case CL_GAP_LEFT:
				pattern = cairo_pattern_create_linear (width-2, 0, 0, 0);
				break;
			case CL_GAP_RIGHT:
				pattern = cairo_pattern_create_linear (0, 0, width, 0);
				break;
		}

		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);

		cairo_pattern_add_color_stop_rgba (pattern, 0.0, stripe_fill->r, stripe_fill->g, stripe_fill->b, 0.6);
		/* cairo_pattern_add_color_stop_rgba (pattern, 1.0/(horizontal ? height : width), stripe_fill->r, stripe_fill->g, stripe_fill->b, 0.34);
		   cairo_pattern_add_color_stop_rgba (pattern, 1.0/(horizontal ? height : width), stripe_fill->r, stripe_fill->g, stripe_fill->b, 0.5); */
		cairo_pattern_add_color_stop_rgb  (pattern, stripe_fill_size, stripe_fill->r, stripe_fill->g, stripe_fill->b);
		cairo_pattern_add_color_stop_rgba (pattern, stripe_fill_size, stripe_border->r, stripe_border->g, stripe_border->b, 0.72);
		cairo_pattern_add_color_stop_rgba (pattern, stripe_border_pos, stripe_border->r, stripe_border->g, stripe_border->b, 0.72);
		cairo_pattern_add_color_stop_rgb  (pattern, stripe_border_pos, shade1.r, shade1.g, shade1.b);
		cairo_pattern_add_color_stop_rgba (pattern, 0.8, fill->r, fill->g, fill->b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}

	ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);

	if (params->active)
	{
		ge_cairo_set_color (cr, border);
		cairo_stroke (cr);
	}
	else
	{
		switch	(tab->gap_side)
		{
			case CL_GAP_TOP:
				pattern = cairo_pattern_create_linear (2, height-2, 2, 2);
				break;
			case CL_GAP_BOTTOM:
				pattern = cairo_pattern_create_linear (2, 2, 2, height);
				break;
			case CL_GAP_LEFT:
				pattern = cairo_pattern_create_linear (width-2, 2, 2, 2);
				break;
			case CL_GAP_RIGHT:
				pattern = cairo_pattern_create_linear (2, 2, width, 2);
				break;
		}

		cairo_pattern_add_color_stop_rgb (pattern, 0.0, stripe_border->r, stripe_border->g, stripe_border->b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.8, border->r,        border->g,        border->b);
		cairo_set_source (cr, pattern);
		cairo_stroke (cr);
		cairo_pattern_destroy (pattern);
	}

	/* In current GTK+ focus and active cannot happen together, but we are robust against it. */
	if (params->focus && !params->active)
	{
		CairoColor focus_fill = tab->focus.color;
		CairoColor fill_shade1, fill_shade2, fill_shade3;
		CairoColor focus_border;

		double focus_inset_x = ((tab->gap_side == CL_GAP_TOP || tab->gap_side == CL_GAP_BOTTOM) ? 4 : stripe_size + 3);
		double focus_inset_y = ((tab->gap_side == CL_GAP_TOP || tab->gap_side == CL_GAP_BOTTOM) ? stripe_size + 3 : 4);
		double border_alpha = 0.54;
		double fill_alpha = 0.17;

		ge_shade_color (&focus_fill, 0.65, &focus_border);
		ge_shade_color (&focus_fill, 1.18, &fill_shade1);
		ge_shade_color (&focus_fill, 1.02, &fill_shade2);
		ge_shade_color (&focus_fill, 0.84, &fill_shade3);

		ge_cairo_rounded_rectangle (cr, focus_inset_x, focus_inset_y, width-focus_inset_x*2-1, height-focus_inset_y*2-1, radius-1, CR_CORNER_ALL);
		pattern = cairo_pattern_create_linear (0, 0, 0, height);

		cairo_pattern_add_color_stop_rgba (pattern, 0.0, fill_shade1.r, fill_shade1.g, fill_shade1.b, fill_alpha);
		cairo_pattern_add_color_stop_rgba (pattern, 0.5, fill_shade2.r, fill_shade2.g, fill_shade2.b, fill_alpha);
		cairo_pattern_add_color_stop_rgba (pattern, 0.5, focus_fill.r,   focus_fill.g, focus_fill.b,  fill_alpha);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, fill_shade3.r, fill_shade3.g, fill_shade3.b, fill_alpha);
		cairo_set_source (cr, pattern);
		cairo_fill_preserve (cr);

		cairo_pattern_destroy (pattern);

		clearlooks_set_mixed_color (cr, &params->parentbg, &focus_border, border_alpha);
		cairo_stroke (cr);
	}
}

static void
clearlooks_gummy_draw_separator (cairo_t                   *cr,
                                 const ClearlooksColors    *colors,
                                 const WidgetParameters    *widget,
                                 const SeparatorParameters *separator,
                                 int x, int y, int width, int height)
{
	CairoColor color = colors->shade[3];
	CairoColor hilight;
	ge_shade_color (&color, 1.3, &hilight);

	cairo_save (cr);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

	if (separator->horizontal)
	{
		cairo_set_line_width (cr, 1.0);
		cairo_translate      (cr, x, y+0.5);

		cairo_move_to        (cr, 0.0,   0.0);
		cairo_line_to        (cr, width, 0.0);
		ge_cairo_set_color   (cr, &color);
		cairo_stroke         (cr);

		cairo_move_to        (cr, 0.0,   1.0);
		cairo_line_to        (cr, width, 1.0);
		ge_cairo_set_color   (cr, &hilight);
		cairo_stroke         (cr);
	}
	else
	{
		cairo_set_line_width (cr, 1.0);
		cairo_translate      (cr, x+0.5, y);

		cairo_move_to        (cr, 0.0, 0.0);
		cairo_line_to        (cr, 0.0, height);
		ge_cairo_set_color   (cr, &color);
		cairo_stroke         (cr);

		cairo_move_to        (cr, 1.0, 0.0);
		cairo_line_to        (cr, 1.0, height);
		ge_cairo_set_color   (cr, &hilight);
		cairo_stroke         (cr);
	}

	cairo_restore (cr);
}

static void
clearlooks_gummy_draw_slider (cairo_t                *cr,
                              const ClearlooksColors *colors,
                              const WidgetParameters *params,
                              int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[7];
	CairoColor fill;
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;
	int bar_x, i;
	int shift_x;

	cairo_set_line_width (cr, 1.0);
	cairo_translate      (cr, x, y);

	ge_shade_color (&colors->bg[params->state_type], 1.0, &fill);
	if (params->prelight)
		ge_shade_color (&fill, 1.04, &fill);

	ge_shade_color (&fill, SHADE_TOP, &shade1);
	ge_shade_color (&fill, SHADE_CENTER_TOP, &shade2);
	ge_shade_color (&fill, SHADE_BOTTOM, &shade3);

	pattern = cairo_pattern_create_linear (1, 1, 1, height-2);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill.r, fill.g, fill.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, shade3.r, shade3.g, shade3.b);
	cairo_rectangle (cr, 1, 1, width-2, height-2);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	clearlooks_set_mixed_color (cr, border, &fill, 0.2);
	if (params->prelight)
		ge_cairo_set_color (cr, &colors->spot[2]);
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, 2.5, params->corners);
	cairo_stroke (cr);

	/* Handle */
	shift_x = (width%2 == 0 ? 1 : 0);
	bar_x = width/2-3+shift_x;
	cairo_translate (cr, 0.5, 0.5);
	ge_cairo_set_color (cr, border);
	for (i=0; i<3-shift_x; i++)
	{
		cairo_move_to (cr, bar_x, 4);
		cairo_line_to (cr, bar_x, height-5);
		bar_x += 3;
	}
	cairo_stroke (cr);

	params->style_functions->draw_top_left_highlight (cr, &fill, params, 1, 1, width-2, height-2, 2.0, params->corners);
}

static void
clearlooks_gummy_draw_slider_button (cairo_t                *cr,
                                     const ClearlooksColors *colors,
                                     const WidgetParameters *params,
                                     const SliderParameters *slider,
                                     int x, int y, int width, int height)
{
	double radius = MIN (params->radius, MIN ((width - 1.0) / 2.0, (height - 1.0) / 2.0));

	cairo_set_line_width (cr, 1.0);

	if (!slider->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	cairo_translate (cr, x, y);

	params->style_functions->draw_shadow (cr, colors, radius, width, height);
	params->style_functions->draw_slider (cr, colors, params, 1, 1, width-2, height-2);
}

static void
clearlooks_gummy_draw_scrollbar_stepper (cairo_t                          *cr,
                                         const ClearlooksColors           *colors,
                                         const WidgetParameters           *widget,
                                         const ScrollBarParameters        *scrollbar,
                                         const ScrollBarStepperParameters *stepper,
                                         int x, int y, int width, int height)
{
	CairoCorners corners = CR_CORNER_NONE;
	const CairoColor *border = &colors->shade[scrollbar->has_color ? 7 : 6];
	CairoColor fill;
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;
	double radius = MIN (widget->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));

	if (scrollbar->horizontal)
	{
		if (stepper->stepper == CL_STEPPER_A)
			corners = CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT;
		else if (stepper->stepper == CL_STEPPER_D)
			corners = CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT;
	}
	else
	{
		if (stepper->stepper == CL_STEPPER_A)
			corners = CR_CORNER_TOPLEFT | CR_CORNER_TOPRIGHT;
		else if (stepper->stepper == CL_STEPPER_D)
			corners = CR_CORNER_BOTTOMLEFT | CR_CORNER_BOTTOMRIGHT;
	}

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1);

	ge_cairo_rounded_rectangle (cr, 1, 1, width-2, height-2, radius, corners);

	if (scrollbar->horizontal)
		pattern = cairo_pattern_create_linear (0, 0, 0, height);
	else
		pattern = cairo_pattern_create_linear (0, 0, width, 0);

	fill = colors->bg[widget->state_type];
	ge_shade_color (&fill, SHADE_TOP, &shade1);
	ge_shade_color (&fill, SHADE_CENTER_TOP, &shade2);
	ge_shade_color (&fill, SHADE_BOTTOM, &shade3);

	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill.r, fill.g, fill.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, shade3.r, shade3.g, shade3.b);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	widget->style_functions->draw_top_left_highlight (cr, &fill, widget, 1, 1, width - 2, height - 2,
	                                                  radius, corners);

	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, corners);
	clearlooks_set_mixed_color (cr, border, &fill, 0.2);
	cairo_stroke (cr);
}

static void
clearlooks_gummy_draw_scrollbar_slider (cairo_t                   *cr,
                                        const ClearlooksColors    *colors,
                                        const WidgetParameters    *widget,
                                        const ScrollBarParameters *scrollbar,
                                        int x, int y, int width, int height)
{
	CairoColor fill = scrollbar->color;
	CairoColor border, handles;
	CairoColor hilight;
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;
	int bar_x, i;

	gdouble hue_scroll, brightness_scroll, saturation_scroll;
	gdouble hue_bg, brightness_bg, saturation_bg;

	ge_hsb_from_color (&fill, &hue_scroll, &saturation_scroll, &brightness_scroll);
	ge_hsb_from_color (&colors->bg[0], &hue_bg, &saturation_bg, &brightness_bg);

	/* Set the right color for border and handles */
	if ((fabs(saturation_scroll - saturation_bg) < 0.30) &&
	    (fabs(brightness_scroll - brightness_bg) < 0.20))
		ge_shade_color (&fill, 0.475, &border);
	else
		ge_shade_color (&fill, 0.575, &border);
	/* The following lines increase contrast when the HUE is between 25 and 195, */
	/* fixing a LOT of colorschemes! */
	if (scrollbar->has_color && (hue_scroll < 195) && (hue_scroll > 25))
		ge_shade_color (&border, 0.85, &border);

	handles = border;
	ge_mix_color (&border, &fill, scrollbar->has_color? 0.3 : 0.2, &border);

	if (scrollbar->junction & CL_JUNCTION_BEGIN)
	{
		if (scrollbar->horizontal)
		{
			x -= 1;
			width += 1;
		}
		else
		{
			y -= 1;
			height += 1;
		}
	}
	if (scrollbar->junction & CL_JUNCTION_END)
	{
		if (scrollbar->horizontal)
			width += 1;
		else
			height += 1;
	}

	if (!scrollbar->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	cairo_translate (cr, x, y);

	if (widget->prelight)
		ge_shade_color (&fill, 1.04, &fill);

	cairo_set_line_width (cr, 1);

	ge_shade_color (&fill, widget->style_constants->topleft_highlight_shade, &hilight);
	ge_shade_color (&fill, SHADE_TOP, &shade1);
	ge_shade_color (&fill, SHADE_CENTER_TOP, &shade2);
	ge_shade_color (&fill, SHADE_BOTTOM, &shade3);

	pattern = cairo_pattern_create_linear (1, 1, 1, height-2);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill.r,  fill.g,  fill.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1,   shade3.r, shade3.g, shade3.b);
	cairo_rectangle (cr, 1, 1, width-2, height-2);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	if (scrollbar->has_color)
	{
		cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.2);
		ge_cairo_stroke_rectangle (cr, 1.5, 1.5, width-3, height-3);
	}
	else
	{
		cairo_move_to (cr, 1.5, height-1.5);
		cairo_line_to (cr, 1.5, 1.5);
		cairo_line_to (cr, width-1.5, 1.5);
		cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, widget->style_constants->topleft_highlight_alpha);
		cairo_stroke (cr);
	}

	ge_cairo_set_color (cr, &border);
	ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);

	/* Handle */
	bar_x = width/2 - 4;
	cairo_translate(cr, 0.5, 0.5);
	ge_cairo_set_color (cr, &handles);
	for (i=0; i<3; i++)
	{
		cairo_move_to (cr, bar_x, 5);
		cairo_line_to (cr, bar_x, height-6);
		bar_x += 3;
	}
	cairo_stroke (cr);
}

static void
clearlooks_gummy_draw_list_view_header (cairo_t                        *cr,
                                        const ClearlooksColors         *colors,
                                        const WidgetParameters         *params,
                                        const ListViewHeaderParameters *header,
                                        int x, int y, int width, int height)
{
/*
	CairoColor *border = !params->prelight? (CairoColor*)&colors->shade[4] : (CairoColor*)&colors->spot[1];
*/
	const CairoColor *border = &colors->shade[4];
	const CairoColor *fill   = &colors->bg[params->state_type];
	CairoColor hilight;
	CairoColor shade1, shade2, shade3;

	cairo_pattern_t *pattern;

	ge_shade_color (fill, 1.11, &hilight);
	ge_shade_color (fill, LISTVIEW_SHADE_TOP, &shade1);
	ge_shade_color (fill, LISTVIEW_SHADE_CENTER_TOP, &shade2);
	ge_shade_color (fill, LISTVIEW_SHADE_BOTTOM, &shade3);

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1.0);

	/* Draw the fill */
	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill->r, fill->g, fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0-1.0/height, shade3.r, shade3.g, shade3.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0-1.0/height, border->r, border->g, border->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, border->r, border->g, border->b);

	cairo_set_source (cr, pattern);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	cairo_pattern_destroy (pattern);

	/* Draw highlight */
	if (header->order & CL_ORDER_FIRST)
	{
		cairo_move_to (cr, 0.5, height-1.5);
		cairo_line_to (cr, 0.5, 0.5);
	}
	else
		cairo_move_to (cr, 0.0, 0.5);

	cairo_line_to (cr, width, 0.5);

	ge_cairo_set_color (cr, &hilight);
	cairo_stroke (cr);

	/* Draw resize grip */
	if ((params->ltr && !(header->order & CL_ORDER_LAST)) ||
	    (!params->ltr && !(header->order & CL_ORDER_FIRST)) || header->resizable)
	{
		SeparatorParameters separator;
		separator.horizontal = FALSE;

		if (params->ltr)
			params->style_functions->draw_separator (cr, colors, params, &separator,
			                                         width-1.5, 4.0, 2, height-8.0);
		else
			params->style_functions->draw_separator (cr, colors, params, &separator,
			                                         1.5, 4.0, 2, height-8.0);
	}
}

static void
clearlooks_gummy_draw_toolbar (cairo_t                 *cr,
                               const ClearlooksColors  *colors,
                               const WidgetParameters  *widget,
                               const ToolbarParameters *toolbar,
                               int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->bg[GTK_STATE_NORMAL];
	const CairoColor *dark = &colors->shade[3];
	CairoColor light;
	ge_shade_color (fill, toolbar->style == 1 ? 1.1 : 1.05, &light);

	cairo_set_line_width (cr, 1.0);
	cairo_translate (cr, x, y);

	if (toolbar->style == 1) /* Enable Extra features */
	{
		cairo_pattern_t *pattern;
		CairoColor shade1, shade2, shade3;

		ge_shade_color (fill, TOOLBAR_SHADE_TOP, &shade1);
		ge_shade_color (fill, TOOLBAR_SHADE_CENTER_TOP, &shade2);
		ge_shade_color (fill, TOOLBAR_SHADE_BOTTOM, &shade3);

		/* Draw the fill */
		pattern = cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, shade1.r, shade1.g, shade1.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.5, shade2.r, shade2.g, shade2.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill->r, fill->g, fill->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, shade3.r, shade3.g, shade3.b);

		cairo_set_source (cr, pattern);
		cairo_rectangle (cr, 0, 0, width, height);
		cairo_fill (cr);

		cairo_pattern_destroy (pattern);
	}
	else /* Flat */
	{
		ge_cairo_set_color (cr, fill);
		cairo_paint (cr);
	}

	if (!toolbar->topmost)
	{
		/* Draw highlight */
		cairo_move_to       (cr, 0, 0.5);
		cairo_line_to       (cr, width-0.5, 0.5);
		ge_cairo_set_color  (cr, &light);
		cairo_stroke        (cr);
	}

	/* Draw shadow */
	cairo_move_to       (cr, 0, height-0.5);
	cairo_line_to       (cr, width-0.5, height-0.5);
	ge_cairo_set_color  (cr, dark);
	cairo_stroke        (cr);
}

static void
clearlooks_gummy_draw_menuitem (cairo_t                *cr,
                                const ClearlooksColors *colors,
                                const WidgetParameters *params,
                                int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->spot[1];
	const CairoColor *border = &colors->spot[2];
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;

	ge_shade_color (fill, SHADE_TOP, &shade1);
	ge_shade_color (fill, SHADE_CENTER_TOP, &shade2);
	ge_shade_color (fill, SHADE_BOTTOM, &shade3);
	cairo_set_line_width (cr, 1.0);

	ge_cairo_rounded_rectangle (cr, x+0.5, y+0.5, width - 1, height - 1, params->radius, params->corners);

	pattern = cairo_pattern_create_linear (x, y, x, y + height);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill->r,  fill->g,  fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1,	shade3.r, shade3.g, shade3.b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);
}

static void
clearlooks_gummy_draw_menubaritem (cairo_t                *cr,
                                   const ClearlooksColors *colors,
                                   const WidgetParameters *params,
                                   int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->spot[1];
	const CairoColor *border = &colors->spot[2];
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;

	ge_shade_color (fill, SHADE_TOP, &shade1);
	ge_shade_color (fill, SHADE_CENTER_TOP, &shade2);
	ge_shade_color (fill, SHADE_BOTTOM, &shade3);
	cairo_set_line_width (cr, 1.0);

	ge_cairo_rounded_rectangle (cr, x+0.5, y+0.5, width - 1, height - 1, params->radius, params->corners);

	pattern = cairo_pattern_create_linear (x, y, x, y + height);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill->r,  fill->g,  fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1,	shade3.r, shade3.g, shade3.b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);
}

static void
clearlooks_gummy_draw_selected_cell (cairo_t                *cr,
	                             const ClearlooksColors *colors,
	                             const WidgetParameters *params,
	                             int x, int y, int width, int height)
{
	CairoColor color;

	if (params->focus)
		color = colors->base[params->state_type];
	else
		color = colors->base[GTK_STATE_ACTIVE];

	clearlooks_draw_gummy_gradient (cr, x, y, width, height, &color, params->disabled, 0.0, CR_CORNER_NONE);
}

static void
clearlooks_gummy_draw_statusbar (cairo_t                *cr,
                                 const ClearlooksColors *colors,
                                 const WidgetParameters *widget,
                                 int x, int y, int width, int height)
{
	const CairoColor *dark = &colors->shade[3];
	CairoColor hilight;

	ge_shade_color (dark, 1.3, &hilight);

	cairo_set_line_width  (cr, 1);
	cairo_translate       (cr, x, y+0.5);
	cairo_move_to         (cr, 0, 0);
	cairo_line_to         (cr, width, 0);
	ge_cairo_set_color    (cr, dark);
	cairo_stroke          (cr);

	cairo_translate       (cr, 0, 1);
	cairo_move_to         (cr, 0, 0);
	cairo_line_to         (cr, width, 0);
	ge_cairo_set_color    (cr, &hilight);
	cairo_stroke          (cr);
}

static void
clearlooks_gummy_draw_radiobutton (cairo_t                  *cr,
                                   const ClearlooksColors   *colors,
                                   const WidgetParameters   *widget,
                                   const CheckboxParameters *checkbox,
                                   int x, int y, int width, int height)
{
	const CairoColor *border;
	const CairoColor *dot;
	CairoColor shadow;
	CairoColor highlight;
	cairo_pattern_t *pt;
	gboolean inconsistent;
	gboolean draw_bullet = (checkbox->shadow_type == GTK_SHADOW_IN);
	gdouble w, h, cx, cy, radius;

	w = (gdouble) width;
	h = (gdouble) height;
	cx = width / 2.0;
	cy = height / 2.0;
	radius = MIN (width, height) / 2.0;

	inconsistent = (checkbox->shadow_type == GTK_SHADOW_ETCHED_IN);
	draw_bullet |= inconsistent;

	if (widget->disabled)
	{
		border = &colors->shade[5];
		dot    = &colors->shade[6];
	}
	else
	{
		if (widget->prelight)
			border = &colors->spot[2];
		else
			border = &colors->shade[6];
		dot    = &colors->text[0];
	}

	ge_shade_color (&widget->parentbg, 0.9, &shadow);
	ge_shade_color (&widget->parentbg, 1.1, &highlight);

	pt = cairo_pattern_create_linear (0, 0, radius * 2.0, radius * 2.0);
	cairo_pattern_add_color_stop_rgb (pt, 0.0, shadow.r, shadow.b, shadow.g);
	cairo_pattern_add_color_stop_rgba (pt, 0.5, shadow.r, shadow.b, shadow.g, 0.5);
	cairo_pattern_add_color_stop_rgba (pt, 0.5, highlight.r, highlight.g, highlight.b, 0.5);
	cairo_pattern_add_color_stop_rgb (pt, 1.0, highlight.r, highlight.g, highlight.b);

	cairo_translate (cr, x, y);

	cairo_set_line_width (cr, MAX (1.0, floor (radius/3)));
	cairo_arc (cr, ceil (cx), ceil (cy), floor (radius - 0.1), 0, G_PI*2);
	cairo_set_source (cr, pt);
	cairo_stroke (cr);
	cairo_pattern_destroy (pt);

	cairo_set_line_width (cr, MAX (1.0, floor (radius/6)));

	cairo_arc (cr, ceil (cx), ceil (cy), MAX (1.0, ceil (radius) - 1.5), 0, G_PI*2);

	if (!widget->disabled)
	{
		if (widget->prelight)
			clearlooks_set_mixed_color (cr, &colors->base[0], &colors->spot[1], 0.5);
		else
			ge_cairo_set_color (cr, &colors->base[0]);
		cairo_fill_preserve (cr);
	}

	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);

	if (draw_bullet)
	{
		if (inconsistent)
		{
			cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
			cairo_set_line_width (cr, ceil (radius * 2 / 3));

			cairo_move_to (cr, ceil (cx - radius/3.0), ceil (cy));
			cairo_line_to (cr, ceil (cx + radius/3.0), ceil (cy));

			ge_cairo_set_color (cr, dot);
			cairo_stroke (cr);
		}
		else
		{
			cairo_arc (cr, ceil (cx), ceil (cy), floor (radius/2.0), 0, G_PI*2);
			ge_cairo_set_color (cr, dot);
			cairo_fill (cr);

			cairo_arc (cr, floor (cx - radius/10.0), floor (cy - radius/10.0), floor (radius/6.0), 0, G_PI*2);
			cairo_set_source_rgba (cr, highlight.r, highlight.g, highlight.b, 0.5);
			cairo_fill (cr);
		}
	}
}

static void
clearlooks_gummy_draw_checkbox (cairo_t                  *cr,
                                const ClearlooksColors   *colors,
                                const WidgetParameters   *widget,
                                const CheckboxParameters *checkbox,
                                int x, int y, int width, int height)
{
	const CairoColor *border;
	const CairoColor *dot;
	gboolean inconsistent = FALSE;
	gboolean draw_bullet = (checkbox->shadow_type == GTK_SHADOW_IN);

	inconsistent = (checkbox->shadow_type == GTK_SHADOW_ETCHED_IN);
	draw_bullet |= inconsistent;

	if (widget->disabled)
	{
		border = &colors->shade[5];
		dot    = &colors->shade[6];
	}
	else
	{
		if (widget->prelight)
			border = &colors->spot[2];
		else
			border = &colors->shade[6];
		dot    = &colors->text[GTK_STATE_NORMAL];
	}

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1);

	if (widget->xthickness > 2 && widget->ythickness > 2)
	{
		widget->style_functions->draw_inset (cr, &widget->parentbg, 0, 0,
		                                     width, height, (widget->radius > 0)? 1 : 0, CR_CORNER_ALL);

		/* Draw the rectangle for the checkbox itself */
		ge_cairo_rounded_rectangle (cr, 1.5, 1.5,
		                            width-3, height-3, (widget->radius > 0)? 1 : 0, CR_CORNER_ALL);
	}
	else
	{
		/* Draw the rectangle for the checkbox itself */
		ge_cairo_rounded_rectangle (cr, 0.5, 0.5,
		                            width-1, height-1, (widget->radius > 0)? 1 : 0, CR_CORNER_ALL);
	}

	if (!widget->disabled)
	{
		if (widget->prelight)
			clearlooks_set_mixed_color (cr, &colors->base[0], &colors->spot[1], 0.5);
		else
			ge_cairo_set_color (cr, &colors->base[0]);
		cairo_fill_preserve (cr);
	}

	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);

	if (draw_bullet)
	{
		if (inconsistent) /* Inconsistent */
		{
			cairo_set_line_width (cr, 2.0);
			cairo_move_to (cr, 3, height*0.5);
			cairo_line_to (cr, width-3, height*0.5);
		}
		else
		{
			cairo_set_line_width (cr, 1.7);
			cairo_move_to (cr, 0.5 + (width*0.2), (height*0.5));
			cairo_line_to (cr, 0.5 + (width*0.4), (height*0.7));

			cairo_curve_to (cr, 0.5 + (width*0.4), (height*0.7),
			                    0.5 + (width*0.5), (height*0.4),
			                    0.5 + (width*0.70), (height*0.25));
		}

		ge_cairo_set_color (cr, dot);
		cairo_stroke (cr);
	}
}

static void
clearlooks_gummy_draw_focus (cairo_t                *cr,
                             const ClearlooksColors *colors,
                             const WidgetParameters *widget,
                             const FocusParameters  *focus,
                             int x, int y, int width, int height)
{
	CairoColor fill = focus->color;
	CairoColor fill_shade1, fill_shade2, fill_shade3;
	CairoColor border;
	CairoColor parentbg = widget->parentbg;

	/* Default values */
	double xoffset = 1.5;
	double yoffset = 1.5;
	double radius = widget->radius-1.0;
	double border_alpha = 0.64;
	double fill_alpha = 0.18;
	double shadow_alpha = 0.5;
	boolean focus_fill = TRUE;
	boolean focus_border = TRUE;
	boolean focus_shadow = FALSE;

	ge_shade_color (&fill, 0.65, &border);
	ge_shade_color (&fill, 1.18, &fill_shade1);
	ge_shade_color (&fill, 1.02, &fill_shade2);
	ge_shade_color (&fill, 0.84, &fill_shade3);

	/* Do some useful things to adjust focus */
	switch (focus->type)
	{
		case CL_FOCUS_BUTTON:
			xoffset = -1.5-(focus->padding);
			yoffset = -1.5-(focus->padding);
			radius++;
			border_alpha = 0.9;
			fill_alpha = 0.12;
			if (!widget->active)
				focus_shadow = TRUE;
			break;
		case CL_FOCUS_BUTTON_FLAT:
			xoffset = -1.5-(focus->padding);
			yoffset = -1.5-(focus->padding);
			radius++;
			if (widget->active || widget->prelight)
			{
				border_alpha = 0.9;
				fill_alpha = 0.12;
				if (!widget->active)
					focus_shadow = TRUE;
			}
			break;
		case CL_FOCUS_LABEL:
			xoffset = 0.5;
			yoffset = 0.5;
			break;
		case CL_FOCUS_TREEVIEW:
			parentbg = colors->base[widget->state_type];
			xoffset = -1.5;
			yoffset = -1.5;
			fill_alpha = 0.08;
			focus_border = FALSE;
			break;
		case CL_FOCUS_TREEVIEW_DND:
			parentbg = colors->base[widget->state_type];
			break;
		case CL_FOCUS_TREEVIEW_HEADER:
			cairo_translate (cr, -1, 0);
			break;
		case CL_FOCUS_TREEVIEW_ROW:
			parentbg = colors->base[widget->state_type];
			xoffset = -2.5; /* hack to hide vertical lines */
			yoffset = 0.5;
			radius = CLAMP (radius, 0.0, 2.0);
			border_alpha = 0.7;
			focus_fill = FALSE;
			break;
		case CL_FOCUS_TAB:
			/* In current GTK+ focus and active cannot happen together, but we are robust against it.
			 * IF the application sets the state to ACTIVE while drawing the tabs focus. */
			if (widget->focus && !widget->active)
				return;
			break;
		case CL_FOCUS_SCALE:
			break;
		case CL_FOCUS_UNKNOWN:
			/* Fallback to classic function, dots */
			clearlooks_draw_focus (cr, colors, widget, focus, x, y, width, height);
			return;
			break;
		default:
			break;
	};

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, focus->line_width);

	ge_cairo_rounded_rectangle (cr, xoffset, yoffset, width-(xoffset*2), height-(yoffset*2), radius, widget->corners);

	if (focus_fill)
	{
		cairo_pattern_t *pattern;

		pattern = cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, fill_shade1.r, fill_shade1.g, fill_shade1.b, fill_alpha);
		cairo_pattern_add_color_stop_rgba (pattern, 0.5, fill_shade2.r, fill_shade2.g, fill_shade2.b, fill_alpha);
		cairo_pattern_add_color_stop_rgba (pattern, 0.5, fill.r,        fill.g,        fill.b,        fill_alpha);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, fill_shade3.r, fill_shade3.g, fill_shade3.b, fill_alpha);
		cairo_set_source (cr, pattern);
		cairo_fill_preserve (cr);

		cairo_pattern_destroy (pattern);
	}

	if (focus_border)
	{
		clearlooks_set_mixed_color (cr, &parentbg, &border, border_alpha);
		cairo_stroke (cr);
	}

	if (focus_shadow)
	{
		if (radius > 0)
			radius++;
		ge_cairo_rounded_rectangle (cr, xoffset-1, yoffset-1, width-(xoffset*2)+2, height-(yoffset*2)+2, radius, widget->corners);
		clearlooks_set_mixed_color (cr, &parentbg, &fill, shadow_alpha);
		cairo_stroke (cr);
	}
}

void
clearlooks_register_style_gummy (ClearlooksStyleFunctions *functions, ClearlooksStyleConstants *constants)
{
	functions->draw_button             = clearlooks_gummy_draw_button;
	functions->draw_entry              = clearlooks_gummy_draw_entry;
	functions->draw_progressbar_trough = clearlooks_gummy_draw_progressbar_trough;
	functions->draw_progressbar_fill   = clearlooks_gummy_draw_progressbar_fill;
	functions->draw_scale_trough       = clearlooks_gummy_draw_scale_trough;
	functions->draw_tab                = clearlooks_gummy_draw_tab;
	functions->draw_separator          = clearlooks_gummy_draw_separator;
	functions->draw_slider             = clearlooks_gummy_draw_slider;
	functions->draw_slider_button      = clearlooks_gummy_draw_slider_button;
	functions->draw_scrollbar_stepper  = clearlooks_gummy_draw_scrollbar_stepper;
	functions->draw_scrollbar_slider   = clearlooks_gummy_draw_scrollbar_slider;
	functions->draw_list_view_header   = clearlooks_gummy_draw_list_view_header;
	functions->draw_toolbar            = clearlooks_gummy_draw_toolbar;
	functions->draw_menuitem           = clearlooks_gummy_draw_menuitem;
	functions->draw_menubaritem        = clearlooks_gummy_draw_menubaritem;
	functions->draw_selected_cell      = clearlooks_gummy_draw_selected_cell;
	functions->draw_statusbar          = clearlooks_gummy_draw_statusbar;
	functions->draw_checkbox           = clearlooks_gummy_draw_checkbox;
	functions->draw_radiobutton        = clearlooks_gummy_draw_radiobutton;
	functions->draw_focus              = clearlooks_gummy_draw_focus;

	constants->topleft_highlight_shade = 1.3;
	constants->topleft_highlight_alpha = 0.4;
}
