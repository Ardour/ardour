/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "widgets/ardour_button.h"

#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace ARDOUR {
	class AudioRange;
	class ExportStatus;
	class Session;
}

class LoudnessDialog : public ArdourDialog
{
public:
	LoudnessDialog (ARDOUR::Session*, ARDOUR::AudioRange const&);
	int run ();
	float gain_db () const { return _gain; }

protected:
	bool on_delete_event (GdkEventAny*);

private:
	int  analyze ();
	void cancel_analysis ();
	gint progress_timeout ();
	void display_results ();
	void display_report ();
	void calculate_gain ();

	ARDOUR::Session*                        _session;
	ARDOUR::AudioRange const&               _range;
	boost::shared_ptr<ARDOUR::ExportStatus> _status;

	Gtk::VBox        _progress_box;
	Gtk::VBox        _result_box;
	Gtk::ProgressBar _progress_bar;
	Gtk::Button*     _ok_button;
	Gtk::Button*     _cancel_button;

	Gtk::Label       _dbfs_label;
	Gtk::Label       _dbtp_label;
	Gtk::Label       _lufs_integrated_label;
	Gtk::Label       _lufs_short_label;
	Gtk::Label       _lufs_momentary_label;
	Gtk::Label       _gain_label;

	ArdourWidgets::ArdourButton _report_button;

  Gtk::Adjustment _dbfs_adjustment;
  Gtk::Adjustment _dbtp_adjustment;
  Gtk::Adjustment _lufs_adjustment;

	Gtk::SpinButton _dbfs_spinbutton;
  Gtk::SpinButton _dbtp_spinbutton;
  Gtk::SpinButton _lufs_spinbutton;

	float _dbfs;
	float _dbtp;
	float _lufs;
	float _gain;
};
