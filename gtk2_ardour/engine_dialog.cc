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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <vector>
#include <cmath>
#include <fstream>
#include <map>

#include <boost/scoped_ptr.hpp>

#include <glibmm.h>
#include <gtkmm/messagedialog.h>

#include "pbd/xml++.h"

#include <jack/jack.h>

#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include "ardour/rc_configuration.h"

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"

#include "ardour/jack_utils.h"

#include "engine_dialog.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

EngineControl::EngineControl ()
	: periods_adjustment (2, 2, 16, 1, 2),
	  periods_spinner (periods_adjustment),
	  ports_adjustment (128, 8, 1024, 1, 16),
	  ports_spinner (ports_adjustment),
	  input_latency_adjustment (0, 0, 99999, 1),
	  input_latency (input_latency_adjustment),
	  output_latency_adjustment (0, 0, 99999, 1),
	  output_latency (output_latency_adjustment),
	  realtime_button (_("Realtime")),
	  no_memory_lock_button (_("Do not lock memory")),
	  unlock_memory_button (_("Unlock memory")),
	  soft_mode_button (_("No zombies")),
	  monitor_button (_("Provide monitor ports")),
	  force16bit_button (_("Force 16 bit")),
	  hw_monitor_button (_("H/W monitoring")),
	  hw_meter_button (_("H/W metering")),
	  verbose_output_button (_("Verbose output")),
	  start_button (_("Start")),
	  stop_button (_("Stop")),
#ifdef __APPLE___
	  basic_packer (5, 2),
	  options_packer (4, 2),
	  device_packer (4, 2)
#else
	  basic_packer (8, 2),
	  options_packer (14, 2),
	  device_packer (6, 2)
