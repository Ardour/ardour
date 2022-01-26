/*
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 Ben Loftis <ben@harrisonconsoles.com>
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

#include <exception>
#include <vector>
#include <cmath>
#include <map>

#include <boost/scoped_ptr.hpp>

#include "pbd/error.h"
#include "pbd/locale_guard.h"
#include "pbd/xml++.h"
#include "pbd/unwind.h"
#include "pbd/failed_constructor.h"

#include <gtkmm/alignment.h>
#include <gtkmm/stock.h>
#include <gtkmm/notebook.h>
#include <gtkmm2ext/utils.h>

#include "widgets/tooltips.h"

#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/mtdm.h"
#include "ardour/mididm.h"
#include "ardour/rc_configuration.h"
#include "ardour/types.h"
#include "ardour/profile.h"

#include "pbd/convert.h"
#include "pbd/error.h"

#include "opts.h"
#include "debug.h"
#include "ardour_message.h"
#include "ardour_ui.h"
#include "engine_dialog.h"
#include "gui_thread.h"
#include "ui_config.h"
#include "public_editor.h"
#include "utils.h"
#include "pbd/i18n.h"
#include "splash.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;

#define DEBUG_ECONTROL(msg) DEBUG_TRACE (PBD::DEBUG::EngineControl, string_compose ("%1: %2\n", __LINE__, msg));

static const unsigned int midi_tab = 2;
static const unsigned int latency_tab = 1; /* zero-based, page zero is the main setup page */

static const char* results_markup = X_("<span weight=\"bold\" size=\"larger\">%1</span>");

EngineControl::EngineControl ()
	: ArdourDialog (_("Audio/MIDI Setup"))
	, engine_status ("")
	, basic_packer (9, 4)
	, input_latency_adjustment (0, 0, 99999, 1)
	, input_latency (input_latency_adjustment)
	, output_latency_adjustment (0, 0, 99999, 1)
	, output_latency (output_latency_adjustment)
	, input_channels_adjustment (0, 0, 256, 1)
	, input_channels (input_channels_adjustment)
	, output_channels_adjustment (0, 0, 256, 1)
	, output_channels (output_channels_adjustment)
	, ports_adjustment (128, 8, 1024, 1, 16)
	, ports_spinner (ports_adjustment)
	, control_app_button (_("Device Control Panel"))
	, midi_devices_button (_("Midi Device Setup"))
	, start_stop_button (_("Stop"))
	, update_devices_button (_("Refresh Devices"))
	, use_buffered_io_button (_("Use Buffered I/O"), ArdourButton::led_default_elements)
	, try_autostart_button (_("Autostart"), ArdourButton::led_default_elements)
	, lm_measure_label (_("Measure"))
	, lm_use_button (_("Use results"))
	, lm_back_button (_("Back to settings ... (ignore results)"))
	, lm_button_audio (_("Calibrate Audio"))
	, lm_table (12, 3)
	, have_lm_results (false)
	, lm_running (false)
	, midi_back_button (_("Back to settings"))
	, ignore_changes (0)
	, ignore_device_changes (0)
	, _desired_sample_rate (0)
	, started_at_least_once (false)
	, queue_device_changed (false)
	, _have_control (true)
	, block_signals(0)
{
	using namespace Notebook_Helpers;
	vector<string> backend_names;
	Label* label;
	AttachOptions xopt = AttachOptions (FILL|EXPAND);
	int row;

	set_name (X_("AudioMIDISetup"));

	/* the backend combo is the one thing that is ALWAYS visible */

	vector<const ARDOUR::AudioBackendInfo*> backends = ARDOUR::AudioEngine::instance()->available_backends();

	if (backends.empty()) {
		ArdourMessageDialog msg (string_compose (_("No audio/MIDI backends detected. %1 cannot run\n\n(This is a build/packaging/system error. It should never happen.)"), PROGRAM_NAME));
		msg.run ();
		throw failed_constructor ();
	}

	for (vector<const ARDOUR::AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		backend_names.push_back ((*b)->name);
	}

	set_popdown_strings (backend_combo, backend_names);

	/* setup basic packing characteristics for the table used on the main
	 * tab of the notebook
	 */

	basic_packer.set_spacings (6);
	basic_packer.set_border_width (12);
	basic_packer.set_homogeneous (false);

	/* pack it in */

	basic_hbox.pack_start (basic_packer, false, false);

	/* latency measurement tab */

	lm_title.set_markup (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", _("Latency Measurement Tool")));

	row = 0;
	lm_table.set_row_spacings (12);
	lm_table.set_col_spacings (6);
	lm_table.set_homogeneous (false);

	lm_table.attach (lm_title, 0, 3, row, row+1, xopt, (AttachOptions) 0);
	row++;

	lm_preamble.set_width_chars (60);
	lm_preamble.set_line_wrap (true);
	lm_preamble.set_markup (_("<span weight=\"bold\">Turn down the volume on your audio equipment to a very low level.</span>"));

	lm_table.attach (lm_preamble, 0, 3, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);
	row++;

	Gtk::Label* preamble;
	preamble = manage (new Label);
	preamble->set_width_chars (60);
	preamble->set_line_wrap (true);
	preamble->set_markup (_("Select two channels below and connect them using a cable."));

	lm_table.attach (*preamble, 0, 3, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);
	row++;

	label = manage (new Label (_("Output channel:")));
	lm_table.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);

	lm_output_channel_list = Gtk::ListStore::create (lm_output_channel_cols);
	lm_output_channel_combo.set_model (lm_output_channel_list);
	lm_output_channel_combo.pack_start (lm_output_channel_cols.pretty_name);

	Gtk::Alignment* misc_align = manage (new Alignment (0.0, 0.5));
	misc_align->add (lm_output_channel_combo);
	lm_table.attach (*misc_align, 1, 3, row, row+1, xopt, (AttachOptions) 0);
	++row;

	label = manage (new Label (_("Input channel:")));
	lm_table.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);

	lm_input_channel_list = Gtk::ListStore::create (lm_input_channel_cols);
	lm_input_channel_combo.set_model (lm_input_channel_list);
	lm_input_channel_combo.pack_start (lm_input_channel_cols.pretty_name);

	misc_align = manage (new Alignment (0.0, 0.5));
	misc_align->add (lm_input_channel_combo);
	lm_table.attach (*misc_align, 1, 3, row, row+1, FILL, (AttachOptions) 0);
	++row;

	lm_measure_label.set_padding (10, 10);
	lm_measure_button.add (lm_measure_label);
	lm_measure_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::latency_button_clicked));
	lm_use_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::use_latency_button_clicked));
	lm_back_button_signal = lm_back_button.signal_clicked().connect(
	    sigc::mem_fun(*this, &EngineControl::latency_back_button_clicked));

	lm_use_button.set_sensitive (false);

	/* Increase the default spacing around the labels of these three
	 * buttons
	 */

	Gtk::Misc* l;

	if ((l = dynamic_cast<Gtk::Misc*>(lm_use_button.get_child())) != 0) {
		l->set_padding (10, 10);
	}

	if ((l = dynamic_cast<Gtk::Misc*>(lm_back_button.get_child())) != 0) {
		l->set_padding (10, 10);
	}

	preamble = manage (new Label);
	preamble->set_width_chars (60);
	preamble->set_line_wrap (true);
	preamble->set_markup (_("Once the channels are connected, click the \"Measure\" button."));
	lm_table.attach (*preamble, 0, 3, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);
	row++;

	preamble = manage (new Label);
	preamble->set_width_chars (60);
	preamble->set_line_wrap (true);
	preamble->set_markup (_("When satisfied with the results, click the \"Use results\" button."));
	lm_table.attach (*preamble, 0, 3, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);

	++row; // skip a row in the table
	++row; // skip a row in the table

	lm_table.attach (lm_results, 0, 3, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);

	++row; // skip a row in the table
	++row; // skip a row in the table

	lm_table.attach (lm_measure_button, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);
	lm_table.attach (lm_use_button, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);
	lm_table.attach (lm_back_button, 2, 3, row, row+1, AttachOptions(FILL|EXPAND), (AttachOptions) 0);

	lm_results.set_markup (string_compose (results_markup, _("No measurement results yet")));

	lm_vbox.set_border_width (12);
	lm_vbox.pack_start (lm_table, false, false);

	midi_back_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (notebook, &Gtk::Notebook::set_current_page), 0));

	/* pack it all up */

	notebook.pages().push_back (TabElem (basic_vbox, _("Audio")));
	notebook.pages().push_back (TabElem (lm_vbox, _("Latency")));
	notebook.pages().push_back (TabElem (midi_vbox, _("MIDI")));
	notebook.set_border_width (12);

	notebook.set_show_tabs (false);
	notebook.show_all ();

	notebook.set_name ("SettingsNotebook");

	/* packup the notebook */

	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (notebook);

	/* need a special function to print "all available channels" when the
	 * channel counts hit zero.
	 */

	input_channels.signal_output().connect (sigc::bind (sigc::ptr_fun (&EngineControl::print_channel_count), &input_channels));
	output_channels.signal_output().connect (sigc::bind (sigc::ptr_fun (&EngineControl::print_channel_count), &output_channels));

	midi_devices_button.signal_clicked.connect (mem_fun (*this, &EngineControl::configure_midi_devices));
	midi_devices_button.set_name ("generic button");
	midi_devices_button.set_can_focus(true);

	control_app_button.signal_clicked.connect (mem_fun (*this, &EngineControl::control_app_button_clicked));
	control_app_button.set_name ("generic button");
	control_app_button.set_can_focus(true);
	manage_control_app_sensitivity ();

	start_stop_button.signal_clicked.connect (mem_fun (*this, &EngineControl::start_stop_button_clicked));
	start_stop_button.set_sensitive (false);
	start_stop_button.set_name ("generic button");
	start_stop_button.set_can_focus(true);
	start_stop_button.set_can_default(true);
	start_stop_button.set_act_on_release (false);

	update_devices_button.signal_clicked.connect (mem_fun (*this, &EngineControl::update_devices_button_clicked));
	update_devices_button.set_sensitive (false);
	update_devices_button.set_name ("generic button");
	update_devices_button.set_can_focus(true);

	use_buffered_io_button.signal_clicked.connect (mem_fun (*this, &EngineControl::use_buffered_io_button_clicked));
	use_buffered_io_button.set_sensitive (false);
	use_buffered_io_button.set_name ("generic button");
	use_buffered_io_button.set_can_focus(true);

	try_autostart_button.signal_clicked.connect (mem_fun (*this, &EngineControl::try_autostart_button_clicked));
	try_autostart_button.set_name ("generic button");
	try_autostart_button.set_can_focus(true);
	config_parameter_changed ("try-autostart-engine");
	set_tooltip (try_autostart_button,
			string_compose (_("Always try these settings when starting %1, if the same device is available"), PROGRAM_NAME));

	ARDOUR::Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&EngineControl::config_parameter_changed, this, _1), gui_context());

	/* Pick up any existing audio setup configuration, if appropriate */

	XMLNode* audio_setup = ARDOUR::Config->extra_xml ("AudioMIDISetup");

	ARDOUR::AudioEngine::instance()->Running.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::engine_running, this), gui_context());
	ARDOUR::AudioEngine::instance()->Stopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::engine_stopped, this), gui_context());
	ARDOUR::AudioEngine::instance()->Halted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::engine_stopped, this), gui_context());
	ARDOUR::AudioEngine::instance()->DeviceListChanged.connect (devicelist_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::device_list_changed, this), gui_context());

	if (audio_setup) {
		if (!set_state (*audio_setup)) {
			set_default_state ();
		}
	} else {
		set_default_state ();
	}

	update_sensitivity ();
	connect_changed_signals ();

	notebook.signal_switch_page().connect (sigc::mem_fun (*this, &EngineControl::on_switch_page));

	connect_disconnect_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::connect_disconnect_click));

	connect_disconnect_button.set_no_show_all();
	start_stop_button.set_no_show_all();
}

void
EngineControl::connect_changed_signals ()
{
	backend_combo_connection = backend_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::backend_changed));
	driver_combo_connection = driver_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::driver_changed));
	sample_rate_combo_connection = sample_rate_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::sample_rate_changed));
	buffer_size_combo_connection = buffer_size_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::buffer_size_changed));
	nperiods_combo_connection = nperiods_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::nperiods_changed));
	device_combo_connection = device_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::device_changed));
	midi_option_combo_connection = midi_option_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::midi_option_changed));

	input_device_combo_connection = input_device_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::input_device_changed));
	output_device_combo_connection = output_device_combo.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::output_device_changed));

	input_latency_connection = input_latency.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::latency_changed));
	output_latency_connection = output_latency.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::latency_changed));
	input_channels_connection = input_channels.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::channels_changed));
	output_channels_connection = output_channels.signal_changed ().connect (
	    sigc::mem_fun (*this, &EngineControl::channels_changed));
}

