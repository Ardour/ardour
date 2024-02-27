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

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "debug.h"
#include "editing_context.h"
#include "editor_drag.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "item_counts.h"
#include "keyboard.h"
#include "midi_channel_dialog.h"
#include "midi_cut_buffer.h"
#include "midi_list_editor.h"
#include "midi_view.h"
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

namespace ARDOUR {
class MidiTrack;
}

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace std;
using namespace Temporal;
using Gtkmm2ext::Keyboard;

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero())?0:1)

MidiView::MidiView (std::shared_ptr<MidiTrack> mt,
                    ArdourCanvas::Item&        parent,
                    EditingContext&            ec,
                    MidiViewBackground&        bg,
                    uint32_t                   basic_color)
	: _midi_track (mt)
	, _editing_context (ec)
	, _midi_context (bg)
	, _active_notes (0)
	, _note_group (new ArdourCanvas::Container (&parent))
	, _note_diff_command (nullptr)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1, 0)
	, _channel_selection_scoped_note (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _optimization_iterator (_events.end())
	, _list_editor (nullptr)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _entered (false)
	, _entered_note (0)
	, _select_all_notes_after_add (false)
	, _mouse_changed_selection (false)
	, split_tuple (0)
	, note_splitting (false)
{
	init ();
}


MidiView::MidiView (MidiView const & other)
	: sigc::trackable ()
	, _midi_track (other._midi_track)
	, _editing_context (other.editing_context())
	, _midi_context (other.midi_context())
	, _midi_region (other.midi_region())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (other._note_group->parent()))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1, 0)
	, _channel_selection_scoped_note (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _entered (false)
	, _entered_note (0)
	, _select_all_notes_after_add (false)
	, _mouse_changed_selection (false)
	, split_tuple (0)
	, note_splitting (false)
{
	init ();
}

void
MidiView::init ()
{
	_patch_change_outline = UIConfiguration::instance().color ("midi patch change outline");
	_patch_change_fill = UIConfiguration::instance().color_mod ("midi patch change fill", "midi patch change fill");

	_note_group->raise_to_top();
	EditingContext::DropDownKeys.connect (sigc::mem_fun (*this, &MidiView::drop_down_keys));
}

void
MidiView::set_region (std::shared_ptr<MidiRegion> mr)
{
	_midi_region = mr;
	if (!_midi_region) {
		_model.reset ();
		connections_requiring_model.drop_connections();
		return;
	}
	set_model (_midi_region->midi_source (0)->model());
}

void
MidiView::set_model (std::shared_ptr<MidiModel> m)
{
	_model = m;
	assert (_model);

	//set_height (trackview.current_height());

/* XXXX
	region_muted ();
	region_sync_changed ();
	region_resized (ARDOUR::bounds_change);
	//region_locked ();

	set_colors ();
	reset_width_dependent_items (_pixel_width);
*/

	connections_requiring_model.drop_connections ();

	_model->ContentsChanged.connect (connections_requiring_model, invalidator (*this), boost::bind (&MidiView::model_changed, this), gui_context());

	_midi_track->playback_filter().ChannelModeChanged.connect (connections_requiring_model, invalidator (*this),
	                                                                         boost::bind (&MidiView::midi_channel_mode_changed, this),
	                                                                         gui_context ());

	_midi_track->instrument_info().Changed.connect (connections_requiring_model, invalidator (*this),
	                                   boost::bind (&MidiView::instrument_settings_changed, this), gui_context());

	_editing_context.SnapChanged.connect (connections_requiring_model, invalidator(*this),
	                                       boost::bind (&MidiView::snap_changed, this),
	                                       gui_context());

	_editing_context.MouseModeChanged.connect (connections_requiring_model, invalidator (*this),
	                                            boost::bind (&MidiView::mouse_mode_changed, this),
	                                            gui_context ());

	model_changed ();
}

bool
MidiView::canvas_group_event (GdkEvent* ev)
{
	//For now, move the snapped cursor aside so it doesn't bother you during internal editing
	//_editing_context.set_snapped_cursor_position(_midi_region->position());

	bool r;

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		_last_event_x = ev->crossing.x;
		_last_event_y = ev->crossing.y;
		enter_notify (&ev->crossing);
		// set entered_regionview (among other things)
		return true;

	case GDK_LEAVE_NOTIFY:
		_last_event_x = ev->crossing.x;
		_last_event_y = ev->crossing.y;
		leave_notify (&ev->crossing);
		// reset entered_regionview (among other things)
		return true;

	case GDK_SCROLL:
		if (scroll (&ev->scroll)) {
			return true;
		}
		break;

	case GDK_KEY_PRESS:
		return key_press (&ev->key);

	case GDK_KEY_RELEASE:
		return key_release (&ev->key);

	case GDK_BUTTON_PRESS:
		return button_press (&ev->button);

	case GDK_BUTTON_RELEASE:
		r = button_release (&ev->button);
		return r;

	case GDK_MOTION_NOTIFY:
		_last_event_x = ev->motion.x;
		_last_event_y = ev->motion.y;
		return motion (&ev->motion);

	default:
		break;
	}

	return false;
}

bool
MidiView::enter_notify (GdkEventCrossing* ev)
{
	enter_internal (ev->state);

	_entered = true;
	return false;
}

bool
MidiView::leave_notify (GdkEventCrossing*)
{
	leave_internal ();

	_entered = false;
	return false;
}

void
MidiView::mouse_mode_changed ()
{
	// Adjust frame colour (become more transparent for internal tools)
	// XXX	set_frame_color();

	if (!_editing_context.internal_editing()) {

		/* Switched out of internal editing mode while entered.
		   Only necessary for leave as a mouse_mode_change over a region
		   automatically triggers an enter event.
		*/

		leave_internal ();

		for (Events::iterator it = _events.begin(); it != _events.end(); ++it) {
			it->second->set_hide_selection (true);
		}

	} else if (_editing_context.current_mouse_mode() == MouseContent) {

		// hide cursor and ghost note after changing to internal edit mode

		remove_ghost_note ();

		/* XXX This is problematic as the function is executed for every region
		   and only for one region _entered_note can be true. Still it's
		   necessary as to hide the verbose cursor when we're changing from
		   draw mode to internal edit mode. These lines are the reason why
		   in some situations no verbose cursor is shown when we enter internal
		   edit mode over a note.
		*/

		if (!_entered_note) {
			hide_verbose_cursor ();
		}

		for (Events::iterator it = _events.begin(); it != _events.end(); ++it) {
			it->second->set_hide_selection (false);
		}
	}
}

void
MidiView::enter_internal (uint32_t state)
{
	if (_editing_context.current_mouse_mode() == MouseDraw && _mouse_state != AddDragging) {
		// Show ghost note under pencil
		create_ghost_note(_last_event_x, _last_event_y, state);
	}
}

void
MidiView::leave_internal()
{
	hide_verbose_cursor ();
	remove_ghost_note ();
	_entered_note = 0;
}

bool
MidiView::button_press (GdkEventButton* ev)
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

			if (_midi_context.note_mode() == Percussive) {
				_editing_context.drags()->set (new HitCreateDrag (_editing_context, drag_group(), this), (GdkEvent *) ev);
			} else {
				_editing_context.drags()->set (new NoteCreateDrag (_editing_context, drag_group(), this), (GdkEvent *) ev);
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

	return false;
}

bool
MidiView::button_release (GdkEventButton* ev)
{
	double event_x, event_y;

	if (ev->button != 1) {
		return false;
	}

	event_x = ev->x;
	event_y = ev->y;

	_note_group->parent()->canvas_to_item (event_x, event_y);
	_note_group->parent()->ungrab ();

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
		case MouseTimeFX:
			_mouse_changed_selection = true;
			clear_selection_internal ();
			break;
		case MouseDraw:
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
MidiView::motion (GdkEventMotion* ev)
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
				MidiRubberbandSelectDrag* mrbsd = new MidiRubberbandSelectDrag (_editing_context, this);
				mrbsd->set_bounding_item (_editing_context.get_trackview_group());
				_editing_context.drags()->set (mrbsd, (GdkEvent *) ev);

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

	return false;
}


bool
MidiView::scroll (GdkEventScroll* ev)
{
	if (_editing_context.drags()->active()) {
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
				set_note_range (_midi_context.lowest_note(), min (127, _midi_context.highest_note() + step));
			} else if (zoom) {
				/* zoom out to show more higher and lower pitches */
				set_note_range (max (0, _midi_context.lowest_note() - step), min (127, _midi_context.highest_note() + step));
			} else {
				/* scroll towards higher pitches */
				set_note_range (max (0, _midi_context.lowest_note() + step), min (127, _midi_context.highest_note() + step));
			}
			return true;

		case GDK_SCROLL_DOWN:
			if (just_one_edge) {
				/* make lower notes visible aka expand lower pitch range */
				set_note_range (max (0, _midi_context.lowest_note() - step), _midi_context.highest_note());
			} else if (zoom) {
				/* zoom in to show less higher and lower pitches */
				set_note_range (min (127, _midi_context.lowest_note() + step), max (0, _midi_context.highest_note() - step));
			} else {
				/* scroll towards lower pitches */
				set_note_range (min (127, _midi_context.lowest_note() - step), max (0, _midi_context.highest_note() - step));
			}
			return true;

		case GDK_SCROLL_LEFT:
			_editing_context.set_horizontal_position (_editing_context.horizontal_position() - 20.0);
			break;

		case GDK_SCROLL_RIGHT:
			_editing_context.set_horizontal_position (_editing_context.horizontal_position() + 20.0);
			break;

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

bool
MidiView::key_press (GdkEventKey* ev)
{
	/* since GTK bindings are generally activated on press, and since
	   detectable auto-repeat is the name of the game and only sends
	   repeated presses, carry out key actions at key press, not release.
	*/

	if (Keyboard::no_modifier_keys_pressed(ev) && (ev->keyval == GDK_Alt_L || ev->keyval == GDK_Alt_R)) {

		if (_mouse_state != AddDragging) {
			_mouse_state = SelectTouchDragging;
		}

		return true;
	}

	return false;
}

bool
MidiView::key_release (GdkEventKey* ev)
{
	if ((_mouse_state == SelectTouchDragging) && (ev->keyval == GDK_Alt_L || ev->keyval == GDK_Alt_R)) {
		_mouse_state = None;
		return true;
	}
	return false;
}

void
MidiView::channel_edit ()
{
	if (_selection.empty()) {
		return;
	}

	/* pick a note somewhat at random (since Selection is a set<>) to
	 * provide the "current" channel for the dialog.
	 */

	uint8_t current_channel = (*_selection.begin())->note()->channel ();
	MidiChannelDialog channel_dialog (current_channel);
	int ret = channel_dialog.run ();

	switch (ret) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}

	uint8_t new_channel = channel_dialog.active_channel ();

	start_note_diff_command (_("channel edit"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_channel (*i, new_channel);
		i = next;
	}

	apply_note_diff ();
}

void
MidiView::velocity_edit ()
{
	if (_selection.empty()) {
		return;
	}

	/* pick a note somewhat at random (since Selection is a set<>) to
	 * provide the "current" velocity for the dialog.
	 */

	uint8_t current_velocity = (*_selection.begin())->note()->velocity ();
	MidiVelocityDialog velocity_dialog (current_velocity);
	int ret = velocity_dialog.run ();

	switch (ret) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}

	uint8_t new_velocity = velocity_dialog.velocity ();

	start_note_diff_command (_("velocity edit"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_velocity (*i, new_velocity, false);
		i = next;
	}

	apply_note_diff ();
}

void
MidiView::show_list_editor ()
{
	if (!_list_editor) {
		_list_editor = new MidiListEditor (_editing_context.session(), midi_region(), _midi_track);
	}
	_list_editor->present ();
}


/** Add a note to the model, and the view, at a canvas (click) coordinate.
 * \param t time in samples relative to the position of the region
 * \param y vertical position in pixels
 * \param length duration of the note in beats
 * \param snap_t true to snap t to the grid, otherwise false.
 */
void
MidiView::create_note_at (timepos_t const & t, double y, Temporal::Beats length, uint32_t state, bool shift_snap)
{
	if (length < Temporal::Beats::one_tick()) {
		return;
	}

	/* assume time is already region-relative and snapped */

	Temporal::Beats region_start = t.beats();

	const double  note     = y_to_note(y);
	const uint8_t chan     = get_channel_for_add(region_start);
	const uint8_t velocity = get_velocity_for_add (region_start);

	const std::shared_ptr<NoteType> new_note (new NoteType (chan, region_start, length, (uint8_t)note, velocity));

	if (_model->contains (new_note)) {
		return;
	}

	_midi_context.maybe_extend_note_range (new_note->note());

	start_note_diff_command(_("add note"));
	note_diff_add_note (new_note, true, false);
	apply_note_diff();

	// XXX _editing_context.set_selected_midi_region_view (*this);
	list<Evoral::event_id_t> to_be_selected;
	to_be_selected.push_back (new_note->id());
	select_notes (to_be_selected, true);

	play_midi_note (new_note);
}

void
MidiView::clear_events ()
{
	// clear selection without signaling or trying to change state of event objects
	_selection.clear ();

	clear_ghost_events ();

	_note_group->clear (true);
	_events.clear();
	_patch_changes.clear();
	_sys_exes.clear();
	_optimization_iterator = _events.end();
}

void
MidiView::display_model (std::shared_ptr<MidiModel> model)
{
	set_model (_model);
	/* Don't signal as nobody else needs to know until selection has been altered. */
	clear_events();
	model_changed ();
}

void
MidiView::start_note_diff_command (string name)
{
	if (!_note_diff_command) {
		_editing_context.begin_reversible_command (name);
		_note_diff_command = _model->new_note_diff_command (name);
	} else {
		std::cerr << "ERROR: start_note_diff_command command called, but a note_diff_command was already underway" << std::endl;
	}
}

void
MidiView::note_diff_add_note (const std::shared_ptr<NoteType> note, bool selected, bool show_velocity)
{
	if (_note_diff_command) {
		_note_diff_command->add (note);
	}
	if (selected) {
		_marked_for_selection.insert(note);
	}
	if (show_velocity) {
		_marked_for_velocity.insert(note);
	}
}

void
MidiView::note_diff_remove_note (NoteBase* ev)
{
	if (_note_diff_command && ev->note()) {
		_note_diff_command->remove(ev->note());
	}
}

void
MidiView::note_diff_add_change (NoteBase* ev,
                                      MidiModel::NoteDiffCommand::Property property,
                                      uint8_t val)
{
	if (_note_diff_command) {
		_note_diff_command->change (ev->note(), property, val);
	}
}

void
MidiView::note_diff_add_change (NoteBase* ev,
                                      MidiModel::NoteDiffCommand::Property property,
                                      Temporal::Beats val)
{
	if (_note_diff_command) {
		_note_diff_command->change (ev->note(), property, val);
	}
}

void
MidiView::apply_note_diff (bool as_subcommand, bool was_copy)
{
	if (!_note_diff_command) {
		return;
	}

	bool add_or_remove = _note_diff_command->adds_or_removes();

	if (!was_copy && add_or_remove) {
		// Mark all selected notes for selection when model reloads
		for (auto const & sel : _selection) {
			_marked_for_selection.insert (sel->note());
		}
	}

	{
		PBD::Unwinder<bool> puw (_select_all_notes_after_add, true);
		/*note that we don't use as_commit here, because that would BEGIN a new undo record; we already have one underway*/
		_model->apply_diff_command_as_subcommand (*_editing_context.session(), _note_diff_command);
	}

	if (!as_subcommand) {
		_editing_context.commit_reversible_command ();  /*instead, we can explicitly commit the command in progress */
	}

	_note_diff_command = nullptr;

	if (add_or_remove && !was_copy) {
		_marked_for_selection.clear();
	}

	_marked_for_velocity.clear();
}

void
MidiView::abort_note_diff()
{
	delete _note_diff_command;
	_note_diff_command = 0;
	_editing_context.abort_reversible_command();
	clear_selection_internal ();
}

NoteBase*
MidiView::find_canvas_note (std::shared_ptr<NoteType> note)
{

	if (_optimization_iterator != _events.end()) {
		++_optimization_iterator;
	}

	if (_optimization_iterator != _events.end() && _optimization_iterator->first == note) {
		return _optimization_iterator->second;
	}

	_optimization_iterator = _events.find (note);
	if (_optimization_iterator != _events.end()) {
		return _optimization_iterator->second;
	}

	return 0;
}

/** This version finds any canvas note matching the supplied note. */
NoteBase*
MidiView::find_canvas_note (Evoral::event_id_t id)
{
	Events::iterator it;

	for (it = _events.begin(); it != _events.end(); ++it) {
		if (it->first->id() == id) {
			return it->second;
		}
	}

	return 0;
}

std::shared_ptr<PatchChange>
MidiView::find_canvas_patch_change (MidiModel::PatchChangePtr p)
{
	PatchChanges::const_iterator f = _patch_changes.find (p);

	if (f != _patch_changes.end()) {
		return f->second;
	}

	return std::shared_ptr<PatchChange>();
}

std::shared_ptr<SysEx>
MidiView::find_canvas_sys_ex (MidiModel::SysExPtr s)
{
	SysExes::const_iterator f = _sys_exes.find (s);

	if (f != _sys_exes.end()) {
		return f->second;
	}

	return std::shared_ptr<SysEx>();
}

void
MidiView::get_events (Events& e, Evoral::Sequence<Temporal::Beats>::NoteOperator op, uint8_t val, int chan_mask)
{
	MidiModel::Notes notes;
	_model->get_notes (notes, op, val, chan_mask);

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
		NoteBase* cne = find_canvas_note (*n);
		if (cne) {
			e.insert (make_pair (*n, cne));
		}
	}
}

