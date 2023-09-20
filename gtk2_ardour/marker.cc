/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <sstream>

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
#include "tempo_curve.h"

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

uint32_t
ArdourMarker::color (std::string const& color_name)
{
	const Gtkmm2ext::SVAModifier alpha = UIConfiguration::instance().modifier (color_name);
	return Gtkmm2ext::HSV (UIConfiguration::instance().color (color_name)).mod (alpha).color();
}


ArdourMarker::ArdourMarker (PublicEditor& ed, ArdourCanvas::Item& parent, std::string const& color_name, string const& annotation,
                            Type type, timepos_t const & pos, bool handle_events, RegionView* rv, bool use_tooltip)

	: editor (ed)
	, _parent (&parent)
	, _track_canvas_line (0)
	, _type (type)
	, _selected (false)
	, _entered (false)
	, _shown (false)
	, _line_shown (false)
	, _use_tooltip (use_tooltip)
	, _color (color_name)
	, _points_color (color_name)
	, _left_label_limit (DBL_MAX)
	, _right_label_limit (DBL_MAX)
	, _label_offset (0)
	, _line_height (-1)
	, _region_view (rv)
	, _cue_index (-1)
{
	const double scale = UIConfiguration::instance ().get_ui_scale ();

	const double MH = marker_height - .5;
	const double M3 = std::max(1.f, rintf(3.f * scale));
	const double M6 = std::max(2.f, rintf(6.f * scale));

	const double M5 =  std::max(1.f, rintf(5.f * scale));
	const double M10 = std::max(2.f, rintf(10.f * scale));

	/* Shapes we use:
	 *
	 * Mark:
	 * RegionCue:
	 * BBTPosition
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
	 * PunchIn:
	 * LoopStart:
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
	 * PunchOut:
	 * LoopEnd:
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
	 * SelectionStart:
	 *
	 *   0,0 ------> MH,0
	 *    |       /
	 *    |      /
	 *    |     /
	 *    |    /
	 *    |   /
	 *    |  /
	 *   0,MH
	 *
	 *
	 * SelectionEnd:
	 *
	 * -MH,0 ------> 0,0
	 *    \        |
	 *     \       |
	 *      \      |
	 *       \     |
	 *        \    |
	 *         \   |
	 *          \  |
	 *           0,MH
	 *
	 * Cue:
	 *  ben: put your shape here :)
	 */

	switch (_type) {
	case Mark: /* fallthrough */
	case RegionCue: /* fallthrough */
	case BBTPosition:
		points = new ArdourCanvas::Points ();

		points->push_back (ArdourCanvas::Duple (0.0, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, MH * .4));
		points->push_back (ArdourCanvas::Duple ( M3, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH * .4));
		points->push_back (ArdourCanvas::Duple (0.0, 0.0));

		_shift = 3 * scale;
		_label_offset = 8.0 * scale;
		break;

	case Tempo: /* fallthrough */
	case Meter:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple ( M5, 0.0));
		points->push_back (ArdourCanvas::Duple ( M10, MH * .6));
		points->push_back (ArdourCanvas::Duple ( M10, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH * .6));
		points->push_back (ArdourCanvas::Duple ( M5, 0.0));

		_shift = 5 * scale;
		_label_offset = 12.0 * scale;
		break;

	case PunchIn: /* fallthrough */
	case LoopStart: /* fallthrough */
	case SessionStart: /* fallthrough */
	case RangeStart:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (    0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (M6 + .5, MH * .5));
		points->push_back (ArdourCanvas::Duple (    0.0, MH));
		points->push_back (ArdourCanvas::Duple (    0.0, 0.0));

		_shift = 0 * scale;
		_label_offset = 8.0 * scale;
		break;

	case Section:
		points = new ArdourCanvas::Points (); //unused
		points->push_back (ArdourCanvas::Duple (    0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (M6 + .5, MH * .5));
		points->push_back (ArdourCanvas::Duple (    0.0, MH));
		points->push_back (ArdourCanvas::Duple (    0.0, 0.0));

		_shift = 0 * scale;
		_label_offset = 4.0 * scale;
		break;

	case PunchOut: /* fallthrough */
	case LoopEnd: /* fallthrough */
	case SessionEnd: /* fallthrough */
	case RangeEnd:
		points = new ArdourCanvas::Points (); // leaks
		points->push_back (ArdourCanvas::Duple ( M6, 0.0));
		points->push_back (ArdourCanvas::Duple ( M6, MH));
		points->push_back (ArdourCanvas::Duple (0.0, MH * .5));
		points->push_back (ArdourCanvas::Duple ( M6, 0.0));

		_shift = M6;
		_label_offset = 0.0 * scale;
		break;

	case SelectionStart:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (     0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (.75 * MH, 0.0));
		points->push_back (ArdourCanvas::Duple (     0.0, .75 * MH));
		points->push_back (ArdourCanvas::Duple (     0.0, 0.0));
		_shift = 0;
		_label_offset = 0;
		break;

	case SelectionEnd:
		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (-.75 * MH, 0.0));
		points->push_back (ArdourCanvas::Duple (      0.0, 0.0));
		points->push_back (ArdourCanvas::Duple (      0.0, .75 * MH));
		points->push_back (ArdourCanvas::Duple (-.75 * MH, 0.0));

		_shift = 0;
		_label_offset = 0;
		break;

	case Cue:
		// XXX TODO scope offs
		float offs = 1.0 * scale;

		points = new ArdourCanvas::Points ();
		points->push_back (ArdourCanvas::Duple (offs,    offs));
		points->push_back (ArdourCanvas::Duple (MH-offs*2, offs));
		points->push_back (ArdourCanvas::Duple (MH-offs*2, MH-offs*2));
		points->push_back (ArdourCanvas::Duple (offs,    MH-offs*2));
		points->push_back (ArdourCanvas::Duple (offs,    offs));

		_shift = MH/2;
		_label_offset = 2.0 * scale;
		break;
	}

	_position = pos;
	unit_position = editor.sample_to_pixel (pos.is_zero () ? 0 : pos.samples());
	unit_position -= _shift;

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (unit_position, 1));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("Marker::group for %1", annotation);
#endif

	switch (_type) {
		case RegionCue:
			/* fallthrough */
		case Meter:
			/* fallthrough */
		case Tempo:
			/* fallthrough */
		case SelectionStart:
			/* fallthrough */
		case SelectionEnd:
			_name_flag = 0;
			break;
		default:
			_name_flag = new ArdourCanvas::Rectangle (group);
#ifdef CANVAS_DEBUG
			_name_flag->name = string_compose ("Marker::_name_flag for %1", annotation);
#endif
			break;
	}

	/* adjust to properly locate the tip */

	_pcue = new ArdourCanvas::Circle (group);
	_pmark = new ArdourCanvas::Polygon (group);
	CANVAS_DEBUG_NAME (_pmark, string_compose ("Marker::mark for %1", annotation));

	_pmark->set (*points);

	switch (_type) {
		case SelectionStart:
			/* fallthrough */
		case SelectionEnd:
			_pcue->hide();
			_pmark->show();
			break;
		case Cue:
			_pcue->set_outline(false);
			_pcue->set_fill(true);
			_pcue->set_center ( ArdourCanvas::Duple (MH/2, MH/2) );
			_pcue->set_radius ( MH/2 );

			_pcue->show();
			_pmark->hide();
			if (_name_flag) {
				_name_flag->hide();
			}
			break;
		default:
			_pcue->hide();
			_pmark->show();
			break;
	}

	/* setup name pixbuf sizes */
	name_font = get_font_for_style (N_("MarkerText"));

	Gtk::Label foo;

	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
	int width;

	layout->set_font_description (name_font);
	Gtkmm2ext::get_ink_pixel_size_with_descent (layout, width, name_height, name_descent);

	_name_item = new ArdourCanvas::Text (group);
	CANVAS_DEBUG_NAME (_name_item, string_compose ("ArdourMarker::_name_item for %1", annotation));
	_name_item->set_font_description (name_font);
	_name_item->set_color (RGBA_TO_UINT (0,0,0,255));

	if (_type==Section) {
		_name_item->set_position (ArdourCanvas::Duple (_label_offset, floor (.5 * (name_height - name_height))));
	} else {
		_name_item->set_position (ArdourCanvas::Duple (_label_offset, floor (.5 * (name_height - name_descent - .5))));
	}

	apply_color ();

	set_name (annotation.c_str());

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &ArdourMarker::reposition));

	/* events will be handled by both the group and the mark itself, so
	 * make sure they can both be used to lookup this object.
	 */

	group->set_data ("marker", this);
	_pmark->set_data ("marker", this);
	_pcue->set_data ("marker", this);

	if (handle_events) {
		group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ArdourMarker::color_handler));
}

