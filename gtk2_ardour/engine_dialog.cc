/*
    Copyright (C) 2010 Paul Davis

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

#include <exception>
#include <vector>
#include <cmath>
#include <fstream>
#include <map>

#include <boost/scoped_ptr.hpp>

#include <gtkmm/messagedialog.h>

#include "pbd/error.h"
#include "pbd/xml++.h"
#include "pbd/unwind.h"
#include "pbd/failed_constructor.h"

#include <gtkmm/alignment.h>
#include <gtkmm/stock.h>
#include <gtkmm/notebook.h>
#include <gtkmm2ext/utils.h>

#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/mtdm.h"
#include "ardour/mididm.h"
#include "ardour/rc_configuration.h"
#include "ardour/types.h"
#include "ardour/profile.h"

#include "pbd/convert.h"
#include "pbd/error.h"

#include "ardour_ui.h"
#include "engine_dialog.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;
using namespace ARDOUR_UI_UTILS;

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
	, lm_measure_label (_("Measure"))
	, lm_use_button (_("Use results"))
	, lm_back_button (_("Back to settings ... (ignore results)"))
	, lm_button_audio (_("Calibrate Audio"))
	, lm_table (12, 3)
	, have_lm_results (false)
	, lm_running (false)
	, midi_back_button (_("Back to settings"))
	, ignore_changes (0)
	, _desired_sample_rate (0)
	, started_at_least_once (false)
	, queue_device_changed (false)
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
		MessageDialog msg (string_compose (_("No audio/MIDI backends detected. %1 cannot run\n\n(This is a build/packaging/system error. It should never happen.)"), PROGRAM_NAME));
		msg.run ();
		throw failed_constructor ();
	}

	for (vector<const ARDOUR::AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		backend_names.push_back ((*b)->name);
	}

	set_popdown_strings (backend_combo, backend_names);
	backend_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::backend_changed));

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

	label = manage (new Label (_("Output channel")));
	lm_table.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);

	Gtk::Alignment* misc_align = manage (new Alignment (0.0, 0.5));
	misc_align->add (lm_output_channel_combo);
	lm_table.attach (*misc_align, 1, 3, row, row+1, xopt, (AttachOptions) 0);
	++row;

	label = manage (new Label (_("Input channel")));
	lm_table.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);

	misc_align = manage (new Alignment (0.0, 0.5));
	misc_align->add (lm_input_channel_combo);
	lm_table.attach (*misc_align, 1, 3, row, row+1, FILL, (AttachOptions) 0);
	++row;

	xopt = AttachOptions(0);

	lm_measure_label.set_padding (10, 10);
	lm_measure_button.add (lm_measure_label);
	lm_measure_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::latency_button_clicked));
	lm_use_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::use_latency_button_clicked));
	lm_back_button_signal = lm_back_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (notebook, &Gtk::Notebook::set_current_page), 0));

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

	get_action_area()->pack_start (engine_status);
	engine_status.show();

	/* need a special function to print "all available channels" when the
	 * channel counts hit zero.
	 */

	input_channels.signal_output().connect (sigc::bind (sigc::ptr_fun (&EngineControl::print_channel_count), &input_channels));
	output_channels.signal_output().connect (sigc::bind (sigc::ptr_fun (&EngineControl::print_channel_count), &output_channels));

	midi_devices_button.signal_clicked.connect (mem_fun (*this, &EngineControl::configure_midi_devices));
	midi_devices_button.set_sensitive (false);
	midi_devices_button.set_name ("generic button");
	midi_devices_button.set_can_focus(true);

	control_app_button.signal_clicked().connect (mem_fun (*this, &EngineControl::control_app_button_clicked));
	manage_control_app_sensitivity ();

	cancel_button = add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_CANCEL);
	apply_button = add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_APPLY);
	ok_button = add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

	/* Pick up any existing audio setup configuration, if appropriate */

	XMLNode* audio_setup = ARDOUR::Config->extra_xml ("AudioMIDISetup");

	ARDOUR::AudioEngine::instance()->Running.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::engine_running, this), gui_context());
	ARDOUR::AudioEngine::instance()->Stopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::engine_stopped, this), gui_context());
	ARDOUR::AudioEngine::instance()->Halted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::engine_stopped, this), gui_context());
	ARDOUR::AudioEngine::instance()->DeviceListChanged.connect (devicelist_connection, MISSING_INVALIDATOR, boost::bind (&EngineControl::device_list_changed, this), gui_context());

	if (audio_setup) {
		set_state (*audio_setup);
	}

	if (backend_combo.get_active_text().empty()) {
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
		backend_combo.set_active_text (backend_names.front());
	}

	backend_changed ();

	/* in case the setting the backend failed, e.g. stale config, from set_state(), try again */
	if (0 == ARDOUR::AudioEngine::instance()->current_backend()) {
		backend_combo.set_active_text (backend_names.back());
		/* ignore: don't save state */
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
		backend_changed ();
	}


	/* Connect to signals */

	driver_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::driver_changed));
	sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::sample_rate_changed));
	buffer_size_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::buffer_size_changed));
	device_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::device_changed));
	midi_option_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::midi_option_changed));

	input_latency.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::parameter_changed));
	output_latency.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::parameter_changed));
	input_channels.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::parameter_changed));
	output_channels.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::parameter_changed));

	notebook.signal_switch_page().connect (sigc::mem_fun (*this, &EngineControl::on_switch_page));

	connect_disconnect_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::connect_disconnect_click));
	connect_disconnect_button.set_no_show_all();

}

