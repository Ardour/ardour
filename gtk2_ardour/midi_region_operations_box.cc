/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include <algorithm>
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"

#include "midi_region_operations_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

MidiRegionOperationsBox::MidiRegionOperationsBox ()
{
	_header_label.set_text(_("MIDI Region Operations:"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	pack_start (table, false, false);

	table.set_homogeneous (true);
	table.set_spacings (4);
	table.set_border_width (8);
	table.set_col_spacings (2);

	quantize_button.set_text (_("Quantize..."));
	quantize_button.set_name ("generic button");
	quantize_button.signal_clicked.connect (sigc::mem_fun (*this, &MidiRegionOperationsBox::quantize_button_clicked));

	legatize_button.set_text (_("Legatize..."));
	legatize_button.set_name ("generic button");
	legatize_button.signal_clicked.connect (sigc::mem_fun (*this, &MidiRegionOperationsBox::legatize_button_clicked));

	transform_button.set_text (_("Transform..."));
	transform_button.set_name ("generic button");
	transform_button.signal_clicked.connect (sigc::mem_fun (*this, &MidiRegionOperationsBox::transform_button_clicked));

	int row = 0;
	table.attach(quantize_button,  0, 1, row, row+1, Gtk::SHRINK, Gtk::FILL|Gtk::EXPAND );  row++;
	table.attach(legatize_button,  0, 1, row, row+1, Gtk::SHRINK, Gtk::FILL|Gtk::EXPAND );  row++;
	table.attach(transform_button, 0, 1, row, row+1, Gtk::SHRINK, Gtk::FILL|Gtk::EXPAND );  row++;
}

MidiRegionOperationsBox::~MidiRegionOperationsBox ()
{
}

void
MidiRegionOperationsBox::quantize_button_clicked ()
{
	Editor::instance().quantize_region();
}

void
MidiRegionOperationsBox::legatize_button_clicked ()
{
	Editor::instance().legatize_region(true);
}

void
MidiRegionOperationsBox::transform_button_clicked ()
{
	Editor::instance().transform_region();
}
