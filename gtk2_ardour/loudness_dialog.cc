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

#include "pbd/unwind.h"

#include "ardour/dB.h"
#include "ardour/plugin_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/session.h"

#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_base.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_handler.h"
#include "ardour/export_status.h"
#include "ardour/export_timespan.h"

#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_spacer.h"
#include "widgets/tooltips.h"

#include "ardour_message.h"
#include "export_report.h"
#include "loudness_dialog.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

/* https://www.masteringthemix.com/blogs/learn/76296773-mastering-audio-for-soundcloud-itunes-spotify-and-youtube
 * https://youlean.co/loudness-standards-full-comparison-table/
 */
LoudnessDialog::LoudnessPreset LoudnessDialog::presets[] =
{
	/*                   | dbFS   dBTP   LUFS   short   mom.    | FS,  TP , int, sht, mom |maxIntg|  notes */
	{"EBU R128",         { false, true,  true,  false, false},  {  0, -1.0, -23,   0,   0 }, {-22.5,  -23.5}}, // +/- 0.5 LU
	{"EBU R128 S1",      { false, true,  true,  true,  false},  {  0, -1.0, -23, -18,   0 }, {-22.5,  -23.5}}, // +/- 0.5 LU
	{"ATSC A/85",        { false, true,  true,  true,  false},  {  0, -2.0, -24,   0,   0 }, {-22.0,  -26.0}}, // +/- 2 LU
	{"AES Streaming",    { false, true,  true,  false, false},  {  0, -1.0, -18,   0,   0 }, {-16.0,  -20.0}}, // min/max Integrated: -20 / -16 LUFS - same as "ASWG-R001 PORTABLE"
	{"ASWG-R001 HOME",   { false, true,  true,  true,  false},  {  0, -1.0, -24,   0,   0 }, {-22.0,  -26.0}}, // +/- 2 LU
	{"Digital Peak",     { true,  false, false, false, false},  {  0,  0.0,   0,   0,   0 }, {  0.0, -200.0}},
	{"CD",               { true,  true,  true,  false, false},  {  0, -0.1,  -9,   0,   0 }, {  0.0, -200.0}},

	{"Amazon Music",     { false, true,  true,  false, false},  {  0, -2.0, -14,   0,   0 }, { -9.0,  -19.0}}, // -9 to -19 LUFS
	{"Apple Music",      { false, true,  true,  false, false},  {  0, -1.0, -16,   0,   0 }, {-15.0,  -17.0}}, // (+/- 1.0 LU)
	{"Deezer",           { false, true,  true,  false, false},  {  0, -1.0, -15,   0,   0 }, {-14.0,  -16.0}}, // -14 to -16 LUFS
	{"Soundcloud",       { false, true,  true,  false, false},  {  0, -1.0, -10,   0,   0 }, { -8.0,  -13.0}}, // -8 to -13 LUFS
	{"Spotify",          { false, true,  true,  false, false},  {  0, -1.0, -14,   0,   0 }, { -8.0,  -20.0}}, // Spotify use replay-gain to match -14 or -11 ..
	{"Spotify Loud",     { false, true,  true,  false, false},  {  0, -2.0, -11,   0,   0 }, { -5.0,  -17.0}}, // .. so the min/max range is arbitrary +/- 6dB
	{"Youtube",          { false, true,  true,  false, false},  {  0, -1.0, -14,   0,   0 }, {-13.0,  -15.0}}, // -13 to -15 LUFS
};

LoudnessDialog::LoudnessPreset LoudnessDialog::_preset = LoudnessDialog::presets [0];

