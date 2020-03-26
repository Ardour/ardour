/*
 * Copyright (C) 2013-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
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

#ifdef HAVE_ALSA
#include "ardouralsautil/devicelist.h"
#endif

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CFString.h>
#include <sys/param.h>
#include <mach-o/dyld.h>
#endif

#ifdef PLATFORM_WINDOWS
#include <shobjidl.h>  //  Needed for
#include <shlguid.h>   // 'IShellLink'
#endif

#if (defined PLATFORM_WINDOWS && defined HAVE_PORTAUDIO)
#include <portaudio.h>
#endif

#include <boost/scoped_ptr.hpp>

#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>

#include "pbd/epa.h"
#include "pbd/error.h"
#include "pbd/string_convert.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"

#include "jack_utils.h"

#ifdef __APPLE
#include <CFBundle.h>
#endif

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

namespace ARDOUR {
	// The pretty driver names
	const char * const portaudio_driver_name = X_("Portaudio");
	const char * const coreaudio_driver_name = X_("CoreAudio");
	const char * const alsa_driver_name = X_("ALSA");
	const char * const oss_driver_name = X_("OSS");
	const char * const sun_driver_name = X_("Sun");
	const char * const freebob_driver_name = X_("FreeBoB");
	const char * const ffado_driver_name = X_("FFADO");
	const char * const netjack_driver_name = X_("NetJACK");
	const char * const dummy_driver_name = X_("Dummy");
}

namespace {

	// The real driver names
	const char * const portaudio_driver_command_line_name = X_("portaudio");
	const char * const coreaudio_driver_command_line_name = X_("coreaudio");
	const char * const alsa_driver_command_line_name = X_("alsa");
	const char * const oss_driver_command_line_name = X_("oss");
	const char * const sun_driver_command_line_name = X_("sun");
	const char * const freebob_driver_command_line_name = X_("freebob");
	const char * const ffado_driver_command_line_name = X_("firewire");
	const char * const netjack_driver_command_line_name = X_("netjack");
	const char * const dummy_driver_command_line_name = X_("dummy");

	// should we provide more "pretty" names like above?
	const char * const alsa_seq_midi_driver_name = X_("alsa");
	const char * const alsa_raw_midi_driver_name = X_("alsarawmidi");
	const char * const alsaseq_midi_driver_name = X_("seq");
	const char * const alsaraw_midi_driver_name = X_("raw");
	const char * const winmme_midi_driver_name = X_("winmme");
	const char * const coremidi_midi_driver_name = X_("coremidi");

	// this should probably be translated
	const char * const default_device_name = X_("Default");
}

static ARDOUR::MidiOptions midi_options;

std::string
get_none_string ()
{
	return _("None");
}

void
ARDOUR::get_jack_audio_driver_names (vector<string>& audio_driver_names)
{
#ifdef PLATFORM_WINDOWS
	audio_driver_names.push_back (portaudio_driver_name);
#elif __APPLE__
	audio_driver_names.push_back (coreaudio_driver_name);
#else
#ifdef HAVE_ALSA
	audio_driver_names.push_back (alsa_driver_name);
#endif
	audio_driver_names.push_back (oss_driver_name);
#if defined(__NetBSD__) || defined(__sun)
	audio_driver_names.push_back (sun_driver_name);
#endif
	audio_driver_names.push_back (freebob_driver_name);
	audio_driver_names.push_back (ffado_driver_name);
#endif
	audio_driver_names.push_back (netjack_driver_name);
	audio_driver_names.push_back (dummy_driver_name);
}

void
ARDOUR::get_jack_default_audio_driver_name (string& audio_driver_name)
{
	vector<string> drivers;
	get_jack_audio_driver_names (drivers);
	audio_driver_name = drivers.front ();
}

void
ARDOUR::get_jack_sample_rate_strings (vector<string>& samplerates)
{
	// do these really need to be translated?
	samplerates.push_back (_("8000Hz"));
	samplerates.push_back (_("22050Hz"));
	samplerates.push_back (_("44100Hz"));
	samplerates.push_back (_("48000Hz"));
	samplerates.push_back (_("88200Hz"));
	samplerates.push_back (_("96000Hz"));
	samplerates.push_back (_("192000Hz"));
}

string
ARDOUR::get_jack_default_sample_rate ()
{
	return _("48000Hz");
}

void
ARDOUR::get_jack_period_size_strings (std::vector<std::string>& period_sizes)
{
	period_sizes.push_back ("32");
	period_sizes.push_back ("64");
	period_sizes.push_back ("128");
	period_sizes.push_back ("256");
	period_sizes.push_back ("512");
	period_sizes.push_back ("1024");
	period_sizes.push_back ("2048");
	period_sizes.push_back ("4096");
	period_sizes.push_back ("8192");
}

string
ARDOUR::get_jack_default_period_size ()
{
	return "1024";
}

void
ARDOUR::get_jack_dither_mode_strings (const string& driver, vector<string>& dither_modes)
{
	dither_modes.push_back (get_none_string ());

	if (driver == alsa_driver_name ) {
		dither_modes.push_back (_("Triangular"));
		dither_modes.push_back (_("Rectangular"));
		dither_modes.push_back (_("Shaped"));
	}
}

string
ARDOUR::get_jack_default_dither_mode (const string& /*driver*/)
{
	return get_none_string ();
}

