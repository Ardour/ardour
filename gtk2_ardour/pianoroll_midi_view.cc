/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/triggerbox.h"
#include "ardour/types.h"

#include "gtkmm2ext/utils.h"

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/text.h"

#include "control_point.h"
#include "editing_context.h"
#include "editor_drag.h"
#include "hit.h"
#include "keyboard.h"
#include "mergeable_line.h"
#include "pianoroll_automation_line.h"
#include "pianoroll_midi_view.h"
#include "pianoroll_velocity.h"
#include "note.h"
#include "ui_config.h"
#include "velocity_display.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;

PianorollMidiView::PianorollMidiView (std::shared_ptr<ARDOUR::MidiTrack> mt,
                                      ArdourCanvas::Item&      parent,
                                      ArdourCanvas::Item&      noscroll_parent,
                                      EditingContext&          ec,
                                      MidiViewBackground&      bg)
	: MidiView (mt, parent, ec, bg)
	, _noscroll_parent (&noscroll_parent)
	, overlay_text (nullptr)
	, active_automation_parameter (NullAutomation)
	, _height (0.)
{
	CANVAS_DEBUG_NAME (_note_group, X_("note group for MIDI cue"));

	/* Containers don't get canvas events, so we need an invisible rect
	 * that will. It will be resized as needed sothat it always covers the
	 * entire canvas/view.
	 */

	event_rect = new ArdourCanvas::Rectangle (&parent);
	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, 10.));
	event_rect->set_fill (false);
	event_rect->set_outline (false);
	CANVAS_DEBUG_NAME (event_rect, "cue event rect");

	/* The event rect is a sibling of canvas items that share @param
	 * parent. Consequently, it does not get events that they do not handle
	 * (because event propagation is up the item child->parent hierarchy,
	 * not sideways.
	 *
	 * This means that if, for example, the start boundary rect doesn't
	 * handle an event, it will propagate to @param parent. By contrast, if
	 * the mouse pointer is not over some other type of item, an event will
	 * be delivered to the event_rect.
	 *
	 * We need to capture events in both scenarios, so we connect to both
	 * the event rect and the @param parent, with the same handler.
	 *
	 * The reason this is more complex than one may expect is that other
	 * classes create items that share @param parent. Consequently, we
	 * can't make event_rect the parent of "all items in this MidiView".
	 * However, if we don't have an event_rect, then we can end up with
	 * the current_item in the canvas being invalid because the canvas
	 * cannot identify any *new* item to be the current item as the mouse
	 * pointer moves. This means that mouse pointer motion does not change
	 * the current_item, and this means that we do not get enter/leave
	 * events for the current_item.
	 */

	er_connection = event_rect->Event.connect ([this] (GdkEvent* ev) { return midi_canvas_group_event (ev); });
	parent_connection = parent.Event.connect ([this] (GdkEvent* ev) { return midi_canvas_group_event (ev); });

	_note_group->raise_to_top ();

	_show_source = true;
	_on_timeline = false;
	set_extensible (true);
}

PianorollMidiView::~PianorollMidiView ()
{
	er_connection.disconnect ();
	parent_connection.disconnect ();

	for (auto & [param,lane] : automation_map) {
		delete lane;
	}
}

bool
PianorollMidiView::midi_canvas_group_event (GdkEvent* ev)
{
	// std::cerr << "mcge " << Gtkmm2ext::event_type_string (ev->type) << std::endl;

	if (!_sensitive) {
		return false;
	}

	EC_LOCAL_TEMPO_SCOPE_ARG (_editing_context);

	if (ev->type != GDK_MOTION_NOTIFY || (active_automation_parameter.type() == NullAutomation)) {
		/* Let MidiView do its thing */
		if (MidiView::midi_canvas_group_event (ev)) {
			return true;
		}
	}

	return _editing_context.canvas_bg_event (ev, event_rect);
}

XMLNode*
PianorollMidiView::automation_state () const
{
	if (automation_map.empty()) {
		return nullptr;
	}

	XMLNode* root = new XMLNode (X_("lanes"));

	for (auto const & [param,lane] : automation_map) {
		XMLNode* child = new XMLNode (X_("lane"));
		child->set_property (X_("param"), ARDOUR::EventTypeMap::instance().to_symbol (param));
		root->add_child_nocopy (*child);
	}

	return root;
}

