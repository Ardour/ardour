/* Clearlooks theme engine
 * Copyright (C) 2006 Richard Stellingwerff
 * Copyright (C) 2006 Daniel Borgman
 * Copyright (C) 2007 Benjamin Berg
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
 */

#include "clearlooks_draw.h"
#include "clearlooks_style.h"
#include "clearlooks_types.h"

#include "support.h"
#include <ge-support.h>
#include <math.h>

#include <cairo.h>

/* Normal shadings */
#define SHADE_TOP 1.055
#define SHADE_CENTER_TOP 1.01
#define SHADE_CENTER_BOTTOM 0.98
#define SHADE_BOTTOM 0.90

typedef void (*menubar_draw_proto) (cairo_t *cr,
                                    const ClearlooksColors *colors,
                                    const WidgetParameters *params,
                                    const MenuBarParameters *menubar,
                                    int x, int y, int width, int height);

static void
clearlooks_draw_inset (cairo_t          *cr,
                       const CairoColor *bg_color,
                       double x, double y, double width, double height,
                       double radius, uint8 corners)
{
	CairoColor shadow;
	CairoColor highlight;
	double line_width;
	double min = MIN (width, height);

	line_width = cairo_get_line_width (cr);

	/* not really sure of shading ratios... we will think */
	ge_shade_color (bg_color, 0.94, &shadow);
	ge_shade_color (bg_color, 1.06, &highlight);

	/* highlight */
	cairo_save (cr);

	cairo_move_to (cr, x, y + height);
	cairo_line_to (cr, x + min / 2.0, y + height - min / 2.0);
	cairo_line_to (cr, x + width - min / 2.0, y + min / 2.0);
	cairo_line_to (cr, x + width, y);
	cairo_line_to (cr, x, y);
	cairo_close_path (cr);
	
	cairo_clip (cr);

	ge_cairo_rounded_rectangle (cr, x + line_width / 2.0, y + line_width / 2.0,
	                            width - line_width, height - line_width,
	                            radius, corners);

	ge_cairo_set_color (cr, &shadow);
	cairo_stroke (cr);
	
	cairo_restore (cr);

	/* shadow */
	cairo_save (cr);

	cairo_move_to (cr, x, y + height);
	cairo_line_to (cr, x + min / 2.0, y + height - min / 2.0);
	cairo_line_to (cr, x + width - min / 2.0, y + min / 2.0);
	cairo_line_to (cr, x + width, y);
	cairo_line_to (cr, x + width, y + height);
	cairo_close_path (cr);
	
	cairo_clip (cr);

	ge_cairo_rounded_rectangle (cr, x + line_width / 2.0, y + line_width / 2.0,
	                            width - line_width, height - line_width,
	                            radius, corners);

	ge_cairo_set_color (cr, &highlight);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_shadow (cairo_t *cr, const ClearlooksColors *colors, gfloat radius, int width, int height)
{
	CairoColor shadow;
	cairo_save (cr);

	ge_shade_color (&colors->shade[6], 0.92, &shadow);

	cairo_set_line_width (cr, 1.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

	cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, 0.1);

	cairo_move_to (cr, width - 0.5, radius);
	ge_cairo_rounded_corner (cr, width - 0.5, height - 0.5, radius, CR_CORNER_BOTTOMRIGHT);
	cairo_line_to (cr, radius, height - 0.5);

	cairo_stroke (cr);
	cairo_restore (cr);
}

/* This is copied at least in clearlooks_draw_gummy.c.
 * KEEP IN SYNC IF POSSIBLE! */
static void
clearlooks_draw_top_left_highlight (cairo_t *cr, const CairoColor *color,
                                    const WidgetParameters *params,
                                    int x, int y, int width, int height,
                                    gdouble radius, CairoCorners corners)
{
	CairoColor hilight;

	double line_width = cairo_get_line_width (cr);
	double offset = line_width / 2.0;
	double light_top, light_bottom, light_left, light_right;

	cairo_save (cr);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

	light_top = y + offset;
	light_bottom = y + height;
	light_left = x + offset;
	light_right = x + width;
	
	if (corners & CR_CORNER_BOTTOMLEFT)
		light_bottom -= radius;
	if (corners & CR_CORNER_TOPRIGHT)
		light_right -= radius;

	ge_shade_color (color, params->style_constants->topleft_highlight_shade, &hilight);
	cairo_move_to         (cr, light_left, light_bottom);

	ge_cairo_rounded_corner (cr, light_left, light_top, radius, corners & CR_CORNER_TOPLEFT);

	cairo_line_to         (cr, light_right, light_top);
	cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, params->style_constants->topleft_highlight_alpha);
	cairo_stroke          (cr);

	cairo_restore (cr);
}

#ifdef DEVELOPMENT
#warning seems to be very slow in scrollbar_stepper
#endif

static void
clearlooks_draw_highlight_and_shade (cairo_t *cr, const ClearlooksColors *colors,
                                     const ShadowParameters *params,
                                     int width, int height, gdouble radius)
{
	CairoColor hilight;
	CairoColor shadow;
	uint8 corners = params->corners;
	double x = 1.0;
	double y = 1.0;

	ge_shade_color (&colors->bg[0], 1.06, &hilight);
	ge_shade_color (&colors->bg[0], 0.94, &shadow);

	width  -= 2;
	height -= 2;

	cairo_save (cr);

	/* Top/Left highlight */
	if (corners & CR_CORNER_BOTTOMLEFT)
		cairo_move_to (cr, x + 0.5, y+height-radius);
	else
		cairo_move_to (cr, x + 0.5, y+height);

	ge_cairo_rounded_corner (cr, x + 0.5, y + 0.5, radius, corners & CR_CORNER_TOPLEFT);

	if (corners & CR_CORNER_TOPRIGHT)
		cairo_line_to (cr, x+width-radius, y + 0.5);
	else
		cairo_line_to (cr, x+width, y + 0.5);

	if (params->shadow & CL_SHADOW_OUT)
		ge_cairo_set_color (cr, &hilight);
	else
		ge_cairo_set_color (cr, &shadow);

	cairo_stroke (cr);

	/* Bottom/Right highlight -- this includes the corners */
	cairo_arc (cr, x + width - 0.5 - radius, y + radius, radius, G_PI * (3/2.0+1/4.0), G_PI * 2);
	ge_cairo_rounded_corner (cr, x+width - 0.5, y+height - 0.5, radius, corners & CR_CORNER_BOTTOMRIGHT);
	cairo_arc (cr, x + radius, y + height - 0.5 - radius, radius, G_PI * 1/2, G_PI * 3/4);

	if (params->shadow & CL_SHADOW_OUT)
		ge_cairo_set_color (cr, &shadow);
	else
		ge_cairo_set_color (cr, &hilight);

	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_set_border_gradient (cairo_t *cr, const CairoColor *color, double hilight, int width, int height)
{
	cairo_pattern_t *pattern;

	CairoColor bottom_shade;
	ge_shade_color (color, hilight, &bottom_shade);

	pattern	= cairo_pattern_create_linear (0, 0, width, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0, color->r, color->g, color->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1, bottom_shade.r, bottom_shade.g, bottom_shade.b);

	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);
}

static void
clearlooks_draw_gripdots (cairo_t *cr, const ClearlooksColors *colors, int x, int y,
                          int width, int height, int xr, int yr,
                          float contrast)
{
	const CairoColor *dark = &colors->shade[4];
	CairoColor hilight;
	int i, j;
	int xoff, yoff;
	int x_start, y_start;

	ge_shade_color (dark, 1.5, &hilight);
	
	/* The "- 1" is because there is no space in front of the first dot. */
	x_start = x + width / 2 - ((xr * 3 - 1) / 2);
	y_start = y + height / 2 - ((yr * 3 - 1) / 2);
	
	for ( i = 0; i < xr; i++ )
	{
		for ( j = 0; j < yr; j++ )
		{
			xoff = 3 * i;
			yoff = 3 * j;

			cairo_rectangle (cr, x_start + xoff, y_start + yoff, 2, 2);
			cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.8+contrast);
			cairo_fill (cr);
			cairo_rectangle (cr, x_start + xoff, y_start + yoff, 1, 1);
			cairo_set_source_rgba (cr, dark->r, dark->g, dark->b, 0.8+contrast);
			cairo_fill (cr);
		}
	}
}

