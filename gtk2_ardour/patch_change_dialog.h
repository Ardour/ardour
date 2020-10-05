/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/spinbutton.h>
#include <gtkmm/comboboxtext.h>

#include "evoral/PatchChange.h"
#include "ardour_dialog.h"
#include "audio_clock.h"

namespace ARDOUR {
	class BeatsSamplesConverter;
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
		ARDOUR::Session*,
		Evoral::PatchChange<Temporal::Beats> const&,
		ARDOUR::InstrumentInfo&,
		const Gtk::BuiltinStockID&,
		bool allow_delete = false,
		bool modal = true,
		boost::shared_ptr<ARDOUR::Region> region = boost::shared_ptr<ARDOUR::Region>()
		);

	Evoral::PatchChange<Temporal::Beats> patch () const;

protected:
	void on_response (int);

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

	int get_14bit_bank () const;

	const boost::shared_ptr<ARDOUR::Region> _region;
	ARDOUR::InstrumentInfo& _info;
	AudioClock _time;
	Gtk::SpinButton   _channel;
	Gtk::SpinButton   _program;
	Gtk::SpinButton   _bank_msb;
	Gtk::SpinButton   _bank_lsb;
	Gtk::ComboBoxText _bank_combo;
	Gtk::ComboBoxText _patch_combo;

	boost::shared_ptr<MIDI::Name::PatchBank> _current_patch_bank;

	bool _ignore_signals;
	bool _keep_open;

	void instrument_info_changed ();
	PBD::ScopedConnection _info_changed_connection;
};