string
ARDOUR::get_jack_latency_string (string samplerate, float periods, string period_size)
{
	uint32_t rate = atoi (samplerate);
	float psize = atof (period_size);

	char buf[32];
	snprintf (buf, sizeof(buf), "%.1fmsec", (periods * psize) / (rate/1000.0));

	return buf;
}

bool
get_jack_command_line_audio_driver_name (const string& driver_name, string& command_line_name)
{
	using namespace ARDOUR;
	if (driver_name == portaudio_driver_name) {
		command_line_name = portaudio_driver_command_line_name;
		return true;
	} else if (driver_name == coreaudio_driver_name) {
		command_line_name = coreaudio_driver_command_line_name;
		return true;
	} else if (driver_name == alsa_driver_name) {
		command_line_name = alsa_driver_command_line_name;
		return true;
	} else if (driver_name == oss_driver_name) {
		command_line_name = oss_driver_command_line_name;
		return true;
	} else if (driver_name == sun_driver_name) {
		command_line_name = sun_driver_command_line_name;
		return true;
	} else if (driver_name == freebob_driver_name) {
		command_line_name = freebob_driver_command_line_name;
		return true;
	} else if (driver_name == ffado_driver_name) {
		command_line_name = ffado_driver_command_line_name;
		return true;
	} else if (driver_name == netjack_driver_name) {
		command_line_name = netjack_driver_command_line_name;
		return true;
	} else if (driver_name == dummy_driver_name) {
		command_line_name = dummy_driver_command_line_name;
		return true;
	}
	return false;
}

bool
get_jack_command_line_audio_device_name (const string& driver_name,
		const string& device_name, string& command_line_device_name)
{
	using namespace ARDOUR;
	device_map_t devices;

	get_jack_device_names_for_audio_driver (driver_name, devices);

	for (device_map_t::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		if (i->first == device_name) {
			command_line_device_name = i->second;
			return true;
		}
	}
	return false;
}

bool
get_jack_command_line_dither_mode (const string& dither_mode, string& command_line_dither_mode)
{
	using namespace ARDOUR;

	if (dither_mode == _("Triangular")) {
		command_line_dither_mode = "triangular";
		return true;
	} else if (dither_mode == _("Rectangular")) {
		command_line_dither_mode = "rectangular";
		return true;
	} else if (dither_mode == _("Shaped")) {
		command_line_dither_mode = "shaped";
		return true;
	}

	return false;
}

void
ARDOUR::get_jack_alsa_device_names (device_map_t& devices)
{
#ifdef HAVE_ALSA
	get_alsa_audio_device_names(devices, HalfDuplexOut);
#else
	/* silence a compiler unused variable warning */
	(void) devices;
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
#endif

void
ARDOUR::get_jack_coreaudio_device_names (device_map_t& devices)
{
#ifdef __APPLE__
	// Find out how many Core Audio devices are there, if any...
	// (code snippet gently "borrowed" from St?hane Letz jackdmp;)
	OSStatus err;
	Boolean isWritable;
	UInt32 outSize = sizeof(isWritable);

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
							devices.insert (make_pair (coreDeviceName, drivername));
						}
					}
				}
			}
		}
		delete [] coreDeviceIDs;
	}
#else
	/* silence a compiler unused variable warning */
	(void) devices;
#endif
}