LoudnessDialog::LoudnessDialog (Session* s, AudioRange const& ar, bool as)
	: ArdourDialog (as ? _("Loudness Assistant") : _("Loudness Analyzer and Normalizer"))
	, _session (s)
	, _range (ar)
	, _status (s->get_export_status ())
	, _autostart (as)
	, _conformity_expander (_("Conformity Analysis"))
	, _dbfs_btn (_("Peak:"), ArdourButton::led_default_elements, true)
	, _dbtp_btn (_("True Peak:"), ArdourButton::led_default_elements, true)
	, _lufs_i_btn (_("Integrated Loudness:"), ArdourButton::led_default_elements, true)
	, _lufs_s_btn (_("Max. Short Loudness:"), ArdourButton::led_default_elements, true)
	, _lufs_m_btn (_("Max. Momentary Loudness:"), ArdourButton::led_default_elements, true)
	, _rt_analysis_button (_("Realtime"), ArdourButton::led_default_elements, true)
	, _start_analysis_button (_("Analyze"))
	, _show_report_button (_("Show Graphical Analysis"))
	, _custom_pos_button (_("Custom Amplifier Position"), ArdourButton::led_default_elements, true)
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
	, _gain_out (0)
	, _gain_norm (0)
	, _ignore_change (false)
{
	/* query initial gain */
	_gain_out = accurate_coefficient_to_dB (_session->master_volume()->get_value());

	_start_analysis_button.set_name ("generic button");
	_rt_analysis_button.set_name ("generic button");
	_show_report_button.set_name ("generic button");
	_custom_pos_button.set_name ("generic button");

	_custom_pos_button.set_active (!_session->master_out()->volume_applies_to_output ());

	GtkRequisition req = _start_analysis_button.size_request ();
	_start_analysis_button.set_size_request (-1, req.height * 1.1);
	_rt_analysis_button.set_size_request (-1, req.height * 1.1);

	_dbfs_btn.set_name ("generic button");
	_dbtp_btn.set_name ("generic button");
	_lufs_i_btn.set_name ("generic button");
	_lufs_s_btn.set_name ("generic button");
	_lufs_m_btn.set_name ("generic button");

	_dbfs_btn.set_led_left (true);
	_dbtp_btn.set_led_left (true);
	_lufs_i_btn.set_led_left (true);
	_lufs_s_btn.set_led_left (true);
	_lufs_m_btn.set_led_left (true);

	_dbfs_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_dbtp_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_lufs_i_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_lufs_s_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_lufs_m_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());

	_delta_dbfs_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_delta_dbtp_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_delta_lufs_i_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_delta_lufs_s_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_delta_lufs_m_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());

	_gain_out_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_gain_norm_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_gain_total_label.modify_font (UIConfiguration::instance().get_NormalMonospaceFont());
	_gain_exceeds_label.modify_font (UIConfiguration::instance().get_NormalFont());

