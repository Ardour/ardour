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

#include "gtkmm2ext/utils.h"

#include "quantize_dialog.h"
#include "public_editor.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;

static const gchar *_grid_strings[] = {
	N_("main grid"),
	N_("Beats/128"),
	N_("Beats/64"),
	N_("Beats/32"),
	N_("Beats/16"),
	N_("Beats/8"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats"),
	0
};

static const gchar *_type_strings[] = {
	N_("Grid"),
	N_("Legato"),
	N_("Groove"),
	0
};

std::vector<std::string> QuantizeDialog::grid_strings;
std::vector<std::string> QuantizeDialog::type_strings;

QuantizeDialog::QuantizeDialog (PublicEditor& e)
	: ArdourDialog (_("Quantize"), false, false)
	, editor (e)
	, type_label (_("Quantize Type"))
	, strength_adjustment (100.0, 0.0, 100.0, 1.0, 10.0)
	, strength_spinner (strength_adjustment)
	, strength_label (_("Strength"))
	, swing_adjustment (100.0, -130.0, 130.0, 1.0, 10.0)
	, swing_spinner (swing_adjustment)
	, swing_button (_("Swing"))
	, threshold_adjustment (0.0, -1920.0, 1920.0, 1.0, 10.0) // XXX MAGIC TICK NUMBER FIX ME
	, threshold_spinner (threshold_adjustment)
	, threshold_label (_("Threshold (ticks)"))
	, snap_start_button (_("Snap note start"))
	, snap_end_button (_("Snap note end"))
{
	if (grid_strings.empty()) {
		grid_strings =  I18N (_grid_strings);
		type_strings =  I18N (_type_strings);
	}

	set_popdown_strings (start_grid_combo, grid_strings);
	start_grid_combo.set_active_text (grid_strings.front());
	set_popdown_strings (end_grid_combo, grid_strings);
	end_grid_combo.set_active_text (grid_strings.front());

	set_popdown_strings (type_combo, type_strings);
	type_combo.set_active_text (type_strings.front());

	get_vbox()->set_border_width (12);

	HBox* hbox;

	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->set_border_width (6);
	hbox->pack_start (type_label);
	hbox->pack_start (type_combo);
	hbox->show ();
	type_label.show ();
	type_combo.show ();
	get_vbox()->pack_start (*hbox);

	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->set_border_width (6);
	hbox->pack_start (snap_start_button);
	hbox->pack_start (start_grid_combo);
	hbox->show ();
	snap_start_button.show ();
	start_grid_combo.show ();
	get_vbox()->pack_start (*hbox);

	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->set_border_width (6);
	hbox->pack_start (snap_end_button);
	hbox->pack_start (end_grid_combo);
	hbox->show ();
	snap_end_button.show ();
	end_grid_combo.show ();
	get_vbox()->pack_start (*hbox);

	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->set_border_width (6);
	hbox->pack_start (threshold_label);
	hbox->pack_start (threshold_spinner);
	hbox->show ();
	threshold_label.show ();
	threshold_spinner.show ();
	get_vbox()->pack_start (*hbox);

	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->set_border_width (6);
	hbox->pack_start (strength_label);
	hbox->pack_start (strength_spinner);
	hbox->show ();
	strength_label.show ();
	strength_spinner.show ();
	get_vbox()->pack_start (*hbox);

	hbox = manage (new HBox);
	hbox->set_spacing (12);
	hbox->set_border_width (6);
	hbox->pack_start (swing_button);
	hbox->pack_start (swing_spinner);
	hbox->show ();
	swing_button.show ();
	swing_spinner.show ();
	get_vbox()->pack_start (*hbox);

	snap_start_button.set_active (true);
	snap_end_button.set_active (false);
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
	if (txt == "main_grid") {
		bool success;

		Evoral::MusicalTime b = editor.get_grid_type_as_beats (success, 0);
		if (!success) {
			return 1.0;
		}
		return (double) b;
	}

	if (txt == _("Beats/128")) {
		return 1.0/128.0;
	} else if (txt == _("Beats/64")) {
		return 1.0/64.0;
	} else if (txt == _("Beats/32")) {
		return 1.0/32.0;
	} else if (txt == _("Beats/16")) {
		return 1.0/16.0;
	} if (txt == _("Beats/8")) {
		return 1.0/8.0;
	} else if (txt == _("Beats/4")) {
		return 1.0/4.0;
	} else if (txt == _("Beats/3")) {
		return 1.0/3.0;
	} else if (txt == _("Beats")) {
		return 1.0;
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