static void
clearlooks_draw_button (cairo_t *cr,
                        const ClearlooksColors *colors,
                        const WidgetParameters *params,
                        int x, int y, int width, int height)
{
	double xoffset = 0, yoffset = 0;
	double radius = params->radius;
	const CairoColor *fill = &colors->bg[params->state_type];
	CairoColor border_normal = colors->shade[6];
	CairoColor border_disabled = colors->shade[4];

	CairoColor shadow;
	ge_shade_color (&border_normal, 1.04, &border_normal);
	ge_shade_color (&border_normal, 0.94, &shadow);
	ge_shade_color (&border_disabled, 1.08, &border_disabled);

	cairo_save (cr);

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1.0);

	if (params->xthickness == 3 || params->ythickness == 3)
	{
		if (params->xthickness == 3)
			xoffset = 1;
		if (params->ythickness == 3)
			yoffset = 1;
	}

	radius = MIN (radius, MIN ((width - 2.0 - xoffset * 2.0) / 2.0, (height - 2.0 - yoffset * 2) / 2.0));

	if (params->xthickness == 3 || params->ythickness == 3)
	{
		params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, width, height, radius+1, params->corners);
	}

	ge_cairo_rounded_rectangle (cr, xoffset+1, yoffset+1,
	                                     width-(xoffset*2)-2,
	                                     height-(yoffset*2)-2,
	                                     radius, params->corners);

	if (!params->active)
	{
		cairo_pattern_t *pattern;
		CairoColor top_shade, topmiddle_shade, bottom_shade, middle_shade;

		ge_shade_color (fill, SHADE_TOP, &top_shade);
		ge_shade_color (fill, SHADE_CENTER_TOP, &topmiddle_shade);
		ge_shade_color (fill, SHADE_CENTER_BOTTOM, &middle_shade);
		ge_shade_color (fill, SHADE_BOTTOM, &bottom_shade);

		cairo_save (cr);
		cairo_clip_preserve (cr);

		pattern	= cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, top_shade.r, top_shade.g, top_shade.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.3, topmiddle_shade.r, topmiddle_shade.g, topmiddle_shade.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.7, middle_shade.r, middle_shade.g, middle_shade.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, bottom_shade.r, bottom_shade.g, bottom_shade.b);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);

		cairo_move_to (cr, width-(xoffset*2)-0.5, 0);
		cairo_line_to (cr, width-(xoffset*2)-0.5, height);
		ge_cairo_set_color (cr, &bottom_shade);
		cairo_stroke (cr);

		/* Draw topleft shadow */
		params->style_functions->draw_top_left_highlight (cr, fill, params, xoffset + 1, yoffset + 1,
		                                                  width - 2*(xoffset + 1), height - 2*(yoffset + 1),
		                                                  MAX(radius-1, 0), params->corners);

		cairo_restore (cr);
	}
	else
	{
		cairo_pattern_t *pattern;

		ge_cairo_set_color (cr, fill);
		cairo_fill_preserve (cr);

		pattern	= cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_pattern_add_color_stop_rgba (pattern, 0.4, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.2);
		cairo_set_source (cr, pattern);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pattern);

		pattern	= cairo_pattern_create_linear (0, yoffset+1, 0, 3+yoffset);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, params->disabled ? 0.125 : 0.32);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pattern);

		pattern	= cairo_pattern_create_linear (xoffset+1, 0, 3+xoffset, 0);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, params->disabled ? 0.125 : 0.32);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}

	/* Drawing the border */
	if (!params->active && params->is_default)
	{
		ge_shade_color (&border_normal, 0.74, &border_normal);
	}

	ge_cairo_inner_rounded_rectangle (cr, xoffset, yoffset, width-(xoffset*2), height-(yoffset*2), radius, params->corners);

	if (params->disabled)
	{
		ge_cairo_set_color (cr, &border_disabled);
	}
	else
	{
		if (!params->active)
			clearlooks_set_border_gradient (cr, &border_normal,
			                                params->is_default ? 1.1 : 1.3, 0, height);
		else
		{
			ge_shade_color (&border_normal, 1.08, &border_normal);
			ge_cairo_set_color (cr, &border_normal);
		}
	}

	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_entry (cairo_t *cr,
                       const ClearlooksColors *colors,
                       const WidgetParameters *params,
                       int x, int y, int width, int height)
{
	const CairoColor *base = &colors->base[params->state_type];
	CairoColor border = colors->shade[params->disabled ? 3 : 6];
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
		ge_cairo_set_color (cr, &colors->spot[0]);
		ge_cairo_inner_rounded_rectangle (cr, 2, 2, width-4, height-4, MAX(0, radius-1), params->corners);
		cairo_stroke (cr);
	}
	else
	{
		CairoColor shadow;
		ge_shade_color (&border, 0.925, &shadow);

		cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, params->disabled ? 0.05 : 0.1);

		cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
		cairo_move_to (cr, 2.5, height-radius);
		cairo_arc (cr, 2.5+MAX(0, radius-1), 2.5+MAX(0, radius-1), MAX(0, radius-1), G_PI, 270*(G_PI/180));
		cairo_line_to (cr, width-radius, 2.5);
		cairo_stroke (cr);
	}

	ge_cairo_inner_rounded_rectangle (cr, 1, 1, width-2, height-2, radius, params->corners);
	if (params->focus || params->disabled)
		ge_cairo_set_color (cr, &border);
	else
		clearlooks_set_border_gradient (cr, &border, 1.32, 0, height);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_spinbutton (cairo_t *cr,
                            const ClearlooksColors *colors,
                            const WidgetParameters *params,
                            int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[!params->disabled ? 5 : 3];
	CairoColor hilight;

	params->style_functions->draw_button (cr, colors, params, x, y, width, height);

	ge_shade_color (&colors->bg[0], params->style_constants->topleft_highlight_shade, &hilight);
	hilight.a = params->style_constants->topleft_highlight_alpha;

	cairo_translate (cr, x, y);

	cairo_move_to (cr, params->xthickness + 0.5,       (height/2) + 0.5);
	cairo_line_to (cr, width-params->xthickness - 0.5, (height/2) + 0.5);
	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);

	cairo_move_to (cr, params->xthickness + 0.5,       (height/2)+1.5);
	cairo_line_to (cr, width-params->xthickness - 0.5, (height/2)+1.5);
	ge_cairo_set_color (cr, &hilight);
	cairo_stroke (cr);
}

static void
clearlooks_draw_spinbutton_down (cairo_t *cr,
                                 const ClearlooksColors *colors,
                                 const WidgetParameters *params,
                                 int x, int y, int width, int height)
{
	cairo_pattern_t *pattern;
	double radius = MIN (params->radius, MIN ((width - 4.0) / 2.0, (height - 4.0) / 2.0));
	CairoColor shadow;
	ge_shade_color (&colors->bg[0], 0.8, &shadow);

	cairo_translate (cr, x+1, y+1);

	ge_cairo_rounded_rectangle (cr, 1, 1, width-4, height-4, radius, params->corners);

	ge_cairo_set_color (cr, &colors->bg[params->state_type]);

	cairo_fill_preserve (cr);

	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, shadow.r, shadow.g, shadow.b);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);

	cairo_set_source (cr, pattern);
	cairo_fill (cr);

	cairo_pattern_destroy (pattern);
}

static void
clearlooks_scale_draw_gradient (cairo_t *cr,
                                const CairoColor *c1,
                                const CairoColor *c2,
                                const CairoColor *c3,
                                int x, int y, int width, int height,
                                boolean horizontal)
{
	cairo_pattern_t *pattern;

	pattern = cairo_pattern_create_linear (0.5, 0.5, horizontal ? 0.5 :  width + 1, horizontal ? height + 1: 0.5);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, c1->r, c1->g, c1->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, c2->r, c2->g, c2->b);

	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, c3);
	ge_cairo_inner_rectangle (cr, x, y, width, height);
	cairo_stroke (cr);
}

#define TROUGH_SIZE 7
static void
clearlooks_draw_scale_trough (cairo_t *cr,
                              const ClearlooksColors *colors,
                              const WidgetParameters *params,
                              const SliderParameters *slider,
                              int x, int y, int width, int height)
{
	int     trough_width, trough_height;
	double  translate_x, translate_y;

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
		translate_y  = y;
	}

	cairo_set_line_width (cr, 1.0);
	cairo_translate (cr, translate_x, translate_y);

	if (!slider->fill_level)
		params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, trough_width, trough_height, 0, 0);
	
	if (!slider->lower && !slider->fill_level)
	{
		CairoColor shadow;
		ge_shade_color (&colors->shade[2], 0.96, &shadow);

		clearlooks_scale_draw_gradient (cr, &shadow, /* top */
		                                &colors->shade[2], /* bottom */
		                                &colors->shade[4], /* border */
		                                1.0, 1.0, trough_width - 2, trough_height - 2,
		                                slider->horizontal);
	}
	else
	{
		CairoColor border = colors->spot[2];
		border.a = 0.64;

		clearlooks_scale_draw_gradient (cr, &colors->spot[1], /* top */
		                                &colors->spot[0], /* bottom */
		                                &border, /* border */
		                                1.0, 1.0, trough_width - 2, trough_height - 2,
		                                slider->horizontal);
	}
	cairo_restore (cr);
}

