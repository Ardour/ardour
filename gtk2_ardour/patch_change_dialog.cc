/*
    Copyright (C) 2010 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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
#include "ardour/beats_frames_converter.h"
#include "patch_change_dialog.h"
#include "i18n.h"

using namespace Gtk;

/** @param tc If non-0, a time converter for this patch change.  If 0, time control will be desensitized */
PatchChangeDialog::PatchChangeDialog (
	const ARDOUR::BeatsFramesConverter* tc,
	ARDOUR::Session* session,
	Evoral::PatchChange<Evoral::MusicalTime> const & patch,
	const Gtk::BuiltinStockID& ok
	)
	: ArdourDialog (_("Patch Change"), true)
	, _time_converter (tc)
	, _time (X_("patchchangetime"), true, "", true, false)
	, _channel (*manage (new Adjustment (1, 1, 16, 1, 4)))
	, _program (*manage (new Adjustment (1, 1, 128, 1, 16)))
	, _bank (*manage (new Adjustment (1, 1, 16384, 1, 64)))
{
	Table* t = manage (new Table (4, 2));
	Label* l;
	t->set_spacings (6);
	int r = 0;

	if (_time_converter) {
		
		l = manage (new Label (_("Time")));
		l->set_alignment (0, 0.5);
		t->attach (*l, 0, 1, r, r + 1);
		t->attach (_time, 1, 2, r, r + 1);
		++r;

		_time.set_session (session);
		_time.set_mode (AudioClock::BBT);
		_time.set (_time_converter->to (patch.time ()), true);
	}

	l = manage (new Label (_("Channel")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_channel, 1, 2, r, r + 1);
	++r;
	
	_channel.set_value (patch.channel() + 1);

	l = manage (new Label (_("Program")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_program, 1, 2, r, r + 1);
	++r;

	_program.set_value (patch.program () + 1);

	l = manage (new Label (_("Bank")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, r, r + 1);
	t->attach (_bank, 1, 2, r, r + 1);
	++r;

	_bank.set_value (patch.bank() + 1);

	get_vbox()->add (*t);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (ok, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	show_all ();
}

Evoral::PatchChange<Evoral::MusicalTime>
PatchChangeDialog::patch () const
{
	Evoral::MusicalTime t = 0;

	if (_time_converter) {
		t = _time_converter->from (_time.current_time ());
	}

	return Evoral::PatchChange<Evoral::MusicalTime> (
		t,
		_channel.get_value_as_int() - 1,
		_program.get_value_as_int() - 1,
		_bank.get_value_as_int() - 1
		);
}
