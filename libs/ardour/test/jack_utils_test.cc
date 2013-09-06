
#include <stdexcept>

#ifdef PLATFORM_WINDOWS
#include <windows.h> // only for Sleep
#endif

#include <glibmm/miscutils.h>

#include "ardour/jack_utils.h"

#include "jack_utils_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (JackUtilsTest);

using namespace std;
using namespace ARDOUR;

void
JackUtilsTest::test_driver_names ()
{
	vector<string> driver_names;

	get_jack_audio_driver_names (driver_names);

	CPPUNIT_ASSERT(!driver_names.empty());

	cout << endl;
	cout << "Number of possible JACK Audio drivers found on this system: " << driver_names.size () << endl;

	for (vector<string>::const_iterator i = driver_names.begin(); i != driver_names.end(); ++i) {
		cout << "JACK Audio driver found: " << *i << endl;
	}

	string default_audio_driver;
	get_jack_default_audio_driver_name (default_audio_driver);

	cout << "The default audio driver on this system is: " << default_audio_driver << endl;

	driver_names.clear();

	get_jack_midi_system_names (default_audio_driver, driver_names);

	CPPUNIT_ASSERT(!driver_names.empty());

	cout << "Number of possible JACK MIDI drivers found on this system for default audio driver: " << driver_names.size () << endl;

	for (vector<string>::const_iterator i = driver_names.begin(); i != driver_names.end(); ++i) {
		cout << "JACK MIDI driver found: " << *i << endl;
	}

	string default_midi_driver;
	get_jack_default_midi_system_name (default_audio_driver, default_midi_driver);

	cout << "The default midi driver on this system is: " << default_midi_driver << endl;
}

string
devices_string (const vector<string>& devices)
{
	std::string str;
	for (vector<string>::const_iterator i = devices.begin(); i != devices.end();) {
		str += *i;
		if (++i != devices.end()) str += ", ";
	}
	return str;
}

void
JackUtilsTest::test_device_names ()
{
	vector<string> driver_names;

	get_jack_audio_driver_names (driver_names);

	CPPUNIT_ASSERT(!driver_names.empty());

	cout << endl;

	for (vector<string>::const_iterator i = driver_names.begin(); i != driver_names.end(); ++i) {
		string devices = devices_string (get_jack_device_names_for_audio_driver (*i));
		cout << "JACK Audio driver found: " << *i << " with devices: " << devices << endl;
	}
}

void
JackUtilsTest::test_samplerates ()
{
	vector<string> samplerates;

	get_jack_sample_rate_strings (samplerates);
	cout << endl;
	cout << "Number of possible Samplerates supported by JACK: " << samplerates.size () << endl;

	for (vector<string>::const_iterator i = samplerates.begin(); i != samplerates.end(); ++i) {
		cout << "Samplerate: " << *i << endl;
	}
}

void
JackUtilsTest::test_period_sizes ()
{
	vector<string> period_sizes;

	get_jack_period_size_strings (period_sizes);
	cout << endl;
	cout << "Number of possible Period sizes supported by JACK: " << period_sizes.size () << endl;

	for (vector<string>::const_iterator i = period_sizes.begin(); i != period_sizes.end(); ++i) {
		cout << "Period size: " << *i << endl;
	}
}

void
JackUtilsTest::test_dither_modes ()
{
	vector<string> driver_names;

	get_jack_audio_driver_names (driver_names);

	CPPUNIT_ASSERT(!driver_names.empty());

	cout << endl;

	for (vector<string>::const_iterator i = driver_names.begin(); i != driver_names.end(); ++i) {
		vector<string> dither_modes;

		get_jack_dither_mode_strings (*i, dither_modes);
		cout << "Number of possible Dither Modes supported by JACK driver " << *i <<
			": " << dither_modes.size () << endl;
		for (vector<string>::const_iterator j = dither_modes.begin(); j != dither_modes.end(); ++j) {
			cout << "Dither Mode: " << *j << endl;
		}
		cout << endl;
	}

}

void
JackUtilsTest::test_connect_server ()
{
	cout << endl;
	if (jack_server_running ()) {
		cout << "Jack server running " << endl;
	} else {
		cout << "Jack server not running " << endl;
	}
}

