/*
    Copyright (C) 2010 Paul Davis

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

#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/utils.h"

#include "canvas-note-event.h"
#include "edit_note_dialog.h"
#include "midi_region_view.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;

/**
 *    EditNoteDialog constructor.
 *
 *    @param n Note to edit.
 */

EditNoteDialog::EditNoteDialog (MidiRegionView* rv, Gnome::Canvas::CanvasNoteEvent* ev)
	: ArdourDialog (_("Note"))
	, _region_view (rv)
	, _event (ev)
	, _time_clock (X_("notetime"), true, "", true, false)
	, _length_clock (X_("notelength"), true, "", true, false, true)
{
	Table* table = manage (new Table (4, 2));
	table->set_spacings (6);

	int r = 0;

	Label* l = manage (left_aligned_label (_("Channel")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_channel, 1, 2, r, r + 1);
	++r;

	_channel.set_range (1, 16);
	_channel.set_increments (1, 2);
	_channel.set_value (ev->note()->channel () + 1);

	l = manage (left_aligned_label (_("Pitch")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_pitch, 1, 2, r, r + 1);
	++r;

	_pitch.set_range (0, 127);
	_pitch.set_increments (1, 10);
	_pitch.set_value (ev->note()->note ());

	l = manage (left_aligned_label (_("Velocity")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_velocity, 1, 2, r, r + 1);
	++r;

	_velocity.set_range (0, 127);
	_velocity.set_increments (1, 10);
	_velocity.set_value (ev->note()->velocity ());

	l = manage (left_aligned_label (_("Time")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_time_clock, 1, 2, r, r + 1);
	++r;

	_time_clock.set_session (_region_view->get_time_axis_view().session ());
	_time_clock.set_mode (AudioClock::BBT);
	_time_clock.set (_region_view->source_relative_time_converter().to (ev->note()->time ()), true);

	l = manage (left_aligned_label (_("Length")));
	table->attach (*l, 0, 1, r, r + 1);
	table->attach (_length_clock, 1, 2, r, r + 1);
	++r;

	_length_clock.set_session (_region_view->get_time_axis_view().session ());
	_length_clock.set_mode (AudioClock::BBT);
	_length_clock.set (_region_view->region_relative_time_converter().to (ev->note()->length ()), true);

	get_vbox()->pack_start (*table);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_ACCEPT);
	set_default_response (Gtk::RESPONSE_ACCEPT);

	show_all ();
}

int
EditNoteDialog::run ()
{
	int const r = Dialog::run ();
	if (r != RESPONSE_ACCEPT) {
		return r;
	}

	/* These calls mean that if a value is entered using the keyboard
	   it will be returned by the get_value_as_int()s below.
	*/
	_channel.update ();
	_pitch.update ();
	_velocity.update ();

	_region_view->start_note_diff_command (_("edit note"));

	bool had_change = false;

	if (_channel.get_value_as_int() - 1 != _event->note()->channel()) {
		_region_view->change_note_channel (_event, _channel.get_value_as_int () - 1);
		had_change = true;
	}

	if (_pitch.get_value_as_int() != _event->note()->note()) {
		_region_view->change_note_note (_event, _pitch.get_value_as_int (), false);
		had_change = true;
	}

	if (_velocity.get_value_as_int() != _event->note()->velocity()) {
		_region_view->change_note_velocity (_event, _velocity.get_value_as_int (), false);
		had_change = true;
	}

	double const t = _region_view->source_relative_time_converter().from (_time_clock.current_time ());

	if (t != _event->note()->time()) {
		_region_view->change_note_time (_event, t);
		had_change = true;
	}

	double const d = _region_view->region_relative_time_converter().from (_length_clock.current_duration ());

	if (d != _event->note()->length()) {
		_region_view->change_note_length (_event, d);
		had_change = true;
	}

	if (!had_change) {
		_region_view->abort_command ();
	}

	_region_view->apply_diff ();

	_event->set_selected (_event->selected()); // change color

	return r;
}