void
PianorollMidiView::set_automation_state (XMLNode const & node)
{
}

void
PianorollMidiView::set_sensitive (bool yn)
{
	MidiView::set_sensitive (yn);
	for (auto & [param,lane] : automation_map) {
		lane->set_sensitive (yn);
	}
}

void
PianorollMidiView::set_height (double h)
{
	/* lane heights are set in ::partition_height() */

	_height = h;
	view_changed ();
}

ArdourCanvas::Item*
PianorollMidiView::drag_group () const
{
	return event_rect;
}

bool
PianorollMidiView::scroll (GdkEventScroll* ev)
{
	if (_editing_context.drags()->active()) {
		return false;
	}

	switch (ev->direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollHorizontalModifier)) {
			_editing_context.scroll_left_step ();
			return true;
		}
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			_editing_context.reset_zoom (_editing_context.get_current_zoom() / 2);
			return true;
		}
		break;
	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollHorizontalModifier)) {
			_editing_context.scroll_right_step ();
			return true;
		}
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			_editing_context.reset_zoom (_editing_context.get_current_zoom() * 2);
			return true;
		}
		break;
	default:
		break;
	}

	return MidiView::scroll (ev);
}

void
PianorollMidiView::set_samples_per_pixel (double spp)
{
	Temporal::timecnt_t duration;

	if (_midi_region) {
		duration = Temporal::timecnt_t (_midi_region->midi_source()->length().beats());
	} else {
		duration = Temporal::timecnt_t (Temporal::Beats (4, 0));
	}

	/* XXX Really needs to use a tempo map based on the _midi_region (and its SMF) */

	reset_width_dependent_items (_editing_context.duration_to_pixels (duration));
}

void
PianorollMidiView::reset_width_dependent_items (double pixel_width)
{
	MidiView::reset_width_dependent_items (pixel_width);

	if (overlay_text) {
		overlay_text->set_position (ArdourCanvas::Duple ((midi_context().width() / 2.0) - (overlay_text->text_width()/2.), (midi_context().height() / 2.0) - (overlay_text->text_height() / 2.)));
	}

	for (auto & a : automation_map) {
		if (a.second->line) {
			a.second->line->reset ();
		}
	}
}
void
PianorollMidiView::clear_ghost_events ()
{
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->clear ();
	}
}

void
PianorollMidiView::ghosts_model_changed ()
{
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->clear();
		for (auto & ev : _events) {
			lane->velocity_display->add_note (ev.second);
		}
	}
}

void
PianorollMidiView::ghosts_view_changed ()
{
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->redisplay();
	}
}

void
PianorollMidiView::ghost_remove_note (NoteBase* nb)
{
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->remove_note (nb);
	}
}

void
PianorollMidiView::ghost_add_note (NoteBase* nb)
{
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->add_note (nb);
	}
}

void
PianorollMidiView::ghost_sync_selection (NoteBase* nb)
{
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->note_selected (nb);
	}
}

void
PianorollMidiView::update_sustained (Note* n)
{
	MidiView::update_sustained (n);
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->update_note (n);
	}
}

void
PianorollMidiView::update_hit (Hit* h)
{
	MidiView::update_hit (h);
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->update_note (h);
	}
}

void
PianorollMidiView::swap_automation_channel (int new_channel)
{
	std::vector<Evoral::Parameter> new_params;
	std::vector<Pianoroll::AutomationLane*> parents;
	/* Make a note of what was visible, but use the new channel */

	for (auto & [old_param,lane] : automation_map) {
		Evoral::Parameter param (old_param.type(), new_channel, old_param.id());
		new_params.push_back (param);
		parents.push_back (&lane->parent);
		delete lane;
	}

	/* Drop the old */

	automation_map.clear ();

	/* Create the new */

	auto par = parents.begin();

	for (auto const & p : new_params) {
		add_automation_lane (p, **par);
		++par;
	}
}

void
PianorollMidiView::color_handler()
{
	MidiView::color_handler ();
	AutomationLane* lane = automation_lane_by_param (MidiVelocityAutomation);
	if (lane) {
		lane->velocity_display->set_colors ();
	}
}