#define ROW row, row +1

	int row;
	Label* l;
	Table* t = manage (new Table (12, 4, false));
	t->set_spacings (4);

	l = manage (new Label (_("<b>Analysis Results</b>"), ALIGN_LEFT));
	l->set_use_markup (true);
	t->attach (*l, 0, 1, 0, 1);
	l = manage (new Label (_("Preset:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, 1, 2);

	t->attach (_show_report_button, 1, 4, 0, 1);
	t->attach (_preset_dropdown,    1, 3, 1, 2);

	l = manage (new Label (_("<b>Target</b>"), ALIGN_CENTER));
	l->set_use_markup (true);
	t->attach (*l, 1, 2, 2, 3);
	l = manage (new Label (_("<b>Measured</b>"), ALIGN_CENTER));
	l->set_use_markup (true);
	t->attach (*l, 2, 3, 2, 3);
	l = manage (new Label (_("<b>Delta</b>"), ALIGN_CENTER));
	l->set_use_markup (true);
	t->attach (*l, 3, 4, 2, 3);

	row = 3;
	t->attach (_dbfs_btn,   0, 1, ROW); ++row;
	t->attach (_dbtp_btn,   0, 1, ROW); ++row;
	t->attach (_lufs_i_btn, 0, 1, ROW); ++row;
	t->attach (_lufs_s_btn, 0, 1, ROW); ++row;
	t->attach (_lufs_m_btn, 0, 1, ROW); ++row;

	++row; // spacer

	l = manage (new Label (_("Gain to normalize:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, ROW); ++row;
	l = manage (new Label (_("Previous output gain:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, ROW); ++row;

	l = manage (new Label (_("Total gain:"), ALIGN_LEFT));
	t->attach (*l, 0, 1, ROW); ++row;

	row = 3;
	t->attach (_dbfs_spinbutton,    1, 2, ROW, EXPAND|FILL, EXPAND|FILL, 8, 0); ++row;
	t->attach (_dbtp_spinbutton,    1, 2, ROW, EXPAND|FILL, EXPAND|FILL, 8, 0); ++row;
	t->attach (_lufs_i_spinbutton,  1, 2, ROW, EXPAND|FILL, EXPAND|FILL, 8, 0); ++row;
	t->attach (_lufs_s_spinbutton,  1, 2, ROW, EXPAND|FILL, EXPAND|FILL, 8, 0); ++row;
	t->attach (_lufs_m_spinbutton,  1, 2, ROW, EXPAND|FILL, EXPAND|FILL, 8, 0); ++row;

	row = 3;
	t->attach (_dbfs_label,         2, 3, ROW); ++row;
	t->attach (_dbtp_label,         2, 3, ROW); ++row;
	t->attach (_lufs_i_label,       2, 3, ROW); ++row;
	t->attach (_lufs_s_label,       2, 3, ROW); ++row;
	t->attach (_lufs_m_label,       2, 3, ROW); ++row;

	row = 3;
	t->attach (_delta_dbfs_label,   3, 4, ROW); ++row;
	t->attach (_delta_dbtp_label,   3, 4, ROW); ++row;
	t->attach (_delta_lufs_i_label, 3, 4, ROW); ++row;
	t->attach (_delta_lufs_s_label, 3, 4, ROW); ++row;
	t->attach (_delta_lufs_m_label, 3, 4, ROW); ++row;

	ArdourHSpacer* spc = manage (new ArdourHSpacer (1.0));
	t->attach (*spc,                2, 4, ROW); ++row;
	t->attach (_gain_norm_label,    3, 4, ROW); ++row;
	t->attach (_gain_out_label,    3, 4, ROW); ++row;
	t->attach (_gain_exceeds_label, 2, 3, ROW);
	t->attach (_gain_total_label,   3, 4, ROW); ++row;
	t->attach (_custom_pos_button,  1, 4, ROW); ++row;

	set_tooltip (_custom_pos_button,
		_("<b>When enabled</b> an amplifier processor is used to apply the gain. "
		  "This allows for custom positoning of the gain-stage in the master-bus' signal flow, "
		  "potentially followed by a limiter to conform to both loudness and peak requirements. "
		  "Depending on limiter settings or DSP after the gain-stage, repeat loudness measurements may produce different results.\n"
		  "<b>When disabled</b>, the gain is applied diretcly to the output of the master-bus. This results in an efficient and reliable volume adjustment."
		 ));

	_dbfs_label.set_alignment (ALIGN_RIGHT);
	_dbtp_label.set_alignment (ALIGN_RIGHT);
	_lufs_i_label.set_alignment (ALIGN_RIGHT);
	_lufs_s_label.set_alignment (ALIGN_RIGHT);
	_lufs_m_label.set_alignment (ALIGN_RIGHT);

	_delta_dbfs_label.set_alignment (ALIGN_RIGHT);
	_delta_dbtp_label.set_alignment (ALIGN_RIGHT);
	_delta_lufs_i_label.set_alignment (ALIGN_RIGHT);
	_delta_lufs_s_label.set_alignment (ALIGN_RIGHT);
	_delta_lufs_m_label.set_alignment (ALIGN_RIGHT);

	_gain_norm_label.set_alignment (ALIGN_RIGHT);
	_gain_out_label.set_alignment (ALIGN_RIGHT);
	_gain_total_label.set_alignment (ALIGN_RIGHT);
	_gain_exceeds_label.set_alignment (ALIGN_RIGHT);

	_result_box.pack_start (*t, false, false, 6);
	_result_box.pack_start (_conformity_expander, false, false, 6);

	_progress_box.pack_start (_progress_bar, false, false, 6);

	t = manage (new Table (2, 3, false));
	t->set_spacings (4);
	l = manage (new Label ());
	l->set_line_wrap ();
	l->set_alignment (ALIGN_LEFT, ALIGN_TOP);
	l->set_markup (_(
	  "<b>Loudness Analysis</b>\n\n"
	  "This allows the user to analyze and conform the loudness of the signal at the master-bus "
	  "output of the complete session, as it would be exported. "
	  "When using this feature, remember to disable normalization in the session export profile."
	));
	t->attach (*l,                     0, 1, 0, 2, EXPAND|FILL, FILL, 8, 2);

	l = manage (new Label ());
	l->set_line_wrap ();
	l->set_alignment (ALIGN_LEFT, ALIGN_TOP);
	l->set_markup (_(
	  "By default, a faster-than-realtime export is used to assess the loudness of the "
	  "session. If any outboard gear is used, a <i>realtime</i> export is available, to "
	  "play at normal speed."
	));
	t->attach (*l,                     0, 1, 2, 4, EXPAND|FILL, FILL, 8, 2);

	t->attach (_start_analysis_button, 1, 2, 0, 1, FILL, SHRINK, 2, 0);
	t->attach (_rt_analysis_button,    1, 2, 2, 3, FILL, SHRINK, 2, 0);

	_setup_box.pack_start (*t, false, false, 6);

	get_vbox()->pack_start (_setup_box);
	get_vbox()->pack_start (_progress_box);
	get_vbox()->pack_start (_result_box);

	_progress_box.set_size_request (400,-1);

	_ok_button     = add_button (Stock::APPLY, RESPONSE_APPLY);
	_cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);

	for (size_t i = 0; i < sizeof (presets) / sizeof (LoudnessDialog::LoudnessPreset); ++i) {
		using namespace Gtkmm2ext;
		_preset_dropdown.AddMenuElem (MenuElemNoMnemonic (presets[i].name, sigc::bind (sigc::mem_fun (*this, &LoudnessDialog::load_preset), i)));
	}

	_conformity_expander.set_expanded(false);
	_initial_preset_name = _preset.name;
	apply_preset ();

	_gain_out_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::showpos, std::fixed, _gain_out));

	_cancel_button->signal_clicked().connect (sigc::mem_fun (this, &LoudnessDialog::cancel_analysis));
	_dbfs_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_dbtp_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_lufs_i_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_lufs_s_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_lufs_m_spinbutton.signal_value_changed().connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_show_report_button.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::display_report));
	_start_analysis_button.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::start_analysis));
	_dbfs_btn.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_dbtp_btn.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_lufs_i_btn.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_lufs_s_btn.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::update_settings));
	_lufs_m_btn.signal_clicked.connect (mem_fun (*this, &LoudnessDialog::update_settings));

	_conformity_expander.property_expanded().signal_changed().connect( sigc::mem_fun(*this, &LoudnessDialog::toggle_conformity_display));

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
LoudnessDialog::on_delete_event (GdkEventAny* ev)
{
	cancel_analysis ();
	return ArdourDialog::on_delete_event (ev);
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

	if (r == RESPONSE_APPLY) {
		_session->master_volume ()->set_value (dB_to_coefficient (gain_db ()), PBD::Controllable::NoGroup);
		_session->master_out()->set_volume_applies_to_output (!_custom_pos_button.get_active ());

		_preset.level[0] = _dbfs_spinbutton.get_value();
		_preset.level[1] = _dbtp_spinbutton.get_value();
		_preset.level[2] = _lufs_i_spinbutton.get_value();
		_preset.level[3] = _lufs_s_spinbutton.get_value();
		_preset.level[4] = _lufs_m_spinbutton.get_value();
		_preset.enable[0] = _dbfs_btn.get_active ();
		_preset.enable[1] = _dbtp_btn.get_active ();
		_preset.enable[2] = _lufs_i_btn.get_active ();
		_preset.enable[3] = _lufs_s_btn.get_active ();
		_preset.enable[4] = _lufs_m_btn.get_active ();
	} else {
		/* retore preset name, in case user has changed it */
		_preset.name = _initial_preset_name;
	}

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
	assert (_session->master_volume ());
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
	ExportReport er ("Export Loudness Report", _status->result_map);
	er.set_transient_for (*this);
	er.run();
}

