#include <vector>
#include <cmath>
#include <fstream>

#include <glibmm.h>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CFString.h>
#endif __APPLE__

#include <ardour/profile.h>
#include <jack/jack.h>

#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include <pbd/convert.h>
#include <pbd/error.h>

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
	  basic_packer (8, 2),
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
#ifdef __APPLE__
	strings.push_back (X_("CoreAudio"));
#else
	strings.push_back (X_("ALSA"));
	strings.push_back (X_("OSS"));
	strings.push_back (X_("FFADO"));
#endif
	strings.push_back (X_("NetJACK"));
	strings.push_back (X_("Dummy"));
	set_popdown_strings (driver_combo, strings);
	driver_combo.set_active_text (strings.front());

	/* figure out available devices and set up interface_combo */

	enumerate_devices ();
	driver_combo.signal_changed().connect (mem_fun (*this, &EngineControl::driver_changed));
	driver_changed ();

	strings.clear ();
	strings.push_back (_("Duplex"));
	strings.push_back (_("Playback only"));
	strings.push_back (_("Capture only"));
	set_popdown_strings (audio_mode_combo, strings);
	audio_mode_combo.set_active_text (strings.front());

	audio_mode_combo.signal_changed().connect (mem_fun (*this, &EngineControl::audio_mode_changed));
	audio_mode_changed ();

	label = manage (new Label (_("Driver")));
	basic_packer.attach (*label, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (driver_combo, 1, 2, 0, 1, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Interface")));
	basic_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (interface_combo, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Sample Rate")));
	basic_packer.attach (*label, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, 2, 3, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Buffer size")));
	basic_packer.attach (*label, 0, 1, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (period_size_combo, 1, 2, 3, 4, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Number of buffers")));
	basic_packer.attach (*label, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (periods_spinner, 1, 2, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	periods_spinner.set_value (2);

	label = manage (new Label (_("Approximate latency")));
	basic_packer.attach (*label, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (latency_label, 1, 2, 5, 6, FILL|EXPAND, (AttachOptions) 0);

	sample_rate_combo.signal_changed().connect (mem_fun (*this, &EngineControl::redisplay_latency));
	periods_adjustment.signal_value_changed().connect (mem_fun (*this, &EngineControl::redisplay_latency));
	period_size_combo.signal_changed().connect (mem_fun (*this, &EngineControl::redisplay_latency));
	redisplay_latency();

	label = manage (new Label (_("Audio Mode")));
	basic_packer.attach (*label, 0, 1, 6, 7, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (audio_mode_combo, 1, 2, 6, 7, FILL|EXPAND, (AttachOptions) 0);

	/* 

	if (engine_running()) {
		start_button.set_sensitive (false);
	} else {
		stop_button.set_sensitive (false);
	}

	start_button.signal_clicked().connect (mem_fun (*this, &EngineControl::start_engine));
	stop_button.signal_clicked().connect (mem_fun (*this, &EngineControl::start_engine));
	*/

	button_box.pack_start (start_button, false, false);
	button_box.pack_start (stop_button, false, false);

	// basic_packer.attach (button_box, 0, 2, 8, 9, FILL|EXPAND, (AttachOptions) 0);

	/* options */

	options_packer.attach (realtime_button, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Realtime Priority")));
	options_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (priority_spinner, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	priority_spinner.set_value (60);

	realtime_button.signal_toggled().connect (mem_fun (*this, &EngineControl::realtime_changed));
	realtime_changed ();

#ifndef __APPLE__
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

	find_jack_servers (strings);

	if (strings.empty()) {
		fatal << _("No JACK server found anywhere on this system. Please install JACK and restart") << endmsg;
		/*NOTREACHED*/
	}
	
	set_popdown_strings (serverpath_combo, strings);
	serverpath_combo.set_active_text (strings.front());

	cerr << "we have " << strings.size() << " possible Jack servers\n";

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
	label = manage (new Label (_("Input latency (samples)")));
	device_packer.attach (*label, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_latency, 1, 2, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output latency (samples)")));
	device_packer.attach (*label, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_latency, 1, 2, 5, 6, FILL|EXPAND, (AttachOptions) 0);

	notebook.pages().push_back (TabElem (basic_packer, _("Basics")));
	notebook.pages().push_back (TabElem (options_packer, _("Options")));
	notebook.pages().push_back (TabElem (device_packer, _("Device Parameters")));

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

	driver = driver_combo.get_active_text ();
	if (driver == X_("ALSA")) {
		using_alsa = true;
		cmd.push_back ("alsa");
	} else if (driver == X_("OSS")) {
		using_oss = true;
		cmd.push_back ("oss");
	} else if (driver == X_("CoreAudio")) {
		using_coreaudio = true;
		cmd.push_back ("coreaudio");
	} else if (driver == X_("NetJACK")) {
		using_netjack = true;
		cmd.push_back ("netjack");
	} else if (driver == X_("FFADO")) {
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

	if (!using_coreaudio) {
		cmd.push_back ("-n");
		cmd.push_back (to_string ((uint32_t) floor (periods_spinner.get_value()), std::dec));
	}

	cmd.push_back ("-r");
	cmd.push_back (to_string (get_rate(), std::dec));
	
	cmd.push_back ("-p");
	cmd.push_back (period_size_combo.get_active_text());

	if (using_alsa) {

		cmd.push_back ("-d");
		cmd.push_back (interface_combo.get_active_text());

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

#ifdef __APPLE__
		cmd.push_back ("-n");

		Glib::ustring str = interface_combo.get_active_text();
		vector<string>::iterator n;
		vector<string>::iterator i;
		
		for (i = devices[driver].begin(), n = coreaudio_devs.begin(); i != devices[driver].end(); ++i, ++n) {
			if (str == (*i)) {
				cerr << "for " << str << " use " << (*n) << endl;
				cmd.push_back (*n);
				break;
			}
		}
			
		if (i == devices[driver].end()) {
			fatal << string_compose (_("programming error: %1"), "coreaudio device ID missing") << endmsg;
			/*NOTREACHED*/
		}
#endif

	} else if (using_oss) {

	} else if (using_netjack) {

	}
}

bool
EngineControl::engine_running ()
{
	jack_status_t status;
	jack_client_t* c = jack_client_open ("ardourprobe", JackNoStartServer, &status);

	if (status == 0) {
		jack_client_close (c);
		return true;
	}
	return false;
}

int
EngineControl::start_engine ()
{
	vector<string> args;
	std::string cwd = "/tmp";
	int ret = 0;

	build_command_line (args);

	Glib::ustring jackdrc_path = Glib::get_home_dir();
	jackdrc_path += "/.jackdrc";

	ofstream jackdrc (jackdrc_path.c_str());
	if (!jackdrc) {
		error << string_compose (_("cannot open JACK rc file %1 to store parameters"), jackdrc_path) << endmsg;
		return -1;
	}

	cerr << "will execute ...\n";
	for (vector<string>::iterator i = args.begin(); i != args.end(); ++i) {
		jackdrc << (*i) << ' ';
		cerr << (*i) << ' ';
	}
	jackdrc << endl;
	cerr << endl;
	jackdrc.close ();
	
#if 0

	try {
		spawn_async_with_pipes (cwd, args, SpawnFlags (0), sigc::slot<void>(), &engine_pid, &engine_stdin, &engine_stdout, &engine_stderr);
	}
	
	catch (Glib::Exception& err) {
		error << _("could not start JACK server: ") << err.what() << endmsg;
		ret = -1;
	}
#endif

	return ret;
}

int
EngineControl::stop_engine ()
{
	close (engine_stdin);
	close (engine_stderr);
	close (engine_stdout);
	spawn_close_pid (engine_pid);
	return 0;
}

void
EngineControl::realtime_changed ()
{
	priority_spinner.set_sensitive (realtime_button.get_active());
}

void
EngineControl::enumerate_devices ()
{
	/* note: case matters for the map keys */

#ifdef __APPLE__
	devices["CoreAudio"] = enumerate_coreaudio_devices ();
#else
	devices["ALSA"] = enumerate_alsa_devices ();
	devices["FFADO"] = enumerate_ffado_devices ();
	devices["OSS"] = enumerate_oss_devices ();
	devices["Dummy"] = enumerate_dummy_devices ();
	devices["NetJACK"] = enumerate_netjack_devices ();
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
	size_t outSize = sizeof(isWritable);

	coreaudio_devs.clear ();

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
			size_t nameSize = sizeof (coreDeviceName);
			for (int i = 0; i < numCoreDevices; i++) {
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
							coreaudio_devs.push_back (drivername);
						} 
					}
				}
			}
		}
		delete [] coreDeviceIDs;
	}

	return devs;
}
#else
vector<string>
EngineControl::enumerate_alsa_devices ()
{
	vector<string> devs;
	devs.push_back ("hw:0");
	devs.push_back ("hw:1");
	devs.push_back ("plughw:0");
	devs.push_back ("plughw:1");
	return devs;
}
vector<string>
EngineControl::enumerate_ffado_devices ()
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

uint32_t
EngineControl::get_rate ()
{
	return atoi (sample_rate_combo.get_active_text ());
}

void
EngineControl::redisplay_latency ()
{
	uint32_t rate = get_rate();
	float periods = periods_adjustment.get_value();
	float period_size = atof (period_size_combo.get_active_text());

	char buf[32];
	snprintf (buf, sizeof(buf), "%.1fmsec", (periods * period_size) / (rate/1000.0));

	latency_label.set_text (buf);
}

void
EngineControl::audio_mode_changed ()
{
	Glib::ustring str = audio_mode_combo.get_active_text();

	if (str == _("Duplex")) {
		input_device_combo.set_sensitive (false);
		output_device_combo.set_sensitive (false);
	} else {
		input_device_combo.set_sensitive (true);
		output_device_combo.set_sensitive (true);
	}
}

void
EngineControl::find_jack_servers (vector<string>& strings)
{
#ifdef __APPLE
	if (Profile->get_single_package()) {

		/* this magic lets us finds the path to the OSX bundle, and then
		   we infer JACK's location from there
		*/
		
		CFURLRef pluginRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
		CFStringRef macPath = CFURLCopyFileSystemPath(pluginRef, 
							      kCFURLPOSIXPathStyle);
		std::string path = CFStringGetCStringPtr(macPath, 
							 CFStringGetSystemEncoding());
		CFRelease(pluginRef);
		CFRelease(macPath);
		
		path += '/jackd';

		if (Glib::file_test (path, FILE_TEST_EXISTS)) {
			strings.push_back ();
		} else {
			warning << _("JACK appears to be missing from the Ardour bundle") << endmsg;
		}
#endif

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

}