void
ARDOUR::get_jack_portaudio_device_names (device_map_t& devices)
{
#if (defined PLATFORM_WINDOWS && defined HAVE_PORTAUDIO)
	if (Pa_Initialize() != paNoError) {
		return;
	}

	for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount (); ++i) {
		string api_name;
		string readable_name;
		string jack_device_name;
		const PaDeviceInfo* device_info = Pa_GetDeviceInfo(i);

		if (device_info != NULL) { // it should never be ?
			api_name = Pa_GetHostApiInfo (device_info->hostApi)->name;
			readable_name = api_name + " " + device_info->name;
			jack_device_name = api_name + "::" + device_info->name;
			devices.insert (make_pair (readable_name, jack_device_name));
		}
	}
	Pa_Terminate();
#else
	/* silence a compiler unused variable warning */
	(void) devices;
#endif
}

void
ARDOUR::get_jack_oss_device_names (device_map_t& devices)
{
	devices.insert (make_pair (default_device_name, default_device_name));
}

void
ARDOUR::get_jack_sun_device_names (device_map_t& devices)
{
	devices.insert (make_pair (default_device_name, default_device_name));
}


void
ARDOUR::get_jack_freebob_device_names (device_map_t& devices)
{
	devices.insert (make_pair (default_device_name, default_device_name));
}

void
ARDOUR::get_jack_ffado_device_names (device_map_t& devices)
{
	devices.insert (make_pair (default_device_name, default_device_name));
}

void
ARDOUR::get_jack_netjack_device_names (device_map_t& devices)
{
	devices.insert (make_pair (default_device_name, default_device_name));
}

void
ARDOUR::get_jack_dummy_device_names (device_map_t& devices)
{
	devices.insert (make_pair (default_device_name, default_device_name));
}

bool
ARDOUR::get_jack_device_names_for_audio_driver (const string& driver_name, device_map_t& devices)
{
	devices.clear();

	if (driver_name == portaudio_driver_name) {
		get_jack_portaudio_device_names (devices);
	} else if (driver_name == coreaudio_driver_name) {
		get_jack_coreaudio_device_names (devices);
	} else if (driver_name == alsa_driver_name) {
		get_jack_alsa_device_names (devices);
	} else if (driver_name == oss_driver_name) {
		get_jack_oss_device_names (devices);
	} else if (driver_name == sun_driver_name) {
		get_jack_sun_device_names (devices);
	} else if (driver_name == freebob_driver_name) {
		get_jack_freebob_device_names (devices);
	} else if (driver_name == ffado_driver_name) {
		get_jack_ffado_device_names (devices);
	} else if (driver_name == netjack_driver_name) {
		get_jack_netjack_device_names (devices);
	} else if (driver_name == dummy_driver_name) {
		get_jack_dummy_device_names (devices);
	}

	return !devices.empty();
}


std::vector<std::string>
ARDOUR::get_jack_device_names_for_audio_driver (const string& driver_name)
{
	std::vector<std::string> readable_names;
	device_map_t devices;

	get_jack_device_names_for_audio_driver (driver_name, devices);

	for (device_map_t::const_iterator i = devices.begin (); i != devices.end(); ++i) {
		readable_names.push_back (i->first);
	}

	return readable_names;
}

bool
ARDOUR::get_jack_audio_driver_supports_two_devices (const string& driver)
{
	return (driver == alsa_driver_name || driver == oss_driver_name ||
			driver == sun_driver_name);
}

bool
ARDOUR::get_jack_audio_driver_supports_latency_adjustment (const string& driver)
{
	return (driver == alsa_driver_name || driver == coreaudio_driver_name ||
			driver == ffado_driver_name || driver == portaudio_driver_name);
}

bool
ARDOUR::get_jack_audio_driver_supports_setting_period_count (const string& driver)
{
	return !(driver == dummy_driver_name || driver == coreaudio_driver_name ||
			driver == portaudio_driver_name);
}

bool
ARDOUR::get_jack_server_application_names (std::vector<std::string>& server_names)
{
#ifdef PLATFORM_WINDOWS
	server_names.push_back ("jackd.exe");
#else
	server_names.push_back ("jackd");
	server_names.push_back ("jackdmp");
#endif
	return !server_names.empty();
}

void
ARDOUR::set_path_env_for_jack_autostart (const vector<std::string>& dirs)
{
#ifdef __APPLE__
	// push it back into the environment so that auto-started JACK can find it.
	// XXX why can't we just expect OS X users to have PATH set correctly? we can't ...
	setenv ("PATH", Searchpath(dirs).to_string().c_str(), 1);
#else
	/* silence a compiler unused variable warning */
	(void) dirs;
#endif
}