static void
clearlooks_draw_slider (cairo_t *cr,
                        const ClearlooksColors *colors,
                        const WidgetParameters *params,
                        int x, int y, int width, int height)
{
	const CairoColor *spot   = &colors->spot[1];
	const CairoColor *fill   = &colors->shade[2];
	CairoColor border = colors->shade[params->disabled ? 4 : 6];
	double radius = MIN (params->radius, MIN ((width - 1.0) / 2.0, (height - 1.0) / 2.0));

	cairo_pattern_t *pattern;

	cairo_set_line_width (cr, 1.0);
	cairo_translate      (cr, x, y);

	if (params->prelight)
		border = colors->spot[2];

	/* fill the widget */
	ge_cairo_rounded_rectangle (cr, 1.0, 1.0, width-2, height-2, radius, params->corners);

	/* Fake light */
	if (!params->disabled)
	{
		const CairoColor *top = &colors->shade[0];
		const CairoColor *bot = &colors->shade[2];

		pattern	= cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgb (pattern, 0.0,  top->r, top->g, top->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0,  bot->r, bot->g, bot->b);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}
	else
	{
		ge_cairo_set_color (cr, fill);
		cairo_fill         (cr);
	}

	/* Set the clip */
	cairo_save (cr);
	cairo_rectangle (cr, 1.0, 1.0, 6, height-2);
	cairo_rectangle (cr, width-7.0, 1.0, 6, height-2);
	cairo_clip_preserve (cr);

	cairo_new_path (cr);

	/* Draw the handles */
	ge_cairo_rounded_rectangle (cr, 1.0, 1.0, width-1, height-1, radius, params->corners);
	pattern = cairo_pattern_create_linear (1.0, 1.0, 1.0, 1.0+height);

	if (params->prelight)
	{
		CairoColor highlight;
		ge_shade_color (spot, 1.3, &highlight);
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, highlight.r, highlight.g, highlight.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, spot->r, spot->g, spot->b);
		cairo_set_source (cr, pattern);
	}
	else
	{
		CairoColor hilight;
		ge_shade_color (fill, 1.3, &hilight);
		cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
	}

	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	cairo_restore (cr);

	/* Draw the border */
	ge_cairo_inner_rounded_rectangle (cr, 0, 0, width, height, radius, params->corners);

	if (params->prelight || params->disabled)
		ge_cairo_set_color (cr, &border);
	else
		clearlooks_set_border_gradient (cr, &border, 1.2, 0, height);
	cairo_stroke (cr);

	/* Draw handle lines */
	if (width > 14)
	{
		cairo_move_to (cr, 6.5, 1.0);
		cairo_line_to (cr, 6.5, height-1);

		cairo_move_to (cr, width-6.5, 1.0);
		cairo_line_to (cr, width-6.5, height-1);

		cairo_set_line_width (cr, 1.0);
		border.a = params->disabled ? 0.6 : 0.3;
		ge_cairo_set_color (cr, &border);
		cairo_stroke (cr);
	}
}

static void
clearlooks_draw_slider_button (cairo_t *cr,
                               const ClearlooksColors *colors,
                               const WidgetParameters *params,
                               const SliderParameters *slider,
                               int x, int y, int width, int height)
{
	double radius = MIN (params->radius, MIN ((width - 1.0) / 2.0, (height - 1.0) / 2.0));

	cairo_save (cr);
	cairo_set_line_width (cr, 1.0);

	if (!slider->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);
	cairo_translate (cr, x, y);

	params->style_functions->draw_shadow (cr, colors, radius, width, height);
	params->style_functions->draw_slider (cr, colors, params, 1, 1, width-2, height-2);

	if (width > 24)
		params->style_functions->draw_gripdots (cr, colors, 1, 1, width-2, height-2, 3, 3, 0);

	cairo_restore (cr);
}

static void
clearlooks_draw_progressbar_trough (cairo_t *cr,
                                    const ClearlooksColors *colors,
                                    const WidgetParameters *params,
                                    int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[4];
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
	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);

	/* clip the corners of the shadows */
	ge_cairo_rounded_rectangle (cr, x+1, y+1, width-2, height-2, radius, params->corners);
	cairo_clip (cr);

	ge_shade_color (border, 0.925, &shadow);

	/* Top shadow */
	cairo_rectangle (cr, x+1, y+1, width-2, 4);
	pattern = cairo_pattern_create_linear (x, y, x, y+4);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.2);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	/* Left shadow */
	cairo_rectangle (cr, x+1, y+1, 4, height-2);
	pattern = cairo_pattern_create_linear (x, y, x+4, y);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.2);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	cairo_restore (cr);
}

static void
clearlooks_draw_progressbar_fill (cairo_t *cr,
                                  const ClearlooksColors *colors,
                                  const WidgetParameters *params,
                                  const ProgressBarParameters *progressbar,
                                  int x, int y, int width, int height,
                                  gint offset)
{
	boolean      is_horizontal = progressbar->orientation < 2;
	double       tile_pos = 0;
	double       stroke_width;
	double       radius;
	int          x_step;

	cairo_pattern_t *pattern;
	CairoColor       bg_shade;
	CairoColor       border;
	CairoColor       shadow;

	radius = MAX (0, params->radius - params->xthickness);

	cairo_save (cr);

	if (!is_horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	if ((progressbar->orientation == CL_ORIENTATION_RIGHT_TO_LEFT) || (progressbar->orientation == CL_ORIENTATION_BOTTOM_TO_TOP))
		ge_cairo_mirror (cr, CR_MIRROR_HORIZONTAL, &x, &y, &width, &height);

	/* Clamp the radius so that the _height_ fits ...  */
	radius = MIN (radius, height / 2.0);

	stroke_width = height;
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
	ge_shade_color (&colors->spot[1], 1.1, &bg_shade);

	/* Just leave this disabled, maybe we could use the same gradient
	 * as the buttons in the future, not flat fill */
/*	pattern = cairo_pattern_create_linear (0, 0, 0, height);*/
/*	cairo_pattern_add_color_stop_rgb (pattern, 0.0, bg_shade.r, bg_shade.g, bg_shade.b);*/
/*	cairo_pattern_add_color_stop_rgb (pattern, 0.6, colors->spot[1].r, colors->spot[1].g, colors->spot[1].b);*/
/*	cairo_pattern_add_color_stop_rgb (pattern, 1.0, bg_shade.r, bg_shade.g, bg_shade.b);*/
/*	cairo_set_source (cr, pattern);*/
/*	cairo_paint (cr);*/
/*	cairo_pattern_destroy (pattern);*/

	ge_cairo_set_color (cr, &bg_shade);
	cairo_paint (cr);

	/* Draw the Strokes */
	while (tile_pos <= width+x_step)
	{
		cairo_move_to (cr, stroke_width/2-x_step, 0);
		cairo_line_to (cr, stroke_width-x_step,   0);
		cairo_line_to (cr, stroke_width/2-x_step, height);
		cairo_line_to (cr, -x_step, height);

		cairo_translate (cr, stroke_width, 0);
		tile_pos += stroke_width;
	}

	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgba (pattern, 0.0, colors->spot[2].r, colors->spot[2].g, colors->spot[2].b, 0);
	cairo_pattern_add_color_stop_rgba (pattern, 1.0, colors->spot[2].r, colors->spot[2].g, colors->spot[2].b, 0.24);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	cairo_restore (cr); /* rounded clip region */

	/* Draw the dark lines and the shadow */
	cairo_save (cr);
	/* Again, this weird clip area. */
	ge_cairo_rounded_rectangle (cr, -1.0, 0, width + radius + 2.0, height, radius, CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT);
	cairo_clip (cr);
	ge_cairo_rounded_rectangle (cr, -radius - 1.0, 0, width + radius + 2.0, height, radius, CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT);
	cairo_clip (cr);

	shadow.r = 0.0;
	shadow.g = 0.0;
	shadow.b = 0.0;
	shadow.a = 0.1;

	if (progressbar->pulsing)
	{
		/* At the beginning of the bar. */
		cairo_move_to (cr, -0.5 + radius, height + 0.5);
		ge_cairo_rounded_corner (cr, -0.5, height + 0.5, radius + 1, CR_CORNER_BOTTOMLEFT);
		ge_cairo_rounded_corner (cr, -0.5, -0.5, radius + 1, CR_CORNER_TOPLEFT);
		ge_cairo_set_color (cr, &shadow);
		cairo_stroke (cr);
	}
	if (progressbar->value < 1.0 || progressbar->pulsing)
	{
		/* At the end of the bar. */
		cairo_move_to (cr, width + 0.5 - radius, -0.5);
		ge_cairo_rounded_corner (cr, width + 0.5, -0.5, radius + 1, CR_CORNER_TOPRIGHT);
		ge_cairo_rounded_corner (cr, width + 0.5, height + 0.5, radius + 1, CR_CORNER_BOTTOMRIGHT);
		ge_cairo_set_color (cr, &shadow);
		cairo_stroke (cr);
	}

/*	ge_cairo_rounded_rectangle (cr, 1.5,1.5, width-2, height-2, radius, CR_CORNER_ALL);*/
/*	cairo_set_source_rgba (cr, colors->spot[0].r, colors->spot[0].g, colors->spot[0].b, 1);*/
/*	cairo_stroke (cr);*/

	params->style_functions->draw_top_left_highlight (cr, &colors->spot[1], params, 1.5, 1.5,
	                                                  width - 1, height - 1,
	                                                  radius, params->corners);

	border = colors->spot[2];
	border.a = 0.6;
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, CR_CORNER_ALL);
	ge_cairo_set_color (cr, &border);
	cairo_stroke (cr);

	cairo_restore (cr);

	cairo_restore (cr); /* rotation, mirroring */
}

