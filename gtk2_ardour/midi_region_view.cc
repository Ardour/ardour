/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2017 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2017 André Nusser <andre.nusser@googlemail.com>
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
#include <algorithm>
#include <ostream>

#include <gtkmm.h>

#include "gtkmm2ext/gtk_ui.h"

#include <sigc++/signal.h>

#include "midi++/midnam_patch.h"

#include "pbd/stateful_diff_command.h"
#include "pbd/unwind.h"

#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/quantize.h"
#include "ardour/session.h"

#include "evoral/Parameter.h"
#include "evoral/Event.h"
#include "evoral/Control.h"
#include "evoral/midi_util.h"

#include "canvas/debug.h"

#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "debug.h"
#include "editor.h"
#include "editor_drag.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "item_counts.h"
#include "keyboard.h"
#include "midi_channel_dialog.h"
#include "midi_cut_buffer.h"
#include "midi_list_editor.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "midi_util.h"
#include "midi_velocity_dialog.h"
#include "note_player.h"
#include "paste_context.h"
#include "public_editor.h"
#include "route_time_axis.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "patch_change_dialog.h"
#include "velocity_ghost_region.h"
#include "verbose_cursor.h"
#include "note.h"
#include "hit.h"
#include "patch_change.h"
#include "sys_ex.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace std;
using namespace Temporal;
using Gtkmm2ext::Keyboard;

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero())?0:1)

MidiRegionView::MidiRegionView (ArdourCanvas::Container*      parent,
                                EditingContext&               ec,
                                RouteTimeAxisView&            tv,
                                std::shared_ptr<MidiRegion>   r,
                                double                        spu,
                                uint32_t                      basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, MidiView (std::dynamic_pointer_cast<MidiTrack> (tv.stripable()), *group, ec, *dynamic_cast<MidiStreamView*>(tv.view()), basic_color)
{
	connect_to_diskstream ();
}

MidiRegionView::MidiRegionView (ArdourCanvas::Container*      parent,
                                EditingContext&               ec,
                                RouteTimeAxisView&            tv,
                                std::shared_ptr<MidiRegion>   r,
                                double                        spu,
                                uint32_t                      basic_color,
                                bool                          recording,
                                TimeAxisViewItem::Visibility  visibility)
	: RegionView (parent, tv, r, spu, basic_color, recording, visibility)
	, MidiView (std::dynamic_pointer_cast<MidiTrack> (tv.stripable()), *group, ec, *dynamic_cast<MidiStreamView*>(tv.view()), basic_color)
{
	connect_to_diskstream ();
}

MidiRegionView::MidiRegionView (const MidiRegionView& other)
	: sigc::trackable(other)
	, RegionView (other)
	, MidiView (other)
{
	init (false);
}

MidiRegionView::MidiRegionView (const MidiRegionView& other, std::shared_ptr<MidiRegion> region)
	: RegionView (other, std::shared_ptr<Region> (region))
	, MidiView (other)
{
	init (true);
}

void
MidiRegionView::init (bool /*wfd*/)
{
	DisplaySuspender ds (*this, true);

	RegionView::init (false);

	CANVAS_DEBUG_NAME (_note_group, string_compose ("note group for %1", get_item_name()));

	set_region (std::dynamic_pointer_cast<MidiRegion> (_region));

	//set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();
	region_resized (ARDOUR::bounds_change);
	//region_locked ();

	set_colors ();
	reset_width_dependent_items (_pixel_width);

	_note_group->parent()->raise_to_top();

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&MidiRegionView::parameter_changed, this, _1), gui_context());
	connect_to_diskstream ();
}

bool
MidiRegionView::display_is_enabled () const
{
	return RegionView::display_enabled ();
}

void
MidiRegionView::parameter_changed (std::string const & p)
{
	RegionView::parameter_changed (p);

	if (p == "display-first-midi-bank-as-zero") {
		if (display_enabled()) {
			view_changed ();
		}
	} else if (p == "color-regions-using-track-color") {
		set_colors ();
	} else if (p == "use-note-color-for-velocity") {
		color_handler ();
	}
}

void
MidiRegionView::color_handler ()
{
	RegionView::color_handler ();
	MidiView::color_handler ();
}

void
MidiRegionView::region_resized (PBD::PropertyChange const & change)
{
	RegionView::region_resized (change);
	MidiView::region_resized (change);
}

InstrumentInfo&
MidiRegionView::instrument_info () const
{
	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
	return route_ui->route()->instrument_info();
}

const std::shared_ptr<ARDOUR::MidiRegion>
MidiRegionView::midi_region() const
{
	return _midi_region;
}