bool
ARDOUR::get_jack_server_dir_paths (vector<std::string>& server_dir_paths)
{
#ifdef __APPLE__
	/* this magic lets us finds the path to the OSX bundle, and then
	   we infer JACK's location from there
	*/

	char execpath[MAXPATHLEN+1];
	uint32_t pathsz = sizeof (execpath);

	_NSGetExecutablePath (execpath, &pathsz);

	server_dir_paths.push_back (Glib::path_get_dirname (execpath));
#endif

	Searchpath sp(string(g_getenv("PATH")));

#ifdef PLATFORM_WINDOWS
// N.B. The #define (immediately below) can be safely removed once we know that this code builds okay with mingw
#ifdef COMPILER_MSVC
	IShellLinkA  *pISL = NULL;
	IPersistFile *ppf  = NULL;

	// Mixbus creates a Windows shortcut giving the location of its
	// own (bundled) version of Jack. Let's see if that shortcut exists
	if (SUCCEEDED (CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pISL)))
	{
		if (SUCCEEDED (pISL->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf)))
		{
			char  target_path[MAX_PATH];
			char  shortcut_pathA[MAX_PATH];
			WCHAR shortcut_pathW[MAX_PATH];

			// Our Windows installer should have created a shortcut to the Jack
			// server so let's start by finding out what drive it got installed on
			if (char *env_path = getenv ("windir"))
			{
				strcpy (shortcut_pathA, env_path);
				shortcut_pathA[2] = '\0'; // Gives us just the drive letter and colon
			}
			else // Assume 'C:'
				strcpy (shortcut_pathA, "C:");

			strcat (shortcut_pathA, "\\Program Files (x86)\\Jack\\Start Jack.lnk");

			MultiByteToWideChar (CP_ACP, MB_PRECOMPOSED, shortcut_pathA, -1, shortcut_pathW, MAX_PATH);

			// If it did, load the shortcut into our persistent file
			if (SUCCEEDED (ppf->Load(shortcut_pathW, 0)))
			{
				// Read the target information from the shortcut object
				if (S_OK == (pISL->GetPath (target_path, MAX_PATH, NULL, SLGP_UNCPRIORITY)))
				{
					char *p = strrchr (target_path, '\\');

					if (p)
					{
						*p = NULL;
						sp.push_back (target_path);
					}
				}
			}
		}
	}

	if (ppf)
		ppf->Release();

	if (pISL)
		pISL->Release();
#endif

	gchar *install_dir = g_win32_get_package_installation_directory_of_module (NULL);
	if (install_dir) {
		sp.push_back (install_dir);
		g_free (install_dir);
	}
	// don't try and use a system wide JACK install yet.
#else
	if (sp.empty()) {
		sp.push_back ("/usr/bin");
		sp.push_back ("/bin");
		sp.push_back ("/usr/local/bin");
		sp.push_back ("/opt/local/bin");
	}
#endif

	std::copy (sp.begin(), sp.end(), std::back_inserter(server_dir_paths));

	return !server_dir_paths.empty();
}

bool
ARDOUR::get_jack_server_paths (const vector<std::string>& server_dir_paths,
		const vector<string>& server_names,
		vector<std::string>& server_paths)
{
	for (vector<string>::const_iterator i = server_names.begin(); i != server_names.end(); ++i) {
		find_files_matching_pattern (server_paths, server_dir_paths, *i);
	}
	return !server_paths.empty();
}

bool
ARDOUR::get_jack_server_paths (vector<std::string>& server_paths)
{
	vector<std::string> server_dirs;

	if (!get_jack_server_dir_paths (server_dirs)) {
		return false;
	}

	vector<string> server_names;

	if (!get_jack_server_application_names (server_names)) {
		return false;
	}

	if (!get_jack_server_paths (server_dirs, server_names, server_paths)) {
		return false;
	}

	return !server_paths.empty();
}

bool
ARDOUR::get_jack_default_server_path (std::string& server_path)
{
	vector<std::string> server_paths;

	if (!get_jack_server_paths (server_paths)) {
		return false;
	}

	server_path = server_paths.front ();
	return true;
}

string
quote_string (const string& str)
{
	return "\"" + str + "\"";
}