#endif
{
	using namespace Notebook_Helpers;
	Label* label;
	vector<string> strings;
	int row = 0;

	_used = false;

	ARDOUR::get_jack_sample_rate_strings (strings);
	set_popdown_strings (sample_rate_combo, strings);
	sample_rate_combo.set_active_text (ARDOUR::get_jack_default_sample_rate ());

	strings.clear ();
	ARDOUR::get_jack_period_size_strings (strings);
	set_popdown_strings (period_size_combo, strings);
	period_size_combo.set_active_text (ARDOUR::get_jack_default_period_size ());

	/* basic parameters */

	basic_packer.set_spacings (6);

	strings.clear ();
	ARDOUR::get_jack_audio_driver_names (strings);
	set_popdown_strings (driver_combo, strings);
	string default_driver;
	ARDOUR::get_jack_default_audio_driver_name (default_driver);
	driver_combo.set_active_text (default_driver);

	driver_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::driver_changed));
	driver_changed ();

	strings.clear ();
	strings.push_back (_("Playback/recording on 1 device"));
	strings.push_back (_("Playback/recording on 2 devices"));
	strings.push_back (_("Playback only"));
	strings.push_back (_("Recording only"));
	set_popdown_strings (audio_mode_combo, strings);
	audio_mode_combo.set_active_text (strings.front());

	audio_mode_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::audio_mode_changed));
	audio_mode_changed ();


	row = 0;

	label = manage (left_aligned_label (_("Driver:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (driver_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (left_aligned_label (_("Audio Interface:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (interface_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (left_aligned_label (_("Sample rate:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (left_aligned_label (_("Buffer size:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (period_size_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	label = manage (left_aligned_label (_("Number of buffers:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (periods_spinner, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	periods_spinner.set_value (2);
	row++;
#endif

	label = manage (left_aligned_label (_("Approximate latency:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (latency_label, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	periods_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	period_size_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	redisplay_latency();
	row++;
	/* no audio mode with CoreAudio, its duplex or nuthin' */

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	label = manage (left_aligned_label (_("Audio mode:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (audio_mode_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;
#endif

	interface_combo.set_size_request (250, -1);
	input_device_combo.set_size_request (250, -1);
	output_device_combo.set_size_request (250, -1);

	/*

	if (engine_running()) {
		start_button.set_sensitive (false);
	} else {
		stop_button.set_sensitive (false);
	}

	start_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::start_engine));
	stop_button.signal_clicked().connect (sigc::mem_fun (*this, &EngineControl::start_engine));
	*/

	button_box.pack_start (start_button, false, false);
	button_box.pack_start (stop_button, false, false);

	// basic_packer.attach (button_box, 0, 2, 8, 9, FILL|EXPAND, (AttachOptions) 0);

	/* options */

	options_packer.set_spacings (6);
	row = 0;

	options_packer.attach (realtime_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

	realtime_button.set_active (true);

#if PROVIDE_TOO_MANY_OPTIONS

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	options_packer.attach (no_memory_lock_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (unlock_memory_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (soft_mode_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (monitor_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (force16bit_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (hw_monitor_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (hw_meter_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	options_packer.attach (verbose_output_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
#else
	options_packer.attach (verbose_output_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
#endif

	strings.clear ();
	strings.push_back (_("Ignore"));
	strings.push_back ("500 msec");
	strings.push_back ("1 sec");
	strings.push_back ("2 sec");
	strings.push_back ("10 sec");
	set_popdown_strings (timeout_combo, strings);
	timeout_combo.set_active_text (strings.front ());

	label = manage (new Label (_("Client timeout")));
	label->set_alignment (1.0, 0.5);
	options_packer.attach (timeout_combo, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

#endif /* PROVIDE_TOO_MANY_OPTIONS */
	label = manage (left_aligned_label (_("Number of ports:")));
	options_packer.attach (ports_spinner, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

	label = manage (left_aligned_label (_("MIDI driver:")));
	options_packer.attach (midi_driver_combo, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	label = manage (left_aligned_label (_("Dither:")));
	options_packer.attach (dither_mode_combo, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
#endif

	vector<std::string> jack_server_paths;

	if (!ARDOUR::get_jack_server_paths (jack_server_paths)) {
		fatal << _("No JACK server found anywhere on this system. Please install JACK and restart") << endmsg;
		/*NOTREACHED*/
	}

	for (vector<std::string>::const_iterator i = jack_server_paths.begin(); i != jack_server_paths.end(); ++i) {
		server_strings.push_back (*i);
	}

	std::string default_server_path;
	ARDOUR::get_jack_default_server_path (default_server_path);

	set_popdown_strings (serverpath_combo, server_strings);
	serverpath_combo.set_active_text (default_server_path);

	if (server_strings.size() > 1) {
		label = manage (left_aligned_label (_("Server:")));
		options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
		options_packer.attach (serverpath_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
		++row;
	}

	/* device settings */

	device_packer.set_spacings (6);
	row = 0;

#if !defined(__APPLE__) && !defined(__FreeBSD__)
	label = manage (left_aligned_label (_("Input device:")));
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_device_combo, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	label = manage (left_aligned_label (_("Output device:")));
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_device_combo, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;
#endif
	label = manage (left_aligned_label (_("Hardware input latency:")));
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_latency, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (left_aligned_label (_("samples")));
	device_packer.attach (*label, 2, 3, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	label = manage (left_aligned_label (_("Hardware output latency:")));
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_latency, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (left_aligned_label (_("samples")));
	device_packer.attach (*label, 2, 3, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;

	basic_hbox.pack_start (basic_packer, false, false);
	options_hbox.pack_start (options_packer, false, false);

	device_packer.set_border_width (12);
	options_packer.set_border_width (12);
	basic_packer.set_border_width (12);

	notebook.pages().push_back (TabElem (basic_hbox, _("Device")));
	notebook.pages().push_back (TabElem (options_hbox, _("Options")));
	notebook.pages().push_back (TabElem (device_packer, _("Advanced")));
	notebook.set_border_width (12);

	set_border_width (12);
	pack_start (notebook);

	/* Pick up any existing audio setup configuration, if appropriate */

	XMLNode* audio_setup = ARDOUR::Config->extra_xml ("AudioSetup");
	
	if (audio_setup) {
		set_state (*audio_setup);
	}
}

EngineControl::~EngineControl ()
{

}

bool
EngineControl::build_command_line (string& command_line)
{
	ARDOUR::JackCommandLineOptions options;

	options.server_path = serverpath_combo.get_active_text ();

	/* now jackd arguments */

	string str = timeout_combo.get_active_text ();

	if (str != _("Ignore")) {

		double secs = 0;
		uint32_t msecs;
		secs = atof (str);
		msecs = (uint32_t) floor (secs * 1000.0);

		options.timeout = msecs;
	}

	options.no_mlock = no_memory_lock_button.get_active();
	options.ports_max = (uint32_t) floor (ports_spinner.get_value());
	options.realtime = realtime_button.get_active();
	options.unlock_gui_libs = unlock_memory_button.get_active();
	options.verbose = verbose_output_button.get_active();
	options.driver = driver_combo.get_active_text ();

	options.input_device = input_device_combo.get_active_text ();
	options.output_device = output_device_combo.get_active_text ();

	options.num_periods = periods_spinner.get_value ();

	options.samplerate = get_rate();

	options.period_size = atoi (period_size_combo.get_active_text ());

	options.input_latency = input_latency_adjustment.get_value ();
	options.output_latency = output_latency_adjustment.get_value ();

	options.hardware_metering = hw_meter_button.get_active();
	options.hardware_monitoring = hw_monitor_button.get_active();

	options.dither_mode = dither_mode_combo.get_active_text();
	options.force16_bit = force16bit_button.get_active();
	options.soft_mode = soft_mode_button.get_active();

	options.midi_driver = midi_driver_combo.get_active_text ();

	return get_jack_command_line_string (options, command_line);
}

bool
EngineControl::need_setup ()
{
	return !engine_running();
}

bool
EngineControl::engine_running ()
{
	return ARDOUR::jack_server_running ();
}

int
EngineControl::setup_engine ()
{
	string command_line;

	if (!build_command_line (command_line)) {
		error << _("Unable to start JACK server please try a different configuration") << endmsg;
		return 1; // try again
	}

#ifdef WIN32
	if (!ARDOUR::start_jack_server (command_line)) {
		error << _("Unable to start JACK server please try a different configuration") << endmsg;
		return -1;
	};
#else
	std::string config_file_path = ARDOUR::get_jack_server_user_config_file_path ();

	if (ARDOUR::write_jack_config_file (config_file_path, command_line)) {
		error << string_compose (_("cannot open JACK rc file %1 to store parameters"), config_file_path) << endmsg;
		return -1;
	}
#endif

	_used = true;

	return 0;
}

void
EngineControl::enumerate_devices (const string& driver)
{
	devices[driver] = ARDOUR::get_jack_device_names_for_audio_driver (driver);

	if (devices[driver].empty () && driver == ARDOUR::coreaudio_driver_name) {
		MessageDialog msg (string_compose (_("\
You do not have any audio devices capable of\n\
simultaneous playback and recording.\n\n\
Please use Applications -> Utilities -> Audio MIDI Setup\n\
to create an \"aggregrate\" device, or install a suitable\n\
audio interface.\n\n\
Please send email to Apple and ask them why new Macs\n\
have no duplex audio device.\n\n\
Alternatively, if you really want just playback\n\
or recording but not both, start JACK before running\n\
%1 and choose the relevant device then."
							   ), PROGRAM_NAME),
				   true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
		msg.set_title (_("No suitable audio devices"));
		msg.set_position (Gtk::WIN_POS_MOUSE);
		msg.run ();
		exit (1);
	}
}

void
EngineControl::driver_changed ()
{
	string driver = driver_combo.get_active_text();
	string::size_type maxlen = 0;
	int n = 0;

	enumerate_devices (driver);

	vector<string>& strings = devices[driver];

	if (strings.empty()) {
		error << string_compose (_("No devices found for driver \"%1\""), driver) << endmsg;
		return;
	}

	for (vector<string>::iterator i = strings.begin(); i != strings.end(); ++i, ++n) {
		if ((*i).length() > maxlen) {
			maxlen = (*i).length();
		}
	}

	set_popdown_strings (interface_combo, strings);
	set_popdown_strings (input_device_combo, strings);
	set_popdown_strings (output_device_combo, strings);

	interface_combo.set_active_text (strings.front());
	input_device_combo.set_active_text (strings.front());
	output_device_combo.set_active_text (strings.front());

	vector<string> dither_modes;
	ARDOUR::get_jack_dither_mode_strings (driver, dither_modes);
	set_popdown_strings (dither_mode_combo, dither_modes);
	dither_mode_combo.set_active_text (ARDOUR::get_jack_default_dither_mode (driver));

	vector<string> midi_systems;
	ARDOUR::get_jack_midi_system_names (driver, midi_systems);
	set_popdown_strings (midi_driver_combo, midi_systems);
	string default_midi_system;
	ARDOUR::get_jack_default_midi_system_name (driver, default_midi_system);
	midi_driver_combo.set_active_text (default_midi_system);

	if (driver == ARDOUR::alsa_driver_name) {
		soft_mode_button.set_sensitive (true);
		force16bit_button.set_sensitive (true);
		hw_monitor_button.set_sensitive (true);
		hw_meter_button.set_sensitive (true);
		monitor_button.set_sensitive (true);
	} else {
		soft_mode_button.set_sensitive (false);
		force16bit_button.set_sensitive (false);
		hw_monitor_button.set_sensitive (false);
		hw_meter_button.set_sensitive (false);
		monitor_button.set_sensitive (false);
	}
}

uint32_t
EngineControl::get_rate ()
{
	double r = atof (sample_rate_combo.get_active_text ());
	/* the string may have been translated with an abbreviation for
	 * thousands, so use a crude heuristic to fix this.
	 */
	if (r < 1000.0) {
		r *= 1000.0;
	}
	return lrint (r);
}

void
EngineControl::redisplay_latency ()
{
#if defined(__APPLE__) || defined(__FreeBSD__)
	float periods = 2;
#else
	float periods = periods_adjustment.get_value();
#endif
	string latency = ARDOUR::get_jack_latency_string (sample_rate_combo.get_active_text (),
				periods, period_size_combo.get_active_text ());
	latency_label.set_text (latency);
	latency_label.set_alignment (0, 0.5);
}

void
EngineControl::audio_mode_changed ()
{
	std::string str = audio_mode_combo.get_active_text();

	if (str == _("Playback/recording on 1 device")) {
		input_device_combo.set_sensitive (false);
		output_device_combo.set_sensitive (false);
	} else if (str == _("Playback/recording on 2 devices")) {
		input_device_combo.set_sensitive (true);
		output_device_combo.set_sensitive (true);
	} else if (str == _("Playback only")) {
		output_device_combo.set_sensitive (true);
		input_device_combo.set_sensitive (false);
	} else if (str == _("Recording only")) {
		input_device_combo.set_sensitive (true);
		output_device_combo.set_sensitive (false);
	}
}

void
EngineControl::find_jack_servers (vector<string>& strings)
{
	vector<std::string> server_paths;

	ARDOUR::get_jack_server_paths (server_paths);

#ifdef __APPLE__
	if (getenv ("ARDOUR_WITH_JACK")) {
		/* no other options - only use the JACK we supply */
		if (server_paths.empty()) {
			fatal << string_compose (_("JACK appears to be missing from the %1 bundle"), PROGRAM_NAME) << endmsg;
			/*NOTREACHED*/
		}
		return;
	}

	// push it back into the environment so that auto-started JACK can find it.
	// XXX why can't we just expect OS X users to have PATH set correctly? we can't ...
	ARDOUR::set_path_env_for_jack_autostart (server_paths);
#endif

	for (vector<std::string>::const_iterator i = server_paths.begin();
			i != server_paths.end(); ++i) {
		strings.push_back(*i);
	}
}

XMLNode&
EngineControl::get_state ()
{
	XMLNode* root = new XMLNode ("AudioSetup");
	XMLNode* child;
	std::string path;

	child = new XMLNode ("periods");
	child->add_property ("val", to_string (periods_adjustment.get_value(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("ports");
	child->add_property ("val", to_string (ports_adjustment.get_value(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("inlatency");
	child->add_property ("val", to_string (input_latency.get_value(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("outlatency");
	child->add_property ("val", to_string (output_latency.get_value(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("realtime");
	child->add_property ("val", to_string (realtime_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("nomemorylock");
	child->add_property ("val", to_string (no_memory_lock_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("unlockmemory");
	child->add_property ("val", to_string (unlock_memory_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("softmode");
	child->add_property ("val", to_string (soft_mode_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("force16bit");
	child->add_property ("val", to_string (force16bit_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("hwmonitor");
	child->add_property ("val", to_string (hw_monitor_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("hwmeter");
	child->add_property ("val", to_string (hw_meter_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("verbose");
	child->add_property ("val", to_string (verbose_output_button.get_active(), std::dec));
	root->add_child_nocopy (*child);

	child = new XMLNode ("samplerate");
	child->add_property ("val", sample_rate_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("periodsize");
	child->add_property ("val", period_size_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("serverpath");
	child->add_property ("val", serverpath_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("driver");
	child->add_property ("val", driver_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("interface");
	child->add_property ("val", interface_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("timeout");
	child->add_property ("val", timeout_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("dither");
	child->add_property ("val", dither_mode_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("audiomode");
	child->add_property ("val", audio_mode_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("inputdevice");
	child->add_property ("val", input_device_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("outputdevice");
	child->add_property ("val", output_device_combo.get_active_text());
	root->add_child_nocopy (*child);

	child = new XMLNode ("mididriver");
	child->add_property ("val", midi_driver_combo.get_active_text());
	root->add_child_nocopy (*child);

	return *root;
}

void
EngineControl::set_state (const XMLNode& root)
{
	XMLNodeList          clist;
	XMLNodeConstIterator citer;
	XMLNode* child;
	XMLProperty* prop = NULL;
	bool using_dummy = false;
	bool using_ffado = false;

	int val;
	string strval;

	if ( (child = root.child ("driver"))){
		prop = child->property("val");

		if (prop && (prop->value() == "Dummy") ) {
			using_dummy = true;
		}
		if (prop && (prop->value() == "FFADO") ) {
			using_ffado = true;
		}

	}

	clist = root.children();

	for (citer = clist.begin(); citer != clist.end(); ++citer) {

		child = *citer;

		prop = child->property ("val");

		if (!prop || prop->value().empty()) {

			if (((using_dummy || using_ffado)
				&& ( child->name() == "interface"
					|| child->name() == "inputdevice"
					|| child->name() == "outputdevice"))
				|| child->name() == "timeout")
			{
				continue;
			}

			error << string_compose (_("AudioSetup value for %1 is missing data"), child->name()) << endmsg;
			continue;
		}

		strval = prop->value();

		/* adjustments/spinners */

		if (child->name() == "periods") {
			val = atoi (strval);
			periods_adjustment.set_value(val);
		} else if (child->name() == "ports") {
			val = atoi (strval);
			ports_adjustment.set_value(val);
		} else if (child->name() == "inlatency") {
			val = atoi (strval);
			input_latency.set_value(val);
		} else if (child->name() == "outlatency") {
			val = atoi (strval);
			output_latency.set_value(val);
		}

		/* buttons */

		else if (child->name() == "realtime") {
			val = atoi (strval);
			realtime_button.set_active(val);
		} else if (child->name() == "nomemorylock") {
			val = atoi (strval);
			no_memory_lock_button.set_active(val);
		} else if (child->name() == "unlockmemory") {
			val = atoi (strval);
			unlock_memory_button.set_active(val);
		} else if (child->name() == "softmode") {
			val = atoi (strval);
			soft_mode_button.set_active(val);
		} else if (child->name() == "force16bit") {
			val = atoi (strval);
			force16bit_button.set_active(val);
		} else if (child->name() == "hwmonitor") {
			val = atoi (strval);
			hw_monitor_button.set_active(val);
		} else if (child->name() == "hwmeter") {
			val = atoi (strval);
			hw_meter_button.set_active(val);
		} else if (child->name() == "verbose") {
			val = atoi (strval);
			verbose_output_button.set_active(val);
		}

		/* combos */

		else if (child->name() == "samplerate") {
			sample_rate_combo.set_active_text(strval);
		} else if (child->name() == "periodsize") {
			period_size_combo.set_active_text(strval);
		} else if (child->name() == "serverpath") {

                        /* only attempt to set this if we have bothered to look
                           up server names already. otherwise this is all
                           redundant (actually, all of this dialog/widget
                           is redundant in that case ...)
                        */

                        if (!server_strings.empty()) {
                                /* do not allow us to use a server path that doesn't
                                   exist on this system. this handles cases where
                                   the user has an RC file listing a serverpath
                                   from some other machine.
                                */
                                vector<string>::iterator x;
                                for (x = server_strings.begin(); x != server_strings.end(); ++x) {
                                        if (*x == strval) {
                                                break;
                                        }
                                }
                                if (x != server_strings.end()) {
                                        serverpath_combo.set_active_text (strval);
                                } else {
                                        warning << string_compose (_("configuration files contain a JACK server path that doesn't exist (%1)"),
                                                                   strval)
                                                << endmsg;
                                }
                        }

		} else if (child->name() == "driver") {
			driver_combo.set_active_text(strval);
		} else if (child->name() == "interface") {
			interface_combo.set_active_text(strval);
		} else if (child->name() == "timeout") {
			timeout_combo.set_active_text(strval);
		} else if (child->name() == "dither") {
			dither_mode_combo.set_active_text(strval);
		} else if (child->name() == "audiomode") {
			audio_mode_combo.set_active_text(strval);
		} else if (child->name() == "inputdevice") {
			input_device_combo.set_active_text(strval);
		} else if (child->name() == "outputdevice") {
			output_device_combo.set_active_text(strval);
		} else if (child->name() == "mididriver") {
			midi_driver_combo.set_active_text(strval);
		}
	}
}