void
MidiRegionView::connect_to_diskstream ()
{
	midi_view()->midi_track()->DataRecorded.connect(
		*this, invalidator(*this),
		boost::bind (&MidiRegionView::data_recorded, this, _1),
		gui_context());
}

std::string
MidiRegionView::get_modifier_name () const
{
	const bool opaque = _region->opaque() || trackview.layer_display () == Stacked;

	std::string mod_name;

	if (_dragging) {
		mod_name = "dragging region";
	} else if (_editing_context.internal_editing()) {
		if (!opaque || _region->muted ()) {
			mod_name = "editable region";
		}
	} else {
		if (!opaque || _region->muted ()) {
			mod_name = "transparent region base";
		}
	}

	return mod_name;
}

GhostRegion*
MidiRegionView::add_ghost (TimeAxisView& tv)
{
	double unit_position = _editing_context.time_to_pixel (_region->position ());
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&tv);
	MidiGhostRegion* ghost;

	if (mtv && mtv->midi_view()) {
		return 0;
	} else {
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*>(&tv);
		if (atv && atv->parameter() == Evoral::Parameter (MidiVelocityAutomation)) {
			ghost = new VelocityGhostRegion (*this, tv, trackview, unit_position);
		} else {
			ghost = new MidiGhostRegion (*this, tv, trackview, unit_position);
		}
	}

	ghost->set_colors ();
	ghost->set_height ();
	ghost->set_duration (_region->length().samples() / samples_per_pixel);

	for (auto const & i : _events) {
		ghost->add_note (i.second);
	}

	ghosts.push_back (ghost);
	return ghost;
}


bool
MidiRegionView::canvas_group_event(GdkEvent* ev)
{
	if (in_destructor || _recregion) {
		return false;
	}

	if (!_editing_context.internal_editing()) {
		// not in internal edit mode, so just act like a normal region
		return RegionView::canvas_group_event (ev);
	}

	return MidiView::canvas_group_event (ev);
}

bool
MidiRegionView::enter_notify (GdkEventCrossing* ev)
{
	enter_internal (ev->state);

	_entered = true;
	return false;
}

bool
MidiRegionView::leave_notify (GdkEventCrossing*)
{
	leave_internal ();

	_entered = false;
	return false;
}

void
MidiRegionView::mouse_mode_changed ()
{
	// Adjust frame colour (become more transparent for internal tools)
	set_frame_color();
	MidiView::mouse_mode_changed ();
}

void
MidiRegionView::enter_internal (uint32_t state)
{
	if (_editing_context.current_mouse_mode() == MouseDraw && _mouse_state != AddDragging) {
		// Show ghost note under pencil
		create_ghost_note(_last_event_x, _last_event_y, state);
	}

	// Lower frame handles below notes so they don't steal events

	if (frame_handle_start) {
		frame_handle_start->lower_to_bottom();
	}
	if (frame_handle_end) {
		frame_handle_end->lower_to_bottom();
	}
}

void
MidiRegionView::leave_internal()
{
	hide_verbose_cursor ();
	remove_ghost_note ();
	_entered_note = 0;

	// Raise frame handles above notes so they catch events
	if (frame_handle_start) {
		frame_handle_start->raise_to_top();
	}

	if (frame_handle_end) {
		frame_handle_end->raise_to_top();
	}
}

bool
MidiRegionView::button_press (GdkEventButton* ev)
{
	if (ev->button != 1) {
		return false;
	}

	MouseMode m = _editing_context.current_mouse_mode();

	if (m == MouseContent && Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {
		_press_cursor_ctx = CursorContext::create(_editing_context, _editing_context.cursors()->midi_pencil);
	}

	if (_mouse_state != SelectTouchDragging) {

		_pressed_button = ev->button;

		if (m == MouseDraw || (m == MouseContent && Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier()))) {

			if (midi_view()->note_mode() == Percussive) {
				_editing_context.drags()->set (new HitCreateDrag (_editing_context, group, this), (GdkEvent *) ev);
			} else {
				_editing_context.drags()->set (new NoteCreateDrag (_editing_context, group, this), (GdkEvent *) ev);
			}

			_mouse_state = AddDragging;
			remove_ghost_note ();
			hide_verbose_cursor ();
		} else {
			_mouse_state = Pressed;
		}

		return true;
	}

	_pressed_button = ev->button;
	_mouse_changed_selection = false;

	return true;
}

