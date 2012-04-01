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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <cairo.h>

typedef void (*menubar_draw_proto) (cairo_t *cr,
                                    const ClearlooksColors *colors,
                                    const WidgetParameters *params,
                                    const MenuBarParameters *menubar,
                                    int x, int y, int width, int height);

static void
clearlooks_draw_inset (cairo_t          *cr, 
                       const CairoColor *bg_color, 
                       double x, double y, double w, double h, 
                       double radius, uint8 corners)
{
	CairoColor shadow;
	CairoColor highlight;

	/* not really sure of shading ratios... we will think */
	ge_shade_color (bg_color, 0.94, &shadow);
	ge_shade_color (bg_color, 1.06, &highlight);

	/* highlight */
	cairo_move_to (cr, x + w + (radius * -0.2928932188), y - (radius * -0.2928932188)); /* 0.2928932... 1-sqrt(2)/2 gives middle of curve */

	if (corners & CR_CORNER_TOPRIGHT)
		cairo_arc (cr, x + w - radius, y + radius, radius, G_PI * 1.75, G_PI * 2);
	else
		cairo_line_to (cr, x + w, y);

	if (corners & CR_CORNER_BOTTOMRIGHT)
		cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, G_PI * 0.5);
	else
		cairo_line_to (cr, x + w, y + h);

	if (corners & CR_CORNER_BOTTOMLEFT)
		cairo_arc (cr, x + radius, y + h - radius, radius, G_PI * 0.5, G_PI * 0.75);
	else
		cairo_line_to (cr, x, y + h);

	ge_cairo_set_color (cr, &highlight);
	cairo_stroke (cr);

	/* shadow */
	cairo_move_to (cr, x + (radius * 0.2928932188), y + h + (radius * -0.2928932188));

	if (corners & CR_CORNER_BOTTOMLEFT)
		cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.75, M_PI);
	else
		cairo_line_to (cr, x, y + h);

	if (corners & CR_CORNER_TOPLEFT)
		cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
	else
		cairo_line_to (cr, x, y);

	if (corners & CR_CORNER_TOPRIGHT)
	    cairo_arc (cr, x + w - radius, y + radius, radius, M_PI * 1.5, M_PI * 1.75);
	else
		cairo_line_to (cr, x + w, y);

	ge_cairo_set_color (cr, &shadow);
	cairo_stroke (cr);
}

static void
clearlooks_draw_shadow (cairo_t *cr, const ClearlooksColors *colors, gfloat radius, int width, int height)
{
	CairoColor shadow; 
	ge_shade_color (&colors->shade[6], 0.92, &shadow);

	cairo_set_line_width (cr, 1.0);
	
	cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, 0.1);
	
	cairo_move_to (cr, width, radius);
	ge_cairo_rounded_corner (cr, width, height, radius, CR_CORNER_BOTTOMRIGHT);
	cairo_line_to (cr, radius, height);

	cairo_stroke (cr);
}