static void
clearlooks_draw_optionmenu (cairo_t *cr,
                            const ClearlooksColors *colors,
                            const WidgetParameters *params,
                            const OptionMenuParameters *optionmenu,
                            int x, int y, int width, int height)
{
	SeparatorParameters separator;
	int offset = params->ythickness + 2;

	params->style_functions->draw_button (cr, colors, params, x, y, width, height);

	separator.horizontal = FALSE;
	params->style_functions->draw_separator (cr, colors, params, &separator, x+optionmenu->linepos, y + offset, 2, height - offset*2);
}

static void
clearlooks_draw_menu_item_separator (cairo_t                   *cr,
                                     const ClearlooksColors    *colors,
                                     const WidgetParameters    *widget,
                                     const SeparatorParameters *separator,
                                     int x, int y, int width, int height)
{
	cairo_save (cr);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	ge_cairo_set_color (cr, &colors->shade[5]);

	if (separator->horizontal)
		cairo_rectangle (cr, x, y, width, 1);
	else
		cairo_rectangle (cr, x, y, 1, height);

	cairo_fill      (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_menubar0 (cairo_t *cr,
                          const ClearlooksColors *colors,
                          const WidgetParameters *params,
                          const MenuBarParameters *menubar,
                          int x, int y, int width, int height)
{
	const CairoColor *dark = &colors->shade[3];

	cairo_save (cr);

	cairo_set_line_width (cr, 1);
	cairo_translate (cr, x, y);

	cairo_move_to (cr, 0, height-0.5);
	cairo_line_to (cr, width, height-0.5);
	ge_cairo_set_color (cr, dark);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_menubar2 (cairo_t *cr,
                          const ClearlooksColors *colors,
                          const WidgetParameters *params,
                          const MenuBarParameters *menubar,
                          int x, int y, int width, int height)
{
	CairoColor lower;
	cairo_pattern_t *pattern;

	cairo_save (cr);

	ge_shade_color (&colors->bg[0], 0.96, &lower);

	cairo_translate (cr, x, y);
	cairo_rectangle (cr, 0, 0, width, height);

	/* Draw the gradient */
	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, colors->bg[0].r,
	                                                colors->bg[0].g,
	                                                colors->bg[0].b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, lower.r,
	                                                lower.g,
	                                                lower.b);
	cairo_set_source      (cr, pattern);
	cairo_fill            (cr);
	cairo_pattern_destroy (pattern);

	/* Draw bottom line */
	cairo_set_line_width (cr, 1.0);
	cairo_move_to        (cr, 0, height-0.5);
	cairo_line_to        (cr, width, height-0.5);
	ge_cairo_set_color   (cr, &colors->shade[3]);
	cairo_stroke         (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_menubar1 (cairo_t *cr,
                          const ClearlooksColors *colors,
                          const WidgetParameters *params,
                          const MenuBarParameters *menubar,
                          int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[3];

	clearlooks_draw_menubar2 (cr, colors, params, menubar,
	                          x, y, width, height);

	ge_cairo_set_color (cr, border);
	ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
}


static menubar_draw_proto clearlooks_menubar_draw[3] =
{
	clearlooks_draw_menubar0,
	clearlooks_draw_menubar1,
	clearlooks_draw_menubar2
};

static void
clearlooks_draw_menubar (cairo_t *cr,
                         const ClearlooksColors *colors,
                         const WidgetParameters *params,
                         const MenuBarParameters *menubar,
                         int x, int y, int width, int height)
{
	if (menubar->style < 0 || menubar->style >= G_N_ELEMENTS (clearlooks_menubar_draw))
		return;

	clearlooks_menubar_draw[menubar->style](cr, colors, params, menubar,
	                             x, y, width, height);
}

static void
clearlooks_get_frame_gap_clip (int x, int y, int width, int height,
                               const FrameParameters     *frame,
                               ClearlooksRectangle *bevel,
                               ClearlooksRectangle *border)
{
	if (frame->gap_side == CL_GAP_TOP)
	{
		CLEARLOOKS_RECTANGLE_SET (*bevel,  2.0 + frame->gap_x,  0.0,
		                          frame->gap_width - 3, 2.0);
		CLEARLOOKS_RECTANGLE_SET (*border, 1.0 + frame->gap_x,  0.0,
		                         frame->gap_width - 2, 2.0);
	}
	else if (frame->gap_side == CL_GAP_BOTTOM)
	{
		CLEARLOOKS_RECTANGLE_SET (*bevel,  2.0 + frame->gap_x,  height - 2.0,
		                          frame->gap_width - 3, 2.0);
		CLEARLOOKS_RECTANGLE_SET (*border, 1.0 + frame->gap_x,  height - 1.0,
		                          frame->gap_width - 2, 2.0);
	}
	else if (frame->gap_side == CL_GAP_LEFT)
	{
		CLEARLOOKS_RECTANGLE_SET (*bevel,  0.0, 2.0 + frame->gap_x,
		                          2.0, frame->gap_width - 3);
		CLEARLOOKS_RECTANGLE_SET (*border, 0.0, 1.0 + frame->gap_x,
		                          1.0, frame->gap_width - 2);
	}
	else if (frame->gap_side == CL_GAP_RIGHT)
	{
		CLEARLOOKS_RECTANGLE_SET (*bevel,  width - 2.0, 2.0 + frame->gap_x,
		                          2.0, frame->gap_width - 3);
		CLEARLOOKS_RECTANGLE_SET (*border, width - 1.0, 1.0 + frame->gap_x,
		                          1.0, frame->gap_width - 2);
	}
}

static void
clearlooks_draw_frame            (cairo_t *cr,
                                  const ClearlooksColors     *colors,
                                  const WidgetParameters     *params,
                                  const FrameParameters      *frame,
                                  int x, int y, int width, int height)
{
	const CairoColor *border = frame->border;
	const CairoColor *dark   = (CairoColor*)&colors->shade[4];
	ClearlooksRectangle bevel_clip = {0, 0, 0, 0};
	ClearlooksRectangle frame_clip = {0, 0, 0, 0};
	double radius = MIN (params->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));
	CairoColor hilight;

	ge_shade_color (&colors->bg[0], 1.05, &hilight);

	if (frame->shadow == CL_SHADOW_NONE)
		return;

	if (frame->gap_x != -1)
		clearlooks_get_frame_gap_clip (x, y, width, height,
		                               frame, &bevel_clip, &frame_clip);

	cairo_set_line_width (cr, 1.0);
	cairo_translate      (cr, x, y);

	/* save everything */
	cairo_save (cr);
	/* Set clip for the bevel */
	if (frame->gap_x != -1)
	{
		/* Set clip for gap */
		cairo_set_fill_rule  (cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_rectangle      (cr, 0, 0, width, height);
		cairo_rectangle      (cr, bevel_clip.x, bevel_clip.y, bevel_clip.width, bevel_clip.height);
		cairo_clip           (cr);
	}

	/* Draw the bevel */
	if (frame->shadow == CL_SHADOW_ETCHED_IN || frame->shadow == CL_SHADOW_ETCHED_OUT)
	{
		ge_cairo_set_color (cr, &hilight);
		if (frame->shadow == CL_SHADOW_ETCHED_IN)
			ge_cairo_inner_rounded_rectangle (cr, 1, 1, width-1, height-1, radius, params->corners);
		else
			ge_cairo_inner_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
		cairo_stroke (cr);
	}
	else if (frame->shadow != CL_SHADOW_NONE)
	{
		ShadowParameters shadow;
		shadow.corners = params->corners;
		shadow.shadow  = frame->shadow;
		clearlooks_draw_highlight_and_shade (cr, colors, &shadow, width, height, radius);
	}

	/* restore the previous clip region */
	cairo_restore    (cr);
	cairo_save       (cr);
	if (frame->gap_x != -1)
	{
		/* Set clip for gap */
		cairo_set_fill_rule  (cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_rectangle      (cr, 0, 0, width, height);
		cairo_rectangle      (cr, frame_clip.x, frame_clip.y, frame_clip.width, frame_clip.height);
		cairo_clip           (cr);
	}

	/* Draw frame */
	if (frame->shadow == CL_SHADOW_ETCHED_IN || frame->shadow == CL_SHADOW_ETCHED_OUT)
	{
		ge_cairo_set_color (cr, dark);
		if (frame->shadow == CL_SHADOW_ETCHED_IN)
			ge_cairo_inner_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
		else
			ge_cairo_inner_rounded_rectangle (cr, 1, 1, width-1, height-1, radius, params->corners);
	}
	else
	{
		ge_cairo_set_color (cr, border);
		ge_cairo_inner_rounded_rectangle (cr, 0, 0, width, height, radius, params->corners);
	}
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_tab (cairo_t *cr,
                     const ClearlooksColors *colors,
                     const WidgetParameters *params,
                     const TabParameters    *tab,
                     int x, int y, int width, int height)
{
	const CairoColor *border1       = &colors->shade[6];
	const CairoColor *border2       = &colors->shade[5];
	const CairoColor *stripe_fill   = &colors->spot[1];
	const CairoColor *stripe_border = &colors->spot[2];
	const CairoColor *fill;
	CairoColor        hilight;

	cairo_pattern_t  *pattern;

	double            radius;
	double            stripe_size = 2.0;
	double            stripe_fill_size;
	double            length;

	radius = MIN (params->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));

	cairo_save (cr);

	/* Set clip */
	cairo_rectangle      (cr, x, y, width, height);
	cairo_clip           (cr);
	cairo_new_path       (cr);

	/* Translate and set line width */
	cairo_set_line_width (cr, 1.0);
	cairo_translate      (cr, x, y);


	/* Make the tabs slightly bigger than they should be, to create a gap */
	/* And calculate the strip size too, while you're at it */
	if (tab->gap_side == CL_GAP_TOP || tab->gap_side == CL_GAP_BOTTOM)
	{
		height += 3.0;
		length = height;
		stripe_fill_size = (tab->gap_side == CL_GAP_TOP ? stripe_size/height : stripe_size/(height-2));

		if (tab->gap_side == CL_GAP_TOP)
			cairo_translate (cr, 0.0, -3.0); /* gap at the other side */
	}
	else
	{
		width += 3.0;
		length = width;
		stripe_fill_size = (tab->gap_side == CL_GAP_LEFT ? stripe_size/width : stripe_size/(width-2));

		if (tab->gap_side == CL_GAP_LEFT)
			cairo_translate (cr, -3.0, 0.0); /* gap at the other side */
	}

	/* Set the fill color */
	fill = &colors->bg[params->state_type];

	/* Set tab shape */
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1,
	                            radius, params->corners);

	/* Draw fill */
	ge_cairo_set_color (cr, fill);
	cairo_fill   (cr);


	ge_shade_color (fill, 1.3, &hilight);

	/* Draw highlight */
	if (!params->active)
	{
		ShadowParameters shadow;

		shadow.shadow  = CL_SHADOW_OUT;
		shadow.corners = params->corners;

		clearlooks_draw_highlight_and_shade (cr, colors, &shadow,
		                                     width,
		                                     height, radius);
	}


	if (params->active)
	{
		CairoColor shadow;
		switch (tab->gap_side)
		{
			case CL_GAP_TOP:
				pattern = cairo_pattern_create_linear (0.5, height-1.5, 0.5, 0.5);
				break;
			case CL_GAP_BOTTOM:
				pattern = cairo_pattern_create_linear (0.5, 1.5, 0.5, height+0.5);
				break;
			case CL_GAP_LEFT:
				pattern = cairo_pattern_create_linear (width-1.5, 0.5, 1.5, 0.5);
				break;
			case CL_GAP_RIGHT:
				pattern = cairo_pattern_create_linear (1.5, 0.5, width-1.5, 0.5);
				break;
			default:
				pattern = NULL;
		}

		ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, params->corners);

		ge_shade_color (fill, 0.92, &shadow);

		cairo_pattern_add_color_stop_rgba  (pattern, 0.0,        hilight.r, hilight.g, hilight.b, 0.4);
		cairo_pattern_add_color_stop_rgba  (pattern, 1.0/length, hilight.r, hilight.g, hilight.b, 0.4);
		cairo_pattern_add_color_stop_rgb   (pattern, 1.0/length, fill->r,fill->g,fill->b);
		cairo_pattern_add_color_stop_rgb   (pattern, 1.0,        shadow.r,shadow.g,shadow.b);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}
	else
	{
		/* Draw shade */
		switch (tab->gap_side)
		{
			case CL_GAP_TOP:
				pattern = cairo_pattern_create_linear (0.5, height-1.5, 0.5, 0.5);
				break;
			case CL_GAP_BOTTOM:
				pattern = cairo_pattern_create_linear (0.5, 0.5, 0.5, height+0.5);
				break;
			case CL_GAP_LEFT:
				pattern = cairo_pattern_create_linear (width-1.5, 0.5, 0.5, 0.5);
				break;
			case CL_GAP_RIGHT:
				pattern = cairo_pattern_create_linear (0.5, 0.5, width+0.5, 0.5);
				break;
			default:
				pattern = NULL;
		}

		ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, params->corners);

		cairo_pattern_add_color_stop_rgb  (pattern, 0.0,        stripe_fill->r, stripe_fill->g, stripe_fill->b);
		cairo_pattern_add_color_stop_rgb  (pattern, stripe_fill_size, stripe_fill->r, stripe_fill->g, stripe_fill->b);
		cairo_pattern_add_color_stop_rgba (pattern, stripe_fill_size, hilight.r, hilight.g, hilight.b, 0.5);
		cairo_pattern_add_color_stop_rgba (pattern, 0.8,        hilight.r, hilight.g, hilight.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}

	ge_cairo_inner_rounded_rectangle (cr, 0, 0, width, height, radius, params->corners);

	if (params->active)
	{
		ge_cairo_set_color (cr, border2);
		cairo_stroke (cr);
	}
	else
	{
		switch (tab->gap_side)
		{
			case CL_GAP_TOP:
				pattern = cairo_pattern_create_linear (2.5, height-1.5, 2.5, 2.5);
				break;
			case CL_GAP_BOTTOM:
				pattern = cairo_pattern_create_linear (2.5, 2.5, 2.5, height+0.5);
				break;
			case CL_GAP_LEFT:
				pattern = cairo_pattern_create_linear (width-1.5, 2.5, 2.5, 2.5);
				break;
			case CL_GAP_RIGHT:
				pattern = cairo_pattern_create_linear (2.5, 2.5, width+0.5, 2.5);
				break;
			default:
				pattern = NULL;
		}

		cairo_pattern_add_color_stop_rgb (pattern, 0.0,        stripe_border->r, stripe_border->g, stripe_border->b);
		cairo_pattern_add_color_stop_rgb (pattern, stripe_fill_size, stripe_border->r, stripe_border->g, stripe_border->b);
		cairo_pattern_add_color_stop_rgb (pattern, stripe_fill_size, border1->r,       border1->g,       border1->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0,        border2->r,       border2->g,       border2->b);
		cairo_set_source (cr, pattern);
		cairo_stroke (cr);
		cairo_pattern_destroy (pattern);
	}

	cairo_restore (cr);
}

static void
clearlooks_draw_separator (cairo_t *cr,
                           const ClearlooksColors     *colors,
                           const WidgetParameters     *widget,
                           const SeparatorParameters  *separator,
                           int x, int y, int width, int height)
{
	CairoColor color = colors->shade[2];
	CairoColor hilight;
	ge_shade_color (&colors->bg[0], 1.065, &hilight);

	cairo_save (cr);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

	if (separator->horizontal)
	{
		cairo_set_line_width  (cr, 1.0);
		cairo_translate       (cr, x, y+0.5);

		cairo_move_to         (cr, 0.0,   0.0);
		cairo_line_to         (cr, width, 0.0);
		ge_cairo_set_color    (cr, &color);
		cairo_stroke          (cr);

		cairo_move_to         (cr, 0.0,   1.0);
		cairo_line_to         (cr, width, 1.0);
		ge_cairo_set_color    (cr, &hilight);
		cairo_stroke          (cr);
	}
	else
	{
		cairo_set_line_width  (cr, 1.0);
		cairo_translate       (cr, x+0.5, y);

		cairo_move_to         (cr, 0.0, 0.0);
		cairo_line_to         (cr, 0.0, height);
		ge_cairo_set_color    (cr, &color);
		cairo_stroke          (cr);

		cairo_move_to         (cr, 1.0, 0.0);
		cairo_line_to         (cr, 1.0, height);
		ge_cairo_set_color    (cr, &hilight);
		cairo_stroke          (cr);
	}

	cairo_restore (cr);
}

static void
clearlooks_draw_list_view_header (cairo_t *cr,
                                  const ClearlooksColors          *colors,
                                  const WidgetParameters          *params,
                                  const ListViewHeaderParameters  *header,
                                  int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[4];
	CairoColor hilight;

	ge_shade_color (&colors->bg[params->state_type],
	                params->style_constants->topleft_highlight_shade, &hilight);
	hilight.a = params->style_constants->topleft_highlight_alpha;

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1.0);

	/* Draw highlight */
	if (header->order & CL_ORDER_FIRST)
	{
		cairo_move_to (cr, 0.5, height-1);
		cairo_line_to (cr, 0.5, 0.5);
	}
	else
		cairo_move_to (cr, 0.0, 0.5);

	cairo_line_to (cr, width, 0.5);

	ge_cairo_set_color (cr, &hilight);
	cairo_stroke (cr);

	/* Draw bottom border */
	cairo_move_to (cr, 0.0, height-0.5);
	cairo_line_to (cr, width, height-0.5);
	ge_cairo_set_color (cr, border);
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

/* We can't draw transparent things here, since it will be called on the same
 * surface multiple times, when placed on a handlebox_bin or dockitem_bin */
static void
clearlooks_draw_toolbar (cairo_t *cr,
                         const ClearlooksColors          *colors,
                         const WidgetParameters          *widget,
                         const ToolbarParameters         *toolbar,
                         int x, int y, int width, int height)
{
	const CairoColor *fill  = &colors->bg[0];
	const CairoColor *dark  = &colors->shade[3];
	CairoColor light;
	ge_shade_color (fill, 1.065, &light);

	cairo_set_line_width (cr, 1.0);
	cairo_translate (cr, x, y);

	ge_cairo_set_color (cr, fill);
	cairo_paint (cr);

	if (!toolbar->topmost)
	{
		/* Draw highlight */
		cairo_move_to       (cr, 0, 0.5);
		cairo_line_to       (cr, width-1, 0.5);
		ge_cairo_set_color  (cr, &light);
		cairo_stroke        (cr);
	}

	/* Draw shadow */
	cairo_move_to       (cr, 0, height-0.5);
	cairo_line_to       (cr, width-1, height-0.5);
	ge_cairo_set_color  (cr, dark);
	cairo_stroke        (cr);
}

static void
clearlooks_draw_menuitem (cairo_t *cr,
                          const ClearlooksColors          *colors,
                          const WidgetParameters          *widget,
                          int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->spot[1];
	CairoColor fill_shade;
	CairoColor border = colors->spot[2];
	cairo_pattern_t *pattern;

	ge_shade_color (&border, 1.05, &border);
	ge_shade_color (fill, 0.85, &fill_shade);
	cairo_set_line_width (cr, 1.0);

	ge_cairo_rounded_rectangle (cr, x+0.5, y+0.5, width - 1, height - 1, widget->radius, widget->corners);

	pattern = cairo_pattern_create_linear (x, y, x, y + height);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   fill->r, fill->g, fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, fill_shade.r, fill_shade.g, fill_shade.b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve  (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, &border);
	cairo_stroke (cr);
}

static void
clearlooks_draw_menubaritem (cairo_t *cr,
                          const ClearlooksColors          *colors,
                          const WidgetParameters          *widget,
                          int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->spot[1];
	CairoColor fill_shade;
	CairoColor border = colors->spot[2];
	cairo_pattern_t *pattern;

	ge_shade_color (&border, 1.05, &border);
	ge_shade_color (fill, 0.85, &fill_shade);

	cairo_set_line_width (cr, 1.0);
	ge_cairo_rounded_rectangle (cr, x + 0.5, y + 0.5, width - 1, height, widget->radius, widget->corners);

	pattern = cairo_pattern_create_linear (x, y, x, y + height);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   fill->r, fill->g, fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, fill_shade.r, fill_shade.g, fill_shade.b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve  (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, &border);
	cairo_stroke_preserve (cr);
}

static void
clearlooks_draw_selected_cell (cairo_t                  *cr,
	                       const ClearlooksColors   *colors,
	                       const WidgetParameters   *params,
	                       int x, int y, int width, int height)
{
	CairoColor upper_color;
	CairoColor lower_color;
	cairo_pattern_t *pattern;
	cairo_save (cr);

	cairo_translate (cr, x, y);

	if (params->focus)
		upper_color = colors->base[params->state_type];
	else
		upper_color = colors->base[GTK_STATE_ACTIVE];

	ge_shade_color(&upper_color, 0.92, &lower_color);

	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, upper_color.r,
	                                                upper_color.g,
	                                                upper_color.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, lower_color.r,
	                                                lower_color.g,
	                                                lower_color.b);

	cairo_set_source (cr, pattern);
	cairo_rectangle  (cr, 0, 0, width, height);
	cairo_fill       (cr);

	cairo_pattern_destroy (pattern);

	cairo_restore (cr);
}


static void
clearlooks_draw_scrollbar_trough (cairo_t *cr,
                                  const ClearlooksColors           *colors,
                                  const WidgetParameters           *widget,
                                  const ScrollBarParameters        *scrollbar,
                                  int x, int y, int width, int height)
{
	const CairoColor *bg     = &colors->shade[2];
	const CairoColor *border = &colors->shade[5];
	CairoColor        bg_shade;
	cairo_pattern_t *pattern;
	double radius = MIN (widget->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));

	ge_shade_color (bg, 0.95, &bg_shade);

	cairo_set_line_width (cr, 1);
	/* cairo_translate (cr, x, y); */

	if (scrollbar->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	cairo_translate (cr, x, y);

	/* Draw fill */
	if (radius > 3.0)
		ge_cairo_rounded_rectangle (cr, 1, 0, width-2, height,
		                            radius, widget->corners);
	else
		cairo_rectangle (cr, 1, 0, width-2, height);
	ge_cairo_set_color (cr, bg);
	cairo_fill (cr);

	/* Draw shadow */
	pattern = cairo_pattern_create_linear (1, 0, 3, 0);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   bg_shade.r, bg_shade.g, bg_shade.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, bg->r,      bg->g,      bg->b);
	cairo_rectangle (cr, 1, 0, 4, height);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	/* Draw border */
	if (radius > 3.0)
		ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1,
		                            radius, widget->corners);
	else
		cairo_rectangle (cr, 0.5, 0.5, width-1, height-1);
	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);
}