bool
MidiRegionView::button_release (GdkEventButton* ev)
{
	double event_x, event_y;

	if (ev->button != 1) {
		return false;
	}

	event_x = ev->x;
	event_y = ev->y;

	group->canvas_to_item (event_x, event_y);
	group->ungrab ();

	_press_cursor_ctx.reset();

	switch (_mouse_state) {
	case Pressed: // Clicked

		switch (_editing_context.current_mouse_mode()) {
		case MouseRange:
			/* no motion occurred - simple click */
			clear_selection_internal ();
			_mouse_changed_selection = true;
			break;

		case MouseContent:
			_editing_context.get_selection().set (this);
			/* fallthru */
		case MouseTimeFX:
			_mouse_changed_selection = true;
			clear_selection_internal ();
			break;
		case MouseDraw:
			_editing_context.get_selection().set (this);
			break;

		default:
			break;
		}

		_mouse_state = None;
		break;

	case AddDragging:
		/* Don't a ghost note when we added a note - wait until motion to avoid visual confusion.
		   we don't want one when we were drag-selecting either. */
	case SelectRectDragging:
		_editing_context.drags()->end_grab ((GdkEvent *) ev);
		_mouse_state = None;
		break;


	default:
		break;
	}

	if (_mouse_changed_selection) {
		_editing_context.begin_reversible_selection_op (X_("Mouse Selection Change"));
		_editing_context.commit_reversible_selection_op ();
	}

	return false;
}
 
bool
MidiRegionView::motion (GdkEventMotion* ev)
{
	if (!_entered_note) {

		if (_mouse_state == AddDragging) {
			if (_ghost_note) {
				remove_ghost_note ();
			}

		} else if (!_ghost_note && _editing_context.current_mouse_mode() == MouseContent &&
		    Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier()) &&
		    _mouse_state != AddDragging) {

			create_ghost_note (ev->x, ev->y, ev->state);

		} else if (_ghost_note && _editing_context.current_mouse_mode() == MouseContent &&
			   Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {

			update_ghost_note (ev->x, ev->y, ev->state);

		} else if (_ghost_note && _editing_context.current_mouse_mode() == MouseContent) {

			remove_ghost_note ();
			hide_verbose_cursor ();

		} else if (_editing_context.current_mouse_mode() == MouseDraw) {

			if (_ghost_note) {
				update_ghost_note (ev->x, ev->y, ev->state);
			} else {
				create_ghost_note (ev->x, ev->y, ev->state);
			}
		}
	}

	/* any motion immediately hides velocity text that may have been visible */

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	switch (_mouse_state) {
	case Pressed:

		if (_pressed_button == 1) {

			MouseMode m = _editing_context.current_mouse_mode();

			if (m == MouseContent && !Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {
				_editing_context.drags()->set (new MidiRubberbandSelectDrag (_editing_context, this), (GdkEvent *) ev);
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
					clear_selection_internal ();
					_mouse_changed_selection = true;
				}
				_mouse_state = SelectRectDragging;
				return true;
			} else if (m == MouseRange) {
				_editing_context.drags()->set (new MidiVerticalSelectDrag (_editing_context, this), (GdkEvent *) ev);
				_mouse_state = SelectVerticalDragging;
				return true;
			}
		}

		return false;

	case SelectRectDragging:
	case SelectVerticalDragging:
	case AddDragging:
		_editing_context.drags()->motion_handler ((GdkEvent *) ev, false);
		break;

	case SelectTouchDragging:
		return false;

	default:
		break;

	}

	//let RegionView do it's thing.  drags are handled in here
	return RegionView::canvas_group_event ((GdkEvent *) ev);
}


