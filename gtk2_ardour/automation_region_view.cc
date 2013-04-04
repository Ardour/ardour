/*
    Copyright (C) 2007 Paul Davis
    Author: David Robillard

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

#include <utility>

#include "pbd/memento_command.h"

#include "ardour/automation_control.h"
#include "ardour/event_type_map.h"
#include "ardour/midi_automation_list_binder.h"
#include "ardour/midi_region.h"
#include "ardour/session.h"

#include "automation_region_view.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
#include "public_editor.h"

#include "i18n.h"

AutomationRegionView::AutomationRegionView (ArdourCanvas::Group*                      parent,
					    AutomationTimeAxisView&                   time_axis,
					    boost::shared_ptr<ARDOUR::Region>         region,
					    const Evoral::Parameter&                  param,
					    boost::shared_ptr<ARDOUR::AutomationList> list,
					    double                                    spu,
					    Gdk::Color const &                        basic_color)
	: RegionView(parent, time_axis, region, spu, basic_color, true)
	, _parameter(param)
{
	if (list) {
		assert(list->parameter() == param);
		create_line(list);
	}

	group->Event.connect (sigc::mem_fun (this, &AutomationRegionView::canvas_event));
	group->raise_to_top();
}

AutomationRegionView::~AutomationRegionView ()
{
}

void
AutomationRegionView::init (Gdk::Color const & basic_color, bool /*wfd*/)
{
	_enable_display = false;

	RegionView::init(basic_color, false);

	compute_colors (basic_color);

	reset_width_dependent_items ((double) _region->length() / frames_per_pixel);

	set_height (trackview.current_height());

	set_colors ();

	_enable_display = true;
}

void
AutomationRegionView::create_line (boost::shared_ptr<ARDOUR::AutomationList> list)
{
	_line = boost::shared_ptr<AutomationLine> (new MidiAutomationLine(
				ARDOUR::EventTypeMap::instance().to_symbol(list->parameter()),
				trackview, *get_canvas_group(), list,
				boost::dynamic_pointer_cast<ARDOUR::MidiRegion> (_region),
				_parameter,
				&_source_relative_time_converter));
	_line->set_colors();
	_line->set_height ((uint32_t)rint(trackview.current_height() - NAME_HIGHLIGHT_SIZE));
	_line->set_visibility (AutomationLine::VisibleAspects (AutomationLine::Line|AutomationLine::ControlPoints));
	_line->set_maximum_time (_region->length());
	_line->set_offset (_region->start ());
}

bool
AutomationRegionView::canvas_event (GdkEvent* ev)
{
	PublicEditor& e = trackview.editor ();

	if (ev->type == GDK_BUTTON_PRESS && e.current_mouse_mode() == Editing::MouseObject) {

		/* XXX: icky dcast to Editor */
		e.drags()->set (new EditorRubberbandSelectDrag (dynamic_cast<Editor*> (&e), group), ev);

	} else if (ev->type == GDK_BUTTON_RELEASE) {

		if (trackview.editor().drags()->active() && trackview.editor().drags()->end_grab (ev)) {
			return true;
		}

		double x = ev->button.x;
		double y = ev->button.y;

		/* convert to item coordinates in the time axis view */
		automation_view()->canvas_display()->canvas_to_item (x, y);

		/* clamp y */
		y = std::max (y, 0.0);
		y = std::min (y, _height - NAME_HIGHLIGHT_SIZE);

		add_automation_event (ev, trackview.editor().pixel_to_frame (x) - _region->position() + _region->start(), y);
	}

	return false;
}

/** @param when Position in frames, where 0 is the start of the region.
 *  @param y y position, relative to our TimeAxisView.
 */
void
AutomationRegionView::add_automation_event (GdkEvent *, framepos_t when, double y)
{
	if (!_line) {
		boost::shared_ptr<Evoral::Control> c = _region->control(_parameter, true);
		boost::shared_ptr<ARDOUR::AutomationControl> ac
				= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(c);
		assert(ac);
		create_line(ac->alist());
	}
	assert(_line);

	AutomationTimeAxisView* const view = automation_view ();

	/* compute vertical fractional position */

	const double h = trackview.current_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2;
	y = 1.0 - (y / h);

	/* snap frame */

	when = snap_frame_to_frame (when - _region->start ()) + _region->start ();

	/* map using line */

	double when_d = when;
	_line->view_to_model_coord (when_d, y);

	view->session()->begin_reversible_command (_("add automation event"));
	XMLNode& before = _line->the_list()->get_state();

	_line->the_list()->add (when_d, y);

	XMLNode& after = _line->the_list()->get_state();

	/* XXX: hack! */
	boost::shared_ptr<ARDOUR::MidiRegion> mr = boost::dynamic_pointer_cast<ARDOUR::MidiRegion> (_region);
	assert (mr);

	view->session()->commit_reversible_command (
		new MementoCommand<ARDOUR::AutomationList> (new ARDOUR::MidiAutomationListBinder (mr->midi_source(), _parameter), &before, &after)
		);


	view->session()->set_dirty ();
}

void
AutomationRegionView::set_height (double h)
{
	RegionView::set_height(h);

	if (_line) {
		_line->set_height ((uint32_t)rint(h - NAME_HIGHLIGHT_SIZE));
	}
}

bool
AutomationRegionView::set_position (framepos_t pos, void* src, double* ignored)
{
	if (_line) {
		_line->set_maximum_time (_region->length ());
	}

	return RegionView::set_position(pos, src, ignored);
}


void
AutomationRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);

	if (_line) {
		_line->reset();
	}
}


void
AutomationRegionView::region_resized (const PBD::PropertyChange& what_changed)
{
	RegionView::region_resized (what_changed);

	if (!_line) {
		return;
	}

	if (what_changed.contains (ARDOUR::Properties::start)) {
		_line->set_offset (_region->start ());
	}

	if (what_changed.contains (ARDOUR::Properties::length)) {
		_line->set_maximum_time (_region->length());
	}
}


void
AutomationRegionView::entered (bool)
{
	if (_line) {
		_line->track_entered();
	}
}


void
AutomationRegionView::exited ()
{
	if (_line) {
		_line->track_exited();
	}
}