static void
clearlooks_draw_scrollbar_stepper (cairo_t *cr,
                                   const ClearlooksColors           *colors,
                                   const WidgetParameters           *widget,
                                   const ScrollBarParameters        *scrollbar,
                                   const ScrollBarStepperParameters *stepper,
                                   int x, int y, int width, int height)
{
	CairoCorners corners = CR_CORNER_NONE;
	CairoColor   border;
	CairoColor   s1, s2, s3, s4;
	cairo_pattern_t *pattern;
	double radius = MIN (widget->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));

	ge_shade_color(&colors->shade[6], 1.08, &border);

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

	ge_shade_color (&colors->bg[widget->state_type], SHADE_TOP, &s1);
	ge_shade_color (&colors->bg[widget->state_type], SHADE_CENTER_TOP, &s2);
	ge_shade_color (&colors->bg[widget->state_type], SHADE_CENTER_BOTTOM, &s3);
	ge_shade_color (&colors->bg[widget->state_type], SHADE_BOTTOM, &s4);

	cairo_pattern_add_color_stop_rgb(pattern, 0,   s1.r, s1.g, s1.b);
	cairo_pattern_add_color_stop_rgb(pattern, 0.3, s2.r, s2.g, s2.b);
	cairo_pattern_add_color_stop_rgb(pattern, 0.7, s3.r, s3.g, s3.b);
	cairo_pattern_add_color_stop_rgb(pattern, 1.0, s4.r, s4.g, s4.b);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	widget->style_functions->draw_top_left_highlight (cr, &s2, widget, 1, 1, width - 2, height - 2, MAX(radius - 1, 0), corners);

	ge_cairo_inner_rounded_rectangle (cr, 0, 0, width, height, radius, corners);
	clearlooks_set_border_gradient (cr, &border, 1.1, (scrollbar->horizontal ? 0 : width), (scrollbar->horizontal ? height: 0));
	cairo_stroke (cr);
}