void
MidiView::redisplay (bool view_only)
{
	if (view_only) {
		PropertyChange what_changed;

		/* we don't know what actually happened that requires a view
		   update, but both _start and _length need to be shown
		   correctly on the display, so make sure we do that.
		*/

		what_changed.add (Properties::start);
		what_changed.add (Properties::length);

		/* this calls reset_width_dependent_items() which calls
		   view_changed() in the right way.
		*/
		region_resized (what_changed);
	} else {
		model_changed ();
	}
}

void
MidiView::model_changed()
{
	if (!display_is_enabled()) {
		return;
	}

	if (_active_notes) {
		// Currently recording
		const samplecnt_t zoom = _editing_context.get_current_zoom();
		if (zoom != _last_display_zoom) {
			/* Update resolved canvas notes to reflect changes in zoom without
			   touching model.  Leave active notes (with length max) alone since
			   they are being extended. */
			for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
				if (i->second->note()->end_time() != std::numeric_limits<Temporal::Beats>::max()) {
					update_note(i->second);
				}
			}
			_last_display_zoom = zoom;
		}
		return;
	}

	if (!_model) {
		return;
	}

	for (_optimization_iterator = _events.begin(); _optimization_iterator != _events.end(); ++_optimization_iterator) {
		_optimization_iterator->second->invalidate();
	}

	bool empty_when_starting = _events.empty();
	_optimization_iterator = _events.begin();
	MidiModel::Notes missing_notes;
	Note* sus = NULL;
	Hit*  hit = NULL;

	MidiModel::ReadLock lock(_model->read_lock());
	MidiModel::Notes& notes (_model->notes());

	NoteBase* cne;

	if (_midi_context.visibility_range_style() == MidiViewBackground::ContentsRange) {

		uint8_t low_note = std::numeric_limits<uint8_t>::max();
		uint8_t hi_note = std::numeric_limits<uint8_t>::min();

		for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
			if ((*n)->note() < low_note) {
				low_note = (*n)->note();
			}
			if ((*n)->note() > hi_note)  {
				hi_note = (*n)->note();
			}
		}

		set_note_range (low_note, hi_note);
	}

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		std::shared_ptr<NoteType> note (*n);
		bool visible;

		if (note_in_region_range (note, visible)) {
			if (!empty_when_starting && (cne = find_canvas_note (note)) != 0) {
				cne->validate ();
				if (visible) {
					cne->show ();
				} else {
					cne->hide ();
				}
			} else {
				missing_notes.insert (note);
			}
		}
	}

	if (!empty_when_starting) {
		MidiModel::Notes::iterator f;
		for (Events::iterator i = _events.begin(); i != _events.end(); ) {

			NoteBase* cne = i->second;

			/* remove note items that are no longer valid */
			if (!cne->valid()) {

				ghost_remove_note (cne);

				delete cne;
				i = _events.erase (i);

			} else {

				bool visible;

				if (note_in_region_range (cne->note(), visible)) {

					if (visible) {
						cne->item()->show ();

						if ((sus = dynamic_cast<Note*>(cne))) {
							update_sustained (sus);
						} else if ((hit = dynamic_cast<Hit*>(cne))) {
							update_hit (hit);
						}
					} else {
						cne->item()->hide ();
					}

				} else {

					cne->item()->hide ();
				}

				++i;
			}
		}
	}

	for (MidiModel::Notes::iterator n = missing_notes.begin(); n != missing_notes.end(); ++n) {
		std::shared_ptr<NoteType> note (*n);
		NoteBase* cne;
		bool visible;

		if (note_in_region_range (note, visible)) {
			cne = add_note (note, visible);
		} else {
			cne = add_note (note, false);
		}

		for (set<Evoral::event_id_t>::iterator it = _pending_note_selection.begin(); it != _pending_note_selection.end(); ++it) {
			if ((*it) == note->id()) {
				add_to_selection (cne);
				break;
			}
		}
	}

	ghosts_model_changed ();

	display_sysexes();
	display_patch_changes ();

	_marked_for_selection.clear ();
	_marked_for_velocity.clear ();
	_pending_note_selection.clear ();
}

void
MidiView::view_changed()
{
	if (!display_is_enabled()) {
		return;
	}

	if (_active_notes) {
		// Currently recording
		const samplecnt_t zoom = _editing_context.get_current_zoom();
		if (zoom != _last_display_zoom) {
			/* Update resolved canvas notes to reflect changes in zoom without
			   touching model.  Leave active notes (with length max) alone since
			   they are being extended. */
			for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
				if (i->second->note()->end_time() != std::numeric_limits<Temporal::Beats>::max()) {
					update_note(i->second);
				}
			}
			_last_display_zoom = zoom;
		}
		return;
	}

	if (!_model) {
		return;
	}

	Note* sus = NULL;
	Hit*  hit = NULL;

	for (Events::iterator i = _events.begin(); i != _events.end(); ) {

		NoteBase* cne = i->second;
		bool visible;

		if (note_in_region_range (cne->note(), visible)) {

			if (visible) {
				cne->item()->show ();

				if ((sus = dynamic_cast<Note*>(cne))) {
					update_sustained (sus);
				} else if ((hit = dynamic_cast<Hit*>(cne))) {
					update_hit (hit);
				}
			} else {
				cne->item()->hide ();
			}

		} else {

			cne->item()->hide ();
		}

		++i;
	}

	ghosts_view_changed ();

	update_sysexes();
	update_patch_changes ();
}

void
MidiView::display_patch_changes ()
{
	uint16_t chn_mask = _midi_track->get_playback_channel_mask();

	for (uint8_t i = 0; i < 16; ++i) {
		display_patch_changes_on_channel (i, chn_mask & (1 << i));
	}
}

/** @param active_channel true to display patch changes fully, false to display
 * them `greyed-out' (as on an inactive channel)
 */
void
MidiView::display_patch_changes_on_channel (uint8_t channel, bool active_channel)
{
	for (MidiModel::PatchChanges::const_iterator i = _model->patch_changes().begin(); i != _model->patch_changes().end(); ++i) {
		std::shared_ptr<PatchChange> p;

		if ((*i)->channel() != channel) {
			continue;
		}

		if ((p = find_canvas_patch_change (*i)) != 0) {

			const timepos_t region_time = _midi_region->source_beats_to_region_time ((*i)->time());
			if (region_time < timepos_t() || region_time >= _midi_region->length()) {
				p->hide();
			} else {
				const timepos_t flag_time = _midi_region->source_beats_to_absolute_time ((*i)->time());
				const double flag_x = _editing_context.time_to_pixel (flag_time);

				const double region_x = _editing_context.time_to_pixel (_midi_region->position());

				p->canvas_item()->set_position (ArdourCanvas::Duple (flag_x-region_x, 1.0));
				p->update_name ();

				p->show();
		}

		} else {
			add_canvas_patch_change (*i);
		}
	}
}

void
MidiView::update_patch_changes ()
{
	for (PatchChanges::iterator p = _patch_changes.begin(); p != _patch_changes.end(); ++p) {

		std::shared_ptr<PatchChange> pc (p->second);

		const timepos_t region_time (_midi_region->source_beats_to_region_time (p->first->time()));

		if (region_time < timepos_t() || region_time >= _midi_region->length()) {
			pc->hide();
		} else {
			const timepos_t flag_time = _midi_region->source_beats_to_absolute_time (p->first->time());
			const double flag_x = _editing_context.time_to_pixel (flag_time);

			const double region_x = _editing_context.time_to_pixel (_midi_region->position());

			pc->canvas_item()->set_position (ArdourCanvas::Duple (flag_x-region_x, 1.0));
			pc->update_name ();
			pc->show();
		}
	}
}

void
MidiView::display_sysexes()
{
	bool have_periodic_system_messages = false;
	bool display_periodic_messages = true;

	if (!UIConfiguration::instance().get_never_display_periodic_midi()) {

		for (MidiModel::SysExes::const_iterator i = _model->sysexes().begin(); i != _model->sysexes().end(); ++i) {
			if ((*i)->is_spp() || (*i)->is_mtc_quarter() || (*i)->is_mtc_full()) {
				have_periodic_system_messages = true;
				break;
			}
		}

		if (have_periodic_system_messages) {
			double zoom = _editing_context.get_current_zoom (); // samples per pixel

			/* get an approximate value for the number of samples per video frame */

			double video_frame = _editing_context.session()->sample_rate() * (1.0/30);

			/* if we are zoomed out beyond than the cutoff (i.e. more
			 * samples per pixel than samples per 4 video frames), don't
			 * show periodic sysex messages.
			 */

			if (zoom > (video_frame*4)) {
				display_periodic_messages = false;
			}
		}
	} else {
		display_periodic_messages = false;
	}

	const std::shared_ptr<MidiRegion> mregion (midi_region());

	for (MidiModel::SysExes::const_iterator i = _model->sysexes().begin(); i != _model->sysexes().end(); ++i) {
		MidiModel::SysExPtr sysex_ptr = *i;
		timepos_t time = timepos_t (sysex_ptr->time());

		if ((*i)->is_spp() || (*i)->is_mtc_quarter() || (*i)->is_mtc_full()) {
			if (!display_periodic_messages) {
				continue;
			}
		}

		ostringstream str;
		str << hex;
		for (uint32_t b = 0; b < (*i)->size(); ++b) {
			str << int((*i)->buffer()[b]);
			if (b != (*i)->size() -1) {
				str << " ";
			}
		}
		string text = str.str();

		const double x = _editing_context.time_to_pixel (_midi_region->source_beats_to_region_time (time.beats()));

		double height = _midi_context.contents_height();

		// CAIROCANVAS: no longer passing *i (the sysex event) to the
		// SysEx canvas object!!!
		std::shared_ptr<SysEx> sysex = find_canvas_sys_ex (sysex_ptr);

		if (!sysex) {
			sysex = std::shared_ptr<SysEx>(new SysEx (_note_group, text, height, x, 1.0, sysex_ptr));
			_sys_exes.insert (make_pair (sysex_ptr, sysex));
		} else {
			sysex->set_height (height);
			sysex->item().set_position (ArdourCanvas::Duple (x, 1.0));
		}

		// Show unless message is beyond the region bounds
		if (_midi_region->source_relative_position (time) >= _midi_region->length() || time < _midi_region->start()) {
			sysex->hide();
		} else {
			sysex->show();
		}
	}
}

void
MidiView::update_sysexes ()
{
	double height = _midi_context.contents_height();

	for (SysExes::iterator s = _sys_exes.begin(); s != _sys_exes.end(); ++s) {

		const timepos_t time (s->first->time());
		std::shared_ptr<SysEx> sysex (s->second);

		// Show unless message is beyond the region bounds
		if (_midi_region->source_relative_position (time) >= _midi_region->length() || time < _midi_region->start()) {
			sysex->hide();
			continue;
		} else {
			sysex->show();
		}

		const double x = _editing_context.time_to_pixel (_midi_region->source_beats_to_region_time (time.beats()));

		sysex->set_height (height);
		sysex->item().set_position (ArdourCanvas::Duple (x, 1.0));
	}
}

MidiView::~MidiView ()
{
	hide_verbose_cursor ();

	delete _list_editor;

	if (_active_notes) {
		end_write();
	}
	_entered_note = 0;
	clear_events ();

	delete _note_group;
	delete _note_diff_command;
	delete _step_edit_cursor;
}

void
MidiView::region_resized (const PropertyChange& what_changed)
{
	// XXX RegionView::region_resized(what_changed); // calls RegionView::set_duration()
}

