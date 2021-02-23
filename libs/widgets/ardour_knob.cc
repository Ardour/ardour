/*
 * Copyright (C) 2010 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2021 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include <pangomm/layout.h>

#include "pbd/compose.h"

#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_knob.h"
#include "widgets/ui_config.h"

#include "pbd/i18n.h"

using namespace ArdourWidgets;
using namespace PBD;

ArdourKnob::Element ArdourKnob::default_elements = ArdourKnob::Element (ArdourKnob::Arc);

ArdourKnob::ArdourKnob (Element e, Flags flags)
	: ArdourCtrlBase (flags)
	, _elements (e)
{
}

void
ArdourKnob::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj ();

	float width  = get_width ();
	float height = get_height ();

	const float scale             = std::min (width, height);
	const float pointer_thickness = 3.0 * (scale / 80); // if the knob is 80 pixels wide, we want a 3-pix line on it

	const float start_angle = ((180 - 65) * G_PI) / 180;
	const float end_angle   = ((360 + 65) * G_PI) / 180;

	float zero = 0;
	if (_flags & ArcToZero) {
		zero = _normal;
	}

	const float value_angle = start_angle + (_val * (end_angle - start_angle));
	const float zero_angle  = start_angle + (zero * (end_angle - start_angle));

	float value_x = cos (value_angle);
	float value_y = sin (value_angle);

	float xc = 0.5 + width / 2.0;
	float yc = 0.5 + height / 2.0;

	cairo_translate (cr, xc, yc);

	/* get the knob color from the theme */
	Gtkmm2ext::Color knob_color = UIConfigurationBase::instance ().color (string_compose ("%1", get_name ()));

	float center_radius = 0.48 * scale;
	float border_width  = 0.8;

	bool arc   = (_elements & Arc) == Arc;
	bool bevel = (_elements & Bevel) == Bevel;
	bool flat  = flat_buttons ();

	if (arc) {
		center_radius = scale * 0.33;

		float inner_progress_radius = scale * 0.38;
		float outer_progress_radius = scale * 0.48;
		float progress_width        = (outer_progress_radius - inner_progress_radius);
		float progress_radius       = inner_progress_radius + progress_width / 2.0;

		/* dark arc background */
		cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
		cairo_set_line_width (cr, progress_width);
		cairo_arc (cr, 0, 0, progress_radius, start_angle, end_angle);
		cairo_stroke (cr);

		/* look up the arc colors from the config */
		double red_start, green_start, blue_start, unused;
		double red_end, green_end, blue_end;

		Gtkmm2ext::Color arc_start_color = UIConfigurationBase::instance ().color (string_compose ("%1: arc start", get_name ()));
		Gtkmm2ext::color_to_rgba (arc_start_color, red_start, green_start, blue_start, unused);
		Gtkmm2ext::Color arc_end_color = UIConfigurationBase::instance ().color (string_compose ("%1: arc end", get_name ()));
		Gtkmm2ext::color_to_rgba (arc_end_color, red_end, green_end, blue_end, unused);

		/* vary the arc color over the travel of the knob */
		float       intensity     = fabsf (_val - zero) / std::max (zero, (1.f - zero));
		const float intensity_inv = 1.0 - intensity;

		float r = intensity_inv * red_end   + intensity * red_start;
		float g = intensity_inv * green_end + intensity * green_start;
		float b = intensity_inv * blue_end  + intensity * blue_start;

		/* draw the arc */
		cairo_set_source_rgb (cr, r, g, b);
		cairo_set_line_width (cr, progress_width);
		if (zero_angle > value_angle) {
			cairo_arc (cr, 0, 0, progress_radius, value_angle, zero_angle);
		} else {
			cairo_arc (cr, 0, 0, progress_radius, zero_angle, value_angle);
		}
		cairo_stroke (cr);

		/* shade the arc */
		if (!flat) {
			cairo_pattern_t* shade_pattern;
			shade_pattern = cairo_pattern_create_linear (0.0, -yc, 0.0, yc);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1, 1, 1, 0.15);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.5, 1, 1, 1, 0.0);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 1, 1, 1, 0.0);
			cairo_set_source (cr, shade_pattern);
			cairo_arc (cr, 0, 0, outer_progress_radius - 1, 0, 2.0 * G_PI);
			cairo_fill (cr);
			cairo_pattern_destroy (shade_pattern);
		}

