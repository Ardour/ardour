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

#include <gtkmm/spinbutton.h>
#include "ardour/midi_model.h"
#include "ardour_dialog.h"
#include "audio_clock.h"

class MidiRegionView;
class NoteBase;

class EditNoteDialog : public ArdourDialog
{
public:
	EditNoteDialog (MidiRegionView *, std::set<NoteBase*>);

	int run ();

private:
	MidiRegionView* _region_view;
	std::set<NoteBase*> _events;
	Gtk::SpinButton _channel;
	Gtk::CheckButton _channel_all;
	Gtk::SpinButton _pitch;
	Gtk::CheckButton _pitch_all;
	Gtk::SpinButton _velocity;
	Gtk::CheckButton _velocity_all;
	AudioClock _time_clock;
	Gtk::CheckButton _time_all;
	AudioClock _length_clock;
	Gtk::CheckButton _length_all;
};