void
MidiView::reset_width_dependent_items (double pixel_width)
{
	view_changed ();

	bool hide_all = false;
	PatchChanges::iterator x = _patch_changes.begin();
	if (x != _patch_changes.end()) {
		hide_all = x->second->width() >= pixel_width;
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
MidiView::set_height (double ht)
{
	if (ht != height()) {
		view_changed ();
	}

	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		(*x).second->set_height (_midi_context.contents_height());
	}

	if (_step_edit_cursor) {
		_step_edit_cursor->set_y1 (_midi_context.contents_height());
	}
}


/** Apply the current note range from the stream view
 * by repositioning/hiding notes as necessary
 */
void
MidiView::apply_note_range (uint8_t min, uint8_t max, bool force)
{
	view_changed ();
}

/** Begin tracking note state for successive calls to add_event
 */
void
MidiView::begin_write()
{
	if (_active_notes) {
		delete[] _active_notes;
	}
	_active_notes = new Note*[128];
	for (unsigned i = 0; i < 128; ++i) {
		_active_notes[i] = 0;
	}
}


/** Destroy note state for add_event
 */
void
MidiView::end_write()
{
	delete[] _active_notes;
	_active_notes = 0;
	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}

/** Extend active notes to rightmost edge of region (if length is changed)
 */
void
MidiView::extend_active_notes()
{
	if (!_active_notes) {
		return;
	}

	for (unsigned i = 0; i < 128; ++i) {
		if (_active_notes[i]) {
			_active_notes[i]->set_x1 (_editing_context.duration_to_pixels (_midi_region->length()));
		}
	}
}

void
MidiView::play_midi_note(std::shared_ptr<NoteType> note)
{
	if (_no_sound_notes || !UIConfiguration::instance().get_sound_midi_notes()) {
		return;
	}

	NotePlayer* np = new NotePlayer (_midi_track);
	np->add (note);
	np->play ();

	/* NotePlayer deletes itself */
}

void
MidiView::start_playing_midi_note(std::shared_ptr<NoteType> note)
{
	const std::vector< std::shared_ptr<NoteType> > notes(1, note);
	start_playing_midi_chord(notes);
}

void
MidiView::start_playing_midi_chord (vector<std::shared_ptr<NoteType> > notes)
{
	if (_no_sound_notes || !UIConfiguration::instance().get_sound_midi_notes()) {
		return;
	}

	NotePlayer* player = new NotePlayer (_midi_track);

	for (vector<std::shared_ptr<NoteType> >::iterator n = notes.begin(); n != notes.end(); ++n) {
		player->add (*n);
	}

	player->play ();
}

bool
MidiView::note_in_region_time_range (const std::shared_ptr<NoteType> note) const
{
	const std::shared_ptr<ARDOUR::MidiRegion> midi_reg = midi_region();
	return  (timepos_t (note->time()) >= _midi_region->start()) && (timepos_t (note->time()) < _midi_region->start() + _midi_region->length());
}

bool
MidiView::note_in_region_range (const std::shared_ptr<NoteType> note, bool& visible) const
{
	const std::shared_ptr<ARDOUR::MidiRegion> midi_reg = midi_region();

	const bool outside = !note_in_region_time_range (note);

	visible = (note->note() >= _midi_context.lowest_note()) && (note->note() <= _midi_context.highest_note());

	return !outside;
}

void
MidiView::update_note (NoteBase* note, bool update_ghost_regions)
{
	Note* sus = NULL;
	Hit*  hit = NULL;
	if ((sus = dynamic_cast<Note*>(note))) {
		update_sustained(sus, update_ghost_regions);
	} else if ((hit = dynamic_cast<Hit*>(note))) {
		update_hit(hit, update_ghost_regions);
	}
}

/** Update a canvas note's size from its model note.
 *  @param ev Canvas note to update.
 *  @param update_ghost_regions true to update the note in any ghost regions that we have, otherwise false.
 */
void
MidiView::update_sustained (Note* ev, bool update_ghost_regions)
{
	const std::shared_ptr<ARDOUR::MidiRegion> mr = midi_region();
	std::shared_ptr<NoteType> note = ev->note();
	const timepos_t note_start (note->time());
	timepos_t note_end (note->end_time());

	/* The note is drawn as a child item of this region view, so its
	 * coordinate system is relative to the region view. This means that x0
	 * and x1 are pixel offsets relative to beginning of the region (view)
	 */

	/* compute absolute time where the start of the source is
	 */

	const timepos_t session_source_start = _midi_region->source_position();

	/* this computes the number of samples from the start of the region of the start of the
	 * note. We add the source start to get to the absolute time of the
	 * note, then subtract the start of the region
	 */

	const samplepos_t note_start_samples = _midi_region->position().distance ((note_start + session_source_start)).samples();

	const double x0 = _editing_context.sample_to_pixel (note_start_samples);
	double x1;

	const double y0 = 1 + floor(note_to_y(note->note()));
	double y1;

	if (note->length() == Temporal::Beats()) {

		/* special case actual zero-length notes */

		x1 = x0 + 1.;

	} else if (note->end_time() != std::numeric_limits<Temporal::Beats>::max()) {

		/* normal note */

		const Temporal::Beats source_end ((_midi_region->start() + _midi_region->length()).beats());

		if (note->end_time() > source_end) {
			note_end = timepos_t (source_end);
		}

		const samplepos_t note_end_samples = _midi_region->position().distance ((session_source_start + note_end)).samples();

		x1 = std::max(1., _editing_context.sample_to_pixel (note_end_samples));

	} else {

		/* nascent note currently being recorded, noteOff has not yet arrived */

		x1 = std::max(1., _editing_context.duration_to_pixels (_midi_region->length()));
	}

	y1 = y0 + std::max(1., floor(note_height()) - 1);

	ev->set (ArdourCanvas::Rect (x0, y0, x1, y1));
	ev->set_velocity (note->velocity()/127.0);

	if (note->end_time() == std::numeric_limits<Temporal::Beats>::max())  {
		if (_active_notes && note->note() < 128) {
			Note* const old_rect = _active_notes[note->note()];
			if (old_rect) {
				/* There is an active note on this key, so we have a stuck
				   note.  Finish the old rectangle here. */
				old_rect->set_x1 (x1);
				old_rect->set_outline_all ();
			}
			_active_notes[note->note()] = ev;
		}
		/* outline all but right edge */
		ev->set_outline_what (ArdourCanvas::Rectangle::What (
					      ArdourCanvas::Rectangle::TOP|
					      ArdourCanvas::Rectangle::LEFT|
					      ArdourCanvas::Rectangle::BOTTOM));
	} else {
		/* outline all edges */
		ev->set_outline_all ();
	}

	// Update color in case velocity has changed
	const uint32_t base_col = ev->base_color();
	ev->set_fill_color(base_col);
	ev->set_outline_color(ev->calculate_outline(base_col, ev->selected()));
}

void
MidiView::update_hit (Hit* ev, bool update_ghost_regions)
{
	std::shared_ptr<NoteType> note = ev->note();
	const timepos_t note_time = _midi_region->source_beats_to_absolute_time (note->time());

	const double x = _editing_context.time_to_pixel(note_time) - _editing_context.time_to_pixel (_midi_region->position());
	const double diamond_size = std::max(1., floor(note_height()) - 2.);
	const double y = 1.5 + floor(note_to_y(note->note())) + diamond_size * .5;

	if (y <= 0 || y >= height()) {
		ev->hide();
	} else {
		ev->show();
	}

	ev->set_position (ArdourCanvas::Duple (x, y));
	ev->set_height (diamond_size);

	// Update color in case velocity has changed
	const uint32_t base_col = ev->base_color();
	ev->set_fill_color(base_col);
	ev->set_outline_color(ev->calculate_outline(base_col, ev->selected()));
}

/** Add a MIDI note to the view (with length).
 *
 * If in sustained mode, notes with an end at numeric_limits<Beats>::max() will be
 * considered active notes, and resolve_note should be called when the
 * corresponding note off event arrives, to properly display the note.
 */
NoteBase*
MidiView::add_note(const std::shared_ptr<NoteType> note, bool visible)
{
	NoteBase* event = 0;

	if (_midi_context.note_mode() == Sustained) {

		Note* ev_rect = new Note (*this, _note_group, note); // XXX may leak

		update_sustained (ev_rect);

		event = ev_rect;

	} else if (_midi_context.note_mode() == Percussive) {

		const double diamond_size = std::max(1., floor(note_height()) - 2.);

		Hit* ev_diamond = new Hit (*this, _note_group, diamond_size, note); // XXX may leak

		update_hit (ev_diamond);

		event = ev_diamond;

	} else {
		event = 0;
	}

	if (event) {
		ghost_add_note (event);

		if (_select_all_notes_after_add) {
			note_selected (event, true);
		} else {
			if (_marked_for_selection.find(note) != _marked_for_selection.end()) {
				note_selected (event, false);
			}
		}

		if (_marked_for_velocity.find(note) != _marked_for_velocity.end()) {
			event->show_velocity();
		}

		event->on_channel_selection_change (get_selected_channels());
		_events.insert (make_pair (event->note(), event));

		if (visible) {
			event->show();
		} else {
			event->hide ();
		}
	}

	_midi_context.maybe_extend_note_range (note->note());
	return event;
}

void
MidiView::step_add_note (uint8_t channel, uint8_t number, uint8_t velocity,
                               Temporal::Beats pos, Temporal::Beats len)
{
	std::shared_ptr<NoteType> new_note (new NoteType (channel, pos, len, number, velocity));

	/* potentially extend region to hold new note */

	timepos_t note_end = _midi_region->source_beats_to_absolute_time (new_note->end_time());
	timepos_t region_end = _midi_region->nt_last();

	if (note_end > region_end) {
		_midi_region->set_length (timecnt_t (note_end.earlier (_midi_region->position()), timepos_t()));
	}

	_midi_context.maybe_extend_note_range (new_note->note());

	_marked_for_selection.clear ();

	start_note_diff_command (_("step add"));

	clear_selection_internal ();
	note_diff_add_note (new_note, true, false);

	apply_note_diff();

	// last_step_edit_note = new_note;
}

void
MidiView::step_sustain (Temporal::Beats beats)
{
	change_note_lengths (false, false, beats, false, true);
}

/** Add a new patch change flag to the canvas.
 * @param patch the patch change to add
 * @param the text to display in the flag
 * @param active_channel true to display the flag as on an active channel, false to grey it out for an inactive channel.
 */
void
MidiView::add_canvas_patch_change (MidiModel::PatchChangePtr patch)
{
	timecnt_t off (_midi_region->source_beats_to_region_time (patch->time()), _midi_region->position());
	const double x = _editing_context.duration_to_pixels (off);
	double const height = _midi_context.contents_height();

	// CAIROCANVAS: active_channel info removed from PatcChange constructor
	// so we need to do something more sophisticated to keep its color
	// appearance (MidiPatchChangeFill/MidiPatchChangeInactiveChannelFill)
	// up to date.
	std::shared_ptr<PatchChange> patch_change = std::shared_ptr<PatchChange>(
		new PatchChange(*this, _note_group->parent(),
				height, x, 1.0,
		                _midi_track->instrument_info(),
				patch,
				_patch_change_outline,
				_patch_change_fill)
		);

	_patch_changes.insert (make_pair (patch, patch_change));
}

void
MidiView::remove_canvas_patch_change (PatchChange* pc)
{
	/* remove the canvas item */
	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		if (x->second->patch() == pc->patch()) {
			_patch_changes.erase (x);
			break;
		}
	}
}

MIDI::Name::PatchPrimaryKey
MidiView::patch_change_to_patch_key (MidiModel::PatchChangePtr p)
{
	return MIDI::Name::PatchPrimaryKey (p->program(), p->bank());
}

/// Return true iff @p pc applies to the given time on the given channel.
static bool
patch_applies (const ARDOUR::MidiModel::constPatchChangePtr pc, Temporal::Beats time, uint8_t channel)
{
	return pc->time() <= time && pc->channel() == channel;
}

void
MidiView::get_patch_key_at (Temporal::Beats time, uint8_t channel, MIDI::Name::PatchPrimaryKey& key) const
{
	// The earliest event not before time
	MidiModel::PatchChanges::iterator i = _model->patch_change_lower_bound (time);

	// Go backwards until we find the latest PC for this channel, or the start
	while (i != _model->patch_changes().begin() &&
	       (i == _model->patch_changes().end() ||
	        !patch_applies(*i, time, channel))) {
		--i;
	}

	if (i != _model->patch_changes().end() && patch_applies(*i, time, channel)) {
		key.set_bank((*i)->bank());
		key.set_program((*i)->program ());
	} else {
		key.set_bank(0);
		key.set_program(0);
	}
}

void
MidiView::change_patch_change (PatchChange& pc, const MIDI::Name::PatchPrimaryKey& new_patch)
{
	string name = _("alter patch change");

	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (name);

	if (pc.patch()->program() != new_patch.program()) {
		c->change_program (pc.patch (), new_patch.program());
	}

	int const new_bank = new_patch.bank();
	if (pc.patch()->bank() != new_bank) {
		c->change_bank (pc.patch (), new_bank);
	}

	_model->apply_diff_command_as_commit (*_editing_context.session(), c);

	remove_canvas_patch_change (&pc);
	display_patch_changes ();
}

void
MidiView::change_patch_change (MidiModel::PatchChangePtr old_change, const Evoral::PatchChange<Temporal::Beats> & new_change)
{
	string name = _("alter patch change");

	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (name);

	if (old_change->time() != new_change.time()) {
		c->change_time (old_change, new_change.time());
	}

	if (old_change->channel() != new_change.channel()) {
		c->change_channel (old_change, new_change.channel());
	}

	if (old_change->program() != new_change.program()) {
		c->change_program (old_change, new_change.program());
	}

	if (old_change->bank() != new_change.bank()) {
		c->change_bank (old_change, new_change.bank());
	}

	_model->apply_diff_command_as_commit (*_editing_context.session(), c);

	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		if (x->second->patch() == old_change) {
			_patch_changes.erase (x);
			break;
		}
	}

	display_patch_changes ();
}

/** Add a patch change to the region.
 *  @param t Time in samples relative to region position
 *  @param patch Patch to add; time and channel are ignored (time is converted from t, and channel comes from
 *  MidiTimeAxisView::get_preferred_midi_channel())
 */
void
MidiView::add_patch_change (timecnt_t const & t, Evoral::PatchChange<Temporal::Beats> const & patch)
{
	string name = _("add patch change");

	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (name);

	c->add (MidiModel::PatchChangePtr (
		        new Evoral::PatchChange<Temporal::Beats>
		        (_midi_region->source_relative_position (_midi_region->position() + t).beats(),
		         patch.channel(), patch.program(), patch.bank())));

	_model->apply_diff_command_as_commit (*_editing_context.session(), c);

	display_patch_changes ();
}

void
MidiView::move_patch_change (PatchChange& pc, Temporal::Beats t)
{
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (_("move patch change"));
	c->change_time (pc.patch (), t);
	_model->apply_diff_command_as_commit (*_editing_context.session(), c);

	display_patch_changes ();
}