void
LoudnessDialog::load_preset (size_t n)
{
	_preset = presets[n];
	apply_preset ();
	calculate_gain ();
}

void
LoudnessDialog::apply_preset ()
{
	PBD::Unwinder<bool> uw (_ignore_change, true);
	_preset_dropdown.set_text (_preset.name);

	_dbfs_btn.set_active (_preset.enable[0]);
	_dbtp_btn.set_active (_preset.enable[1]);
	_lufs_i_btn.set_active (_preset.enable[2]);
	_lufs_s_btn.set_active (_preset.enable[3]);
	_lufs_m_btn.set_active (_preset.enable[4]);
	_dbfs_spinbutton.set_value (_preset.level[0]);
	_dbtp_spinbutton.set_value (_preset.level[1]);
	_lufs_i_spinbutton.set_value (_preset.level[2]);
	_lufs_s_spinbutton.set_value (_preset.level[3]);
	_lufs_m_spinbutton.set_value (_preset.level[4]);
	update_sensitivity ();
}

void
LoudnessDialog::update_sensitivity ()
{
	_dbfs_spinbutton.set_sensitive (_dbfs_btn.get_active () && _dbfs_btn.sensitive ());
	_dbtp_spinbutton.set_sensitive (_dbtp_btn.get_active () && _dbtp_btn.sensitive ());
	_lufs_i_spinbutton.set_sensitive (_lufs_i_btn.get_active () && _dbtp_btn.sensitive ());
	_lufs_s_spinbutton.set_sensitive (_lufs_s_btn.get_active () && _lufs_s_btn.sensitive ());
	_lufs_m_spinbutton.set_sensitive (_lufs_m_btn.get_active () && _lufs_m_btn.sensitive ());
}

