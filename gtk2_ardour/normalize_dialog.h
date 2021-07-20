/*
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace Gtk {
	class RadioButton;
	class SpinButton;
	class ProgressBar;
	class ComboBoxText;
}

class NormalizeDialog : public ArdourDialog, public ProgressReporter
{
public:
	NormalizeDialog (bool);

	bool normalize_individually () const;
	bool use_true_peak () const;
	bool constrain_rms () const;
	bool constrain_lufs () const;

	double target_peak () const;
	double target_rms () const;
	double target_lufs () const;

	int run ();

	void on_response (int response_id) {
		Gtk::Dialog::on_response (response_id);
	}

private:
	void update_progress_gui (float);
	void button_clicked (int);
	void update_sensitivity ();

	Gtk::ComboBoxText* _dbfs_dbtp;
	Gtk::RadioButton*  _normalize_individually;
	Gtk::CheckButton*  _constrain_rms;
	Gtk::CheckButton*  _constrain_lufs;
	Gtk::SpinButton*   _spin_peak;
	Gtk::SpinButton*   _spin_rms;
	Gtk::SpinButton*   _spin_lufs;
	Gtk::ProgressBar*  _progress_bar;

	static double _last_normalization_value;
	static double _last_rms_target_value;
	static double _last_lufs_target_value;

	static bool _last_normalize_individually;
	static bool _last_normalize_true_peak;
	static bool _last_constrain_rms;
	static bool _last_constrain_lufs;
};