void
MidiView::delete_patch_change (PatchChange* pc)
{
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (_("delete patch change"));
	c->remove (pc->patch ());
	_model->apply_diff_command_as_commit (*_editing_context.session(), c);

	remove_canvas_patch_change (pc);
	display_patch_changes ();
}

void
MidiView::step_patch (PatchChange& patch, bool bank, int delta)
{
	MIDI::Name::PatchPrimaryKey key = patch_change_to_patch_key(patch.patch());
	if (bank) {
		key.set_bank(key.bank() + delta);
	} else {
		key.set_program(key.program() + delta);
	}
	change_patch_change(patch, key);
}

void
MidiView::note_deleted (NoteBase* cne)
{
	if (_entered_note && cne == _entered_note) {
		_entered_note = 0;
	}

	if (_selection.empty()) {
		return;
	}

	_selection.erase (cne);
}

void
MidiView::delete_selection()
{
	if (_selection.empty()) {
		return;
	}

	if (_editing_context.drags()->active()) {
		return;
	}

	start_note_diff_command (_("delete selection"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected()) {
			_note_diff_command->remove((*i)->note());
		}
	}

	_selection.clear();

	apply_note_diff ();

	hide_verbose_cursor ();
}

void
MidiView::delete_note (std::shared_ptr<NoteType> n)
{
	start_note_diff_command (_("delete note"));
	_note_diff_command->remove (n);
	apply_note_diff ();

	hide_verbose_cursor ();
}

void
MidiView::clear_selection ()
{
	clear_note_selection ();
	_mouse_state = None;
	end_note_splitting ();
}

void
MidiView::clear_selection_internal ()
{
	DEBUG_TRACE(DEBUG::Selection, "MRV::clear_selection_internal\n");

	for (auto & sel : _selection) {
		sel->set_selected(false);
		sel->hide_velocity();
		ghost_sync_selection (sel);
	}
	_selection.clear();
}

void
MidiView::clear_note_selection ()
{
	clear_selection_internal ();
	unselect_self ();
}

void
MidiView::unique_select(NoteBase* ev)
{
	clear_selection ();
	add_to_selection(ev);
}

void
MidiView::select_all_notes ()
{
	PBD::Unwinder<bool> uw (_no_sound_notes, true);
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		add_to_selection (i->second);
	}
}

void
MidiView::select_range (timepos_t const & start, timepos_t const & end)
{
	PBD::Unwinder<bool> uw (_no_sound_notes, true);
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		timepos_t t = _midi_region->source_beats_to_absolute_time (i->first->time());
		if (t >= start && t <= end) {
			add_to_selection (i->second);
		}
	}
}

void
MidiView::extend_selection ()
{
	if (_selection.empty()) {
		return;
	}

	PBD::Unwinder<bool> uw (_no_sound_notes, true);

	/* find end of current selection */

	timepos_t first_note_start = timepos_t::max (BeatTime);

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		timepos_t e (_midi_region->source_beats_to_absolute_beats ((*i)->note()->time()));
		if (e < first_note_start) {
			first_note_start = e;
		}
	}

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		timepos_t t (_midi_region->source_beats_to_absolute_beats(i->first->time()));

		if (i->second->selected()) {
			continue;
		}

		if (t >= first_note_start) {
			add_to_selection (i->second);
		}
	}
}

void
MidiView::invert_selection ()
{
	PBD::Unwinder<bool> uw (_no_sound_notes, true);
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if (i->second->selected()) {
			remove_from_selection(i->second);
		} else {
			add_to_selection (i->second);
		}
	}
}

/** Used for selection undo/redo.
    The requested notes most likely won't exist in the view until the next model redisplay.
*/
void
MidiView::select_notes (list<Evoral::event_id_t> notes, bool allow_audition)
{
	NoteBase* cne;
	list<Evoral::event_id_t>::iterator n;

	PBD::Unwinder<bool> uw (_no_sound_notes, allow_audition ? _no_sound_notes : true);

	for (n = notes.begin(); n != notes.end(); ++n) {
		if ((cne = find_canvas_note(*n)) != 0) {
			add_to_selection (cne);
		} else {
			_pending_note_selection.insert(*n);
		}
	}
}

void
MidiView::select_matching_notes (uint8_t notenum, uint16_t channel_mask, bool add, bool extend)
{
	uint8_t low_note = 127;
	uint8_t high_note = 0;
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();

	if (_selection.empty()) {
		extend = false;
	}

	if (!add && !extend && !_selection.empty()) {
		clear_note_selection ();
	}

	/* scan existing selection to get note range */

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->note()->note() < low_note) {
			low_note = (*i)->note()->note();
		}
		if ((*i)->note()->note() > high_note) {
			high_note = (*i)->note()->note();
		}
	}

	if (!add) {

		if (!extend && (low_note == high_note) && (high_note == notenum)) {
			/* only note previously selected is the one we are
			 * reselecting. treat this as cancelling the selection.
			 */
			return;
		}
	}

	if (extend) {
		low_note = min (low_note, notenum);
		high_note = max (high_note, notenum);
	}

	PBD::Unwinder<bool> uw (_no_sound_notes, true);

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		std::shared_ptr<NoteType> note (*n);
		NoteBase* cne;
		bool select = false;

		if (((1 << note->channel()) & channel_mask) != 0) {
			if (extend) {
				if ((note->note() >= low_note && note->note() <= high_note)) {
					select = true;
				}
			} else if (note->note() == notenum) {
				select = true;
			}
		}

		if (select) {
			if ((cne = find_canvas_note (note)) != 0) {
				// extend is false because we've taken care of it,
				// since it extends by time range, not pitch.
				note_selected (cne, add, false);
			}
		}

		add = true; // we need to add all remaining matching notes, even if the passed in value was false (for "set")

	}
}

void
MidiView::toggle_matching_notes (uint8_t notenum, uint16_t channel_mask)
{
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		std::shared_ptr<NoteType> note (*n);
		NoteBase* cne;

		if (note->note() == notenum && (((0x0001 << note->channel()) & channel_mask) != 0)) {
			if ((cne = find_canvas_note (note)) != 0) {
				if (cne->selected()) {
					note_deselected (cne);
				} else {
					note_selected (cne, true, false);
				}
			}
		}
	}
}

void
MidiView::note_selected (NoteBase* ev, bool add, bool extend)
{
	if (!extend) {

		if (!add) {
			clear_selection_internal ();
		}

		add_to_selection (ev);
		return;

	} else {
		/* find end of latest note selected, select all between that and the start of "ev" */

		Temporal::Beats earliest = std::numeric_limits<Temporal::Beats>::max();
		Temporal::Beats latest   = Temporal::Beats();

		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->end_time() > latest) {
				latest = (*i)->note()->end_time();
			}
			if ((*i)->note()->time() < earliest) {
				earliest = (*i)->note()->time();
			}
		}

		if (ev->note()->end_time() > latest) {
			latest = ev->note()->end_time();
		}

		if (ev->note()->time() < earliest) {
			earliest = ev->note()->time();
		}

		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {

			/* find notes entirely within OR spanning the earliest..latest range */

			if ((i->first->time() >= earliest && i->first->end_time() <= latest) ||
			    (i->first->time() <= earliest && i->first->end_time() >= latest)) {
				add_to_selection (i->second);
			}
		}
	}
}

void
MidiView::note_deselected(NoteBase* ev)
{
	remove_from_selection (ev);
}

void
MidiView::update_drag_selection(timepos_t const & start, timepos_t const & end, double gy0, double gy1, bool extend)
{
	// Convert to local coordinates
	const double     y  = _midi_context.y_position();
	const double     x0 = _editing_context.sample_to_pixel_unrounded (max<samplepos_t>(0, _midi_region->region_relative_position (start).samples()));
	const double     x1 = _editing_context.sample_to_pixel_unrounded (max<samplepos_t>(0, _midi_region->region_relative_position (end).samples()));
	const double     y0 = max(0.0, gy0 - y);
	const double     y1 = max(0.0, gy1 - y);

	// TODO: Make this faster by storing the last updated selection rect, and only
	// adjusting things that are in the area that appears/disappeared.
	// We probably need a tree to be able to find events in O(log(n)) time.

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if (i->second->x0() < x1 && i->second->x1() > x0 && i->second->y0() < y1 && i->second->y1() > y0) {
			// Rectangles intersect
			if (!i->second->selected()) {
				add_to_selection (i->second);
			}
		} else if (i->second->selected() && !extend) {
			// Rectangles do not intersect
			remove_from_selection (i->second);
		}
	}

	typedef RouteTimeAxisView::AutomationTracks ATracks;
	typedef std::list<Selectable*>              Selectables;


#warning paul fix me MRV/MV
#if 0
/* Add control points to selection. */
	const ATracks& atracks = midi_view()->automation_tracks();
	Selectables    selectables;
	_editing_context.get_selection().clear_points();

	timepos_t st (start);
	timepos_t et (end);

	for (ATracks::const_iterator a = atracks.begin(); a != atracks.end(); ++a) {
		a->second->get_selectables (st, et, gy0, gy1, selectables);
		for (Selectables::const_iterator s = selectables.begin(); s != selectables.end(); ++s) {
			ControlPoint* cp = dynamic_cast<ControlPoint*>(*s);
			if (cp) {
				_editing_context.get_selection().add(cp);
			}
		}
		a->second->set_selected_points(_editing_context.get_selection().points);
	}
#endif
}

void
MidiView::update_vertical_drag_selection (double y1, double y2, bool extend)
{
	if (y1 > y2) {
		swap (y1, y2);
	}

	// TODO: Make this faster by storing the last updated selection rect, and only
	// adjusting things that are in the area that appears/disappeared.
	// We probably need a tree to be able to find events in O(log(n)) time.

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((i->second->y1() >= y1 && i->second->y1() <= y2)) {
			// within y- (note-) range
			if (!i->second->selected()) {
				add_to_selection (i->second);
			}
		} else if (i->second->selected() && !extend) {
			remove_from_selection (i->second);
		}
	}
}

void
MidiView::remove_from_selection (NoteBase* ev)
{
	Selection::iterator i = _selection.find (ev);

	if (i != _selection.end()) {
		_selection.erase (i);
	}

	ev->set_selected (false);
	ev->hide_velocity ();

	ghost_sync_selection (ev);

	if (_selection.empty()) {
		unselect_self ();
	}
}

void
MidiView::add_to_selection (NoteBase* ev)
{
	if (_selection.empty()) {

		/* we're about to select a note/some notes. Obey rule that only
		 * 1 thing can be selected by clearing any current selection
		 */

		_editing_context.get_selection().clear ();

		/* first note selected in this region, force Editor region
		 * selection to this region.
		 *
		 * this breaks the "only 1 type of thing selected" rule, but
		 * having the region selected allows "operations applied to
		 * selected MIDI regions" to work. And we can only select notes
		 * when in internal edit mode, so we know that operations will
		 * only apply to notes anyway, not regions.
		 */

		select_self_uniquely ();
	}

	if (_selection.insert (ev).second == true) {
		ev->set_selected (true);
		start_playing_midi_note ((ev)->note());
		ghost_sync_selection (ev);
	}
}

Temporal::Beats
MidiView::earliest_in_selection ()
{
	Temporal::Beats earliest = std::numeric_limits<Temporal::Beats>::max();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->note()->time() < earliest) {
			earliest = (*i)->note()->time();
		}
	}

	return earliest;
}

void
MidiView::move_selection(timecnt_t const & dx_qn, double dy, double cumulative_dy)
{
	typedef vector<std::shared_ptr<NoteType> > PossibleChord;
	PossibleChord to_play;
	Temporal::Beats earliest = earliest_in_selection();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		NoteBase* n = *i;
		if (n->note()->time() == earliest) {
			to_play.push_back (n->note());
		}
		Temporal::Beats const note_time_qn = _midi_region->source_beats_to_absolute_beats (n->note()->time());
		double dx = 0.0;

		if (_midi_context.note_mode() == Sustained) {
			dx = _editing_context.time_to_pixel_unrounded (timepos_t (note_time_qn + dx_qn.beats()))
				- n->item()->item_to_canvas (ArdourCanvas::Duple (n->x0(), 0)).x;
		} else {
			/* Hit::x0() is offset by _position.x, unlike Note::x0() */
			Hit* hit = dynamic_cast<Hit*>(n);
			if (hit) {
				dx = _editing_context.time_to_pixel_unrounded (timepos_t (note_time_qn + dx_qn.beats()))
					- n->item()->item_to_canvas (ArdourCanvas::Duple (((hit->x0() + hit->x1()) / 2.0) - hit->position().x, 0)).x;
			}
		}

		(*i)->move_event(dx, dy);

		/* update length */
		if (_midi_context.note_mode() == Sustained) {
			Note* sus = dynamic_cast<Note*> (*i);
			double const len_dx = _editing_context.time_to_pixel_unrounded (timepos_t (note_time_qn) + dx_qn + timecnt_t (n->note()->length()));

			sus->set_x1 (n->item()->canvas_to_item (ArdourCanvas::Duple (len_dx, 0)).x);
		}
	}

	if (dy && !_selection.empty() && !_no_sound_notes && UIConfiguration::instance().get_sound_midi_notes()) {

		if (to_play.size() > 1) {

			PossibleChord shifted;

			for (PossibleChord::iterator n = to_play.begin(); n != to_play.end(); ++n) {
				std::shared_ptr<NoteType> moved_note (new NoteType (**n));
				moved_note->set_note (moved_note->note() + cumulative_dy);
				shifted.push_back (moved_note);
			}

			start_playing_midi_chord (shifted);

		} else if (!to_play.empty()) {

			std::shared_ptr<NoteType> moved_note (new NoteType (*to_play.front()));
			moved_note->set_note (moved_note->note() + cumulative_dy);
			start_playing_midi_note (moved_note);
		}
	}
}

NoteBase*
MidiView::copy_selection (NoteBase* primary)
{
	_copy_drag_events.clear ();

	if (_selection.empty()) {
		return 0;
	}

	NoteBase* note;
	NoteBase* ret = 0;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		std::shared_ptr<NoteType> g (new NoteType (*((*i)->note())));
		if (_midi_context.note_mode() == Sustained) {
			Note* n = new Note (*this, _note_group, g);
			update_sustained (n, false);
			note = n;
		} else {
			Hit* h = new Hit (*this, _note_group, 10, g);
			update_hit (h, false);
			note = h;
		}

		if ((*i) == primary) {
			ret = note;
		}

		_copy_drag_events.push_back (note);
	}

	return ret;
}

