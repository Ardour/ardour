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

#include "ytkmm/label.h"
#include "ytkmm/entry.h"
#include "ytkmm/spinbutton.h"

#include "ardour/scale.h"

#include "ardour_dialog.h"

class ScaleDialog : public ArdourDialog
{
  public:
	ScaleDialog ();
	~ScaleDialog ();

	void set (ARDOUR::MusicalKey&);
	ARDOUR::MusicalKey get() const;

  private:
	ARDOUR::MusicalKey _key;

	struct StepEntry : public Gtk::Entry {
		StepEntry (int idx) : index (idx) {}
		int index;
	};

	Gtk::HBox step_packer;
	Gtk::HBox name_packer;
	Gtk::Label name_label;
	Gtk::Entry name_entry;
	Gtk::Adjustment step_adjustment;
	Gtk::SpinButton step_spinner;

	void pack ();
};