static void
clearlooks_draw_top_left_highlight (cairo_t *cr, const CairoColor *color,
                                    const WidgetParameters *params,
                                    int width, int height, gdouble radius)
{
	CairoColor hilight; 

	double light_top = params->ythickness-1,
	       light_bottom = height - params->ythickness - 1,
	       light_left = params->xthickness-1,
	       light_right = width - params->xthickness - 1;

	ge_shade_color (color, 1.3, &hilight);
	cairo_move_to         (cr, light_left, light_bottom - (int)radius/2);

	ge_cairo_rounded_corner (cr, light_left, light_top, radius, params->corners & CR_CORNER_TOPLEFT);

	cairo_line_to         (cr, light_right - (int)radius/2, light_top);
	cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
	cairo_stroke          (cr);
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

	ge_shade_color (&colors->bg[GTK_STATE_NORMAL], 1.06, &hilight);
	ge_shade_color (&colors->bg[GTK_STATE_NORMAL], 0.94, &shadow);

	width  -= 3;
	height -= 3;
	
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
		ge_cairo_set_color (cr, &hilight);
	else
		ge_cairo_set_color (cr, &shadow);
		
	cairo_stroke (cr);
	
	/* Bottom/Right highlight -- this includes the corners */
	cairo_move_to (cr, x+width-radius, y); /* topright and by radius to the left */
	ge_cairo_rounded_corner (cr, x+width, y, radius, corners & CR_CORNER_TOPRIGHT);
	ge_cairo_rounded_corner (cr, x+width, y+height, radius, corners & CR_CORNER_BOTTOMRIGHT);
	ge_cairo_rounded_corner (cr, x, y+height, radius, corners & CR_CORNER_BOTTOMLEFT);
	
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

	ge_shade_color (dark, 1.5, &hilight);

	for ( i = 0; i < xr; i++ ) 
	{
		for ( j = 0; j < yr; j++ )
		{
			xoff = x -(xr * 3 / 2) + 3 * i;
			yoff = y -(yr * 3 / 2) + 3 * j; 
			
			cairo_rectangle (cr, width/2+0.5+xoff, height/2+0.5+yoff, 2, 2);
			cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.8+contrast);
			cairo_fill (cr);
			cairo_rectangle (cr, width/2+0.5+xoff, height/2+0.5+yoff, 1, 1);
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
	const CairoColor *border_normal = &colors->shade[6];
	const CairoColor *border_disabled = &colors->shade[4];

	CairoColor shadow;
	ge_shade_color (border_normal, 0.925, &shadow);
	
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
		cairo_translate (cr, 0.5, 0.5);
		params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, width-1, height-1, radius+1, params->corners);
		cairo_translate (cr, -0.5, -0.5);
	}		
	
	ge_cairo_rounded_rectangle (cr, xoffset+1, yoffset+1,
	                                     width-(xoffset*2)-2,
	                                     height-(yoffset*2)-2,
	                                     radius, params->corners);
	
	if (!params->active)
	{
		cairo_pattern_t *pattern;
		gdouble shade_size = ((100.0/height)*8.0)/100.0;
		CairoColor top_shade, bottom_shade, middle_shade;
		
		ge_shade_color (fill, 1.1, &top_shade);
		ge_shade_color (fill, 0.98, &middle_shade);
		ge_shade_color (fill, 0.93, &bottom_shade);
		
		pattern	= cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, top_shade.r, top_shade.g, top_shade.b);
		cairo_pattern_add_color_stop_rgb (pattern, shade_size, fill->r, fill->g, fill->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0 - shade_size, middle_shade.r, middle_shade.g, middle_shade.b);
		cairo_pattern_add_color_stop_rgb (pattern, (height-(yoffset*2)-1)/height, bottom_shade.r, bottom_shade.g, bottom_shade.b);
		cairo_pattern_add_color_stop_rgba (pattern, (height-(yoffset*2)-1)/height, bottom_shade.r, bottom_shade.g, bottom_shade.b, 0.7);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, bottom_shade.r, bottom_shade.g, bottom_shade.b, 0.7);

		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
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
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, params->disabled ? 0.125 : 0.3);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pattern);

		pattern	= cairo_pattern_create_linear (xoffset+1, 0, 3+xoffset, 0);
		cairo_pattern_add_color_stop_rgba (pattern, 0.0, shadow.r, shadow.g, shadow.b, params->disabled ? 0.125 : 0.3);
		cairo_pattern_add_color_stop_rgba (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}


	/* Drawing the border */
	if (!params->active && params->is_default)
	{
		const CairoColor *l = &colors->shade[4];
		const CairoColor *d = &colors->shade[4];
		ge_cairo_set_color (cr, l);
		ge_cairo_stroke_rectangle (cr, 2.5, 2.5, width-5, height-5);

		ge_cairo_set_color (cr, d);
		ge_cairo_stroke_rectangle (cr, 3.5, 3.5, width-7, height-7);
	}
	
	ge_cairo_rounded_rectangle (cr, xoffset + 0.5, yoffset + 0.5, width-(xoffset*2)-1, height-(yoffset*2)-1, radius, params->corners);

	if (params->disabled)
		ge_cairo_set_color (cr, border_disabled);
	else
		if (!params->active)
			clearlooks_set_border_gradient (cr, border_normal, 1.32, 0, height); 
		else
			ge_cairo_set_color (cr, border_normal);
	
	cairo_stroke (cr);
	
	/* Draw the "shadow" */
	if (!params->active)
	{
		cairo_translate (cr, 0.5, 0.5);
		/* Draw right shadow */
		cairo_move_to (cr, width-params->xthickness, params->ythickness - 1);
		cairo_line_to (cr, width-params->xthickness, height - params->ythickness - 1);
		cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, 0.1);
		cairo_stroke (cr);
		
		/* Draw topleft shadow */
		clearlooks_draw_top_left_highlight (cr, fill, params, width, height, radius);
	}
	cairo_restore (cr);
}

static void
clearlooks_draw_entry (cairo_t *cr,
                       const ClearlooksColors *colors,
                       const WidgetParameters *params,
                       int x, int y, int width, int height)
{
	const CairoColor *base = &colors->base[params->state_type];
	CairoColor border = colors->shade[params->disabled ? 4 : 6];
	double radius = MIN (params->radius, MIN ((width - 4.0) / 2.0, (height - 4.0) / 2.0));
	
	if (params->focus)
		border = colors->spot[2];

	cairo_translate (cr, x+0.5, y+0.5);
	cairo_set_line_width (cr, 1.0);
	
	/* Fill the background (shouldn't have to) */
	cairo_rectangle (cr, -0.5, -0.5, width, height);
	ge_cairo_set_color (cr, &params->parentbg);
	cairo_fill (cr);

	/* Fill the entry's base color (why isn't is large enough by default?) */
	cairo_rectangle (cr, 1.5, 1.5, width-4, height-4);
	ge_cairo_set_color (cr, base);
	cairo_fill (cr);
	
	params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, width-1, height-1, radius+1, params->corners);

	/* Draw the inner shadow */
	if (params->focus)
	{
		/* ge_cairo_rounded_rectangle (cr, 2, 2, width-5, height-5, RADIUS-1, params->corners); */
		ge_cairo_set_color (cr, &colors->spot[0]);
		ge_cairo_stroke_rectangle (cr, 2, 2, width-5, height-5);
	}
	else
	{
		CairoColor shadow; 
		ge_shade_color (&border, 0.925, &shadow);

		cairo_set_source_rgba (cr, shadow.r, shadow.g, shadow.b, params->disabled ? 0.05 : 0.1);
		/*
		cairo_move_to (cr, 2, height-3);
		cairo_arc (cr, params->xthickness+RADIUS-1, params->ythickness+RADIUS-1, RADIUS, G_PI, 270*(G_PI/180));
		cairo_line_to (cr, width-3, 2);*/
		cairo_move_to (cr, 2, height-3);
		cairo_line_to (cr, 2, 2);
		cairo_line_to (cr, width-3, 2);
		cairo_stroke (cr);
	}

	ge_cairo_rounded_rectangle (cr, 1, 1, width-3, height-3, radius, params->corners);
	if (params->focus || params->disabled)
		ge_cairo_set_color (cr, &border);
	else
		clearlooks_set_border_gradient (cr, &border, 1.32, 0, height); 
	cairo_stroke (cr);
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

	ge_shade_color (border, 1.5, &hilight);

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
	ge_shade_color (&colors->bg[GTK_STATE_NORMAL], 0.8, &shadow);

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

	pattern = cairo_pattern_create_linear (0, 0, horizontal ? 0 :  width, horizontal ? height : 0);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, c1->r, c1->g, c1->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, c2->r, c2->g, c2->b);

	cairo_rectangle (cr, x+0.5, y+0.5, width-1, height-1);	
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);
	
	ge_cairo_set_color (cr, c3);
	ge_cairo_stroke_rectangle (cr, x, y, width, height);	
}