Gtkmm2ext::Color
PianorollMidiView::line_color_for (Evoral::Parameter const & param)
{
	UIConfiguration& uic (UIConfiguration::instance());

	switch (param.type()) {
	case MidiCCAutomation:
		switch (param.id()) {
		case MIDI_CTL_MSB_EXPRESSION:
			return uic.color ("pianoroll: insensitive expression line");
		case MIDI_CTL_MSB_MODWHEEL:
			return uic.color ("pianoroll: insensitive modulation line");
		}
		break;
	case MidiPitchBenderAutomation:
		return uic.color ("pianoroll: insensitive bender line");
	case MidiChannelPressureAutomation:
		return uic.color ("pianoroll: insensitive pressure line");
	}

	return 0xff0000ff;
}

void
PianorollMidiView::remove_automation_lane (Evoral::Parameter const & param, Pianoroll::AutomationLane& lane_parent)
{
	auto existing = automation_map.find (param);
	if (existing == automation_map.end()) {
		return;
	}

	delete existing->second;

	automation_map.erase (existing);
}

void
PianorollMidiView::partition_height ()
{
	for (auto & [param,lane] : automation_map) {
		lane->set_height (lane->parent.group->height());
	}
}

PianorollMidiView::AutomationLane*
PianorollMidiView::automation_lane_by_param (Evoral::Parameter const & param)
{
	auto i = automation_map.find (param);
	if (i != automation_map.end()) {
		return i->second;
	}
	return nullptr;
}

void
PianorollMidiView::set_active_automation (Evoral::Parameter const & param)
{
	AutomationLane* lane;

	if (active_automation_parameter.type() != NullAutomation) {
		lane = automation_lane_by_param (active_automation_parameter);
		if (lane && lane->line) {
			lane->line->track_exited ();
		}
	}

	active_automation_parameter = param;
	lane = automation_lane_by_param (param);

	if (lane && lane->line) {
		lane->line->track_entered ();
	}

	_editing_context.verbose_cursor().set ("");
	hide_verbose_cursor();
}

void
PianorollMidiView::clear_automation_lane (Evoral::Parameter const & param)
{
	AutomationLane* lane = automation_lane_by_param (param);
	if (!lane) {
		return;
	}

	editing_context().begin_reversible_command (_("Clear automation"));
	lane->line->clear ();
	editing_context().commit_reversible_command ();
}

void
PianorollMidiView::add_automation_lane (Evoral::Parameter const & param, Pianoroll::AutomationLane& lane_parent)
{
	if (!midi_region()) {
		editing_context().make_a_region();
	}

	/* Step one: find the AutomationLane object for this parameter,
	 * or create it if it does not already exist.
	 */

	if (automation_map.find (param) != automation_map.end()) {
		/* already present */
		return;
	}

	AutomationLane* lane = nullptr;

	if (param.type() == MidiVelocityAutomation) {

		/* Create and add to automation display map */

		VelocityDisplay* velocity_display = new PianorollVelocityDisplay (editing_context(), midi_context(), *this, *lane_parent.group, 0x312244ff);
		for (auto & ev : _events) {
			velocity_display->add_note (ev.second);
		}
		velocity_display->set_sensitive (_sensitive);

		lane = new AutomationLane (*velocity_display, false, lane_parent);

	} else {

		std::shared_ptr<Evoral::Control> control = _midi_region->model()->control (param, true);
		CueAutomationControl ac = std::dynamic_pointer_cast<AutomationControl> (control);

		if (!ac) {
			return;
		}

		CueAutomationLine line (new PianorollAutomationLine (ARDOUR::EventTypeMap::instance().to_symbol (param),
		                                                     _editing_context,
		                                                     *lane_parent.group,
		                                                     lane_parent.group,
		                                                     ac->alist(),
		                                                     ac->desc()));

		Gtkmm2ext::Color c (_midi_track->presentation_info().color ());
		line->set_sensitive_line_color (c);
		c = Gtkmm2ext::change_alpha (c, 0.2);
		line->set_insensitive_line_color (c);
		line->set_sensitive (_sensitive);

		lane = new AutomationLane (ac, line, false, lane_parent);
	}

	automation_map.insert (std::make_pair (param, lane));
	lane->set_height (lane->parent.group->height());
}

void
PianorollMidiView::remove_all_automation ()
{
	for (auto & [parameter,lane] : automation_map) {
		delete lane;
	}

	automation_map.clear ();
}

std::list<SelectableOwner*>
PianorollMidiView::selectable_owners()
{
	std::list<SelectableOwner*> sl;
	return sl;
}

