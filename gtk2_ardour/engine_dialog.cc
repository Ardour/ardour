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

#include <vector>
#include <cmath>
#include <fstream>
#include <map>

#include <boost/scoped_ptr.hpp>

#include <glibmm.h>
#include <gtkmm/messagedialog.h>

#include "pbd/epa.h"
#include "pbd/xml++.h"

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CFString.h>
#include <sys/param.h>
#include <mach-o/dyld.h>
#else
#include <alsa/asoundlib.h>
#endif

#include "ardour/profile.h"
#include <jack/jack.h>

#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"

#ifdef __APPLE
#include <CFBundle.h>
#endif

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
	  priority_adjustment (60, 10, 90, 1, 10),
	  priority_spinner (priority_adjustment),
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
#ifdef __APPLE__
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

	strings.push_back (_("8000Hz"));
	strings.push_back (_("22050Hz"));
	strings.push_back (_("44100Hz"));
	strings.push_back (_("48000Hz"));
	strings.push_back (_("88200Hz"));
	strings.push_back (_("96000Hz"));
	strings.push_back (_("192000Hz"));
	set_popdown_strings (sample_rate_combo, strings);
	sample_rate_combo.set_active_text ("48000Hz");

	strings.clear ();
	strings.push_back ("32");
	strings.push_back ("64");
	strings.push_back ("128");
	strings.push_back ("256");
	strings.push_back ("512");
	strings.push_back ("1024");
	strings.push_back ("2048");
	strings.push_back ("4096");
	strings.push_back ("8192");
	set_popdown_strings (period_size_combo, strings);
	period_size_combo.set_active_text ("1024");

	strings.clear ();
	strings.push_back (_("None"));
	strings.push_back (_("Triangular"));
	strings.push_back (_("Rectangular"));
	strings.push_back (_("Shaped"));
	set_popdown_strings (dither_mode_combo, strings);
	dither_mode_combo.set_active_text (_("None"));

	/* basic parameters */

	basic_packer.set_spacings (6);

	strings.clear ();
#ifdef __APPLE__
	strings.push_back (X_("CoreAudio"));
#else
	strings.push_back (X_("ALSA"));
	strings.push_back (X_("OSS"));
	strings.push_back (X_("FreeBoB"));
	strings.push_back (X_("FFADO"));
