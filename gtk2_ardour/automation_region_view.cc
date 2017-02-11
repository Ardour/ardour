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

#include "gtkmm2ext/keyboard.h"

#include "automation_region_view.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
#include "public_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

AutomationRegionView::AutomationRegionView (ArdourCanvas::Container*                  parent,
                                            AutomationTimeAxisView&                   time_axis,
                                            boost::shared_ptr<ARDOUR::Region>         region,
                                            const Evoral::Parameter&                  param,
                                            boost::shared_ptr<ARDOUR::AutomationList> list,
                                            double                                    spu,
                                            uint32_t                                  basic_color)
	: RegionView(parent, time_axis, region, spu, basic_color, true)
	, _region_relative_time_converter(region->session().tempo_map(), region->position())
	, _source_relative_time_converter(region->session().tempo_map(), region->position() - region->start())
	, _parameter(param)
{
	TimeAxisViewItem::set_position (_region->position(), this);

	if (list) {
		assert(list->parameter() == param);
		create_line(list);
	}

	group->raise_to_top();

	trackview.editor().MouseModeChanged.connect(_mouse_mode_connection, invalidator (*this),
	                                            boost::bind (&AutomationRegionView::mouse_mode_changed, this),
	                                            gui_context ());
}

AutomationRegionView::~AutomationRegionView ()
{
	in_destructor = true;
	RegionViewGoingAway (this); /* EMIT_SIGNAL */
}

void
AutomationRegionView::init (bool /*wfd*/)
{
	_enable_display = false;

	RegionView::init (false);

	reset_width_dependent_items ((double) _region->length() / samples_per_pixel);

	set_height (trackview.current_height());

	fill_color_name = "midi frame base";
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
	_line->set_height ((uint32_t)rint(trackview.current_height() - 2.5 - NAME_HIGHLIGHT_SIZE));
	_line->set_visibility (AutomationLine::VisibleAspects (AutomationLine::Line|AutomationLine::ControlPoints));
	_line->set_maximum_time (_region->length());
	_line->set_offset (_region->start ());
}

uint32_t
AutomationRegionView::get_fill_color() const
{
	const std::string mod_name = (_dragging ? "dragging region" :
	                              trackview.editor().internal_editing() ? "editable region" :
	                              "midi frame base");
	if (_selected) {
		return UIConfiguration::instance().color_mod ("selected region base", mod_name);
	} else if (high_enough_for_name || !UIConfiguration::instance().get_color_regions_using_track_color()) {
		return UIConfiguration::instance().color_mod ("midi frame base", mod_name);
	}
	return UIConfiguration::instance().color_mod (fill_color, mod_name);
}

void
AutomationRegionView::mouse_mode_changed ()
{
	// Adjust frame colour (become more transparent for internal tools)
	set_frame_color();
}

bool
AutomationRegionView::canvas_group_event (GdkEvent* ev)
{
	if (in_destructor) {
		return false;
	}

	PublicEditor& e = trackview.editor ();

	if (trackview.editor().internal_editing() &&
	    ev->type == GDK_BUTTON_RELEASE &&
	    e.current_mouse_mode() == Editing::MouseDraw &&
	    !e.drags()->active()) {

		double x = ev->button.x;
		double y = ev->button.y;

		/* convert to item coordinates in the time axis view */
		automation_view()->canvas_display()->canvas_to_item (x, y);

		/* clamp y */
		y = std::max (y, 0.0);
		y = std::min (y, _height - NAME_HIGHLIGHT_SIZE);

		/* guard points only if primary modifier is used */
		bool with_guard_points = Gtkmm2ext::Keyboard::modifier_state_equals (ev->button.state, Gtkmm2ext::Keyboard::PrimaryModifier);
		add_automation_event (ev, e.pixel_to_sample (x) - _region->position() + _region->start(), y, with_guard_points);
		return true;
	}

	return RegionView::canvas_group_event (ev);
}

/** @param when Position in frames, where 0 is the start of the region.
 *  @param y y position, relative to our TimeAxisView.
 */
void
AutomationRegionView::add_automation_event (GdkEvent *, framepos_t when, double y, bool with_guard_points)
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

	when = snap_frame_to_frame (when - _region->start ()).frame + _region->start ();

	/* map using line */

	double when_d = when;
	_line->view_to_model_coord (when_d, y);

	XMLNode& before = _line->the_list()->get_state();

	if (_line->the_list()->editor_add (when_d, y, with_guard_points)) {
		view->editor().begin_reversible_command (_("add automation event"));

		XMLNode& after = _line->the_list()->get_state();

		view->session()->add_command (new MementoCommand<ARDOUR::AutomationList> (_line->memento_command_binder(), &before, &after));
		view->editor().commit_reversible_command ();

		view->session()->set_dirty ();
	}
}

bool
AutomationRegionView::paste (framepos_t                                      pos,
                             unsigned                                        paste_count,
                             float                                           times,
                             boost::shared_ptr<const ARDOUR::AutomationList> slist)
{
	AutomationTimeAxisView* const             view    = automation_view();
	boost::shared_ptr<ARDOUR::AutomationList> my_list = _line->the_list();

	if (view->session()->transport_rolling() && my_list->automation_write()) {
		/* do not paste if this control is in write mode and we're rolling */
		return false;
	}

	/* add multi-paste offset if applicable */
	pos += view->editor().get_paste_offset(
		pos, paste_count, _source_relative_time_converter.to(slist->length()));

	const double model_pos = _source_relative_time_converter.from(
		pos - _source_relative_time_converter.origin_b());

	XMLNode& before = my_list->get_state();
	my_list->paste(*slist, model_pos, times);
	view->session()->add_command(
		new MementoCommand<ARDOUR::AutomationList>(_line->memento_command_binder(), &before, &my_list->get_state()));

	return true;
}

void
AutomationRegionView::set_height (double h)
{
	RegionView::set_height(h);

	if (_line) {
		_line->set_height ((uint32_t)rint(h - 2.5 - NAME_HIGHLIGHT_SIZE));
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

	if (what_changed.contains (ARDOUR::Properties::position)) {
		_region_relative_time_converter.set_origin_b(_region->position());
	}

	if (what_changed.contains (ARDOUR::Properties::start) ||
	    what_changed.contains (ARDOUR::Properties::position)) {
		_source_relative_time_converter.set_origin_b (_region->position() - _region->start());
	}

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
AutomationRegionView::entered ()
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