MergeableLine*
PianorollMidiView::make_merger ()
{
	AutomationLane* lane = automation_lane_by_param (active_automation_parameter);

	if (lane && lane->line) {
		return new MergeableLine (lane->line, lane->control,
		                          [](Temporal::timepos_t const& t) { return t; },
		                          nullptr, nullptr);
	}

	return nullptr;
}

bool
PianorollMidiView::automation_rb_click (GdkEvent* event, Temporal::timepos_t const & pos, Evoral::Parameter param)
{
	AutomationLane* lane = automation_lane_by_param (param);

	if (!lane || !lane->control || !lane->line) {
		return false;
	}

	bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
	lane->line->add (lane->control, event, pos, event->button.y, with_guard_points);

	return false;
}

bool
PianorollMidiView::velocity_rb_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
	return false;
}

void
PianorollMidiView::line_drag_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
}

PianorollMidiView::AutomationLane::~AutomationLane()
{
	/* line is managed via a shared_ptr */
	delete velocity_display;
}

void
PianorollMidiView::AutomationLane::set_sensitive (bool yn)
{
	if (line) {
		line->set_sensitive (yn);
	} else if (velocity_display) {
		velocity_display->set_sensitive (yn);
	}
}

void
PianorollMidiView::AutomationLane::set_height (double h)
{
	if (velocity_display) {
		// velocity_display->set_height (h);
	} else if (line) {
		line->set_height (h - 4);
	}
}

void
PianorollMidiView::point_selection_changed ()
{
	AutomationLane* lane = automation_lane_by_param (active_automation_parameter);

	if (lane && lane->line) {
		lane->line->set_selected_points (_editing_context.get_selection().points);
	}
}

void
PianorollMidiView::clear_selection ()
{
	MidiView::clear_note_selection ();
	PointSelection empty;

	for (auto & [param,lane] : automation_map) {
		if (lane->line) {
			lane->line->set_selected_points (empty);
		}
	}
}

void
PianorollMidiView::set_overlay_text (std::string const & str)
{
	if (!overlay_text) {
		overlay_text = new ArdourCanvas::Text (_noscroll_parent);
		Pango::FontDescription font ("Sans 200");
		overlay_text->set_font_description (font);
		overlay_text->set_color (0xff000088);
		overlay_text->set ("0"); /* not shown, used for positioning math */
		overlay_text->set_position (ArdourCanvas::Duple ((midi_context().width() / 2.0) - (overlay_text->text_width()/2.), (midi_context().height() / 2.0) - (overlay_text->text_height() / 2.)));
	}

	overlay_text->set (str);
	show_overlay_text ();
}

void
PianorollMidiView::show_overlay_text ()
{
	if (overlay_text) {
		overlay_text->show ();
	}
}

void
PianorollMidiView::hide_overlay_text ()
{
	if (overlay_text) {
		overlay_text->hide ();
	}
}

void
PianorollMidiView::cut_copy_clear (::Selection& selection, Editing::CutCopyOp op)
{
	MidiView::cut_copy_clear (selection, op);

	cut_copy_points (op, timepos_t::zero (Temporal::BeatTime));
}

struct PointsSelectionPositionSorter {
	bool operator() (ControlPoint* a, ControlPoint* b) {
		return (*(a->model()))->when < (*(b->model()))->when;
	}
};

/** Cut, copy or clear selected automation points.
 *  @param op Operation (Cut, Copy or Clear)
 */
