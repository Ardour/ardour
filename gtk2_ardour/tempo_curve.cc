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

void TempoCurve::setup_sizes(const double timebar_height)
{
	curve_height = floor (timebar_height) - 2.5;
}
/* ignores Tempo note type - only note_types_per_minute is potentially curved */
TempoCurve::TempoCurve (PublicEditor& ed, ArdourCanvas::Container& parent, guint32 rgba, TempoPoint& temp, samplepos_t sample, bool handle_events)

	: editor (ed)
	, _parent (&parent)
	, _curve (0)
	, _shown (false)
	, _color (rgba)
	, _min_tempo (temp.note_types_per_minute())
	, _max_tempo (temp.note_types_per_minute())
	, _tempo (temp)
	, _start_text (0)
	, _end_text (0)
{
	sample_position = sample;
	unit_position = editor.sample_to_pixel (sample);

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (unit_position, 1));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("TempoCurve::group for %1", _tempo.note_types_per_minute());
#endif

	_curve = new ArdourCanvas::FramedCurve (group);
#ifdef CANVAS_DEBUG
	_curve->name = string_compose ("TempoCurve::curve for %1", _tempo.note_types_per_minute());
#endif
	_curve->set_points_per_segment (3);
	points = new ArdourCanvas::Points ();
	_curve->set (*points);

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

void TempoCurve::reparent(ArdourCanvas::Container & parent)
{
	group->reparent (&parent);
	_parent = &parent;
}

void
TempoCurve::canvas_height_set (double h)
{
	_canvas_height = h;
}

ArdourCanvas::Item&
TempoCurve::the_item() const
{
	return *group;
}

void
TempoCurve::set_position (samplepos_t sample, samplepos_t end_sample)
{
	unit_position = editor.sample_to_pixel (sample);
	group->set_x_position (unit_position);
	sample_position = sample;
	_end_sample = end_sample;

	points->clear();
	points = new ArdourCanvas::Points ();

	points->push_back (ArdourCanvas::Duple (0.0, curve_height));

	if (sample >= end_sample) {
		/* shouldn't happen but ..*/
		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos =  (curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height);

		points->push_back (ArdourCanvas::Duple (0.0, y_pos));
		points->push_back (ArdourCanvas::Duple (1.0, y_pos));

	} else if (!_tempo.ramped()) {
		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos =  (curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height);

		points->push_back (ArdourCanvas::Duple (0.0, y_pos));
		points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (end_sample - sample), y_pos));
	} else {

		const samplepos_t sample_step = std::max ((end_sample - sample) / 5, (samplepos_t) 1);
		samplepos_t current_sample = sample;

		while (current_sample < end_sample) {
			const double tempo_at = _tempo.note_types_per_minute_at_DOUBLE (timepos_t (current_sample));
			const double y_pos = std::max ((curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height), 0.0);

			points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (current_sample - sample), std::min (y_pos, curve_height)));

			current_sample += sample_step;
		}

		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos = std::max ((curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height), 0.0);

		points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (end_sample - sample), std::min (y_pos, curve_height)));
	}

	_curve->set (*points);

	char buf[10];
	snprintf (buf, sizeof (buf), "%.3f/%.0f", _tempo.note_types_per_minute(), _tempo.note_type());
	_start_text->set (buf);
	snprintf (buf, sizeof (buf), "%.3f", _tempo.end_note_types_per_minute());
	_end_text->set (buf);

	_start_text->set_position (ArdourCanvas::Duple (10, .5 ));
	_end_text->set_position (ArdourCanvas::Duple (editor.sample_to_pixel (end_sample - sample) - _end_text->text_width() - 10, .5 ));

	if (_end_text->text_width() + _start_text->text_width() + 20 > editor.sample_to_pixel (end_sample - sample)) {
		_start_text->hide();
		_end_text->hide();
	} else {
		_start_text->show();
		_end_text->show();
	}
}

void
TempoCurve::reposition ()
{
	set_position (sample_position, _end_sample);
}

void
TempoCurve::show ()
{
	_shown = true;

	group->show ();
}

void
TempoCurve::hide ()
{
	_shown = false;

	group->hide ();
}

void
TempoCurve::set_color_rgba (uint32_t c)
{
	_color = c;
	_curve->set_fill_color (UIConfiguration::instance().color_mod (_color, "selection rect"));
	_curve->set_outline_color (_color);

}