void
LoudnessDialog::update_settings ()
{
	if (_ignore_change) {
		return;
	}
	_preset.name = "User";
	_preset_dropdown.set_text (_preset.name);

	update_sensitivity ();
	calculate_gain ();
}

float
LoudnessDialog::gain_db () const
{
	return _gain_norm + _gain_out;
}

void
LoudnessDialog::display_results ()
{
	AnalysisResults const& ar (_status->result_map);
	assert (ar.size () == 1);
	ExportAnalysisPtr p = ar.begin()->second;

	_dbfs   = accurate_coefficient_to_dB (p->peak);
	_dbtp   = accurate_coefficient_to_dB (p->truepeak);
	_lufs_i = p->integrated_loudness    > -200  ? p->integrated_loudness    : -std::numeric_limits<float>::infinity();
	_lufs_s = p->max_loudness_short     > -200  ? p->max_loudness_short     : -std::numeric_limits<float>::infinity();
	_lufs_m = p->max_loudness_momentary > - 200 ? p->max_loudness_momentary : -std::numeric_limits<float>::infinity();

	_dbfs_btn.set_sensitive (_dbfs > -300);
	_dbtp_btn.set_sensitive (_dbtp > -300);
	_lufs_i_btn.set_sensitive (p->integrated_loudness > -200);
	_lufs_s_btn.set_sensitive (p->max_loudness_short > -200);
	_lufs_m_btn.set_sensitive (p->max_loudness_momentary > -200);

	_dbfs_label.set_text (string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed, _dbfs));
	_dbtp_label.set_text (string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed, _dbtp));
	_lufs_i_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs_i));
	_lufs_s_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs_s));
	_lufs_m_label.set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, _lufs_m));

	update_sensitivity ();
	calculate_gain ();

	_result_box.show_all ();
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

	float gain = 0;
	bool set = false;

#define MIN_IF_SET(A, B) \
	{ if (set) { gain = std::min (gain, (A) - (B));} else { gain = (A) - (B); } set = true; }

	if (_dbfs_btn.get_active () && _dbfs_btn.sensitive ()) {
		MIN_IF_SET (dbfs, _dbfs);
	}
	if (_dbtp_btn.get_active () && _dbtp_btn.sensitive ()) {
		MIN_IF_SET (dbtp, _dbtp);
	}
	if (_lufs_i_btn.get_active () && _lufs_i_btn.sensitive ()) {
		MIN_IF_SET (lufs_i, _lufs_i);
	}
	if (_lufs_s_btn.get_active () && _lufs_s_btn.sensitive ()) {
		MIN_IF_SET (lufs_s, _lufs_s);
	}
	if (_lufs_m_btn.get_active () && _lufs_m_btn.sensitive ()) {
		MIN_IF_SET (lufs_m, _lufs_m);
	}

	_delta_dbfs_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::showpos, std::fixed, dbfs - _dbfs));
	_delta_dbtp_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::showpos, std::fixed, dbtp - _dbtp));
	_delta_lufs_i_label.set_text (string_compose (_("%1 LU"), std::setprecision (2), std::showpos, std::fixed, lufs_i - _lufs_i));
	_delta_lufs_s_label.set_text (string_compose (_("%1 LU"), std::setprecision (2), std::showpos, std::fixed, lufs_s - _lufs_s));
	_delta_lufs_m_label.set_text (string_compose (_("%1 LU"), std::setprecision (2), std::showpos, std::fixed, lufs_m - _lufs_m));

	_delta_dbfs_label.set_sensitive (_dbfs_btn.get_active ());
	_delta_dbtp_label.set_sensitive (_dbtp_btn.get_active ());
	_delta_lufs_i_label.set_sensitive (_lufs_i_btn.get_active ());
	_delta_lufs_s_label.set_sensitive (_lufs_s_btn.get_active ());
	_delta_lufs_m_label.set_sensitive (_lufs_m_btn.get_active ());

	_gain_norm = gain;
	bool in_range = gain_db () >= -40 && gain_db () <= 40;

	_gain_norm_label.set_text (string_compose (_("%1 dB"), std::setprecision (2), std::showpos, std::fixed, _gain_norm));
	if (!in_range) {
		_gain_exceeds_label.set_text (_("exceeds"));
		_gain_total_label.set_markup (_("<b>    \u00B140 dB</b>"));
	} else {
		_gain_exceeds_label.set_text (X_(""));
		_gain_total_label.set_markup (string_compose (_("<b>%1 dB</b>"), std::setw(7), std::setprecision (2), std::showpos, std::fixed, gain_db ()));
	}

	test_conformity ();
	_ok_button->set_sensitive (in_range);
}