void
EngineControl::block_changed_signals ()
{
	if (block_signals++ == 0) {
		DEBUG_ECONTROL ("Blocking changed signals");
		backend_combo_connection.block ();
		driver_combo_connection.block ();
		sample_rate_combo_connection.block ();
		buffer_size_combo_connection.block ();
		nperiods_combo_connection.block ();
		device_combo_connection.block ();
		input_device_combo_connection.block ();
		output_device_combo_connection.block ();
		midi_option_combo_connection.block ();
		input_latency_connection.block ();
		output_latency_connection.block ();
		input_channels_connection.block ();
		output_channels_connection.block ();
	}
}

void
EngineControl::unblock_changed_signals ()
{
	if (--block_signals == 0) {
		DEBUG_ECONTROL ("Unblocking changed signals");
		backend_combo_connection.unblock ();
		driver_combo_connection.unblock ();
		sample_rate_combo_connection.unblock ();
		buffer_size_combo_connection.unblock ();
		nperiods_combo_connection.unblock ();
		device_combo_connection.unblock ();
		input_device_combo_connection.unblock ();
		output_device_combo_connection.unblock ();
		midi_option_combo_connection.unblock ();
		input_latency_connection.unblock ();
		output_latency_connection.unblock ();
		input_channels_connection.unblock ();
		output_channels_connection.unblock ();
	}
}

EngineControl::SignalBlocker::SignalBlocker (EngineControl& engine_control,
                                             const std::string& reason)
	: ec (engine_control)
	, m_reason (reason)
{
	DEBUG_ECONTROL (string_compose ("SignalBlocker: %1", m_reason));
	ec.block_changed_signals ();
}

EngineControl::SignalBlocker::~SignalBlocker ()
{
	DEBUG_ECONTROL (string_compose ("~SignalBlocker: %1", m_reason));
	ec.unblock_changed_signals ();
}

void
EngineControl::on_show ()
{
	ArdourDialog::on_show ();
	if (!ARDOUR::AudioEngine::instance()->current_backend() || !ARDOUR::AudioEngine::instance()->running()) {
		// re-check _have_control (jackd running) see #6041
		backend_changed ();
	}
	device_changed ();
	start_stop_button.grab_focus();
}

void
EngineControl::on_map ()
{
	if (!ARDOUR_UI::instance()->the_session () && !PublicEditor::_instance) {
		set_type_hint (Gdk::WINDOW_TYPE_HINT_NORMAL);
	} else if (UIConfiguration::instance().get_all_floating_windows_are_dialogs()) {
		set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);
	} else {
		set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
	}
	ArdourDialog::on_map ();
}

void
EngineControl::config_parameter_changed (std::string const & p)
{
	if (p == "try-autostart-engine") {
		try_autostart_button.set_active (ARDOUR::Config->get_try_autostart_engine ());
	}
}

bool
EngineControl::start_engine ()
{
	int rv = push_state_to_backend (true);
	if (rv < 0) {
		/* error message from backend */
		ArdourMessageDialog msg (*this, ARDOUR::AudioEngine::instance()->get_last_backend_error());
		msg.run();
	} else if (rv > 0) {
		/* error from push_state_to_backend() */
		// TODO: get error message from push_state_to_backend
		ArdourMessageDialog msg (*this, _("Could not configure Audio/MIDI engine with given settings."));
		msg.run();
	}
	return rv == 0;
}

bool
EngineControl::stop_engine (bool for_latency)
{
	if (ARDOUR::AudioEngine::instance()->stop(for_latency)) {
		return false;
	}
	return true;
}

void
EngineControl::build_notebook ()
{
	Label* label;
	AttachOptions xopt = AttachOptions (FILL|EXPAND);

	/* clear the table */

	Gtkmm2ext::container_clear (basic_vbox);
	Gtkmm2ext::container_clear (basic_packer);

	if (control_app_button.get_parent()) {
		control_app_button.get_parent()->remove (control_app_button);
	}

	label = manage (left_aligned_label (_("Audio System:")));
	basic_packer.attach (*label, 0, 1, 0, 1, xopt, (AttachOptions) 0);
	basic_packer.attach (backend_combo, 1, 2, 0, 1, xopt, (AttachOptions) 0);

	basic_packer.attach (engine_status, 2, 3, 0, 1, xopt, (AttachOptions) 0);
	engine_status.show();

	basic_packer.attach (start_stop_button, 3, 4, 0, 1, xopt, xopt);

	lm_button_audio.signal_clicked.connect (sigc::mem_fun (*this, &EngineControl::calibrate_audio_latency));
	lm_button_audio.set_name ("generic button");
	lm_button_audio.set_can_focus(true);

	if (_have_control) {
		build_full_control_notebook ();
	} else {
		build_no_control_notebook ();
	}

	basic_vbox.pack_start (basic_hbox, false, false);

	{
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
		basic_vbox.show_all ();
	}
}