#define TROUGH_SIZE 6
static void
clearlooks_draw_scale_trough (cairo_t *cr,
                              const ClearlooksColors *colors,
                              const WidgetParameters *params,
                              const SliderParameters *slider,
                              int x, int y, int width, int height)
{
	int     trough_width, trough_height;
	double  translate_x, translate_y;

	if (slider->horizontal)
	{
		trough_width  = width-3;
		trough_height = TROUGH_SIZE-2;
		
		translate_x   = x + 0.5;
		translate_y   = y + 0.5 + (height/2) - (TROUGH_SIZE/2);
	}
	else
	{
		trough_width  = TROUGH_SIZE-2;
		trough_height = height-3;
		
		translate_x   = x + 0.5 + (width/2) - (TROUGH_SIZE/2);
		translate_y  = y + 0.5;
	}

	cairo_set_line_width (cr, 1.0);
	cairo_translate (cr, translate_x, translate_y);
	
	if (!slider->fill_level)
		params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, trough_width+2, trough_height+2, 0, 0);
	
	cairo_translate (cr, 1, 1);
	
	if (!slider->lower && ! slider->fill_level)
		clearlooks_scale_draw_gradient (cr, &colors->shade[3], /* top */
		                                    &colors->shade[2], /* bottom */
		                                    &colors->shade[6], /* border */
		                                    0, 0, trough_width, trough_height,
		                                    slider->horizontal);
	else
		clearlooks_scale_draw_gradient (cr, &colors->spot[1], /* top    */
		                                    &colors->spot[0], /* bottom */
		                                    &colors->spot[2], /* border */
		                                    0, 0, trough_width, trough_height,
		                                    slider->horizontal);
}

static void
clearlooks_draw_slider (cairo_t *cr,
                        const ClearlooksColors *colors,
                        const WidgetParameters *params,
                        int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[params->disabled ? 4 : 6];
	const CairoColor *spot   = &colors->spot[1];
	const CairoColor *fill   = &colors->shade[2];
	double radius = MIN (params->radius, MIN ((width - 1.0) / 2.0, (height - 1.0) / 2.0));

	cairo_pattern_t *pattern;

	cairo_set_line_width (cr, 1.0);	
	cairo_translate      (cr, x, y);

	if (params->prelight)
		border = &colors->spot[2];

	/* fill the widget */
	cairo_rectangle (cr, 0.5, 0.5, width-2, height-2);

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
		cairo_rectangle    (cr, 0.5, 0.5, width-2, height-2);
		cairo_fill         (cr);
	}

	/* Set the clip */
	cairo_save (cr);
	cairo_rectangle (cr, 0.5, 0.5, 6, height-2);
	cairo_rectangle (cr, width-7.5, 0.5, 6 , height-2);
	cairo_clip_preserve (cr);

	cairo_new_path (cr);

	/* Draw the handles */
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, params->corners);
	pattern = cairo_pattern_create_linear (0.5, 0.5, 0.5, 0.5+height);

	if (params->prelight)
	{
		CairoColor highlight;
		ge_shade_color (spot, 1.5, &highlight);	
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, highlight.r, highlight.g, highlight.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, spot->r, spot->g, spot->b);
		cairo_set_source (cr, pattern);
	}
	else 
	{
		CairoColor hilight; 
		ge_shade_color (fill, 1.5, &hilight);
		cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
	}

	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	cairo_restore (cr);

	/* Draw the border */
	ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);

	if (params->prelight || params->disabled)
		ge_cairo_set_color (cr, border);
	else
		clearlooks_set_border_gradient (cr, border, 1.2, 0, height); 
	cairo_stroke (cr);

	/* Draw handle lines */
	if (width > 14)
	{
		cairo_move_to (cr, 6, 0.5);
		cairo_line_to (cr, 6, height-1);
	
		cairo_move_to (cr, width-7, 0.5);
		cairo_line_to (cr, width-7, height-1);
	
		cairo_set_line_width (cr, 1.0);
		cairo_set_source_rgba (cr, border->r,
		                           border->g,
		                           border->b,
	                                   0.3);
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
	double radius = MIN (params->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));
	cairo_set_line_width (cr, 1.0);
	
	if (!slider->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);
	cairo_translate (cr, x+0.5, y+0.5);

	params->style_functions->draw_shadow (cr, colors, radius, width-1, height-1);
	params->style_functions->draw_slider (cr, colors, params, 1, 1, width-2, height-2);

	if (width > 24)
		params->style_functions->draw_gripdots (cr, colors, 0, 0, width-2, height-2, 3, 3, 0);
}

