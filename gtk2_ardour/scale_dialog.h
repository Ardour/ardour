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

#include <map>

#include "ytkmm/label.h"
#include "ytkmm/entry.h"
#include "ytkmm/spinbutton.h"
#include "ytkmm/filechooserbutton.h"

#include "ardour/scale.h"

#include "widgets/ardour_dropdown.h"

#include "ardour_dialog.h"

class ScaleDialog : public ArdourDialog
{
  public:
	ScaleDialog ();
	~ScaleDialog ();

	void set (ARDOUR::MusicalKey const &);
	ARDOUR::MusicalKey get() const;

  private:
	static std::map<ARDOUR::MusicalModeType,std::string> type_string_map;
	static std::map<std::string,ARDOUR::MusicalModeType> string_type_map;
	static void fill_maps ();

	ARDOUR::MusicalKey _key;

	struct StepEntry : public Gtk::Entry {
		StepEntry (int idx) : index (idx) {}
		int index;
	};

	Gtk::VBox step_packer;
	Gtk::HBox name_packer;
	Gtk::Label name_label;
	Gtk::HBox type_box;
	Gtk::Label type_label;
	Gtk::Entry name_entry;
	Gtk::Adjustment step_adjustment;
	Gtk::Label steps_label;
	Gtk::SpinButton step_spinner;
	Gtk::HBox steps_box;
	ArdourWidgets::ArdourDropdown type_dropdown;
	Gtk::HBox scala_box;
	Gtk::Label scala_label;
	Gtk::FileChooserButton scala_file_button;
	Gtk::Button clear_button;

	ArdourWidgets::ArdourDropdown culture_dropdown;
	ArdourWidgets::ArdourDropdown root_dropdown;
	ArdourWidgets::ArdourDropdown mode_dropdown;
	Gtk::HBox root_mode_box;
	Gtk::VBox named_scale_box;

	void pack_steps ();
	void fill_dropdowns (ARDOUR::MusicalModeCulture);
	void set_type (ARDOUR::MusicalModeType);
};
