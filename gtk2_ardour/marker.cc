/*
    Copyright (C) 2001 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <sigc++/bind.h>
#include "ardour/tempo.h"

#include "canvas/rectangle.h"
#include "canvas/container.h"
#include "canvas/line.h"
#include "canvas/polygon.h"
#include "canvas/text.h"
#include "canvas/canvas.h"
#include "canvas/scroll_group.h"
#include "canvas/debug.h"

#include "ui_config.h"
/*
 * ardour_ui.h include was moved to the top of the list
 * due to a conflicting definition of 'Rect' between
 * Apple's MacTypes.h and GTK.
 *
 * Now that we are including ui_config.h and not ardour_ui.h
 * the above comment may no longer apply and this comment
 * can be removed and ui_config.h inclusion moved.
 */

#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "rgb_macros.h"

#include <gtkmm2ext/utils.h>

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtkmm2ext;

PBD::Signal1<void,ArdourMarker*> ArdourMarker::CatchDeletion;

static double marker_height = 13.0;

void ArdourMarker::setup_sizes(const double timebar_height)
{
	marker_height = floor (timebar_height) - 2;
}

ArdourMarker::ArdourMarker (PublicEditor& ed, ArdourCanvas::Container& parent, guint32 rgba, const string& annotation,
		Type type, samplepos_t sample, bool handle_events)

	: editor (ed)
	, _parent (&parent)
	, _track_canvas_line (0)
	, _type (type)
	, _selected (false)
	, _shown (false)
	, _line_shown (false)
	, _color (rgba)
	, _points_color (rgba)
	, _left_label_limit (DBL_MAX)
	, _right_label_limit (DBL_MAX)
	, _label_offset (0)

{

	const double MH = marker_height - .5;
	const double M3 = std::max(1.f, rintf(3.f * UIConfiguration::instance().get_ui_scale()));
	const double M6 = std::max(2.f, rintf(6.f * UIConfiguration::instance().get_ui_scale()));

	/* Shapes we use:
	 *
	 * Mark:
	 *
	 *  (0,0)   ->  (6,0)
	 *    ^           |
	 *    |           V
	 * (0,MH*.4)  (6,MH*.4)
	 *     \         /
	 *        (3,MH)
	 *
	 *
	 * TempoMark:
	 * MeterMark:
	 *
	 *        (3,0)
	 *     /         \
	 * (0,MH*.6)  (6,MH.*.6)
	 *    ^           |
	 *    |           V
	 * (0,MH)   <-  (6,MH)
	 *
	 *
	 * SessionStart:
	 * RangeStart:
	 *
	 *       0,0\
	 *        |  \
	 *        |   \ 6,MH/2
	 *        |   /
	 *        |  /
	 *       0,MH
	 *
	 *
	 * SessionEnd:
	 * RangeEnd:
	 *
	 *         /12,0
	 *        /   |
	 * 6,MH/2/    |
	 *       \    |
	 *        \   |
	 *         \12,MH
	 *
	 *
	 * PunchIn:
	 *
	 *   0,0 ------> marker_height,0
	 *    |       /
	 *    |      /
	 *    |     /
	 *    |    /
	 *    |   /
	 *    |  /
	 *   0,MH
	 *
	 *
	 *   PunchOut
	 *
	 *   0,0 ------> MH,0
	 *    \        |
	 *     \       |
	 *      \      |
	 *       \     |
	 *        \    |
	 *         \   |
	 *          \  |
	 *   MH,MH
	 *
	 */

	switch (type) {
	case Mark:
		points = new ArdourCanvas::Points ();

		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, MH * .4));
		points->push_back (ArdourCanvas::Duple ( M3, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH * .4));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 3;
		_label_offset = 10.0;
		break;

	case Tempo:
	case Meter:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple ( M3, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, MH * .6));
		points->push_back (ArdourCanvas::Duple ( M6, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH * .6));
		points->push_back (ArdourCanvas::Duple ( M3, 0.0));

		_shift = 3;
		_label_offset = 8.0;
		break;

	case SessionStart:
	case RangeStart:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (    0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (M6 + .5, MH * .5));
		points->push_back (ArdourCanvas::Duple (    0.0, MH));
		points->push_back (ArdourCanvas::Duple (    0.0, 0.0));

		_shift = 0;
		_label_offset = 8.0;
		break;

	case SessionEnd:
	case RangeEnd:
		points = new ArdourCanvas::Points (); // leaks
		points->push_back (ArdourCanvas::Duple ( M6, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH * .5));
		points->push_back (ArdourCanvas::Duple ( M6, 0.0));

		_shift = M6;
		_label_offset = 0.0;
		break;

	case LoopStart:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (MH, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 0;
		_label_offset = MH;
		break;

	case LoopEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (MH,  0.0));
		points->push_back (ArdourCanvas::Duple (MH, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH));
		points->push_back (ArdourCanvas::Duple (MH, 0.0));

		_shift = MH;
		_label_offset = 0.0;
		break;

	case PunchIn:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (MH, 0.0));
		points->push_back (ArdourCanvas::Duple (0.0, MH));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 0;
		_label_offset = MH;
		break;

	case PunchOut:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (MH, 0.0));
		points->push_back (ArdourCanvas::Duple (MH, MH));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = MH;
		_label_offset = 0.0;
		break;

	}

	sample_position = sample;
	unit_position = editor.sample_to_pixel (sample);
	unit_position -= _shift;

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (unit_position, 1));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("Marker::group for %1", annotation);
#endif

	_name_background = new ArdourCanvas::Rectangle (group);