static void
clearlooks_draw_scrollbar_slider (cairo_t *cr,
                                   const ClearlooksColors          *colors,
                                   const WidgetParameters          *widget,
                                   const ScrollBarParameters       *scrollbar,
                                   int x, int y, int width, int height)
{
	cairo_save (cr);

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

	if (scrollbar->has_color)
	{
		const CairoColor *border  = &colors->shade[7];
		CairoColor  fill    = scrollbar->color;
		CairoColor  hilight;
		CairoColor  shade1, shade2, shade3;
		cairo_pattern_t *pattern;

		if (widget->prelight)
			ge_shade_color (&fill, 1.1, &fill);

		cairo_set_line_width (cr, 1);

		ge_shade_color (&fill, 1.3, &hilight);
		ge_shade_color (&fill, 1.1, &shade1);
		ge_shade_color (&fill, 1.05, &shade2);
		ge_shade_color (&fill, 0.98, &shade3);

		pattern = cairo_pattern_create_linear (1, 1, 1, height-2);
		cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade2.r, shade2.g, shade2.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade3.r, shade3.g, shade3.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1, 	fill.r,  fill.g,  fill.b);
		cairo_rectangle (cr, 1, 1, width-2, height-2);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);

		cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
		ge_cairo_stroke_rectangle (cr, 1.5, 1.5, width-3, height-3);

		ge_cairo_set_color (cr, border);
		ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
	}
	else
	{
		const CairoColor *dark  = &colors->shade[4];
		const CairoColor *light = &colors->shade[0];
		CairoColor border;
		CairoColor s1, s2, s3, s4, s5;
		cairo_pattern_t *pattern;
		int bar_x, i;

		ge_shade_color (&colors->shade[6], 1.08, &border);
		ge_shade_color (&colors->bg[widget->state_type], SHADE_TOP, &s1);
		ge_shade_color (&colors->bg[widget->state_type], SHADE_CENTER_TOP, &s2);
		ge_shade_color (&colors->bg[widget->state_type], SHADE_CENTER_BOTTOM, &s3);
		ge_shade_color (&colors->bg[widget->state_type], SHADE_BOTTOM, &s4);

		pattern = cairo_pattern_create_linear(1, 1, 1, height-1);
		cairo_pattern_add_color_stop_rgb(pattern, 0,   s1.r, s1.g, s1.b);
		cairo_pattern_add_color_stop_rgb(pattern, 0.3, s2.r, s2.g, s2.b);
		cairo_pattern_add_color_stop_rgb(pattern, 0.7, s3.r, s3.g, s3.b);
		cairo_pattern_add_color_stop_rgb(pattern, 1.0, s4.r, s4.g, s4.b);

		cairo_rectangle (cr, 1, 1, width-2, height-2);
		cairo_set_source(cr, pattern);
		cairo_fill(cr);
		cairo_pattern_destroy(pattern);

		clearlooks_set_border_gradient (cr, &border, 1.1, 0, height);
		ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);

		cairo_move_to (cr, 1.5, height-1.5);
		cairo_line_to (cr, 1.5, 1.5);
		cairo_line_to (cr, width-1.5, 1.5);
		ge_shade_color (&s2, widget->style_constants->topleft_highlight_shade, &s5);
		s5.a = widget->style_constants->topleft_highlight_alpha;
		ge_cairo_set_color (cr, &s5);
		cairo_stroke(cr);

		/* draw handles */
		cairo_set_line_width (cr, 1);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);

		bar_x = width/2 - 4;

		for (i=0; i<3; i++)
		{
			cairo_move_to (cr, bar_x + 0.5, 4);
			cairo_line_to (cr, bar_x + 0.5, height-4);
			ge_cairo_set_color (cr, dark);
			cairo_stroke (cr);

			cairo_move_to (cr, bar_x+1.5, 4);
			cairo_line_to (cr, bar_x+1.5, height-4);
			ge_cairo_set_color (cr, light);
			cairo_stroke (cr);

			bar_x += 3;
		}
	}

	cairo_restore (cr);
}

