/*
   Copyright (C) 2009 Paul Davis

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

#include <gtkmm/stock.h>
#include <gtkmm/table.h>
#include "gtkmm2ext/utils.h"

#include "pbd/convert.h"
#include "quantize_dialog.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;

static const gchar *_grid_strings[] = {
	N_("main grid"),
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/28"),
	N_("Beats/24"),
	N_("Beats/20"),
	N_("Beats/16"),
	N_("Beats/14"),
	N_("Beats/12"),
	N_("Beats/10"),
	N_("Beats/8"),
	N_("Beats/7"),
	N_("Beats/6"),
	N_("Beats/5"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats/2"),
	N_("Beats"),
	0
};

std::vector<std::string> QuantizeDialog::grid_strings;

QuantizeDialog::QuantizeDialog (PublicEditor& e)
	: ArdourDialog (_("Quantize"), false, false)
	, editor (e)
	, strength_adjustment (100.0, 0.0, 100.0, 1.0, 10.0)
	, strength_spinner (strength_adjustment)
	, strength_label (_("Strength"))
	, swing_adjustment (100.0, -130.0, 130.0, 1.0, 10.0)
	, swing_spinner (swing_adjustment)
	, swing_button (_("Swing"))
	, threshold_adjustment (0.0, -Timecode::BBT_Time::ticks_per_beat, Timecode::BBT_Time::ticks_per_beat, 1.0, 10.0)
	, threshold_spinner (threshold_adjustment)
	, threshold_label (_("Threshold (ticks)"))
	, snap_start_button (_("Snap note start"))
	, snap_end_button (_("Snap note end"))
{
	if (grid_strings.empty()) {
		grid_strings =  I18N (_grid_strings);
	}

	set_popdown_strings (start_grid_combo, grid_strings);
	start_grid_combo.set_active_text (grid_strings.front());
	set_popdown_strings (end_grid_combo, grid_strings);
	end_grid_combo.set_active_text (grid_strings.front());

	Table* table = manage (new Table (6, 2));
	table->set_spacings (12);
	table->set_border_width (12);

	int r = 0;

	table->attach (snap_start_button, 0, 1, r, r + 1);
	table->attach (start_grid_combo, 1, 2, r, r + 1);
	++r;

	table->attach (snap_end_button, 0, 1, r, r + 1);
	table->attach (end_grid_combo, 1, 2, r, r + 1);
	++r;

	threshold_label.set_alignment (0, 0.5);
	table->attach (threshold_label, 0, 1, r, r + 1);
	table->attach (threshold_spinner, 1, 2, r, r + 1);
	++r;

	strength_label.set_alignment (0, 0.5);
	table->attach (strength_label, 0, 1, r, r + 1);
	table->attach (strength_spinner, 1, 2, r, r + 1);
	++r;

	table->attach (swing_button, 0, 1, r, r + 1);
	table->attach (swing_spinner, 1, 2, r, r + 1);

	snap_start_button.set_active (true);
	snap_end_button.set_active (false);

	get_vbox()->pack_start (*table);
	show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (_("Quantize"), RESPONSE_OK);
}

QuantizeDialog::~QuantizeDialog()
{
}

double
QuantizeDialog::start_grid_size () const
{
	return grid_size_to_musical_time (start_grid_combo.get_active_text ());
}

double
QuantizeDialog::end_grid_size () const
{
	return grid_size_to_musical_time (start_grid_combo.get_active_text ());
}

double
QuantizeDialog::grid_size_to_musical_time (const string& txt) const
{
	if (txt == "main grid") {
		bool success;

		Evoral::Beats b = editor.get_grid_type_as_beats (success, 0);
		if (!success) {
			return 1.0;
		}
		return b.to_double();
	}

	string::size_type slash;

	if ((slash = txt.find ('/')) != string::npos) {
		if (slash < txt.length() - 1) {
			double divisor = PBD::atof (txt.substr (slash+1));
			if (divisor != 0.0) {
				return 1.0/divisor;
			}
		}
	}

	return 1.0;
}

float
QuantizeDialog::swing () const
{
	if (!swing_button.get_active()) {
		return 0.0f;
	}

	return swing_adjustment.get_value ();
}

float
QuantizeDialog::strength () const
{
	return strength_adjustment.get_value ();
}

float
QuantizeDialog::threshold () const
{
	return threshold_adjustment.get_value ();
}
