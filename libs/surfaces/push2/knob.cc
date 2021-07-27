/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>

#include <cairomm/context.h>
#include <cairomm/pattern.h>

#include "ardour/automation_control.h"
#include "ardour/dB.h"
#include "ardour/utils.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "gtkmm2ext/colors.h"
#include "canvas/text.h"

#include "knob.h"
#include "push2.h"
#include "utils.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourCanvas;

Push2Knob::Element Push2Knob::default_elements = Push2Knob::Element (Push2Knob::Arc);

Push2Knob::Push2Knob (Push2& p, Item* parent, Element e, Flags flags)
	: Container (parent)
	, p2 (p)
	, _elements (e)
	, _flags (flags)
	, _r (0)
	, _val (0)
	, _normal (0)
{
	Pango::FontDescription fd ("Sans 10");

	text = new Text (this);
	text->set_font_description (fd);
	text->set_position (Duple (0, -20)); /* changed when radius changes */

	/* typically over-ridden */

	text_color = p2.get_color (Push2::ParameterName);
	arc_start_color = p2.get_color (Push2::KnobArcStart);
	arc_end_color = p2.get_color (Push2::KnobArcEnd);
}

Push2Knob::~Push2Knob ()
{
}

void
Push2Knob::set_text_color (Gtkmm2ext::Color c)
{
	text->set_color (c);
}

void
Push2Knob::set_radius (double r)
{
	_r = r;
	text->set_position (Duple (-_r, -_r - 20));
	_bounding_box_dirty = true;
	redraw ();
}

void
Push2Knob::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_controllable) {
		/* no controllable, nothing to draw */
		return;
	}

	const float scale = 2.0 * _r;
	const float pointer_thickness = 3.0 * (scale/80);  //(if the knob is 80 pixels wide, we want a 3-pix line on it)

	const float start_angle = ((180 - 65) * G_PI) / 180;
	const float end_angle = ((360 + 65) * G_PI) / 180;

	float zero = 0;

	if (_flags & ArcToZero) {
		zero = _normal;
	}

	const float value_angle = start_angle + (_val * (end_angle - start_angle));
	const float zero_angle = start_angle + (zero * (end_angle - start_angle));

	float value_x = cos (value_angle);
	float value_y = sin (value_angle);

	/* translate so that all coordinates are based on the center of the
	 * knob (which is also its position()
	 */
	Duple origin = item_to_window (Duple (0, 0));
	context->translate (origin.x, origin.y);
	context->begin_new_path ();

	float center_radius = 0.48*scale;
	float border_width = 0.8;

	const bool arc = (_elements & Arc)==Arc;
	const bool flat = false;

	if (arc) {
		center_radius = scale*0.33;

		float inner_progress_radius = scale*0.38;
		float outer_progress_radius = scale*0.48;
		float progress_width = (outer_progress_radius-inner_progress_radius);
		float progress_radius = inner_progress_radius + progress_width/2.0;

		//dark arc background
		set_source_rgb (context, p2.get_color (Push2::KnobArcBackground));
		context->set_line_width (progress_width);
		context->arc (0, 0, progress_radius, start_angle, end_angle);
		context->stroke ();

		double red_start, green_start, blue_start, astart;
		double red_end, green_end, blue_end, aend;

		Gtkmm2ext::color_to_rgba (arc_start_color, red_start, green_start, blue_start, astart);
		Gtkmm2ext::color_to_rgba (arc_end_color, red_end, green_end, blue_end, aend);

		//vary the arc color over the travel of the knob
		float intensity = fabsf (_val - zero) / std::max(zero, (1.f - zero));
		const float intensity_inv = 1.0 - intensity;
		float r = intensity_inv * red_end   + intensity * red_start;
		float g = intensity_inv * green_end + intensity * green_start;
		float b = intensity_inv * blue_end  + intensity * blue_start;

		//draw the arc
		context->set_source_rgb (r,g,b);
		context->set_line_width (progress_width);
		if (zero_angle > value_angle) {
			context->arc (0, 0, progress_radius, value_angle, zero_angle);
		} else {
			context->arc (0, 0, progress_radius, zero_angle, value_angle);
		}
		context->stroke ();

		//shade the arc
		if (!flat) {
			//note we have to offset the pattern from our centerpoint
			Cairo::RefPtr<Cairo::LinearGradient> pattern = Cairo::LinearGradient::create (0.0, -_position.y, 0.0, _position.y);
			pattern->add_color_stop_rgba (0.0, 1,1,1, 0.15);
			pattern->add_color_stop_rgba (0.5, 1,1,1, 0.0);
			pattern->add_color_stop_rgba (1.0, 1,1,1, 0.0);
			context->set_source (pattern);
			context->arc (0, 0, outer_progress_radius-1, 0, 2.0*G_PI);
			context->fill ();
		}
	}

	if (!flat) {
		//knob shadow
		context->save();
		context->translate(pointer_thickness+1, pointer_thickness+1 );
		set_source_rgba (context, p2.get_color (Push2::KnobShadow));
		context->arc (0, 0, center_radius-1, 0, 2.0*G_PI);
		context->fill ();
		context->restore();

		//inner circle
		set_source_rgb (context, p2.get_color (Push2::KnobForeground));
		context->arc (0, 0, center_radius, 0, 2.0*G_PI);
		context->fill ();

		//radial gradient as a lightness shade
		Cairo::RefPtr<Cairo::RadialGradient> pattern = Cairo::RadialGradient::create (-center_radius, -center_radius, 1, -center_radius, -center_radius, center_radius*2.5  );  //note we have to offset the gradient from our centerpoint
		pattern->add_color_stop_rgba (0.0, 0, 0, 0, 0.2);
		pattern->add_color_stop_rgba (1.0, 1, 1, 1, 0.3);
		context->set_source (pattern);
		context->arc (0, 0, center_radius, 0, 2.0*G_PI);
		context->fill ();

	}

	//black knob border
	context->set_line_width (border_width);
	set_source_rgba (context, p2.get_color (Push2::KnobBorder));
	context->set_source_rgba (0, 0, 0, 1 );
	context->arc (0, 0, center_radius, 0, 2.0*G_PI);
	context->stroke ();

	//line shadow
	if (!flat) {
		context->save();
		context->translate(1, 1 );
		set_source_rgba (context, p2.get_color (Push2::KnobLineShadow));
		context->set_line_cap (Cairo::LINE_CAP_ROUND);
		context->set_line_width (pointer_thickness);
		context->move_to ((center_radius * value_x), (center_radius * value_y));
		context->line_to (((center_radius*0.4) * value_x), ((center_radius*0.4) * value_y));
		context->stroke ();
		context->restore();
	}

	//line
	set_source_rgba (context, p2.get_color (Push2::KnobLine));
	context->set_line_cap (Cairo::LINE_CAP_ROUND);
	context->set_line_width (pointer_thickness);
	context->move_to ((center_radius * value_x), (center_radius * value_y));
	context->line_to (((center_radius*0.4) * value_x), ((center_radius*0.4) * value_y));
	context->stroke ();

	/* reset all translations, scaling etc. */
	context->set_identity_matrix();

	render_children (area, context);
}

 void
