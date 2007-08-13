#include <vector>

#include <glibmm.h>

#include <jack/jack.h>

#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include <pbd/convert.h>

#include "engine_dialog.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

EngineDialog::EngineDialog ()
	: ArdourDialog (_("Audio Engine"), false, true),
	  periods_adjustment (2, 2, 16, 1, 2),
	  periods_spinner (periods_adjustment),
	  priority_adjustment (60, 10, 90, 1, 10),
	  priority_spinner (priority_adjustment),
	  ports_adjustment (128, 8, 1024, 1, 16),
	  ports_spinner (ports_adjustment),
	  realtime_button (_("Realtime")),
	  no_memory_lock_button (_("Do not lock memory")),
	  unlock_memory_button (_("Unlock memory")),
	  soft_mode_button (_("No zombies")),
	  monitor_button (_("Provide monitor ports")),
	  force16bit_button (_("Force 16 bit")),
	  hw_monitor_button (_("H/W monitoring")),
	  hw_meter_button (_("H/W metering")),
	  verbose_output_button (_("Verbose output")),
	  basic_packer (5, 2),
	  options_packer (12, 2),
	  device_packer (3, 2)
{
	using namespace Notebook_Helpers;
	Label* label;
	vector<string> strings;

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

	/* basic parameters */

	basic_packer.set_spacings (6);

	strings.clear ();
#ifndef __APPLE
	strings.push_back (X_("ALSA"));
	strings.push_back (X_("OSS"));
	strings.push_back (X_("FFADO"));
#else
	strings.push_back (X_("CoreAudio"));
#endif
	strings.push_back (X_("NetJACK"));
	strings.push_back (X_("Dummy"));
	set_popdown_strings (driver_combo, strings);
	driver_combo.set_active_text (strings.front());

	/* figure out available devices and set up interface_combo */

	enumerate_devices ();
	driver_combo.signal_changed().connect (mem_fun (*this, &EngineDialog::driver_changed));
	driver_changed ();

	strings.clear ();
	strings.push_back (_("Duplex"));
	strings.push_back (_("Playback only"));
	strings.push_back (_("Capture only"));
	set_popdown_strings (audio_mode_combo, strings);
	audio_mode_combo.set_active_text (strings.front());

	label = manage (new Label (_("Driver")));
	basic_packer.attach (*label, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (driver_combo, 1, 2, 0, 1, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Interface")));
	basic_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (interface_combo, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Sample Rate")));
	basic_packer.attach (*label, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, 2, 3, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Period Size")));
	basic_packer.attach (*label, 0, 1, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (period_size_combo, 1, 2, 3, 4, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Number of periods")));
	basic_packer.attach (*label, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (periods_spinner, 1, 2, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	periods_spinner.set_value (2);

	label = manage (new Label (_("Audio Mode")));
	basic_packer.attach (*label, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (audio_mode_combo, 1, 2, 5, 6, FILL|EXPAND, (AttachOptions) 0);

	/* options */

	options_packer.attach (realtime_button, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Realtime Priority")));
	options_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (priority_spinner, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	priority_spinner.set_value (60);

	realtime_button.signal_toggled().connect (mem_fun (*this, &EngineDialog::realtime_changed));
	realtime_changed ();

#ifndef __APPLE
	options_packer.attach (no_memory_lock_button, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (unlock_memory_button, 0, 1, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (soft_mode_button, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (monitor_button, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (force16bit_button, 0, 1, 6, 7, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (hw_monitor_button, 0, 1, 7, 8, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (hw_meter_button, 0, 1, 8, 9, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (verbose_output_button, 0, 1, 9, 10, FILL|EXPAND, (AttachOptions) 0);
#else 
	options_packer.attach (verbose_output_button, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
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
	options_packer.attach (*label, 0, 1, 11, 12, (AttachOptions) 0, (AttachOptions) 0);
	options_packer.attach (timeout_combo, 1, 2, 11, 12, FILL|EXPAND, AttachOptions(0));

	label = manage (new Label (_("Number of ports")));
	options_packer.attach (*label, 0, 1, 12, 13, (AttachOptions) 0, (AttachOptions) 0);
	options_packer.attach (ports_spinner, 1, 2, 12, 13, FILL|EXPAND, AttachOptions(0));

	strings.clear ();

	if (Glib::file_test ("/usr/bin/jackd", FILE_TEST_EXISTS)) {
		strings.push_back ("/usr/bin/jackd");
	}
	if (Glib::file_test ("/usr/local/bin/jackd", FILE_TEST_EXISTS)) {
		strings.push_back ("/usr/local/bin/jackd");
	}
	if (Glib::file_test ("/opt/bin/jackd", FILE_TEST_EXISTS)) {
		strings.push_back ("/opt/bin/jackd");
	}
	if (Glib::file_test ("/usr/bin/jackdmp", FILE_TEST_EXISTS)) {
		strings.push_back ("/usr/bin/jackd");
	}
	if (Glib::file_test ("/usr/local/bin/jackdmp", FILE_TEST_EXISTS)) {
		strings.push_back ("/usr/local/bin/jackd");
	}
	if (Glib::file_test ("/opt/bin/jackdmp", FILE_TEST_EXISTS)) {
		strings.push_back ("/opt/bin/jackd");
	}

	if (strings.empty()) {
		fatal << _("No JACK server found anywhere on this system. Please install JACK and restart") << endmsg;
		/*NOTREACHED*/
	}
	
	set_popdown_strings (serverpath_combo, strings);
	serverpath_combo.set_active_text (strings.front());

	if (strings.size() > 1) {
		label = manage (new Label (_("Server:")));
		options_packer.attach (*label, 0, 1, 11, 12, (AttachOptions) 0, (AttachOptions) 0);
		options_packer.attach (serverpath_combo, 1, 2, 11, 12, FILL|EXPAND, (AttachOptions) 0);
	}

	/* device settings */

	device_packer.set_spacings (6);

	label = manage (new Label (_("Input device")));
	device_packer.attach (*label, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_device_combo, 1, 2, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output device")));
	device_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_device_combo, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);	
	label = manage (new Label (_("Input channels")));
	device_packer.attach (*label, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_channels, 1, 2, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output channels")));
	device_packer.attach (*label, 0, 1, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_channels, 1, 2, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Input latency")));
	device_packer.attach (*label, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_latency, 1, 2, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output latency")));
	device_packer.attach (*label, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_latency, 1, 2, 5, 6, FILL|EXPAND, (AttachOptions) 0);

	notebook.pages().push_back (TabElem (basic_packer, _("Parameters")));
	notebook.pages().push_back (TabElem (options_packer, _("Options")));
	notebook.pages().push_back (TabElem (device_packer, _("Device")));

	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (notebook);

	add_button (Stock::OK, RESPONSE_ACCEPT);
	start_button = add_button (_("Start"), RESPONSE_YES);
	stop_button = add_button (_("Stop"), RESPONSE_NO);

	if (engine_running()) {
		start_button->set_sensitive (false);
	} else {
		stop_button->set_sensitive (false);
	}

	start_button->signal_clicked().connect (mem_fun (*this, &EngineDialog::start_engine));
	stop_button->signal_clicked().connect (mem_fun (*this, &EngineDialog::start_engine));

}

EngineDialog::~EngineDialog ()
{

}

void
EngineDialog::build_command_line (vector<string>& cmd)
{
	string str;
	bool using_oss = false;
	bool using_alsa = false;
	bool using_coreaudio = false;
	bool using_netjack = false;
	bool using_ffado = false;

	/* first, path to jackd */

	cmd.push_back (serverpath_combo.get_active_text ());
	
	/* now jackd arguments */

	str = timeout_combo.get_active_text ();
	if (str != _("Ignore")) {
		double secs;
		uint32_t msecs;
		atof (str);
		msecs = (uint32_t) floor (secs * 1000.0);
		cmd.push_back ("-t");
		cmd.push_back (to_string (msecs, std::dec));
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

	str = driver_combo.get_active_text ();
	if (str == X_("ALSA")) {
		using_alsa = true;
		cmd.push_back ("alsa");
	} else if (str == X_("OSS")) {
		using_oss = true;
		cmd.push_back ("oss");
	} else if (str == X_("CoreAudio")) {
		using_coreaudio = true;
		cmd.push_back ("coreaudio");
	} else if (str == X_("NetJACK")) {
		using_netjack = true;
		cmd.push_back ("netjack");
	} else if (str == X_("FFADO")) {
		using_ffado = true;
		cmd.push_back ("ffado");
	}

	/* driver arguments */

	str = audio_mode_combo.get_active_text();
	if (str == _("Duplex")) {
		/* relax */
	} else if (str == _("Playback only")) {
		cmd.push_back ("-P");
	} else if (str == _("Capture only")) {
		cmd.push_back ("-C");
	}

	cmd.push_back ("-n");
	cmd.push_back (to_string ((uint32_t) floor (periods_spinner.get_value()), std::dec));

	cmd.push_back ("-r");
	/* rate string has "Hz" on the end of it */
	uint32_t rate = atoi (sample_rate_combo.get_active_text ());
	cmd.push_back (to_string (rate, std::dec));
	
	cmd.push_back ("-p");
	cmd.push_back (period_size_combo.get_active_text());

	if (using_alsa) {

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

	} else if (using_coreaudio) {

		cmd.push_back ("-I");
		cmd.push_back (interface_combo.get_active_text());

	} else if (using_oss) {

	} else if (using_netjack) {

	}
}

bool
EngineDialog::engine_running ()
{
	jack_status_t status;
	jack_client_t* c = jack_client_open ("ardourprobe", JackNoStartServer, &status);

	if (status == 0) {
		jack_client_close (c);
		return true;
	}
	return false;
}

void
EngineDialog::start_engine ()
{
	vector<string> args;
	std::string cwd;

	build_command_line (args);

	cerr << "will execute:\n";
	for (vector<string>::iterator i = args.begin(); i != args.end(); ++i) {
		cerr << (*i) << ' ';
	}
	cerr << endl;

	try {
		// spawn_async_with_pipes (cwd, args, SpawnFlags (0), sigc::slot<void>(), &engine_pid, &engine_stdin, &engine_stdout, &engine_stderr);
	}
	
	catch (Glib::Exception& err) {
		cerr << "spawn failed\n";
	}
}

void
EngineDialog::stop_engine ()
{
	spawn_close_pid (engine_pid);
}

void
EngineDialog::realtime_changed ()
{
	priority_spinner.set_sensitive (realtime_button.get_active());
}

void
EngineDialog::enumerate_devices ()
{
	/* note: case matters for the map keys */

#ifdef __APPLE
	devices["CoreAudio"] = enumerate_coreaudio_devices ();
#else
	devices["ALSA"] = enumerate_alsa_devices ();
	devices["FFADO"] = enumerate_ffado_devices ();
	devices["OSS"] = enumerate_oss_devices ();
	devices["Dummy"] = enumerate_dummy_devices ();
	devices["NetJACK"] = enumerate_netjack_devices ();
#endif
}

#ifdef __APPLE
vector<string>
EngineDialog::enumerate_coreaudio_devices ()
{
	vector<string> devs;
	return devs;
}
#else
vector<string>
EngineDialog::enumerate_alsa_devices ()
{
	vector<string> devs;
	devs.push_back ("hw:0");
	devs.push_back ("hw:1");
	devs.push_back ("plughw:0");
	devs.push_back ("plughw:1");
	return devs;
}
vector<string>
EngineDialog::enumerate_ffado_devices ()
{
	vector<string> devs;
	return devs;
}
vector<string>
EngineDialog::enumerate_oss_devices ()
{
	vector<string> devs;
	return devs;
}
vector<string>
EngineDialog::enumerate_dummy_devices ()
{
	vector<string> devs;
	return devs;
}
vector<string>
EngineDialog::enumerate_netjack_devices ()
{
	vector<string> devs;
	return devs;
}
#endif

void
EngineDialog::driver_changed ()
{
	string driver = driver_combo.get_active_text();
	vector<string>& strings = devices[driver];
	
	set_popdown_strings (interface_combo, strings);

	if (!strings.empty()) {
		interface_combo.set_active_text (strings.front());
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