static void
clearlooks_draw_progressbar_trough (cairo_t *cr,
                                    const ClearlooksColors *colors,
                                    const WidgetParameters *params,
                                    int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[6];
	CairoColor       shadow;
	cairo_pattern_t *pattern;
	double          radius = MIN (params->radius, MIN ((height-2.0) / 2.0, (width-2.0) / 2.0));
	
	cairo_save (cr);

	cairo_set_line_width (cr, 1.0);
	
	/* Fill with bg color */
	ge_cairo_set_color (cr, &colors->bg[params->state_type]);
	
	cairo_rectangle (cr, x, y, width, height);	
	cairo_fill (cr);

	/* Create trough box */
	ge_cairo_rounded_rectangle (cr, x+1, y+1, width-2, height-2, radius, params->corners);
	ge_cairo_set_color (cr, &colors->shade[3]);
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
	ge_shade_color (&colors->spot[1], 1.1, &bg_shade);
	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, bg_shade.r, bg_shade.g, bg_shade.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.6, colors->spot[1].r, colors->spot[1].g, colors->spot[1].b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, bg_shade.r, bg_shade.g, bg_shade.b);
	cairo_set_source (cr, pattern);
	cairo_paint (cr);
	cairo_pattern_destroy (pattern);

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
	
	cairo_set_source_rgba (cr, colors->spot[2].r,
	                           colors->spot[2].g,
	                           colors->spot[2].b,
	                           0.15);
	
	cairo_fill (cr);
	cairo_restore (cr); /* rounded clip region */

	/* inner highlight border
	 * This is again kinda ugly. Draw once from each side, clipping away the other. */
	cairo_set_source_rgba (cr, colors->spot[0].r, colors->spot[0].g, colors->spot[0].b, 0.5);

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
	border.a = 0.5;
	shadow.r = 0.0;
	shadow.g = 0.0;
	shadow.b = 0.0;
	shadow.a = 0.1;

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
clearlooks_draw_optionmenu (cairo_t *cr,
                            const ClearlooksColors *colors,
                            const WidgetParameters *params,
                            const OptionMenuParameters *optionmenu,
                            int x, int y, int width, int height)
{
	SeparatorParameters separator;
	int offset = params->ythickness + 1;
	
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
	(void) widget;
	
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
	(void) params;
	(void) menubar;
	
/* 	const CairoColor *light = &colors->shade[0]; */
	const CairoColor *dark = &colors->shade[3];

	cairo_set_line_width (cr, 1);
	cairo_translate (cr, x, y+0.5);

/* 	cairo_move_to (cr, 0, 0); */
/* 	cairo_line_to (cr, width, 0); */
/* 	ge_cairo_set_color (cr, light); */
/* 	cairo_stroke (cr); */

	cairo_move_to (cr, 0, height-1);
	cairo_line_to (cr, width, height-1);
	ge_cairo_set_color (cr, dark);
	cairo_stroke (cr);
}

