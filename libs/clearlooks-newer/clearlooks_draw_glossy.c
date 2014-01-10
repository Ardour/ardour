/* Clearlooks Glossy style
 * Copyright (C) 2006 Benjamin Berg
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
 *        and Benjamin Berg <benjamin@sipsolutions.net>
 * Based on code by Richard Stellingwerff <remenic@gmail.com>
 *              and Daniel Borgmann <daniel.borgmann@gmail.com>
 * from the ubuntulooks engine.
 */

#include "clearlooks_draw.h"
#include "clearlooks_style.h"
#include "clearlooks_types.h"

#include "support.h"
#include <ge-support.h>

#include <cairo.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void
clearlooks_draw_glossy_gradient (cairo_t         *cr,
                                double x, double y, int width, int height,
                                const CairoColor *color,
                                gboolean disabled, gboolean radius, CairoCorners corners)
{
	CairoColor a, b, c, d;
	cairo_pattern_t *pt;

	ge_shade_color (color, disabled? 1.06 : 1.16, &a);
	ge_shade_color (color, disabled? 1.02 : 1.08, &b);
	ge_shade_color (color, disabled? 0.98 : 1.00, &c);
	ge_shade_color (color, disabled? 1.02 : 1.08, &d);

	pt = cairo_pattern_create_linear (x, y, x, y+height);
	cairo_pattern_add_color_stop_rgb (pt, 0.0,  a.r, a.g, a.b);
	cairo_pattern_add_color_stop_rgb (pt, 0.5,  b.r, b.g, b.b);
	cairo_pattern_add_color_stop_rgb (pt, 0.5,  c.r, c.g, c.b);
	cairo_pattern_add_color_stop_rgb (pt, 1.0,  d.r, d.g, d.b);

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
clearlooks_glossy_draw_inset (cairo_t          *cr, 
                              const CairoColor *bg_color,
                              double x, double y, double w, double h,
                              double radius, uint8 corners)
{
	CairoColor shadow;
	CairoColor highlight;

	/* not really sure of shading ratios... we will think */
	ge_shade_color (bg_color, 0.93, &shadow);
	ge_shade_color (bg_color, 1.07, &highlight);

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
clearlooks_glossy_draw_light_inset (cairo_t          *cr, 
                                    const CairoColor *bg_color, 
                                    double x, double y, double w, double h, 
                                    double radius, uint8 corners)
{
	CairoColor shadow;
	CairoColor highlight;

	/* not really sure of shading ratios... we will think */
	ge_shade_color (bg_color, 0.95, &shadow);
	ge_shade_color (bg_color, 1.05, &highlight);

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
clearlooks_glossy_draw_highlight_and_shade (cairo_t          *cr, 
                                            const CairoColor *bg_color,
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
clearlooks_glossy_draw_button (cairo_t *cr,
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

	/* Shadows and Glow */
	if (params->xthickness == 3 || params->ythickness == 3)
	{
		if (params->xthickness == 3)
			xoffset = 1;
		if (params->ythickness == 3)
			yoffset = 1;
	}

	radius = MIN (params->radius, MIN ((width - 2.0 - 2*xoffset) / 2.0, (height - 2.0 - 2*yoffset) / 2.0));

	if (params->xthickness == 3 || params->ythickness == 3)
	{
		cairo_translate (cr, 0.5, 0.5);

		/* if (params->enable_glow && !params->active && !params->disabled) */
		if (params->prelight && params->enable_glow && !params->active)
		{
			/* Glow becomes a shadow to have 3d prelight buttons :) */
			CairoColor glow;

			radius = MIN (params->radius, MIN ((width - 2.0 - 2*xoffset) / 2.0 - 1.0, (height - 2.0 - 2*yoffset) / 2.0 - 1.0));

			ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius+1, params->corners);
			ge_shade_color (&params->parentbg, 0.96, &glow);
			ge_cairo_set_color (cr, &glow);
			cairo_stroke (cr);

			ge_cairo_rounded_rectangle (cr, 1, 1, width-2, height-2, radius+1, params->corners);
			ge_shade_color (&params->parentbg, 0.92, &glow);
			ge_cairo_set_color (cr, &glow);
			cairo_stroke (cr);
		}
		
		/* if (!(params->enable_glow && !params->active && !params->disabled)) */
		if (!(params->prelight && params->enable_glow && !params->active)) {
			if (!(params->disabled))
				params->style_functions->draw_inset (cr, &params->parentbg, 0, 0, width-1, height-1, params->radius+1, params->corners);
			else
				/*Draw a lighter inset */
				clearlooks_glossy_draw_light_inset (cr, &params->parentbg, 0, 0, width-1, height-1, params->radius+1, params->corners);
		}
		cairo_translate (cr, -0.5, -0.5);
	}

	clearlooks_draw_glossy_gradient (cr, xoffset+1, yoffset+1, 
	                              width-(xoffset*2)-2, height-(yoffset*2)-2, 
	                              &fill, params->disabled, radius, params->corners);
	
	/* Pressed button shadow */
	if (params->active)
	{
		CairoColor shadow;
		ge_shade_color (&fill, 0.92, &shadow);

		cairo_save (cr);

		ge_cairo_rounded_rectangle (cr, xoffset+1, yoffset+1, width-(xoffset*2)-2, height, radius, params->corners & (CR_CORNER_TOPLEFT | CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMLEFT));
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
	
	/* Default button highlight */
	if (params->is_default && !params->active && !params->disabled)
	{
		const CairoColor *glow = &colors->spot[0];
		double hh = (height-5)/2.0 + 1;
		
		cairo_rectangle (cr, 3.5, 3.5, width-7, height-7);
		ge_cairo_set_color (cr, glow);
		cairo_stroke (cr);

		glow = &colors->spot[0];
		cairo_move_to (cr, 2.5, 2.5+hh); cairo_rel_line_to (cr, 0, -hh);
		cairo_rel_line_to (cr, width-5, 0); cairo_rel_line_to (cr, 0, hh);
		ge_cairo_set_color (cr, glow);
		cairo_stroke (cr);
		
		hh--;

		glow = &colors->spot[1];
		cairo_move_to (cr, 2.5, 2.5+hh); cairo_rel_line_to (cr, 0, hh);
		cairo_rel_line_to (cr, width-5, 0); cairo_rel_line_to (cr, 0, -hh);
		ge_cairo_set_color (cr, glow);
		cairo_stroke (cr);
	}
	
	/* Border */
	if (params->is_default || (params->prelight && params->enable_glow))
		border_normal = colors->spot[2];
		/* ge_mix_color (&border_normal, &colors->spot[2], 0.5, &border_normal); */
	if (params->disabled)
		ge_cairo_set_color (cr, &border_disabled);
	else
		clearlooks_set_mixed_color (cr, &border_normal, &fill, 0.2);
	ge_cairo_rounded_rectangle (cr, xoffset + 0.5, yoffset + 0.5,
                                  width-(xoffset*2)-1, height-(yoffset*2)-1,
                                  radius, params->corners);
	cairo_stroke (cr);
	cairo_restore (cr);
}

static void
clearlooks_glossy_draw_progressbar_trough (cairo_t *cr,
                                    const ClearlooksColors *colors,
                                    const WidgetParameters *params,
                                    int x, int y, int width, int height)
{
	const CairoColor *border = &colors->shade[6];
	CairoColor        shadow;
	cairo_pattern_t  *pattern;
	double           radius = MIN (params->radius, MIN ((height-2.0) / 2.0, (width-2.0) / 2.0));
	
	cairo_save (cr);

	cairo_set_line_width (cr, 1.0);
	
	/* Fill with bg color */
	ge_cairo_set_color (cr, &colors->bg[params->state_type]);
	
	cairo_rectangle (cr, x, y, width, height);
	cairo_fill (cr);

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
clearlooks_glossy_draw_progressbar_fill (cairo_t *cr,
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
	CairoColor       a;
	CairoColor       b;
	CairoColor       e;
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
	ge_shade_color (&colors->spot[1], 1.16, &a);
	ge_shade_color (&colors->spot[1], 1.08, &b);
	ge_shade_color (&colors->spot[1], 1.08, &e);
	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, a.r, a.g, a.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, b.r, b.g, b.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, colors->spot[1].r, colors->spot[1].g, colors->spot[1].b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, e.r, e.g, e.b);
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
	cairo_set_source_rgba (cr, colors->spot[0].r, colors->spot[0].g, colors->spot[0].b, 0.3);

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
	ge_shade_color (&colors->shade[6], 0.92, &shadow);
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
clearlooks_glossy_scale_draw_gradient (cairo_t *cr,
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
	
	clearlooks_set_mixed_color (cr, c3, c1, 0.3);
	ge_cairo_stroke_rectangle (cr, x, y, width, height);
}

#define TROUGH_SIZE 6
static void
clearlooks_glossy_draw_scale_trough (cairo_t *cr,
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
	
	if (!slider->lower && !slider->fill_level)
		clearlooks_glossy_scale_draw_gradient (cr, &colors->shade[3], /* top */
		                                    &colors->shade[2], /* bottom */
		                                    &colors->shade[6], /* border */
		                                    0, 0, trough_width, trough_height,
		                                    slider->horizontal);
	else
		clearlooks_glossy_scale_draw_gradient (cr, &colors->spot[1], /* top */
		                                    &colors->spot[0], /* bottom */
		                                    &colors->spot[2], /* border */
		                                    0, 0, trough_width, trough_height,
		                                    slider->horizontal);
}

static void
clearlooks_glossy_draw_tab (cairo_t *cr,
                            const ClearlooksColors *colors,
                            const WidgetParameters *params,
                            const TabParameters    *tab,
                            int x, int y, int width, int height)
{

	const CairoColor    *border       = &colors->shade[5];
	const CairoColor    *stripe_fill   = &colors->spot[1];
	const CairoColor    *stripe_border = &colors->spot[2];
	const CairoColor    *fill;
	CairoColor           hilight;

	cairo_pattern_t     *pattern;
	
	double               radius;

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
		
		if (tab->gap_side == CL_GAP_TOP)
			cairo_translate (cr, 0.0, -3.0); /* gap at the other side */
	}
	else
	{
		width += 3.0;
		
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
		
		clearlooks_glossy_draw_highlight_and_shade (cr, &colors->bg[0], &shadow,
		                                     width,
		                                     height, radius);
	}

	if (params->active)
	{
		CairoColor shadow, f1, f2;

		pattern = cairo_pattern_create_linear (tab->gap_side == CL_GAP_LEFT   ? width-1  : 0,
		                                       tab->gap_side == CL_GAP_TOP    ? height-2 : 1,
		                                       tab->gap_side == CL_GAP_RIGHT  ? width    : 0,
		                                       tab->gap_side == CL_GAP_BOTTOM ? height   : 0);

		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
		
		ge_shade_color (fill, 1.06, &shadow);
		ge_shade_color (fill, 1.18, &hilight);
		ge_shade_color (fill, 1.12, &f1);
		ge_shade_color (fill, 1.06, &f2);

		cairo_pattern_add_color_stop_rgb (pattern, 0.0,        hilight.r, hilight.g, hilight.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0/height, hilight.r, hilight.g, hilight.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0/height, f1.r, f1.g, f1.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.45,       f2.r, f2.g, f2.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.45,       fill->r, fill->g, fill->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0,        shadow.r, shadow.g, shadow.b);
		cairo_set_source (cr, pattern);
		cairo_fill (cr);
		cairo_pattern_destroy (pattern);
	}
	else
	{
		/* Draw shade */
		pattern = cairo_pattern_create_linear (tab->gap_side == CL_GAP_LEFT   ? width-2  : 0,
		                                       tab->gap_side == CL_GAP_TOP    ? height-2 : 0,
		                                       tab->gap_side == CL_GAP_RIGHT  ? width    : 0,
		                                       tab->gap_side == CL_GAP_BOTTOM ? height   : 0);
	
		ge_cairo_rounded_rectangle (cr, 0, 0, width-1, height-1, radius, params->corners);
		

		cairo_pattern_add_color_stop_rgba (pattern, 0.0, stripe_fill->r, stripe_fill->g, stripe_fill->b, 0.5);
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
		pattern = cairo_pattern_create_linear (tab->gap_side == CL_GAP_LEFT   ? width-2  : 2,
		                                       tab->gap_side == CL_GAP_TOP    ? height-2 : 2,
		                                       tab->gap_side == CL_GAP_RIGHT  ? width    : 2,
		                                       tab->gap_side == CL_GAP_BOTTOM ? height   : 2);
		
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, stripe_border->r, stripe_border->g, stripe_border->b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.8, border->r,        border->g,        border->b);
		cairo_set_source (cr, pattern);
		cairo_stroke (cr);
		cairo_pattern_destroy (pattern);
	}
}

static void
clearlooks_glossy_draw_slider (cairo_t *cr,
                        const ClearlooksColors *colors,
                        const WidgetParameters *params,
                        int x, int y, int width, int height)
{
	const CairoColor *border  = &colors->shade[7];
	CairoColor  fill;
	CairoColor  hilight;
	CairoColor  a, b, c, d;
	cairo_pattern_t *pattern;

	cairo_set_line_width (cr, 1.0);	
	cairo_translate      (cr, x, y);

	cairo_translate (cr, -0.5, -0.5);

	ge_shade_color (&colors->bg[params->state_type], 1.0, &fill);
	if (params->prelight)
		ge_shade_color (&fill, 1.1, &fill);

	ge_shade_color (&fill, 1.25, &hilight);
	ge_shade_color (&fill, 1.16, &a);
	ge_shade_color (&fill, 1.08, &b);
	ge_shade_color (&fill, 1.0,  &c);
	ge_shade_color (&fill, 1.08, &d);

	pattern = cairo_pattern_create_linear (1, 1, 1, height-2);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   a.r, a.g, a.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, b.r, b.g, b.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, c.r, c.g, c.b);	
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, d.r, d.g, d.b);
	cairo_rectangle (cr, 1, 1, width-2, height-2);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	clearlooks_set_mixed_color (cr, border, &fill, 0.2);
	if (params->prelight)
		ge_cairo_set_color (cr, &colors->spot[2]);
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, 2.5, params->corners);
	cairo_stroke (cr);

	cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
	ge_cairo_rounded_rectangle (cr, 1.5, 1.5, width-3, height-3, 2.0, params->corners);
	cairo_stroke (cr);
}