#if 0 //black border
		const float start_angle_x = cos (start_angle);
		const float start_angle_y = sin (start_angle);
		const float end_angle_x = cos (end_angle);
		const float end_angle_y = sin (end_angle);

		cairo_set_source_rgb (cr, 0, 0, 0 );
		cairo_set_line_width (cr, border_width);
		cairo_move_to (cr, (outer_progress_radius * start_angle_x), (outer_progress_radius * start_angle_y));
		cairo_line_to (cr, (inner_progress_radius * start_angle_x), (inner_progress_radius * start_angle_y));
		cairo_stroke (cr);
		cairo_move_to (cr, (outer_progress_radius * end_angle_x), (outer_progress_radius * end_angle_y));
		cairo_line_to (cr, (inner_progress_radius * end_angle_x), (inner_progress_radius * end_angle_y));
		cairo_stroke (cr);
		cairo_arc (cr, 0, 0, outer_progress_radius, start_angle, end_angle);
		cairo_stroke (cr);
#endif
	}

	if (!flat) {
		/* knob shadow */
		cairo_save (cr);
		cairo_translate (cr, pointer_thickness + 1, pointer_thickness + 1);
		cairo_set_source_rgba (cr, 0, 0, 0, 0.1);
		cairo_arc (cr, 0, 0, center_radius - 1, 0, 2.0 * G_PI);
		cairo_fill (cr);
		cairo_restore (cr);

		/* inner circle */
		Gtkmm2ext::set_source_rgba (cr, knob_color);
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
		cairo_fill (cr);

		if (bevel) {
			/* knob gradient */
			cairo_pattern_t* shade_pattern;
			shade_pattern = cairo_pattern_create_linear (0.0, -yc, 0.0, yc);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1, 1, 1, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.2, 1, 1, 1, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.8, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 0, 0, 0, 0.2);
			cairo_set_source (cr, shade_pattern);
			cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
			cairo_fill (cr);
			cairo_pattern_destroy (shade_pattern);

			/* flat top over beveled edge */
			Gtkmm2ext::set_source_rgb_a (cr, knob_color, 0.5);
			cairo_arc (cr, 0, 0, center_radius - pointer_thickness, 0, 2.0 * G_PI);
			cairo_fill (cr);
		} else {
			/* radial gradient */
			cairo_pattern_t* shade_pattern;
			shade_pattern = cairo_pattern_create_radial (-center_radius, -center_radius, 1, -center_radius, -center_radius, center_radius * 2.5);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 1, 1, 1, 0.2);
			cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 0, 0, 0, 0.3);
			cairo_set_source (cr, shade_pattern);
			cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
			cairo_fill (cr);
			cairo_pattern_destroy (shade_pattern);
		}

	} else {
		/* inner circle */
		Gtkmm2ext::set_source_rgba (cr, knob_color);
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
		cairo_fill (cr);
	}

	/* black knob border */
	cairo_set_line_width (cr, border_width);
	cairo_set_source_rgba (cr, 0, 0, 0, 1);
	cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
	cairo_stroke (cr);

	/* line shadow */
	if (!flat) {
		cairo_save (cr);
		cairo_translate (cr, 1, 1);
		cairo_set_source_rgba (cr, 0, 0, 0, 0.3);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
		cairo_set_line_width (cr, pointer_thickness);
		cairo_move_to (cr, (center_radius * value_x), (center_radius * value_y));
		cairo_line_to (cr, ((center_radius * 0.4) * value_x), ((center_radius * 0.4) * value_y));
		cairo_stroke (cr);
		cairo_restore (cr);
	}

	/* line */
	cairo_set_source_rgba (cr, 1, 1, 1, 1);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_width (cr, pointer_thickness);
	cairo_move_to (cr, (center_radius * value_x), (center_radius * value_y));
	cairo_line_to (cr, ((center_radius * 0.4) * value_x), ((center_radius * 0.4) * value_y));
	cairo_stroke (cr);

	/* a transparent overlay to indicate insensitivity */
	if (!get_sensitive ()) {
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
		uint32_t ins_color = UIConfigurationBase::instance ().color ("gtk_background");
		Gtkmm2ext::set_source_rgb_a (cr, ins_color, 0.6);
		cairo_fill (cr);
	}

	/* highlight if grabbed or if mouse is hovering over me */
	if (_tooltip.dragging () || (_hovering && UIConfigurationBase::instance ().get_widget_prelight ())) {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.12);
		cairo_arc (cr, 0, 0, center_radius, 0, 2.0 * G_PI);
		cairo_fill (cr);
	}

	cairo_identity_matrix (cr);
}

void
ArdourKnob::on_size_request (Gtk::Requisition* req)
{
	req->width  = _req_width;
	req->height = _req_height;
	if (req->width < 1) {
		req->width = 13;
	}
	if (req->height < 1) {
		req->height = 13;
	}

	/* knob is square */
	req->width  = std::max (req->width, req->height);
	req->height = std::max (req->width, req->height);
}

void
ArdourKnob::set_elements (Element e)
{
	_elements = e;
}

void
ArdourKnob::add_elements (Element e)
{
	_elements = (ArdourKnob::Element) (_elements | e);
}
