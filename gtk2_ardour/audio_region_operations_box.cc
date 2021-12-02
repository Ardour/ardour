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

#include "audio_region_operations_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

AudioRegionOperationsBox::AudioRegionOperationsBox ()
{
	_header_label.set_text(_("AUDIO Region Operations:"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	pack_start (table, false, false);

	table.set_homogeneous (true);
	table.set_spacings (4);
	table.set_border_width (8);
	table.set_col_spacings (2);

	reverse_button.set_text (_("Reverse"));
	reverse_button.set_name ("generic button");
	reverse_button.signal_clicked.connect (sigc::mem_fun (*this, &AudioRegionOperationsBox::reverse_button_clicked));

	shift_button.set_text (_("Pitch Shift..."));
	shift_button.set_name ("generic button");
	shift_button.signal_clicked.connect (sigc::mem_fun (*this, &AudioRegionOperationsBox::shift_button_clicked));

	normalize_button.set_text (_("Normalize..."));
	normalize_button.set_name ("generic button");
	normalize_button.signal_clicked.connect (sigc::mem_fun (*this, &AudioRegionOperationsBox::normalize_button_clicked));

	int row = 0;
	table.attach(reverse_button,   0, 1, row, row+1, Gtk::FILL, Gtk::FILL|Gtk::EXPAND );  row++;
	table.attach(shift_button,     0, 1, row, row+1, Gtk::FILL, Gtk::FILL|Gtk::EXPAND );  row++;
	table.attach(normalize_button, 0, 1, row, row+1, Gtk::FILL, Gtk::FILL|Gtk::EXPAND );  row++;
}

AudioRegionOperationsBox::~AudioRegionOperationsBox ()
{
}

void
AudioRegionOperationsBox::reverse_button_clicked ()
{
	Editor::instance().reverse_region();
}

void
AudioRegionOperationsBox::shift_button_clicked ()
{
	Editor::instance().pitch_shift_region();
}

void
AudioRegionOperationsBox::normalize_button_clicked ()
{
	Editor::instance().normalize_region();
}
