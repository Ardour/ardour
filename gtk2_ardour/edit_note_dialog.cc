/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/utils.h"

#include "edit_note_dialog.h"
#include "midi_region_view.h"
#include "note_base.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Temporal;
using namespace Gtk;
using namespace Gtkmm2ext;

/**
 *    EditNoteDialog constructor.
 *
 *    @param n Notes to edit.
 */

EditNoteDialog::EditNoteDialog (MidiRegionView* rv, set<NoteBase*> n)
	: ArdourDialog (_("Note"))
	, _region_view (rv)
	, _events (n)
	, _channel_all (_("Set selected notes to this channel"))
	, _pitch_all (_("Set selected notes to this pitch"))
	, _velocity_all (_("Set selected notes to this velocity"))
	, _time_clock (X_("notetime"), true, "", true, false)
	, _time_all (_("Set selected notes to this time"))
	, _length_clock (X_("notelength"), true, "", true, false, true)
	, _length_all (_("Set selected notes to this length"))
{
	Table* table = manage (new Table (4, 2));
	table->set_spacings (6);

	int r = 0;

	Label* l = manage (left_aligned_label (_("Channel")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_channel, 1, 2, r, r + 1);
	table->attach (_channel_all, 2, 3, r, r + 1);
	++r;

	_channel.set_range (1, 16);
	_channel.set_increments (1, 2);
	_channel.set_value ((*_events.begin())->note()->channel () + 1);

	l = manage (left_aligned_label (_("Pitch")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_pitch, 1, 2, r, r + 1);
	table->attach (_pitch_all, 2, 3, r, r + 1);
	++r;

	_pitch.set_range (0, 127);
	_pitch.set_increments (1, 10);
	_pitch.set_value ((*_events.begin())->note()->note());

	l = manage (left_aligned_label (_("Velocity")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_velocity, 1, 2, r, r + 1);
	table->attach (_velocity_all, 2, 3, r, r + 1);
	++r;

	_velocity.set_range (0, 127);
	_velocity.set_increments (1, 10);
	_velocity.set_value ((*_events.begin())->note()->velocity ());

	l = manage (left_aligned_label (_("Time")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_time_clock, 1, 2, r, r + 1);
	table->attach (_time_all, 2, 3, r, r + 1);
	++r;

	_time_clock.set_session (_region_view->get_time_axis_view().session ());
	_time_clock.set_mode (AudioClock::BBT);

	timecnt_t dur = _region_view->source_relative_distance (timecnt_t ((*_events.begin())->note()->time(), timepos_t()), BeatTime);
	timepos_t pos = _region_view->region()->source_position() + dur;

	_time_clock.set (pos, true);

	l = manage (left_aligned_label (_("Length")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_length_clock, 1, 2, r, r + 1);
	table->attach (_length_all, 2, 3, r, r + 1);
	++r;

	_length_clock.set_session (_region_view->get_time_axis_view().session ());
	_length_clock.set_mode (AudioClock::BBT);

	dur = _region_view->region_relative_distance (timecnt_t ((*_events.begin())->note()->end_time (), timepos_t()), BeatTime);
	pos = _region_view->region()->position() + dur;
	timecnt_t offset;
	dur = _region_view->region_relative_distance (timecnt_t ((*_events.begin())->note()->time (), timepos_t()), BeatTime);
	offset = timecnt_t (_region_view->region()->position(), timepos_t()) + dur;

	_length_clock.set_is_duration (true, pos);
	_length_clock.set_duration (offset, true);

	/* Set up `set all notes...' buttons' sensitivity */

	_channel_all.set_sensitive (false);
	_pitch_all.set_sensitive (false);
	_velocity_all.set_sensitive (false);
	_time_all.set_sensitive (false);
	_length_all.set_sensitive (false);

	int test_channel = (*_events.begin())->note()->channel ();
	int test_pitch = (*_events.begin())->note()->note ();
	int test_velocity = (*_events.begin())->note()->velocity ();
	Temporal::Beats test_time = (*_events.begin())->note()->time ();
	Temporal::Beats test_length = (*_events.begin())->note()->length ();

	for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->note()->channel() != test_channel) {
			_channel_all.set_sensitive (true);
		}

		if ((*i)->note()->note() != test_pitch) {
			_pitch_all.set_sensitive (true);
		}

		if ((*i)->note()->velocity() != test_velocity) {
			_velocity_all.set_sensitive (true);
		}

		if ((*i)->note()->time () != test_time) {
			_time_all.set_sensitive (true);
		}

		if ((*i)->note()->length () != test_length) {
			_length_all.set_sensitive (true);
		}
	}

	get_vbox()->pack_start (*table);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_ACCEPT);
	set_default_response (Gtk::RESPONSE_ACCEPT);
}

void
EditNoteDialog::done (int r)
{
	if (r != RESPONSE_ACCEPT) {
		return;
	}

	/* These calls mean that if a value is entered using the keyboard
	   it will be returned by the get_value_as_int()s below.
	*/
	_channel.update ();
	_pitch.update ();
	_velocity.update ();

	_region_view->start_note_diff_command (_("edit note"));

	bool had_change = false;

	if (!_channel_all.get_sensitive() || _channel_all.get_active ()) {
		for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
			if (_channel.get_value_as_int() - 1 != (*i)->note()->channel()) {
				_region_view->change_note_channel (*i, _channel.get_value_as_int () - 1);
				had_change = true;
			}
		}
	}

	if (!_pitch_all.get_sensitive() || _pitch_all.get_active ()) {
		for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
			if (_pitch.get_value_as_int() != (*i)->note()->note()) {
				_region_view->change_note_note (*i, _pitch.get_value_as_int ());
				had_change = true;
			}
		}
	}

	if (!_velocity_all.get_sensitive() || _velocity_all.get_active ()) {
		for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
			if (_velocity.get_value_as_int() != (*i)->note()->velocity()) {
				_region_view->change_note_velocity (*i, _velocity.get_value_as_int ());
				had_change = true;
			}
		}
	}

	timepos_t source_start = _region_view->region()->position().earlier (_region_view->region()->start());

	/* convert current clock time into an offset from the start of the source */

	timepos_t time_clock_source_relative = _time_clock.current_time().earlier (source_start);

	/* convert that into a position in Beats - this will be the new note time (as an offset inside the source) */

	Beats const new_note_time_source_relative_beats = time_clock_source_relative.beats ();

	if (!_time_all.get_sensitive() || _time_all.get_active ()) {
		for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
			if (new_note_time_source_relative_beats != (*i)->note()->time()) {
				_region_view->change_note_time (*i, new_note_time_source_relative_beats);
				had_change = true;
			}
		}
	}
	if (!_length_all.get_sensitive() || _length_all.get_active ()) {


		/* get current note duration, interpreted as beats at the time indicated by the _time_clock (the new note position) */
		Beats const duration = _length_clock.current_duration (_time_clock.current_time()).beats ();

		/* compute end of note */
		Beats const new_note_end_source_relative_beats = new_note_time_source_relative_beats + duration;

		for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
			Beats const new_note_length_beats = new_note_end_source_relative_beats - (*i)->note()->time();
			if (new_note_length_beats != (*i)->note()->length()) {
				_region_view->change_note_length (*i, new_note_length_beats);
				had_change = true;
			}
		}

	}

	if (!had_change) {
		_region_view->abort_command ();
	}

	_region_view->apply_diff ();

	list<Evoral::event_id_t> notes;
	for (set<NoteBase*>::iterator i = _events.begin(); i != _events.end(); ++i) {
		notes.push_back ((*i)->note()->id());
	}

	_region_view->select_notes (notes, true);
}