#ifdef CANVAS_DEBUG
	_name_background->name = string_compose ("Marker::_name_background for %1", annotation);
#endif

	/* adjust to properly locate the tip */

	mark = new ArdourCanvas::Polygon (group);
	CANVAS_DEBUG_NAME (mark, string_compose ("Marker::mark for %1", annotation));

	mark->set (*points);
	set_color_rgba (rgba);

	/* setup name pixbuf sizes */
	name_font = get_font_for_style (N_("MarkerText"));

	Gtk::Label foo;

	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
	int width;

	layout->set_font_description (name_font);
	Gtkmm2ext::get_ink_pixel_size (layout, width, name_height);

	_name_item = new ArdourCanvas::Text (group);
	CANVAS_DEBUG_NAME (_name_item, string_compose ("ArdourMarker::_name_item for %1", annotation));
	_name_item->set_font_description (name_font);
	_name_item->set_color (RGBA_TO_UINT (0,0,0,255));
	_name_item->set_position (ArdourCanvas::Duple (_label_offset, (marker_height - name_height - 1) * .5 ));

	set_name (annotation.c_str());

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &ArdourMarker::reposition));

	/* events will be handled by both the group and the mark itself, so
	 * make sure they can both be used to lookup this object.
	 */

	group->set_data ("marker", this);
	mark->set_data ("marker", this);

	if (handle_events) {
		group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}
}

ArdourMarker::~ArdourMarker ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete group;
	delete _track_canvas_line;
	delete points;
}

void ArdourMarker::reparent(ArdourCanvas::Container & parent)
{
	group->reparent (&parent);
	_parent = &parent;
}

void
ArdourMarker::set_selected (bool s)
{
	_selected = s;
	setup_line ();
}

void
ArdourMarker::set_show_line (bool s)
{
	_line_shown = s;
	setup_line ();
}

void
ArdourMarker::setup_line ()
{
	if (_shown && (_selected || _line_shown)) {

		if (_track_canvas_line == 0) {

			_track_canvas_line = new ArdourCanvas::Line (editor.get_hscroll_group());
			_track_canvas_line->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
		}

		ArdourCanvas::Duple g = group->canvas_origin();
		ArdourCanvas::Duple d = _track_canvas_line->canvas_to_item (ArdourCanvas::Duple (g.x + _shift, 0));

		_track_canvas_line->set_x0 (d.x);
		_track_canvas_line->set_x1 (d.x);
		_track_canvas_line->set_y0 (d.y);
		_track_canvas_line->set_y1 (ArdourCanvas::COORD_MAX);
		_track_canvas_line->set_outline_color ( _selected ? UIConfiguration::instance().color ("entered marker") : _color );
		_track_canvas_line->raise_to_top ();
		_track_canvas_line->show ();

	} else {
		if (_track_canvas_line) {
			_track_canvas_line->hide ();
		}
	}
}

void
ArdourMarker::canvas_height_set (double h)
{
	_canvas_height = h;
	setup_line ();
}

ArdourCanvas::Item&
ArdourMarker::the_item() const
{
	return *group;
}

void
ArdourMarker::set_name (const string& new_name)
{
	_name = new_name;

	mark->set_tooltip(new_name);
	_name_background->set_tooltip(new_name);
	_name_item->set_tooltip(new_name);

	setup_name_display ();
}

/** @return true if our label is on the left of the mark, otherwise false */
bool
ArdourMarker::label_on_left () const
{
	return (_type == SessionEnd || _type == RangeEnd || _type == LoopEnd || _type == PunchOut);
}

