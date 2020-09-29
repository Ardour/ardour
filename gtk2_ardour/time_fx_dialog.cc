/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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


#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string>

#include <gtkmm/stock.h>

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"
#include "pbd/unwind.h"

#include "gtkmm2ext/utils.h"

#include "audio_clock.h"
#include "editor.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "region_selection.h"
#include "time_fx_dialog.h"
#include "timers.h"

#ifdef USE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
using namespace RubberBand;
#endif

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

TimeFXDialog::TimeFXDialog (Editor& e, bool pitch, timecnt_t const & oldlen, timecnt_t const & new_length, timepos_t const & position)
	: ArdourDialog (X_("time fx dialog"))
	, editor (e)
	, pitching (pitch)
	, quick_button (_("Quick but Ugly"))
	, antialias_button (_("Skip Anti-aliasing"))
	, stretch_opts_label (_("Contents"))
	, precise_button (_("Minimize time distortion"))
	, preserve_formants_button(_("Preserve Formants"))
	, original_length (oldlen)
	, pitch_octave_adjustment (0.0, -4.0, 4.0, 1, 2.0)
	, pitch_semitone_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, pitch_cent_adjustment (0.0, -499.0, 500.0, 5.0, 15.0)
	, pitch_octave_spinner (pitch_octave_adjustment)
	, pitch_semitone_spinner (pitch_semitone_adjustment)
	, pitch_cent_spinner (pitch_cent_adjustment)
	, duration_adjustment (100.0, -1000.0, 1000.0, 1.0, 10.0)
	, duration_clock (0)
	, ignore_adjustment_change (false)
	, ignore_clock_change (false)
	, progress (0.0f)
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
		Table* table = manage (new Table (4, 2, false));
		int row = 0;

		table->set_row_spacings	(6);
		table->set_col_spacings	(12);

#ifdef USE_RUBBERBAND
		vector<string> strings;
		duration_clock = manage (new AudioClock (X_("stretch"), true, X_("stretch"), true, false, true, false, true));
		duration_clock->set_session (e.session());
		duration_clock->set (timepos_t (new_length), true);
		duration_clock->set_mode (AudioClock::BBT);
#warning NUTEMPO FIXME figure out what we are doing here
		// duration_clock->set_bbt_reference (position);

		Gtk::Alignment* clock_align = manage (new Gtk::Alignment);
		clock_align->add (*duration_clock);
		clock_align->set (0.0, 0.5, 0.0, 1.0);

		l = manage (new Gtk::Label (_("Duration")));
		table->attach (*l, 0, 1, row, row+1, Gtk::FILL, Gtk::FILL, 0, 0);
		table->attach (*clock_align, 1, 2, row, row+1, Gtk::AttachOptions (Gtk::EXPAND|Gtk::FILL), Gtk::FILL, 0, 0);
		row++;

		const double fract = (double) (new_length / original_length);
		/* note the *100.0 to convert fract into a percentage */
		duration_adjustment.set_value (fract*100.0);
		Gtk::SpinButton* spinner = manage (new Gtk::SpinButton (duration_adjustment, 1.0, 3));

		l = manage (new Gtk::Label (_("Percent")));
		table->attach (*l, 0, 1, row, row+1, Gtk::FILL, Gtk::FILL, 0, 0);
		table->attach (*spinner, 1, 2, row, row+1, Gtk::FILL, Gtk::FILL, 0, 0);
		row++;

		table->attach (stretch_opts_label, 0, 1, row, row+1, Gtk::FILL, Gtk::EXPAND, 0, 0);

		set_popdown_strings (stretch_opts_selector, editor.rb_opt_strings);
		/* set default */
		stretch_opts_selector.set_active_text (editor.rb_opt_strings[editor.rb_current_opt]);
		table->attach (stretch_opts_selector, 1, 2, row, row+1, Gtk::FILL, Gtk::EXPAND & Gtk::FILL, 0, 0);
		row++;

		table->attach (precise_button, 0, 2, row, row+1, Gtk::FILL, Gtk::EXPAND, 0, 0);
		row++;

		duration_clock->ValueChanged.connect (sigc::mem_fun (*this, &TimeFXDialog::duration_clock_changed));
		duration_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &TimeFXDialog::duration_adjustment_changed));

#else
		quick_button.set_name (N_("TimeFXButton"));
		table->attach (quick_button, 1, 3, row, row+1, Gtk::FILL, Gtk::EXPAND, 0, 0);
		row++;

		antialias_button.set_name (N_("TimeFXButton"));
		table->attach (antialias_button, 1, 3, row, row+1, Gtk::FILL, Gtk::EXPAND, 0, 0);

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
TimeFXDialog::start_updates ()
{
	update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &TimeFXDialog::timer_update));
}

void
TimeFXDialog::update_progress_gui (float p)
{
	/* time/pitch FX are applied in a dedicated thread, so we cannot just
	   update the GUI when notified about progress. That is deferred to a
	   timer-driven callback which will ensure that the visual progress
	   indicator is updated.
	*/
	progress = p;
}

void
TimeFXDialog::timer_update ()
{
	progress_bar.set_fraction (progress);

	if (request.done || request.cancel) {
		update_connection.disconnect ();
	}
}

void
TimeFXDialog::cancel_in_progress ()
{
	request.cancel = true;
	first_cancel.disconnect();
}

gint
TimeFXDialog::delete_in_progress (GdkEventAny*)
{
	request.cancel = true;
	first_delete.disconnect();
	return TRUE;
}

float
TimeFXDialog::get_time_fraction () const
{
	if (pitching) {
		return 1.0;
	}

	return duration_adjustment.get_value() / 100.0;
}

float
TimeFXDialog::get_pitch_fraction () const
{
	if (!pitching) {
		return 1.0;
	}

	float cents = pitch_octave_adjustment.get_value() * 1200.0;

	cents += pitch_semitone_adjustment.get_value() * 100.0;
	cents += pitch_cent_adjustment.get_value();

	if (cents == 0.0) {
		return 1.0;
	}

	// one octave == 1200 cents
	// adding one octave doubles the frequency
	// ratio is 2^^octaves

	return pow(2, cents/1200);
}

void
TimeFXDialog::duration_adjustment_changed ()
{
	if (ignore_adjustment_change) {
		return;
	}

	PBD::Unwinder<bool> uw (ignore_clock_change, true);

	duration_clock->set_duration (original_length * Temporal::ratio_t (1.0, (duration_adjustment.get_value() / 100.0)));
}

void
TimeFXDialog::duration_clock_changed ()
{
	if (ignore_clock_change) {
		return;
	}

	PBD::Unwinder<bool> uw (ignore_adjustment_change, true);

	duration_adjustment.set_value (100.0 * (double) (duration_clock->current_duration() / original_length));
}
