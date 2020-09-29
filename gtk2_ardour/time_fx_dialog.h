/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_time_fx_dialog_h__
#define __ardour_time_fx_dialog_h__

#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/box.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>

#include "ardour/timefx_request.h"

#include "ardour_dialog.h"
#include "progress_reporter.h"

class Editor;
class AudioClock;

class TimeFXDialog : public ArdourDialog, public ProgressReporter
{
public:
	/* We need a position so that BBT mode in the clock can function */
	TimeFXDialog (Editor& e, bool for_pitch, Temporal::timecnt_t const & old_length, Temporal::timecnt_t const & new_length, Temporal::timepos_t const & position);

	ARDOUR::TimeFXRequest request;
	Editor&               editor;
	bool                  pitching;
	Gtk::ProgressBar      progress_bar;
	ARDOUR::RegionList    regions;

	/* SoundTouch */
	Gtk::CheckButton      quick_button;
	Gtk::CheckButton      antialias_button;
	Gtk::VBox             upper_button_box;

	/* RubberBand */
	Gtk::ComboBoxText     stretch_opts_selector;
	Gtk::Label            stretch_opts_label;
	Gtk::CheckButton      precise_button;
	Gtk::CheckButton      preserve_formants_button;

	Gtk::Button*          cancel_button;
	Gtk::Button*          action_button;
	Gtk::VBox             packer;
	int                   status;

	sigc::connection first_cancel;
	sigc::connection first_delete;
	void cancel_in_progress ();
	gint delete_in_progress (GdkEventAny*);

	float get_time_fraction () const;
	float get_pitch_fraction () const;

	void start_updates ();

	void on_response (int response_id) {
		Gtk::Dialog::on_response (response_id);
	}

	void hide () {
		regions.clear ();
		ArdourDialog::hide ();
	}

private:
	Temporal::timecnt_t original_length;
	Gtk::Adjustment     pitch_octave_adjustment;
	Gtk::Adjustment     pitch_semitone_adjustment;
	Gtk::Adjustment     pitch_cent_adjustment;
	Gtk::SpinButton     pitch_octave_spinner;
	Gtk::SpinButton     pitch_semitone_spinner;
	Gtk::SpinButton     pitch_cent_spinner;
	Gtk::Adjustment     duration_adjustment;
	AudioClock*         duration_clock;
	bool                ignore_adjustment_change;
	bool                ignore_clock_change;
	sigc::connection    update_connection;
	float               progress;

	void update_progress_gui (float);
	void duration_clock_changed ();
	void duration_adjustment_changed ();
	void timer_update ();
};

#endif /* __ardour_time_fx_dialog_h__ */