void
LoudnessDialog::toggle_conformity_display ()
{
	if (_conformity_expander.get_expanded ()) {
		test_conformity ();
	} else {
		const int child_height = _conformity_expander.get_child ()->get_height ();
		_conformity_expander.remove ();

		Gtk::Requisition wr;
		get_size (wr.width, wr.height);
		wr.height -= child_height;
		resize (wr.width, wr.height);
	}
}

void
LoudnessDialog::test_conformity ()
{
	if (!_conformity_expander.get_expanded ()) {
		return;
	}
	if (_conformity_expander.get_child ()) {
		_conformity_expander.remove ();
	}

	const float dbfs = rintf ((_dbfs + _gain_norm) * 10.f) / 10.f;
	const float dbtp = rintf ((_dbtp + _gain_norm) * 10.f) / 10.f;
	const float lufs_i = rintf ((_lufs_i + _gain_norm) * 10.f) / 10.f;

	Table* t = manage (new Table ());
	size_t n_pset = sizeof (presets) / sizeof (LoudnessDialog::LoudnessPreset);
	size_t n_rows = ceil (n_pset / 3.0);

	size_t row = 0;
	size_t col = 0;

	uint32_t c_good = UIConfigurationBase::instance().color ("alert:green"); // OK / green
	uint32_t c_warn = UIConfigurationBase::instance().color ("alert:yellow"); // Warning / yellow
	uint32_t c_fail = UIConfigurationBase::instance().color ("alert:red"); // Fail / red

	Gdk::Color color_good = ARDOUR_UI_UTILS::gdk_color_from_rgba (c_good);
	Gdk::Color color_warn = ARDOUR_UI_UTILS::gdk_color_from_rgba (c_warn);
	Gdk::Color color_fail = ARDOUR_UI_UTILS::gdk_color_from_rgba (c_fail);

	for (size_t i = 0; i < n_pset; ++i) {
		Label* l = manage (new Label (presets[i].name + ":", ALIGN_LEFT));
		t->attach (*l, col, col + 1, row, row + 1, EXPAND|FILL, SHRINK, 2, 0);

		if (lufs_i > presets[i].max_integrated[0]
		    || (presets[i].enable[0] && dbfs > presets[i].level[0])
		    || (presets[i].enable[1] && dbtp > presets[i].level[1])
		   ) {
			l = manage (new Label ("\u274C", ALIGN_CENTER)); // "X"
			l->modify_fg (Gtk::STATE_NORMAL, color_fail);
			set_tooltip (*l, "The signal is too loud.");
		} else if (lufs_i < presets[i].max_integrated[1]) {
			l = manage (new Label ("\u2713", ALIGN_CENTER)); // "check-mark" - maybe use inv-box: \u274C
			l->modify_fg (Gtk::STATE_NORMAL, color_warn);
			set_tooltip (*l, "The signal is too quiet, but satisfies the max. loudness spec.");
		} else {
			l = manage (new Label ("\u2713", ALIGN_CENTER)); // "check-mark"
			l->modify_fg (Gtk::STATE_NORMAL, color_good);
			set_tooltip (*l, "Signal loudness is within the spec.");
		}

		t->attach (*l, col + 1, col + 2, row, row + 1, SHRINK, SHRINK, 2, 0);

		if (++row == n_rows) {
			ArdourVSpacer* spc = manage (new ArdourVSpacer (1.0));
			t->attach (*spc, col + 2, col + 3, 0, n_rows, FILL, EXPAND|FILL, 8, 0);
			row = 0;
			col += 3;
		}
	}

	_conformity_expander.add (*t);
	_conformity_expander.show_all ();
}
