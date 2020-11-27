/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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
	TempoDialog (Temporal::TempoMap::SharedPtr const &, Temporal::timepos_t const & , const std::string & action);
	TempoDialog (Temporal::TempoMap::SharedPtr const &, Temporal::TempoPoint&, const std::string & action);

	double get_bpm ();
	double get_end_bpm ();
	double get_note_type ();
	bool   get_bbt_time (Temporal::BBT_Time&);
	Temporal::Tempo::Type get_tempo_type ();
	Temporal::TimeDomain get_lock_style ();

private:
	void init (const Temporal::BBT_Time& start, double bpm, double end_bpm, double note_type, Temporal::TempoPoint::Type type, bool movable, Temporal::TimeDomain style);
	bool is_user_input_valid() const;
	void bpm_changed ();
	bool bpm_button_press (GdkEventButton* );
	bool bpm_button_release (GdkEventButton* );
	bool entry_key_release (GdkEventKey* );
	void pulse_change ();
	void tempo_type_change ();
	void lock_style_change ();
	bool tap_tempo_key_press (GdkEventKey*);
	bool tap_tempo_button_press (GdkEventButton*);
	bool tap_tempo_focus_out (GdkEventFocus* );

	void tap_tempo ();

	typedef std::map<std::string,float> NoteTypes;
	NoteTypes note_types;

	typedef std::map<std::string, Temporal::Tempo::Type> TempoTypes;
	TempoTypes tempo_types;

	typedef std::map<std::string, Temporal::TimeDomain> LockStyles;
	LockStyles lock_styles;

	bool tapped;      // whether the tap-tempo button has been clicked
	double sum_x, sum_xx, sum_xy, sum_y;
	double tap_count;
	double last_t;
	gint64 first_t;

	Temporal::TempoMap::SharedPtr _map;
	Temporal::TempoPoint* _section;

	Gtk::ComboBoxText pulse_selector;
	Gtk::Adjustment   bpm_adjustment;
	Gtk::SpinButton   bpm_spinner;
	Gtk::Adjustment   end_bpm_adjustment;
	Gtk::SpinButton   end_bpm_spinner;
	Gtk::Label   _end_bpm_label;
	Gtk::Entry   when_bar_entry;
	Gtk::Entry   when_beat_entry;
	Gtk::Label   when_bar_label;
	Gtk::Label   when_beat_label;
	Gtk::Label   pulse_selector_label;
	Gtk::Button  tap_tempo_button;
	Gtk::ComboBoxText tempo_type;
	Gtk::ComboBoxText lock_style;
};

class MeterDialog : public ArdourDialog
{
public:
	MeterDialog (Temporal::TempoMap::SharedPtr const & , Temporal::timepos_t const &, const std::string & action);
	MeterDialog (Temporal::MeterPoint&, const std::string & action);

	double get_bpb ();
	double get_note_type ();
	Temporal::TimeDomain get_lock_style ();
	bool   get_bbt_time (Temporal::BBT_Time&);

private:
	void init (const Temporal::BBT_Time&, double, double, bool, Temporal::TimeDomain style);
	bool is_user_input_valid() const;
	bool entry_key_press (GdkEventKey* );
	bool entry_key_release (GdkEventKey* );
	void note_type_change ();
	void lock_style_change ();

	typedef std::map<std::string,float> NoteTypes;
	NoteTypes note_types;

	typedef std::map<std::string, Temporal::TimeDomain> LockStyles;
	LockStyles lock_styles;

	Gtk::Entry   bpb_entry;
	Gtk::ComboBoxText note_type;
	Gtk::ComboBoxText lock_style;
	std::vector<std::string> strings;
	Gtk::Button  ok_button;
	Gtk::Button  cancel_button;
	Gtk::Entry   when_bar_entry;
};

#endif /* __ardour_gtk_tempo_dialog_h__ */