Push2Knob::compute_bounding_box () const
{
	if (!_canvas || _r == 0) {
		_bounding_box = Rect ();
		_bounding_box_dirty = false;
		return;
	}

	if (_bounding_box_dirty) {
		Rect r = Rect (_position.x - _r, _position.y - _r, _position.x + _r, _position.y + _r);
		_bounding_box = r;
		_bounding_box_dirty = false;
	}

	/* Item::bounding_box() will add children */
}

void
Push2Knob::set_controllable (boost::shared_ptr<AutomationControl> c)
{
	watch_connection.disconnect ();  //stop watching the old controllable

	if (!c) {
		_controllable.reset ();
		return;
	}

	_controllable = c;
	_controllable->Changed.connect (watch_connection, invalidator(*this), boost::bind (&Push2Knob::controllable_changed, this), &p2);

	controllable_changed ();
}

void
Push2Knob::set_pan_azimuth_text (double pos)
{
	/* We show the position of the center of the image relative to the left & right.
	   This is expressed as a pair of percentage values that ranges from (100,0)
	   (hard left) through (50,50) (hard center) to (0,100) (hard right).

	   This is pretty wierd, but its the way audio engineers expect it. Just remember that
	   the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
	*/

	char buf[64];
	snprintf (buf, sizeof (buf), _("L:%3d R:%3d"), (int) rint (100.0 * (1.0 - pos)), (int) rint (100.0 * pos));
	text->set (buf);
}

void
Push2Knob::set_pan_width_text (double val)
{
	char buf[16];
	snprintf (buf, sizeof (buf), "%d%%", (int) floor (val*100));
	text->set (buf);
}

void
Push2Knob::set_gain_text (double)
{
	char buf[16];

	/* need to ignore argument, because it has already been converted into
	   the "interface" (0..1) range.
	*/

	snprintf (buf, sizeof (buf), "%.1f dB", accurate_coefficient_to_dB (_controllable->get_value()));
	text->set (buf);
}

void
Push2Knob::controllable_changed ()
{
	if (_controllable) {
		_normal = _controllable->internal_to_interface (_controllable->normal());
		_val = _controllable->internal_to_interface (_controllable->get_value());

		switch (_controllable->parameter().type()) {
		case ARDOUR::PanAzimuthAutomation:
			set_pan_azimuth_text (_val);
			break;

		case ARDOUR::PanWidthAutomation:
			set_pan_width_text (_val);
			break;

		case ARDOUR::GainAutomation:
		case ARDOUR::BusSendLevel:
		case ARDOUR::TrimAutomation:
			set_gain_text (_val);
			break;

		default:
			text->set (std::string());
		}
	}

	redraw ();
}

void
Push2Knob::add_flag (Flags f)
{
	_flags = Flags (_flags | f);
	redraw ();
}

void
Push2Knob::remove_flag (Flags f)
{
	_flags = Flags (_flags & ~f);
	redraw ();
}

void
Push2Knob::set_arc_start_color (uint32_t c)
{
	arc_start_color = c;
	redraw ();
}

void
Push2Knob::set_arc_end_color (uint32_t c)
{
	arc_end_color = c;
	redraw ();
}