void
ArdourMarker::setup_name_display ()
{
	double limit = DBL_MAX;

	if (label_on_left ()) {
		limit = _left_label_limit;
	} else {
		limit = _right_label_limit;
	}

	const float padding =  std::max(2.f, rintf(2.f * UIConfiguration::instance().get_ui_scale()));
	const double M3 = std::max(1.f, rintf(3.f * UIConfiguration::instance().get_ui_scale()));

	/* Work out how wide the name can be */
	int name_width = min ((double) pixel_width (_name, name_font) + padding, limit);

	if (name_width == 0) {
		_name_item->hide ();
	} else {
		_name_item->show ();

		if (label_on_left ()) {
			_name_item->set_x_position (-name_width);
		}

		_name_item->clamp_width (name_width);
		_name_item->set (_name);

		if (label_on_left ()) {
			/* adjust right edge of background to fit text */
			_name_background->set_x0 (_name_item->position().x - padding);
			_name_background->set_x1 (_name_item->position().x + name_width + _shift);
		} else {
			/* right edge remains at zero (group-relative). Add
			 * arbitrary 2 pixels of extra padding at the end
			 */
			switch (_type) {
				case Tempo:
					_name_item->hide ();
					// tip's x-pos is at "M3", box is 2x marker's
					_name_background->set_x0 (-M3);
					_name_background->set_x1 (3 * M3);
					break;
				case Mark:
				case Meter:
					_name_background->set_x0 (M3);
					_name_background->set_x1 (_name_item->position().x + name_width + padding);
					break;
				default:
					_name_background->set_x0 (0);
					_name_background->set_x1 (_name_item->position().x + name_width + padding);
					break;
			}
		}
	}

	_name_background->set_y0 (0);
	_name_background->set_y1 (marker_height + 1);
}

void
ArdourMarker::set_position (samplepos_t sample)
{
	unit_position = editor.sample_to_pixel (sample) - _shift;
	group->set_x_position (unit_position);
	setup_line ();
	sample_position = sample;
}

void
ArdourMarker::reposition ()
{
	set_position (sample_position);
}

void
ArdourMarker::show ()
{
	_shown = true;

	group->show ();
	setup_line ();
}

void
ArdourMarker::hide ()
{
	_shown = false;

	group->hide ();
	setup_line ();
}

void
ArdourMarker::set_points_color (uint32_t c)
{
	_points_color = c;
	mark->set_fill_color (_points_color);
	mark->set_outline_color (_points_color);
}

void
ArdourMarker::set_color_rgba (uint32_t c)
{
	_color = c;
	mark->set_fill_color (_color);
	mark->set_outline_color (_color);

	if (_track_canvas_line && !_selected) {
		_track_canvas_line->set_outline_color (_color);
	}

	_name_background->set_fill (true);
	_name_background->set_fill_color (UINT_RGBA_CHANGE_A (_color, 0x70));
	_name_background->set_outline (false);
}

/** Set the number of pixels that are available for a label to the left of the centre of this marker */
void
ArdourMarker::set_left_label_limit (double p)
{
	/* Account for the size of the marker */
	_left_label_limit = p - marker_height;
	if (_left_label_limit < 0) {
		_left_label_limit = 0;
	}

	if (label_on_left ()) {
		setup_name_display ();
	}
}

/** Set the number of pixels that are available for a label to the right of the centre of this marker */
void
ArdourMarker::set_right_label_limit (double p)
{
	/* Account for the size of the marker */
	_right_label_limit = p - marker_height;
	if (_right_label_limit < 0) {
		_right_label_limit = 0;
	}

	if (!label_on_left ()) {
		setup_name_display ();
	}
}

/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, ArdourCanvas::Container& parent, guint32 rgba, const string& text,
			  ARDOUR::TempoSection& temp)
	: ArdourMarker (editor, parent, rgba, text, Tempo, temp.sample(), false),
	  _tempo (temp)
{
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_marker_event), group, this));
}

TempoMarker::~TempoMarker ()
{
}

void
TempoMarker::update_height_mark (const double ratio)
{
	const double MH = marker_height - .5;
	const double top = MH * (1 - ratio);
	const double M3 = std::max(1.f, rintf(3.f * UIConfiguration::instance().get_ui_scale()));
	const double M6 = std::max(2.f, rintf(6.f * UIConfiguration::instance().get_ui_scale()));

	delete points;
	points = new ArdourCanvas::Points ();
	points->push_back (ArdourCanvas::Duple ( M3, top));
	points->push_back (ArdourCanvas::Duple ( M6, min (top + (MH * .6), MH)));
	points->push_back (ArdourCanvas::Duple ( M6, MH));
	points->push_back (ArdourCanvas::Duple (0.0, MH));
	points->push_back (ArdourCanvas::Duple (0.0, min (top + (MH * .6), MH)));
	points->push_back (ArdourCanvas::Duple ( M3, top));

	mark->set (*points);
}

/***********************************************************************/

MeterMarker::MeterMarker (PublicEditor& editor, ArdourCanvas::Container& parent, guint32 rgba, const string& text,
			  ARDOUR::MeterSection& m)
	: ArdourMarker (editor, parent, rgba, text, Meter, m.sample(), false),
	  _meter (m)
{
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_meter_marker_event), group, this));
}

MeterMarker::~MeterMarker ()
{
}