void
MidiView::move_copies (timecnt_t const & dx_qn, double dy, double cumulative_dy)
{
	typedef vector<std::shared_ptr<NoteType> > PossibleChord;
	PossibleChord to_play;
	Temporal::Beats earliest = earliest_in_selection();

	for (CopyDragEvents::iterator i = _copy_drag_events.begin(); i != _copy_drag_events.end(); ++i) {
		NoteBase* n = *i;
		if (n->note()->time() == earliest) {
			to_play.push_back (n->note());
		}

		timepos_t const note_time_qn = _midi_region->source_beats_to_absolute_time (n->note()->time());
		double_t dx = 0;

		if (_midi_context.note_mode() == Sustained) {
			dx = _editing_context.time_to_pixel_unrounded (timepos_t (note_time_qn) + dx_qn)
				- n->item()->item_to_canvas (ArdourCanvas::Duple (n->x0(), 0)).x;
		} else {
			Hit* hit = dynamic_cast<Hit*>(n);
			if (hit) {
				dx = _editing_context.time_to_pixel_unrounded (timepos_t (note_time_qn) + dx_qn)
					- n->item()->item_to_canvas (ArdourCanvas::Duple (((hit->x0() + hit->x1()) / 2.0) - hit->position().x, 0)).x;
			}
		}

		(*i)->move_event(dx, dy);

		if (_midi_context.note_mode() == Sustained) {
			Note* sus = dynamic_cast<Note*> (*i);
			double const len_dx = _editing_context.time_to_pixel_unrounded (timepos_t (note_time_qn) + dx_qn + timecnt_t (n->note()->length()));

			sus->set_x1 (n->item()->canvas_to_item (ArdourCanvas::Duple (len_dx, 0)).x);
		}
	}

	if (dy && !_copy_drag_events.empty() && !_no_sound_notes && UIConfiguration::instance().get_sound_midi_notes()) {

		if (to_play.size() > 1) {

			PossibleChord shifted;

			for (PossibleChord::iterator n = to_play.begin(); n != to_play.end(); ++n) {
				std::shared_ptr<NoteType> moved_note (new NoteType (**n));
				moved_note->set_note (moved_note->note() + cumulative_dy);
				shifted.push_back (moved_note);
			}

			start_playing_midi_chord (shifted);

		} else if (!to_play.empty()) {

			std::shared_ptr<NoteType> moved_note (new NoteType (*to_play.front()));
			moved_note->set_note (moved_note->note() + cumulative_dy);
			start_playing_midi_note (moved_note);
		}
	}
}

void
MidiView::note_dropped (NoteBase *, timecnt_t const & d_qn, int8_t dnote, bool copy)
{
	uint8_t lowest_note_in_selection  = 127;
	uint8_t highest_note_in_selection = 0;
	uint8_t highest_note_difference   = 0;

	if (!copy) {
		// find highest and lowest notes first

		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			uint8_t pitch = (*i)->note()->note();
			lowest_note_in_selection  = std::min(lowest_note_in_selection,  pitch);
			highest_note_in_selection = std::max(highest_note_in_selection, pitch);
		}

		/*
		  cerr << "dnote: " << (int) dnote << endl;
		  cerr << "lowest note (streamview): " << int(_midi_context.lowest_note())
		  << " highest note (streamview): " << int(_midi_context.highest_note()) << endl;
		  cerr << "lowest note (selection): " << int(lowest_note_in_selection) << " highest note(selection): "
		  << int(highest_note_in_selection) << endl;
		  cerr << "selection size: " << _selection.size() << endl;
		  cerr << "Highest note in selection: " << (int) highest_note_in_selection << endl;
		*/

		// Make sure the note pitch does not exceed the MIDI standard range
		if (highest_note_in_selection + dnote > 127) {
			highest_note_difference = highest_note_in_selection - 127;
		}

		start_note_diff_command (_("move notes"));

		for (auto & sel : _selection) {

			Temporal::Beats new_time = sel->note()->time() + d_qn.beats ();

			if (new_time < Temporal::Beats()) {
				continue;
			}

			note_diff_add_change (sel, MidiModel::NoteDiffCommand::StartTime, new_time);

			uint8_t original_pitch = sel->note()->note();
			uint8_t new_pitch      = original_pitch + dnote - highest_note_difference;

			// keep notes in standard midi range
			clamp_to_0_127(new_pitch);

			lowest_note_in_selection  = std::min(lowest_note_in_selection,  new_pitch);
			highest_note_in_selection = std::max(highest_note_in_selection, new_pitch);

			note_diff_add_change (sel, MidiModel::NoteDiffCommand::NoteNumber, new_pitch);
		}
	} else {

		clear_selection_internal ();

		for (CopyDragEvents::iterator i = _copy_drag_events.begin(); i != _copy_drag_events.end(); ++i) {
			uint8_t pitch = (*i)->note()->note();
			lowest_note_in_selection  = std::min(lowest_note_in_selection,  pitch);
			highest_note_in_selection = std::max(highest_note_in_selection, pitch);
		}

		// Make sure the note pitch does not exceed the MIDI standard range
		if (highest_note_in_selection + dnote > 127) {
			highest_note_difference = highest_note_in_selection - 127;
		}

		start_note_diff_command (_("copy notes"));

		for (auto & copy_event : _copy_drag_events) {

			/* update time */
			Temporal::Beats new_time = copy_event->note()->time() + d_qn.beats();

			if (new_time < Temporal::Beats()) {
				continue;
			}

			copy_event->note()->set_time (new_time);

			/* update pitch */

			uint8_t original_pitch = copy_event->note()->note();
			uint8_t new_pitch      = original_pitch + dnote - highest_note_difference;

			copy_event->note()->set_note (new_pitch);

			// keep notes in standard midi range
			clamp_to_0_127(new_pitch);

			lowest_note_in_selection  = std::min(lowest_note_in_selection,  new_pitch);
			highest_note_in_selection = std::max(highest_note_in_selection, new_pitch);

			note_diff_add_note (copy_event->note(), true);

			delete copy_event;
		}

		_copy_drag_events.clear ();
	}

	std::cerr << "DROP & EDIT\n";

	apply_note_diff (true /*subcommand, we don't want this to start a new commit*/, copy);
	_editing_context.commit_reversible_command ();

	// care about notes being moved beyond the upper/lower bounds on the canvas
	if (lowest_note_in_selection  < _midi_context.lowest_note() ||
	    highest_note_in_selection > _midi_context.highest_note()) {
		_midi_context.set_note_visibility_range_style (MidiStreamView::ContentsRange);
	}
}

/** @param x Pixel relative to the region position.
 *  @param ensure_snap defaults to false. true = snap always, ignoring snap mode and magnetic snap.
 *  Used for inverting the snap logic with key modifiers and snap delta calculation.
 *  @return Snapped time relative to the region position.
 */
timecnt_t
MidiView::snap_pixel_to_time (double x, bool ensure_snap)
{
	return _editing_context.snap_relative_time_to_relative_time (_midi_region->position(), timecnt_t (_editing_context.pixel_to_sample (x)), ensure_snap);
}

/** @param x Pixel relative to the region position.
 *  @param ensure_snap defaults to false. true = ignore magnetic snap and snap mode (used for snap delta calculation).
 *  @return Snapped pixel relative to the region position.
 */
double
MidiView::snap_to_pixel(double x, bool ensure_snap)
{
	return (double) _editing_context.sample_to_pixel (snap_pixel_to_time(x, ensure_snap).samples());
}

double
MidiView::get_position_pixels()
{
	return _editing_context.time_to_pixel(_midi_region->position());
}

double
MidiView::get_end_position_pixels()
{
	return _editing_context.time_to_pixel (_midi_region->end());
}

void
MidiView::begin_resizing (bool /*at_front*/)
{
	_resize_data.clear();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		Note *note = dynamic_cast<Note*> (*i);

		// only insert CanvasNotes into the map
		if (note) {
			NoteResizeData *resize_data = new NoteResizeData();
			resize_data->note = note;

			// create a new SimpleRect from the note which will be the resize preview
			ArdourCanvas::Rectangle *resize_rect = new ArdourCanvas::Rectangle (_note_group,
											    ArdourCanvas::Rect (note->x0(), note->y0(), note->x0(), note->y1()));

			// calculate the colors: get the color settings
			uint32_t fill_color = NoteBase::meter_style_fill_color (note->note()->velocity(), true);

			// make the resize preview notes more transparent and bright
			fill_color = UINT_INTERPOLATE(fill_color, 0xFFFFFF40, 0.5);

			// calculate color based on note velocity
			resize_rect->set_fill_color (UINT_INTERPOLATE(
				NoteBase::meter_style_fill_color(note->note()->velocity(), note->selected()),
				fill_color,
				0.85));

			resize_rect->set_outline_color (NoteBase::calculate_outline (
								UIConfiguration::instance().color ("midi note selected outline")));

			resize_data->resize_rect = resize_rect;
			_resize_data.push_back(resize_data);
		}
	}
}

/** Update resizing notes while user drags.
 * @param primary `primary' note for the drag; ie the one that is used as the reference in non-relative mode.
 * @param at_front which end of the note (true == note on, false == note off)
 * @param delta_x change in mouse position since the start of the drag
 * @param relative true if relative resizing is taking place, false if absolute resizing.  This only makes
 * a difference when multiple notes are being resized; in relative mode, each note's length is changed by the
 * amount of the drag.  In non-relative mode, all selected notes are set to have the same start or end point
 * as the \a primary note.
 * @param snap_delta snap offset of the primary note in pixels. used in SnapRelative SnapDelta mode.
 * @param with_snap true if snap is to be used to determine the position, false if no snap is to be used.
 */
void
MidiView::update_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative, double snap_delta, bool with_snap)
{
	bool cursor_set = false;
	bool const ensure_snap = _editing_context.snap_mode () != SnapMagnetic;

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		ArdourCanvas::Rectangle* resize_rect = (*i)->resize_rect;
		Note* canvas_note = (*i)->note;
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x0() + delta_x + snap_delta;
			} else {
				current_x = primary->x0() + delta_x + snap_delta;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x1() + delta_x + snap_delta;
			} else {
				current_x = primary->x1() + delta_x + snap_delta;
			}
		}

		if (current_x < 0) {
			/* This works even with snapping because RegionView::snap_sample_to_sample()
			 * snaps forward if the snapped sample is before the beginning of the region
			 */
			current_x = 0;
		}
		if (current_x > _editing_context.duration_to_pixels (_midi_region->length())) {
			current_x = _editing_context.duration_to_pixels (_midi_region->length());
		}

		if (at_front) {
			if (with_snap) {
				resize_rect->set_x0 (snap_to_pixel (current_x, ensure_snap) - snap_delta);
			} else {
				resize_rect->set_x0 (current_x - snap_delta);
			}
			resize_rect->set_x1 (canvas_note->x1());
		} else {
			if (with_snap) {
				resize_rect->set_x1 (snap_to_pixel (current_x, ensure_snap) - snap_delta);
			} else {
				resize_rect->set_x1 (current_x - snap_delta);
			}
			resize_rect->set_x0 (canvas_note->x0());
		}


		if (!cursor_set) {
			/* Convert snap delta from pixels to beats. */
			timepos_t snap_delta_time = timepos_t (_editing_context.pixel_to_sample (snap_delta));
			Beats snap_delta_beats;
			int sign = 1;


			/* negative beat offsets aren't allowed */
			if (snap_delta_time > 0) {
				snap_delta_beats = _midi_region->region_distance_to_region_beats (timecnt_t (snap_delta_time, _midi_region->position()));
			} else if (snap_delta_time < 0) {
				snap_delta_beats = _midi_region->region_distance_to_region_beats (timecnt_t (-snap_delta_time, _midi_region->position()));
				sign = -1;
			}

			timepos_t snapped_x;

			if (with_snap) {
				snapped_x = snap_pixel_to_time (current_x, ensure_snap); /* units depend on snap settings */
			} else {
				snapped_x = timepos_t (_editing_context.pixel_to_sample (current_x)); /* probably samples */
			}

			Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
			const timepos_t abs_beats (tmap->quarters_at (snapped_x));
			const Temporal::Beats src_beats = _midi_region->absolute_time_to_source_beats (abs_beats);
			Temporal::Beats len         = Temporal::Beats();

			if (at_front) {
				if (src_beats < canvas_note->note()->end_time()) {
					len = canvas_note->note()->time() - src_beats + (snap_delta_beats * sign);
					len += canvas_note->note()->length();
				}
			} else {
				if (src_beats >= canvas_note->note()->time()) {
					len = src_beats - canvas_note->note()->time() - (snap_delta_beats * sign);
				}
			}

			/* drawn notes must be at least 1/512th note (1/4 note
			 * divided by 128. But notice that we're not *setting*
			 * note length here, this is just telling the user how
			 * long it will be if the drag completes.
			*/
			len = std::max (Temporal::Beats (0, 128), len);

			char buf[16];
			/* represent as float frac to help out the user */
			snprintf (buf, sizeof (buf), "%.3f beats", len.get_beats() + (len.get_ticks()/(double)Temporal::ticks_per_beat));
			show_verbose_cursor (buf, 0, 0);

			cursor_set = true;

			_editing_context.set_snapped_cursor_position (snapped_x + midi_region()->position());
		}

	}
}


/** Finish resizing notes when the user releases the mouse button.
 *  Parameters the same as for \a update_resizing().
 */
void
MidiView::finish_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative, double snap_delta, bool with_snap)
{
	_note_diff_command = _model->new_note_diff_command (_("resize notes"));  /* we are a subcommand, so we don't want to use start_note_diff() which begins a new command */

	/* XX why doesn't snap_pixel_to_sample() handle this properly? */
	bool const ensure_snap = _editing_context.snap_mode () != SnapMagnetic;

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		Note*  canvas_note = (*i)->note;
		ArdourCanvas::Rectangle*  resize_rect = (*i)->resize_rect;

		/* Get the new x position for this resize, which is in pixels relative
		 * to the region position.
		 */

		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x0() + delta_x + snap_delta;
			} else {
				current_x = primary->x0() + delta_x + snap_delta;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x1() + delta_x + snap_delta;
			} else {
				current_x = primary->x1() + delta_x + snap_delta;
			}
		}

		if (current_x < 0) {
			current_x = 0;
		}

		if (current_x > _editing_context.duration_to_pixels (_midi_region->length())) {
			current_x = _editing_context.duration_to_pixels (_midi_region->length());
		}

		/* Convert snap delta from pixels to beats with sign. */
		timepos_t snap_delta_time (_editing_context.pixel_to_sample (snap_delta));
		Temporal::Beats snap_delta_beats;
		int sign = 1;

		if (snap_delta_time.is_positive()) {
			snap_delta_beats = _midi_region->region_distance_to_region_beats (timecnt_t (snap_delta_time, _midi_region->position()));
		} else if (snap_delta_time.is_negative()) {
			snap_delta_beats = _midi_region->region_distance_to_region_beats (timecnt_t (-snap_delta_time, _midi_region->position()));
			sign = -1;
		}

		/* Convert the new x position to a position within the source */
		timecnt_t current_time;
		if (with_snap) {
			current_time = snap_pixel_to_time (current_x, ensure_snap);
		} else {
			current_time = timecnt_t (_editing_context.pixel_to_sample (current_x));
		}

		/* and then to beats */
		const Temporal::Beats src_beats = _midi_region->absolute_time_to_source_beats (_midi_region->position() + current_time);

		if (at_front && src_beats < canvas_note->note()->end_time()) {
			note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::StartTime, src_beats - (snap_delta_beats * sign));
			Temporal::Beats len = canvas_note->note()->time() - src_beats + (snap_delta_beats * sign);
			len += canvas_note->note()->length();

			if (!!len) {
				note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
			}
		}

		if (!at_front) {
			Temporal::Beats llen = src_beats - canvas_note->note()->time() - (snap_delta_beats * sign);
			Temporal::Beats len = std::max (Temporal::Beats (0, 1), llen);
			note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
		}

		delete resize_rect;
		delete (*i);
	}

	_resize_data.clear();
	apply_note_diff(true/*subcommand*/);
}

