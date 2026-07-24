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
#include "ytkmm/entry.h"
#include "ytkmm/label.h"
#include "ytkmm/sizegroup.h"

#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_button.h"

#include "ardour/chord_provider.h"

class EditingContext;

class ChordEditor : public Gtk::VBox
{
  public:
	ChordEditor (EditingContext&, ARDOUR::ChordProvider&, int chord_size);
	~ChordEditor();

	void show_chord (std::vector<int> const & intervals);
	void set_protected (bool);
	bool is_protected () const;
	ARDOUR::ChordProvider::ChordInfo get_chord () const;
	int chord_size() const { return _chord_size; }

 private:
	ARDOUR::ChordProvider& chord_provider;
	int _chord_size;

	Gtk::HBox upper_interval_packer;
	Gtk::HBox lower_interval_packer;
	Glib::RefPtr<Gtk::SizeGroup> button_sizing;
	std::vector<ArdourWidgets::ArdourButton*> interval_buttons;
	std::vector<Gtk::Label> labels;

	Gtk::HBox  canonical_box;
	Gtk::Entry canonical_entry;
	Gtk::Label canonical_label;

	Gtk::HBox  short_box;
	Gtk::Entry short_entry;
	Gtk::Label short_label;

	Gtk::HBox  other_box;
	Gtk::Entry other_entry;
	Gtk::Label other_label;
};