static void
clearlooks_draw_menubar2 (cairo_t *cr,
                          const ClearlooksColors *colors,
                          const WidgetParameters *params,
                          const MenuBarParameters *menubar,
                          int x, int y, int width, int height)
{
	(void) params;
	(void) menubar;
	
	CairoColor lower;
	cairo_pattern_t *pattern;

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
	if (menubar->style < 0 || menubar->style >= 3)
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
	(void) x;
	(void) y;
	
	if (frame->gap_side == CL_GAP_TOP)
	{
		CLEARLOOKS_RECTANGLE_SET ((*bevel),  1.5 + frame->gap_x,  -0.5,
											 frame->gap_width - 3, 2.0);
		CLEARLOOKS_RECTANGLE_SET ((*border), 0.5 + frame->gap_x,  -0.5,
											 frame->gap_width - 2, 2.0);
	}
	else if (frame->gap_side == CL_GAP_BOTTOM)
	{
		CLEARLOOKS_RECTANGLE_SET ((*bevel),  1.5 + frame->gap_x,  height - 2.5,
											 frame->gap_width - 3, 2.0);
		CLEARLOOKS_RECTANGLE_SET ((*border), 0.5 + frame->gap_x,  height - 1.5,
											 frame->gap_width - 2, 2.0);		
	}
	else if (frame->gap_side == CL_GAP_LEFT)
	{
		CLEARLOOKS_RECTANGLE_SET ((*bevel),  -0.5, 1.5 + frame->gap_x,
											 2.0, frame->gap_width - 3);
		CLEARLOOKS_RECTANGLE_SET ((*border), -0.5, 0.5 + frame->gap_x,
											 1.0, frame->gap_width - 2);			
	}
	else if (frame->gap_side == CL_GAP_RIGHT)
	{
		CLEARLOOKS_RECTANGLE_SET ((*bevel),  width - 2.5, 1.5 + frame->gap_x,
											 2.0, frame->gap_width - 3);
		CLEARLOOKS_RECTANGLE_SET ((*border), width - 1.5, 0.5 + frame->gap_x,
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

	ge_shade_color (&colors->bg[GTK_STATE_NORMAL], 1.05, &hilight);
	
	if (frame->shadow == CL_SHADOW_NONE)
		return;
	
	if (frame->gap_x != -1)
		clearlooks_get_frame_gap_clip (x, y, width, height,
		                               frame, &bevel_clip, &frame_clip);
	
	cairo_set_line_width (cr, 1.0);
	cairo_translate      (cr, x+0.5, y+0.5);
	
	/* save everything */
	cairo_save (cr);
	/* Set clip for the bevel */
	if (frame->gap_x != -1)
	{
		/* Set clip for gap */
		cairo_set_fill_rule  (cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_rectangle      (cr, -0.5, -0.5, width, height);
		cairo_rectangle      (cr, bevel_clip.x, bevel_clip.y, bevel_clip.width, bevel_clip.height);
		cairo_clip           (cr);
	}
	
	/* Draw the bevel */
	if (frame->shadow == CL_SHADOW_ETCHED_IN || frame->shadow == CL_SHADOW_ETCHED_OUT)
	{
		ge_cairo_set_color (cr, &hilight);
		if (frame->shadow == CL_SHADOW_ETCHED_IN)
			ge_cairo_rounded_rectangle (cr, 1, 1, width-2, height-2, radius, params->corners);
		else
			ge_cairo_rounded_rectangle (cr, 0, 0, width-2, height-2, radius, params->corners);
		cairo_stroke (cr);
	}
	else if (frame->shadow != CL_SHADOW_NONE)
	{
		ShadowParameters shadow;
		shadow.corners = params->corners;
		shadow.shadow  = frame->shadow;
		clearlooks_draw_highlight_and_shade (cr, colors, &shadow, width, height, 0);
	}
	
	/* restore the previous clip region */
	cairo_restore    (cr);
	cairo_save       (cr);
	if (frame->gap_x != -1)
	{
		/* Set clip for gap */
		cairo_set_fill_rule  (cr, CAIRO_FILL_RULE_EVEN_ODD);
		cairo_rectangle      (cr, -0.5, -0.5, width, height);
		cairo_rectangle      (cr, frame_clip.x, frame_clip.y, frame_clip.width, frame_clip.height);
		cairo_clip           (cr);
	}

	/* Draw frame */
	if (frame->shadow == CL_SHADOW_ETCHED_IN || frame->shadow == CL_SHADOW_ETCHED_OUT)
	{
		ge_cairo_set_color (cr, dark);
		if (frame->shadow == CL_SHADOW_ETCHED_IN)
			ge_cairo_rounded_rectangle (cr, 0, 0, width-2, height-2, radius, params->corners);
		else
			ge_cairo_rounded_rectangle (cr, 1, 1, width-2, height-2, radius, params->corners);
	}
	else
	{
		ge_cairo_set_color (cr, border);
		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
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
	const CairoColor    *border1       = &colors->shade[6];
	const CairoColor    *border2       = &colors->shade[5];
	const CairoColor    *stripe_fill   = &colors->spot[1];
	const CairoColor    *stripe_border = &colors->spot[2];
	const CairoColor    *fill;
	CairoColor           hilight;

	cairo_pattern_t     *pattern;
	
	double               radius;
	double               strip_size;

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
		height += 3.0;
	 	strip_size = 2.0/height; /* 2 pixel high strip */
		
		if (tab->gap_side == CL_GAP_TOP)
			cairo_translate (cr, 0.0, -3.0); /* gap at the other side */
	}
	else
	{
		width += 3.0;
	 	strip_size = 2.0/width;
		
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
		pattern = cairo_pattern_create_linear ( tab->gap_side == CL_GAP_LEFT   ? width-1  : 0,
		                                        tab->gap_side == CL_GAP_TOP    ? height-2 : 1,
		                                        tab->gap_side == CL_GAP_RIGHT  ? width    : 0,
		                                        tab->gap_side == CL_GAP_BOTTOM ? height   : 0 );

		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
		
		ge_shade_color (fill, 0.92, &shadow);

		cairo_pattern_add_color_stop_rgba  (pattern, 0.0,  				hilight.r, hilight.g, hilight.b, 0.4);     
		cairo_pattern_add_color_stop_rgba  (pattern, 1.0/height,  hilight.r, hilight.g, hilight.b, 0.4); 
		cairo_pattern_add_color_stop_rgb	(pattern, 1.0/height, 	fill->r,fill->g,fill->b);
		cairo_pattern_add_color_stop_rgb 	(pattern, 1.0, 					shadow.r,shadow.g,shadow.b);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}
	else
	{
		/* Draw shade */
		pattern = cairo_pattern_create_linear ( tab->gap_side == CL_GAP_LEFT   ? width-2  : 0,
		                                        tab->gap_side == CL_GAP_TOP    ? height-2 : 0,
		                                        tab->gap_side == CL_GAP_RIGHT  ? width    : 0,
		                                        tab->gap_side == CL_GAP_BOTTOM ? height   : 0 );
	
		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
		

		cairo_pattern_add_color_stop_rgb  (pattern, 0.0,        stripe_fill->r, stripe_fill->g, stripe_fill->b);
		cairo_pattern_add_color_stop_rgb  (pattern, strip_size, stripe_fill->r, stripe_fill->g, stripe_fill->b);
		cairo_pattern_add_color_stop_rgba (pattern, strip_size, hilight.r, hilight.g, hilight.b, 0.5);
		cairo_pattern_add_color_stop_rgba (pattern, 0.8,        hilight.r, hilight.g, hilight.b, 0.0);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}

	ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
	
	if (params->active)
	{
		ge_cairo_set_color (cr, border2);
		cairo_stroke (cr);
	}
	else
	{
		pattern = cairo_pattern_create_linear ( tab->gap_side == CL_GAP_LEFT   ? width-2  : 2,
		                                        tab->gap_side == CL_GAP_TOP    ? height-2 : 2,
		                                        tab->gap_side == CL_GAP_RIGHT  ? width    : 2,
		                                        tab->gap_side == CL_GAP_BOTTOM ? height   : 2 );
		
		cairo_pattern_add_color_stop_rgb (pattern, 0.0,        stripe_border->r, stripe_border->g, stripe_border->b);
		cairo_pattern_add_color_stop_rgb (pattern, strip_size, stripe_border->r, stripe_border->g, stripe_border->b);
		cairo_pattern_add_color_stop_rgb (pattern, strip_size, border1->r,       border1->g,       border1->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0,        border2->r,       border2->g,       border2->b);
		cairo_set_source (cr, pattern);
		cairo_stroke (cr);
		cairo_pattern_destroy (pattern);
	}
}

static void
clearlooks_draw_separator (cairo_t *cr,
                           const ClearlooksColors     *colors,
                           const WidgetParameters     *widget,
                           const SeparatorParameters  *separator,
                           int x, int y, int width, int height)
{
	(void) widget;
	
	CairoColor color = colors->shade[3];
	CairoColor hilight; 
	ge_shade_color (&color, 1.4, &hilight);

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
	const CairoColor *border = &colors->shade[5];
	cairo_pattern_t *pattern;
	CairoColor hilight; 
	CairoColor shadow;

 	ge_shade_color (border, 1.5, &hilight);	
	ge_shade_color (border, 0.925, &shadow);	

	cairo_translate (cr, x, y);
	cairo_set_line_width (cr, 1.0);
	
	/* Draw highlight */
	if (header->order == CL_ORDER_FIRST)
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

	/* Draw bottom shade */	
	pattern = cairo_pattern_create_linear (0.0, height-5.0, 0.0, height-1.0);
	cairo_pattern_add_color_stop_rgba     (pattern, 0.0, shadow.r, shadow.g, shadow.b, 0.0);
	cairo_pattern_add_color_stop_rgba     (pattern, 1.0, shadow.r, shadow.g, shadow.b, 0.3);

	cairo_rectangle       (cr, 0.0, height-5.0, width, 4.0);
	cairo_set_source      (cr, pattern);
	cairo_fill            (cr);
	cairo_pattern_destroy (pattern);
	
	/* Draw resize grip */
	if ((params->ltr && header->order != CL_ORDER_LAST) ||
	    (!params->ltr && header->order != CL_ORDER_FIRST) || header->resizable)
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
	(void) widget;

	const CairoColor *fill  = &colors->bg[GTK_STATE_NORMAL];
	const CairoColor *dark  = &colors->shade[3];
	CairoColor light;
	ge_shade_color (fill, 1.1, &light);
	
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
	CairoColor border;
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
	
	ge_shade_color(&upper_color, 0.8, &border);	

	cairo_move_to  (cr, 0, 0.5);
	cairo_rel_line_to (cr, width, 0);
	cairo_move_to  (cr, 0, height-0.5);
	cairo_rel_line_to (cr, width, 0);

	ge_cairo_set_color (cr, &border);
	cairo_stroke (cr);

	cairo_restore (cr);
}


static void
clearlooks_draw_scrollbar_trough (cairo_t *cr,
                                  const ClearlooksColors           *colors,
                                  const WidgetParameters           *widget,
                                  const ScrollBarParameters        *scrollbar,
                                  int x, int y, int width, int height)
{
	(void) widget;

	const CairoColor *bg     = &colors->shade[2];
	const CairoColor *border = &colors->shade[5];
	CairoColor        bg_shade;
	cairo_pattern_t *pattern;
	
	ge_shade_color (bg, 0.95, &bg_shade);
	
	cairo_set_line_width (cr, 1);
	/* cairo_translate (cr, x, y); */
	
	if (scrollbar->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	cairo_translate (cr, x, y);	

	/* Draw fill */
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
	ge_cairo_set_color (cr, border);
	ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
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
	ShadowParameters shadow;
	double radius = MIN (widget->radius, MIN ((width - 2.0) / 2.0, (height - 2.0) / 2.0));

	ge_shade_color(&colors->shade[6], 1.05, &border);
	
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
				
	s2 = colors->bg[widget->state_type];
	ge_shade_color(&s2, 1.06, &s1);
	ge_shade_color(&s2, 0.98, &s3); 
	ge_shade_color(&s2, 0.94, &s4); 
	
	cairo_pattern_add_color_stop_rgb(pattern, 0,    s1.r, s1.g, s1.b);
	cairo_pattern_add_color_stop_rgb(pattern, 0.5,	s2.r, s2.g, s2.b);
	cairo_pattern_add_color_stop_rgb(pattern, 0.7,	s3.r, s3.g, s3.b);
	cairo_pattern_add_color_stop_rgb(pattern, 1.0,  s4.r, s4.g, s4.b);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);
	
	cairo_translate (cr, 0.5, 0.5);
	clearlooks_draw_top_left_highlight (cr, &s2, widget, width, height, (stepper->stepper == CL_STEPPER_A) ? radius : 0);
	cairo_translate (cr, -0.5, -0.5);
	
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, corners);
	clearlooks_set_border_gradient (cr, &border, 1.2, (scrollbar->horizontal ? 0 : width), (scrollbar->horizontal ? height: 0)); 
	cairo_stroke (cr);
	
	cairo_translate (cr, 0.5, 0.5);
	shadow.shadow  = CL_SHADOW_OUT;
	shadow.corners = corners;
	/*
	clearlooks_draw_highlight_and_shade (cr, &shadow,
	                                     width,
	                                     height, params->radius);*/
}

static void
clearlooks_draw_scrollbar_slider (cairo_t *cr,
                                   const ClearlooksColors          *colors,
                                   const WidgetParameters          *widget,
                                   const ScrollBarParameters       *scrollbar,
                                   int x, int y, int width, int height)
{
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

		ge_shade_color(&colors->shade[6], 1.05, &border);
		
		s2 = colors->bg[widget->state_type];
		ge_shade_color(&s2, 1.06, &s1);
		ge_shade_color(&s2, 0.98, &s3); 
		ge_shade_color(&s2, 0.94, &s4); 
	
		pattern = cairo_pattern_create_linear(1, 1, 1, height-1);
		cairo_pattern_add_color_stop_rgb(pattern, 0,   s1.r, s1.g, s1.b);
		cairo_pattern_add_color_stop_rgb(pattern, 0.5, s2.r, s2.g, s2.b);
		cairo_pattern_add_color_stop_rgb(pattern, 0.7, s3.r, s3.g, s3.b);
		cairo_pattern_add_color_stop_rgb(pattern, 1.0, s4.r, s4.g, s4.b);

		cairo_rectangle (cr, 1, 1, width-2, height-2);
		cairo_set_source(cr, pattern);
		cairo_fill(cr);
		cairo_pattern_destroy(pattern);
		
		clearlooks_set_border_gradient (cr, &border, 1.2, 0, height); 
		ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
		
		cairo_move_to (cr, 1.5, height-1.5);
		cairo_line_to (cr, 1.5, 1.5);
		cairo_line_to (cr, width-1.5, 1.5);
		ge_shade_color (&s2, 1.3, &s5);
		cairo_set_source_rgba (cr, s5.r, s5.g, s5.b, 0.5);
		cairo_stroke(cr);
		
		/* draw handles */
		cairo_set_line_width (cr, 1);
		
		bar_x = width/2 - 4;
		cairo_translate(cr, 0.5, 0.5);
		for (i=0; i<3; i++)
		{
			cairo_move_to (cr, bar_x, 4);
			cairo_line_to (cr, bar_x, height-5);
			ge_cairo_set_color (cr, dark);
			cairo_stroke (cr);
			
			cairo_move_to (cr, bar_x+1, 4);
			cairo_line_to (cr, bar_x+1, height-5);
			ge_cairo_set_color (cr, light);
			cairo_stroke (cr);
			
			bar_x += 3;
		}
	}
	
}

static void
clearlooks_draw_statusbar (cairo_t *cr,
                           const ClearlooksColors          *colors,
                           const WidgetParameters          *widget,
                           int x, int y, int width, int height)
{
	(void) widget;
	(void) height;
	
	const CairoColor *dark = &colors->shade[3];
	CairoColor hilight;

	ge_shade_color (dark, 1.4, &hilight);

	cairo_set_line_width  (cr, 1);
	cairo_translate       (cr, x, y+0.5);
	cairo_move_to         (cr, 0, 0);
	cairo_line_to         (cr, width, 0);
	ge_cairo_set_color  (cr, dark);
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
	(void) widget;

	const CairoColor *border = &colors->shade[5];
	cairo_translate      (cr, x, y);
	cairo_set_line_width (cr, 1);
/*
	cairo_set_source_rgba (cr, colors->bg[0].r, colors->bg[0].g, colors->bg[0].b, 0.9);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint          (cr);
*/
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
	int bar_spacing;
	
	switch (handle->type)
	{
		case CL_HANDLE_TOOLBAR:
			num_bars    = 6;
			bar_spacing = 3;
		break;
		case CL_HANDLE_SPLITTER:
			num_bars    = 16;
			bar_spacing = 3;
		break;
	}

	if (params->prelight)
	{
		cairo_rectangle (cr, x, y, width, height);
		ge_cairo_set_color (cr, fill);
		cairo_fill (cr);
	}
	
	cairo_translate (cr, x+0.5, y+0.5);
	
	cairo_set_line_width (cr, 1);
	
	if (handle->horizontal)
	{
		params->style_functions->draw_gripdots (cr, colors, 0, 0, width, height, num_bars, 2, 0.1);
	}
	else
	{
		params->style_functions->draw_gripdots (cr, colors, 0, 0, width, height, 2, num_bars, 0.1);
	}
}

static void
clearlooks_draw_resize_grip (cairo_t *cr,
                             const ClearlooksColors          *colors,
                             const WidgetParameters          *widget,
                             const ResizeGripParameters      *grip,
                             int x, int y, int width, int height)
{
	(void) widget;
	
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
	(void) width;
	(void) height;
	
	const CairoColor *border;
	const CairoColor *dot;
	CairoColor shadow;
	CairoColor highlight;
	cairo_pattern_t *pt;
	gboolean inconsistent;
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
		dot    = &colors->text[0];
	}

	ge_shade_color (&widget->parentbg, 0.9, &shadow);
	ge_shade_color (&widget->parentbg, 1.1, &highlight);

	pt = cairo_pattern_create_linear (0, 0, 13, 13);
	cairo_pattern_add_color_stop_rgb (pt, 0.0, shadow.r, shadow.b, shadow.g);
	cairo_pattern_add_color_stop_rgba (pt, 0.5, shadow.r, shadow.b, shadow.g, 0.5);
	cairo_pattern_add_color_stop_rgba (pt, 0.5, highlight.r, highlight.g, highlight.b, 0.5);
	cairo_pattern_add_color_stop_rgb (pt, 1.0, highlight.r, highlight.g, highlight.b);
	
	cairo_translate (cr, x, y);
	
	cairo_set_line_width (cr, 2);
	cairo_arc       (cr, 7, 7, 6, 0, G_PI*2);	
	cairo_set_source (cr, pt);
	cairo_stroke (cr);
	cairo_pattern_destroy (pt);

	cairo_set_line_width (cr, 1);

	cairo_arc       (cr, 7, 7, 5.5, 0, G_PI*2);	
	
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
			cairo_set_line_width (cr, 4);

			cairo_move_to(cr, 5, 7);
			cairo_line_to(cr, 9, 7);

			ge_cairo_set_color (cr, dot);
			cairo_stroke (cr);
		}
		else
		{
			cairo_arc (cr, 7, 7, 3, 0, G_PI*2);
			ge_cairo_set_color (cr, dot);
			cairo_fill (cr);
		
			cairo_arc (cr, 6, 6, 1, 0, G_PI*2);
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
		widget->style_functions->draw_inset (cr, &widget->parentbg, 0.5, 0.5, width-1, height-1, 1, CR_CORNER_ALL);
		
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
                       const ClearlooksColors          *colors,
                       const WidgetParameters          *widget,
                       const ArrowParameters           *arrow,
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
clearlooks_register_style_classic (ClearlooksStyleFunctions *functions)
{
	g_assert (functions);

	functions->draw_button             = clearlooks_draw_button;
	functions->draw_scale_trough       = clearlooks_draw_scale_trough;
	functions->draw_progressbar_trough = clearlooks_draw_progressbar_trough;
	functions->draw_progressbar_fill   = clearlooks_draw_progressbar_fill;
	functions->draw_slider_button      = clearlooks_draw_slider_button;
	functions->draw_entry              = clearlooks_draw_entry;
	functions->draw_spinbutton         = clearlooks_draw_spinbutton;
	functions->draw_spinbutton_down    = clearlooks_draw_spinbutton_down;
	functions->draw_optionmenu         = clearlooks_draw_optionmenu;
	functions->draw_inset              = clearlooks_draw_inset;
	functions->draw_menubar	           = clearlooks_draw_menubar;
	functions->draw_tab                = clearlooks_draw_tab;
	functions->draw_frame              = clearlooks_draw_frame;
	functions->draw_separator          = clearlooks_draw_separator;
	functions->draw_menu_item_separator = clearlooks_draw_menu_item_separator;
	functions->draw_list_view_header   = clearlooks_draw_list_view_header;
	functions->draw_toolbar            = clearlooks_draw_toolbar;
	functions->draw_menuitem           = clearlooks_draw_menuitem;
	functions->draw_menubaritem        = clearlooks_draw_menubaritem;
	functions->draw_selected_cell      = clearlooks_draw_selected_cell;
	functions->draw_scrollbar_stepper  = clearlooks_draw_scrollbar_stepper;
	functions->draw_scrollbar_slider   = clearlooks_draw_scrollbar_slider;
	functions->draw_scrollbar_trough   = clearlooks_draw_scrollbar_trough;
	functions->draw_statusbar          = clearlooks_draw_statusbar;
	functions->draw_menu_frame         = clearlooks_draw_menu_frame;
	functions->draw_tooltip            = clearlooks_draw_tooltip;
	functions->draw_handle             = clearlooks_draw_handle;
	functions->draw_resize_grip        = clearlooks_draw_resize_grip;
	functions->draw_arrow              = clearlooks_draw_arrow;
	functions->draw_checkbox           = clearlooks_draw_checkbox;
	functions->draw_radiobutton        = clearlooks_draw_radiobutton;
	functions->draw_shadow             = clearlooks_draw_shadow;
	functions->draw_slider             = clearlooks_draw_slider;
	functions->draw_gripdots           = clearlooks_draw_gripdots;
}
