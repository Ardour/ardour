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
#include "evoral/PatchChange.hpp"
#include "ardour_dialog.h"
#include "audio_clock.h"

namespace ARDOUR {
	class BeatsFramesConverter;
	class Session;
}

class PatchChangeDialog : public ArdourDialog
{
public:
	PatchChangeDialog (
		const ARDOUR::BeatsFramesConverter *,
		ARDOUR::Session *,
		Evoral::PatchChange<Evoral::MusicalTime> const &,
		const Gtk::BuiltinStockID &
		);

	Evoral::PatchChange<Evoral::MusicalTime> patch () const;

private:
	const ARDOUR::BeatsFramesConverter* _time_converter;
	AudioClock _time;
	Gtk::SpinButton _channel;
	Gtk::SpinButton _program;
	Gtk::SpinButton _bank;
};
