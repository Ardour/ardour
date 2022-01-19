/*
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <sigc++/bind.h>

#include "temporal/tempo.h"

#include "canvas/rectangle.h"
#include "canvas/container.h"
#include "canvas/curve.h"
#include "canvas/canvas.h"
#include "canvas/debug.h"

#include "ui_config.h"

#include "tempo_curve.h"
#include "public_editor.h"
#include "utils.h"
#include "rgb_macros.h"

#include <gtkmm2ext/utils.h>

#include "pbd/i18n.h"

using namespace Temporal;

PBD::Signal1<void,TempoCurve*> TempoCurve::CatchDeletion;

static double curve_height = 13.0;

void
TempoCurve::setup_sizes(const double timebar_height)
{
	const double ui_scale  = UIConfiguration::instance ().get_ui_scale ();
	curve_height = floor (timebar_height) - (2.5 * ui_scale);
}

/* ignores Tempo note type - only note_types_per_minute is potentially curved */
TempoCurve::TempoCurve (PublicEditor& ed, ArdourCanvas::Item& parent, guint32 rgba, TempoPoint const & temp, bool handle_events, ArdourCanvas::Distance marker_width)

	: editor (ed)
	, _parent (&parent)
	, _curve (0)
	, _duration (UINT32_MAX)
	, _marker_width (marker_width)
	, _color (rgba)
	, _min_tempo (temp.note_types_per_minute())
	, _max_tempo (temp.note_types_per_minute())
	, _tempo (temp)
	, _start_text (0)
	, _end_text (0)
{
	/* XXX x arg for Duple should probably be marker width, passed in from owner */
	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (marker_width, 1));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("TempoCurve::group for %1", _tempo.note_types_per_minute());
#endif

	_curve = new ArdourCanvas::FramedCurve (group);
#ifdef CANVAS_DEBUG
	_curve->name = string_compose ("TempoCurve::curve for %1", _tempo.note_types_per_minute());
#endif
	_curve->set_points_per_segment (3);
	_curve->set (points);

	_start_text = new ArdourCanvas::Text (group);
	_end_text = new ArdourCanvas::Text (group);
	_start_text->set_font_description (ARDOUR_UI_UTILS::get_font_for_style (N_("MarkerText")));
	_end_text->set_font_description (ARDOUR_UI_UTILS::get_font_for_style (N_("MarkerText")));
	_start_text->set_color (RGBA_TO_UINT (255,255,255,255));
	_end_text->set_color (RGBA_TO_UINT (255,255,255,255));
	char buf[10];
	snprintf (buf, sizeof (buf), "%.3f/%d", _tempo.note_types_per_minute(), _tempo.note_type());
	_start_text->set (buf);
	snprintf (buf, sizeof (buf), "%.3f", _tempo.end_note_types_per_minute());
	_end_text->set (buf);

	set_color_rgba (rgba);

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &TempoCurve::reposition));

	/* events will be handled by both the group and the mark itself, so
	 * make sure they can both be used to lookup this object.
	 */

	_curve->set_data ("tempo curve", this);

	if (handle_events) {
		//group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}

	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_curve_event), _curve, this));

}

TempoCurve::~TempoCurve ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete group;
}

ArdourCanvas::Item&
TempoCurve::the_item() const
{
	return *group;
}

void
TempoCurve::set_duration (samplecnt_t duration)
{
	points.clear();
	points.push_back (ArdourCanvas::Duple (0.0, curve_height));

	ArdourCanvas::Coord duration_pixels = editor.sample_to_pixel (duration);

	if (!_tempo.ramped()) {

		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos =  (curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height);

		points.push_back (ArdourCanvas::Duple (0.0, y_pos));
		points.push_back (ArdourCanvas::Duple (duration_pixels, y_pos));

	} else {

		const samplepos_t sample_step = std::max ((duration) / 5, (samplepos_t) 1);
		samplepos_t current_sample = 0;

		while (current_sample < duration) {
			const double tempo_at = _tempo.note_types_per_minute_at_DOUBLE (timepos_t (current_sample));
			const double y_pos = std::max ((curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height), 0.0);

			points.push_back (ArdourCanvas::Duple (editor.sample_to_pixel (current_sample), std::min (y_pos, curve_height)));

			current_sample += sample_step;
		}

		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos = std::max ((curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height), 0.0);

		points.push_back (ArdourCanvas::Duple (duration_pixels, std::min (y_pos, curve_height)));
	}

	_curve->set (points);

	char buf[10];

	snprintf (buf, sizeof (buf), "%.3f/%d", _tempo.note_types_per_minute(), _tempo.note_type());
	_start_text->set (buf);
	snprintf (buf, sizeof (buf), "%.3f", _tempo.end_note_types_per_minute());
	_end_text->set (buf);

	const double ui_scale  = UIConfiguration::instance ().get_ui_scale ();

	_start_text->set_position (ArdourCanvas::Duple (_marker_width + (10 * ui_scale), (.5 * ui_scale)));
	_end_text->set_position (ArdourCanvas::Duple (duration_pixels - _end_text->text_width() - _marker_width - (10. * ui_scale), (.5 * ui_scale)));

	if (_end_text->text_width() + _start_text->text_width() + (20.0 * ui_scale) > duration_pixels) {
		_start_text->hide();
		_end_text->hide();
	} else {
		_start_text->show();
		_end_text->show();
	}

	_duration = duration;
}

void
TempoCurve::reposition ()
{
	set_duration (_duration);
}

void
TempoCurve::show ()
{
	group->show ();
}

void
TempoCurve::hide ()
{
	group->hide ();
}

void
TempoCurve::set_color_rgba (uint32_t c)
{
	_color = c;
	_curve->set_fill_color (UIConfiguration::instance().color_mod (_color, "selection rect"));
	_curve->set_outline_color (_color);

}
