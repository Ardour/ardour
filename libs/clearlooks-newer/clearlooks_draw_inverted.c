/* Clearlooks Inverted style
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


static void
clearlooks_draw_top_left_highlight (cairo_t *cr,
                 							const CairoColor *color,
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
	cairo_set_source_rgba (cr, hilight.r, hilight.g, hilight.b, 0.7);
	cairo_stroke          (cr);
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
clearlooks_inverted_draw_button (cairo_t *cr,
                        const ClearlooksColors *colors,
                        const WidgetParameters *params,
                        int x, int y, int width, int height)
{
	double xoffset = 0, yoffset = 0;
	double radius = params->radius;
	const CairoColor *fill = &colors->bg[params->state_type];	
	const CairoColor *border_disabled = &colors->shade[4];
	CairoColor border_normal;
	CairoColor shadow;

	ge_shade_color(&colors->shade[6], 1.05, &border_normal);
	ge_shade_color (&border_normal, 0.925, &shadow);
	
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

		CairoColor top_shade, bottom_shade;
		ge_shade_color (fill, 0.95, &top_shade);		
		ge_shade_color (fill, 1.05, &bottom_shade);
		
		pattern	= cairo_pattern_create_linear (0, 0, 0, height);
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, top_shade.r, top_shade.g, top_shade.b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, bottom_shade.r, bottom_shade.g, bottom_shade.b);
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
	
	if (params->disabled)
			ge_cairo_set_color (cr, border_disabled);
	else
		if (!params->active)
			clearlooks_set_border_gradient (cr, &border_normal, 1.32, 0, height); 
		else
			ge_cairo_set_color (cr, &border_normal);
	
	ge_cairo_rounded_rectangle (cr, xoffset + 0.5, yoffset + 0.5,
                                  width-(xoffset*2)-1, height-(yoffset*2)-1,
                                  radius, params->corners);
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
clearlooks_inverted_draw_progressbar_fill (cairo_t *cr,
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
	CairoColor       top_shade;

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

		
	ge_shade_color (&colors->spot[1], 1.05, &top_shade);

	/* Draw the background gradient */
	ge_shade_color (&colors->spot[1], 0.925, &bg_shade);
	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, bg_shade.r, bg_shade.g, bg_shade.b);
	cairo_pattern_add_color_stop_rgb (pattern, 0.5, top_shade.r, top_shade.g, top_shade.b);
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
clearlooks_inverted_draw_menuitem (cairo_t *cr,
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
	cairo_pattern_add_color_stop_rgb (pattern, 0, fill_shade.r, fill_shade.g, fill_shade.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0,   fill->r, fill->g, fill->b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve  (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, &border);
	cairo_stroke (cr);
}

static void
clearlooks_inverted_draw_menubaritem (cairo_t *cr,
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
	cairo_pattern_add_color_stop_rgb (pattern, 0, fill_shade.r, fill_shade.g, fill_shade.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0,   fill->r, fill->g, fill->b);

	cairo_set_source (cr, pattern);
	cairo_fill_preserve  (cr);
	cairo_pattern_destroy (pattern);

	ge_cairo_set_color (cr, &border);
	cairo_stroke_preserve (cr);
}

static void
clearlooks_inverted_draw_tab (cairo_t *cr,
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
	CairoColor           shadow;

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

	if (params->active)
	{
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
		cairo_pattern_add_color_stop_rgba (pattern, strip_size, hilight.r, hilight.g, hilight.b, 0.0);
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
clearlooks_inverted_draw_slider (cairo_t *cr,
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

	if (params->disabled)
		border = &colors->shade[4];
	else if (params->prelight)
		border = &colors->spot[2];
	else
		border = &colors->shade[6];

	/* fill the widget */
	cairo_rectangle (cr, 0.5, 0.5, width-2, height-2);

	/* Fake light */
	if (!params->disabled)
	{
		const CairoColor *top = &colors->shade[2];
		const CairoColor *bot = &colors->shade[0];

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
		cairo_pattern_add_color_stop_rgb (pattern, 0.0, spot->r, spot->g, spot->b);
		cairo_pattern_add_color_stop_rgb (pattern, 1.0, highlight.r, highlight.g, highlight.b);
		cairo_set_source (cr, pattern);
	}
	else {
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
clearlooks_inverted_draw_slider_button (cairo_t *cr,
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
clearlooks_inverted_draw_list_view_header (cairo_t *cr,
                                  const ClearlooksColors          *colors,
                                  const WidgetParameters          *params,
                                  const ListViewHeaderParameters  *header,
                                  int x, int y, int width, int height)
{
	const CairoColor *fill = &colors->bg[params->state_type];
	const CairoColor *border = &colors->shade[4];
	cairo_pattern_t *pattern;
	CairoColor hilight_header;
	CairoColor hilight;
	CairoColor shadow;

	ge_shade_color (border, 1.5, &hilight);
	ge_shade_color (fill, 1.05, &hilight_header);	
	ge_shade_color (fill, 0.95, &shadow);	

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
	pattern = cairo_pattern_create_linear (0.0, 0, 0.0, height-1.0);
	cairo_pattern_add_color_stop_rgb     (pattern, 0.0, shadow.r, shadow.g, shadow.b);
	cairo_pattern_add_color_stop_rgb     (pattern, 1.0, hilight_header.r, hilight_header.g, hilight_header.b);

	cairo_rectangle       (cr, 0, 1, width, height-2);
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


static void
clearlooks_inverted_draw_scrollbar_stepper (cairo_t *cr,
                                   const ClearlooksColors           *colors,
                                   const WidgetParameters           *widget,
                                   const ScrollBarParameters        *scrollbar,
                                   const ScrollBarStepperParameters *stepper,
                                   int x, int y, int width, int height)
{
	CairoCorners corners = CR_CORNER_NONE;
	CairoColor border;
	CairoColor s1, s2, s3;
	cairo_pattern_t *pattern;
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
				
	s1 = colors->bg[widget->state_type];
	ge_shade_color(&s1, 0.95, &s2); 
	ge_shade_color(&s1, 1.05, &s3); 
	
	cairo_pattern_add_color_stop_rgb(pattern, 0,    s2.r, s2.g, s2.b);
	cairo_pattern_add_color_stop_rgb(pattern, 1.0,  s3.r, s3.g, s3.b);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);
	cairo_pattern_destroy (pattern);

	clearlooks_draw_top_left_highlight (cr, &s1, widget, width, height, radius);

	ge_cairo_rounded_rectangle (cr, 0.5, 0.5, width-1, height-1, radius, corners);	
	clearlooks_set_border_gradient (cr, &border, 1.2, (scrollbar->horizontal ? 0 : width), (scrollbar->horizontal ? height: 0)); 
	cairo_stroke (cr);
	
	cairo_translate (cr, 0.5, 0.5);
}

static void
clearlooks_inverted_draw_scrollbar_slider (cairo_t *cr,
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
		const CairoColor *border = &colors->shade[8];
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
		cairo_pattern_add_color_stop_rgb (pattern, 0,    fill.r,  fill.g,  fill.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.5,  shade3.r, shade3.g, shade3.b);
		cairo_pattern_add_color_stop_rgb (pattern, 0.5,  shade2.r, shade2.g, shade2.b);	
		cairo_pattern_add_color_stop_rgb (pattern, 1.0,  shade1.r, shade1.g, shade1.b);
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
		CairoColor border;
		CairoColor s1, s2, s3;
		cairo_pattern_t *pattern;
		int bar_x, i;
		
		const CairoColor *dark  = &colors->shade[4];
		const CairoColor *light = &colors->shade[0];		

		ge_shade_color(&colors->shade[6], 1.05, &border);

		pattern = cairo_pattern_create_linear(1, 1, 1, height-1);

		s1 = colors->bg[widget->state_type];
		ge_shade_color(&s1, 0.95, &s2); 
		ge_shade_color(&s1, 1.05, &s3); 

		cairo_pattern_add_color_stop_rgb(pattern, 0,    s2.r, s2.g, s2.b);
		cairo_pattern_add_color_stop_rgb(pattern, 1.0,  s3.r, s3.g, s3.b);

		cairo_rectangle (cr, 1, 1, width-2, height-2);
		cairo_set_source(cr, pattern);
		cairo_fill(cr);
		cairo_pattern_destroy(pattern);
		
		clearlooks_draw_top_left_highlight (cr, &s2, widget, width, height, 0);

		clearlooks_set_border_gradient (cr, &border, 1.2, 0, height);
		ge_cairo_stroke_rectangle (cr, 0.5, 0.5, width-1, height-1);
		
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
clearlooks_inverted_draw_selected_cell (cairo_t                  *cr,
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

	ge_shade_color(&upper_color, 0.9, &lower_color);

	pattern = cairo_pattern_create_linear (0, 0, 0, height);
	cairo_pattern_add_color_stop_rgb (pattern, 0.0, lower_color.r,
	                                                lower_color.g,
	                                                lower_color.b);
	cairo_pattern_add_color_stop_rgb (pattern, 1.0, upper_color.r,
	                                                upper_color.g,
	                                                upper_color.b);


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

void
clearlooks_register_style_inverted (ClearlooksStyleFunctions *functions)
{
	functions->draw_button            = clearlooks_inverted_draw_button;
	functions->draw_slider            = clearlooks_inverted_draw_slider;
	functions->draw_slider_button     = clearlooks_inverted_draw_slider_button;
	functions->draw_progressbar_fill  = clearlooks_inverted_draw_progressbar_fill;
	functions->draw_menuitem          = clearlooks_inverted_draw_menuitem;
	functions->draw_menubaritem       = clearlooks_inverted_draw_menubaritem;
	functions->draw_tab               = clearlooks_inverted_draw_tab;
	functions->draw_list_view_header  = clearlooks_inverted_draw_list_view_header;
	functions->draw_scrollbar_stepper = clearlooks_inverted_draw_scrollbar_stepper;	
	functions->draw_scrollbar_slider  = clearlooks_inverted_draw_scrollbar_slider;
	functions->draw_selected_cell     = clearlooks_inverted_draw_selected_cell;
}