#endif
	strings.push_back (X_("NetJACK"));
	strings.push_back (X_("Dummy"));
	set_popdown_strings (driver_combo, strings);
	driver_combo.set_active_text (strings.front());

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

	strings.clear ();
	strings.push_back (_("None"));
	strings.push_back (_("seq"));
	strings.push_back (_("raw"));
	set_popdown_strings (midi_driver_combo, strings);
	midi_driver_combo.set_active_text (strings.front ());

	row = 0;

	label = manage (new Label (_("Driver:")));
	label->set_alignment (0, 0.5);
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (driver_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (new Label (_("Audio Interface:")));
	label->set_alignment (0, 0.5);
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (interface_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (new Label (_("Sample rate:")));
	label->set_alignment (0, 0.5);
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (new Label (_("Buffer size:")));
	label->set_alignment (0, 0.5);
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (period_size_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

#ifndef __APPLE__
	label = manage (new Label (_("Number of buffers:")));
	label->set_alignment (0, 0.5);
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (periods_spinner, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	periods_spinner.set_value (2);
	row++;
#endif

	label = manage (new Label (_("Approximate latency:")));
	label->set_alignment (0, 0.5);
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (latency_label, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	periods_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	period_size_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	redisplay_latency();
	row++;
	/* no audio mode with CoreAudio, its duplex or nuthin' */

#ifndef __APPLE__
	label = manage (new Label (_("Audio mode:")));
	label->set_alignment (0, 0.5);
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
	realtime_button.signal_toggled().connect (sigc::mem_fun (*this, &EngineControl::realtime_changed));
	realtime_changed ();

#if PROVIDE_TOO_MANY_OPTIONS

#ifndef __APPLE__
	label = manage (new Label (_("Realtime Priority")));
	label->set_alignment (1.0, 0.5);
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (priority_spinner, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	priority_spinner.set_value (60);

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
	label = manage (new Label (_("Number of ports:")));
	label->set_alignment (0, 0.5);
	options_packer.attach (ports_spinner, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

	label = manage (new Label (_("MIDI driver:")));
	label->set_alignment (0, 0.5);
	options_packer.attach (midi_driver_combo, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

#ifndef __APPLE__
	label = manage (new Label (_("Dither:")));
	label->set_alignment (0, 0.5);
	options_packer.attach (dither_mode_combo, 1, 2, row, row + 1, FILL|EXPAND, AttachOptions(0));
	options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;
#endif

	find_jack_servers (server_strings);

	if (server_strings.empty()) {
		fatal << _("No JACK server found anywhere on this system. Please install JACK and restart") << endmsg;
		/*NOTREACHED*/
	}

	set_popdown_strings (serverpath_combo, server_strings);
	serverpath_combo.set_active_text (server_strings.front());

	if (server_strings.size() > 1) {
		label = manage (new Label (_("Server:")));
		options_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
		label->set_alignment (0.0, 0.5);
		options_packer.attach (serverpath_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
		++row;
	}

	/* device settings */

	device_packer.set_spacings (6);
	row = 0;

#ifndef __APPLE__
	label = manage (new Label (_("Input device:")));
	label->set_alignment (0, 0.5);
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_device_combo, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	label = manage (new Label (_("Output device:")));
	label->set_alignment (0, 0.5);
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_device_combo, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;
#endif
	label = manage (new Label (_("Hardware input latency:")));
	label->set_alignment (0, 0.5);
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_latency, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("samples")));
	label->set_alignment (0, 0.5);
	device_packer.attach (*label, 2, 3, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	++row;
	label = manage (new Label (_("Hardware output latency:")));
	label->set_alignment (0, 0.5);
	device_packer.attach (*label, 0, 1, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_latency, 1, 2, row, row+1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("samples")));
	label->set_alignment (0, 0.5);
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
}

EngineControl::~EngineControl ()
{

}

void
EngineControl::build_command_line (vector<string>& cmd)
{
	string str;
	string driver;
	bool using_alsa = false;
	bool using_coreaudio = false;
	bool using_dummy = false;
	bool using_ffado = false;

	/* first, path to jackd */

	cmd.push_back (serverpath_combo.get_active_text ());

	/* now jackd arguments */

	str = timeout_combo.get_active_text ();

	if (str != _("Ignore")) {

		double secs = 0;
		uint32_t msecs;
		secs = atof (str);
		msecs = (uint32_t) floor (secs * 1000.0);

		if (msecs > 0) {
			cmd.push_back ("-t");
			cmd.push_back (to_string (msecs, std::dec));
		}
	}

	if (no_memory_lock_button.get_active()) {
		cmd.push_back ("-m"); /* no munlock */
	}

	cmd.push_back ("-p"); /* port max */
	cmd.push_back (to_string ((uint32_t) floor (ports_spinner.get_value()), std::dec));

	if (realtime_button.get_active()) {
		cmd.push_back ("-R");
		cmd.push_back ("-P");
		cmd.push_back (to_string ((uint32_t) floor (priority_spinner.get_value()), std::dec));
	} else {
		cmd.push_back ("-r"); /* override jackd's default --realtime */
	}

	if (unlock_memory_button.get_active()) {
		cmd.push_back ("-u");
	}

	if (verbose_output_button.get_active()) {
		cmd.push_back ("-v");
	}

	/* now add fixed arguments (not user-selectable) */

	cmd.push_back ("-T"); // temporary */

	/* next the driver */

	cmd.push_back ("-d");

	driver = driver_combo.get_active_text ();

	if (driver == X_("ALSA")) {
		using_alsa = true;
		cmd.push_back ("alsa");
	} else if (driver == X_("OSS")) {
		cmd.push_back ("oss");
	} else if (driver == X_("CoreAudio")) {
		using_coreaudio = true;
		cmd.push_back ("coreaudio");
	} else if (driver == X_("NetJACK")) {
		cmd.push_back ("netjack");
	} else if (driver == X_("FreeBoB")) {
		cmd.push_back ("freebob");
	} else if (driver == X_("FFADO")) {
		using_ffado = true;
		cmd.push_back ("firewire");
	} else if ( driver == X_("Dummy")) {
		using_dummy = true;
		cmd.push_back ("dummy");
	}

	/* driver arguments */

	if (!using_coreaudio) {
		str = audio_mode_combo.get_active_text();

		if (str == _("Playback/Recording on 1 Device")) {

			/* relax */

		} else if (str == _("Playback/Recording on 2 Devices")) {

			string input_device = get_device_name (driver, input_device_combo.get_active_text());
			string output_device = get_device_name (driver, output_device_combo.get_active_text());

			if (input_device.empty() || output_device.empty()) {
				cmd.clear ();
				return;
			}

			cmd.push_back ("-C");
			cmd.push_back (input_device);

			cmd.push_back ("-P");
			cmd.push_back (output_device);

		} else if (str == _("Playback only")) {
			cmd.push_back ("-P");
		} else if (str == _("Recording only")) {
			cmd.push_back ("-C");
		}

		if (!using_dummy) {
			cmd.push_back ("-n");
			cmd.push_back (to_string ((uint32_t) floor (periods_spinner.get_value()), std::dec));
		}
	}

	cmd.push_back ("-r");
	cmd.push_back (to_string (get_rate(), std::dec));

	cmd.push_back ("-p");
	cmd.push_back (period_size_combo.get_active_text());

	if (using_alsa || using_ffado || using_coreaudio) {

		double val = input_latency_adjustment.get_value();

                if (val) {
                        cmd.push_back ("-I");
                        cmd.push_back (to_string ((uint32_t) val, std::dec));
                }

                val = output_latency_adjustment.get_value();

		if (val) {
                        cmd.push_back ("-O");
                        cmd.push_back (to_string ((uint32_t) val, std::dec));
                }
	}

	if (using_alsa) {

		if (audio_mode_combo.get_active_text() != _("Playback/Recording on 2 Devices")) {

			string device = get_device_name (driver, interface_combo.get_active_text());
			if (device.empty()) {
				cmd.clear ();
				return;
			}

			cmd.push_back ("-d");
			cmd.push_back (device);
		}

		if (hw_meter_button.get_active()) {
			cmd.push_back ("-M");
		}

		if (hw_monitor_button.get_active()) {
			cmd.push_back ("-H");
		}

		str = dither_mode_combo.get_active_text();

		if (str == _("None")) {
		} else if (str == _("Triangular")) {
			cmd.push_back ("-z triangular");
		} else if (str == _("Rectangular")) {
			cmd.push_back ("-z rectangular");
		} else if (str == _("Shaped")) {
			cmd.push_back ("-z shaped");
		}

		if (force16bit_button.get_active()) {
			cmd.push_back ("-S");
		}

		if (soft_mode_button.get_active()) {
			cmd.push_back ("-s");
		}

		str = midi_driver_combo.get_active_text ();

		if (str == _("seq")) {
			cmd.push_back ("-X seq");
		} else if (str == _("raw")) {
			cmd.push_back ("-X raw");
		}
	} else if (using_coreaudio) {

#ifdef __APPLE__
		// note: older versions of the CoreAudio JACK backend use -n instead of -d here

		string device = get_device_name (driver, interface_combo.get_active_text());
		if (device.empty()) {
			cmd.clear ();
			return;
		}

		cmd.push_back ("-d");
		cmd.push_back (device);
#endif

	}
}

bool
EngineControl::engine_running ()
{
        EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
        boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;

        /* revert all environment settings back to whatever they were when ardour started
         */

        if (global_epa) {
                current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
                global_epa->restore ();
        }

	jack_status_t status;
	jack_client_t* c = jack_client_open ("ardourprobe", JackNoStartServer, &status);

	if (status == 0) {
		jack_client_close (c);
		return true;
	}
	return false;
}

int
EngineControl::setup_engine ()
{
	vector<string> args;
	std::string cwd = "/tmp";

	build_command_line (args);

	if (args.empty()) {
		return 1; // try again
	}

	std::string jackdrc_path = Glib::get_home_dir();
	jackdrc_path += "/.jackdrc";

	ofstream jackdrc (jackdrc_path.c_str());
	if (!jackdrc) {
		error << string_compose (_("cannot open JACK rc file %1 to store parameters"), jackdrc_path) << endmsg;
		return -1;
	}
	cerr << "JACK COMMAND: ";
	for (vector<string>::iterator i = args.begin(); i != args.end(); ++i) {
		cerr << (*i) << ' ';
		jackdrc << (*i) << ' ';
	}
	cerr << endl;
	jackdrc << endl;
	jackdrc.close ();

	_used = true;

	return 0;
}

void
EngineControl::realtime_changed ()
{
#ifndef __APPLE__
	priority_spinner.set_sensitive (realtime_button.get_active());
#endif
}

void
EngineControl::enumerate_devices (const string& driver)
{
	/* note: case matters for the map keys */

	if (driver == "CoreAudio") {
#ifdef __APPLE__
		devices[driver] = enumerate_coreaudio_devices ();
#endif

#ifndef __APPLE__
	} else if (driver == "ALSA") {
		devices[driver] = enumerate_alsa_devices ();
	} else if (driver == "FreeBOB") {
		devices[driver] = enumerate_freebob_devices ();
	} else if (driver == "FFADO") {
		devices[driver] = enumerate_ffado_devices ();
	} else if (driver == "OSS") {
		devices[driver] = enumerate_oss_devices ();
	} else if (driver == "Dummy") {
		devices[driver] = enumerate_dummy_devices ();
	} else if (driver == "NetJACK") {
		devices[driver] = enumerate_netjack_devices ();
	}
#else
        }
#endif
}

#ifdef __APPLE__
static OSStatus
getDeviceUIDFromID( AudioDeviceID id, char *name, size_t nsize)
{
	UInt32 size = sizeof(CFStringRef);
	CFStringRef UI;
	OSStatus res = AudioDeviceGetProperty(id, 0, false,
		kAudioDevicePropertyDeviceUID, &size, &UI);
	if (res == noErr)
		CFStringGetCString(UI,name,nsize,CFStringGetSystemEncoding());
	CFRelease(UI);
	return res;
}

vector<string>
EngineControl::enumerate_coreaudio_devices ()
{
	vector<string> devs;

	// Find out how many Core Audio devices are there, if any...
	// (code snippet gently "borrowed" from St?hane Letz jackdmp;)
	OSStatus err;
	Boolean isWritable;
	UInt32 outSize = sizeof(isWritable);

	backend_devs.clear ();

	err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,
					   &outSize, &isWritable);
	if (err == noErr) {
		// Calculate the number of device available...
		int numCoreDevices = outSize / sizeof(AudioDeviceID);
		// Make space for the devices we are about to get...
		AudioDeviceID *coreDeviceIDs = new AudioDeviceID [numCoreDevices];
		err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
					       &outSize, (void *) coreDeviceIDs);
		if (err == noErr) {
			// Look for the CoreAudio device name...
			char coreDeviceName[256];
			UInt32 nameSize;

			for (int i = 0; i < numCoreDevices; i++) {

				nameSize = sizeof (coreDeviceName);

				/* enforce duplex devices only */

				err = AudioDeviceGetPropertyInfo(coreDeviceIDs[i],
								 0, true, kAudioDevicePropertyStreams,
								 &outSize, &isWritable);

				if (err != noErr || outSize == 0) {
					continue;
				}

				err = AudioDeviceGetPropertyInfo(coreDeviceIDs[i],
								 0, false, kAudioDevicePropertyStreams,
								 &outSize, &isWritable);

				if (err != noErr || outSize == 0) {
					continue;
				}

				err = AudioDeviceGetPropertyInfo(coreDeviceIDs[i],
								 0, true, kAudioDevicePropertyDeviceName,
								 &outSize, &isWritable);
				if (err == noErr) {
					err = AudioDeviceGetProperty(coreDeviceIDs[i],
								     0, true, kAudioDevicePropertyDeviceName,
								     &nameSize, (void *) coreDeviceName);
					if (err == noErr) {
						char drivername[128];

						// this returns the unique id for the device
						// that must be used on the commandline for jack

						if (getDeviceUIDFromID(coreDeviceIDs[i], drivername, sizeof (drivername)) == noErr) {
							devs.push_back (coreDeviceName);
							backend_devs.push_back (drivername);
						}
					}
				}
			}
		}
		delete [] coreDeviceIDs;
	}


	if (devs.size() == 0) {
		MessageDialog msg (_("\
You do not have any audio devices capable of\n\
simultaneous playback and recording.\n\n\
Please use Applications -> Utilities -> Audio MIDI Setup\n\
to create an \"aggregrate\" device, or install a suitable\n\
audio interface.\n\n\
Please send email to Apple and ask them why new Macs\n\
have no duplex audio device.\n\n\
Alternatively, if you really want just playback\n\
or recording but not both, start JACK before running\n\
Ardour and choose the relevant device then."
					   ),
				   true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
		msg.set_title (_("No suitable audio devices"));
		msg.set_position (Gtk::WIN_POS_MOUSE);
		msg.run ();
		exit (1);
	}


	return devs;
}
#else
vector<string>
EngineControl::enumerate_alsa_devices ()
{
	vector<string> devs;

	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
	string devname;
	int cardnum = -1;
	int device = -1;

	backend_devs.clear ();

	while (snd_card_next (&cardnum) >= 0 && cardnum >= 0) {

		devname = "hw:";
		devname += to_string (cardnum, std::dec);

		if (snd_ctl_open (&handle, devname.c_str(), 0) >= 0 && snd_ctl_card_info (handle, info) >= 0) {

			while (snd_ctl_pcm_next_device (handle, &device) >= 0 && device >= 0) {

				snd_pcm_info_set_device (pcminfo, device);
				snd_pcm_info_set_subdevice (pcminfo, 0);
				snd_pcm_info_set_stream (pcminfo, SND_PCM_STREAM_PLAYBACK);

				if (snd_ctl_pcm_info (handle, pcminfo) >= 0) {
					devs.push_back (snd_pcm_info_get_name (pcminfo));
					devname += ',';
					devname += to_string (device, std::dec);
					backend_devs.push_back (devname);
				}
			}

			snd_ctl_close(handle);
		}
	}

	return devs;
}

vector<string>
EngineControl::enumerate_ffado_devices ()
{
	vector<string> devs;
	backend_devs.clear ();
	return devs;
}

vector<string>
EngineControl::enumerate_freebob_devices ()
{
	vector<string> devs;
	return devs;
}

vector<string>
EngineControl::enumerate_oss_devices ()
{
	vector<string> devs;
	return devs;
}
vector<string>
EngineControl::enumerate_dummy_devices ()
{
	vector<string> devs;
	return devs;
}
vector<string>
EngineControl::enumerate_netjack_devices ()
{
	vector<string> devs;
	return devs;
}
#endif

void
EngineControl::driver_changed ()
{
	string driver = driver_combo.get_active_text();
	string::size_type maxlen = 0;
	int n = 0;

	enumerate_devices (driver);

	vector<string>& strings = devices[driver];

	if (strings.empty() && driver != "FreeBoB" && driver != "FFADO" && driver != "Dummy") {
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

	if (!strings.empty()) {
		interface_combo.set_active_text (strings.front());
		input_device_combo.set_active_text (strings.front());
		output_device_combo.set_active_text (strings.front());
	}

	if (driver == "ALSA") {
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
	uint32_t rate = get_rate();
#ifdef __APPLE_
	float periods = 2;
#else
	float periods = periods_adjustment.get_value();
#endif
	float period_size = atof (period_size_combo.get_active_text());

	char buf[32];
	snprintf (buf, sizeof(buf), "%.1fmsec", (periods * period_size) / (rate/1000.0));

	latency_label.set_text (buf);
	latency_label.set_alignment (0, 0.5);
}

void
EngineControl::audio_mode_changed ()
{
	std::string str = audio_mode_combo.get_active_text();

	if (str == _("Playback/Recording on 1 Device")) {
		input_device_combo.set_sensitive (false);
		output_device_combo.set_sensitive (false);
	} else if (str == _("Playback/Recording on 2 Devices")) {
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

static bool jack_server_filter(const string& str, void */*arg*/)
{
   return str == "jackd" || str == "jackdmp";
}

void
EngineControl::find_jack_servers (vector<string>& strings)
{
#ifdef __APPLE__
	/* this magic lets us finds the path to the OSX bundle, and then
	   we infer JACK's location from there
	*/

	char execpath[MAXPATHLEN+1];
	uint32_t pathsz = sizeof (execpath);

	_NSGetExecutablePath (execpath, &pathsz);

	string path (Glib::path_get_dirname (execpath));
	path += "/jackd";

	if (Glib::file_test (path, FILE_TEST_EXISTS)) {
		strings.push_back (path);
	}

	if (getenv ("ARDOUR_WITH_JACK")) {
		/* no other options - only use the JACK we supply */
		if (strings.empty()) {
			fatal << string_compose (_("JACK appears to be missing from the %1 bundle"), PROGRAM_NAME) << endmsg;
			/*NOTREACHED*/
		}
		return;
	}
#else
	string path;
#endif

	PathScanner scanner;
	vector<string *> *jack_servers;
	std::map<string,int> un;
	char *p;
	bool need_minimal_path = false;

	p = getenv ("PATH");

	if (p && *p) {
		path = p;
	} else {
		need_minimal_path = true;
	}

#ifdef __APPLE__
	// many mac users don't have PATH set up to include
	// likely installed locations of JACK
	need_minimal_path = true;
#endif

	if (need_minimal_path) {
		if (path.empty()) {
			path = "/usr/bin:/bin:/usr/local/bin:/opt/local/bin";
		} else {
			path += ":/usr/local/bin:/opt/local/bin";
		}
	}

#ifdef __APPLE__
	// push it back into the environment so that auto-started JACK can find it.
	// XXX why can't we just expect OS X users to have PATH set correctly? we can't ...
	setenv ("PATH", path.c_str(), 1);
#endif

	jack_servers = scanner (path, jack_server_filter, 0, false, true);

	vector<string *>::iterator iter;

	for (iter = jack_servers->begin(); iter != jack_servers->end(); iter++) {
		string p = **iter;

		if (un[p]++ == 0) {
			strings.push_back(p);
		}
	}
}


string
EngineControl::get_device_name (const string& driver, const string& human_readable)
{
	vector<string>::iterator n;
	vector<string>::iterator i;

	if (human_readable.empty()) {
		/* this can happen if the user's .ardourrc file has a device name from
		   another computer system in it
		*/
		MessageDialog msg (_("You need to choose an audio device first."));
		msg.run ();
		return string();
	}

	if (backend_devs.empty()) {
		return human_readable;
	}

	for (i = devices[driver].begin(), n = backend_devs.begin(); i != devices[driver].end(); ++i, ++n) {
		if (human_readable == (*i)) {
			return (*n);
		}
	}

	if (i == devices[driver].end()) {
		warning << string_compose (_("Audio device \"%1\" not known on this computer."), human_readable) << endmsg;
	}

	return string();
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

	child = new XMLNode ("priority");
	child->add_property ("val", to_string (priority_adjustment.get_value(), std::dec));
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
		} else if (child->name() == "priority") {
			val = atoi (strval);
			priority_adjustment.set_value(val);
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
