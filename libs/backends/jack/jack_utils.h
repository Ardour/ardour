/*
    Copyright (C) 2011 Tim Mayberry

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

#include <stdint.h>

#include <vector>
#include <map>
#include <string>

namespace ARDOUR {

	// Names for the drivers on all possible systems
	extern const char * const portaudio_driver_name;
	extern const char * const coreaudio_driver_name;
	extern const char * const alsa_driver_name;
	extern const char * const oss_driver_name;
	extern const char * const freebob_driver_name;
	extern const char * const ffado_driver_name;
	extern const char * const netjack_driver_name;
	extern const char * const dummy_driver_name;

	/**
	 * Get a list of possible JACK audio driver names based on platform
	 */
	void get_jack_audio_driver_names (std::vector<std::string>& driver_names);

	/**
	 * Get the default JACK audio driver based on platform
	 */
	void get_jack_default_audio_driver_name (std::string& driver_name);

	/**
	 * Get a list of possible samplerates supported be JACK
	 */
	void get_jack_sample_rate_strings (std::vector<std::string>& sample_rates);

	/**
	 * @return The default samplerate
	 */
	std::string get_jack_default_sample_rate ();

	/**
	 * @return true if sample rate string was able to be converted
	 */
	bool get_jack_sample_rate_value_from_string (const std::string& srs, uint32_t& srv);

	/**
	 * Get a list of possible period sizes supported be JACK
	 */
	void get_jack_period_size_strings (std::vector<std::string>& samplerates);

	/**
	 * @return The default period size
	 */
	std::string get_jack_default_period_size ();

	/**
	 * @return true if period size string was able to be converted
	 */
	bool get_jack_period_size_value_from_string (const std::string& pss, uint32_t& psv);

	/**
	 * These are driver specific I think, so it may require a driver arg
	 * in future
	 */
	void get_jack_dither_mode_strings (const std::string& driver, std::vector<std::string>& dither_modes);

	/**
	 * @return The default dither mode
	 */
	std::string get_jack_default_dither_mode (const std::string& driver);

	/**
	 * @return Estimate of latency
	 *
	 * API matches current use in GUI
	 */
	std::string get_jack_latency_string (std::string samplerate, float periods, std::string period_size);

	/**
	 * Key being a readable name to display in a GUI
	 * Value being name used in a jack commandline
	 */
	typedef std::map<std::string, std::string> device_map_t;

	/**
	 * Use library specific code to find out what what devices exist for a given
	 * driver that might work in JACK. There is no easy way to find out what
	 * modules the JACK server supports so guess based on platform. For instance
	 * portaudio is cross-platform but we only return devices if built for
	 * windows etc
	 */
	void get_jack_alsa_device_names (device_map_t& devices);
	void get_jack_portaudio_device_names (device_map_t& devices);
	void get_jack_coreaudio_device_names (device_map_t& devices);
	void get_jack_oss_device_names (device_map_t& devices);
	void get_jack_freebob_device_names (device_map_t& devices);
	void get_jack_ffado_device_names (device_map_t& devices);
	void get_jack_netjack_device_names (device_map_t& devices);
	void get_jack_dummy_device_names (device_map_t& devices);

	/*
	 * @return true if there were devices found for the driver
	 *
	 * @param driver The driver name returned by get_jack_audio_driver_names
	 * @param devices The map used to insert the drivers into, devices will be cleared before
	 * adding the available drivers
	 */
	bool get_jack_device_names_for_audio_driver (const std::string& driver, device_map_t& devices);

	/*
	 * @return a list of readable device names for a specific driver.
	 */
	std::vector<std::string> get_jack_device_names_for_audio_driver (const std::string& driver);

	/**
	 * @return true if the driver supports playback and recording
	 * on separate devices
	 */
	bool get_jack_audio_driver_supports_two_devices (const std::string& driver);

	bool get_jack_audio_driver_supports_latency_adjustment (const std::string& driver);

	bool get_jack_audio_driver_supports_setting_period_count (const std::string& driver);

	/**
	 * The possible names to use to try and find servers, this includes
	 * any file extensions like .exe on Windows
	 *
	 * @return true if the JACK application names for this platform could be guessed
	 */
	bool get_jack_server_application_names (std::vector<std::string>& server_names);

	/**
	 * Sets the PATH environment variable to contain directories likely to contain
	 * JACK servers so that if the JACK server is auto-started it can find the server
	 * executable.
	 *
	 * This is only modifies PATH on the mac at the moment.
	 */
	void set_path_env_for_jack_autostart (const std::vector<std::string>&);

	/**
	 * Get absolute paths to directories that might contain JACK servers on the system
	 *
	 * @return true if !server_paths.empty()
	 */
	bool get_jack_server_dir_paths (std::vector<std::string>& server_dir_paths);

	/**
	 * Get absolute paths to JACK servers on the system
	 *
	 * @return true if a server was found
	 */
	bool get_jack_server_paths (const std::vector<std::string>& server_dir_paths,
			const std::vector<std::string>& server_names,
			std::vector<std::string>& server_paths);


	bool get_jack_server_paths (std::vector<std::string>& server_paths);

	/**
	 * Get absolute path to default JACK server
	 */
	bool get_jack_default_server_path (std::string& server_path);

	/**
	 * @return The name of the jack server config file
	 */
	std::string get_jack_server_config_file_name ();

	std::string get_jack_server_user_config_dir_path ();

	std::string get_jack_server_user_config_file_path ();

	bool write_jack_config_file (const std::string& config_file_path, const std::string& command_line);

	struct JackCommandLineOptions {

		// see implementation for defaults
		JackCommandLineOptions ();

		//operator bool
		//operator ostream

		std::string      server_path;
		uint32_t         timeout;
		bool             no_mlock;
		uint32_t         ports_max;
		bool             realtime;
		uint32_t         priority;
		bool             unlock_gui_libs;
		bool             verbose;
		bool             temporary;
		bool             playback_only;
		bool             capture_only;
		std::string      driver;
		std::string      input_device;
		std::string      output_device;
		uint32_t         num_periods;
		uint32_t         period_size;
		uint32_t         samplerate;
		uint32_t         input_channels;
		uint32_t         output_channels;
		uint32_t         input_latency;
		uint32_t         output_latency;
		bool             hardware_metering;
		bool             hardware_monitoring;
		std::string      dither_mode;
		bool             force16_bit;
		bool             soft_mode;
		std::string      midi_driver;
	};

	/**
	 * @return true if able to build a valid command line based on options
	 */
	bool get_jack_command_line_string (JackCommandLineOptions& options, std::string& command_line);
}
