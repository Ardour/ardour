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

#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include "ardour/dB.h"
#include "ardour/session.h"

#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_base.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"

#include "widgets/ardour_spacer.h"

#include "export_report.h"
#include "loudness_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

LoudnessDialog::LoudnessDialog (Session* s, AudioRange const& ar, bool as)
	: ArdourDialog (_("Loudness Mate"))
	, _session (s)
	, _range (ar)
	, _status (s->get_export_status ())
	, _autostart (as)
	, _rt_analysis_button (_("Realtime"), ArdourButton::led_default_elements, true)
	, _start_analysis_button (_("Analyze"))
	, _show_report_button (_("Show Detailed Report"))
	, _dbfs_adjustment ( 0.00, -90.00, 0.00, 0.1, 0.2)
	, _dbtp_adjustment ( -1.0, -90.00, 0.00, 0.1, 0.2)
	, _lufs_i_adjustment (-23.0, -90.00, 0.00, 0.1, 1.0)
	, _lufs_s_adjustment (-20.0, -90.00, 0.00, 0.1, 1.0)
	, _lufs_m_adjustment (-17.0, -90.00, 0.00, 0.1, 1.0)
	, _dbfs_spinbutton (_dbfs_adjustment, 0.1, 1)
	, _dbtp_spinbutton (_dbtp_adjustment, 0.1, 1)
	, _lufs_i_spinbutton (_lufs_i_adjustment, 0.1, 1)
	, _lufs_s_spinbutton (_lufs_s_adjustment, 0.1, 1)
	, _lufs_m_spinbutton (_lufs_m_adjustment, 0.1, 1)
	, _gain (0)
{
	_start_analysis_button.set_name ("generic button");
	_rt_analysis_button.set_name ("generic button");
	_show_report_button.set_name ("generic button");

	Label* l;
	Table* t = manage (new Table (8, 3, false));
	t->set_spacings (4);

	l = manage (new Label (_("<b>Analysis Results</b>"), ALIGN_LEFT));
	l->set_use_markup (true);
	t->attach (*l, 0, 1, 0, 1);
	t->attach (_show_report_button, 1, 4, 0, 1);

	l = manage (new Label (_("<b>Measured</b>"), ALIGN_CENTER));
	l->set_use_markup (true);
	t->attach (*l, 1, 2, 1, 2);
	l = manage (new Label (_("<b>Target</b>"), ALIGN_CENTER));
	l->set_use_markup (true);
	t->attach (*l, 2, 3, 1, 2);
	l = manage (new Label (_("<b>Delta</b>"), ALIGN_CENTER));
	l->set_use_markup (true);
	t->attach (*l, 3, 4, 1, 2);

	l = manage (new Label (_("Peak:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 2, 3);
	l = manage (new Label (_("True Peak:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 3, 4);
	l = manage (new Label (_("Integrated Loudness:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 4, 5);
	l = manage (new Label (_("Max. Short Loudness:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 5, 6);
	l = manage (new Label (_("Max. Momentary Loudness:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 6, 7);
	l = manage (new Label (_("Suggested Gain:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 8, 9);

	t->attach (_dbfs_label,         1, 2, 2, 3);
	t->attach (_dbtp_label,         1, 2, 3, 4);
	t->attach (_lufs_i_label,       1, 2, 4, 5);
	t->attach (_lufs_s_label,       1, 2, 5, 6);
	t->attach (_lufs_m_label,       1, 2, 6, 7);

	t->attach (_dbfs_spinbutton,    2, 3, 2, 3);
	t->attach (_dbtp_spinbutton,    2, 3, 3, 4);
	t->attach (_lufs_i_spinbutton,  2, 3, 4, 5);
	t->attach (_lufs_s_spinbutton,  2, 3, 5, 6);
	t->attach (_lufs_m_spinbutton,  2, 3, 6, 7);

	t->attach (_delta_dbfs_label,   3, 4, 2, 3);
	t->attach (_delta_dbtp_label,   3, 4, 3, 4);
	t->attach (_delta_lufs_i_label, 3, 4, 4, 5);
	t->attach (_delta_lufs_s_label, 3, 4, 5, 6);
	t->attach (_delta_lufs_m_label, 3, 4, 6, 7);

	ArdourHSpacer* spc = manage (new ArdourHSpacer (1.0));
	t->attach (*spc       ,         1, 4, 7, 8);
	t->attach (_gain_label,         3, 4, 8, 9);

	_result_box.pack_start (*t, false, false, 6);
	_progress_box.pack_start (_progress_bar, false, false, 6);

	t = manage (new Table (2, 3, false));
	t->set_spacings (4);
	l = manage (new Label ());
	l->set_line_wrap ();
	l->set_markup (_(
	  "<b>Loudness Analysis</b>\n\n"
	  "This allows to analyze the loudness of of the signal at the master-bus "
	  "output of the complete session, as it would be exported.\n"
	  "The result an be used to interactively normalize the session's loudnless level "
	  "(as opposed to automatical normalization during export).\n"
	  "By default a faster than realtime export is is used to asses the loudness of the "
	  "session. If any outboard gear is used, a <i>realtime</i> export is available, to "
	  "play at normal speed.\n"
	));
	t->attach (*l,                     1, 4, 1, 2, EXPAND|FILL, EXPAND|FILL, 0, 8);
	l = manage (new Label (""));
	t->attach (*l,                     1, 2, 2, 3);
	t->attach (_rt_analysis_button,    2, 3, 2, 3, FILL, SHRINK);
	t->attach (_start_analysis_button, 3, 4, 2, 3, FILL, SHRINK);
	_setup_box.pack_start (*t, false, false, 6);

	get_vbox()->pack_start (_setup_box);
	get_vbox()->pack_start (_progress_box);
	get_vbox()->pack_start (_result_box);
	get_vbox()->set_size_request(400,-1);

	_ok_button     = add_button (Stock::APPLY, RESPONSE_APPLY);
	_cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);

	_cancel_button->signal_clicked().connect (sigc::mem_fun (this, &LoudnessDialog::cancel_analysis));
	_dbfs_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_dbtp_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_lufs_i_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_lufs_s_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_lufs_m_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_show_report_button.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::display_report));
	_start_analysis_button.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::start_analysis));

	_ok_button->set_sensitive (false);
	_show_report_button.set_sensitive (false);
	show_all_children ();

	_result_box.hide ();
	_progress_box.hide ();
}

void
LoudnessDialog::cancel_analysis ()
{
	if (_status->running ()) {
		 _status->abort();
	}
}

void
LoudnessDialog::start_analysis ()
{
	if (0 == analyze ()) {
		display_results ();
	} else {
		_setup_box.show ();
	}
}

bool
LoudnessDialog::on_delete_event (GdkEventAny*)
{
	cancel_analysis ();
	return true;
}

int
LoudnessDialog::run ()
{
	if (_autostart) {
		show ();
		if (0 == analyze ()) {
			display_results ();
		} else {
			return RESPONSE_CANCEL;
		}
	}

	int const r = ArdourDialog::run ();
	cancel_analysis ();
	return r;
}

gint
LoudnessDialog::progress_timeout ()
{
	float progress = ((float) _status->processed_samples_current_timespan) / _status->total_samples_current_timespan;
	_progress_bar.set_text ("Analyzing");
	_progress_bar.set_fraction (progress);
	return true;
}

int
LoudnessDialog::analyze ()
{
	 /* These are ensured in Editor::measure_master_loudness () */
	assert (_session->master_out());
	assert (_session->master_out()->output());
	assert (_session->master_out()->output()->n_ports().n_audio() == 2);
	assert (_range.start < _range.end);

	ExportTimespanPtr tsp = _session->get_export_handler()->add_timespan();

	boost::shared_ptr<ExportChannelConfiguration> ccp = _session->get_export_handler()->add_channel_config();
	boost::shared_ptr<ARDOUR::ExportFilename>     fnp = _session->get_export_handler()->add_filename();
	boost::shared_ptr<ExportFormatSpecification>  fmp = _session->get_export_handler()->add_format();

	/* setup format */
	fmp->set_tag (false);
	fmp->set_sample_format (ExportFormatBase::SF_Float);
	fmp->set_sample_rate (ExportFormatBase::SR_Session);
	fmp->set_format_id (ExportFormatBase::F_None);
	fmp->set_type (ExportFormatBase::T_None);
	fmp->set_extension ("wav");
	fmp->set_soundcloud_upload (false);
	fmp->set_analyse (true);

	/* setup range */
	tsp->set_range (_range.start, _range.end);
	tsp->set_range_id ("selection");
	tsp->set_realtime (_rt_analysis_button.get_active ());
	tsp->set_name ("master");

	/* setup channels, use master out */
	IO* master_out = _session->master_out()->output().get();
	for (uint32_t n = 0; n < master_out->n_ports().n_audio(); ++n) {
		PortExportChannel * channel = new PortExportChannel ();
		channel->add_port (master_out->audio (n));
		ExportChannelPtr chan_ptr (channel);
		ccp->register_channel (chan_ptr);
	}

	/* do audio export */
	boost::shared_ptr<AudioGrapher::BroadcastInfo> b;
	_session->get_export_handler()->reset ();
	_session->get_export_handler()->add_export_config (tsp, ccp, fmp, fnp, b);
	_session->get_export_handler()->do_export();

	/* show progress */
	_setup_box.hide ();
	_progress_box.show_all ();

	/* shrink window height (setup box) */
	Gtk::Requisition wr;
	get_size (wr.width, wr.height);
	resize (wr.width, 60);

	sigc::connection progress_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &LoudnessDialog::progress_timeout), 100);

	while (_status->running ()) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			Glib::usleep (10000);
		}
	}
  progress_connection.disconnect();
	_progress_box.hide_all ();

	/* done */
  _status->finish (TRS_UI);
  return _status->aborted() ? 1 : 0;
}

void
LoudnessDialog::display_report ()
{
	hide ();
	ExportReport er ("Export Loudness Report", _status->result_map);
	er.run();
	show ();
}

void
LoudnessDialog::display_results ()
{
	AnalysisResults const& ar (_status->result_map);
	assert (ar.size () == 1);
	ExportAnalysisPtr p = ar.begin()->second;

	_dbfs   = accurate_coefficient_to_dB (p->peak);
	_dbtp   = accurate_coefficient_to_dB (p->truepeak);
	_lufs_i = p->integrated_loudness;
	_lufs_s = p->max_loudness_short;
	_lufs_m = p->max_loudness_momentary;

	_dbfs_label.set_text (string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed, _dbfs));
	_dbtp_label.set_text (string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed, _dbtp));
	_lufs_i_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs_i));
	_lufs_s_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs_s));
	_lufs_m_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs_m));

	calculate_gain ();

	_result_box.show_all ();
	_ok_button->set_sensitive (true);
	_show_report_button.set_sensitive (true);
}

void
LoudnessDialog::calculate_gain ()
{
	float dbfs   = _dbfs_spinbutton.get_value();
	float dbtp   = _dbtp_spinbutton.get_value();
	float lufs_i = _lufs_i_spinbutton.get_value();
	float lufs_s = _lufs_s_spinbutton.get_value();
	float lufs_m = _lufs_m_spinbutton.get_value();

	_gain = dbfs - _dbfs;
	_gain = std::min (_gain, dbtp - _dbtp);
	_gain = std::min (_gain, lufs_i - _lufs_i);
	_gain = std::min (_gain, lufs_s - _lufs_s);
	_gain = std::min (_gain, lufs_m - _lufs_m);

	_delta_dbfs_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::fixed, dbfs - _dbfs));
	_delta_dbtp_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::fixed, dbtp - _dbtp));
	_delta_lufs_i_label.set_text (string_compose (_("%1 LU"), std::setprecision (2), std::fixed, lufs_i - _lufs_i));
	_delta_lufs_s_label.set_text (string_compose (_("%1 LU"), std::setprecision (2), std::fixed, lufs_s - _lufs_s));
	_delta_lufs_m_label.set_text (string_compose (_("%1 LU"), std::setprecision (2), std::fixed, lufs_m - _lufs_m));

	_gain_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::fixed, _gain));
}