void
MidiView::abort_resizing ()
{
	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		delete (*i)->resize_rect;
		delete *i;
	}

	_resize_data.clear ();
}

void
MidiView::change_note_velocity(NoteBase* event, int8_t velocity, bool relative)
{
	uint8_t new_velocity;

	if (relative) {
		new_velocity = event->note()->velocity() + velocity;
		clamp_to_0_127(new_velocity);
	} else {
		new_velocity = velocity;
	}

	event->set_selected (event->selected()); // change color

	note_diff_add_change (event, MidiModel::NoteDiffCommand::Velocity, new_velocity);
}

uint8_t
MidiView::change_note_note (NoteBase* event, int8_t note, bool relative)
{
	uint8_t new_note;

	if (relative) {
		new_note = event->note()->note() + note;
	} else {
		new_note = note;
	}

	clamp_to_0_127 (new_note);
	note_diff_add_change (event, MidiModel::NoteDiffCommand::NoteNumber, new_note);
	return new_note;
}

void
MidiView::trim_note (NoteBase* event, Temporal::Beats front_delta, Temporal::Beats end_delta)
{
	bool change_start = false;
	bool change_length = false;
	Temporal::Beats new_start;
	Temporal::Beats new_length;

	/* NOTE: the semantics of the two delta arguments are slightly subtle:

	   front_delta: if positive - move the start of the note later in time (shortening it)
	   if negative - move the start of the note earlier in time (lengthening it)

	   end_delta:   if positive - move the end of the note later in time (lengthening it)
	   if negative - move the end of the note earlier in time (shortening it)
	*/

	if (!!front_delta) {
		if (front_delta < std::numeric_limits<Temporal::Beats>::lowest()) {

			if (event->note()->time() < -front_delta) {
				new_start = Temporal::Beats();
			} else {
				new_start = event->note()->time() + front_delta; // moves earlier
			}

			/* start moved toward zero, so move the end point out to where it used to be.
			   Note that front_delta is negative, so this increases the length.
			*/

			new_length = event->note()->length() - front_delta;
			change_start = true;
			change_length = true;

		} else {

			Temporal::Beats new_pos = event->note()->time() + front_delta;

			if (new_pos < event->note()->end_time()) {
				new_start = event->note()->time() + front_delta;
				/* start moved toward the end, so move the end point back to where it used to be */
				new_length = event->note()->length() - front_delta;
				change_start = true;
				change_length = true;
			}
		}

	}

	if (!!end_delta) {
		bool can_change = true;
		if (end_delta < Temporal::Beats()) {
			if (event->note()->length() < -end_delta) {
				can_change = false;
			}
		}

		if (can_change) {
			new_length = event->note()->length() + end_delta;
			change_length = true;
		}
	}

	if (change_start) {
		note_diff_add_change (event, MidiModel::NoteDiffCommand::StartTime, new_start);
	}

	if (change_length) {
		note_diff_add_change (event, MidiModel::NoteDiffCommand::Length, new_length);
	}
}

void
MidiView::change_note_channel (NoteBase* event, int8_t chn, bool relative)
{
	uint8_t new_channel;

	if (relative) {
		if (chn < 0.0) {
			if (event->note()->channel() < -chn) {
				new_channel = 0;
			} else {
				new_channel = event->note()->channel() + chn;
			}
		} else {
			new_channel = event->note()->channel() + chn;
		}
	} else {
		new_channel = (uint8_t) chn;
	}

	note_diff_add_change (event, MidiModel::NoteDiffCommand::Channel, new_channel);
}

void
MidiView::change_note_time (NoteBase* event, Temporal::Beats delta, bool relative)
{
	Temporal::Beats new_time;

	if (relative) {
		if (delta < Temporal::Beats()) {
			if (event->note()->time() < -delta) {
				new_time = Temporal::Beats();
			} else {
				new_time = event->note()->time() + delta;
			}
		} else {
			new_time = event->note()->time() + delta;
		}
	} else {
		new_time = delta;
	}

	note_diff_add_change (event, MidiModel::NoteDiffCommand::StartTime, new_time);
}

void
MidiView::change_note_length (NoteBase* event, Temporal::Beats t)
{
	note_diff_add_change (event, MidiModel::NoteDiffCommand::Length, t);
}

void
MidiView::begin_drag_edit (std::string const & why)
{
}

void
MidiView::end_drag_edit ()
{
}

bool
MidiView::set_velocities_for_notes (std::vector<NoteBase*>& notes, std::vector<int>& velocities)
{
	start_note_diff_command (_("draw velocities"));

	assert (notes.size() == velocities.size());

	/* Does not use selection, used when drawing/dragging in velocity lane */

	bool changed = false;

	auto notei = notes.begin();
	auto veloi = velocities.begin();

	while (notei != notes.end()) {

		int delta = (*veloi) - (*notei)->note()->velocity();

		if (delta) {
			changed = true;
			change_note_velocity (*notei, delta, true);
		}

		++notei;
		++veloi;
	}

	apply_note_diff (true /*subcommand, we don't want this to start a new commit*/, false);
	_editing_context.commit_reversible_command ();
	delete _note_diff_command;
	_note_diff_command = nullptr;


	return changed;
}

bool
MidiView::set_velocity_for_notes (std::vector<NoteBase*>& notes, int velocity)
{
	/* Does not use selection, used when drawing/dragging in velocity lane */

	bool changed = false;

	for (auto & note : notes) {

		int delta = velocity - note->note()->velocity();

		if (!delta) {
			continue;
		}

		changed = true;
		change_note_velocity (note, delta, true);
	}

	return changed;
}

void
MidiView::set_velocity (NoteBase* note, int velocity)
{
	if (_selection.empty()) {
		return;
	}

	int delta = velocity - note->note()->velocity();

	if (!delta) {
		return;
	}

	start_note_diff_command (_("set velocities"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;

		change_note_velocity (*i, delta, true);

		i = next;
	}

	apply_note_diff();
}

void
MidiView::change_velocities (bool up, bool fine, bool allow_smush, bool all_together)
{
	int8_t delta;
	int8_t value = 0;

	if (_selection.empty()) {
		return;
	}

	if (fine) {
		delta = 1;
	} else {
		delta = 10;
	}

	if (!up) {
		delta = -delta;
	}

	if (!allow_smush) {
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->velocity() < -delta || (*i)->note()->velocity() + delta > 127) {
				goto cursor_label;
			}
		}
	}

	start_note_diff_command (_("change velocities"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;

		if (all_together) {
			if (i == _selection.begin()) {
				change_note_velocity (*i, delta, true);
				value = (*i)->note()->velocity() + delta;
			} else {
				change_note_velocity (*i, value, false);
			}

		} else {
			change_note_velocity (*i, delta, true);
		}

		i = next;
	}

	apply_note_diff();

  cursor_label:
	if (!_selection.empty()) {
		char buf[24];
		snprintf (buf, sizeof (buf), "Vel %d",
		          (int) (*_selection.begin())->note()->velocity());
		show_verbose_cursor (buf, 10, 10);
	}
}


void
MidiView::transpose (bool up, bool fine, bool allow_smush)
{
	if (_selection.empty()) {
		return;
	}

	int8_t delta;

	if (fine) {
		delta = 1;
	} else {
		delta = 12;
	}

	if (!up) {
		delta = -delta;
	}

	if (!allow_smush) {
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if (!up) {
				if ((int8_t) (*i)->note()->note() + delta <= 0) {
					return;
				}
			} else {
				if ((int8_t) (*i)->note()->note() + delta > 127) {
					return;
				}
			}
		}
	}

	start_note_diff_command (_("transpose"));

	uint8_t lowest = _midi_context.lowest_note();
	uint8_t highest = _midi_context.highest_note();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		uint8_t new_note = change_note_note (*i, delta, true);
		lowest = min (lowest, new_note);
		highest = max (highest, new_note);
		i = next;
	}

	apply_note_diff ();

	if (lowest < _midi_context.lowest_note() || highest > _midi_context.highest_note()) {
		_midi_context.maybe_extend_note_range (lowest);
		_midi_context.maybe_extend_note_range (highest);
	}
}

void
MidiView::change_note_lengths (bool fine, bool shorter, Temporal::Beats delta, bool start, bool end)
{
	if (!delta) {
		if (fine) {
			delta = Temporal::Beats::ticks (Temporal::ticks_per_beat / 128);
		} else {
			/* grab the current grid distance */
			delta = get_draw_length_beats (_midi_region->position());
		}
	}

	if (shorter) {
		delta = -delta;
	}

	start_note_diff_command (_("change note lengths"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;

		/* note the negation of the delta for start */

		trim_note (*i,
		           (start ? -delta : Temporal::Beats()),
		           (end   ? delta  : Temporal::Beats()));
		i = next;
	}

	apply_note_diff ();

}

void
MidiView::nudge_notes (bool forward, bool fine)
{
	if (_selection.empty()) {
		return;
	}

	/* pick a note as the point along the timeline to get the nudge distance.
	   its not necessarily the earliest note, so we may want to pull the notes out
	   into a vector and sort before using the first one.
	*/

	const timepos_t ref_point = _midi_region->source_beats_to_absolute_time ((*(_selection.begin()))->note()->time());
	Temporal::Beats  delta;

	timecnt_t       unused;
	const timecnt_t distance = _editing_context.get_nudge_distance (ref_point, unused);

	if (!distance.is_zero() || _editing_context.snap_mode() == Editing::SnapOff) {

		/* grid is off - use nudge distance */

		delta = _midi_region->region_distance_to_region_beats (timecnt_t (distance.beats(), _midi_region->position()));

	} else {

		/* use grid */

		bool success;

		delta = _editing_context.get_grid_type_as_beats (success, ref_point);

		if (!success) {
			delta = Temporal::Beats (1, 0);
		}
	}

	if (!delta) {
		return;
	}

	if (fine) {
		delta = delta / 4;
	}

	if (!forward) {
		delta = -delta;
	}

	start_note_diff_command (_("nudge"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_time (*i, delta, true);
		i = next;
	}

	apply_note_diff ();
}

void
MidiView::change_channel(uint8_t channel)
{
	start_note_diff_command(_("change channel"));
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		note_diff_add_change (*i, MidiModel::NoteDiffCommand::Channel, channel);
	}

	apply_note_diff();
}


void
MidiView::note_entered(NoteBase* ev)
{
	_entered_note = ev;

	if (_mouse_state == SelectTouchDragging) {

		note_selected (ev, true);

	} else if (_editing_context.current_mouse_mode() == MouseContent) {

		remove_ghost_note ();
		show_verbose_cursor (ev->note ());

	} else if (_editing_context.current_mouse_mode() == MouseDraw) {

		remove_ghost_note ();
		show_verbose_cursor (ev->note ());
	}
}

void
MidiView::note_left (NoteBase*)
{
	_entered_note = 0;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	hide_verbose_cursor ();
}

void
MidiView::patch_entered (PatchChange* p)
{
	ostringstream s;
	s << _("Bank ") << (p->patch()->bank() + MIDI_BP_ZERO) << '\n'
	  << _midi_track->instrument_info().get_patch_name_without (p->patch()->bank(), p->patch()->program(), p->patch()->channel()) << '\n'
	  << _("Channel ") << ((int) p->patch()->channel() + 1);
	show_verbose_cursor (s.str(), 10, 20);
	//p->item().grab_focus();
}

void
MidiView::patch_left (PatchChange *)
{
	hide_verbose_cursor ();
	/* focus will transfer back via the enter-notify event sent to this
	 * midi region view.
	 */
}

void
MidiView::sysex_entered (SysEx* p)
{
	// ostringstream s;
	// CAIROCANVAS
	// need a way to extract text from p->_flag->_text
	// s << p->text();
	// show_verbose_cursor (s.str(), 10, 20);
	//p->item().grab_focus();
}

void
MidiView::sysex_left (SysEx *)
{
	hide_verbose_cursor ();
	/* focus will transfer back via the enter-notify event sent to this
	 * midi region view.
	 */
}

void
MidiView::note_mouse_position (float x_fraction, float /*y_fraction*/, bool can_set_cursor)
{
	Editing::MouseMode mm = _editing_context.current_mouse_mode();
	bool trimmable = (mm == MouseContent || mm == MouseTimeFX || mm == MouseDraw);

	EditingContext::EnterContext* ctx = _editing_context.get_enter_context(NoteItem);
	if (can_set_cursor && ctx) {
		if (trimmable && x_fraction > 0.0 && x_fraction < 0.2) {
			ctx->cursor_ctx->change(_editing_context.cursors()->left_side_trim);
		} else if (trimmable && x_fraction >= 0.8 && x_fraction < 1.0) {
			ctx->cursor_ctx->change(_editing_context.cursors()->right_side_trim);
		} else {
			ctx->cursor_ctx->change(_editing_context.cursors()->grabber_note);
		}
	}
}

std::string
MidiView::get_modifier_name () const
{
	return "editable region";
}

uint32_t
MidiView::get_fill_color() const
{
	std::string mod_name = get_modifier_name();

	if (mod_name.empty ()) {
		return get_fill_color();
	} else {
		return UIConfiguration::instance().color_mod (get_fill_color(), mod_name);
	}
}

void
MidiView::midi_channel_mode_changed ()
{
	uint16_t mask = _midi_track->get_playback_channel_mask();
	ChannelMode mode = _midi_track->get_playback_channel_mode ();

	if (mode == ForceChannel) {
		mask = 0xFFFF; // Show all notes as active (below)
	}

	// Update notes for selection
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		i->second->on_channel_selection_change (mask);
	}

	_patch_changes.clear ();
	display_patch_changes ();
}

void
MidiView::instrument_settings_changed ()
{
	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		(*x).second->update_name ();
	}
}

