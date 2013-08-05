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

#include <glibmm.h>
#include <gtkmm/messagedialog.h>

#include "pbd/error.h"
#include "pbd/xml++.h"

#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/rc_configuration.h"

#include "pbd/convert.h"
#include "pbd/error.h"

#include "engine_dialog.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

EngineControl::EngineControl ()
	: input_latency_adjustment (0, 0, 99999, 1)
	, input_latency (input_latency_adjustment)
	, output_latency_adjustment (0, 0, 99999, 1)
	, output_latency (output_latency_adjustment)
	, input_channels_adjustment (2, 0, 256, 1)
	, input_channels (input_channels_adjustment)
	, output_channels_adjustment (2, 0, 256, 1)
	, output_channels (output_channels_adjustment)
	, ports_adjustment (128, 8, 1024, 1, 16)
	, ports_spinner (ports_adjustment)
	, realtime_button (_("Realtime"))
#ifdef __APPLE___
	, basic_packer (6, 2)
	, options_packer (4, 2)
	, device_packer (4, 2)
#else
	, basic_packer (9, 2)
	, options_packer (14, 2)
	, device_packer (6, 2)
#endif
{
	using namespace Notebook_Helpers;
	Label* label;
	vector<string> strings;
	int row = 0;

	_used = false;

	/* basic parameters */

	basic_packer.set_spacings (6);

	strings.clear ();

	vector<const ARDOUR::AudioBackendInfo*> backends = ARDOUR::AudioEngine::instance()->available_backends();
	for (vector<const ARDOUR::AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		strings.push_back ((*b)->name);
	}

	set_popdown_strings (backend_combo, strings);
	backend_combo.set_active_text (strings.front());

	backend_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::backend_changed));
	backend_changed ();

	driver_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::driver_changed));

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
#ifdef __APPLE__
	strings.push_back (_("coremidi"));
#else
	strings.push_back (_("seq"));
	strings.push_back (_("raw"));
