/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include "ardour/automation_control.h"
#include "ardour/value_as_string.h"
#include "ardour/dB.h"
#include "ardour/utils.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "canvas/text.h"

#include "maschine2.h"
#include "m2controls.h"

#include "ui_knob.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourCanvas;

Maschine2Knob::Maschine2Knob (PBD::EventLoop* el, Item* parent)
	: Container (parent)
	, _ctrl (0)
	, _eventloop (el)
	, _radius (11)
	, _val (0)
	, _normal (0)
{
	Pango::FontDescription fd ("Sans 10px");

	text = new Text (this);
	text->set_font_description (fd);
	text->set_position (Duple (-_radius, _radius + 2));
	text->set_color (0xffffffff);
	_bounding_box_dirty = true;
}

Maschine2Knob::~Maschine2Knob ()
{
}

void
Maschine2Knob::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_controllable) {
		/* no controllable, nothing to draw */
		return;
	}

	// TODO consider a "bar" circular shape + 1bit b/w is ugly

	const float scale = 2.f * _radius;
	const float pointer_thickness = std::max (1.f, 3.f * (scale / 80.f));

	const float start_angle = ((180.f - 65.f) * M_PI) / 180.f;
	const float end_angle = ((360.f + 65.f) * M_PI) / 180.f;

	float zero = 0;

	const float value_angle = start_angle + (_val * (end_angle - start_angle));
	const float zero_angle = start_angle + (zero * (end_angle - start_angle));

	float value_x = cos (value_angle);
	float value_y = sin (value_angle);

	/* translate so that all coordinates are based on the center of the
	 * knob (which is also its position()
	 */
	context->save ();
	Duple origin = item_to_window (Duple (0, 0));
	context->translate (origin.x - 0.5, origin.y - 0.5);
	context->begin_new_path ();

	float center_radius = 0.48*scale;
	float border_width = 0.8;

	const bool arc = true;

	if (arc) {
		center_radius = scale * 0.33;

		float inner_progress_radius = scale * 0.38;
		float outer_progress_radius = scale * 0.48;
		float progress_width = (outer_progress_radius-inner_progress_radius);
		float progress_radius = inner_progress_radius + progress_width/2.0;

		// draw the arc
		context->set_source_rgb (1, 1, 1);
		context->set_line_width (progress_width);
		if (zero_angle > value_angle) {
			context->arc (0, 0, progress_radius, value_angle, zero_angle);
		} else {
			context->arc (0, 0, progress_radius, zero_angle, value_angle);
		}
		context->stroke ();
	}

	// knob body
	context->set_line_width (border_width);
	context->set_source_rgb (1, 1, 1);
	context->arc (0, 0, center_radius, 0, 2.0*G_PI);
	context->fill ();

	// line
	context->set_source_rgb (0, 0, 0);
	context->set_line_cap (Cairo::LINE_CAP_ROUND);
	context->set_line_width (pointer_thickness);
	context->move_to ((center_radius * value_x), (center_radius * value_y));
	context->line_to (((center_radius * 0.2) * value_x), ((center_radius * 0.2) * value_y));
	context->stroke ();

	/* reset all translations, scaling etc. */
	context->restore ();

	render_children (area, context);
}

 void
Maschine2Knob::compute_bounding_box () const
{
	if (!_canvas || _radius == 0) {
		_bounding_box = Rect ();
		_bounding_box_dirty = false;
		return;
	}

	if (_bounding_box_dirty) {
		_bounding_box = Rect (- _radius, - _radius, _radius, _radius);
		_bounding_box_dirty = false;
	}

	/* Item::bounding_box() will add children */
}

void
Maschine2Knob::set_controllable (boost::shared_ptr<AutomationControl> c)
{
	watch_connection.disconnect ();

	if (!c) {
		_controllable.reset ();
		return;
	}

	_controllable = c;
	_controllable->Changed.connect (watch_connection, invalidator(*this), boost::bind (&Maschine2Knob::controllable_changed, this), _eventloop);

	controllable_changed ();
	// set _controllable->desc()->label
}

void
Maschine2Knob::set_control (M2EncoderInterface* ctrl)
{
	encoder_connection.disconnect ();
	_ctrl = ctrl;
	if (!ctrl) {
		return;
	}
	ctrl->changed.connect_same_thread (encoder_connection, boost::bind (&Maschine2Knob::encoder_changed, this, _1));
}

void
Maschine2Knob::encoder_changed (int delta)
{
	if (!_controllable) {
		return;
	}
	const double d = delta * 0.5 / _ctrl->range ();
	boost::shared_ptr<AutomationControl> ac = _controllable;
	ac->set_value (ac->interface_to_internal (std::min (ac->upper(), std::max (ac->lower(), ac->internal_to_interface (ac->get_value()) + d))), PBD::Controllable::UseGroup);
}

void
Maschine2Knob::controllable_changed ()
{
	if (_controllable) {
		_normal = _controllable->internal_to_interface (_controllable->normal());
		_val = _controllable->internal_to_interface (_controllable->get_value());

		const ParameterDescriptor& desc (_controllable->desc());

		char buf[64];
		switch (_controllable->parameter().type()) {
			case ARDOUR::PanAzimuthAutomation:
				snprintf (buf, sizeof (buf), _("L:%3d R:%3d"), (int) rint (100.0 * (1.0 - _val)), (int) rint (100.0 * _val));
				text->set (buf);
				break;

			case ARDOUR::PanWidthAutomation:
				snprintf (buf, sizeof (buf), "%d%%", (int) floor (_val*100));
				text->set (buf);
				break;

			case ARDOUR::GainAutomation:
			case ARDOUR::BusSendLevel:
			case ARDOUR::TrimAutomation:
				snprintf (buf, sizeof (buf), "%+4.1f dB", accurate_coefficient_to_dB (_controllable->get_value()));
				text->set (buf);
				break;

			default:
				text->set (ARDOUR::value_as_string (desc, _val));
				break;
		}
	} else {
		text->set ("---");
	}

	redraw ();
}
