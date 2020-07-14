/*
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
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

#ifndef __ardour_gtk2_quantize_dialog_h_
#define __ardour_gtk2_quantize_dialog_h_

#include <vector>
#include <string>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>

#include "temporal/beats.h"

#include "ardour/types.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
};

class PublicEditor;

class QuantizeDialog : public ArdourDialog
{
public:
	QuantizeDialog (PublicEditor&);
	~QuantizeDialog ();

	double start_grid_size() const;
	double end_grid_size() const;
	bool   snap_start() const { return snap_start_button.get_active(); }
	bool   snap_end() const { return snap_end_button.get_active(); }
	float  strength() const;
	Temporal::Beats  threshold () const;
	float  swing () const;

private:
	PublicEditor& editor;

	Gtk::ComboBoxText start_grid_combo;
	Gtk::ComboBoxText end_grid_combo;
	Gtk::Adjustment strength_adjustment;
	Gtk::SpinButton strength_spinner;
	Gtk::Label strength_label;
	Gtk::Adjustment swing_adjustment;
	Gtk::SpinButton swing_spinner;
	Gtk::CheckButton swing_button;
	Gtk::Adjustment threshold_adjustment;
	Gtk::SpinButton threshold_spinner;
	Gtk::Label threshold_label;
	Gtk::CheckButton snap_start_button;
	Gtk::CheckButton snap_end_button;

	static std::vector<std::string> grid_strings;
	static std::vector<std::string> type_strings;

	double grid_size_to_musical_time (const std::string&) const;
};

#endif /* __ardour_gtk2_quantize_dialog_h_ */