ArdourMarker::~ArdourMarker ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* not a member of a group that we own, so we must delete it explicitly */

	delete _track_canvas_line;

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete group;
	delete points;
}

void ArdourMarker::reparent(ArdourCanvas::Item & parent)
{
	group->reparent (&parent);
	_parent = &parent;
	group->redraw ();
}

void
ArdourMarker::set_selected (bool s)
{
	if (_selected == s) {
		return;
	}

	_selected = s;
	setup_line ();
	apply_color ();
}

void
ArdourMarker::set_entered (bool yn)
{
	/* if the pointer moves from the polygon to the line, we will get 2
	   enter events in a row, which confuses color management. Catch this.
	*/

	if (yn == _entered) {
		return;
	}

	_entered = yn;
	apply_color ();
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

		ArdourCanvas::Item* line_parent;

		if (_type == RegionCue) {
			line_parent = group;
		} else {
			line_parent = editor.get_cursor_scroll_group();
		}

		if (_track_canvas_line == 0) {
			_track_canvas_line = new ArdourCanvas::Line (line_parent);
			_track_canvas_line->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
		}

		/* discover where our group origin is in canvas coordinates */

		ArdourCanvas::Duple g = group->canvas_origin();
		ArdourCanvas::Duple d;

		if (_type == RegionCue) {
			/* line top is at the top of the region view/track (g.y in canvas coords */
			d = line_parent->canvas_to_item (ArdourCanvas::Duple (g.x + _shift, g.y));
		} else {
			/* line top is at the top of the canvas (0 in canvas coords) */
			d = line_parent->canvas_to_item (ArdourCanvas::Duple (g.x + _shift, 0));
		}

		_track_canvas_line->set_x0 (d.x);
		_track_canvas_line->set_x1 (d.x);
		_track_canvas_line->set_y0 (d.y);
		_track_canvas_line->set_y1 (_line_height > 0 ? d.y + _line_height : ArdourCanvas::COORD_MAX);
		_track_canvas_line->set_outline_color (UIConfiguration::instance().color (_selected ? "entered marker" : _color));
		_track_canvas_line->raise_to_top ();
		_track_canvas_line->show ();

	} else {
		if (_track_canvas_line) {
			_track_canvas_line->hide ();
		}
	}
}

