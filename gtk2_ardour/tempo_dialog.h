/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_tempo_dialog_h__
#define __ardour_gtk_tempo_dialog_h__

#include <gtkmm/entry.h>
#include <gtkmm/frame.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/comboboxtext.h>

#include "ardour/types.h"
#include "ardour/tempo.h"

#include "ardour_dialog.h"

class TempoDialog : public ArdourDialog
{
public:
	TempoDialog (ARDOUR::TempoMap&, framepos_t, const std::string & action);
	TempoDialog (ARDOUR::TempoSection&, const std::string & action);

	double get_bpm ();
	double get_note_type ();
	bool   get_bbt_time (Timecode::BBT_Time&);

private:
	void init (const Timecode::BBT_Time& start, double, double, bool);
	void bpm_changed ();
	bool bpm_button_press (GdkEventButton* );
	bool bpm_button_release (GdkEventButton* );
	bool entry_key_release (GdkEventKey* );
	void pulse_change ();

	typedef std::map<std::string,float> NoteTypes;
	NoteTypes note_types;

	Gtk::ComboBoxText pulse_selector;
	Gtk::Adjustment   bpm_adjustment;
	Gtk::SpinButton   bpm_spinner;
	Gtk::Entry   when_bar_entry;
	Gtk::Entry   when_beat_entry;
	Gtk::Label   when_bar_label;
	Gtk::Label   when_beat_label;
	Gtk::Label   pulse_selector_label;
};

class MeterDialog : public ArdourDialog
{
public:

	MeterDialog (ARDOUR::TempoMap&, framepos_t, const std::string & action);
	MeterDialog (ARDOUR::MeterSection&, const std::string & action);

	double get_bpb ();
	double get_note_type ();
	bool   get_bbt_time (Timecode::BBT_Time&);

private:
	void init (const Timecode::BBT_Time&, double, double, bool);
	bool entry_key_press (GdkEventKey* );
	bool entry_key_release (GdkEventKey* );
	void note_type_change ();

	typedef std::map<std::string,float> NoteTypes;
	NoteTypes note_types;

	Gtk::Entry   bpb_entry;
	Gtk::ComboBoxText note_type;
	std::vector<std::string> strings;
	Gtk::Button  ok_button;
	Gtk::Button  cancel_button;
	Gtk::Entry   when_bar_entry;
};

#endif /* __ardour_gtk_tempo_dialog_h__ */
