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

#include <gtkmm/spinbutton.h>
#include <gtkmm/comboboxtext.h>

#include "evoral/PatchChange.hpp"
#include "ardour_dialog.h"
#include "audio_clock.h"

namespace ARDOUR {
	class BeatsFramesConverter;
	class Session;
	class InstrumentInfo;
}

namespace MIDI {
	namespace Name {
		class PatchBank;
	}
}

class PatchChangeDialog : public ArdourDialog
{
public:
	PatchChangeDialog (
		const ARDOUR::BeatsFramesConverter *,
		ARDOUR::Session *,
		Evoral::PatchChange<Evoral::MusicalTime> const &,
		ARDOUR::InstrumentInfo&,
		const Gtk::BuiltinStockID &,
		bool allow_delete = false
		);

	Evoral::PatchChange<Evoral::MusicalTime> patch () const;

private:
	void fill_bank_combo ();
	void set_active_bank_combo ();
	void fill_patch_combo ();
	void set_active_patch_combo ();
	void bank_combo_changed ();
	void patch_combo_changed ();
	void channel_changed ();
	void bank_changed ();
	void program_changed ();

	const ARDOUR::BeatsFramesConverter* _time_converter;
        ARDOUR::InstrumentInfo& _info;
	AudioClock _time;
	Gtk::SpinButton _channel;
	Gtk::SpinButton _program;
	Gtk::SpinButton _bank;
	Gtk::ComboBoxText _bank_combo;
	Gtk::ComboBoxText _patch_combo;

	boost::shared_ptr<MIDI::Name::PatchBank> _current_patch_bank;
	bool _ignore_signals;

        void instrument_info_changed ();
        PBD::ScopedConnection _info_changed_connection;
};
