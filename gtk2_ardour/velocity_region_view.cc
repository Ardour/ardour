/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#include <utility>

#include "pbd/memento_command.h"

#include "ardour/automation_control.h"
#include "ardour/event_type_map.h"
#include "ardour/midi_automation_list_binder.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/session.h"

#include "gtkmm2ext/keyboard.h"

#include "velocity_region_view.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
#include "public_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Temporal;

VelocityRegionView::VelocityRegionView (ArdourCanvas::Container*                  parent,
                                        VelocityTimeAxisView&                   time_axis,
                                        boost::shared_ptr<ARDOUR::Region>         region,
                                        boost::shared_ptr<ARDOUR::AutomationList> list,
                                        double                                    spu,
                                        uint32_t                                  basic_color)
	: RegionView(parent, time_axis, region, spu, basic_color, true)
	, _parameter(ARDOUR::MidiVelocityAutomation)
{
	TimeAxisViewItem::set_position (_region->position(), this);

	if (list) {
		assert(list->parameter().type() == ARDOUR::MidiVelocityAutomation);
		create_line(list);
	}

	group->raise_to_top();

	trackview.editor().MouseModeChanged.connect(_mouse_mode_connection, invalidator (*this),
	                                            boost::bind (&VelocityRegionView::mouse_mode_changed, this),
	                                            gui_context ());
}

VelocityRegionView::~VelocityRegionView ()
{
	in_destructor = true;
	RegionViewGoingAway (this); /* EMIT_SIGNAL */
}

void
VelocityRegionView::init (bool /*wfd*/)
{
	DisplaySuspender (*this);

	RegionView::init (false);

	reset_width_dependent_items ((double) _region->length_samples() / samples_per_pixel);

	set_height (trackview.current_height());

	set_colors ();
}

void
VelocityRegionView::create_line (boost::shared_ptr<ARDOUR::AutomationList> list)
{
	_line = boost::shared_ptr<AutomationLine> (new MidiAutomationLine(
				ARDOUR::EventTypeMap::instance().to_symbol(list->parameter()),
				trackview, *get_canvas_group(), list,
				boost::dynamic_pointer_cast<ARDOUR::MidiRegion> (_region),
				_parameter));
	_line->set_colors();
	_line->set_height ((uint32_t)rint(trackview.current_height() - 2.5 - NAME_HIGHLIGHT_SIZE));
	_line->set_visibility (AutomationLine::VisibleAspects (AutomationLine::Line|AutomationLine::ControlPoints));
	_line->set_maximum_time (timepos_t (_region->length()));
	_line->set_offset (_region->start ());
}

uint32_t
VelocityRegionView::get_fill_color() const
{
	const std::string mod_name = (_dragging ? "dragging region" :
	                              trackview.editor().internal_editing() ? "editable region" : fill_color_name);
	if (_selected) {
		return UIConfiguration::instance().color_mod ("selected region base", mod_name);
	} else if (high_enough_for_name || !UIConfiguration::instance().get_color_regions_using_track_color()) {
		return UIConfiguration::instance().color_mod (fill_color_name, mod_name);
	}
	return UIConfiguration::instance().color_mod (fill_color, mod_name);
}

void
VelocityRegionView::mouse_mode_changed ()
{
	/* Adjust frame colour (become more transparent for internal tools) */
	set_frame_color();
}

bool
VelocityRegionView::canvas_group_event (GdkEvent* ev)
{
	if (in_destructor) {
		return false;
	}

	PublicEditor& e = trackview.editor ();

	if (trackview.editor().internal_editing() &&
	    ev->type == GDK_BUTTON_RELEASE &&
	    ev->button.button == 1 &&
	    e.current_mouse_mode() == Editing::MouseDraw &&
	    !e.drags()->active()) {

		double x = ev->button.x;
		double y = ev->button.y;

		/* convert to item coordinates in the time axis view */
		velocity_view()->canvas_display()->canvas_to_item (x, y);

		/* clamp y */
		y = std::max (y, 0.0);
		y = std::min (y, _height - NAME_HIGHLIGHT_SIZE);

		/* guard points only if primary modifier is used */
		bool with_guard_points = Gtkmm2ext::Keyboard::modifier_state_equals (ev->button.state, Gtkmm2ext::Keyboard::PrimaryModifier);

		/* the time domain doesn't matter here, because the automation
		 * list will force the position to its own time domain when
		 * adding the point.
		 */

		add_automation_event (ev, timepos_t (e.pixel_to_sample (x)), y, with_guard_points);
		return true;
	}

	return RegionView::canvas_group_event (ev);
}

/** @param when Position is global time position
 *  @param y y position, relative to our TimeAxisView.
 */
