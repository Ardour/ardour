/*
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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
	N_("Main Grid"),
	N_("1/4 Note"),
	N_("1/8 Note"),
	N_("1/16 Note"),
	N_("1/32 Note"),
	N_("1/64 Note"),
	N_("1/128 Note"),
	
	N_("1/3 (8th triplet)"),
	N_("1/6 (16th triplet)"),
	N_("1/12 (32nd triplet)"),

	N_("1/5 (8th quintuplet)"),
	N_("1/10 (16th quintuplet)"),
	N_("1/20 (32nd quintuplet)"),

	N_("1/7 (8th septuplet)"),
	N_("1/14 (16th septuplet)"),
	N_("1/28 (32nd septuplet)"),

	0
};

static const int _grid_beats[] = {
	0,
	1, 2, 4, 8, 16, 32,
	3, 6, 12,
	5, 10, 20,
	7, 14, 28,
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
	get_vbox()->show_all ();

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
	return grid_size_to_musical_time (end_grid_combo.get_active_text ());
}

double
QuantizeDialog::grid_size_to_musical_time (const string& txt) const
{
	if ( txt == _grid_strings[0] ) {  //"Main Grid"
		bool success;

		Temporal::Beats b = editor.get_grid_type_as_beats (success, 0);
		if (!success) {
			return 1.0;
		}
		return b.to_double();
	}


	double divisor = 1.0;
	for (size_t i = 1; i < grid_strings.size(); ++i) {
		if (txt == grid_strings[i]) {
			assert (_grid_beats[i] != 0);
			divisor = 1.0 / _grid_beats[i];
			break;
		}
	}

	return divisor;
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

Temporal::Beats
QuantizeDialog::threshold () const
{
	return Temporal::Beats::from_double (threshold_adjustment.get_value ());
}
