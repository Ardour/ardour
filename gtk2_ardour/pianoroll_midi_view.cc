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
#include "canvas/button.h"
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

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;

PianorollMidiView::PianorollMidiView (std::shared_ptr<ARDOUR::MidiTrack> mt,
                                      ArdourCanvas::Item&      parent,
                                      ArdourCanvas::Item&      noscroll_parent,
                                      EditingContext&          ec,
                                      MidiViewBackground&      bg,
                                      uint32_t                 basic_color)
	: MidiView (mt, parent, ec, bg, basic_color)
	, _noscroll_parent (&noscroll_parent)
	, overlay_text (nullptr)
	, active_automation (nullptr)
	, velocity_display (nullptr)
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

	event_rect->Event.connect (sigc::mem_fun (*this, &PianorollMidiView::midi_canvas_group_event));
	parent.Event.connect (sigc::mem_fun (*this, &PianorollMidiView::midi_canvas_group_event));

	_note_group->raise_to_top ();

	automation_group = new ArdourCanvas::Rectangle (&parent);
	CANVAS_DEBUG_NAME (automation_group, "cue automation group");
	automation_group->set_fill_color (UIConfiguration::instance().color ("midi automation track fill"));
	automation_group->set_data ("linemerger", this);

	_show_source = true;
	_on_timeline = false;
	set_extensible (true);
}

PianorollMidiView::~PianorollMidiView ()
{
	delete velocity_display;
}

bool
PianorollMidiView::midi_canvas_group_event (GdkEvent* ev)
{
	EC_LOCAL_TEMPO_SCOPE_ARG (_editing_context);

	/* Let MidiView do its thing */

	if (MidiView::midi_canvas_group_event (ev)) {

		switch (ev->type) {
		case GDK_ENTER_NOTIFY:
		case GDK_LEAVE_NOTIFY:
			break;
		default:
			return true;
		}
	}

	return _editing_context.canvas_bg_event (ev, event_rect);
}

void
PianorollMidiView::set_height (double h)
{
	_height = h;

	double note_area_height;
	double automation_height;

	if (!have_visible_automation()) {
		note_area_height = h;
		automation_height = 0.;
	} else {
		note_area_height = ceil (2 * h / 3.);
		automation_height = ceil (h - note_area_height);
	}

	event_rect->set (ArdourCanvas::Rect (0.0, 0.0, ArdourCanvas::COORD_MAX, note_area_height));
	midi_context().set_size (midi_context().width(), note_area_height);

	automation_group->set_position (ArdourCanvas::Duple (0., note_area_height));
	automation_group->set (ArdourCanvas::Rect (0., 0., 100000000000000., automation_height));

	for (auto & ads : automation_map) {
		ads.second.set_height (automation_height);
	}

	view_changed ();
}

ArdourCanvas::Duple
PianorollMidiView::automation_group_position() const
{
	return automation_group->position();
}

AutomationLine*
PianorollMidiView::active_automation_line() const
{
	if (active_automation) {
		return active_automation->line.get();
	}

	return nullptr;
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
		overlay_text->set_position (ArdourCanvas::Duple ((pixel_width / 2.0) - overlay_text->text_width(), (_height / 2.0 - overlay_text->text_height())));
	}

	for (auto & a : automation_map) {
		if (a.second.line) {
			a.second.line->reset ();
		}
	}
}
void
PianorollMidiView::clear_ghost_events ()
{
	if (velocity_display) {
		velocity_display->clear ();
	}
}

void
PianorollMidiView::ghosts_model_changed ()
{
	if (velocity_display) {
		velocity_display->clear();
		for (auto & ev : _events) {
			velocity_display->add_note (ev.second);
		}
	}
}

void
PianorollMidiView::ghosts_view_changed ()
{
	if (velocity_display) {
		velocity_display->redisplay();
	}
}

void
PianorollMidiView::ghost_remove_note (NoteBase* nb)
{
	if (velocity_display) {
		velocity_display->remove_note (nb);
	}
}

void
PianorollMidiView::ghost_add_note (NoteBase* nb)
{
	if (velocity_display) {
		velocity_display->add_note (nb);
	}
}

void
PianorollMidiView::ghost_sync_selection (NoteBase* nb)
{
	if (velocity_display) {
		velocity_display->note_selected (nb);
	}
}

void
PianorollMidiView::update_sustained (Note* n)
{
	MidiView::update_sustained (n);
	if (velocity_display) {
		velocity_display->update_note (n);
	}
}