ARDOUR::JackCommandLineOptions::JackCommandLineOptions ()
	: server_path ()
	, timeout(0)
	, no_mlock(false)
	, ports_max(128)
	, realtime(true)
	, priority(0)
	, unlock_gui_libs(false)
	, verbose(false)
	, temporary(true)
	, driver()
	, input_device()
	, output_device()
	, num_periods(2)
	, period_size(1024)
	, samplerate(48000)
	, input_latency(0)
	, output_latency(0)
	, hardware_metering(false)
	, hardware_monitoring(false)
	, dither_mode()
	, force16_bit(false)
	, soft_mode(false)
	, midi_driver()
{

}

bool
ARDOUR::get_jack_command_line_string (JackCommandLineOptions& options, string& command_line)
{
	vector<string> args;

	args.push_back (options.server_path);

#ifdef PLATFORM_WINDOWS
	// must use sync mode on windows
	args.push_back ("-S");
#endif

#if (defined PLATFORM_WINDOWS || defined __APPLE__)
	// midi systems needs to be added before the audio driver for jack2
	if (!options.midi_driver.empty () && options.midi_driver != get_none_string ()) {
		args.push_back ("-X");
		args.push_back (options.midi_driver);
	}
#endif

	/* XXX hack to enforce qjackctl-like behaviour */
	if (options.timeout == 0) {
		options.timeout = 200;
	}

	if (options.timeout) {
		args.push_back ("-t");
		args.push_back (to_string (options.timeout));
	}

	if (options.no_mlock) {
		args.push_back ("-m");
	}

	args.push_back ("-p");
	args.push_back (to_string(options.ports_max));

	if (options.realtime) {
		args.push_back ("-R");
		if (options.priority != 0) {
			args.push_back ("-P");
			args.push_back (to_string(options.priority));
		}
	} else {
		args.push_back ("-r");
	}

	if (options.unlock_gui_libs) {
		args.push_back ("-u");
	}

	if (options.verbose) {
		args.push_back ("-v");
	}

	if (options.temporary) {
		args.push_back ("-T");
	}

	if (options.driver == alsa_driver_name) {
		if (options.midi_driver == alsa_seq_midi_driver_name) {
			args.push_back ("-X");
			args.push_back ("alsa_midi");
		} else if (options.midi_driver == alsa_raw_midi_driver_name) {
			args.push_back ("-X");
			args.push_back ("alsarawmidi");
		}
	}

	string command_line_driver_name;

	string command_line_input_device_name;
	string command_line_output_device_name;

	if (!get_jack_command_line_audio_driver_name (options.driver, command_line_driver_name)) {
		return false;
	}

	args.push_back ("-d");
	args.push_back (command_line_driver_name);

	if (options.driver != dummy_driver_name) {
		if (options.output_device.empty() && options.input_device.empty()) {
			return false;
		}


		if (!get_jack_command_line_audio_device_name (options.driver,
					options.input_device, command_line_input_device_name)) {
			return false;
		}

		if (!get_jack_command_line_audio_device_name (options.driver,
					options.output_device, command_line_output_device_name)) {
			return false;
		}

		if (options.input_device.empty()) {
			// playback only
			if (options.output_device.empty()) {
				return false;
			}
			args.push_back ("-P");
		} else if (options.output_device.empty()) {
			// capture only
			if (options.input_device.empty()) {
				return false;
			}
			args.push_back ("-C");
		} else if (options.input_device != options.output_device) {
			// capture and playback on two devices if supported
			if (get_jack_audio_driver_supports_two_devices (options.driver)) {
				args.push_back ("-C");
				args.push_back (command_line_input_device_name);
				args.push_back ("-P");
				args.push_back (command_line_output_device_name);
			} else {
				return false;
			}
		}

		if (options.input_channels) {
			args.push_back ("-i");
			args.push_back (to_string (options.input_channels));
		}

		if (options.output_channels) {
			args.push_back ("-o");
			args.push_back (to_string (options.output_channels));
		}

		if (get_jack_audio_driver_supports_setting_period_count (options.driver)) {
			args.push_back ("-n");
			args.push_back (to_string (options.num_periods));
		}
	} else {
		// jackd dummy backend
		if (options.input_channels) {
			args.push_back ("-C");
			args.push_back (to_string (options.input_channels));
		}

		if (options.output_channels) {
			args.push_back ("-P");
			args.push_back (to_string (options.output_channels));
		}
	}

	args.push_back ("-r");
	args.push_back (to_string (options.samplerate));

	args.push_back ("-p");
	args.push_back (to_string (options.period_size));

	if (get_jack_audio_driver_supports_latency_adjustment (options.driver)) {
		if (options.input_latency) {
			args.push_back ("-I");
			args.push_back (to_string (options.input_latency));
		}
		if (options.output_latency) {
			args.push_back ("-O");
			args.push_back (to_string (options.output_latency));
		}
	}

	if (options.driver != dummy_driver_name) {
		if (options.input_device == options.output_device && options.input_device != default_device_name) {
			args.push_back ("-d");
			args.push_back (command_line_input_device_name);
		}
	}

	if (options.driver == alsa_driver_name) {
		if (options.hardware_metering) {
			args.push_back ("-M");
		}
		if (options.hardware_monitoring) {
			args.push_back ("-H");
		}

		string command_line_dither_mode;
		if (get_jack_command_line_dither_mode (options.dither_mode, command_line_dither_mode)) {
			args.push_back ("-z");
			args.push_back (command_line_dither_mode);
		}
		if (options.force16_bit) {
			args.push_back ("-S");
		}
		if (options.soft_mode) {
			args.push_back ("-s");
		}
	}

	if (options.driver == alsa_driver_name) {

		if (options.midi_driver != alsa_seq_midi_driver_name) {
			if (!options.midi_driver.empty() && options.midi_driver != get_none_string ()) {
				args.push_back ("-X");
				args.push_back (options.midi_driver);
			}
		}
	}

	ostringstream oss;

	for (vector<string>::const_iterator i = args.begin(); i != args.end();) {
		if (i->find_first_of(' ') != string::npos) {
			oss << "\"" << *i << "\"";
		} else {
			oss << *i;
		}
		if (++i != args.end()) oss << ' ';
	}

	command_line = oss.str();
	return true;
}