void
VelocityRegionView::add_automation_event (GdkEvent *, timepos_t const & w, double y, bool with_guard_points)
{
	boost::shared_ptr<Evoral::Control> c = _region->control(_parameter, true);
	boost::shared_ptr<ARDOUR::AutomationControl> ac = boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(c);
	timepos_t when (w); /* the non-const copy */

	if (!_line) {
		assert(ac);
		create_line(ac->alist());
	}
	assert(_line);

	VelocityTimeAxisView* const view = velocity_view ();

	/* compute vertical fractional position */
	y = 1.0 - (y / _line->height());

	/* snap time */

	when = snap_region_time_to_region_time (_region->source_position().distance (when), false);

	/* map using line */

	_line->view_to_model_coord_y (y);

	if (c->list()->size () == 0) {
		/* we need the MidiTrack::MidiControl, not the region's (midi model source) control */
		boost::shared_ptr<ARDOUR::MidiTrack> mt = boost::dynamic_pointer_cast<ARDOUR::MidiTrack> (view->parent_stripable ());
		assert (mt);
		boost::shared_ptr<Evoral::Control> mc = mt->control(_parameter);
		assert (mc);
		y = mc->get_double ();
	} else if (UIConfiguration::instance().get_new_automation_points_on_lane()) {
		y = c->list()->eval (when);
	}

	XMLNode& before = _line->the_list()->get_state();

	if (_line->the_list()->editor_add (when, y, with_guard_points)) {

		if (ac->automation_state () == ARDOUR::Off) {
			view->set_automation_state (ARDOUR::Play);
		}
		if (UIConfiguration::instance().get_automation_edit_cancels_auto_hide () && ac == view->session()->recently_touched_controllable ()) {
			RouteTimeAxisView::signal_ctrl_touched (false);
		}

		view->editor().begin_reversible_command (_("add automation event"));

		XMLNode& after = _line->the_list()->get_state();

		view->session()->add_command (new MementoCommand<ARDOUR::AutomationList> (_line->memento_command_binder(), &before, &after));
		view->editor().commit_reversible_command ();

		view->session()->set_dirty ();
	}
}

bool
VelocityRegionView::paste (timepos_t const &                               pos,
                             unsigned                                        paste_count,
                             float                                           times,
                             boost::shared_ptr<const ARDOUR::AutomationList> slist)
{
	using namespace ARDOUR;

	VelocityTimeAxisView* const             view    = velocity_view();
	boost::shared_ptr<ARDOUR::AutomationList> my_list = _line->the_list();

	if (view->session()->transport_rolling() && my_list->automation_write()) {
		/* do not paste if this control is in write mode and we're rolling */
		return false;
	}

	timecnt_t len = slist->length();
	timepos_t p (pos);

	/* add multi-paste offset if applicable */
	p += view->editor ().get_paste_offset (pos, paste_count > 0 ? 1 : 0, len);

	timepos_t model_pos = pos;

	/* potentially snap */

	view->editor().snap_to (model_pos, Temporal::RoundNearest);

	/* convert timeline position to model's (source-relative) position */

	model_pos = timepos_t (_region->source_position().distance (model_pos));

	XMLNode& before = my_list->get_state();
	my_list->paste (*slist, model_pos);
	view->session()->add_command(new MementoCommand<ARDOUR::AutomationList>(_line->memento_command_binder(), &before, &my_list->get_state()));

	return true;
}

void
VelocityRegionView::set_height (double h)
{
	RegionView::set_height(h);

	if (_line) {
		_line->set_height ((uint32_t)rint(h - 2.5 - NAME_HIGHLIGHT_SIZE));
	}
}

bool
VelocityRegionView::set_position (timepos_t const & pos, void* src, double* ignored)
{
	if (_line) {
		_line->set_maximum_time (timepos_t (_region->length ()));
	}

	return RegionView::set_position(pos, src, ignored);
}


void
VelocityRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);

	if (_line) {
		_line->reset ();
	}
}

void
VelocityRegionView::region_resized (const PBD::PropertyChange& what_changed)
{
	RegionView::region_resized (what_changed);

	if (!_line) {
		return;
	}

	if (what_changed.contains (ARDOUR::Properties::start)) {
		_line->set_offset (_region->start ());
	}

	if (what_changed.contains (ARDOUR::Properties::length)) {
		_line->set_maximum_time (timepos_t (_region->length()));
	}
}

void
VelocityRegionView::tempo_map_changed ()
{
	if (_line) {
		_line->tempo_map_changed ();
	}

	set_position (_region->position(), 0, 0);
	set_duration (_region->length(), 0);
}

void
VelocityRegionView::entered ()
{
	if (_line) {
		_line->track_entered();
	}
}


void
VelocityRegionView::exited ()
{
	if (_line) {
		_line->track_exited();
	}
}