ArdourCanvas::Item&
ArdourMarker::the_item() const
{
	return *group;
}

void
ArdourMarker::set_line_height (double h)
{
	_line_height = h;
	setup_line ();
}

void
ArdourMarker::set_name (const string& new_name, const string& tooltip)
{
	_name = new_name;

	if (_use_tooltip) {
		_pcue->set_tooltip (new_name);
		_pmark->set_tooltip (new_name);
		if (_name_flag) {
			_name_flag->set_tooltip (new_name);
		}
		if (tooltip.empty()) {
			_name_item->set_tooltip (new_name);
		} else {
			_name_item->set_tooltip (tooltip);
		}
	}

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

	double scale = UIConfiguration::instance().get_ui_scale();

	const double padding =  std::max (2., rint (2. * scale));
	const double M3 = std::max(1., rint (3. * scale));

	/* Work out how wide the name can be */
	int name_width;
	int name_height;

	pixel_size (_name, name_font, name_width, name_height);
	name_width = min ((double) name_width + padding, limit);

	if (_name_flag) {
		_name_flag->set_y0 (0);
		_name_flag->set_y1 (marker_height - padding);
	}

	if (name_width == 0) {
		_name_item->hide ();
	} else {
		_name_item->show ();

		if (label_on_left ()) {
			_name_item->set_x_position (-name_width);
		}

		_name_item->clamp_width (name_width);

		if (_type == Cue) {
			if (_cue_index != CueRecord::stop_all) {
				_name_item->set (cue_marker_name (_cue_index));
				int tw = _name_item->text_width();
				int th = _name_item->text_height();
				_name_item->set_position (ArdourCanvas::Duple (_pcue->width()/2 - tw/2 - 1*scale, _pcue->height()/2 - th/2 - 1*scale ));

				_pcue->show();  //show the circle
				_pmark->hide();
			} else {
				_name_item->set ("");
				_pcue->hide();  //show a square
				_pmark->show();
			}
		} else {
			_name_item->set (_name);
		}

		if (_name_flag) {
			if (label_on_left ()) {
				/* adjust right edge of background to fit text */
				_name_flag->set_x0 (_name_item->position().x - padding);
				_name_flag->set_x1 (_name_item->position().x + name_width + _shift);
			} else {
				/* right edge remains at zero (group-relative). Add
				 * arbitrary 2 pixels of extra padding at the end
				 */
				switch (_type) {
				case Tempo:
					_name_item->hide ();
					// tip's x-pos is at "M3", box is 2x marker's
					_name_flag->set_x0 (-M3);
					_name_flag->set_x1 (3 * M3);
					break;
				case Mark:
				case Meter:
					_name_flag->set_x0 (M3);
					_name_flag->set_x1 (_name_item->position().x + name_width + padding);
					break;
				case Cue:
					_name_flag->set_x0 (M3);
					_name_flag->set_x1 (_name_item->position().x + name_width + padding + 1*scale);
					break;
				case Section:
					_name_flag->set_x0 (0);
					_name_flag->set_x1 (_name_item->position().x + name_width + padding);
					_name_flag->set_y1 (marker_height+0.5);  //full height
					break;
				default:
					_name_flag->set_x0 (0);
					_name_flag->set_x1 (_name_item->position().x + name_width + padding);
					break;
				}
			}
		}
	}
}