void
PianorollMidiView::update_hit (Hit* h)
{
	MidiView::update_hit (h);
	if (velocity_display) {
		velocity_display->update_note (h);
	}
}

void
PianorollMidiView::swap_automation_channel (int new_channel)
{
	std::vector<Evoral::Parameter> new_params;
	Evoral::Parameter active (0, 0, 0);
	bool have_active = false;

	/* Make a note of what was visible, but use the new channel */

	for (CueAutomationMap::iterator i = automation_map.begin(); i != automation_map.end(); ++i) {
		if (i->second.visible) {
			Evoral::Parameter param (i->first.type(), new_channel, i->first.id());
			new_params.push_back (param);
			if (&i->second == active_automation) {
				active = param;
				have_active = true;
			}
		}
	}

	/* Drop the old */

	automation_map.clear ();

	/* Create the new */

	for (auto const & p : new_params) {
		toggle_visibility (p);
	}

	if (have_active) {
		set_active_automation (active);
	} else {
		unset_active_automation ();
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

PianorollMidiView::AutomationDisplayState*
PianorollMidiView::find_or_create_automation_display_state (Evoral::Parameter const & param)
{
	if (!midi_region()) {
		editing_context().make_a_region();
	}

	CueAutomationMap::iterator i = automation_map.find (param);

	/* Step one: find the AutomationDisplayState object for this parameter,
	 * or create it if it does not already exist.
	 */

	if (i != automation_map.end()) {
		return &i->second;
	}

	AutomationDisplayState* ads = nullptr;
	bool was_empty = automation_map.empty();

	if (param.type() == MidiVelocityAutomation) {

		if (!velocity_display) {

			/* Create and add to automation display map */

			velocity_display = new PianorollVelocityDisplay (editing_context(), midi_context(), *this, *automation_group, 0x312244ff);
			auto res = automation_map.insert (std::make_pair (param, AutomationDisplayState (*velocity_display, false)));

			ads = &((*res.first).second);

			for (auto & ev : _events) {
				velocity_display->add_note (ev.second);
			}
		}

	} else {

		std::shared_ptr<Evoral::Control> control = _midi_region->model()->control (param, true);
		CueAutomationControl ac = std::dynamic_pointer_cast<AutomationControl> (control);

		if (!ac) {
			return nullptr;
		}

		CueAutomationLine line (new PianorollAutomationLine (ARDOUR::EventTypeMap::instance().to_symbol (param),
		                                                     _editing_context,
		                                                     *automation_group,
		                                                     automation_group,
		                                                     ac->alist(),
		                                                     ac->desc()));

		line->set_insensitive_line_color (line_color_for (param));

		AutomationDisplayState cad (ac, line, false);

		auto res = automation_map.insert (std::make_pair (param, cad));

		ads = &((*res.first).second);
	}

	if (was_empty) {
		set_height (_height);
	}

	return ads;
}

bool
PianorollMidiView::have_visible_automation () const
{
	for (auto const & [oarameter,ads] : automation_map) {
		if (ads.visible) {
			return true;
		}
	}

	return false;
}

void
PianorollMidiView::toggle_visibility (Evoral::Parameter const & param)
{
	using namespace ARDOUR;

	if (!_midi_region) {
		return;
	}

	switch (param.type()) {
	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiPitchBenderAutomation:
	case MidiChannelPressureAutomation:
	case MidiNotePressureAutomation:
	case MidiSystemExclusiveAutomation:
	case MidiVelocityAutomation:
		break;
	default:
		return;
	}

	AutomationDisplayState* ads = find_or_create_automation_display_state (param);

	if (!ads) {
		return;
	}

	if (ads->visible) {
		ads->hide ();
		if (ads == active_automation) {
			unset_active_automation ();
			return;
		}
		if (!have_visible_automation()) {
			set_height (_height);
		}
	} else {
		ads->show ();
	}

	set_height (_height);
	AutomationStateChange (); /* EMIT SIGNAL */
}

void
PianorollMidiView::set_active_automation (Evoral::Parameter const & param)
{
	AutomationDisplayState* ads = find_or_create_automation_display_state (param);

	if (ads) {
		internal_set_active_automation (*ads);
	}
}

void
PianorollMidiView::unset_active_automation ()
{
	if (!active_automation) {
		return;
	}

	CueAutomationMap::size_type visible = 0;

	for (auto & [param,ads] : automation_map) {
		if (ads.line) {
			ads.line->set_sensitive (false);
		} else {
			ads.velocity_display->set_sensitive (false);
		}

		if (ads.visible) {
			visible++;
		}
	}

	/* If the currently active automation is the only one visible, hide it */

	if (active_automation->visible && visible == 1) {
		active_automation->hide ();
		set_height (_height);
	}

	active_automation = nullptr;

	AutomationStateChange(); /* EMIT SIGNAL */
}

void
PianorollMidiView::internal_set_active_automation (AutomationDisplayState& ads)
{
	if (active_automation == &ads) {
		unset_active_automation ();
		return;
	}

	/* active automation MUST be visible and sensitive */
	bool had_visible = have_visible_automation ();
	ads.show ();
	if (!had_visible) {
		set_height (_height);
	}
	ads.set_sensitive (true);
	ads.set_height (automation_group->get().height());
	active_automation = &ads;

	/* Now desensitize the rest */

	for (auto & [param,ds] : automation_map) {
		if (&ds == &ads) {
			continue;
		}

		ds.set_sensitive (false);
	}

	AutomationStateChange(); /* EMIT SIGNAL */
}

bool
PianorollMidiView::is_active_automation (Evoral::Parameter const & param) const
{
	CueAutomationMap::const_iterator i = automation_map.find (param);

	if (i == automation_map.end()) {
		return false;
	}

	return (&i->second == active_automation);
}

bool
PianorollMidiView::is_visible_automation (Evoral::Parameter const & param) const
{
	CueAutomationMap::const_iterator i = automation_map.find (param);

	if (i == automation_map.end()) {
		return false;
	}

	return (i->second.visible);
}


std::list<SelectableOwner*>
PianorollMidiView::selectable_owners()
{
	std::list<SelectableOwner*> sl;
	if (active_automation && active_automation->line) {
		sl.push_back (active_automation->line.get());
	}
	return sl;
}

MergeableLine*
PianorollMidiView::make_merger ()
{
	if (active_automation && active_automation->line) {

		return new MergeableLine (active_automation->line, active_automation->control,
		                          [](Temporal::timepos_t const& t) { return t; },
		                          nullptr, nullptr);
	}

	return nullptr;
}

bool
PianorollMidiView::automation_rb_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
	if (!active_automation || !active_automation->control || !active_automation->line) {
		return false;
	}

	bool with_guard_points = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);
	active_automation->line->add (active_automation->control, event, pos, event->button.y, with_guard_points);
	return false;
}

