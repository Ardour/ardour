/*
    Copyright (C) 1999 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_configuration_h__
#define __ardour_configuration_h__

#include <map>

#include <sys/types.h>
#include <string>

#include <ardour/types.h>
#include <ardour/stateful.h>

class XMLNode;

namespace ARDOUR {

class Configuration : public Stateful
{
  public:
	Configuration();
	virtual ~Configuration();

	struct MidiPortDescriptor {
	    std::string tag;
	    std::string device;
	    std::string type;
	    std::string mode;

	    MidiPortDescriptor (const XMLNode&);
	    XMLNode& get_state();
	};

	std::map<std::string,MidiPortDescriptor *> midi_ports;

	int load_state ();
	int save_state ();

	XMLNode& option_node (const std::string &, const std::string &);
	
	int set_state (const XMLNode&);
	XMLNode& get_state (void);

	XMLNode * get_keys() const;
	void set_keys(XMLNode *);

	void set_latched_record_enable (bool yn);
	bool get_latched_record_enable();

	void set_use_vst (bool yn);
	bool get_use_vst();

	bool get_trace_midi_input ();
	void set_trace_midi_input (bool);

	bool get_trace_midi_output ();
	void set_trace_midi_output (bool);

	std::string get_raid_path();
	void set_raid_path(std::string);

	uint32_t get_minimum_disk_io(); 
	void set_minimum_disk_io(uint32_t);

	float get_track_buffer();
	void set_track_buffer(float);

	bool does_hiding_groups_deactivates_groups();
	void set_hiding_groups_deactivates_groups(bool);

	std::string get_auditioner_output_left();
	void set_auditioner_output_left(std::string);

	std::string get_auditioner_output_right();
	void set_auditioner_output_right(std::string);

	bool get_mute_affects_pre_fader();
	void set_mute_affects_pre_fader (bool);

	bool get_mute_affects_post_fader();
	void set_mute_affects_post_fader (bool);

	bool get_mute_affects_control_outs ();
	void set_mute_affects_control_outs (bool);

	bool get_mute_affects_main_outs ();
	void set_mute_affects_main_outs (bool);

	bool get_solo_latch ();
	void set_solo_latch (bool);

	uint32_t get_disk_choice_space_threshold();
	void set_disk_choice_space_threshold (uint32_t);

	std::string get_mmc_port_name();
	void   set_mmc_port_name(std::string);

	std::string get_mtc_port_name();
	void   set_mtc_port_name(std::string);
	
	std::string get_midi_port_name();
	void   set_midi_port_name(std::string);

	uint32_t get_midi_feedback_interval_ms();
	void set_midi_feedback_interval_ms (uint32_t);
	
	bool get_use_hardware_monitoring();
	void set_use_hardware_monitoring(bool);

	bool get_use_sw_monitoring();
	void set_use_sw_monitoring(bool);

	bool get_jack_time_master();
	void set_jack_time_master(bool);

	bool get_native_format_is_bwf();
	void set_native_format_is_bwf(bool);

	bool get_plugins_stop_with_transport();
	void set_plugins_stop_with_transport(bool);

	bool get_stop_recording_on_xrun();
	void set_stop_recording_on_xrun(bool);

	bool get_verify_remove_last_capture();
	void set_verify_remove_last_capture(bool);
	
	bool get_stop_at_session_end();
	void set_stop_at_session_end(bool);

	bool get_seamless_looping();
	void set_seamless_looping(bool);

	bool get_auto_xfade();
	void set_auto_xfade (bool);

	bool get_no_new_session_dialog();
	void set_no_new_session_dialog(bool);
	
	uint32_t get_timecode_skip_limit ();
	void set_timecode_skip_limit (uint32_t);

	bool get_timecode_source_is_synced ();
	void set_timecode_source_is_synced (bool);

	gain_t get_quieten_at_speed ();
	void  set_quieten_at_speed (gain_t);

	uint32_t get_destructive_xfade_msecs ();
	void set_destructive_xfade_msecs (uint32_t, jack_nframes_t sample_rate = 0);
	
  private:
	void   set_defaults ();
	std::string get_system_path();
	std::string get_user_path();

	/* this is subject to wordexp, so we need
	   to keep the original (user-entered) form
	   around. e.g. ~/blah-> /home/foo/blah
	*/
	
	std::string raid_path;
	bool   raid_path_is_user;
	std::string orig_raid_path;

	uint32_t minimum_disk_io_bytes;
	bool          minimum_disk_io_bytes_is_user;
	float         track_buffer_seconds;
	bool          track_buffer_seconds_is_user;
	bool          hiding_groups_deactivates_groups;
	bool          hiding_groups_deactivates_groups_is_user;
	std::string   auditioner_output_left;
	bool          auditioner_output_left_is_user;
	std::string   auditioner_output_right;
	bool          auditioner_output_right_is_user;
	bool	      mute_affects_pre_fader;
	bool          mute_affects_pre_fader_is_user;
	bool	      mute_affects_post_fader;
	bool          mute_affects_post_fader_is_user;
	bool	      mute_affects_control_outs;
	bool          mute_affects_control_outs_is_user;
	bool	      mute_affects_main_outs;
	bool          mute_affects_main_outs_is_user;
	bool	      solo_latch;
	bool          solo_latch_is_user;
	uint32_t disk_choice_space_threshold;
	bool          disk_choice_space_threshold_is_user;
	std::string   mtc_port_name;
	bool          mtc_port_name_is_user;
	std::string   mmc_port_name;
	bool          mmc_port_name_is_user;
	std::string   midi_port_name;
	bool          midi_port_name_is_user;
	bool          use_hardware_monitoring;
	bool          use_hardware_monitoring_is_user;
	bool          be_jack_time_master;
	bool          be_jack_time_master_is_user;
	bool          native_format_is_bwf;
	bool          native_format_is_bwf_is_user;
	bool          trace_midi_input;
	bool          trace_midi_input_is_user;
	bool          trace_midi_output;
	bool          trace_midi_output_is_user;
	bool          plugins_stop_with_transport;
	bool          plugins_stop_with_transport_is_user;
	bool          use_sw_monitoring;
	bool          use_sw_monitoring_is_user;
	bool          stop_recording_on_xrun;
	bool          stop_recording_on_xrun_is_user;
	bool	      verify_remove_last_capture;
	bool	      verify_remove_last_capture_is_user;
	bool          stop_at_session_end;
	bool          stop_at_session_end_is_user;
	bool          seamless_looping;
	bool          seamless_looping_is_user;
	bool          auto_xfade;
	bool          auto_xfade_is_user;
	bool	      no_new_session_dialog;
	bool	      no_new_session_dialog_is_user;
	uint32_t      timecode_skip_limit;
	bool          timecode_skip_limit_is_user;
	bool          timecode_source_is_synced;
	bool          timecode_source_is_synced_is_user;
	bool          use_vst; /* always per-user */
	bool          quieten_at_speed;
	bool          quieten_at_speed_is_user;
	uint32_t      midi_feedback_interval_ms;
	bool          midi_feedback_interval_ms_is_user;
	bool          latched_record_enable;
	bool          latched_record_enable_is_user;
	uint32_t      destructive_xfade_msecs;
	bool          destructive_xfade_msecs_is_user;

	XMLNode *key_node;
	bool     user_configuration;

	XMLNode& state (bool user_only);
};

extern Configuration *Config;
extern gain_t speed_quietning; /* see comment in configuration.cc */

}; /* namespace ARDOUR */

#endif /* __ardour_configuration_h__ */