void
EngineControl::on_show ()
{
	ArdourDialog::on_show ();
	device_changed ();
	ok_button->grab_focus();
}

void
EngineControl::on_response (int response_id)
{
	ArdourDialog::on_response (response_id);

	switch (response_id) {
		case RESPONSE_APPLY:
			push_state_to_backend (true);
			break;
		case RESPONSE_OK:
			push_state_to_backend (true);
			hide ();
			break;
		case RESPONSE_DELETE_EVENT:
			{
				GdkEventButton ev;
				ev.type = GDK_BUTTON_PRESS;
				ev.button = 1;
				on_delete_event ((GdkEventAny*) &ev);
				break;
			}
		default:
			hide ();
	}
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

	/* start packing it up */

	if (backend->requires_driver_selection()) {
		label = manage (left_aligned_label (_("Driver:")));
		basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
		basic_packer.attach (driver_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
		row++;
	}

	label = manage (left_aligned_label (_("Device:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (device_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
	row++;

	label = manage (left_aligned_label (_("Sample rate:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
	row++;


	label = manage (left_aligned_label (_("Buffer size:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, xopt, (AttachOptions) 0);
	basic_packer.attach (buffer_size_combo, 1, 2, row, row + 1, xopt, (AttachOptions) 0);
	buffer_size_duration_label.set_alignment (0.0); /* left-align */
	basic_packer.attach (buffer_size_duration_label, 2, 3, row, row+1, SHRINK, (AttachOptions) 0);

	/* button spans 2 rows */

	basic_packer.attach (control_app_button, 3, 4, row-1, row+1, xopt, xopt);
	row++;

	input_channels.set_name ("InputChannels");
	input_channels.set_flags (Gtk::CAN_FOCUS);
	input_channels.set_digits (0);
	input_channels.set_wrap (false);
	output_channels.set_editable (true);

	if (!ARDOUR::Profile->get_mixbus()) {
		label = manage (left_aligned_label (_("Input Channels:")));
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
		label = manage (left_aligned_label (_("Output Channels:")));
		basic_packer.attach (*label, 0, 1, row, row+1, xopt, (AttachOptions) 0);
		basic_packer.attach (output_channels, 1, 2, row, row+1, xopt, (AttachOptions) 0);
		++row;
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
	basic_packer.attach (midi_option_combo, 1, 2, row, row + 1, SHRINK, (AttachOptions) 0);
	basic_packer.attach (midi_devices_button, 3, 4, row, row+1, xopt, xopt);
	row++;
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
	const string msg = string_compose (_("The %1 audio backend was configured and started externally.\nThis limits your control over it."), backend->name());

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
	vector<string> empty;
	set_popdown_strings (lm_output_channel_combo, empty);
	set_popdown_strings (lm_input_channel_combo, empty);
	lm_measure_button.set_sensitive (false);
	lm_use_button.set_sensitive (false);
}

void
EngineControl::enable_latency_tab ()
{
	vector<string> outputs;
	vector<string> inputs;

	ARDOUR::DataType const type = _measure_midi ? ARDOUR::DataType::MIDI : ARDOUR::DataType::AUDIO;
	ARDOUR::AudioEngine::instance()->get_physical_outputs (type, outputs);
	ARDOUR::AudioEngine::instance()->get_physical_inputs (type, inputs);

	if (!ARDOUR::AudioEngine::instance()->running()) {
		MessageDialog msg (_("Failed to start or connect to audio-engine.\n\nLatency calibration requires a working audio interface."));
		notebook.set_current_page (0);
		msg.run ();
		return;
	}
	else if (inputs.empty() || outputs.empty()) {
		MessageDialog msg (_("Your selected audio configuration is playback- or capture-only.\n\nLatency calibration requires playback and capture"));
		notebook.set_current_page (0);
		msg.run ();
		return;
	}

	lm_back_button_signal.disconnect();
	if (_measure_midi) {
		lm_back_button_signal = lm_back_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (notebook, &Gtk::Notebook::set_current_page), midi_tab));
		lm_preamble.hide ();
	} else {
		lm_back_button_signal = lm_back_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (notebook, &Gtk::Notebook::set_current_page), 0));
		lm_preamble.show ();
	}

	set_popdown_strings (lm_output_channel_combo, outputs);
	lm_output_channel_combo.set_active_text (outputs.front());
	lm_output_channel_combo.set_sensitive (true);

	set_popdown_strings (lm_input_channel_combo, inputs);
	lm_input_channel_combo.set_active_text (inputs.front());
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
}

void
EngineControl::midi_device_enabled_toggled (ArdourButton *b, MidiDeviceSettings device) {
	b->set_active (!b->get_active());
	device->enabled = b->get_active();
	refresh_midi_display(device->name);
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
	l = manage (new Label (_("Hardware Latencies"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 1, 3, row, row + 1, xopt, AttachOptions (0));
	row++;
	l = manage (new Label (_("Input"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 1, 2, row, row + 1, xopt, AttachOptions (0));
	l = manage (new Label (_("Output"))); l->show (); l->set_alignment (0.5, 0.5);
	midi_device_table.attach (*l, 2, 3, row, row + 1, xopt, AttachOptions (0));
	row++;

	for (vector<MidiDeviceSettings>::const_iterator p = _midi_devices.begin(); p != _midi_devices.end(); ++p) {
		ArdourButton *m;
		Gtk::Button* b;
		Gtk::Adjustment *a;
		Gtk::SpinButton *s;
		bool enabled = (*p)->enabled;

		m = manage (new ArdourButton ((*p)->name, ArdourButton::led_default_elements));
		m->set_name ("midi device");
		m->set_can_focus (Gtk::CAN_FOCUS);
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
		b->set_sensitive (_can_set_midi_latencies && enabled);
		midi_device_table.attach (*b, 3, 4, row, row + 1, xopt, AttachOptions (0)); b->show ();

		row++;
	}
}

void
EngineControl::update_sensitivity ()
{
}

void
EngineControl::backend_changed ()
{
	string backend_name = backend_combo.get_active_text();
	boost::shared_ptr<ARDOUR::AudioBackend> backend;

	if (!(backend = ARDOUR::AudioEngine::instance()->set_backend (backend_name, "ardour", ""))) {
		/* eh? setting the backend failed... how ? */
		/* A: stale config contains a backend that does not exist in current build */
		return;
	}

	_have_control = ARDOUR::AudioEngine::instance()->setup_required ();

	build_notebook ();
	setup_midi_tab_for_backend ();
	_midi_devices.clear();

	if (backend->requires_driver_selection()) {
		vector<string> drivers = backend->enumerate_drivers();
		driver_combo.set_sensitive (true);

		if (!drivers.empty()) {
			{
				string current_driver;
				current_driver = backend->driver_name ();

				// driver might not have been set yet
				if (current_driver == "") {
					current_driver = driver_combo.get_active_text ();
					if (current_driver == "")
						// driver has never been set, make sure it's not blank
						current_driver = drivers.front ();
				}

				PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
				set_popdown_strings (driver_combo, drivers);
				driver_combo.set_active_text (current_driver);
			}

			driver_changed ();
		}

	} else {
		driver_combo.set_sensitive (false);
		/* this will change the device text which will cause a call to
		 * device changed which will set up parameters
		 */
		list_devices ();
	}

	vector<string> midi_options = backend->enumerate_midi_options();

	if (midi_options.size() == 1) {
		/* only contains the "none" option */
		midi_option_combo.set_sensitive (false);
	} else {
		if (_have_control) {
			set_popdown_strings (midi_option_combo, midi_options);
			midi_option_combo.set_active_text (midi_options.front());
			midi_option_combo.set_sensitive (true);
		} else {
			midi_option_combo.set_sensitive (false);
		}
	}

	connect_disconnect_button.hide();

	midi_option_changed();

	started_at_least_once = false;

	if (!ignore_changes) {
		maybe_display_saved_state ();
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

void
EngineControl::list_devices ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	/* now fill out devices, mark sample rates, buffer sizes insensitive */

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

	if (!available_devices.empty()) {

		update_sensitivity ();

		{
			string current_device, found_device;
			current_device = device_combo.get_active_text ();
			if (current_device == "") {
				current_device = backend->device_name ();
			}

			// Make sure that the active text is still relevant for this
			// device (it might only be relevant to the previous device!!)
			for (vector<string>::const_iterator i = available_devices.begin(); i != available_devices.end(); ++i) {
				if (*i == current_device)
					found_device = current_device;
			}
			if (found_device == "")
				// device has never been set (or was not relevant
				// for this backend) Let's make sure it's not blank
				current_device = available_devices.front ();

			PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
			set_popdown_strings (device_combo, available_devices);

			device_combo.set_active_text (current_device);
		}

		device_changed ();

		input_latency.set_sensitive (true);
		output_latency.set_sensitive (true);
		input_channels.set_sensitive (true);
		output_channels.set_sensitive (true);

		ok_button->set_sensitive (true);
		apply_button->set_sensitive (true);

	} else {
		device_combo.clear();
		sample_rate_combo.set_sensitive (false);
		buffer_size_combo.set_sensitive (false);
		input_latency.set_sensitive (false);
		output_latency.set_sensitive (false);
		input_channels.set_sensitive (false);
		output_channels.set_sensitive (false);
		if (_have_control) {
			ok_button->set_sensitive (false);
			apply_button->set_sensitive (false);
		} else {
			ok_button->set_sensitive (true);
			apply_button->set_sensitive (true);
			if (backend->can_change_sample_rate_when_running() && sample_rate_combo.get_children().size() > 0) {
				sample_rate_combo.set_sensitive (true);
			}
			if (backend->can_change_buffer_size_when_running() && buffer_size_combo.get_children().size() > 0) {
				buffer_size_combo.set_sensitive (true);
			}

		}
	}
}

void
EngineControl::driver_changed ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	backend->set_driver (driver_combo.get_active_text());
	list_devices ();

	if (!ignore_changes) {
		maybe_display_saved_state ();
	}
}

void
EngineControl::device_changed ()
{

	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);
	string device_name = device_combo.get_active_text ();
	vector<string> s;

	if (device_name != backend->device_name()) {
		/* we set the backend-device to query various device related intormation.
		 * This has the side effect that backend->device_name() will match
		 * the device_name and  'change_device' will never be true.
		 * so work around this by setting...
		 */
		queue_device_changed = true;
	}
	
	//the device name must be set FIRST so ASIO can populate buffersizes and the control panel button
	backend->set_device_name(device_name);

	{
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);

		/* don't allow programmatic change to combos to cause a
		   recursive call to this method.
		 */

		/* sample rates */

		string desired;

		vector<float> sr;

		if (_have_control) {
			sr = backend->available_sample_rates (device_name);
		} else {

			sr.push_back (8000.0f);
			sr.push_back (16000.0f);
			sr.push_back (32000.0f);
			sr.push_back (44100.0f);
			sr.push_back (48000.0f);
			sr.push_back (88200.0f);
			sr.push_back (96000.0f);
			sr.push_back (192000.0f);
			sr.push_back (384000.0f);
		}

		for (vector<float>::const_iterator x = sr.begin(); x != sr.end(); ++x) {
			s.push_back (rate_as_string (*x));
			if (*x == _desired_sample_rate) {
				desired = s.back();
			}
		}

		if (!s.empty()) {
			sample_rate_combo.set_sensitive (true);
			set_popdown_strings (sample_rate_combo, s);

			if (desired.empty()) {
				sample_rate_combo.set_active_text (rate_as_string (backend->default_sample_rate()));
			} else {
				sample_rate_combo.set_active_text (desired);
			}

		} else {
			sample_rate_combo.set_sensitive (false);
		}

		/* buffer sizes */

		vector<uint32_t> bs;

		if (_have_control) {
			bs = backend->available_buffer_sizes (device_name);
		} else if (backend->can_change_buffer_size_when_running()) {
			bs.push_back (8);
			bs.push_back (16);
			bs.push_back (32);
			bs.push_back (64);
			bs.push_back (128);
			bs.push_back (256);
			bs.push_back (512);
			bs.push_back (1024);
			bs.push_back (2048);
			bs.push_back (4096);
			bs.push_back (8192);
		}
		s.clear ();
		for (vector<uint32_t>::const_iterator x = bs.begin(); x != bs.end(); ++x) {
			s.push_back (bufsize_as_string (*x));
		}

		if (!s.empty()) {
			buffer_size_combo.set_sensitive (true);
			set_popdown_strings (buffer_size_combo, s);

			uint32_t period = backend->buffer_size();
			if (0 == period) {
				period = backend->default_buffer_size(device_name);
			}
			set_active_text_if_present (buffer_size_combo, bufsize_as_string (period));
			show_buffer_duration ();
		} else {
			buffer_size_combo.set_sensitive (false);
		}

		/* XXX theoretically need to set min + max channel counts here
		*/

		manage_control_app_sensitivity ();
	}

	/* pick up any saved state for this device */

	if (!ignore_changes) {
		maybe_display_saved_state ();
	}
}

string
EngineControl::bufsize_as_string (uint32_t sz)
{
	/* Translators: "samples" is always plural here, so no
	   need for plural+singular forms.
	 */
	char buf[64];
	snprintf (buf, sizeof (buf), "%u %s", sz, P_("sample", "samples", sz));
	return buf;
}

void
EngineControl::sample_rate_changed ()
{
	/* reset the strings for buffer size to show the correct msec value
	   (reflecting the new sample rate).
	 */

	show_buffer_duration ();

}

void
EngineControl::buffer_size_changed ()
{
	show_buffer_duration ();
}

void
EngineControl::show_buffer_duration ()
{

	/* buffer sizes  - convert from just samples to samples + msecs for
	 * the displayed string
	 */

	string bs_text = buffer_size_combo.get_active_text ();
	uint32_t samples = atoi (bs_text); /* will ignore trailing text */
	uint32_t rate = get_rate();

	/* Developers: note the hard-coding of a double buffered model
	   in the (2 * samples) computation of latency. we always start
	   the audiobackend in this configuration.
	 */
	/* note to jack1 developers: ardour also always starts the engine
	 * in async mode (no jack2 --sync option) which adds an extra cycle
	 * of latency with jack2 (and *3 would be correct)
	 * The value can also be wrong if jackd is started externally..
	 *
	 * At the time of writing the ALSA backend always uses double-buffering *2,
	 * The Dummy backend *1, and who knows what ASIO really does :)
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
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	backend->set_midi_option (get_midi_option());

	vector<ARDOUR::AudioBackend::DeviceStatus> midi_devices = backend->enumerate_midi_devices();

	//_midi_devices.clear(); // TODO merge with state-saved settings..
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
EngineControl::parameter_changed ()
{
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
EngineControl::get_saved_state_for_currently_displayed_backend_and_device ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (backend) {
		return get_matching_state (backend_combo.get_active_text(),
				(backend->requires_driver_selection() ? (std::string) driver_combo.get_active_text() : string()),
				device_combo.get_active_text());
	}


	return get_matching_state (backend_combo.get_active_text(),
			string(),
			device_combo.get_active_text());
}

EngineControl::State
EngineControl::save_state ()
{
	State state;

	if (!_have_control) {
		state = get_matching_state (backend_combo.get_active_text(), string(), string());
		if (state) {
			return state;
		}
		state.reset(new StateStruct);
		state->backend = get_backend ();
	} else {
		state.reset(new StateStruct);
		store_state (state);
	}

	for (StateList::iterator i = states.begin(); i != states.end();) {
		if ((*i)->backend == state->backend &&
				(*i)->driver == state->driver &&
				(*i)->device == state->device) {
			i =  states.erase(i);
		} else {
			++i;
		}
	}

	states.push_back (state);

	return state;
}

void
EngineControl::store_state (State state)
{
	state->backend = get_backend ();
	state->driver = get_driver ();
	state->device = get_device_name ();
	state->sample_rate = get_rate ();
	state->buffer_size = get_buffer_size ();
	state->input_latency = get_input_latency ();
	state->output_latency = get_output_latency ();
	state->input_channels = get_input_channels ();
	state->output_channels = get_output_channels ();
	state->midi_option = get_midi_option ();
	state->midi_devices = _midi_devices;
}

void
EngineControl::maybe_display_saved_state ()
{
	if (!_have_control) {
		return;
	}

	State state = get_saved_state_for_currently_displayed_backend_and_device ();

	if (state) {
		PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);

		if (!_desired_sample_rate) {
			sample_rate_combo.set_active_text (rate_as_string (state->sample_rate));
		}
		set_active_text_if_present (buffer_size_combo, bufsize_as_string (state->buffer_size));
		/* call this explicitly because we're ignoring changes to
		   the controls at this point.
		 */
		show_buffer_duration ();
		input_latency.set_value (state->input_latency);
		output_latency.set_value (state->output_latency);

		if (!state->midi_option.empty()) {
			midi_option_combo.set_active_text (state->midi_option);
			_midi_devices = state->midi_devices;
		}
	}
}

XMLNode&
EngineControl::get_state ()
{
	XMLNode* root = new XMLNode ("AudioMIDISetup");
	std::string path;

	if (!states.empty()) {
		XMLNode* state_nodes = new XMLNode ("EngineStates");

		for (StateList::const_iterator i = states.begin(); i != states.end(); ++i) {

			XMLNode* node = new XMLNode ("State");

			node->add_property ("backend", (*i)->backend);
			node->add_property ("driver", (*i)->driver);
			node->add_property ("device", (*i)->device);
			node->add_property ("sample-rate", (*i)->sample_rate);
			node->add_property ("buffer-size", (*i)->buffer_size);
			node->add_property ("input-latency", (*i)->input_latency);
			node->add_property ("output-latency", (*i)->output_latency);
			node->add_property ("input-channels", (*i)->input_channels);
			node->add_property ("output-channels", (*i)->output_channels);
			node->add_property ("active", (*i)->active ? "yes" : "no");
			node->add_property ("midi-option", (*i)->midi_option);

			XMLNode* midi_devices = new XMLNode ("MIDIDevices");
			for (std::vector<MidiDeviceSettings>::const_iterator p = (*i)->midi_devices.begin(); p != (*i)->midi_devices.end(); ++p) {
				XMLNode* midi_device_stuff = new XMLNode ("MIDIDevice");
				midi_device_stuff->add_property (X_("name"), (*p)->name);
				midi_device_stuff->add_property (X_("enabled"), (*p)->enabled);
				midi_device_stuff->add_property (X_("input-latency"), (*p)->input_latency);
				midi_device_stuff->add_property (X_("output-latency"), (*p)->output_latency);
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
EngineControl::set_state (const XMLNode& root)
{
	XMLNodeList          clist, cclist;
	XMLNodeConstIterator citer, cciter;
	XMLNode* child;
	XMLNode* grandchild;
	XMLProperty* prop = NULL;

	if (root.name() != "AudioMIDISetup") {
		return;
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

			if ((prop = grandchild->property ("backend")) == 0) {
				continue;
			}
			state->backend = prop->value ();

			if ((prop = grandchild->property ("driver")) == 0) {
				continue;
			}
			state->driver = prop->value ();

			if ((prop = grandchild->property ("device")) == 0) {
				continue;
			}
			state->device = prop->value ();

			if ((prop = grandchild->property ("sample-rate")) == 0) {
				continue;
			}
			state->sample_rate = atof (prop->value ());

			if ((prop = grandchild->property ("buffer-size")) == 0) {
				continue;
			}
			state->buffer_size = atoi (prop->value ());

			if ((prop = grandchild->property ("input-latency")) == 0) {
				continue;
			}
			state->input_latency = atoi (prop->value ());

			if ((prop = grandchild->property ("output-latency")) == 0) {
				continue;
			}
			state->output_latency = atoi (prop->value ());

			if ((prop = grandchild->property ("input-channels")) == 0) {
				continue;
			}
			state->input_channels = atoi (prop->value ());

			if ((prop = grandchild->property ("output-channels")) == 0) {
				continue;
			}
			state->output_channels = atoi (prop->value ());

			if ((prop = grandchild->property ("active")) == 0) {
				continue;
			}
			state->active = string_is_affirmative (prop->value ());

			if ((prop = grandchild->property ("midi-option")) == 0) {
				continue;
			}
			state->midi_option = prop->value ();

			state->midi_devices.clear();
			XMLNode* midinode;
			if ((midinode = ARDOUR::find_named_node (*grandchild, "MIDIDevices")) != 0) {
				const XMLNodeList mnc = midinode->children();
				for (XMLNodeList::const_iterator n = mnc.begin(); n != mnc.end(); ++n) {
					if ((*n)->property (X_("name")) == 0
							|| (*n)->property (X_("enabled")) == 0
							|| (*n)->property (X_("input-latency")) == 0
							|| (*n)->property (X_("output-latency")) == 0
						 ) {
						continue;
					}

					MidiDeviceSettings ptr (new MidiDeviceSetting(
								(*n)->property (X_("name"))->value (),
								string_is_affirmative ((*n)->property (X_("enabled"))->value ()),
								atoi ((*n)->property (X_("input-latency"))->value ()),
								atoi ((*n)->property (X_("output-latency"))->value ())
								));
					state->midi_devices.push_back (ptr);
				}
			}

#if 1
			/* remove accumulated duplicates (due to bug in ealier version)
			 * this can be removed again before release
			 */
			for (StateList::iterator i = states.begin(); i != states.end();) {
				if ((*i)->backend == state->backend &&
						(*i)->driver == state->driver &&
						(*i)->device == state->device) {
					i =  states.erase(i);
				} else {
					++i;
				}
			}
#endif

			states.push_back (state);
		}
	}

	/* now see if there was an active state and switch the setup to it */

	// purge states of backend that are not available in this built
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

	for (StateList::const_iterator i = states.begin(); i != states.end(); ++i) {

		if ((*i)->active) {
			PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);
			backend_combo.set_active_text ((*i)->backend);
			driver_combo.set_active_text ((*i)->driver);
			device_combo.set_active_text ((*i)->device);
			sample_rate_combo.set_active_text (rate_as_string ((*i)->sample_rate));
			set_active_text_if_present (buffer_size_combo, bufsize_as_string ((*i)->buffer_size));
			input_latency.set_value ((*i)->input_latency);
			output_latency.set_value ((*i)->output_latency);
			midi_option_combo.set_active_text ((*i)->midi_option);
			break;
		}
	}
}

int
EngineControl::push_state_to_backend (bool start)
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

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
	bool change_latency = false;
	bool change_channels = false;
	bool change_midi = false;

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

			if (queue_device_changed || get_device_name() != backend->device_name()) {
				change_device = true;
			}

			if (get_rate() != backend->sample_rate()) {
				change_rate = true;
			}

			if (get_buffer_size() != backend->buffer_size()) {
				change_bufsize = true;
			}

			if (get_midi_option() != backend->midi_option()) {
				change_midi = true;
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

	if (change_driver || change_device || change_channels || change_latency ||
			(change_rate && !backend->can_change_sample_rate_when_running()) ||
			change_midi ||
			(change_bufsize && !backend->can_change_buffer_size_when_running())) {
		restart_required = true;
	} else {
		restart_required = false;
	}

	if (was_running) {

		if (!change_driver && !change_device && !change_channels && !change_latency && !change_midi) {
			/* no changes in any parameters that absolutely require a
			 * restart, so check those that might be changeable without a
			 * restart
			 */

			if (change_rate && !backend->can_change_sample_rate_when_running()) {
				/* can't do this while running ... */
				restart_required = true;
			}

			if (change_bufsize && !backend->can_change_buffer_size_when_running()) {
				/* can't do this while running ... */
				restart_required = true;
			}
		}
	}

	if (was_running) {
		if (restart_required) {
			if (ARDOUR_UI::instance()->disconnect_from_engine ()) {
				return -1;
			}
		}
	}


	if (change_driver && backend->set_driver (get_driver())) {
		error << string_compose (_("Cannot set driver to %1"), get_driver()) << endmsg;
		return -1;
	}
	if (change_device && backend->set_device_name (get_device_name())) {
		error << string_compose (_("Cannot set device name to %1"), get_device_name()) << endmsg;
		return -1;
	}
	if (change_rate && backend->set_sample_rate (get_rate())) {
		error << string_compose (_("Cannot set sample rate to %1"), get_rate()) << endmsg;
		return -1;
	}
	if (change_bufsize && backend->set_buffer_size (get_buffer_size())) {
		error << string_compose (_("Cannot set buffer size to %1"), get_buffer_size()) << endmsg;
		return -1;
	}

	if (change_channels || get_input_channels() == 0 || get_output_channels() == 0) {
		if (backend->set_input_channels (get_input_channels())) {
			error << string_compose (_("Cannot set input channels to %1"), get_input_channels()) << endmsg;
			return -1;
		}
		if (backend->set_output_channels (get_output_channels())) {
			error << string_compose (_("Cannot set output channels to %1"), get_output_channels()) << endmsg;
			return -1;
		}
	}
	if (change_latency) {
		if (backend->set_systemic_input_latency (get_input_latency())) {
			error << string_compose (_("Cannot set input latency to %1"), get_input_latency()) << endmsg;
			return -1;
		}
		if (backend->set_systemic_output_latency (get_output_latency())) {
			error << string_compose (_("Cannot set output latency to %1"), get_output_latency()) << endmsg;
			return -1;
		}
	}

	if (change_midi) {
		backend->set_midi_option (get_midi_option());
	}

	if (1 /* TODO */) {
		for (vector<MidiDeviceSettings>::const_iterator p = _midi_devices.begin(); p != _midi_devices.end(); ++p) {
			if (_measure_midi) {
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
		if (ARDOUR_UI::instance()->reconnect_to_engine()) {
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

	/* all off */

	for (StateList::iterator i = states.begin(); i != states.end(); ++i) {
		(*i)->active = false;
	}

	/* mark this one active (to be used next time the dialog is
	 * shown)
	 */

	state->active = true;

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

string
EngineControl::get_midi_option () const
{
	return midi_option_combo.get_active_text();
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
	if (driver_combo.get_sensitive() && driver_combo.get_parent()) {
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

void
EngineControl::control_app_button_clicked ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();

	if (!backend) {
		return;
	}

	backend->launch_control_app ();
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
}

void
EngineControl::set_desired_sample_rate (uint32_t sr)
{
	_desired_sample_rate = sr;
	device_changed ();
}

void
EngineControl::on_switch_page (GtkNotebookPage*, guint page_num)
{
	if (page_num == 0) {
		cancel_button->set_sensitive (true);
		ok_button->set_sensitive (true);
		apply_button->set_sensitive (true);
		_measure_midi.reset();
	} else {
		cancel_button->set_sensitive (false);
		ok_button->set_sensitive (false);
		apply_button->set_sensitive (false);
	}

	if (page_num == midi_tab) {
		/* MIDI tab */
		refresh_midi_display ();
	}

	if (page_num == latency_tab) {
		/* latency tab */

		if (ARDOUR::AudioEngine::instance()->running()) {
			// TODO - mark as 'stopped for latency
			ARDOUR_UI::instance()->disconnect_from_engine ();
		}

		{
			PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1);

			/* save any existing latency values */

			uint32_t il = (uint32_t) input_latency.get_value ();
			uint32_t ol = (uint32_t) input_latency.get_value ();

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
			ARDOUR::AudioEngine::instance()->stop_latency_detection();
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

	if (mtdm->err () > 0.3) {
		mtdm->invert ();
		mtdm->resolve ();
	}

	char buf[256];
	ARDOUR::framecnt_t const sample_rate = ARDOUR::AudioEngine::instance()->sample_rate();

	if (sample_rate == 0) {
		lm_results.set_markup (string_compose (results_markup, _("Disconnected from audio engine")));
		ARDOUR::AudioEngine::instance()->stop_latency_detection ();
		return false;
	}

	int frames_total = mtdm->del();
	int extra = frames_total - ARDOUR::AudioEngine::instance()->latency_signal_delay();

	snprintf (buf, sizeof (buf), "%s%d samples (%.3lf ms)\n%s%d samples (%.3lf ms)",
			_("Detected roundtrip latency: "),
			frames_total, frames_total * 1000.0f/sample_rate,
			_("Systemic latency: "),
			extra, extra * 1000.0f/sample_rate);

	bool solid = true;

	if (mtdm->err () > 0.2) {
		strcat (buf, " ");
		strcat (buf, _("(signal detection error)"));
		solid = false;
	}

	if (mtdm->inv ()) {
		strcat (buf, " ");
		strcat (buf, _("(inverted - bad wiring)"));
		solid = false;
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
	ARDOUR::framecnt_t const sample_rate = ARDOUR::AudioEngine::instance()->sample_rate();

	if (sample_rate == 0) {
		lm_results.set_markup (string_compose (results_markup, _("Disconnected from audio engine")));
		ARDOUR::AudioEngine::instance()->stop_latency_detection ();
		return false;
	}

	ARDOUR::framecnt_t frames_total = mididm->latency();
	ARDOUR::framecnt_t extra = frames_total - ARDOUR::AudioEngine::instance()->latency_signal_delay();
	snprintf (buf, sizeof (buf), "%s%" PRId64" samples (%.1lf ms) dev: %.2f[spl]\n%s%" PRId64" samples (%.1lf ms)",
			_("Detected roundtrip latency: "),
			frames_total, frames_total * 1000.0f / sample_rate, mididm->deviation (),
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
	ARDOUR::AudioEngine::instance()->set_latency_input_port (lm_input_channel_combo.get_active_text());
	ARDOUR::AudioEngine::instance()->set_latency_output_port (lm_output_channel_combo.get_active_text());

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
	if (!lm_running) {
		start_latency_detection ();
	} else {
		end_latency_detection ();
	}
}

void
EngineControl::use_latency_button_clicked ()
{
	if (_measure_midi) {
		ARDOUR::MIDIDM* mididm = ARDOUR::AudioEngine::instance()->mididm ();
		if (!mididm) {
			return;
		}
		ARDOUR::framecnt_t frames_total = mididm->latency();
		ARDOUR::framecnt_t extra = frames_total - ARDOUR::AudioEngine::instance()->latency_signal_delay();
		uint32_t one_way = max ((ARDOUR::framecnt_t) 0, extra / 2);
		_measure_midi->input_latency = one_way;
		_measure_midi->output_latency = one_way;
		notebook.set_current_page (midi_tab);
	} else {
		MTDM* mtdm = ARDOUR::AudioEngine::instance()->mtdm ();

		if (!mtdm) {
			return;
		}

		double one_way = rint ((mtdm->del() - ARDOUR::AudioEngine::instance()->latency_signal_delay()) / 2.0);
		one_way = std::max (0., one_way);

		input_latency_adjustment.set_value (one_way);
		output_latency_adjustment.set_value (one_way);

		/* back to settings page */
		notebook.set_current_page (0);
}
	}


bool
EngineControl::on_delete_event (GdkEventAny* ev)
{
	if (notebook.get_current_page() == 2) {
		/* currently on latency tab - be sure to clean up */
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

	buffer_size_combo.set_sensitive (true);
	sample_rate_combo.set_sensitive (true);

	connect_disconnect_button.set_label (string_compose (_("Disconnect from %1"), backend->name()));
	connect_disconnect_button.show();

	started_at_least_once = true;
	engine_status.set_markup(string_compose ("<span foreground=\"green\">%1</span>", _("Active")));
}

void
EngineControl::engine_stopped ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	buffer_size_combo.set_sensitive (false);
	connect_disconnect_button.set_label (string_compose (_("Connect to %1"), backend->name()));
	connect_disconnect_button.show();

	sample_rate_combo.set_sensitive (true);
	buffer_size_combo.set_sensitive (true);
	engine_status.set_markup(string_compose ("<span foreground=\"red\">%1</span>", _("Inactive")));
}

void
EngineControl::device_list_changed ()
{
	PBD::Unwinder<uint32_t> protect_ignore_changes (ignore_changes, ignore_changes + 1); // ??
	list_devices ();
	midi_option_changed();
}

void
EngineControl::connect_disconnect_click()
{
	if (ARDOUR::AudioEngine::instance()->running()) {
		ARDOUR_UI::instance()->disconnect_from_engine ();
	} else {
		ARDOUR_UI::instance()->reconnect_to_engine ();
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