bool
PianorollMidiView::velocity_rb_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
	if (!active_automation || !active_automation->control || !active_automation->velocity_display) {
		return false;
	}

	return false;
}

void
PianorollMidiView::line_drag_click (GdkEvent* event, Temporal::timepos_t const & pos)
{
}

PianorollMidiView::AutomationDisplayState::~AutomationDisplayState()
{
	/* We do not own the velocity_display */
}

void
PianorollMidiView::AutomationDisplayState::hide ()
{
	if (velocity_display) {
		velocity_display->hide ();
	} else if (line) {
		line->hide_all ();
	}
	visible = false;
}

void
PianorollMidiView::AutomationDisplayState::set_sensitive (bool yn)
{
	if (velocity_display) {
		velocity_display->set_sensitive (yn);
	} else if (line) {
		line->set_sensitive (yn);
	}
}

void
PianorollMidiView::AutomationDisplayState::show ()
{
	if (velocity_display) {
		velocity_display->show ();
	} else if (line) {
		line->show ();
	}
	visible = true;
}

void
PianorollMidiView::AutomationDisplayState::set_height (double h)
{
	if (velocity_display) {
		// velocity_display->set_height (h);
	} else if (line) {
		line->set_height (h);
	}
}

void
PianorollMidiView::automation_entry ()
{
	if (active_automation && active_automation->line) {
		active_automation->line->track_entered ();
	}
}


void
PianorollMidiView::automation_leave ()
{
	if (active_automation && active_automation->line) {
		active_automation->line->track_entered ();
	}
}

void
PianorollMidiView::point_selection_changed ()
{
	if (active_automation) {
		if (active_automation->line) {
			active_automation->line->set_selected_points (_editing_context.get_selection().points);
		}
	}
}

void
PianorollMidiView::clear_selection ()
{
	MidiView::clear_note_selection ();
	PointSelection empty;

	for (CueAutomationMap::iterator i = automation_map.begin(); i != automation_map.end(); ++i) {
		if (i->second.line) {
			i->second.line->set_selected_points (empty);
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
