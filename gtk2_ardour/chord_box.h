/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <string>
#include <vector>

#include "ytkmm/box.h"
#include "ytkmm/label.h"
#include "ytkmm/table.h"

#include "widgets/ardour_dropdown.h"

#include "ardour/scale.h"
#include "ardour/chord_provider.h"

namespace ARDOUR {
	class ScaleProvider;
}

class EditingContext;

class ChordBox : public Gtk::VBox, public ARDOUR::ChordProvider
{
  public:
	ChordBox (EditingContext&);
	~ChordBox();

	void set_culture (ARDOUR::MusicalModeCulture);

	typedef std::vector<int> IntervalSet;

	bool get_midi_chord (int root_pitch, IntervalSet& pitches, bool& arpeggiate) const;
	void show_chord (std::string const & name);

	sigc::signal<void, IntervalSet> ReplaceChord;
	sigc::signal<void, bool> InvertChord;
	sigc::signal<void, std::vector<int> > DropChord;

 private:
	EditingContext& editing_context;
	void pack (Gtk::Widget&);

	ArdourWidgets::ArdourDropdown culture_button;
	Gtk::Label name_display;

	void refill_tables ();
	void fill_table (Gtk::Table& table, std::vector<std::string> const & names, int chord_size);
	bool radio_ardour_button_hack (GdkEventButton* ev, ArdourWidgets::ArdourButton* button);

	/* Western */

	Gtk::Table triad_table;
	Gtk::Table tetrad_table;
	Gtk::Table ext_table;

	Gtk::Table inversion_table;
	Gtk::Table drop_table;
	Gtk::CheckButton arpeggiate_button;

	Gtk::Label triad_label;
	Gtk::Label tetrad_label;
	Gtk::Label ext_label;
	Gtk::Label inversion_label;
	Gtk::Label drop_label;

	Gtk::VBox western_vbox;

	void build_western ();

	void tet12_replace_chord (std::string const &);
	bool tet12_edit_chord (GdkEventButton*, size_t n, int chord_size);
	void tet12_modify_chord (std::string const &);
	void tet12_invert_chord (bool);
	void tet12_drop_chord (std::vector<int> const &);

	/* end western */

	int _root;
	ARDOUR::MusicalModeCulture _culture;
	PBD::ScopedConnection chord_connection;
};