void
PianorollMidiView::cut_copy_points (Editing::CutCopyOp op, timepos_t const & earliest_time)
{
	using namespace Editing;

	::Selection& selection (_editing_context.get_selection());

	if (selection.points.empty ()) {
		return;
	}

	timepos_t earliest (earliest_time);

	/* Keep a record of the AutomationLists that we end up using in this operation */
	typedef std::map<std::shared_ptr<AutomationList>, EditingContext::AutomationRecord> Lists;
	Lists lists;

	/* user could select points in any order */
	selection.points.sort (PointsSelectionPositionSorter ());

	/* Go through all selected points, making an AutomationRecord for each distinct AutomationList */
	for (auto & selected_point : selection.points) {
		const AutomationLine& line (selected_point->line());
		const std::shared_ptr<AutomationList> al   = line.the_list();
		if (lists.find (al) == lists.end ()) {
			/* We haven't seen this list yet, so make a record for it.  This includes
			   taking a copy of its current state, in case this is needed for undo later.
			*/
			lists[al] = EditingContext::AutomationRecord (&al->get_state (), &line);
		}
	}

	if (op == Cut || op == Copy) {
		/* This operation will involve putting things in the cut buffer, so create an empty
		   ControlList for each of our source lists to put the cut buffer data in.
		*/
		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			i->second.copy = i->first->create (i->first->parameter (), i->first->descriptor(), *i->first);
		}

		/* Add all selected points to the relevant copy ControlLists */

		for (auto & selected_point : selection.points) {
			std::shared_ptr<AutomationList>    al = selected_point->line().the_list();
			AutomationList::const_iterator ctrl_evt = selected_point->model ();

			lists[al].copy->fast_simple_add ((*ctrl_evt)->when, (*ctrl_evt)->value);
			earliest = std::min (earliest, (*ctrl_evt)->when);
		}

		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			/* Correct this copy list so that it is relative to the earliest
			   start time, so relative ordering between points is preserved
			   when copying from several lists and the paste starts at the
			   earliest copied piece of data.
			*/
			std::shared_ptr<Evoral::ControlList> &al_cpy = i->second.copy;
			for (AutomationList::iterator ctrl_evt = al_cpy->begin(); ctrl_evt != al_cpy->end(); ++ctrl_evt) {
				(*ctrl_evt)->when.shift_earlier (earliest);
			}

			/* And add it to the cut buffer */
			_editing_context.get_cut_buffer().add (al_cpy);
		}
	}

	if (op == Delete || op == Cut) {
		/* This operation needs to remove things from the main AutomationList, so do that now */

		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			i->first->freeze ();
		}

		/* Remove each selected point from its AutomationList */
		for (auto & selected_point : selection.points) {
			AutomationLine& line (selected_point->line ());
			std::shared_ptr<AutomationList> al = line.the_list();
			al->erase (selected_point->model ());
		}

		/* Thaw the lists and add undo records for them */
		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			std::shared_ptr<AutomationList> al = i->first;
			al->thaw ();
			_editing_context.add_command (new MementoCommand<AutomationList> (*al.get(), i->second.state, &(al->get_state ())));
		}
	}
}

void
PianorollMidiView::cut_copy_clear_one (AutomationLine& line, ::Selection& selection, Editing::CutCopyOp op)
{
	using namespace Editing;

	std::shared_ptr<Evoral::ControlList> what_we_got;
	std::shared_ptr<AutomationList> alist (line.the_list());

	XMLNode &before = alist->get_state();

	/* convert time selection to automation list model coordinates */
	timepos_t start = selection.time.front().start().earlier (line.get_origin());
	timepos_t end = selection.time.front().end().earlier (line.get_origin());

	std::cerr << "CCCO from " << start << " .. " << end << " based on " << selection.time.front().start() << " .. " << selection.time.front().end() << std::endl;

	switch (op) {
	case Delete:
		if (alist->cut (start, end) != 0) {
			_editing_context.add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
		}
		break;

	case Cut:

		if ((what_we_got = alist->cut (start, end)) != 0) {
			_editing_context.get_cut_buffer().add (what_we_got);
			_editing_context.add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
		}
		break;
	case Copy:
		if ((what_we_got = alist->copy (start, end)) != 0) {
			_editing_context.get_cut_buffer().add (what_we_got);
		}
		break;

	case Clear:
		if ((what_we_got = alist->cut (start, end)) != 0) {
			_editing_context.add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &alist->get_state()));
		}
		break;
	}

	if (what_we_got) {
		for (AutomationList::iterator x = what_we_got->begin(); x != what_we_got->end(); ++x) {
			timepos_t when = (*x)->when;
			double val  = (*x)->value;
			val = line.model_to_view_coord_y (val);
			(*x)->when = when;
			(*x)->value = val;
		}
	}
}

void
PianorollMidiView::set_region (std::shared_ptr<MidiRegion> mr)
{
	MidiView::set_region (mr);
	remove_all_automation ();
}

void
PianorollMidiView::get_selectables (Evoral::Parameter const & param, Temporal::timepos_t const & start, Temporal::timepos_t  const & end, double x, double y, std::list<Selectable*>& results, bool within)
{
	AutomationLane* lane = automation_lane_by_param (param);

	if (!lane || !lane->line) {
		return;
	}

	return lane->line->get_selectables (start, end, x, y, results, within);
}