static void
clearlooks_draw_statusbar (cairo_t *cr,
                           const ClearlooksColors          *colors,
                           const WidgetParameters          *widget,
                           int x, int y, int width, int height)
{
	const CairoColor *dark = &colors->shade[3];
	CairoColor hilight;

	ge_shade_color (dark, 1.4, &hilight);

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
clearlooks_draw_menu_frame (cairo_t *cr,
                            const ClearlooksColors          *colors,
                            const WidgetParameters          *widget,
                            int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[5];
	cairo_translate      (cr, x, y);
	cairo_set_line_width (cr, 1);

	ge_cairo_set_color (cr, border);
	ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
}

static void
clearlooks_draw_tooltip (cairo_t *cr,
                         const ClearlooksColors          *colors,
                         const WidgetParameters          *widget,
                         int x, int y, int width, int height)
{
	CairoColor border;

	ge_shade_color (&colors->bg[widget->state_type], 0.6, &border);

	cairo_save (cr);

	cairo_translate      (cr, x, y);
	cairo_set_line_width (cr, 1);

	ge_cairo_set_color (cr, &colors->bg[widget->state_type]);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	ge_cairo_set_color (cr, &border);
	ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);

	cairo_restore (cr);
}

static void
clearlooks_draw_handle (cairo_t *cr,
                        const ClearlooksColors          *colors,
                        const WidgetParameters          *params,
                        const HandleParameters          *handle,
                        int x, int y, int width, int height)
{
	const CairoColor *fill  = &colors->bg[params->state_type];
	int num_bars = 6; /* shut up gcc warnings */

	cairo_save (cr);

	switch (handle->type)
	{
		case CL_HANDLE_TOOLBAR:
			num_bars    = 6;
		break;
		case CL_HANDLE_SPLITTER:
			num_bars    = 16;
		break;
	}

	if (params->prelight)
	{
		cairo_rectangle (cr, x, y, width, height);
		ge_cairo_set_color (cr, fill);
		cairo_fill (cr);
	}

	cairo_translate (cr, x, y);

	cairo_set_line_width (cr, 1);

	if (handle->horizontal)
	{
		params->style_functions->draw_gripdots (cr, colors, 0, 0, width, height, num_bars, 2, 0.1);
	}
	else
	{
		params->style_functions->draw_gripdots (cr, colors, 0, 0, width, height, 2, num_bars, 0.1);
	}

	cairo_restore (cr);
}

static void
clearlooks_draw_resize_grip (cairo_t *cr,
                             const ClearlooksColors          *colors,
                             const WidgetParameters          *widget,
                             const ResizeGripParameters      *grip,
                             int x, int y, int width, int height)
{
	const CairoColor *dark   = &colors->shade[4];
	CairoColor hilight;
	int lx, ly;
	int x_down;
	int y_down;
	int dots;

	ge_shade_color (dark, 1.5, &hilight);

	/* The number of dots fitting into the area. Just hardcoded to 4 right now. */
	/* dots = MIN (width - 2, height - 2) / 3; */
	dots = 4;

	cairo_save (cr);

	switch (grip->edge)
	{
		case CL_WINDOW_EDGE_NORTH_EAST:
			x_down = 0;
			y_down = 0;
			cairo_translate (cr, x + width - 3*dots + 2, y + 1);
		break;
		case CL_WINDOW_EDGE_SOUTH_EAST:
			x_down = 0;
			y_down = 1;
			cairo_translate (cr, x + width - 3*dots + 2, y + height - 3*dots + 2);
		break;
		case CL_WINDOW_EDGE_SOUTH_WEST:
			x_down = 1;
			y_down = 1;
			cairo_translate (cr, x + 1, y + height - 3*dots + 2);
		break;
		case CL_WINDOW_EDGE_NORTH_WEST:
			x_down = 1;
			y_down = 0;
			cairo_translate (cr, x + 1, y + 1);
		break;
		default:
			/* Not implemented. */
			return;
	}

	for (lx = 0; lx < dots; lx++) /* horizontally */
	{
		for (ly = 0; ly <= lx; ly++) /* vertically */
		{
			int mx, my;
			mx = x_down * dots + (1 - x_down * 2) * lx - x_down;
			my = y_down * dots + (1 - y_down * 2) * ly - y_down;

			ge_cairo_set_color (cr, &hilight);
			cairo_rectangle (cr, mx*3-1, my*3-1, 2, 2);
			cairo_fill (cr);

			ge_cairo_set_color (cr, dark);
			cairo_rectangle (cr, mx*3-1, my*3-1, 1, 1);
			cairo_fill (cr);
		}
	}

	cairo_restore (cr);
}

