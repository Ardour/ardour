/*
 * Copyright (C) 2018 Johannes Mueller <github@johannes-mueller.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


/* Shared code for a-comp.c and a-exp.c
 *
 * This file contains some code that draws the basic layout of for the inline
 * rendering of a-comp.c and a-exp.c. It is put into a shared file for better
 * maintainability. It is not meant to be compiled as a individual compilation
 * unit but to be included like
 *
 * #include "dynamic_screen.c"
 *
 */


static inline void
draw_grid (cairo_t* cr, const float w, const float h)
{
	// clear background
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_fill (cr);

	cairo_set_line_width(cr, 1.0);

	// draw grid 10dB steps
	const double dash1[] = {1, 2};
	const double dash2[] = {1, 3};
	cairo_save (cr);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_dash(cr, dash2, 2, 2);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	for (uint32_t d = 1; d < 7; ++d) {
		const float x = -.5 + floorf (w * (d * 10.f / 70.f));
		const float y = -.5 + floorf (h * (d * 10.f / 70.f));

		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
		cairo_stroke (cr);

		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 1.0);
	cairo_set_dash(cr, dash1, 2, 2);

	// diagonal unity
	cairo_move_to (cr, 0, h);
	cairo_line_to (cr, w, 0);
	cairo_stroke (cr);
	cairo_restore (cr);

	{ // 0, 0
		cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);
		const float x = -.5 + floorf (w * (60.f / 70.f));
		const float y = -.5 + floorf (h * (10.f / 70.f));
		cairo_move_to (cr, x, 0);
		cairo_line_to (cr, x, h);
		cairo_stroke (cr);
		cairo_move_to (cr, 0, y);
		cairo_line_to (cr, w, y);
		cairo_stroke (cr);
	}
}

static inline void
draw_GR_bar (cairo_t* cr, const float w, const float h, const float gainr)
{
	const float x = -.5 + floorf (w * (62.5f / 70.f));
	const float y = -.5 + floorf (h * (10.0f / 70.f));
	const float wd = floorf (w * (5.f / 70.f));
	const float ht = floorf (h * (55.f / 70.f));
	cairo_rectangle (cr, x, y, wd, ht);
	cairo_fill (cr);

	const float h_gr = fminf (ht, floorf (h * gainr / 70.f));
	cairo_set_source_rgba (cr, 0.95, 0.0, 0.0, 1.0);
	cairo_rectangle (cr, x, y, wd, h_gr);
	cairo_fill (cr);
	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);
	cairo_rectangle (cr, x, y, wd, ht);
	cairo_set_source_rgba (cr, 0.75, 0.75, 0.75, 1.0);
	cairo_stroke (cr);
}

static inline void
draw_inline_bars (cairo_t* cr, const float w, const float h,
		  const float thresdb, const float ratio,
		  const float peakdb, const float gainr,
		  const float level_in, const float level_out)
{
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_fill (cr);


	cairo_save (cr);

	const float ht = 0.25f * h;

	const float x1 = w*0.05;
	const float wd = w - 2.0f*x1;

	const float y1 = 0.17*h;
	const float y2 = h - y1 - ht;

	cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.5);

	cairo_rectangle (cr, x1, y1, wd, ht);
	cairo_fill (cr);

	cairo_rectangle (cr, x1, y2, wd, ht);
	cairo_fill (cr);

	cairo_set_source_rgba (cr, 0.75, 0.0, 0.0, 1.0);
	const float w_gr = (gainr > 60.f) ? wd : wd * gainr * (1.f/60.f);
	cairo_rectangle (cr, x1+wd-w_gr, y2, w_gr, ht);
	cairo_fill (cr);

	if (level_in > -60.f) {
		if (level_out > 6.f) {
			cairo_set_source_rgba (cr, 0.75, 0.0, 0.0, 1.0);
		} else if (level_out > 0.f) {
			cairo_set_source_rgba (cr, 0.66, 0.66, 0.0, 1.0);
		} else {
			cairo_set_source_rgba (cr, 0.0, 0.66, 0.0, 1.0);
		}
		const float w_g = (level_in > 10.f) ? wd : wd * (60.f+level_in) / 70.f;
		cairo_rectangle (cr, x1, y1, w_g, ht);
		cairo_fill (cr);
	}

	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);

	const float tck = 0.33*ht;

	cairo_set_line_width (cr, .5);

	for (uint32_t d = 1; d < 7; ++d) {
		const float x = x1 + (d * wd * (10.f / 70.f));

		cairo_move_to (cr, x, y1);
		cairo_line_to (cr, x, y1+tck);

		cairo_move_to (cr, x, y1+ht);
		cairo_line_to (cr, x, y1+ht-tck);

		cairo_move_to (cr, x, y2);
		cairo_line_to (cr, x, y2+tck);

		cairo_move_to (cr, x, y2+ht);
		cairo_line_to (cr, x, y2+ht-tck);
	}

	cairo_stroke (cr);

	const float x_0dB = x1 + wd*(60.f/70.f);

	cairo_move_to (cr, x_0dB, y1);
	cairo_line_to (cr, x_0dB, y1+ht);

	cairo_rectangle (cr, x1, y1, wd, ht);
	cairo_rectangle (cr, x1, y2, wd, ht);
	cairo_stroke (cr);

	cairo_set_line_width (cr, 2.0);

	// visualize threshold
	const float tr = x1 + wd * (60.f+thresdb) / 70.f;
	cairo_set_source_rgba (cr, 0.95, 0.95, 0.0, 1.0);
	cairo_move_to (cr, tr, y1);
	cairo_line_to (cr, tr, y1+ht);
	cairo_stroke (cr);

	// visualize ratio
	const float reduced_0dB = thresdb * (1.f - 1.f/ratio);
	const float rt = x1 + wd * (60.f+reduced_0dB) / 70.f;
	cairo_set_source_rgba (cr, 0.95, 0.0, 0.0, 1.0);
	cairo_move_to (cr, rt, y1);
	cairo_line_to (cr, rt, y1+ht);
	cairo_stroke (cr);

	// visualize in peak
	if (peakdb > -60.f) {
		cairo_set_source_rgba (cr, 0.0, 1.0, 0.0, 1.0);
		const float pk = (peakdb > 10.f) ? x1+wd : wd * (60.f+peakdb) / 70.f;
		cairo_move_to (cr, pk, y1);
		cairo_line_to (cr, pk, y1+ht);
		cairo_stroke (cr);
	}
}
