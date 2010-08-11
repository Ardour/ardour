/*
    Copyright (C) 2000-2010 Paul Davis

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

#include "ardour_dialog.h"
#include "public_editor.h"
#include "editing.h"
#include "audio_clock.h"

class InsertTimeDialog : public ArdourDialog
{
public:
	InsertTimeDialog (PublicEditor &);

	Editing::InsertTimeOption intersected_region_action ();
	bool move_glued () const;
	bool move_markers () const;
	bool move_tempos () const;
	nframes64_t distance () const;

private:
	PublicEditor& _editor;
	Gtk::ComboBoxText _intersected_combo;
	Gtk::CheckButton _move_glued;
	Gtk::CheckButton _move_markers;
	Gtk::CheckButton _move_tempos;
	AudioClock _clock;
};
