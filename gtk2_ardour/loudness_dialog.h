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

#ifndef _gtkardour_loudness_dialog_h_
#define _gtkardour_loudness_dialog_h_

#include <cairomm/cairomm.h>
#include <gtkmm/box.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_dialog.h"
#include "progress_reporter.h"
#include "loudness_settings.h"

namespace ARDOUR {
	class TimelineRange;
	class ExportAnalysis;
	class ExportStatus;
	class PluginInsert;
	class Processor;
	class Session;
}

class LoudnessDialog : public ArdourDialog
{
public:
	LoudnessDialog (ARDOUR::Session*, ARDOUR::TimelineRange const&, bool);
	int run ();

	float gain_db () const;

protected:
	bool on_delete_event (GdkEventAny*);

private:
	int  analyze ();
	void start_analysis ();
	void cancel_analysis ();
	gint progress_timeout ();
	void display_results ();
	void display_report ();
	void save_preset ();
	void remove_preset ();
	void calculate_gain ();

	void load_preset (size_t);
	void apply_preset ();
	void peak_radio (bool);
	void update_settings ();
	void check_preset ();
	void update_sensitivity ();

	void test_conformity ();

	void plot_graph (boost::shared_ptr<ARDOUR::ExportAnalysis>);
	void graph_size_request (Gtk::Requisition*);
	bool graph_expose_event (GdkEventExpose*);

	ALoudnessPresets _lp;
	CLoudnessPreset  _preset;

	static bool            _first_time;
	static CLoudnessPreset _last_preset;

	ARDOUR::Session*                        _session;
	ARDOUR::TimelineRange const&               _range;
	boost::shared_ptr<ARDOUR::ExportStatus> _status;
	bool                                    _autostart;

	Gtk::VBox        _setup_box;
	Gtk::VBox        _progress_box;
	Gtk::VBox        _result_box;
	Gtk::Frame       _conformity_frame;
	Gtk::ProgressBar _progress_bar;
	Gtk::Button*     _ok_button;
	Gtk::Button*     _cancel_button;

	ArdourWidgets::ArdourButton _save_preset;
	ArdourWidgets::ArdourButton _remove_preset;

	ArdourWidgets::ArdourButton _dbfs_btn;
	ArdourWidgets::ArdourButton _dbtp_btn;
	ArdourWidgets::ArdourButton _lufs_i_btn;
	ArdourWidgets::ArdourButton _lufs_s_btn;
	ArdourWidgets::ArdourButton _lufs_m_btn;

	Gtk::Label _dbfs_label;
	Gtk::Label _dbtp_label;
	Gtk::Label _lufs_i_label;
	Gtk::Label _lufs_s_label;
	Gtk::Label _lufs_m_label;

	Gtk::Label _delta_dbfs_label;
	Gtk::Label _delta_dbtp_label;
	Gtk::Label _delta_lufs_i_label;
	Gtk::Label _delta_lufs_s_label;
	Gtk::Label _delta_lufs_m_label;

	Gtk::Label _gain_out_label;
	Gtk::Label _gain_amp_label;
	Gtk::Label _gain_norm_label;
	Gtk::Label _gain_total_label;
	Gtk::Label _gain_exceeds_label;

	Gtk::DrawingArea _loudness_graph;

	Cairo::RefPtr<Cairo::ImageSurface> _loudness_surf;

	ArdourWidgets::ArdourButton _rt_analysis_button;
	ArdourWidgets::ArdourButton _start_analysis_button;
	ArdourWidgets::ArdourButton _show_report_button;
	ArdourWidgets::ArdourButton _custom_pos_button;

	ArdourWidgets::ArdourDropdown _preset_dropdown;

	Gtk::Adjustment _dbfs_adjustment;
	Gtk::Adjustment _dbtp_adjustment;
	Gtk::Adjustment _lufs_i_adjustment;
	Gtk::Adjustment _lufs_s_adjustment;
	Gtk::Adjustment _lufs_m_adjustment;

	Gtk::SpinButton _dbfs_spinbutton;
	Gtk::SpinButton _dbtp_spinbutton;
	Gtk::SpinButton _lufs_i_spinbutton;
	Gtk::SpinButton _lufs_s_spinbutton;
	Gtk::SpinButton _lufs_m_spinbutton;

	float _dbfs;
	float _dbtp;
	float _lufs_i;
	float _lufs_s;
	float _lufs_m;

	float _gain_out;
	float _gain_norm;
	bool  _ignore_preset;
	bool  _ignore_change;
};

#endif