static void
clearlooks_glossy_draw_slider_button (cairo_t *cr,
                                      const ClearlooksColors *colors,
                                      const WidgetParameters *params,
                                      const SliderParameters *slider,
                                      int x, int y, int width, int height)
{
	double radius = MIN (params->radius, MIN ((width - 1.0) / 2.0, (height - 1.0) / 2.0));

	cairo_set_line_width (cr, 1.0);
	
	if (!slider->horizontal)
		ge_cairo_exchange_axis (cr, &x, &y, &width, &height);

	cairo_translate (cr, x+0.5, y+0.5);
	
	params->style_functions->draw_shadow (cr, colors, radius, width-1, height-1);
	params->style_functions->draw_slider (cr, colors, params, 1, 1, width-2, height-2);
}

static void
clearlooks_glossy_draw_scrollbar_stepper (cairo_t *cr,
                                   const ClearlooksColors           *colors,
                                   const WidgetParameters           *widget,
                                   const ScrollBarParameters        *scrollbar,
                                   const ScrollBarStepperParameters *stepper,
                                   int x, int y, int width, int height)
{
	CairoCorners corners = CR_CORNER_NONE;
	const CairoColor *border = &colors->shade[7];
	CairoColor  fill, s1, s2, s4;
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
	ge_shade_color(&fill, 1.16, &s1);
	ge_shade_color(&fill, 1.08, &s2);
	ge_shade_color(&fill, 1.08, &s4);
	
	cairo_pattern_add_color_stop_rgb(pattern, 0,    s1.r, s1.g, s1.b);
	cairo_pattern_add_color_stop_rgb(pattern, 0.5,	s2.r, s2.g, s2.b);
	cairo_pattern_add_color_stop_rgb(pattern, 0.5,	fill.r, fill.g, fill.b);
	cairo_pattern_add_color_stop_rgb(pattern, 1.0,  s4.r, s4.g, s4.b);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);
	
	cairo_translate (cr, 0.5, 0.5);
	cairo_translate (cr, -0.5, -0.5);
	
	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, corners);
	clearlooks_set_mixed_color (cr, border, &fill, 0.2);
	if (widget->prelight)
		ge_cairo_set_color (cr, &colors->spot[2]);
	cairo_stroke (cr);
	
	cairo_translate (cr, 0.5, 0.5);
}

