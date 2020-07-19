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

#include "ardour/session.h"
#include "ardour/dB.h"

#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_base.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"

#include "export_report.h"
#include "loudness_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

LoudnessDialog::LoudnessDialog (Session* s, TimeSelection const& ts)
	: ArdourDialog (_("Loudness Mate"))
	, _session (s)
	, _time (ts)
	, _status (s->get_export_status ())
	, _report_button (_("Show"))
	, _dbfs_adjustment ( 0.00, -90.00, 0.00, 0.1, 0.2)
	, _dbtp_adjustment ( -1.0, -90.00, 0.00, 0.1, 0.2)
	, _lufs_adjustment (-23.0, -90.00, 0.00, 0.1, 1.0)
	, _dbfs_spinbutton (_dbfs_adjustment, 0.1, 1)
	, _dbtp_spinbutton (_dbtp_adjustment, 0.1, 1)
	, _lufs_spinbutton (_lufs_adjustment, 0.1, 1)
	, _gain (0)
{
	Gtk::Label* l;
	Gtk::Table* t = manage (new Table (5, 3, false));
	t->set_spacings (4);
	l = manage (new Label (_("Digital Peak:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 0, 1);
	l = manage (new Label (_("Analog Peak:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 1, 2);
	l = manage (new Label (_("Loudness:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 2, 3);
	l = manage (new Label (_("Detailed Report:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 3, 4);
	l = manage (new Label (_("Suggested Gain:"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	t->attach (*l, 0, 1, 4, 5);

	t->attach (_dbfs_label, 1, 2, 0, 1);
	t->attach (_dbtp_label, 1, 2, 1, 2);
	t->attach (_lufs_label, 1, 2, 2, 3);
	t->attach (_report_button, 1, 2, 3, 4);

	t->attach (_dbfs_spinbutton, 2, 3, 0, 1);
	t->attach (_dbtp_spinbutton, 2, 3, 1, 2);
	t->attach (_lufs_spinbutton, 2, 3, 2, 3);

	t->attach (_gain_label, 1, 2, 4, 5);

	_report_button.set_name ("generic button");

	_result_box.pack_start (*t, false, false, 6);
	_progress_box.pack_start (_progress_bar, false, false, 6);

	get_vbox()->pack_start (_progress_box);
	get_vbox()->pack_start (_result_box);
	get_vbox()->set_size_request(400,-1);

	_ok_button     = add_button (Gtk::Stock::APPLY, RESPONSE_APPLY);
	_cancel_button = add_button (Gtk::Stock::CANCEL, RESPONSE_CANCEL);

	_cancel_button->signal_clicked().connect (sigc::mem_fun (this, &LoudnessDialog::cancel_analysis));
	_dbfs_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_dbtp_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_lufs_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::calculate_gain));
	_report_button.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::display_report));

	_ok_button->set_sensitive (false);
	_report_button.set_sensitive (false);
	show_all_children ();
	_result_box.hide ();
}

void
LoudnessDialog::cancel_analysis ()
{
	if (_status->running ()) {
		 _status->abort();
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
#if 1
	// TODO: consider interactive analysis "start" button,
	// after settings, e.g. realtime export, IO select
	show ();
	switch (analyze ()) {
		case 0:
			/* OK */
			display_results ();
			break;
		case 1:
			/* aborted */
			return RESPONSE_CANCEL;
		default:
			// TODO show an ArdourMessageDialog here or in parent Editor::analyze_range_export
			return RESPONSE_CANCEL;
	}
#endif

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
	/* add master outs as default */
	IO* master_out = _session->master_out()->output().get();
	if (!master_out) {
		return -1;
	}
	if (master_out->n_ports().n_audio() != 2) {
		return -2;
	}
	if (_time.empty()) {
		// use whole session ?!
		return -3;
	}

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
	// TODO: consider multiple ranges
	tsp->set_range (_time.front().start, _time.front().end);
	tsp->set_range_id ("selection");
	tsp->set_realtime (false);

	/* setup channels, use master out */
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
	ExportReport er ("Export Loudness Report", _status->result_map);
	er.run();
}

void
LoudnessDialog::display_results ()
{
	AnalysisResults const& ar (_status->result_map);
	assert (ar.size () == 1);
	ExportAnalysisPtr p = ar.begin()->second;

	_dbfs = accurate_coefficient_to_dB (p->peak);
	_dbtp = accurate_coefficient_to_dB (p->truepeak);
	_lufs = p->loudness;

	_dbfs_label.set_text (string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed, _dbfs));
	_dbtp_label.set_text (string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed, _dbtp));
	_lufs_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs));

	calculate_gain ();

	_result_box.show_all ();
	_ok_button->set_sensitive (true);
	_report_button.set_sensitive (true);
}

void
LoudnessDialog::calculate_gain ()
{
	float dbfs = _dbfs_spinbutton.get_value();
	float dbtp = _dbtp_spinbutton.get_value();
	float lufs = _lufs_spinbutton.get_value();

	_gain = dbfs - _dbfs;
	_gain = std::min (_gain, dbtp - _dbtp);
	_gain = std::min (_gain, lufs - _lufs);

	_gain_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::fixed, _gain));
}