void
MidiView::cut_copy_clear (Editing::CutCopyOp op)
{
	if (_selection.empty()) {
		return;
	}

	switch (op) {
	case Delete:
		/* XXX what to do ? */
		break;
	case Cut:
	case Copy:
		_editing_context.get_cut_buffer().add (selection_as_cut_buffer());
		break;
	default:
		break;
	}

	if (op == Copy) {
		return;
	}

	bool as_subcommand = false;

	/* Editor::cut_copy already started an undo operation,
	 * so we cannot call start_note_diff_command ()
	 */
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if (!_note_diff_command) {
			_note_diff_command = _model->new_note_diff_command ("Cut");
			as_subcommand = true;
		}
		note_diff_remove_note (*i);
	}

	apply_note_diff (as_subcommand);
}

MidiCutBuffer*
MidiView::selection_as_cut_buffer () const
{
	Notes notes;

	for (Selection::const_iterator i = _selection.begin(); i != _selection.end(); ++i) {
		NoteType* n = (*i)->note().get();
		notes.insert (std::shared_ptr<NoteType> (new NoteType (*n)));
	}

	MidiCutBuffer* cb = new MidiCutBuffer (_editing_context.session());
	cb->set (notes);

	return cb;
}

void
MidiView::duplicate_selection ()
{
	_editing_context.begin_reversible_command (_("duplicate notes"));

	if (_selection.empty()) {
		return;
	}

	/* find last selection position */
	timepos_t dup_pos = timepos_t (Temporal::BeatTime);

	for (Selection::const_iterator s = _selection.begin(); s != _selection.end(); ++s) {
		dup_pos = std::max (dup_pos, _midi_region->source_beats_to_absolute_time ((*s)->note()->end_time()));
	}

	/* Use a local Selection object that will not affect the global
	 * selection. Possible ::paste() should accept a different kind of
	 * object but that would conflict with the Editor API.
	 */

	::Selection local_selection (dynamic_cast<PublicEditor*> (&_editing_context), false);
	MidiNoteSelection note_selection;

	note_selection.push_back (selection_as_cut_buffer());

	local_selection.set (note_selection);

	PasteContext ctxt (0, 1, ItemCounts(), false);
	bool commit = paste (dup_pos, local_selection, ctxt);
	if (commit) {
		_editing_context.commit_reversible_command ();
	} else {
		_editing_context.abort_reversible_command ();
	}
}

/** undo commands were initiated at the 'action' level. ::paste and ::paste_internal should implement subcommands */
bool
MidiView::paste (timepos_t const & pos, const ::Selection& selection, PasteContext& ctx)
{
	bool commit = false;

	// Paste notes, if available
	MidiNoteSelection::const_iterator m = selection.midi_notes.get_nth(ctx.counts.n_notes());

	if (m != selection.midi_notes.end()) {

		ctx.counts.increase_n_notes();

		if (!(*m)->empty()) {
			commit = true;
		}

		/* clear any current selection because we're going to select the paste ones */
		clear_note_selection ();
		paste_internal (pos, ctx.count, ctx.times, **m);
	}

#warning paul fix MRV/MV
#if 0
	// Paste control points to automation children, if available
	for (auto & at : midi_view()->automation_tracks()) {
		if (at.second->paste(pos, selection, ctx)) {
			commit = true;
		}
	}
#endif
	return commit;
}

/** undo commands were initiated at the 'action' level. ::paste and ::paste_internal should implement subcommands */
void
MidiView::paste_internal (timepos_t const & pos, unsigned paste_count, float times, const MidiCutBuffer& mcb)
{
	if (mcb.empty()) {
		return;
	}

	PBD::Unwinder<bool> puw (_select_all_notes_after_add, true);

	_note_diff_command = _model->new_note_diff_command (_("paste")); /* we are a subcommand, so we don't want to use start_note_diff */

	const Temporal::Beats snap_beats    = get_grid_beats(pos);
	const Temporal::Beats first_time    = (*mcb.notes().begin())->time();
	const Temporal::Beats last_time     = (*mcb.notes().rbegin())->end_time();
	const Temporal::Beats duration      = last_time - first_time;
	const Temporal::Beats snap_duration = duration.round_to_multiple (snap_beats);
	const Temporal::Beats paste_offset  = snap_duration * int32_t (paste_count);
	const Temporal::Beats quarter_note  = _midi_region->absolute_time_to_source_beats (pos) + paste_offset;
	Temporal::Beats       end_point;

	DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("Paste data spans from %1 to %2 (%3) ; paste pos beats = %4 (based on %5 - %6)\n",
	                                               first_time,
	                                               last_time,
	                                               duration, pos, _midi_region->position(),
	                                               quarter_note));

	for (int n = 0; n < (int) times; ++n) {

		for (auto const & note : mcb.notes()) {

			std::shared_ptr<NoteType> copied_note (new NoteType (*(note.get())));
			copied_note->set_time (quarter_note + copied_note->time() - first_time);
			copied_note->set_id (Evoral::next_event_id());

			/* make all newly added notes selected */

			note_diff_add_note (copied_note, true);
			end_point = copied_note->end_time();
		}
	}

	/* if we pasted past the current end of the region, extend the region */

	timepos_t end = _midi_region->source_beats_to_absolute_time (end_point);
	timepos_t region_end = _midi_region->nt_last();

	if (end > region_end) {

		DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("Paste extended region from %1 to %2\n", region_end, end));

		// XXXX _midi_region->clear_changes ();
		/* we probably need to get the snap modifier somehow to make this correct for non-musical use */
		_midi_region->set_length (_midi_region->position().distance (end));
		_editing_context.session()->add_command (new StatefulDiffCommand (_midi_region));
	}

	_marked_for_selection.clear ();
	_pending_note_selection.clear ();

	_model->apply_diff_command_as_subcommand (*_editing_context.session(), _note_diff_command);
	_note_diff_command = nullptr;
}

struct EventNoteTimeEarlyFirstComparator {
	bool operator() (NoteBase* a, NoteBase* b) {
		return a->note()->time() < b->note()->time();
	}
};

void
MidiView::goto_next_note (bool add_to_selection)
{
	bool use_next = false;

	uint16_t const channel_mask = _midi_track->get_playback_channel_mask();
	NoteBase* first_note = 0;

	MidiModel::ReadLock lock(_model->read_lock());
	MidiModel::Notes& notes (_model->notes());

	if (notes.empty()) {
		return;
	}

	_editing_context.begin_reversible_selection_op (X_("Select Adjacent Note"));

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
		NoteBase* cne = 0;
		if ((cne = find_canvas_note (*n))) {

			if (!first_note && (channel_mask & (1 << (*n)->channel()))) {
				first_note = cne;
			}

			if (cne->selected()) {
				use_next = true;
				continue;
			} else if (use_next) {
				if (channel_mask & (1 << (*n)->channel())) {
					if (!add_to_selection) {
						unique_select (cne);
					} else {
						note_selected (cne, true, false);
					}

					return;
				}
			}
		}
	}

	/* use the first one */

	if (!_events.empty() && first_note) {
		unique_select (first_note);
	}


	_editing_context.commit_reversible_selection_op();
}

void
MidiView::goto_previous_note (bool add_to_selection)
{
	bool use_next = false;

	uint16_t const channel_mask = _midi_track->get_playback_channel_mask ();
	NoteBase* last_note = 0;

	MidiModel::ReadLock lock(_model->read_lock());
	MidiModel::Notes& notes (_model->notes());

	if (notes.empty()) {
		return;
	}

	_editing_context.begin_reversible_selection_op (X_("Select Adjacent Note"));

	for (MidiModel::Notes::reverse_iterator n = notes.rbegin(); n != notes.rend(); ++n) {
		NoteBase* cne = 0;
		if ((cne = find_canvas_note (*n))) {

			if (!last_note && (channel_mask & (1 << (*n)->channel()))) {
				last_note = cne;
			}

			if (cne->selected()) {
				use_next = true;
				continue;

			} else if (use_next) {
				if (channel_mask & (1 << (*n)->channel())) {
					if (!add_to_selection) {
						unique_select (cne);
					} else {
						note_selected (cne, true, false);
					}

					return;
				}
			}
		}
	}

	/* use the last one */

	if (!_events.empty() && last_note) {
		unique_select (last_note);
	}

	_editing_context.commit_reversible_selection_op();
}

void
MidiView::selection_as_notelist (Notes& selected, bool allow_all_if_none_selected)
{
	bool had_selected = false;

	/* we previously time sorted events here, but Notes is a multiset sorted by time */

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if (i->second->selected()) {
			selected.insert (i->first);
			had_selected = true;
		}
	}

	if (allow_all_if_none_selected && !had_selected) {
		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
			selected.insert (i->first);
		}
	}
}

void
MidiView::update_ghost_note (double x, double y, uint32_t state)
{
	assert (_ghost_note);
	x = std::max(0.0, x);

	const double global_x (x);

	_last_ghost_x = x;
	_last_ghost_y = y;

	/* we need the y value only */
	_note_group->canvas_to_item (x, y);

	samplepos_t const unsnapped_sample = _editing_context.pixel_to_sample (global_x);
	Temporal::timepos_t snapped_pos = timepos_t (unsnapped_sample);
	_editing_context.snap_to (snapped_pos, RoundNearest, SnapToGrid_Scaled);

	const Temporal::Beats snapped_beats = _midi_region->absolute_time_to_region_beats(snapped_pos);

	/* prevent Percussive mode from displaying a ghost hit at region end */
	if ((_midi_context.note_mode() == Percussive) && (snapped_beats >= _midi_region->length().beats())) {
		_ghost_note->hide();
		hide_verbose_cursor ();
		return;
	}

	/* ghost note may have been snapped before region */

	if (snapped_beats < Temporal::Beats()) {
		_ghost_note->hide();
		return;
	}

	_ghost_note->show();

	/* calculate time in of a single grid units worth of beats, at the start of source */
	const Temporal::Beats length = get_draw_length_beats (_midi_region->source_position() + timecnt_t (snapped_beats));

	_ghost_note->note()->set_time (snapped_beats);
	_ghost_note->note()->set_length (length);
	_ghost_note->note()->set_note (y_to_note (y));
	_ghost_note->note()->set_channel (_midi_context.get_preferred_midi_channel ());
	_ghost_note->note()->set_velocity (get_velocity_for_add (snapped_beats));

	update_note (_ghost_note, false);

	show_verbose_cursor (_ghost_note->note ());
}

void
MidiView::create_ghost_note (double x, double y, uint32_t state)
{
	remove_ghost_note ();

	std::shared_ptr<NoteType> g (new NoteType);
	if (_midi_context.note_mode() == Sustained) {
		_ghost_note = new Note (*this, _note_group, g);
	} else {
		_ghost_note = new Hit (*this, _note_group, 10, g);
	}
	_ghost_note->set_ignore_events (true);
	_ghost_note->set_outline_color (0x000000aa);
	update_ghost_note (x, y, state);
	_ghost_note->show ();

	show_verbose_cursor (_ghost_note->note ());
}

void
MidiView::remove_ghost_note ()
{
	delete _ghost_note;
	_ghost_note = 0;
}

void
MidiView::hide_verbose_cursor ()
{
	_editing_context.verbose_cursor()->hide ();
	_midi_context.set_note_highlight (NO_MIDI_NOTE);
}

void
MidiView::snap_changed ()
{
	if (!_ghost_note) {
		return;
	}

	create_ghost_note (_last_ghost_x, _last_ghost_y, 0);
}

void
MidiView::drop_down_keys ()
{
	_mouse_state = None;
}

void
MidiView::maybe_select_by_position (GdkEventButton* ev, double /*x*/, double y)
{
	/* XXX: This is dead code.  What was it for? */

	double note = y_to_note(y);
	Events e;

	uint16_t chn_mask = _midi_track->get_playback_channel_mask();

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
		get_events (e, Evoral::Sequence<Temporal::Beats>::PitchGreaterThanOrEqual, (uint8_t) floor (note), chn_mask);
	} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		get_events (e, Evoral::Sequence<Temporal::Beats>::PitchLessThanOrEqual, (uint8_t) floor (note), chn_mask);
	} else {
		return;
	}

	bool add_mrv_selection = false;

	if (_selection.empty()) {
		add_mrv_selection = true;
	}

	for (Events::iterator i = e.begin(); i != e.end(); ++i) {
		if (_selection.insert (i->second).second) {
			i->second->set_selected (true);
			ghost_sync_selection (i->second);
		}
	}

	if (add_mrv_selection) {
		select_self (true);
	}
}

void
MidiView::color_handler ()
{
	_patch_change_outline = UIConfiguration::instance().color ("midi patch change outline");
	_patch_change_fill = UIConfiguration::instance().color_mod ("midi patch change fill", "midi patch change fill");

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		i->second->set_selected (i->second->selected()); // will change color
		ghost_sync_selection (i->second);
	}

	/* XXX probably more to do here */
}

void
MidiView::show_step_edit_cursor (Temporal::Beats pos)
{
	if (_step_edit_cursor == 0) {
		_step_edit_cursor = new ArdourCanvas::Rectangle (_note_group->parent());
		_step_edit_cursor->set_y0 (0);
		_step_edit_cursor->set_y1 (_midi_context.contents_height());
		_step_edit_cursor->set_fill_color (RGBA_TO_UINT (45,0,0,90));
		_step_edit_cursor->set_outline_color (RGBA_TO_UINT (85,0,0,90));
	}

	move_step_edit_cursor (pos);
	_step_edit_cursor->show ();
}

void
MidiView::move_step_edit_cursor (Temporal::Beats pos)
{
	_step_edit_cursor_position = pos;

	if (_step_edit_cursor) {
		double pixel = _editing_context.time_to_pixel (_midi_region->region_beats_to_region_time (pos));
		_step_edit_cursor->set_x0 (pixel);
		set_step_edit_cursor_width (_step_edit_cursor_width);
	}
}

void
MidiView::hide_step_edit_cursor ()
{
	if (_step_edit_cursor) {
		_step_edit_cursor->hide ();
	}
}

void
MidiView::set_step_edit_cursor_width (Temporal::Beats beats)
{
	_step_edit_cursor_width = beats;

	if (_step_edit_cursor) {
		_step_edit_cursor->set_x1 (_step_edit_cursor->x0() + _editing_context.duration_to_pixels (
			                           _midi_region->region_beats_to_region_time (_step_edit_cursor_position).distance
			                           (_midi_region->region_beats_to_region_time (_step_edit_cursor_position + beats))));
	}
}

/** Called when a diskstream on our track has received some data.  Update the view, if applicable.
 *  @param w Source that the data will end up in.
 */