bool
MidiRegionView::scroll (GdkEventScroll* ev)
{
	if (_editing_context.drags()->active()) {
		return false;
	}

	if (!_editing_context.get_selection().selected (this)) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier) ||
	    Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		/* XXX: bit of a hack; allow PrimaryModifier+TertiaryModifier scroll
		 * through so that it still works for navigation and zoom.
		 */
		return false;
	}

	if (_selection.empty()) {
		const int step = 1;
		const bool zoom = Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier);
		const bool just_one_edge = Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier|Keyboard::PrimaryModifier);

		switch (ev->direction) {
		case GDK_SCROLL_UP:
			if (just_one_edge) {
				/* make higher notes visible aka expand higher pitch range */
				midi_stream_view()->apply_note_range (midi_stream_view()->lowest_note(), min (127, midi_stream_view()->highest_note() + step), true);
			} else if (zoom) {
				/* zoom out to show more higher and lower pitches */
				midi_stream_view()->apply_note_range (max (0, midi_stream_view()->lowest_note() - step), min (127, midi_stream_view()->highest_note() + step), true);
			} else {
				/* scroll towards higher pitches */
				midi_stream_view()->apply_note_range (max (0, midi_stream_view()->lowest_note() + step), min (127, midi_stream_view()->highest_note() + step), true);
			}
			return true;

		case GDK_SCROLL_DOWN:
			if (just_one_edge) {
				/* make lower notes visible aka expand lower pitch range */
				midi_stream_view()->apply_note_range (max (0, midi_stream_view()->lowest_note() - step), midi_stream_view()->highest_note(), true);
			} else if (zoom) {
				/* zoom in to show less higher and lower pitches */
				midi_stream_view()->apply_note_range (min (127, midi_stream_view()->lowest_note() + step), max (0, midi_stream_view()->highest_note() - step), true);
			} else {
				/* scroll towards lower pitches */
				midi_stream_view()->apply_note_range (min (127, midi_stream_view()->lowest_note() - step), max (0, midi_stream_view()->highest_note() - step), true);
			}
			return true;

		default:
			break;
		}

		return false;
	}

	hide_verbose_cursor ();

	if (UIConfiguration::instance().get_scroll_velocity_editing()) {
		bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
		Keyboard::ModifierMask mask_together(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier);
		bool together = Keyboard::modifier_state_contains (ev->state, mask_together);

		if (ev->direction == GDK_SCROLL_UP) {
			change_velocities (true, fine, false, together);
		} else if (ev->direction == GDK_SCROLL_DOWN) {
			change_velocities (false, fine, false, together);
		} else {
			/* left, right: we don't use them */
			return false;
		}

		return true;
	}

	return false;
}

void
MidiRegionView::ghosts_view_changed ()
{
	for (auto & g : ghosts) {
		MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (g);
		if (gr && !gr->trackview.hidden()) {
			gr->view_changed ();
		}
	}
}

MidiRegionView::~MidiRegionView ()
{
	in_destructor = true;
	RegionViewGoingAway (this); /* EMIT_SIGNAL */
}

void
MidiRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);

	view_changed ();

	bool hide_all = false;
	PatchChanges::iterator x = _patch_changes.begin();
	if (x != _patch_changes.end()) {
		hide_all = x->second->width() >= _pixel_width;
	}

	if (hide_all) {
		for (; x != _patch_changes.end(); ++x) {
			x->second->hide();
		}
	}

	move_step_edit_cursor (_step_edit_cursor_position);
	set_step_edit_cursor_width (_step_edit_cursor_width);
}

void
MidiRegionView::set_height (double height)
{
	MidiView::set_height (height);
	RegionView::set_height(height);
}
void
MidiRegionView::set_selected (bool selected)
{
	if (!selected) {
		clear_selection_internal ();
	}

	RegionView::set_selected (selected);
}

void
MidiRegionView::ghost_sync_selection (NoteBase* ev)
{
	for (auto & ghost : ghosts) {

		MidiGhostRegion* gr;

		if ((gr = dynamic_cast<MidiGhostRegion*>(ghost)) != 0) {
			gr->note_selected (ev);
		}
	}
}

uint32_t
MidiRegionView::get_fill_color() const
{
	Gtkmm2ext::Color c;
	if (_selected) {
		c = UIConfiguration::instance().color ("selected region base");
	} else if ((!UIConfiguration::instance().get_show_name_highlight() || high_enough_for_name) && !UIConfiguration::instance().get_color_regions_using_track_color()) {
		c = UIConfiguration::instance().color (fill_color_name);
	} else {
		c = fill_color;
	}

	string mod_name = get_modifier_name();

	if (mod_name.empty ()) {
		return c;
	} else {
		return UIConfiguration::instance().color_mod (c, mod_name);
	}
}

double
MidiRegionView::height() const
{
	return TimeAxisViewItem::height();
}

void
MidiRegionView::redisplay (bool view_only)
{
	MidiView::redisplay (view_only);
}

ArdourCanvas::Item*
MidiRegionView::drag_group () const
{
	return get_canvas_group ();
}

void
MidiRegionView::select_self (bool  add)
{
	if (add) {
		_editing_context.get_selection().add (this);
	} else {
		_editing_context.get_selection().set (this);
	}
}

void
MidiRegionView::unselect_self ()
{
	_editing_context.get_selection().remove (this);
}

void
MidiRegionView::begin_drag_edit (std::string const & why)
{
	if (!_selected) {
		/* unclear why gcc can't understand which version of
		   select_self() to use here, but so be it.
		*/
		MidiView::select_self ();
	}
	// start_note_diff_command (why);
}

void
MidiRegionView::select_self_uniquely ()
{
	_editing_context.set_selected_midi_region_view (*this);
}