void
EngineControl::build_full_control_notebook ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	using namespace Notebook_Helpers;
	Label* label;
	vector<string> strings;
	AttachOptions xopt = AttachOptions (FILL|EXPAND);
	int row = 1; // row zero == backend combo
	int btn = 1; // row zero == start_stop_button
	bool autostart_packed = false;

	/* start packing it up */

	if (backend->requires_driver_selection()) {
		label = manage (left_aligned_label (_("Driver:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (driver_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		row++;
	}

	if (backend->use_separate_input_and_output_devices()) {
		label = manage (left_aligned_label (_("Input Device:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (input_device_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		row++;
		label = manage (left_aligned_label (_("Output Device:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (output_device_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		row++;
		// reset so it isn't used in state comparisons
		device_combo.set_active_text ("");
	} else {
		label = manage (left_aligned_label (_("Device:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (device_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		row++;
		// reset these so they don't get used in state comparisons
		input_device_combo.set_active_text ("");
		output_device_combo.set_active_text ("");
	}

	/* same line as Driver */
	if (backend->can_use_buffered_io()) {
		basic_packer.attach (use_buffered_io_button, 3, 4, btn, btn + 1, xopt, xopt);
		btn++;
	}

	/* same line as Device(s) */
	if (backend->can_request_update_devices()) {
		basic_packer.attach (update_devices_button, 3, 4, btn, btn + 1, xopt, xopt);
		btn++;
	}

	/* prefer "try autostart" below "Start" if possible */
	if (btn < row) {
		basic_packer.attach (try_autostart_button, 3, 4, btn, btn + 1, xopt, xopt);
		btn++;
		autostart_packed = true;
	}

	label = manage (left_aligned_label (_("Sample rate:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
	row++;

	label = manage (left_aligned_label (_("Buffer size:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (buffer_size_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
	buffer_size_duration_label.set_alignment (0.0); /* left-align */
	basic_packer.attach (buffer_size_duration_label, 2, 3, row, row+1, SHRINK, (AttachOptions) 0);

	int ctrl_btn_span = 1;
	if (backend->can_set_period_size ()) {
		row++;
		label = manage (left_aligned_label (_("Periods:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (nperiods_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		++ctrl_btn_span;
	}

	/* button spans 2 or 3 rows: Sample rate, Buffer size, Periods */
	basic_packer.attach (control_app_button, 3, 4, row - ctrl_btn_span, row + 1, xopt, xopt);
	row++;

	input_channels.set_name ("InputChannels");
	input_channels.set_flags (Gtk::CAN_FOCUS);
	input_channels.set_digits (0);
	input_channels.set_wrap (false);
	output_channels.set_editable (true);

	if (!ARDOUR::Profile->get_mixbus()) {
		label = manage (left_aligned_label (_("Input channels:")));
		basic_packer.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);
		basic_packer.attach (input_channels, 1, 2, row, row+1, xopt, (AttachOptions) 0);
		++row;
	}

	output_channels.set_name ("OutputChannels");
	output_channels.set_flags (Gtk::CAN_FOCUS);
	output_channels.set_digits (0);
	output_channels.set_wrap (false);
	output_channels.set_editable (true);

	if (!ARDOUR::Profile->get_mixbus()) {
		label = manage (left_aligned_label (_("Output channels:")));
		basic_packer.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);
		basic_packer.attach (output_channels, 1, 2, row, row+1, xopt, (AttachOptions) 0);
		++row;
	}

	/* Prefer next available vertical slot, 1 row */
	if (btn < row && !autostart_packed) {
		basic_packer.attach (try_autostart_button, 3, 4, btn, btn + 1, xopt, xopt);
		btn++;
		autostart_packed = true;
	}

	input_latency.set_name ("InputLatency");
	input_latency.set_flags (Gtk::CAN_FOCUS);
	input_latency.set_digits (0);
	input_latency.set_wrap (false);
	input_latency.set_editable (true);

	label = manage (left_aligned_label (_("Hardware input latency:")));
	basic_packer.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);
	basic_packer.attach (input_latency, 1, 2, row, row+1, xopt, (AttachOptions) 0);
	label = manage (left_aligned_label (_("samples")));
	basic_packer.attach (*label, 2, 3, row, row+1, SHRINK, (AttachOptions) 0);
	++row;

	output_latency.set_name ("OutputLatency");
	output_latency.set_flags (Gtk::CAN_FOCUS);
	output_latency.set_digits (0);
	output_latency.set_wrap (false);
	output_latency.set_editable (true);

	label = manage (left_aligned_label (_("Hardware output latency:")));
	basic_packer.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);
	basic_packer.attach (output_latency, 1, 2, row, row+1, xopt, (AttachOptions) 0);
	label = manage (left_aligned_label (_("samples")));
	basic_packer.attach (*label, 2, 3, row, row+1, SHRINK, (AttachOptions) 0);

	/* button spans 2 rows */

	basic_packer.attach (lm_button_audio, 3, 4, row-1, row+1, xopt, xopt);
	++row;

	label = manage (left_aligned_label (_("MIDI System:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (midi_option_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (midi_devices_button, 3, 4, row, row+1, xopt, xopt);
	row++;

	if (!autostart_packed) {
		basic_packer.attach (try_autostart_button, 3, 4, row, row+1, xopt, xopt);
	}
}

void
EngineControl::build_no_control_notebook ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	using namespace Notebook_Helpers;
	Label* label;
	vector<string> strings;
	AttachOptions xopt = AttachOptions (FILL|EXPAND);
	int row = 1; // row zero == backend combo
	const string msg = string_compose (_("%1 is already running. %2 will connect to it and use the existing settings."), backend->name(), PROGRAM_NAME);

	label = manage (new Label);
	label->set_markup (string_compose ("<span weight=\"bold\" foreground=\"red\">%1</span>", msg));
	basic_packer.attach (*label, 0, 2, row, row + 1, xopt, (AttachOptions) 0);
	row++;

	if (backend->can_change_sample_rate_when_running()) {
		label = manage (left_aligned_label (_("Sample rate:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (sample_rate_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		row++;
	}

	if (backend->can_change_buffer_size_when_running()) {
		label = manage (left_aligned_label (_("Buffer size:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (buffer_size_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		buffer_size_duration_label.set_alignment (0.0); /* left-align */
		basic_packer.attach (buffer_size_duration_label, 2, 3, row, row+1, xopt, (AttachOptions) 0);
		row++;
	}

	basic_packer.attach (connect_disconnect_button, 0, 2, row, row+1, FILL, AttachOptions (0));
	row++;
}

EngineControl::~EngineControl ()
{
	ignore_changes = true;
}

void
EngineControl::disable_latency_tab ()
{
	lm_input_channel_list->clear ();
	lm_output_channel_list->clear ();
	lm_measure_button.set_sensitive (false);
	lm_use_button.set_sensitive (false);
}

void
EngineControl::enable_latency_tab ()
{
	State state = get_saved_state_for_currently_displayed_backend_and_device ();

	vector<string> outputs;
	vector<string> inputs;

	ARDOUR::DataType const type = _measure_midi ? ARDOUR::DataType::MIDI : ARDOUR::DataType::AUDIO;
	ARDOUR::AudioEngine::instance()->get_physical_outputs (type, outputs);
	ARDOUR::AudioEngine::instance()->get_physical_inputs (type, inputs);

	if (!ARDOUR::AudioEngine::instance()->running()) {
		ArdourMessageDialog msg (_("Failed to start or connect to audio-engine.\n\nLatency calibration requires a working audio interface."));
		notebook.set_current_page (0);
		msg.run ();
		return;
	}
	else if (inputs.empty() || outputs.empty()) {
		ArdourMessageDialog msg (_("Your selected audio configuration is playback- or capture-only.\n\nLatency calibration requires playback and capture"));
		notebook.set_current_page (0);
		msg.run ();
		return;
	}

	lm_back_button_signal.disconnect();
	if (_measure_midi) {
		lm_back_button_signal = lm_back_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (notebook, &Gtk::Notebook::set_current_page), midi_tab));
		lm_preamble.hide ();
	} else {
		lm_back_button_signal = lm_back_button.signal_clicked().connect(
		    sigc::mem_fun(*this, &EngineControl::latency_back_button_clicked));
		lm_preamble.show ();
	}

	unsigned int j = 0;
	unsigned int n = 0;
	lm_output_channel_list->clear ();
	for (vector<string>::const_iterator i = outputs.begin(); i != outputs.end(); ++i, ++j) {
		Gtk::TreeModel::iterator iter = lm_output_channel_list->append ();
		Gtk::TreeModel::Row row = *iter;
		row[lm_output_channel_cols.port_name] = *i;
		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*i);
		if (pn.empty()) {
			pn = (*i).substr ((*i).find (':') + 1);
		}
		row[lm_output_channel_cols.pretty_name] = pn;
		if (state && state->lm_output == *i) {
			n = j;
		}
	}
	lm_output_channel_combo.set_active (n);
	lm_output_channel_combo.set_sensitive (true);

	j = n = 0;
	lm_input_channel_list->clear ();
	for (vector<string>::const_iterator i = inputs.begin(); i != inputs.end(); ++i, ++j) {
		Gtk::TreeModel::iterator iter = lm_input_channel_list->append ();
		Gtk::TreeModel::Row row = *iter;
		row[lm_input_channel_cols.port_name] = *i;
		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*i);
		if (pn.empty()) {
			pn = (*i).substr ((*i).find (':') + 1);
		}
		row[lm_input_channel_cols.pretty_name] = pn;
		if (state && state->lm_input == *i) {
			n = j;
		}
	}
	lm_input_channel_combo.set_active (n);
	lm_input_channel_combo.set_sensitive (true);

	lm_measure_button.set_sensitive (true);
}

void
EngineControl::setup_midi_tab_for_backend ()
{
	string backend = backend_combo.get_active_text ();

	Gtkmm2ext::container_clear (midi_vbox);

	midi_vbox.set_border_width (12);
	midi_device_table.set_border_width (12);

	if (backend == "JACK") {
		setup_midi_tab_for_jack ();
	}

	midi_vbox.pack_start (midi_device_table, true, true);
	midi_vbox.pack_start (midi_back_button, false, false);
	midi_vbox.show_all ();
}

void
EngineControl::update_sensitivity ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	if (!backend) {
		start_stop_button.set_sensitive (false);
		return;
	}

	bool valid = true;
	size_t devices_available = 0;
	bool engine_running = ARDOUR::AudioEngine::instance()->running();

	if (backend->use_separate_input_and_output_devices ()) {
		devices_available += get_popdown_string_count (input_device_combo);
		devices_available += get_popdown_string_count (output_device_combo);
	} else {
		devices_available += get_popdown_string_count (device_combo);
	}

	if (devices_available == 0) {
		valid = false;
		input_latency.set_sensitive (false);
		output_latency.set_sensitive (false);
		input_channels.set_sensitive (false);
		output_channels.set_sensitive (false);
	} else {
		input_latency.set_sensitive (true);
		output_latency.set_sensitive (true);
		input_channels.set_sensitive (!engine_running);
		output_channels.set_sensitive (!engine_running);
	}

	if (get_popdown_string_count (buffer_size_combo) > 0) {
		if (!engine_running) {
			buffer_size_combo.set_sensitive (valid);
		} else if (backend->can_change_buffer_size_when_running ()) {
			buffer_size_combo.set_sensitive (valid || !_have_control);
		} else {
			buffer_size_combo.set_sensitive (false);
		}
	} else {
		buffer_size_combo.set_sensitive (false);
		valid = false;
	}

	if (!engine_running || backend->can_change_systemic_latency_when_running ()) {
		input_latency.set_sensitive (true);
		output_latency.set_sensitive (true);
	} else {
		input_latency.set_sensitive (false);
		output_latency.set_sensitive (false);
	}

	if (get_popdown_string_count (sample_rate_combo) > 0) {
		bool allow_to_set_rate = false;
		if (!engine_running) {
			if (!ARDOUR_UI::instance()->the_session ()) {
				// engine is not running, no session loaded -> anything goes.
				allow_to_set_rate = true;
			} else if (_desired_sample_rate > 0 && get_rate () != _desired_sample_rate) {
				// only allow to change if the current setting is not the native session rate.
				allow_to_set_rate = true;
			}
		}
		sample_rate_combo.set_sensitive (allow_to_set_rate);
	} else {
		sample_rate_combo.set_sensitive (false);
		valid = false;
	}

	if (get_popdown_string_count (nperiods_combo) > 0) {
		if (!engine_running) {
			nperiods_combo.set_sensitive (true);
		} else {
			nperiods_combo.set_sensitive (false);
		}
	} else {
		nperiods_combo.set_sensitive (false);
	}

	if (_have_control) {
		start_stop_button.set_sensitive(true);
		start_stop_button.show();
		if (engine_running) {
			start_stop_button.set_text(S_("Engine|Stop"));
			update_devices_button.set_sensitive(false);
			use_buffered_io_button.set_sensitive(false);
		} else {
			start_stop_button.set_text(S_("Engine|Start"));
			update_devices_button.set_sensitive (backend->can_request_update_devices ());
			use_buffered_io_button.set_sensitive (backend->can_use_buffered_io ());
		}
	} else {
		start_stop_button.set_sensitive(false);
		start_stop_button.hide();
	}

	if (engine_running && _have_control) {
		input_device_combo.set_sensitive (false);
		output_device_combo.set_sensitive (false);
		device_combo.set_sensitive (false);
		driver_combo.set_sensitive (false);
	} else {
		input_device_combo.set_sensitive (true);
		output_device_combo.set_sensitive (true);
		device_combo.set_sensitive (true);
		if (backend->requires_driver_selection() && get_popdown_string_count(driver_combo) > 0) {
			driver_combo.set_sensitive (true);
		} else {
			driver_combo.set_sensitive (false);
		}
	}

	midi_option_combo.set_sensitive (!engine_running);
}

void
EngineControl::setup_midi_tab_for_jack ()
{
}

void
EngineControl::midi_latency_adjustment_changed (Gtk::Adjustment *a, MidiDeviceSettings device, bool for_input) {
	if (for_input) {
		device->input_latency = a->get_value();
	} else {
		device->output_latency = a->get_value();
	}

	if (ARDOUR::AudioEngine::instance()->running() && !_measure_midi) {
		boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
		assert (backend);
		if (backend->can_change_systemic_latency_when_running () && device->enabled) {
			if (for_input) {
				backend->set_systemic_midi_input_latency (device->name, device->input_latency);
			} else {
				backend->set_systemic_midi_output_latency (device->name, device->output_latency);
			}
		}
	}
}

void
EngineControl::midi_device_enabled_toggled (ArdourButton *b, MidiDeviceSettings device) {
	b->set_active (!b->get_active());
	device->enabled = b->get_active();
	refresh_midi_display(device->name);

	if (ARDOUR::AudioEngine::instance()->running()) {
		boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
		assert (backend);
		backend->set_midi_device_enabled (device->name, device->enabled);
		if (backend->can_change_systemic_latency_when_running () && device->enabled) {
			backend->set_systemic_midi_input_latency (device->name, device->input_latency);
			backend->set_systemic_midi_output_latency (device->name, device->output_latency);
		}
	}
}

void
EngineControl::refresh_midi_display (std::string focus)
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	int row  = 0;
	AttachOptions xopt = AttachOptions (FILL|EXPAND);
	Gtk::Label* l;

	Gtkmm2ext::container_clear (midi_device_table);

	midi_device_table.set_spacings (6);

	l = manage (new Label);
	l->set_markup (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", _("MIDI Devices")));
	midi_device_table.attach (*l, 0, 4, row, row + 1, xopt, AttachOptions (0));
	l->set_alignment (0.5, 0.5);
	row++;
	l->show ();

	l = manage (new Label (_("Device"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 0, 1, row, row + 2, xopt, AttachOptions (0));
	l = manage (new Label (_("Systemic Latency [samples]"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 1, 3, row, row + 1, xopt, AttachOptions (0));
	row++;
	l = manage (new Label (_("Input"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 1, 2, row, row + 1, xopt, AttachOptions (0));
	l = manage (new Label (_("Output"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 2, 3, row, row + 1, xopt, AttachOptions (0));
	row++;

	/* Don't autostart engine for MIDI latency compensation, only allow to configure when running
	 * or when the engine is stopped after calibration (otherwise ardour proceeds to load session).
	 */
	bool allow_calibration = ARDOUR::AudioEngine::instance()->running() || !backend->can_change_systemic_latency_when_running ();

	for (vector<MidiDeviceSettings>::const_iterator p = _midi_devices.begin(); p != _midi_devices.end(); ++p) {
		ArdourButton *m;
		Gtk::Button* b;
		Gtk::Adjustment *a;
		Gtk::SpinButton *s;
		bool enabled = (*p)->enabled;

		m = manage (new ArdourButton ((*p)->name, ArdourButton::led_default_elements));
		m->set_name ("midi device");
		m->set_can_focus (true);
		m->add_events (Gdk::BUTTON_RELEASE_MASK);
		m->set_active (enabled);
		m->signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &EngineControl::midi_device_enabled_toggled), m, *p));
		midi_device_table.attach (*m, 0, 1, row, row + 1, xopt, AttachOptions (0)); m->show ();
		if ((*p)->name == focus) {
			m->grab_focus();
		}

		a = manage (new Gtk::Adjustment (0, 0, 99999, 1));
		s = manage (new Gtk::SpinButton (*a));
		a->set_value ((*p)->input_latency);
		s->signal_value_changed().connect (sigc::bind (sigc::mem_fun (*this, &EngineControl::midi_latency_adjustment_changed), a, *p, true));
		s->set_sensitive (_can_set_midi_latencies && enabled);
		midi_device_table.attach (*s, 1, 2, row, row + 1, xopt, AttachOptions (0)); s->show ();

		a = manage (new Gtk::Adjustment (0, 0, 99999, 1));
		s = manage (new Gtk::SpinButton (*a));
		a->set_value ((*p)->output_latency);
		s->signal_value_changed().connect (sigc::bind (sigc::mem_fun (*this, &EngineControl::midi_latency_adjustment_changed), a, *p, false));
		s->set_sensitive (_can_set_midi_latencies && enabled);
		midi_device_table.attach (*s, 2, 3, row, row + 1, xopt, AttachOptions (0)); s->show ();

		b = manage (new Button (_("Calibrate")));
		b->signal_clicked().connect (sigc::bind (sigc::mem_fun (*this, &EngineControl::calibrate_midi_latency), *p));
		b->set_sensitive (_can_set_midi_latencies && enabled && allow_calibration);
		midi_device_table.attach (*b, 3, 4, row, row + 1, xopt, AttachOptions (0)); b->show ();

		row++;
	}
}

void
EngineControl::backend_changed ()
{
	SignalBlocker blocker (*this, "backend_changed");
	string backend_name = backend_combo.get_active_text();
	boost::shared_ptr<ARDOUR::AudioBackend> backend;

	if (!(backend = ARDOUR::AudioEngine::instance()->set_backend (backend_name, downcase (std::string(PROGRAM_NAME)), ""))) {
		/* eh? setting the backend failed... how ? */
		/* A: stale config contains a backend that does not exist in current build */
		return;
	}

	DEBUG_ECONTROL (string_compose ("Backend name: %1", backend_name));

	_have_control = ARDOUR::AudioEngine::instance()->setup_required ();

	build_notebook ();
	setup_midi_tab_for_backend ();
	_midi_devices.clear();

	if (backend->requires_driver_selection()) {
		if (set_driver_popdown_strings ()) {
			driver_changed ();
		}
	} else {
		/* this will change the device text which will cause a call to
		 * device changed which will set up parameters
		 */
		list_devices ();
	}

	update_midi_options ();

	connect_disconnect_button.hide();

	midi_option_changed();

	started_at_least_once = false;

	/* changing the backend implies stopping the engine
	 * ARDOUR::AudioEngine() may or may not emit this signal
	 * depending on previous engine state
	 */
	engine_stopped (); // set "active/inactive"

	if (!_have_control) {
		// set settings from backend that we do have control over
		set_buffersize_popdown_strings ();
		set_active_text_if_present (buffer_size_combo, bufsize_as_string (backend->buffer_size()));
	}

	if (_have_control && !ignore_changes) {
		if (!set_state_for_backend (backend_combo.get_active_text ())) {
			DEBUG_ECONTROL ("backend-changed(): no prior state for backend");
		}

	} else {
		DEBUG_ECONTROL (string_compose ("backend-changed(): _have_control=%1 ignore_changes=%2", _have_control, ignore_changes));
	}

	if (!ignore_changes) {
		maybe_display_saved_state ();
	}
}

void
EngineControl::update_midi_options ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<string> midi_options = backend->enumerate_midi_options();

	if (midi_options.size() == 1 || _have_control) {
		set_popdown_strings (midi_option_combo, midi_options);
		midi_option_combo.set_active_text (midi_options.front());
	}
}

bool
EngineControl::print_channel_count (Gtk::SpinButton* sb)
{
	if (ARDOUR::Profile->get_mixbus()) {
		return true;
	}

	uint32_t cnt = (uint32_t) sb->get_value();
	if (cnt == 0) {
		sb->set_text (_("all available channels"));
	} else {
		char buf[32];
		snprintf (buf, sizeof (buf), "%d", cnt);
		sb->set_text (buf);
	}
	return true;
}

// @return true if there are drivers available
bool
EngineControl::set_driver_popdown_strings ()
{
	DEBUG_ECONTROL ("set_driver_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<string> drivers = backend->enumerate_drivers();

	if (drivers.empty ()) {
		// This is an error...?
		return false;
	}

	string current_driver = backend->driver_name ();

	DEBUG_ECONTROL (string_compose ("backend->driver_name: %1", current_driver));

	if (std::find (drivers.begin (), drivers.end (), current_driver) ==
	    drivers.end ()) {

		current_driver = drivers.front ();
	}

	set_popdown_strings (driver_combo, drivers);
	DEBUG_ECONTROL (
	    string_compose ("driver_combo.set_active_text: %1", current_driver));
	driver_combo.set_active_text (current_driver);
	return true;
}

std::string
EngineControl::get_default_device(const string& current_device_name,
                                  const vector<string>& available_devices)
{
	// If the current device is available, use it as default
	if (std::find (available_devices.begin (),
	               available_devices.end (),
	               current_device_name) != available_devices.end ()) {

		return current_device_name;
	}

	using namespace ARDOUR;

	string default_device_name =
	    AudioBackend::get_standard_device_name(AudioBackend::DeviceDefault);

	vector<string>::const_iterator i;

	// If there is a "Default" device available, use it
	for (i = available_devices.begin(); i != available_devices.end(); ++i) {
		if (*i == default_device_name) {
			return *i;
		}
	}

	string none_device_name =
	    AudioBackend::get_standard_device_name(AudioBackend::DeviceNone);

	// Use the first device that isn't "None"
	for (i = available_devices.begin(); i != available_devices.end(); ++i) {
		if (*i != none_device_name) {
			return *i;
		}
	}

	// Use "None" if there are no other available
	return available_devices.front();
}

// @return true if there are devices available
bool
EngineControl::set_device_popdown_strings ()
{
	DEBUG_ECONTROL ("set_device_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<ARDOUR::AudioBackend::DeviceStatus> all_devices = backend->enumerate_devices ();

	/* NOTE: Ardour currently does not display the "available" field of the
	 * returned devices.
	 *
	 * Doing so would require a different GUI widget than the combo
	 * box/popdown that we currently use, since it has no way to list
	 * items that are not selectable. Something more like a popup menu,
	 * which could have unselectable items, would be appropriate.
	 */

	vector<string> available_devices;

	for (vector<ARDOUR::AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		available_devices.push_back (i->name);
	}

	if (available_devices.empty ()) {
		return false;
	}

	set_popdown_strings (device_combo, available_devices);

	std::string default_device =
	    get_default_device(backend->device_name(), available_devices);

	DEBUG_ECONTROL (
	    string_compose ("set device_combo active text: %1", default_device));

	device_combo.set_active_text(default_device);
	return true;
}

// @return true if there are input devices available
bool
EngineControl::set_input_device_popdown_strings ()
{
	DEBUG_ECONTROL ("set_input_device_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<ARDOUR::AudioBackend::DeviceStatus> all_devices = backend->enumerate_input_devices ();

	vector<string> available_devices;

	for (vector<ARDOUR::AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		available_devices.push_back (i->name);
	}

	if (available_devices.empty()) {
		return false;
	}

	set_popdown_strings (input_device_combo, available_devices);

	std::string default_device =
	    get_default_device(backend->input_device_name(), available_devices);

	DEBUG_ECONTROL (
	    string_compose ("set input_device_combo active text: %1", default_device));
	input_device_combo.set_active_text(default_device);
	return true;
}

// @return true if there are output devices available
bool
EngineControl::set_output_device_popdown_strings ()
{
	DEBUG_ECONTROL ("set_output_device_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<ARDOUR::AudioBackend::DeviceStatus> all_devices = backend->enumerate_output_devices ();

	vector<string> available_devices;

	for (vector<ARDOUR::AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		available_devices.push_back (i->name);
	}

	if (available_devices.empty()) {
		return false;
	}

	set_popdown_strings (output_device_combo, available_devices);

	std::string default_device =
	    get_default_device(backend->output_device_name(), available_devices);

	DEBUG_ECONTROL (
	    string_compose ("set output_device_combo active text: %1", default_device));
	output_device_combo.set_active_text(default_device);
	return true;
}

void
EngineControl::list_devices ()
{
	DEBUG_ECONTROL ("list_devices");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	/* now fill out devices, mark sample rates, buffer sizes insensitive */

	bool devices_available = false;

	block_changed_signals ();
	if (backend->use_separate_input_and_output_devices ()) {
		bool input_devices_available = set_input_device_popdown_strings ();
		bool output_devices_available = set_output_device_popdown_strings ();
		devices_available = input_devices_available || output_devices_available;
	} else {
		devices_available = set_device_popdown_strings ();
	}
	unblock_changed_signals ();

	if (devices_available) {
		device_changed ();
	} else {
		device_combo.clear();
		input_device_combo.clear();
		output_device_combo.clear();
	}
	update_sensitivity ();
}

void
EngineControl::driver_changed ()
{
	SignalBlocker blocker (*this, "driver_changed");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	backend->set_driver (driver_combo.get_active_text());
	list_devices ();

	// TODO load LRU device(s) for backend + driver combo

	if (!ignore_changes) {
		maybe_display_saved_state ();
	}
}

vector<float>
EngineControl::get_sample_rates_for_all_devices ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend =
	    ARDOUR::AudioEngine::instance ()->current_backend ();
	vector<float> all_rates;

	if (backend->use_separate_input_and_output_devices ()) {
		all_rates = backend->available_sample_rates2 (get_input_device_name (), get_output_device_name ());
	} else {
		all_rates = backend->available_sample_rates (get_device_name ());
	}
	return all_rates;
}

vector<float>
EngineControl::get_default_sample_rates ()
{
	vector<float> rates;
	rates.push_back (8000.0f);
	rates.push_back (16000.0f);
	rates.push_back (32000.0f);
	rates.push_back (44100.0f);
	rates.push_back (48000.0f);
	rates.push_back (88200.0f);
	rates.push_back (96000.0f);
	rates.push_back (192000.0f);
	rates.push_back (384000.0f);
	return rates;
}

void
EngineControl::set_samplerate_popdown_strings ()
{
	DEBUG_ECONTROL ("set_samplerate_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	string desired;
	vector<float> sr;
	vector<string> s;

	if (_have_control) {
		sr = get_sample_rates_for_all_devices ();
	} else {
		sr = get_default_sample_rates ();
	}

	for (vector<float>::const_iterator x = sr.begin(); x != sr.end(); ++x) {
		s.push_back (rate_as_string (*x));
		if (*x == _desired_sample_rate) {
			desired = s.back();
		}
	}

	set_popdown_strings (sample_rate_combo, s);

	if (!s.empty()) {
		if (ARDOUR::AudioEngine::instance()->running()) {
			sample_rate_combo.set_active_text (rate_as_string (backend->sample_rate()));
		} else if (ARDOUR_UI::instance()->the_session ()) {
			float active_sr = ARDOUR_UI::instance()->the_session()->nominal_sample_rate ();

			if (std::find (sr.begin (), sr.end (), active_sr) == sr.end ()) {
				active_sr = sr.front ();
			}

			sample_rate_combo.set_active_text (rate_as_string (active_sr));
		} else if (desired.empty ()) {
			float new_active_sr = backend->default_sample_rate ();

			if (std::find (sr.begin (), sr.end (), new_active_sr) == sr.end ()) {
				new_active_sr = sr.front ();
			}

			sample_rate_combo.set_active_text (rate_as_string (new_active_sr));
		} else {
			sample_rate_combo.set_active_text (desired);
		}
	}
	update_sensitivity ();
}

vector<uint32_t>
EngineControl::get_buffer_sizes_for_all_devices ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend =
	    ARDOUR::AudioEngine::instance ()->current_backend ();
	vector<uint32_t> all_sizes;

	if (backend->use_separate_input_and_output_devices ()) {
		all_sizes = backend->available_buffer_sizes2 (get_input_device_name (), get_output_device_name ());
	} else {
		all_sizes = backend->available_buffer_sizes (get_device_name ());
	}
	return all_sizes;
}

vector<uint32_t>
EngineControl::get_default_buffer_sizes ()
{
	vector<uint32_t> sizes;
	sizes.push_back (8);
	sizes.push_back (16);
	sizes.push_back (32);
	sizes.push_back (64);
	sizes.push_back (128);
	sizes.push_back (256);
	sizes.push_back (512);
	sizes.push_back (1024);
	sizes.push_back (2048);
	sizes.push_back (4096);
	sizes.push_back (8192);
	return sizes;
}

void
EngineControl::set_buffersize_popdown_strings ()
{
	DEBUG_ECONTROL ("set_buffersize_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<uint32_t> bs;
	vector<string> s;

	if (_have_control) {
		bs = get_buffer_sizes_for_all_devices ();
	} else if (backend->can_change_buffer_size_when_running()) {
		bs = get_default_buffer_sizes ();
	}

	for (vector<uint32_t>::const_iterator x = bs.begin(); x != bs.end(); ++x) {
		s.push_back (bufsize_as_string (*x));
	}

	uint32_t previous_size = backend->buffer_size ();
	if (!buffer_size_combo.get_active_text().empty()) {
		previous_size = get_buffer_size ();
	}

	set_popdown_strings (buffer_size_combo, s);

	if (!s.empty()) {

		if (std::find(bs.begin(), bs.end(), previous_size) != bs.end()) {
			buffer_size_combo.set_active_text(bufsize_as_string(previous_size));
		} else {

			buffer_size_combo.set_active_text(s.front());

			uint32_t period = backend->buffer_size();
			if (0 == period && backend->use_separate_input_and_output_devices()) {
				period = backend->default_buffer_size(get_input_device_name());
			}
			if (0 == period && backend->use_separate_input_and_output_devices()) {
				period = backend->default_buffer_size(get_output_device_name());
			}
			if (0 == period && !backend->use_separate_input_and_output_devices()) {
				period = backend->default_buffer_size(get_device_name());
			}

			set_active_text_if_present(buffer_size_combo, bufsize_as_string(period));
		}
		show_buffer_duration ();
	}
	update_sensitivity ();
}

void
EngineControl::set_nperiods_popdown_strings ()
{
	DEBUG_ECONTROL ("set_nperiods_popdown_strings");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	vector<uint32_t> np;
	vector<string> s;

	if (backend->can_set_period_size()) {
		if (backend->use_separate_input_and_output_devices ()) {
			np = backend->available_period_sizes (get_driver(), get_output_device_name ());
		} else {
			np = backend->available_period_sizes (get_driver(), get_device_name ());
		}
	}

	for (vector<uint32_t>::const_iterator x = np.begin(); x != np.end(); ++x) {
		s.push_back (to_string (*x));
	}

	set_popdown_strings (nperiods_combo, s);

	if (!s.empty()) {
		set_active_text_if_present (nperiods_combo, to_string (backend->period_size())); // XXX
	}

	update_sensitivity ();
}

void
EngineControl::device_changed ()
{
	SignalBlocker blocker (*this, "device_changed");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	string device_name_in;
	string device_name_out; // only used if backend support separate I/O devices

	if (backend->use_separate_input_and_output_devices()) {
		device_name_in  = get_input_device_name ();
		device_name_out = get_output_device_name ();
	} else {
		device_name_in = get_device_name ();
	}

	/* we set the backend-device to query various device related intormation.
	 * This has the side effect that backend->device_name() will match
	 * the device_name and  'change_device' will never be true.
	 * so work around this by setting...
	 */
	if (backend->use_separate_input_and_output_devices()) {
		if (device_name_in != backend->input_device_name() || device_name_out != backend->output_device_name ()) {
			queue_device_changed = true;
		}
	} else {
		if (device_name_in != backend->device_name()) {
			queue_device_changed = true;
		}
	}

	//the device name must be set FIRST so ASIO can populate buffersizes and the control panel button
	if (backend->use_separate_input_and_output_devices()) {
		backend->set_input_device_name (device_name_in);
		backend->set_output_device_name (device_name_out);
	} else {
		backend->set_device_name(device_name_in);
	}

	{
		/* don't allow programmatic change to combos to cause a
		   recursive call to this method.
		 */
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);

		set_samplerate_popdown_strings ();
		set_buffersize_popdown_strings ();
		set_nperiods_popdown_strings ();

		/* TODO set min + max channel counts here */

		manage_control_app_sensitivity ();
	}

	/* pick up any saved state for this device */

	if (!ignore_changes) {
		maybe_display_saved_state ();
	}
}

void
EngineControl::input_device_changed ()
{
	DEBUG_ECONTROL ("input_device_changed");

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	if (backend && backend->match_input_output_devices_or_none ()) {
		const std::string& dev_none = ARDOUR::AudioBackend::get_standard_device_name (ARDOUR::AudioBackend::DeviceNone);

		if (get_output_device_name () != dev_none
				&& get_input_device_name () != dev_none
				&& get_input_device_name () != get_output_device_name ()) {
			block_changed_signals ();
			if (contains_value (output_device_combo, get_input_device_name ())) {
				output_device_combo.set_active_text (get_input_device_name ());
			} else {
				assert (contains_value (output_device_combo, dev_none));
				output_device_combo.set_active_text (dev_none);
			}
			unblock_changed_signals ();
		}
	}
	device_changed ();
}

void
EngineControl::output_device_changed ()
{
	DEBUG_ECONTROL ("output_device_changed");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	if (backend && backend->match_input_output_devices_or_none ()) {
		const std::string& dev_none = ARDOUR::AudioBackend::get_standard_device_name (ARDOUR::AudioBackend::DeviceNone);

		if (get_input_device_name () != dev_none
				&& get_input_device_name () != dev_none
				&& get_input_device_name () != get_output_device_name ()) {
			block_changed_signals ();
			if (contains_value (input_device_combo, get_output_device_name ())) {
				input_device_combo.set_active_text (get_output_device_name ());
			} else {
				assert (contains_value (input_device_combo, dev_none));
				input_device_combo.set_active_text (dev_none);
			}
			unblock_changed_signals ();
		}
	}
	device_changed ();
}

string
EngineControl::bufsize_as_string (uint32_t sz)
{
	return string_compose (P_("%1 sample", "%1 samples", sz), to_string(sz));
}

void
EngineControl::sample_rate_changed ()
{
	DEBUG_ECONTROL ("sample_rate_changed");
	/* reset the strings for buffer size to show the correct msec value
	   (reflecting the new sample rate).
	 */

	show_buffer_duration ();

}

void
EngineControl::buffer_size_changed ()
{
	DEBUG_ECONTROL ("buffer_size_changed");
	if (ARDOUR::AudioEngine::instance()->running()) {
		boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
		if (backend && backend->can_change_buffer_size_when_running ()) {
			backend->set_buffer_size (get_buffer_size());
		}
	}
	show_buffer_duration ();
}

void
EngineControl::nperiods_changed ()
{
	DEBUG_ECONTROL ("nperiods_changed");
	show_buffer_duration ();
}

void
EngineControl::show_buffer_duration ()
{
	DEBUG_ECONTROL ("show_buffer_duration");
	/* buffer sizes  - convert from just samples to samples + msecs for
	 * the displayed string
	 */

	string bs_text = buffer_size_combo.get_active_text ();
	uint32_t samples = atoi (bs_text); /* will ignore trailing text */
	uint32_t rate = get_rate();

	/* Except for ALSA and Dummy backends, we don't know the number of periods
	 * per cycle and settings.
	 *
	 * jack1 vs jack2 have different default latencies since jack2 start
	 * in async-mode unless --sync is given which adds an extra cycle
	 * of latency. The value is not known if jackd is started externally..
	 *
	 * So just display the period size, that's also what
	 * ARDOUR_UI::update_sample_rate() does for the status bar.
	 * (the statusbar calls AudioEngine::instance()->usecs_per_cycle()
	 * but still, that's the buffer period, not [round-trip] latency)
	 */
	char buf[32];
	snprintf (buf, sizeof (buf), _("(%.1f ms)"), (samples / (rate/1000.0f)));
	buffer_size_duration_label.set_text (buf);
}

void
EngineControl::midi_option_changed ()
{
	DEBUG_ECONTROL ("midi_option_changed");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	backend->set_midi_option (get_midi_option());

	vector<ARDOUR::AudioBackend::DeviceStatus> midi_devices = backend->enumerate_midi_devices();

	_can_set_midi_latencies = backend->can_set_systemic_midi_latencies();
	std::vector<MidiDeviceSettings> new_devices;

	for (vector<ARDOUR::AudioBackend::DeviceStatus>::const_iterator i = midi_devices.begin(); i != midi_devices.end(); ++i) {
		MidiDeviceSettings mds = find_midi_device (i->name);
		if (i->available && !mds) {
			uint32_t input_latency = 0;
			uint32_t output_latency = 0;
			if (_can_set_midi_latencies) {
				input_latency = backend->systemic_midi_input_latency (i->name);
				output_latency = backend->systemic_midi_output_latency (i->name);
			}
			bool enabled = backend->midi_device_enabled (i->name);
			MidiDeviceSettings ptr (new MidiDeviceSetting (i->name, enabled, input_latency, output_latency));
			new_devices.push_back (ptr);
		} else if (i->available) {
			new_devices.push_back (mds);
		}
	}
	_midi_devices = new_devices;

	if (_midi_devices.empty()) {
		midi_devices_button.set_sensitive (false);
	} else {
		midi_devices_button.set_sensitive (true);
	}
}

void
EngineControl::latency_changed ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	if (!backend || !_have_control || !ARDOUR::AudioEngine::instance()->running ()) {
		return;
	}
	if (!backend->can_change_systemic_latency_when_running ()) {
		return;
	}
	backend->set_systemic_input_latency (get_input_latency ());
	backend->set_systemic_output_latency (get_output_latency ());
	post_push ();
}

void
EngineControl::channels_changed ()
{
}

bool
EngineControl::set_state_for_backend (const string& backend)
{
	for (StateList::const_iterator i = states.begin(); i != states.end(); ++i) {
		if ((*i)->backend != backend) {
			continue;
		}
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
		if (set_current_state (*i)) {
			//push_state_to_backend (false);
			return true;
		}
	}
	return false;
}

EngineControl::State
EngineControl::get_matching_state (
		const string& backend,
		const string& driver,
		const string& device)
{
	for (StateList::iterator i = states.begin(); i != states.end(); ++i) {
		if ((*i)->backend == backend &&
				(!_have_control || ((*i)->driver == driver && (*i)->device == device)))
		{
			return (*i);
		}
	}
	return State();
}

EngineControl::State
EngineControl::get_matching_state (
		const string& backend,
		const string& driver,
		const string& input_device,
		const string& output_device)
{
	for (StateList::iterator i = states.begin(); i != states.end(); ++i) {
		if ((*i)->backend == backend &&
				(!_have_control || ((*i)->driver == driver && ((*i)->input_device == input_device) && (*i)->output_device == output_device)))
		{
			return (*i);
		}
	}
	return State();
}

EngineControl::State
EngineControl::get_saved_state_for_currently_displayed_backend_and_device ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (backend) {
		if (backend->use_separate_input_and_output_devices ()) {
			return get_matching_state (backend_combo.get_active_text(),
					(backend->requires_driver_selection() ? (std::string) driver_combo.get_active_text() : string()),
					input_device_combo.get_active_text(),
					output_device_combo.get_active_text());
		} else {
			return get_matching_state (backend_combo.get_active_text(),
					(backend->requires_driver_selection() ? (std::string) driver_combo.get_active_text() : string()),
					device_combo.get_active_text());
		}
	}

	return get_matching_state (backend_combo.get_active_text(),
			string(),
			device_combo.get_active_text());
}

bool EngineControl::equivalent_states (const EngineControl::State& state1,
                                       const EngineControl::State& state2)
{
	if (state1->backend == state2->backend &&
			state1->driver == state2->driver &&
			state1->device == state2->device &&
			state1->input_device == state2->input_device &&
			state1->output_device == state2->output_device) {
		return true;
	}
	return false;
}

// sort active first, then most recently used to the beginning of the list
bool
EngineControl::state_sort_cmp (const State &a, const State &b) {
	if (a->active) {
		return true;
	}
	else if (b->active) {
		return false;
	}
	else {
		return a->lru > b->lru;
	}
}

EngineControl::State
EngineControl::save_state ()
{
	State state;

	if (!_have_control) {
		state = get_matching_state (backend_combo.get_active_text(), string(), string());
		if (state) {
			state->lru = time (NULL) ;
			return state;
		}
		state.reset(new StateStruct);
		state->backend = get_backend ();
	} else {
		state.reset(new StateStruct);
		store_state (state);
	}

	for (StateList::iterator i = states.begin(); i != states.end();) {
		if (equivalent_states (*i, state)) {
			i =  states.erase(i);
		} else {
			++i;
		}
	}

	states.push_back (state);

	states.sort (state_sort_cmp);

	return state;
}

void
EngineControl::store_state (State state)
{
	state->backend = get_backend ();
	state->driver = get_driver ();
	state->device = get_device_name ();
	state->input_device = get_input_device_name ();
	state->output_device = get_output_device_name ();
	state->sample_rate = get_rate ();
	state->buffer_size = get_buffer_size ();
	state->n_periods = get_nperiods ();
	state->input_latency = get_input_latency ();
	state->output_latency = get_output_latency ();
	state->input_channels = get_input_channels ();
	state->output_channels = get_output_channels ();
	state->midi_option = get_midi_option ();
	state->midi_devices = _midi_devices;
	state->use_buffered_io = get_use_buffered_io ();
}

void
EngineControl::maybe_display_saved_state ()
{
	if (!_have_control || ARDOUR::AudioEngine::instance()->running ()) {
		return;
	}

	State state = get_saved_state_for_currently_displayed_backend_and_device ();

	if (state) {
		DEBUG_ECONTROL ("Restoring saved state");
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);

		if (0 == _desired_sample_rate && sample_rate_combo.get_sensitive ()) {
			sample_rate_combo.set_active_text (rate_as_string (state->sample_rate));
		}
		set_active_text_if_present (buffer_size_combo, bufsize_as_string (state->buffer_size));

		set_active_text_if_present (nperiods_combo, to_string(state->n_periods));
		/* call this explicitly because we're ignoring changes to
		   the controls at this point.
		 */
		show_buffer_duration ();
		input_latency.set_value (state->input_latency);
		output_latency.set_value (state->output_latency);

		use_buffered_io_button.set_active (state->use_buffered_io);

		if (!state->midi_option.empty()) {
			midi_option_combo.set_active_text (state->midi_option);
			_midi_devices = state->midi_devices;
			midi_option_changed ();
		}
	} else {
		DEBUG_ECONTROL ("Unable to find saved state for backend and devices");

		boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
		if (backend) {
			input_latency.set_value (backend->systemic_hw_input_latency ());
			output_latency.set_value (backend->systemic_hw_output_latency ());
		}
	}
}

XMLNode&
EngineControl::get_state ()
{
	LocaleGuard lg;

	XMLNode* root = new XMLNode ("AudioMIDISetup");
	std::string path;

	if (!states.empty()) {
		XMLNode* state_nodes = new XMLNode ("EngineStates");

		for (StateList::const_iterator i = states.begin(); i != states.end(); ++i) {

			XMLNode* node = new XMLNode ("State");

			node->set_property ("backend", (*i)->backend);
			node->set_property ("driver", (*i)->driver);
			node->set_property ("device", (*i)->device);
			node->set_property ("input-device", (*i)->input_device);
			node->set_property ("output-device", (*i)->output_device);
			node->set_property ("sample-rate", (*i)->sample_rate);
			node->set_property ("buffer-size", (*i)->buffer_size);
			node->set_property ("n-periods", (*i)->n_periods);
			node->set_property ("input-latency", (*i)->input_latency);
			node->set_property ("output-latency", (*i)->output_latency);
			node->set_property ("input-channels", (*i)->input_channels);
			node->set_property ("output-channels", (*i)->output_channels);
			node->set_property ("lm-input", (*i)->lm_input);
			node->set_property ("lm-output", (*i)->lm_output);
			node->set_property ("active", (*i)->active);
			node->set_property ("use-buffered-io", (*i)->use_buffered_io);
			node->set_property ("midi-option", (*i)->midi_option);
			int32_t lru_val = (*i)->active ? time (NULL) : (*i)->lru;
			node->set_property ("lru", lru_val );

			XMLNode* midi_devices = new XMLNode ("MIDIDevices");
			for (std::vector<MidiDeviceSettings>::const_iterator p = (*i)->midi_devices.begin(); p != (*i)->midi_devices.end(); ++p) {
				XMLNode* midi_device_stuff = new XMLNode ("MIDIDevice");
				midi_device_stuff->set_property (X_("name"), (*p)->name);
				midi_device_stuff->set_property (X_("enabled"), (*p)->enabled);
				midi_device_stuff->set_property (X_("input-latency"), (*p)->input_latency);
				midi_device_stuff->set_property (X_("output-latency"), (*p)->output_latency);
				midi_devices->add_child_nocopy (*midi_device_stuff);
			}
			node->add_child_nocopy (*midi_devices);

			state_nodes->add_child_nocopy (*node);
		}

		root->add_child_nocopy (*state_nodes);
	}

	return *root;
}

void
EngineControl::set_default_state ()
{
	vector<string> backend_names;
	vector<const ARDOUR::AudioBackendInfo*> backends = ARDOUR::AudioEngine::instance()->available_backends();

	for (vector<const ARDOUR::AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		backend_names.push_back ((*b)->name);
	}
	backend_combo.set_active_text (backend_names.front());

	// We could set default backends per platform etc here

	backend_changed ();
}

bool
EngineControl::set_state (const XMLNode& root)
{
	XMLNodeList          clist, cclist;
	XMLNodeConstIterator citer, cciter;
	XMLNode const * child;
	XMLNode const * grandchild;

	if (root.name() != "AudioMIDISetup") {
		return false;
	}

	clist = root.children();

	states.clear ();

	for (citer = clist.begin(); citer != clist.end(); ++citer) {

		child = *citer;

		if (child->name() != "EngineStates") {
			continue;
		}

		cclist = child->children();

		for (cciter = cclist.begin(); cciter != cclist.end(); ++cciter) {
			State state (new StateStruct);

			grandchild = *cciter;

			if (grandchild->name() != "State") {
				continue;
			}

			if (!grandchild->get_property ("backend", state->backend)) {
				continue;
			}

			// If any of the required properties are not found in the state node
			// then continue/skip to the next engine state
			if (!grandchild->get_property ("driver", state->driver) ||
			    !grandchild->get_property ("device", state->device) ||
			    !grandchild->get_property ("input-device", state->input_device) ||
			    !grandchild->get_property ("output-device", state->output_device) ||
			    !grandchild->get_property ("sample-rate", state->sample_rate) ||
			    !grandchild->get_property ("buffer-size", state->buffer_size) ||
			    !grandchild->get_property ("input-latency", state->input_latency) ||
			    !grandchild->get_property ("output-latency", state->output_latency) ||
			    !grandchild->get_property ("input-channels", state->input_channels) ||
			    !grandchild->get_property ("output-channels", state->output_channels) ||
			    !grandchild->get_property ("active", state->active) ||
			    !grandchild->get_property ("use-buffered-io", state->use_buffered_io) ||
			    !grandchild->get_property ("midi-option", state->midi_option)) {
				continue;
			}

			if (!grandchild->get_property ("n-periods", state->n_periods)) {
				// optional (new value in 4.5)
				state->n_periods = 0;
			}

			if (!grandchild->get_property ("lm-input", state->lm_input) ||
			    !grandchild->get_property ("lm-output", state->lm_output)) {
				state->lm_input = "";
				state->lm_output = "";
			}

			state->midi_devices.clear();
			XMLNode* midinode;
			if ((midinode = ARDOUR::find_named_node (*grandchild, "MIDIDevices")) != 0) {
				const XMLNodeList mnc = midinode->children();
				for (XMLNodeList::const_iterator n = mnc.begin(); n != mnc.end(); ++n) {
					std::string name;
					bool enabled;
					uint32_t input_latency;
					uint32_t output_latency;

					if (!(*n)->get_property (X_("name"), name) ||
					    !(*n)->get_property (X_("enabled"), enabled) ||
					    !(*n)->get_property (X_("input-latency"), input_latency) ||
					    !(*n)->get_property (X_("output-latency"), output_latency)) {
						continue;
					}

					MidiDeviceSettings ptr (
					    new MidiDeviceSetting (name, enabled, input_latency, output_latency));
					state->midi_devices.push_back (ptr);
				}
			}

			int32_t lru_val;
			if (grandchild->get_property ("lru", lru_val)) {
				state->lru = lru_val;
			}

			states.push_back (state);
		}
	}

	/* now see if there was an active state and switch the setup to it */

	/* purge states of backend that are not available in this built */
	vector<const ARDOUR::AudioBackendInfo*> backends = ARDOUR::AudioEngine::instance()->available_backends();
	vector<std::string> backend_names;

	for (vector<const ARDOUR::AudioBackendInfo*>::const_iterator i = backends.begin(); i != backends.end(); ++i) {
		backend_names.push_back((*i)->name);
	}
	for (StateList::iterator i = states.begin(); i != states.end();) {
		if (std::find(backend_names.begin(), backend_names.end(), (*i)->backend) == backend_names.end()) {
			i = states.erase(i);
		} else {
			++i;
		}
	}

	states.sort (state_sort_cmp);

	/* purge old states referring to the same backend */
	const time_t now = time (NULL);
	for (vector<std::string>::const_iterator bi = backend_names.begin(); bi != backend_names.end(); ++bi) {
		bool first = true;
		for (StateList::iterator i = states.begin(); i != states.end();) {
			if ((*i)->backend != *bi) {
				++i; continue;
			}
			/* keep at latest one for every audio-system */
			if (first) {
				first = false;
				++i; continue;
			}

			/* keep states used in the last 2 weeks */
			if ((now - (*i)->lru) < 86400 * 14) {
				++i; continue;
			}

			/* also keep state if it was used in the last 90 days
			 * and latency was calibrated */
			if ((now - (*i)->lru) < 86400 * 90) {
				if ((*i)->input_latency != 0 || (*i)->output_latency != 0) {
					++i; continue;
				}
			}

			assert (!(*i)->active);
			i = states.erase(i);
		}
	}

	/* active was sorted first */
	for (StateList::const_iterator i = states.begin(); i != states.end(); ++i) {
		/* test if the backend & device is available */
		if (set_current_state (*i)) {
			return 0 == push_state_to_backend (false);
		}
	}
	return false;
}

bool
EngineControl::set_current_state (const State& state)
{
	DEBUG_ECONTROL ("set_current_state");

	boost::shared_ptr<ARDOUR::AudioBackend> backend;

	if (!(backend = ARDOUR::AudioEngine::instance ()->set_backend (state->backend, downcase (std::string (PROGRAM_NAME)), ""))) {
		DEBUG_ECONTROL (string_compose ("Unable to set backend to %1", state->backend));
		// this shouldn't happen as the invalid backend names should have been
		// removed from the list of states.
		return false;
	}

	// now reflect the change in the backend in the GUI so backend_changed will
	// do the right thing
	backend_combo.set_active_text (state->backend);

	if (!ARDOUR::AudioEngine::instance()->setup_required ()) {
		backend_changed ();
		// we don't have control don't restore state
		return true;
	}


	if (!state->driver.empty ()) {
		if (!backend->requires_driver_selection ()) {
			DEBUG_ECONTROL ("Backend should require driver selection");
			// A backend has changed from having driver selection to not having
			// it or someone has been manually editing a config file and messed
			// it up
			return false;
		}

		if (backend->set_driver (state->driver) != 0) {
			DEBUG_ECONTROL (string_compose ("Unable to set driver %1", state->driver));
			// Driver names for a backend have changed and the name in the
			// config file is now invalid or support for driver is no longer
			// included in the backend
			return false;
		}
		// no need to set the driver_combo as backend_changed will use
		// backend->driver_name to set the active driver
	}

	if (!state->device.empty ()) {
		if (backend->set_device_name (state->device) != 0) {
			DEBUG_ECONTROL (
			    string_compose ("Unable to set device name %1", state->device));
			// device is no longer available on the system
			return false;
		}
		// no need to set active device as it will be picked up in
		// via backend_changed ()/set_device_popdown_strings

	} else {
		// backend supports separate input/output devices
		if (backend->set_input_device_name (state->input_device) != 0) {
			DEBUG_ECONTROL (string_compose ("Unable to set input device name %1",
			                                state->input_device));
			// input device is no longer available on the system
			return false;
		}

		if (backend->set_output_device_name (state->output_device) != 0) {
			DEBUG_ECONTROL (string_compose ("Unable to set output device name %1",
			                                state->input_device));
			// output device is no longer available on the system
			return false;
		}
		// no need to set active devices as it will be picked up in via
		// backend_changed ()/set_*_device_popdown_strings
	}

	backend_changed ();

	// Now restore the state of the rest of the controls

	// We don't use a SignalBlocker as set_current_state is currently only
	// called from set_state before any signals are connected. If at some point
	// a more general named state mechanism is implemented and
	// set_current_state is called while signals are connected then a
	// SignalBlocker will need to be instantiated before setting these.

	device_combo.set_active_text (state->device);
	input_device_combo.set_active_text (state->input_device);
	output_device_combo.set_active_text (state->output_device);
	if (0 == _desired_sample_rate && sample_rate_combo.get_sensitive ()) {
		sample_rate_combo.set_active_text (rate_as_string (state->sample_rate));
	}
	set_active_text_if_present (buffer_size_combo, bufsize_as_string (state->buffer_size));
	set_active_text_if_present (nperiods_combo, to_string (state->n_periods));
	set_active_text_if_present (midi_option_combo, state->midi_option);
	input_latency.set_value (state->input_latency);
	output_latency.set_value (state->output_latency);
	use_buffered_io_button.set_active (state->use_buffered_io);

	return true;
}

int
EngineControl::push_state_to_backend (bool start)
{
	DEBUG_ECONTROL ("push_state_to_backend");
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	PBD::Unwinder<uint32_t> protect_ignore_device_changes (ignore_device_changes, ignore_device_changes + 1);

	if (!backend) {
		return 0;
	}

	/* figure out what is going to change */

	bool restart_required = false;
	bool was_running = ARDOUR::AudioEngine::instance()->running();
	bool change_driver = false;
	bool change_device = false;
	bool change_rate = false;
	bool change_bufsize = false;
	bool change_nperiods = false;
	bool change_latency = false;
	bool change_channels = false;
	bool change_midi = false;
	bool change_buffered_io = false;

	uint32_t ochan = get_output_channels ();
	uint32_t ichan = get_input_channels ();

	if (_have_control) {

		if (started_at_least_once) {

			/* we can control the backend */

			if (backend->requires_driver_selection()) {
				if (get_driver() != backend->driver_name()) {
					change_driver = true;
				}
			}

			if (backend->use_separate_input_and_output_devices()) {
				if (get_input_device_name() != backend->input_device_name()) {
					change_device = true;
				}
				if (get_output_device_name() != backend->output_device_name()) {
					change_device = true;
				}
			} else {
				if (get_device_name() != backend->device_name()) {
					change_device = true;
				}
			}

			if (queue_device_changed) {
				change_device = true;
			}

			if (get_rate() != backend->sample_rate()) {
				change_rate = true;
			}

			if (get_buffer_size() != backend->buffer_size()) {
				change_bufsize = true;
			}

			if (backend->can_set_period_size() && get_popdown_string_count (nperiods_combo) > 0
					&& get_nperiods() != backend->period_size()) {
				change_nperiods = true;
			}

			if (get_midi_option() != backend->midi_option()) {
				change_midi = true;
			}

			if (backend->can_use_buffered_io()) {
				if (get_use_buffered_io() != backend->get_use_buffered_io()) {
					change_buffered_io = true;
				}
			}

			/* zero-requested channels means "all available" */

			if (ichan == 0) {
				ichan = backend->input_channels();
			}

			if (ochan == 0) {
				ochan = backend->output_channels();
			}

			if (ichan != backend->input_channels()) {
				change_channels = true;
			}

			if (ochan != backend->output_channels()) {
				change_channels = true;
			}

			if (get_input_latency() != backend->systemic_input_latency() ||
					get_output_latency() != backend->systemic_output_latency()) {
				change_latency = true;
			}
		} else {
			/* backend never started, so we have to force a group
			   of settings.
			 */
			change_device = true;
			if (backend->requires_driver_selection()) {
				change_driver = true;
			}
			change_rate = true;
			change_bufsize = true;
			change_channels = true;
			change_latency = true;
			change_midi = true;
			change_buffered_io = backend->can_use_buffered_io();
			change_channels = true;
			change_nperiods = backend->can_set_period_size() && get_popdown_string_count (nperiods_combo) > 0;
		}

	} else {

		/* we have no control over the backend, meaning that we can
		 * only possibly change sample rate and buffer size.
		 */


		if (get_rate() != backend->sample_rate()) {
			change_bufsize = true;
		}

		if (get_buffer_size() != backend->buffer_size()) {
			change_bufsize = true;
		}
	}

	queue_device_changed = false;

	if (!_have_control) {

		/* We do not have control over the backend, so the best we can
		 * do is try to change the sample rate and/or bufsize and get
		 * out of here.
		 */

		if (change_rate && !backend->can_change_sample_rate_when_running()) {
			return 1;
		}

		if (change_bufsize && !backend->can_change_buffer_size_when_running()) {
			return 1;
		}

		if (change_rate) {
			backend->set_sample_rate (get_rate());
		}

		if (change_bufsize) {
			backend->set_buffer_size (get_buffer_size());
		}

		if (start) {
			if (ARDOUR::AudioEngine::instance()->start ()) {
				error << string_compose (_("Could not start backend engine %1"), backend->name()) << endmsg;
				return -1;
			}
		}

		post_push ();

		return 0;
	}

	/* determine if we need to stop the backend before changing parameters */

	if (change_driver || change_device || change_channels || change_nperiods ||
			(change_latency && !backend->can_change_systemic_latency_when_running ()) ||
			(change_rate && !backend->can_change_sample_rate_when_running()) ||
			change_midi || change_buffered_io ||
			(change_bufsize && !backend->can_change_buffer_size_when_running())) {
		restart_required = true;
	} else {
		restart_required = false;
	}


	if (was_running) {
		if (restart_required) {
			if (ARDOUR::AudioEngine::instance()->stop()) {
				return 1;
			}
		}
	}

	if (change_driver && backend->set_driver (get_driver())) {
		error << string_compose (_("Cannot set driver to %1"), get_driver()) << endmsg;
		return 1;
	}
	if (backend->use_separate_input_and_output_devices()) {
		if (change_device && backend->set_input_device_name (get_input_device_name())) {
			error << string_compose (_("Cannot set input device name to %1"), get_input_device_name()) << endmsg;
			return 1;
		}
		if (change_device && backend->set_output_device_name (get_output_device_name())) {
			error << string_compose (_("Cannot set output device name to %1"), get_output_device_name()) << endmsg;
			return 1;
		}
	} else {
		if (change_device && backend->set_device_name (get_device_name())) {
			error << string_compose (_("Cannot set device name to %1"), get_device_name()) << endmsg;
			return 1;
		}
	}
	if (change_rate && backend->set_sample_rate (get_rate())) {
		error << string_compose (_("Cannot set sample rate to %1"), get_rate()) << endmsg;
		return 1;
	}
	if (change_bufsize && backend->set_buffer_size (get_buffer_size())) {
		error << string_compose (_("Cannot set buffer size to %1"), get_buffer_size()) << endmsg;
		return 1;
	}
	if (change_nperiods && backend->set_peridod_size (get_nperiods())) {
		error << string_compose (_("Cannot set periods to %1"), get_nperiods()) << endmsg;
		return 1;
	}

	if (change_channels || get_input_channels() == 0 || get_output_channels() == 0) {
		if (backend->set_input_channels (get_input_channels())) {
			error << string_compose (_("Cannot set input channels to %1"), get_input_channels()) << endmsg;
			return 1;
		}
		if (backend->set_output_channels (get_output_channels())) {
			error << string_compose (_("Cannot set output channels to %1"), get_output_channels()) << endmsg;
			return 1;
		}
	}
	if (change_latency) {
		if (backend->set_systemic_input_latency (get_input_latency())) {
			error << string_compose (_("Cannot set input latency to %1"), get_input_latency()) << endmsg;
			return 1;
		}
		if (backend->set_systemic_output_latency (get_output_latency())) {
			error << string_compose (_("Cannot set output latency to %1"), get_output_latency()) << endmsg;
			return 1;
		}
	}

	if (change_midi) {
		backend->set_midi_option (get_midi_option());
	}

	if (change_buffered_io) {
		backend->set_use_buffered_io (use_buffered_io_button.get_active());
	}

	if (1 /* TODO */) {
		for (vector<MidiDeviceSettings>::const_iterator p = _midi_devices.begin(); p != _midi_devices.end(); ++p) {
			if (_measure_midi) {
				/* Disable other MIDI devices while measuring.
				 * This is a hack to only show ports from the selected device */
				if (*p == _measure_midi) {
					backend->set_midi_device_enabled ((*p)->name, true);
				} else {
					backend->set_midi_device_enabled ((*p)->name, false);
				}
				continue;
			}
			backend->set_midi_device_enabled ((*p)->name, (*p)->enabled);
			if (backend->can_set_systemic_midi_latencies()) {
				backend->set_systemic_midi_input_latency ((*p)->name, (*p)->input_latency);
				backend->set_systemic_midi_output_latency ((*p)->name, (*p)->output_latency);
			}
		}
	}

	if (start || (was_running && restart_required)) {
		if (ARDOUR::AudioEngine::instance()->start()) {
			return -1;
		}
	}

	post_push ();

	return 0;
}

void
EngineControl::post_push ()
{
	/* get a pointer to the current state object, creating one if
	 * necessary
	 */

	State state = get_saved_state_for_currently_displayed_backend_and_device ();

	if (!state) {
		state = save_state ();
		assert (state);
	} else {
		store_state(state);
	}

	if (ARDOUR::AudioEngine::instance()->running()) {
		/* all off */
		for (StateList::iterator i = states.begin(); i != states.end(); ++i) {
			(*i)->active = false;
		}

		/* mark this one active (to be used next time the dialog is shown) */
		state->active = true;
		state->lru = time (NULL) ;
	}

	states.sort (state_sort_cmp);

	if (_have_control) { // XXX
		manage_control_app_sensitivity ();
	}

	/* schedule a redisplay of MIDI ports */
	//Glib::signal_timeout().connect (sigc::bind_return (sigc::mem_fun (*this, &EngineControl::refresh_midi_display), false), 1000);
}


float
EngineControl::get_rate () const
{
	float r = atof (sample_rate_combo.get_active_text ());
	/* the string may have been translated with an abbreviation for
	 * thousands, so use a crude heuristic to fix this.
	 */
	if (r < 1000.0) {
		r *= 1000.0;
	}
	return r;
}


uint32_t
EngineControl::get_buffer_size () const
{
	string txt = buffer_size_combo.get_active_text ();
	uint32_t samples;

	if (sscanf (txt.c_str(), "%d", &samples) != 1) {
		fprintf(stderr, "Find a trout and repeatedly slap the nearest C++ who throws exceptions without catching them.\n");
		fprintf(stderr, "Ardour will likely crash now, giving you time to get the trout.\n");
		throw exception ();
	}

	return samples;
}

uint32_t
EngineControl::get_nperiods () const
{
	string txt = nperiods_combo.get_active_text ();
	return atoi (txt.c_str());
}

string
EngineControl::get_midi_option () const
{
	return midi_option_combo.get_active_text();
}

bool
EngineControl::get_use_buffered_io () const
{
	return use_buffered_io_button.get_active();
}

uint32_t
EngineControl::get_input_channels() const
{
	if (ARDOUR::Profile->get_mixbus()) {
		boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
		if (!backend) return 0;
		return backend->input_channels();
	}
	return (uint32_t) input_channels_adjustment.get_value();
}

uint32_t
EngineControl::get_output_channels() const
{
	if (ARDOUR::Profile->get_mixbus()) {
		boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
		if (!backend) return 0;
		return backend->input_channels();
	}
	return (uint32_t) output_channels_adjustment.get_value();
}

uint32_t
EngineControl::get_input_latency() const
{
	return (uint32_t) input_latency_adjustment.get_value();
}

uint32_t
EngineControl::get_output_latency() const
{
	return (uint32_t) output_latency_adjustment.get_value();
}

string
EngineControl::get_backend () const
{
	return backend_combo.get_active_text ();
}

string
EngineControl::get_driver () const
{
	if (driver_combo.get_parent()) {
		return driver_combo.get_active_text ();
	} else {
		return "";
	}
}

string
EngineControl::get_device_name () const
{
	return device_combo.get_active_text ();
}

string
EngineControl::get_input_device_name () const
{
	return input_device_combo.get_active_text ();
}

string
EngineControl::get_output_device_name () const
{
	return output_device_combo.get_active_text ();
}

void
EngineControl::control_app_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (!backend) {
		return;
	}

	backend->launch_control_app ();
}

void
EngineControl::on_response (int r)
{
	/* Do not run ArdourDialog::on_response() which will hide us. Leave
	 * that to whoever invoked us, if they wish to hide us after "start".
	 *
	 * StartupFSM does hide us after response(); Window > Audio/MIDI Setup
	 * does not.
	 */
	pop_splash ();
	Gtk::Dialog::on_response (r);
}

void
EngineControl::start_stop_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (!backend) {
		return;
	}

	if (ARDOUR::AudioEngine::instance()->running()) {
		ARDOUR::AudioEngine::instance()->stop ();
	} else {
		/* whoever displayed this dialog is expected to do its own
		   check on whether or not the engine is running.
		*/
		start_engine ();
	}

	response (RESPONSE_OK);
}

void
EngineControl::update_devices_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (!backend) {
		return;
	}
	assert (!ARDOUR::AudioEngine::instance()->running());

	if (backend->update_devices()) {
		device_list_changed ();
		if (set_state_for_backend (backend_combo.get_active_text ())) {
			maybe_display_saved_state ();
		}
	}
}

void
EngineControl::try_autostart_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	ARDOUR::Config->set_try_autostart_engine (!try_autostart_button.get_active ());
	try_autostart_button.set_active (ARDOUR::Config->get_try_autostart_engine ());
}

void
EngineControl::use_buffered_io_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (!backend) {
		return;
	}

	bool set_buffered_io = !use_buffered_io_button.get_active();
	use_buffered_io_button.set_active (set_buffered_io);
	backend->set_use_buffered_io (set_buffered_io);
}

void
EngineControl::manage_control_app_sensitivity ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (!backend) {
		return;
	}

	string appname = backend->control_app_name();

	if (appname.empty()) {
		control_app_button.set_sensitive (false);
	} else {
		control_app_button.set_sensitive (true);
	}

	lm_button_audio.set_sensitive (backend->can_measure_systemic_latency ());
}

void
EngineControl::set_desired_sample_rate (uint32_t sr)
{
	_desired_sample_rate = sr;

	if (ARDOUR::AudioEngine::instance ()->running () && ARDOUR::AudioEngine::instance ()->sample_rate () != sr) {
		stop_engine ();
	}

	device_changed ();
}

void
EngineControl::on_switch_page (GtkNotebookPage*, guint page_num)
{
	if (ignore_changes) {
		return;
	}
	if (page_num == 0) {
		_measure_midi.reset();
		update_sensitivity ();
	}

	if (page_num == midi_tab) {
		/* MIDI tab */
		refresh_midi_display ();

		/* undo special case from push_state_to_backend() when measuring midi latency */
		if (_measure_midi && ARDOUR::AudioEngine::instance()->running ()) {
			boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
			if (backend->can_change_systemic_latency_when_running ()) {
				for (vector<MidiDeviceSettings>::const_iterator p = _midi_devices.begin(); p != _midi_devices.end(); ++p) {
					backend->set_midi_device_enabled ((*p)->name, (*p)->enabled);
				}
			}
		}
		_measure_midi.reset();
	}

	if (page_num == latency_tab) {
		/* latency tab */

		was_running_before_lm = ARDOUR::AudioEngine::instance()->running();

		if (ARDOUR::AudioEngine::instance()->running()) {
			stop_engine (true);
		}

		{
			PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);

			/* save any existing latency values */

			uint32_t il = (uint32_t) input_latency.get_value ();
			uint32_t ol = (uint32_t) output_latency.get_value ();

			/* reset to zero so that our new test instance
			   will be clean of any existing latency measures.

			   NB. this should really be done by the backend
			   when stated for latency measurement.
			*/

			input_latency.set_value (0);
			output_latency.set_value (0);

			push_state_to_backend (false);

			/* reset control */

			input_latency.set_value (il);
			output_latency.set_value (ol);

		}
		// This should be done in push_state_to_backend()
		if (ARDOUR::AudioEngine::instance()->prepare_for_latency_measurement()) {
			disable_latency_tab ();
		}

		enable_latency_tab ();

	} else {
		if (lm_running) {
			end_latency_detection ();
		}
	}
}

/* latency measurement */

bool
EngineControl::check_audio_latency_measurement ()
{
	MTDM* mtdm = ARDOUR::AudioEngine::instance()->mtdm ();

	if (mtdm->resolve () < 0) {
		lm_results.set_markup (string_compose (results_markup, _("No signal detected ")));
		return true;
	}

	if (mtdm->get_peak () > 0.707f) {
		// get_peak() resets the peak-hold in the detector.
		// this GUI callback is at 10Hz and so will be fine (test-signal is at higher freq)
		lm_results.set_markup (string_compose (results_markup, _("Input signal is > -3dBFS. Lower the signal level (output gain, input gain) on the audio-interface.")));
		return true;
	}

	if (mtdm->err () > 0.3) {
		mtdm->invert ();
		mtdm->resolve ();
	}

	char buf[256];
	ARDOUR::samplecnt_t const sample_rate = ARDOUR::AudioEngine::instance()->sample_rate();

	if (sample_rate == 0) {
		lm_results.set_markup (string_compose (results_markup, _("Disconnected from audio engine")));
		ARDOUR::AudioEngine::instance()->stop_latency_detection ();
		return false;
	}

	int samples_total = mtdm->del();
	int extra = samples_total - ARDOUR::AudioEngine::instance()->latency_signal_delay();

	snprintf (buf, sizeof (buf), "%s%d samples (%.3lf ms)\n%s%d samples (%.3lf ms)",
			_("Detected roundtrip latency: "),
			samples_total, samples_total * 1000.0f/sample_rate,
			_("Systemic latency: "),
			extra, extra * 1000.0f/sample_rate);

	bool solid = true;

	if (mtdm->err () > 0.2) {
		strcat (buf, " ");
		strcat (buf, _("(signal detection error)"));
		solid = false;
	}

	if (mtdm->inv ()) {
		/* only warn user, in some cases the measured value is correct,
		 * regardless of the warning - https://github.com/Ardour/ardour/pull/656
		 */
		strcat (buf, " ");
		strcat (buf, _("(inverted - bad wiring)"));
	}

	lm_results.set_markup (string_compose (results_markup, buf));

	if (solid) {
		have_lm_results = true;
		end_latency_detection ();
		lm_use_button.set_sensitive (true);
		return false;
	}

	return true;
}

bool
EngineControl::check_midi_latency_measurement ()
{
	ARDOUR::MIDIDM* mididm = ARDOUR::AudioEngine::instance()->mididm ();

	if (!mididm->have_signal () || mididm->latency () == 0) {
		lm_results.set_markup (string_compose (results_markup, _("No signal detected ")));
		return true;
	}

	char buf[256];
	ARDOUR::samplecnt_t const sample_rate = ARDOUR::AudioEngine::instance()->sample_rate();

	if (sample_rate == 0) {
		lm_results.set_markup (string_compose (results_markup, _("Disconnected from audio engine")));
		ARDOUR::AudioEngine::instance()->stop_latency_detection ();
		return false;
	}

	ARDOUR::samplecnt_t samples_total = mididm->latency();
	ARDOUR::samplecnt_t extra = samples_total - ARDOUR::AudioEngine::instance()->latency_signal_delay();
	snprintf (buf, sizeof (buf), "%s%" PRId64" samples (%.1lf ms) dev: %.2f[spl]\n%s%" PRId64" samples (%.1lf ms)",
			_("Detected roundtrip latency: "),
			samples_total, samples_total * 1000.0f / sample_rate, mididm->deviation (),
			_("Systemic latency: "),
			extra, extra * 1000.0f / sample_rate);

	bool solid = true;

	if (!mididm->ok ()) {
		strcat (buf, " ");
		strcat (buf, _("(averaging)"));
		solid = false;
	}

	if (mididm->deviation () > 50.0) {
		strcat (buf, " ");
		strcat (buf, _("(too large jitter)"));
		solid = false;
	} else if (mididm->deviation () > 10.0) {
		strcat (buf, " ");
		strcat (buf, _("(large jitter)"));
	}

	if (solid) {
		have_lm_results = true;
		end_latency_detection ();
		lm_use_button.set_sensitive (true);
		lm_results.set_markup (string_compose (results_markup, buf));
		return false;
	} else if (mididm->processed () > 400) {
		have_lm_results = false;
		end_latency_detection ();
		lm_results.set_markup (string_compose (results_markup, _("Timeout - large MIDI jitter.")));
		return false;
	}

	lm_results.set_markup (string_compose (results_markup, buf));

	return true;
}

void
EngineControl::start_latency_detection ()
{
	ARDOUR::AudioEngine::instance()->set_latency_input_port (lm_input_channel_combo.get_active ()->get_value (lm_input_channel_cols.port_name));
	ARDOUR::AudioEngine::instance()->set_latency_output_port (lm_output_channel_combo.get_active ()->get_value (lm_output_channel_cols.port_name));

	if (ARDOUR::AudioEngine::instance()->start_latency_detection (_measure_midi ? true : false) == 0) {
		lm_results.set_markup (string_compose (results_markup, _("Detecting ...")));
		if (_measure_midi) {
			latency_timeout = Glib::signal_timeout().connect (mem_fun (*this, &EngineControl::check_midi_latency_measurement), 100);
		} else {
			latency_timeout = Glib::signal_timeout().connect (mem_fun (*this, &EngineControl::check_audio_latency_measurement), 100);
		}
		lm_measure_label.set_text (_("Cancel"));
		have_lm_results = false;
		lm_use_button.set_sensitive (false);
		lm_input_channel_combo.set_sensitive (false);
		lm_output_channel_combo.set_sensitive (false);
		lm_running = true;
	}
}

void
EngineControl::end_latency_detection ()
{
	latency_timeout.disconnect ();
	ARDOUR::AudioEngine::instance()->stop_latency_detection ();
	lm_measure_label.set_text (_("Measure"));
	if (!have_lm_results) {
		lm_use_button.set_sensitive (false);
	}
	lm_input_channel_combo.set_sensitive (true);
	lm_output_channel_combo.set_sensitive (true);
	lm_running = false;
}

void
EngineControl::latency_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	if (!lm_running) {
		start_latency_detection ();
	} else {
		end_latency_detection ();
	}
}

void
EngineControl::latency_back_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	ARDOUR::AudioEngine::instance()->stop_latency_detection ();
	notebook.set_current_page(0);

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	if (backend && backend->can_change_systemic_latency_when_running ()) {
		/* IFF engine was not running before latency detection, stop it */
		if (!was_running_before_lm && ARDOUR::AudioEngine::instance()->running()) {
			stop_engine ();
		}
	}
}

void
EngineControl::use_latency_button_clicked ()
{
	if (!_sensitive) {
		return;
	}

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	if (_measure_midi) {
		ARDOUR::MIDIDM* mididm = ARDOUR::AudioEngine::instance()->mididm ();
		if (!mididm) {
			return;
		}
		ARDOUR::samplecnt_t samples_total = mididm->latency();
		ARDOUR::samplecnt_t extra = samples_total - ARDOUR::AudioEngine::instance()->latency_signal_delay();
		uint32_t one_way = max ((ARDOUR::samplecnt_t) 0, extra / 2);
		_measure_midi->input_latency = one_way;
		_measure_midi->output_latency = one_way;
		if (backend->can_change_systemic_latency_when_running ()) {
			backend->set_systemic_midi_input_latency (_measure_midi->name, one_way);
			backend->set_systemic_midi_output_latency (_measure_midi->name, one_way);
		}
		notebook.set_current_page (midi_tab);
	} else {
		MTDM* mtdm = ARDOUR::AudioEngine::instance()->mtdm ();

		if (!mtdm) {
			return;
		}

		double one_way = rint ((mtdm->del() - ARDOUR::AudioEngine::instance()->latency_signal_delay()) / 2.0);
		one_way = std::max (0., one_way);

		State state = get_saved_state_for_currently_displayed_backend_and_device ();
		if (state) {
			state->lm_input = lm_input_channel_combo.get_active ()->get_value (lm_input_channel_cols.port_name);
			state->lm_output = lm_output_channel_combo.get_active ()->get_value (lm_output_channel_cols.port_name);
			post_push (); /* save */
		}

		/* these trigger EngineControl::latency_changed, and a post_push()
		 * when the latency can be changed while running */
		input_latency_adjustment.set_value (one_way);
		output_latency_adjustment.set_value (one_way);

		if (backend->can_change_systemic_latency_when_running ()) {
			/* engine is running, continue to load session.
			 * RESPONSE_OK is a NO-OP when the dialog is displayed as Window
			 * from a running instance.
			 */
			notebook.set_current_page (0);
			response (RESPONSE_OK);
			return;
		}

		/* back to settings page */
		notebook.set_current_page (0);
	}
}

bool
EngineControl::on_delete_event (GdkEventAny* ev)
{
	if (lm_running || notebook.get_current_page() == 2) {
		/* currently measuring latency - be sure to clean up */
		end_latency_detection ();
	}

	return ArdourDialog::on_delete_event (ev);
}

void
EngineControl::engine_running ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	set_active_text_if_present (buffer_size_combo, bufsize_as_string (backend->buffer_size()));
	sample_rate_combo.set_active_text (rate_as_string (backend->sample_rate()));

	if (backend->can_set_period_size ()) {
		set_active_text_if_present (nperiods_combo, to_string (backend->period_size()));
	}

	connect_disconnect_button.set_label (string_compose (_("Disconnect from %1"), backend->name()));
	connect_disconnect_button.show();

	started_at_least_once = true;
	if (_have_control) {
		engine_status.set_markup(string_compose ("<span foreground=\"green\">%1</span>", _("Running")));
	} else {
		engine_status.set_markup(string_compose ("<span foreground=\"green\">%1</span>", _("Connected")));
	}
	update_sensitivity();
}

void
EngineControl::engine_stopped ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	connect_disconnect_button.set_label (string_compose (_("Connect to %1"), backend->name()));
	connect_disconnect_button.show();

	if (_have_control) {
		engine_status.set_markup(string_compose ("<span foreground=\"red\">%1</span>", _("Stopped")));
	} else {
		engine_status.set_markup(X_(""));
	}

	update_sensitivity();
}

void
EngineControl::device_list_changed ()
{
	if (ignore_device_changes) {
		return;
	}
	PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1); // ??
	if (!ARDOUR::AudioEngine::instance()->running()) {
		list_devices ();
	}

	midi_option_changed();

	if (notebook.get_current_page() == midi_tab) {
		if (_midi_devices.empty ()) {
			notebook.set_current_page (0);
		} else {
			refresh_midi_display ();
		}
	}
}

void
EngineControl::connect_disconnect_click()
{
	if (ARDOUR::AudioEngine::instance()->running()) {
		stop_engine ();
	} else {
		if (!ARDOUR_UI::instance()->the_session ()) {
			pop_splash ();
			hide ();
			ARDOUR::GUIIdle ();
		}
		start_engine ();
		if (!ARDOUR_UI::instance()->the_session ()) {
			ArdourDialog::response (RESPONSE_OK);
		}
	}
}

void
EngineControl::calibrate_audio_latency ()
{
	_measure_midi.reset ();
	have_lm_results = false;
	lm_use_button.set_sensitive (false);
	lm_results.set_markup (string_compose (results_markup, _("No measurement results yet")));
	notebook.set_current_page (latency_tab);
}

void
EngineControl::calibrate_midi_latency (MidiDeviceSettings s)
{
	_measure_midi = s;
	have_lm_results = false;
	lm_use_button.set_sensitive (false);
	lm_results.set_markup (string_compose (results_markup, _("No measurement results yet")));
	notebook.set_current_page (latency_tab);
}

void
EngineControl::configure_midi_devices ()
{
	notebook.set_current_page (midi_tab);
}