static void
clearlooks_glossy_draw_scrollbar_slider (cairo_t *cr,
                                   const ClearlooksColors          *colors,
                                   const WidgetParameters          *widget,
                                   const ScrollBarParameters       *scrollbar,
                                   int x, int y, int width, int height)
{
	const CairoColor *border  = &colors->shade[7];
	CairoColor  fill  = scrollbar->color;
	CairoColor  hilight;
	CairoColor  shade1, shade2, shade3;
	cairo_pattern_t *pattern;

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
		ge_shade_color (&fill, 1.1, &fill);
		
	cairo_set_line_width (cr, 1);
	
	ge_shade_color (&fill, 1.25, &hilight);
	ge_shade_color (&fill, 1.16, &shade1);
	ge_shade_color (&fill, 1.08, &shade2);
	ge_shade_color (&fill, 1.08, &shade3);
	
	pattern = cairo_pattern_create_linear (1, 1, 1, height-2);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, 	fill.r,  fill.g,  fill.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1,	shade3.r, shade3.g, shade3.b);
	cairo_rectangle (cr, 1, 1, width-2, height-2);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);
	
	if (scrollbar->has_color) 
	{
		cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
		ge_cairo_stroke_rectangle (cr, 1.5, 1.5, width-3, height-3);
	}

	clearlooks_set_mixed_color (cr, border, &fill, scrollbar->has_color? 0.4 : 0.2);
	ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
}