void
ArdourMarker::set_position (timepos_t const & pos)
{
	unit_position = editor.sample_to_pixel (pos.samples()) - _shift;
	group->set_x_position (unit_position);
	setup_line ();
	_position = pos;
}

void
ArdourMarker::reposition ()
{
	set_position (_position);
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
ArdourMarker::set_points_color (std::string const& color_name)
{
	_points_color = color_name;
	uint32_t c = UIConfiguration::instance().color (_points_color);
	_pcue->set_fill_color (c);
	_pmark->set_fill_color (c);
	_pmark->set_outline_color (c);
}

void
ArdourMarker::set_color (std::string const& color_name)
{
	if (_color == color_name) {
		return;
	}
	_color = color_name;
	apply_color ();
}

void
ArdourMarker::apply_color ()
{
	uint32_t c = color (_entered ? "entered marker" : _color); 
	uint32_t cs = color (_selected || _entered ? "entered marker" : _color); 

	_pcue->set_fill_color (cs);
	_pmark->set_fill_color (cs);
	_pmark->set_outline_color (cs);

	if (_track_canvas_line) {
		_track_canvas_line->set_outline_color (cs);
	}

	if (_name_item) {
		if (_name_flag) {
			/* make sure text stands out over bg color */
			_name_item->set_color (contrasting_text_color (c));
		} else {
			_name_item->set_color (RGBA_TO_UINT (255,255,255,255));  //white: matched to TempoCurve text
		}
	}

	if (_name_flag) {
		_name_flag->set_fill (true);
		_name_flag->set_fill_color (c);
		_name_flag->set_outline (false);
	}
}

void
ArdourMarker::color_handler ()
{
	apply_color ();
	set_points_color (_points_color);
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

MetricMarker::MetricMarker (PublicEditor& ed, ArdourCanvas::Item& parent, std::string const& color_name, const string& annotation,
                            Type type, timepos_t const & pos, bool handle_events)
	: ArdourMarker (ed, parent, color_name, annotation, type, pos, false, nullptr, false)
{
}

/***********************************************************************/
SelectionMarker::SelectionMarker (PublicEditor& editor, ArdourCanvas::Item& parent, std::string const& color_name, Type type)
	: ArdourMarker (editor, parent, color_name, "", type, timepos_t(0), false)
{
	assert (type == SelectionStart || type == SelectionEnd);
#ifdef CANVAS_DEBUG
	group->name = string_compose ("Marker::group for %1", type == SelectionStart ? "SelectionStart" : "SelectionEnd");
#endif
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_selection_marker_event), group));
}