void
JackUtilsTest::test_set_jack_path_env ()
{
	cout << endl;

	bool path_env_set = false;

	string path_env = Glib::getenv ("PATH", path_env_set);

	if (path_env_set) {
		cout << "PATH env set to: " << path_env << endl;
	} else {
		cout << "PATH env not set" << endl;
	}
	vector<string> server_dirs;
	get_jack_server_dir_paths (server_dirs);
	set_path_env_for_jack_autostart (server_dirs);

	path_env_set = false;

	path_env = Glib::getenv ("PATH", path_env_set);

	CPPUNIT_ASSERT (path_env_set);

	cout << "After set_jack_path_env PATH env set to: " << path_env << endl;
}

void
JackUtilsTest::test_server_paths ()
{
	cout << endl;

	vector<std::string> server_dirs;

	CPPUNIT_ASSERT (get_jack_server_dir_paths (server_dirs));

	cout << "Number of Directories that may contain JACK servers: " << server_dirs.size () << endl;

	for (vector<std::string>::const_iterator i = server_dirs.begin(); i != server_dirs.end(); ++i) {
		cout << "JACK server directory path: " << *i << endl;
	}

	vector<string> server_names;

	CPPUNIT_ASSERT (get_jack_server_application_names (server_names));

	cout << "Number of possible JACK server names on this system: " << server_names.size () << endl;

	for (vector<string>::const_iterator i = server_names.begin(); i != server_names.end(); ++i) {
		cout << "JACK server name: " << *i << endl;
	}

	vector<std::string> server_paths;

	CPPUNIT_ASSERT (get_jack_server_paths (server_dirs, server_names, server_paths));

	cout << "Number of JACK servers on this system: " << server_paths.size () << endl;

	for (vector<std::string>::const_iterator i = server_paths.begin(); i != server_paths.end(); ++i) {
		cout << "JACK server path: " << *i << endl;
	}

	vector<std::string> server_paths2;

	CPPUNIT_ASSERT (get_jack_server_paths (server_paths2));

	CPPUNIT_ASSERT (server_paths.size () == server_paths2.size ());

	std::string default_server_path;

	CPPUNIT_ASSERT (get_jack_default_server_path (default_server_path));

	cout << "The default JACK server on this system: " << default_server_path << endl;
}

bool
get_default_jack_command_line (std::string& command_line)
{
	cout << endl;

	JackCommandLineOptions options;

	CPPUNIT_ASSERT (get_jack_default_server_path (options.server_path));

	get_jack_default_audio_driver_name (options.driver);


	// should fail, haven't set any device yet
	CPPUNIT_ASSERT (!get_jack_command_line_string (options, command_line));

	vector<string> devices = get_jack_device_names_for_audio_driver (options.driver);

	if (!devices.empty()) {
		options.input_device = devices.front ();
		options.output_device = devices.front ();
	} else {
		cout << "No audio devices available using default JACK driver using Dummy driver" << endl;
		options.driver = dummy_driver_name;
		devices = get_jack_device_names_for_audio_driver (options.driver);
		CPPUNIT_ASSERT (!devices.empty ());
		options.input_device = devices.front ();
		options.output_device = devices.front ();
	}

	options.input_device = devices.front ();
	options.output_device = devices.front ();

	string midi_driver;

	get_jack_default_midi_system_name (options.driver, options.midi_driver);
	//
	// this at least should create a valid jack command line
	return get_jack_command_line_string (options, command_line);

}

void
JackUtilsTest::test_config ()
{
	std::string config_path(get_jack_server_user_config_file_path());

	cout << "Jack server config file path: " << config_path << endl;

	std::string command_line;

	CPPUNIT_ASSERT (get_default_jack_command_line (command_line));
	
	CPPUNIT_ASSERT (write_jack_config_file (config_path, command_line));
}


void
JackUtilsTest::test_command_line ()
{
	string command_line;

	// this at least should create a valid jack command line
	CPPUNIT_ASSERT (get_default_jack_command_line (command_line));

	cout << "Default JACK command line: " << command_line << endl;
}