static void
clearlooks_glossy_draw_list_view_header (cairo_t *cr,
                                  const ClearlooksColors          *colors,
                                  const WidgetParameters          *params,
                                  const ListViewHeaderParameters  *header,
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

 	ge_shade_color (fill, 1.2, &hilight);
	ge_shade_color (fill, 1.08, &shade1);
	ge_shade_color (fill, 1.04, &shade2);
	ge_shade_color (fill, 1.04, &shade3);

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
	if (header->order == CL_ORDER_FIRST)
	{
		cairo_move_to (cr, 0.5, height-1);
		cairo_line_to (cr, 0.5, 0.5);
	}
	else
		cairo_move_to (cr, 0.0, 0.5);
	
	cairo_line_to (cr, width, 0.5);
	
	cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.5);
	cairo_stroke (cr);
	
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

static void 
clearlooks_glossy_draw_toolbar (cairo_t *cr,
                         const ClearlooksColors          *colors,
                         const WidgetParameters          *widget,
                         const ToolbarParameters         *toolbar,
                         int x, int y, int width, int height)
{
	CairoColor light;
	const CairoColor *dark;
	
	const CairoColor *fill  = &colors->bg[GTK_STATE_NORMAL];
	dark  = &colors->shade[3];

	(void) widget;
	(void) width;
	(void) height;

	ge_shade_color (fill, 1.1, &light);
	
	cairo_set_line_width (cr, 1.0);
	cairo_translate (cr, x, y);
	
	if (toolbar->style == 1) /* Enable Extra features */
	{ 
		cairo_pattern_t *pattern;
		CairoColor shade1, shade2, shade3;
		
		ge_shade_color (fill, 1.08, &shade1);
		ge_shade_color (fill, 1.04, &shade2);
		ge_shade_color (fill, 1.04, &shade3);

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

		if (!toolbar->topmost) 
		{
			/* Draw highlight */
			cairo_move_to       (cr, 0, 0.5);
			cairo_line_to       (cr, width-1, 0.5);
			ge_cairo_set_color  (cr, &light);
			cairo_stroke        (cr);
		}	
	}

	/* Draw shadow */
	cairo_move_to       (cr, 0, height-0.5);
	cairo_line_to       (cr, width-1, height-0.5);
	ge_cairo_set_color  (cr, dark);
	cairo_stroke        (cr);
}