/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, ArdourCanvas::Item& parent, std::string const& color_name, const string& text, Temporal::TempoPoint const & temp, samplepos_t sample, uint32_t curve_color)
	: MetricMarker (editor, parent, color_name, text, Tempo, temp.time(), false)
	, _tempo (&temp)
{
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_marker_event), group, this));
	/* points[1].x gives the width of the marker */
	_curve = new TempoCurve (editor, *group, curve_color, temp, true, (*points)[1].x);
	_curve->the_item().lower_to_bottom ();
}

TempoMarker::~TempoMarker ()
{
	delete _curve;
}

void
TempoMarker::reposition ()
{
	MetricMarker::reposition ();
}

void
TempoMarker::update ()
{
	set_position (_tempo->time());
}

TempoCurve&
TempoMarker::curve()
{
	return *_curve;
}

void
TempoMarker::reset_tempo (Temporal::TempoPoint const & t)
{
	_tempo = &t;
}

Temporal::Point const &
TempoMarker::point() const
{
	return *_tempo;
}

/***********************************************************************/

MeterMarker::MeterMarker (PublicEditor& editor, ArdourCanvas::Item& parent, std::string const& color_name, const string& text, Temporal::MeterPoint const & m)
	: MetricMarker (editor, parent, color_name, text, Meter, m.time(), false)
	, _meter (&m)
{
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_meter_marker_event), group, this));
}

MeterMarker::~MeterMarker ()
{
}

void
MeterMarker::update ()
{
	set_position (_meter->time());
}

void
MeterMarker::reset_meter (Temporal::MeterPoint const & m)
{
	_meter = &m;
}

Temporal::Point const &
MeterMarker::point() const
{
	return *_meter;
}

/***********************************************************************/

BBTMarker::BBTMarker (PublicEditor& editor, ArdourCanvas::Item& parent, std::string const& color_name, Temporal::MusicTimePoint const & p)
	: MetricMarker (editor, parent, color_name, p.name(), BBTPosition, p.time(), false)
	, _point (&p)
{
	set_name (name(), string());
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_bbt_marker_event), group, this));

	tempo_marker = editor.find_marker_for_tempo (p);
	assert (tempo_marker);
	meter_marker = editor.find_marker_for_meter (p);
	assert (meter_marker);
}

BBTMarker::~BBTMarker ()
{
}

void
BBTMarker::update ()
{
	set_position (_point->time());

	tempo_marker->update ();
	meter_marker->update ();
}

void
BBTMarker::reset_point (Temporal::MusicTimePoint const & p)
{
	_point = &p;
}

Temporal::Point const &
BBTMarker::point() const
{
	return *_point;
}

void
BBTMarker::set_position (Temporal::timepos_t const & pos)
{
	ArdourMarker::set_position (pos);
	tempo_marker->set_position (pos);
	meter_marker->set_position (pos);
}