string
ARDOUR::get_jack_server_config_file_name ()
{
	return ".jackdrc";
}

std::string
ARDOUR::get_jack_server_user_config_dir_path ()
{
	return Glib::get_home_dir ();
}

std::string
ARDOUR::get_jack_server_user_config_file_path ()
{
	return Glib::build_filename (get_jack_server_user_config_dir_path (), get_jack_server_config_file_name ());
}

bool
ARDOUR::write_jack_config_file (const std::string& config_file_path, const string& command_line)
{
	if (!g_file_set_contents (config_file_path.c_str(), command_line.c_str(), -1, NULL)) {
		error << string_compose (_("cannot open JACK rc file %1 to store parameters"), config_file_path) << endmsg;
		return false;
	}
	return true;
}

vector<string>
ARDOUR::enumerate_midi_options ()
{
	if (midi_options.empty()) {
#ifdef HAVE_ALSA
		midi_options.push_back (make_pair (_("(legacy) ALSA raw devices"), alsaraw_midi_driver_name));
		midi_options.push_back (make_pair (_("(legacy) ALSA sequencer"), alsaseq_midi_driver_name));
		midi_options.push_back (make_pair (_("ALSA (JACK1, 0.124 and later)"), alsa_seq_midi_driver_name));
		midi_options.push_back (make_pair (_("ALSA (JACK2, 1.9.8 and later)"), alsa_raw_midi_driver_name));
#endif
#if (defined PLATFORM_WINDOWS && defined HAVE_PORTAUDIO)
		/* Windows folks: what name makes sense here? Are there other
		   choices as well ?
		*/
		midi_options.push_back (make_pair (_("System MIDI (MME)"), winmme_midi_driver_name));
#endif
#ifdef __APPLE__
		midi_options.push_back (make_pair (_("CoreMIDI"), coremidi_midi_driver_name));
#endif
	}

	vector<string> v;

	for (MidiOptions::const_iterator i = midi_options.begin(); i != midi_options.end(); ++i) {
		v.push_back (i->first);
	}

	v.push_back (get_none_string());

	return v;
}

int
ARDOUR::set_midi_option (ARDOUR::JackCommandLineOptions& options, const string& opt)
{
	if (opt.empty() || opt == get_none_string()) {
		options.midi_driver = "";
		return 0;
	}

	for (MidiOptions::const_iterator i = midi_options.begin(); i != midi_options.end(); ++i) {
		if (i->first == opt) {
			options.midi_driver = i->second;
			return 0;
		}
	}

	return -1;
}