static void
clearlooks_glossy_draw_menuitem (cairo_t                   *cr,
                                 const ClearlooksColors    *colors,
                                 const WidgetParameters    *params,
                                 int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->spot[1];
	const CairoColor *border = &colors->spot[2];
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;

	ge_shade_color (fill, 1.16, &shade1);
	ge_shade_color (fill, 1.08, &shade2);
	ge_shade_color (fill, 1.08, &shade3);
	cairo_set_line_width (cr, 1.0);

	ge_cairo_rounded_rectangle (cr, x+0.5, y+0.5, width - 1, height - 1, params->radius, params->corners);

	pattern = cairo_pattern_create_linear (x, y, x, y + height);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill->r,  fill->g,  fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1,	shade3.r, shade3.g, shade3.b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve  (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);
}

static void
clearlooks_glossy_draw_menubaritem (cairo_t                   *cr,
                                    const ClearlooksColors    *colors,
                                    const WidgetParameters    *params,
                                    int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->spot[1];
	const CairoColor *border = &colors->spot[2];
	CairoColor shade1, shade2, shade3;
	cairo_pattern_t *pattern;

	ge_shade_color (fill, 1.16, &shade1);
	ge_shade_color (fill, 1.08, &shade2);
	ge_shade_color (fill, 1.08, &shade3);
	cairo_set_line_width (cr, 1.0);

	ge_cairo_rounded_rectangle (cr, x+0.5, y+0.5, width - 1, height - 1, params->radius, params->corners);

	pattern = cairo_pattern_create_linear (x, y, x, y + height);
	cairo_pattern_add_color_stop_rgb (pattern, 0,   shade1.r, shade1.g, shade1.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5,	shade2.r, shade2.g, shade2.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, fill->r,  fill->g,  fill->b);
	cairo_pattern_add_color_stop_rgb (pattern, 1,	shade3.r, shade3.g, shade3.b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve  (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, border);
	cairo_stroke (cr);
}

static void
clearlooks_glossy_draw_selected_cell (cairo_t                  *cr,
	                              const ClearlooksColors   *colors,
	                              const WidgetParameters   *params,
	                              int x, int y, int width, int height)
{
	CairoColor color;

	if (params->focus)
		color = colors->base[params->state_type];
	else
		color = colors->base[GTK_STATE_ACTIVE];

	clearlooks_draw_glossy_gradient (cr, x, y, width, height, &color, params->disabled, 0.0, CR_CORNER_NONE);
}


static void
clearlooks_glossy_draw_radiobutton (cairo_t *cr,
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

	(void) width;
	(void) height;

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
clearlooks_glossy_draw_checkbox (cairo_t *cr,
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
		widget->style_functions->draw_inset (cr, &widget->parentbg, 0.5, 0.5, 
                                           width-1, height-1, (widget->radius > 0)? 1 : 0, CR_CORNER_ALL);
		
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

void
clearlooks_register_style_glossy (ClearlooksStyleFunctions *functions)
{
	functions->draw_inset              = clearlooks_glossy_draw_inset;
	functions->draw_button             = clearlooks_glossy_draw_button;
	functions->draw_progressbar_trough = clearlooks_glossy_draw_progressbar_trough;
	functions->draw_progressbar_fill   = clearlooks_glossy_draw_progressbar_fill;
	functions->draw_scale_trough       = clearlooks_glossy_draw_scale_trough;
	functions->draw_tab                = clearlooks_glossy_draw_tab;
	functions->draw_slider             = clearlooks_glossy_draw_slider;
	functions->draw_slider_button      = clearlooks_glossy_draw_slider_button;
	functions->draw_scrollbar_stepper  = clearlooks_glossy_draw_scrollbar_stepper;
	functions->draw_scrollbar_slider   = clearlooks_glossy_draw_scrollbar_slider;
	functions->draw_list_view_header   = clearlooks_glossy_draw_list_view_header;
	functions->draw_toolbar            = clearlooks_glossy_draw_toolbar;
	functions->draw_menuitem           = clearlooks_glossy_draw_menuitem;
	functions->draw_menubaritem        = clearlooks_glossy_draw_menubaritem;
	functions->draw_selected_cell      = clearlooks_glossy_draw_selected_cell;
	functions->draw_checkbox           = clearlooks_glossy_draw_checkbox;
	functions->draw_radiobutton        = clearlooks_glossy_draw_radiobutton;
}