#endif
	set_popdown_strings (midi_driver_combo, strings);
	midi_driver_combo.set_active_text (strings.front ());

	row = 0;

	label = manage (left_aligned_label (_("Audio Driver:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (backend_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

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
	basic_packer.attach (buffer_size_combo, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	label = manage (left_aligned_label (_("Approximate latency:")));
	basic_packer.attach (*label, 0, 1, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (latency_label, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	row++;

	sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
	buffer_size_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::redisplay_latency));
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

	interface_combo.signal_changed().connect (sigc::mem_fun (*this, &EngineControl::interface_changed));

	/* options */

	options_packer.set_spacings (6);
	row = 0;

	options_packer.attach (realtime_button, 1, 2, row, row + 1, FILL|EXPAND, (AttachOptions) 0);
	++row;

	realtime_button.set_active (true);

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

void
EngineControl::backend_changed ()
{
	string backend_name = backend_combo.get_active_text();
	boost::shared_ptr<ARDOUR::AudioBackend> backend;

	if (!(backend = ARDOUR::AudioEngine::instance()->set_backend (backend_name, "ardour", ""))) {
		/* eh? */
		return;
	}

	if (backend->requires_driver_selection()) {
		vector<string> drivers = backend->enumerate_drivers();
		set_popdown_strings (driver_combo, drivers);
		driver_combo.set_active_text (drivers.front());
		driver_changed ();
	} else {
		list_devices ();
	}
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

	set_popdown_strings (interface_combo, available_devices);
	interface_combo.set_active_text (available_devices.front());
	set_popdown_strings (input_device_combo, available_devices);
	input_device_combo.set_active_text (available_devices.front());
	set_popdown_strings (output_device_combo, available_devices);
	output_device_combo.set_active_text (available_devices.front());
	
	interface_changed ();
}
	
void
EngineControl::driver_changed ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	backend->set_driver (driver_combo.get_active_text());
	list_devices ();
}

void
EngineControl::interface_changed ()
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);
	string device_name = interface_combo.get_active_text ();
	vector<string> s;

	/* sample rates */

	vector<float> sr = backend->available_sample_rates (device_name);
	for (vector<float>::const_iterator x = sr.begin(); x != sr.end(); ++x) {
		char buf[32];
		if (fmod (*x, 1000.0f)) {
			snprintf (buf, sizeof (buf), "%.1f kHz", (*x)/1000.0);
		} else {
			snprintf (buf, sizeof (buf), "%.0f kHz", (*x)/1000.0);
		}
		s.push_back (buf);
	}

	set_popdown_strings (sample_rate_combo, s);
	sample_rate_combo.set_active_text (s.front());

	/* buffer sizes */

	s.clear ();
	vector<uint32_t> bs = backend->available_buffer_sizes(device_name);
	for (vector<uint32_t>::const_iterator x = bs.begin(); x != bs.end(); ++x) {
		char buf[32];
		snprintf (buf, sizeof (buf), "%u", *x);
		s.push_back (buf);
	}

	set_popdown_strings (buffer_size_combo, s);
	buffer_size_combo.set_active_text (s.front());
}	

void
EngineControl::redisplay_latency ()
{
	uint32_t rate = get_rate();
	float period_size = atof (buffer_size_combo.get_active_text());

	char buf[32];
	snprintf (buf, sizeof(buf), "%.1fmsec", (2 * period_size) / (rate/1000.0));

	latency_label.set_text (buf);
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

XMLNode&
EngineControl::get_state ()
{
	XMLNode* root = new XMLNode ("AudioSetup");
	XMLNode* child;
	std::string path;

#if 0
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
#endif
	return *root;
}

void
EngineControl::set_state (const XMLNode& root)
{
#if 0
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
#endif
}

int
EngineControl::setup_engine (bool start)
{
	boost::shared_ptr<ARDOUR::AudioBackend> backend = ARDOUR::AudioEngine::instance()->current_backend();
	assert (backend);

	/* grab the parameters from the GUI and apply them */

	try {
		if (backend->requires_driver_selection()) {
			if (backend->set_driver (get_driver())) {
				return -1;
			}
		}

		if (backend->set_device_name (get_device_name())) {
			return -1;
		}

		if (backend->set_sample_rate (get_rate())) {
			error << string_compose (_("Cannot set sample rate to %1"), get_rate()) << endmsg;
			return -1;
		}
		if (backend->set_buffer_size (get_buffer_size())) {
			error << string_compose (_("Cannot set buffer size to %1"), get_buffer_size()) << endmsg;
			return -1;
		}
		if (backend->set_input_channels (get_input_channels())) {
			error << string_compose (_("Cannot set input channels to %1"), get_input_channels()) << endmsg;
			return -1;
		}
		if (backend->set_output_channels (get_output_channels())) {
			error << string_compose (_("Cannot set output channels to %1"), get_output_channels()) << endmsg;
			return -1;
		}
		if (backend->set_systemic_input_latency (get_input_latency())) {
			error << string_compose (_("Cannot set input latency to %1"), get_input_latency()) << endmsg;
			return -1;
		}
		if (backend->set_systemic_output_latency (get_output_latency())) {
			error << string_compose (_("Cannot set output latency to %1"), get_output_latency()) << endmsg;
			return -1;
		}

		if (start) {
			return ARDOUR::AudioEngine::instance()->start();
		}

		return 0;

	} catch (...) {
		cerr << "exception thrown...\n";
		return -1;
	}
}

uint32_t
EngineControl::get_rate () const
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

uint32_t
EngineControl::get_buffer_size () const
{
	string txt = buffer_size_combo.get_active_text ();
	uint32_t samples;

	if (sscanf (txt.c_str(), "%d", &samples) != 1) {
		throw exception ();
	}

	return samples;
}

uint32_t
EngineControl::get_input_channels() const
{
	return (uint32_t) input_channels_adjustment.get_value();
}

uint32_t
EngineControl::get_output_channels() const
{
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
EngineControl::get_driver () const
{
	return driver_combo.get_active_text ();
}

string
EngineControl::get_device_name () const
{
	return interface_combo.get_active_text ();
}