void
MidiView::data_recorded (std::weak_ptr<MidiSource> w)
{
	if (!_active_notes) {
		/* we aren't actively being recorded to */
		return;
	}

	std::shared_ptr<MidiSource> src = w.lock ();
	if (!src || src != midi_region()->midi_source()) {
		/* recorded data was not destined for our source */
		return;
	}

	std::shared_ptr<MidiBuffer> buf = _midi_track->get_gui_feed_buffer ();

	samplepos_t back = max_samplepos;

	for (MidiBuffer::iterator i = buf->begin(); i != buf->end(); ++i) {
		const Evoral::Event<MidiBuffer::TimeType>& ev = *i;

		if (ev.is_channel_event()) {
			if (get_channel_mode() == FilterChannels) {
				if (((uint16_t(1) << ev.channel()) & get_selected_channels()) == 0) {
					continue;
				}
			}
		}

		/* ev.time() is in MidiBuffer::TimeType i.e. samples

		   we want to convert to beats relative to source start.
		*/


		Temporal::Beats const time_beats = src->time_since_capture_start (timepos_t (ev.time())).beats();

		if (ev.type() == MIDI_CMD_NOTE_ON) {

			std::shared_ptr<NoteType> note (new NoteType (ev.channel(), time_beats, std::numeric_limits<Temporal::Beats>::max() - time_beats, ev.note(), ev.velocity()));

			assert (note->end_time() == std::numeric_limits<Temporal::Beats>::max());

			NoteBase* nb = add_note (note, true);
			nb->item()->set_fill_color (UIConfiguration::instance().color ("recording note"));
			nb->item()->set_outline_color (UIConfiguration::instance().color ("recording note"));

			/* fix up our note range */
			if (ev.note() < _midi_context.lowest_note()) {
				set_note_range (ev.note(), _midi_context.highest_note());
			} else if (ev.note() > _midi_context.highest_note()) {
				set_note_range (_midi_context.lowest_note(), ev.note());
			}

		} else if (ev.type() == MIDI_CMD_NOTE_OFF) {

			// XXX WAS resolve_note (ev.note (), time_beats);
			uint8_t note = ev.note ();
			Temporal::Beats end_time = time_beats;

			if (_active_notes && _active_notes[note]) {
				/* Set note length so update_note() works.  Note this is a local note
					 for recording, not from a model, so we can safely mess with it. */
				_active_notes[note]->note()->set_length (end_time - _active_notes[note]->note()->time());

				/* End time is relative to the source being recorded. */

				// XXX ideally we'd calculate this based on end_time.
				// - we first have to get the absolute position of end_time (for tempo-map conversion)
				// - then calculate the distance from that relative to src->natural_position ()
				// - and then take the samples() value of that and convert it to pixels
				//
				// Much simpler to just use ev.time() which is already the absolute position (in sample-time)
				_active_notes[note]->set_x1 (_editing_context.sample_to_pixel ((src->time_since_capture_start (timepos_t (ev.time ()))).samples()));
				_active_notes[note]->set_outline_all ();
				_active_notes[note] = 0;
			}
		}

		back = ev.time ();
	}

	_midi_context.record_layer_check (_midi_region, back);
}

void
MidiView::trim_front_starting ()
{
	/* We used to eparent the note group to the region view's parent, so that it didn't change.
	   now we update it.
	*/
}

void
MidiView::trim_front_ending ()
{
	if (_midi_region->start().is_negative()) {
		/* Trim drag made start time -ve; fix this */
		midi_region()->fix_negative_start ();
	}
}

void
MidiView::edit_patch_change (PatchChange* pc)
{
	PatchChangeDialog d (_editing_context.session(), *pc->patch (), _midi_track->instrument_info(), Gtk::Stock::APPLY, true, true, _midi_region);

	int response = d.run();

	switch (response) {
	case Gtk::RESPONSE_ACCEPT:
		break;
	case Gtk::RESPONSE_REJECT:
		delete_patch_change (pc);
		return;
	default:
		return;
	}

	change_patch_change (pc->patch(), d.patch ());
}

void
MidiView::delete_sysex (SysEx* /*sysex*/)
{
	// CAIROCANVAS
	// sysyex object doesn't have a pointer to a sysex event
	// MidiModel::SysExDiffCommand* c = _model->new_sysex_diff_command (_("delete sysex"));
	// c->remove (sysex->sysex());
	// _model->apply_command (*trackview.session(), c);

	//_sys_exes.clear ();
	// display_sysexes();
}

std::string
MidiView::get_note_name (std::shared_ptr<NoteType> n, uint8_t note_value) const
{
	using namespace MIDI::Name;
	std::string name;

	MIDI::Name::PatchPrimaryKey patch_key;
	get_patch_key_at (n->time(), n->channel(), patch_key);
	name = _midi_track->instrument_info ().get_note_name (patch_key.bank(), patch_key.program(), n->channel(), note_value);

	char buf[128];
	snprintf (buf, sizeof (buf), "%s #%d\nCh %d Vel %d\n%.3f beats",
	          name.empty() ? ParameterDescriptor::midi_note_name (note_value).c_str() : name.c_str(),
	          (int) note_value,
	          (int) n->channel() + 1,
	          (int) n->velocity(),  //we display velocity 0-based; velocity 0 is a 'note-off' so the user just sees values 1..127 which 'looks' 1-based
	          n->length().get_beats() + (n->length().get_ticks()/(float)Temporal::ticks_per_beat));

	return buf;
}

void
MidiView::show_verbose_cursor_for_new_note_value(std::shared_ptr<NoteType> current_note,
                                                       uint8_t new_value) const
{
	_midi_context.set_note_highlight (new_value);
	show_verbose_cursor(get_note_name(current_note, new_value), 10, 20);
}

void
MidiView::show_verbose_cursor (std::shared_ptr<NoteType> n) const
{
	show_verbose_cursor_for_new_note_value(n, n->note());
}

void
MidiView::show_verbose_cursor (string const & text, double xoffset, double yoffset) const
{
	_editing_context.verbose_cursor()->set (text);
	_editing_context.verbose_cursor()->show ();
	_editing_context.verbose_cursor()->set_offset (ArdourCanvas::Duple (xoffset, yoffset));
}


uint8_t
MidiView::get_channel_for_add (MidiModel::TimeType time) const
{
	/* first, use the user-specified channel in the editor */
	if (_editing_context.draw_channel() != Editing::DRAW_CHAN_AUTO) {
		return _editing_context.draw_channel();
	}

	/* second, use the nearest note in the region-view (consistent with get_velocity_for_add behavior) */

	if (!_model->notes().empty()) {
		MidiModel::Notes::const_iterator m = _model->note_lower_bound(time);
		if (m == _model->notes().begin()) {
			// Before the start, use the channel of the first note
			return (*m)->channel();
		} else if (m == _model->notes().end()) {
			// Past the end, use the channel of the last note
			--m;
			return (*m)->channel();
		}
	}

	/* lastly: query the track's channel filter */
	return _midi_context.get_preferred_midi_channel();
}

uint8_t
MidiView::get_velocity_for_add (MidiModel::TimeType time) const
{
	if (_editing_context.draw_velocity() != Editing::DRAW_VEL_AUTO) {
		return _editing_context.draw_velocity();
	}

	if (_model->notes().size() < 2) {
		return 0x40;  // No notes, use default
	}

	MidiModel::Notes::const_iterator m = _model->notes().end();

	if (!_model->notes().empty()) {
		m = _model->note_lower_bound(time);
		if (m == _model->notes().begin()) {
			// Before the start, use the velocity of the first note
			return (*m)->velocity();
		} else if (m == _model->notes().end()) {
			// Past the end, use the velocity of the last note
			--m;
			return (*m)->velocity();
		}
	}

	if (_model->notes().size() == 1) {
		return (*m)->velocity();
	}

	// Interpolate velocity of surrounding notes
	MidiModel::Notes::const_iterator n = m;
	--n;

	const double t = DoubleableBeats (time).to_double();
	const double next = DoubleableBeats ((*n)->time()).to_double ();
	const double mmmm = DoubleableBeats ((*m)->time()).to_double ();

	const double frac = (t - next) / (mmmm - next);

	return (*n)->velocity() + (frac * ((*m)->velocity() - (*n)->velocity()));
}

ChannelMode
MidiView::get_channel_mode () const
{
	return _midi_track->get_playback_channel_mode();
}

uint16_t
MidiView::get_selected_channels () const
{
	return _midi_track->get_playback_channel_mask();
}

Temporal::Beats
MidiView::get_grid_beats (timepos_t const & pos) const
{
	bool          success = false;
	Temporal::Beats beats = _editing_context.get_grid_type_as_beats (success, pos);

	if (!success) {
		beats = Temporal::Beats (1, 0);
	}

	return beats;
}

Temporal::Beats
MidiView::get_draw_length_beats (timepos_t const & pos) const
{
	if (_midi_context.note_mode() == Percussive) {
		return Temporal::Beats (0, 1);
	}

	bool          success = false;
	Temporal::Beats beats = _editing_context.get_draw_length_as_beats (success, pos);

	if (!success) {
		beats = Temporal::Beats (1, 0);
	}

	return beats;
}

void
MidiView::quantize_selected_notes ()
{
	std::cerr << "QSN!\n";

	Quantize* quant = _editing_context.get_quantize_op ();

	if (!quant) {
		return;
	}

	PBD::Command* cmd = _editing_context.apply_midi_note_edit_op_to_region (*quant, *this);

	if (cmd) {
	       _editing_context.begin_reversible_command (quant->name ());
	       (*cmd)();
	       _editing_context.session()->add_command (cmd);
	       _editing_context.commit_reversible_command ();
	       _editing_context.session()->set_dirty ();
	}

	delete quant;
}

void
MidiView::sync_velocity_drag (double factor)
{
	for (auto & s : _selection) {
		s->set_velocity (factor);
	}
}

void
MidiView::start_note_splitting ()
{
	note_splitting = true;
	split_info.clear ();

	for (auto & s : _selection) {
		std::shared_ptr<NoteType> base (s->note());

		split_info.push_back (SplitInfo (base->time(),
		                                 base->length(),
		                                 base->note(),
		                                 base->channel(),
		                                 base->velocity(),
		                                 base->off_velocity()));
	}

	split_tuple = 1;
}

void
MidiView::end_note_splitting ()
{
	split_info.clear ();
	note_splitting = false;
}

void
MidiView::split_notes_grid ()
{
	start_note_splitting ();

	if (split_info.empty()) {
		return;
	}

	/* XXX need to adjust pos to be global */
	bool success;
	Temporal::Beats grid = _editing_context.get_grid_type_as_beats (success, timepos_t (split_info.front().time));

	if (!success) {
		/* No grid => use quarters */
		grid = Beats (1, 0);
	}

	split_tuple = split_info.front().base_len.to_ticks() / grid.to_ticks();

	start_note_diff_command (_("split notes"));
	for (auto & s : _selection) {
		note_diff_remove_note (s);
	}
	add_split_notes ();
	apply_note_diff (false);
}

void
MidiView::split_notes_more ()
{
	if (split_info.empty()) {
		start_note_splitting ();
		if (split_info.empty()) {
			return;
		}
	}

	split_tuple++;

	char buf[64];
	snprintf (buf, sizeof (buf), "Split %s into %d", split_info.front().base_len.str().c_str(), split_tuple);
	show_verbose_cursor (buf, 0, 0);

	start_note_diff_command (_("split notes more"));
	for (auto & s : _selection) {
		note_diff_remove_note (s);
	}
	add_split_notes ();
	apply_note_diff (false);
}

void
MidiView::split_notes_less ()
{
	if (split_info.empty()) {
		start_note_splitting ();
		if (split_info.empty()) {
			return;
		}
	}

	if (split_tuple < 2) {
		return;
	}

	split_tuple--;

	char buf[64];
	snprintf (buf, sizeof (buf), "Split %s into %d", split_info.front().base_len.str().c_str(), split_tuple);
	show_verbose_cursor (buf, 0, 0);

	start_note_diff_command (_("split notes less"));
	for (auto & s : _selection) {
		note_diff_remove_note (s);
	}
	add_split_notes ();
	apply_note_diff (false);
}

void
MidiView::join_notes ()
{
	/* Grab the selection, split it by pitch and find the earliest and
	 * latest contiguous segments of the same pitch
	 */

	for (int n = 0; n < 16; ++n) {
		join_notes_on_channel (n);
	}
}

struct NoteExtentInfo
{
	Temporal::Beats start;
	Temporal::Beats end;
	float velocity;
	float off_velocity;
	int cnt;

	NoteExtentInfo()
		: start (std::numeric_limits<Temporal::Beats>::max())
		, end (Temporal::Beats())
		, velocity (0.f)
		, off_velocity (0.f)
		, cnt (0) {}
};

void
MidiView::join_notes_on_channel (int chn)
{
	NoteExtentInfo ninfo[127];

	for (auto & s : _selection) {

		if (s->note()->channel() != chn) {
			continue;
		}

		std::shared_ptr<NoteType> base (s->note());
		NoteExtentInfo& ni (ninfo[base->note()]);
		ni.cnt++;

		if (base->time() < ni.start) {
			ni.start = base->time();
		}

		if (base->end_time() > ni.end) {
			ni.end = base->end_time();
		}

		ni.velocity += base->velocity();
		ni.off_velocity += base->off_velocity();
	}

	start_note_diff_command (_("join notes"));
	for (auto & s : _selection) {
		/* Only remove pitches that occur more than once */
		if (ninfo[s->note()->note()].cnt > 1 && s->note()->channel() == chn) {
			note_diff_remove_note (s);
		}
	}

	for (size_t n = 0; n < 127; ++n) {

		NoteExtentInfo& ni (ninfo[n]);

		if (ni.cnt > 1 && ni.end != Temporal::Beats()) {

			Temporal::Beats b = ni.end - ni.start;
			std::shared_ptr<NoteType> new_note (new NoteType (chn, ni.start, b, n, ni.velocity / ni.cnt));
			new_note->set_off_velocity (ni.off_velocity / ni.cnt);
			note_diff_add_note (new_note, true, true);
		}
	}

	apply_note_diff (false);

	end_note_splitting ();
}

void
MidiView::add_split_notes ()
{
	for (auto const & si : split_info) {

		Beats b = si.base_len / split_tuple;
		Beats pos (si.time);

		for (uint32_t n = 0; n < split_tuple; ++n) {
			std::shared_ptr<NoteType> new_note (new NoteType (si.channel, pos, b, si.note,  si.velocity));
			new_note->set_off_velocity (si.off_velocity);
			note_diff_add_note (new_note, true, true);
			pos += b;
		}
	}
}

double
MidiView::height() const
{
	return _midi_context.height();
}

void
MidiView::set_note_range (uint8_t low, uint8_t high)
{
	_midi_context.apply_note_range (low, high, true);
}

void
MidiView::set_visibility_note_range (MidiViewBackground::VisibleNoteRange nvr, bool)
{
	_midi_context.set_note_visibility_range_style (nvr);
}