static void
clearlooks_draw_radiobutton (cairo_t *cr,
                             const ClearlooksColors  *colors,
                             const WidgetParameters  *widget,
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
clearlooks_draw_checkbox (cairo_t *cr,
                          const ClearlooksColors  *colors,
                          const WidgetParameters  *widget,
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
		border = &colors->shade[6];
		dot    = &colors->text[GTK_STATE_NORMAL];
	}

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1);

	if (widget->xthickness > 2 && widget->ythickness > 2)
	{
		widget->style_functions->draw_inset (cr, &widget->parentbg, 0, 0, width, height, 1, CR_CORNER_ALL);

		/* Draw the rectangle for the checkbox itself */
		ge_cairo_rounded_rectangle (cr, 1.5, 1.5, width-3, height-3, (widget->radius > 0)? 1 : 0, CR_CORNER_ALL);
	}
	else
	{
		/* Draw the rectangle for the checkbox itself */
		ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, (widget->radius > 0)? 1 : 0, CR_CORNER_ALL);
	}

	if (!widget->disabled)
	{
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
clearlooks_draw_normal_arrow (cairo_t *cr, const CairoColor *color,
                              double x, double y, double width, double height)
{
	double arrow_width;
	double arrow_height;
	double line_width_2;

	cairo_save (cr);

	arrow_width = MIN (height * 2.0 + MAX (1.0, ceil (height * 2.0 / 6.0 * 2.0) / 2.0) / 2.0, width);
	line_width_2 = MAX (1.0, ceil (arrow_width / 6.0 * 2.0) / 2.0) / 2.0;
	arrow_height = arrow_width / 2.0 + line_width_2;

	cairo_translate (cr, x, y - arrow_height / 2.0);

	cairo_move_to (cr, -arrow_width / 2.0, line_width_2);
	cairo_line_to (cr, -arrow_width / 2.0 + line_width_2, 0);
	/* cairo_line_to (cr, 0, arrow_height - line_width_2); */
	cairo_arc_negative (cr, 0, arrow_height - 2*line_width_2 - 2*line_width_2 * sqrt(2), 2*line_width_2, G_PI_2 + G_PI_4, G_PI_4);
	cairo_line_to (cr, arrow_width / 2.0 - line_width_2, 0);
	cairo_line_to (cr, arrow_width / 2.0, line_width_2);
	cairo_line_to (cr, 0, arrow_height);
	cairo_close_path (cr);

	ge_cairo_set_color (cr, color);
	cairo_fill (cr);

	cairo_restore (cr);
}

static void
clearlooks_draw_combo_arrow (cairo_t *cr, const CairoColor *color,
                             double x, double y, double width, double height)
{
	double arrow_width = MIN (height * 2 / 3.0, width);
	double arrow_height = arrow_width / 2.0;
	double gap_size = 1.0 * arrow_height;

	cairo_save (cr);
	cairo_translate (cr, x, y - (arrow_height + gap_size) / 2.0);
	cairo_rotate (cr, G_PI);
	clearlooks_draw_normal_arrow (cr, color, 0, 0, arrow_width, arrow_height);
	cairo_restore (cr);

	clearlooks_draw_normal_arrow (cr, color, x, y + (arrow_height + gap_size) / 2.0, arrow_width, arrow_height);
}

static void
_clearlooks_draw_arrow (cairo_t *cr, const CairoColor *color,
                        ClearlooksDirection dir, ClearlooksArrowType type,
                        double x, double y, double width, double height)
{
	double rotate;

	if (dir == CL_DIRECTION_LEFT)
		rotate = G_PI*1.5;
	else if (dir == CL_DIRECTION_RIGHT)
		rotate = G_PI*0.5;
	else if (dir == CL_DIRECTION_UP)
		rotate = G_PI;
	else if (dir == CL_DIRECTION_DOWN)
		rotate = 0;
	else
		return;

	if (type == CL_ARROW_NORMAL)
	{
		cairo_translate (cr, x, y);
		cairo_rotate (cr, -rotate);
		clearlooks_draw_normal_arrow (cr, color, 0, 0, width, height);
	}
	else if (type == CL_ARROW_COMBO)
	{
		cairo_translate (cr, x, y);
		clearlooks_draw_combo_arrow (cr, color, 0, 0, width, height);
	}
}

static void
clearlooks_draw_arrow (cairo_t *cr,
                       const ClearlooksColors *colors,
                       const WidgetParameters *widget,
                       const ArrowParameters  *arrow,
                       int x, int y, int width, int height)
{
	const CairoColor *color = &colors->fg[widget->state_type];
	gdouble tx, ty;

	tx = x + width/2.0;
	ty = y + height/2.0;

	if (widget->disabled)
	{
		_clearlooks_draw_arrow (cr, &colors->shade[0],
		                        arrow->direction, arrow->type,
		                        tx+0.5, ty+0.5, width, height);
	}

	cairo_identity_matrix (cr);

	_clearlooks_draw_arrow (cr, color, arrow->direction, arrow->type,
	                        tx, ty, width, height);
}

void
clearlooks_draw_focus (cairo_t *cr,
                       const ClearlooksColors *colors,
                       const WidgetParameters *widget,
                       const FocusParameters  *focus,
                       int x, int y, int width, int height)
{
	if (focus->has_color)
		ge_cairo_set_color (cr, &focus->color);
	else if (focus->type == CL_FOCUS_COLOR_WHEEL_LIGHT)
		cairo_set_source_rgb (cr, 0., 0., 0.);
	else if (focus->type == CL_FOCUS_COLOR_WHEEL_DARK)
		cairo_set_source_rgb (cr, 1., 1., 1.);
	else
		cairo_set_source_rgba (cr,
		                       colors->fg[widget->state_type].r,
		                       colors->fg[widget->state_type].g,
		                       colors->fg[widget->state_type].b,
		                       0.7);

	cairo_set_line_width (cr, focus->line_width);

	if (focus->dash_list[0])
	{
		gint n_dashes = strlen ((gchar *)focus->dash_list);
		gdouble *dashes = g_new (gdouble, n_dashes);
		gdouble total_length = 0;
		gdouble dash_offset;
		gint i;

		for (i = 0; i < n_dashes; i++)
		{
			dashes[i] = focus->dash_list[i];
			total_length += focus->dash_list[i];
		}

		dash_offset = -focus->line_width / 2.0;
		while (dash_offset < 0)
			dash_offset += total_length;

		cairo_set_dash (cr, dashes, n_dashes, dash_offset);
		g_free (dashes);
	}

	cairo_rectangle (cr,
	                 x + focus->line_width / 2.0,
	                 y + focus->line_width / 2.0,
	                 width - focus->line_width, height - focus->line_width);
	cairo_stroke (cr);
}

void
clearlooks_register_style_classic (ClearlooksStyleFunctions *functions, ClearlooksStyleConstants *constants)
{
	g_assert (functions);

	functions->draw_top_left_highlight  = clearlooks_draw_top_left_highlight;
	functions->draw_button              = clearlooks_draw_button;
	functions->draw_scale_trough        = clearlooks_draw_scale_trough;
	functions->draw_progressbar_trough  = clearlooks_draw_progressbar_trough;
	functions->draw_progressbar_fill    = clearlooks_draw_progressbar_fill;
	functions->draw_slider_button       = clearlooks_draw_slider_button;
	functions->draw_entry               = clearlooks_draw_entry;
	functions->draw_spinbutton          = clearlooks_draw_spinbutton;
	functions->draw_spinbutton_down     = clearlooks_draw_spinbutton_down;
	functions->draw_optionmenu          = clearlooks_draw_optionmenu;
	functions->draw_inset               = clearlooks_draw_inset;
	functions->draw_menubar	            = clearlooks_draw_menubar;
	functions->draw_tab                 = clearlooks_draw_tab;
	functions->draw_frame               = clearlooks_draw_frame;
	functions->draw_separator           = clearlooks_draw_separator;
	functions->draw_menu_item_separator = clearlooks_draw_menu_item_separator;
	functions->draw_list_view_header    = clearlooks_draw_list_view_header;
	functions->draw_toolbar             = clearlooks_draw_toolbar;
	functions->draw_menuitem            = clearlooks_draw_menuitem;
	functions->draw_menubaritem         = clearlooks_draw_menubaritem;
	functions->draw_selected_cell       = clearlooks_draw_selected_cell;
	functions->draw_scrollbar_stepper   = clearlooks_draw_scrollbar_stepper;
	functions->draw_scrollbar_slider    = clearlooks_draw_scrollbar_slider;
	functions->draw_scrollbar_trough    = clearlooks_draw_scrollbar_trough;
	functions->draw_statusbar           = clearlooks_draw_statusbar;
	functions->draw_menu_frame          = clearlooks_draw_menu_frame;
	functions->draw_tooltip             = clearlooks_draw_tooltip;
	functions->draw_handle              = clearlooks_draw_handle;
	functions->draw_resize_grip         = clearlooks_draw_resize_grip;
	functions->draw_arrow               = clearlooks_draw_arrow;
	functions->draw_focus                = clearlooks_draw_focus;
	functions->draw_checkbox            = clearlooks_draw_checkbox;
	functions->draw_radiobutton         = clearlooks_draw_radiobutton;
	functions->draw_shadow              = clearlooks_draw_shadow;
	functions->draw_slider              = clearlooks_draw_slider;
	functions->draw_gripdots            = clearlooks_draw_gripdots;

	constants->topleft_highlight_shade  = 1.3;
	constants->topleft_highlight_alpha  = 0.6;
}
