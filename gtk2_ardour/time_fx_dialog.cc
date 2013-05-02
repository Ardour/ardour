/*
    Copyright (C) 2000-2009 Paul Davis

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

#include "time_fx_dialog.h"

#include <iostream>
#include <cstdlib>
#include <cmath>

#include <string>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"

#include <gtkmm2ext/utils.h>

#include "editor.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "region_selection.h"

#ifdef USE_RUBBERBAND
#include "rubberband/RubberBandStretcher.h"
using namespace RubberBand;
#endif

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

TimeFXDialog::TimeFXDialog (Editor& e, bool pitch)
	: ArdourDialog (X_("time fx dialog"))
	, editor (e)
	, pitching (pitch)
	, pitch_octave_adjustment (0.0, -4.0, 4.0, 1, 2.0)
	, pitch_semitone_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, pitch_cent_adjustment (0.0, -499.0, 500.0, 5.0, 15.0)
	, pitch_octave_spinner (pitch_octave_adjustment)
	, pitch_semitone_spinner (pitch_semitone_adjustment)
	, pitch_cent_spinner (pitch_cent_adjustment)
	, quick_button (_("Quick but Ugly"))
	, antialias_button (_("Skip Anti-aliasing"))
	, stretch_opts_label (_("Contents:"))
	, precise_button (_("Minimize time distortion"))
	, preserve_formants_button(_("Preserve Formants"))
{
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);
	set_name (N_("TimeFXDialog"));

	if (pitching) {
		set_title (_("Pitch Shift Audio"));
	} else {
		set_title (_("Time Stretch Audio"));
	}

	cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	VBox* vbox = manage (new VBox);
	Gtk::Label* l;

	get_vbox()->set_spacing (4);

	vbox->set_spacing (18);
	vbox->set_border_width (5);

	upper_button_box.set_spacing (6);

	l = manage (new Label (_("<b>Options</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false ));
	l->set_use_markup ();

	upper_button_box.pack_start (*l, false, false);

	if (pitching) {
		Table* table = manage (new Table (4, 3, false));
		table->set_row_spacings	(6);
		table->set_col_spacing	(1, 6);
		l = manage (new Label ("", Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false )); //Common gnome way for padding
		l->set_padding (8, 0);
		table->attach (*l, 0, 1, 0, 4, Gtk::FILL, Gtk::FILL, 0, 0);

		l = manage (new Label (_("Octaves:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		table->attach (*l, 1, 2, 0, 1, Gtk::FILL, Gtk::EXPAND, 0, 0);
		table->attach (pitch_octave_spinner, 2, 3, 0, 1, Gtk::FILL, Gtk::EXPAND & Gtk::FILL, 0, 0);
		pitch_octave_spinner.set_activates_default ();

		l = manage (new Label (_("Semitones:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		table->attach (*l, 1, 2, 1, 2, Gtk::FILL, Gtk::EXPAND, 0, 0);
		table->attach (pitch_semitone_spinner, 2, 3, 1, 2, Gtk::FILL, Gtk::EXPAND & Gtk::FILL, 0, 0);
		pitch_semitone_spinner.set_activates_default ();

		l = manage (new Label (_("Cents:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		pitch_cent_spinner.set_digits (1);
		table->attach (*l, 1, 2, 2, 3, Gtk::FILL, Gtk::EXPAND, 0, 0);
		table->attach (pitch_cent_spinner, 2, 3, 2, 3, Gtk::FILL, Gtk::EXPAND & Gtk::FILL, 0, 0);
		pitch_cent_spinner.set_activates_default ();

		table->attach (preserve_formants_button, 1, 3, 3, 4, Gtk::FILL, Gtk::EXPAND, 0, 0);

		add_button (S_("Time|Shift"), Gtk::RESPONSE_ACCEPT);

		upper_button_box.pack_start (*table, false, true);
	} else {
		Table* table = manage (new Table (2, 3, false));
		table->set_row_spacings	(6);
		table->set_col_spacing	(1, 6);
		l = manage (new Label ("", Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false ));
		l->set_padding (8, 0);
		table->attach (*l, 0, 1, 0, 2, Gtk::FILL, Gtk::FILL, 0, 0);

#ifdef USE_RUBBERBAND
		vector<string> strings;

		table->attach (stretch_opts_label, 1, 2, 0, 1, Gtk::FILL, Gtk::EXPAND, 0, 0);

		set_popdown_strings (stretch_opts_selector, editor.rb_opt_strings);
		/* set default */
		stretch_opts_selector.set_active_text (editor.rb_opt_strings[editor.rb_current_opt]);
		table->attach (stretch_opts_selector, 2, 3, 0, 1, Gtk::FILL, Gtk::EXPAND & Gtk::FILL, 0, 0);

		table->attach (precise_button, 1, 3, 1, 2, Gtk::FILL, Gtk::EXPAND, 0, 0);

#else
		quick_button.set_name (N_("TimeFXButton"));
		table->attach (quick_button, 1, 3, 0, 1, Gtk::FILL, Gtk::EXPAND, 0, 0);

		antialias_button.set_name (N_("TimeFXButton"));
		table->attach (antialias_button, 1, 3, 1, 2, Gtk::FILL, Gtk::EXPAND, 0, 0);

#endif

		add_button (_("Stretch/Shrink"), Gtk::RESPONSE_ACCEPT);

		upper_button_box.pack_start (*table, false, true);
	}

	set_default_response (Gtk::RESPONSE_ACCEPT);
	
	VBox* progress_box = manage (new VBox);
	progress_box->set_spacing (6);

	l = manage (new Label (_("<b>Progress</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();

	progress_box->pack_start (*l, false, false);
	progress_box->pack_start (progress_bar, false, true);


	vbox->pack_start (upper_button_box, false, true);
	vbox->pack_start (*progress_box, false, true);

	get_vbox()->pack_start (*vbox, false, false);

	show_all_children ();
}

void
TimeFXDialog::update_progress_gui (float p)
{
	progress_bar.set_fraction (p);
}

void
TimeFXDialog::cancel_in_progress ()
{
	status = -2;
	request.cancel = true;
	first_cancel.disconnect();
}

gint
TimeFXDialog::delete_in_progress (GdkEventAny*)
{
	status = -2;
	request.cancel = true;
	first_delete.disconnect();
	return TRUE;
}

