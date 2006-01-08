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

#include <unistd.h>
#include <cstdio> /* for snprintf, grrr */

#ifdef HAVE_WORDEXP
#include <wordexp.h>
#endif

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/diskstream.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

/* this is global so that we do not have to indirect through an object pointer
   to reference it.
*/

namespace ARDOUR {
    float speed_quietning = 0.251189; // -12dB reduction for ffwd or rewind
}

Configuration::Configuration ()
{
	key_node = 0;
	user_configuration = false;
	set_defaults ();
}

Configuration::~Configuration ()
{
}
	
string
Configuration::get_user_path()
{
	char *envvar;

	if ((envvar = getenv ("ARDOUR_RC")) != 0) {
		return envvar;
	}

	return find_config_file ("ardour.rc");
}

string
Configuration::get_system_path()
{
	char* envvar;

	if ((envvar = getenv ("ARDOUR_SYSTEM_RC")) != 0) {
		return envvar;
	}

	return find_config_file ("ardour_system.rc");
}

int
Configuration::load_state ()
{
	string rcfile;
	
	/* load system configuration first */

	rcfile = get_system_path ();

	if (rcfile.length()) {

		XMLTree tree;

		cerr << "Loading system configuration file " << rcfile << endl;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("Ardour: cannot read system configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root())) {
			error << string_compose(_("Ardour: system configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	/* from this point on, all configuration changes are user driven */

	user_configuration = true;

	/* now load configuration file for user */
	
	rcfile = get_user_path ();

	if (rcfile.length()) {

		XMLTree tree;

		cerr << "Loading user configuration file " << rcfile << endl;

		if (!tree.read (rcfile)) {
			error << string_compose(_("Ardour: cannot read configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}
		
		
		if (set_state (*tree.root())) {
			error << string_compose(_("Ardour: configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
Configuration::save_state()
{
	XMLTree tree;
	string rcfile;
	char *envvar;

	/* Note: this only writes the per-user file, and therefore
	   only saves variables marked as user-set or modified
	*/

	if ((envvar = getenv ("ARDOUR_RC")) != 0) {
		if (strlen (envvar) == 0) {
			return -1;
		}
		rcfile = envvar;
	} else {

		if ((envvar = getenv ("HOME")) == 0) {
			return -1;
		}
		if (strlen (envvar) == 0) {
			return -1;
		}
		rcfile = envvar;
		rcfile += "/.ardour/ardour.rc";
	}

	if (rcfile.length()) {
		tree.set_root (&state (true));
		if (!tree.write (rcfile.c_str())){
			error << _("Config file not saved") << endmsg;
			return -1;
		}
	}

	return 0;
}

XMLNode&
Configuration::get_state ()
{
	return state (false);
}

XMLNode&
Configuration::state (bool user_only)
{
	XMLNode* root = new XMLNode("Ardour");
	LocaleGuard lg (X_("POSIX"));

	typedef map<string, MidiPortDescriptor*>::const_iterator CI;
	for(CI m = midi_ports.begin(); m != midi_ports.end(); ++m){
		root->add_child_nocopy(m->second->get_state());
	}

	XMLNode* node = new XMLNode("Config");
	char buf[32];
	
	if (!user_only || minimum_disk_io_bytes_is_user) {
		snprintf(buf, sizeof(buf), "%" PRIu32 , minimum_disk_io_bytes);
		node->add_child_nocopy(option_node("minimum-disk-io-bytes", string(buf)));
	}
	if (!user_only || track_buffer_seconds_is_user) {
		snprintf(buf, sizeof(buf), "%f", track_buffer_seconds);
		node->add_child_nocopy(option_node("track-buffer-seconds", string(buf)));
	}
	if (!user_only || disk_choice_space_threshold_is_user) {
		snprintf(buf, sizeof(buf), "%" PRIu32, disk_choice_space_threshold);
		node->add_child_nocopy(option_node("disk-choice-space-threshold", string(buf)));
	}

	if (!user_only || midi_feedback_interval_ms_is_user) {
		snprintf(buf, sizeof(buf), "%" PRIu32, midi_feedback_interval_ms);
		node->add_child_nocopy(option_node("midi-feedback-interval-ms", string(buf)));
	}

	if (!user_only || mute_affects_pre_fader_is_user) {
		node->add_child_nocopy(option_node("mute-affects-pre-fader", mute_affects_pre_fader?"yes":"no"));
	}
	if (!user_only || mute_affects_post_fader_is_user) {
		node->add_child_nocopy(option_node("mute-affects-post-fader", mute_affects_post_fader?"yes":"no"));
	}
	if (!user_only || mute_affects_control_outs_is_user) {
		node->add_child_nocopy(option_node("mute-affects-control-outs", mute_affects_control_outs?"yes":"no"));
	}
	if (!user_only || mute_affects_main_outs_is_user) {
		node->add_child_nocopy(option_node("mute-affects-main-outs", mute_affects_main_outs?"yes":"no"));
	}
	if (!user_only || solo_latch_is_user) {
		node->add_child_nocopy(option_node("solo-latch", solo_latch?"yes":"no"));
	}
	if (!user_only || raid_path_is_user) {
		node->add_child_nocopy(option_node("raid-path", orig_raid_path));
	}
	if (!user_only || mtc_port_name_is_user) {
		node->add_child_nocopy(option_node("mtc-port", mtc_port_name));
	}
	if (!user_only || mmc_port_name_is_user) {
		node->add_child_nocopy(option_node("mmc-port", mmc_port_name));
	}
	if (!user_only || midi_port_name_is_user) {
		node->add_child_nocopy(option_node("midi-port", midi_port_name));
	}
	if (!user_only || use_hardware_monitoring_is_user) {
		node->add_child_nocopy(option_node("hardware-monitoring", use_hardware_monitoring?"yes":"no"));
	}
	if (!user_only || be_jack_time_master_is_user) {
		node->add_child_nocopy(option_node("jack-time-master", be_jack_time_master?"yes":"no"));
	}
	if (!user_only || native_format_is_bwf_is_user) {
		node->add_child_nocopy(option_node("native-format-bwf", native_format_is_bwf?"yes":"no"));
	}
	if (!user_only || trace_midi_input_is_user) {
		node->add_child_nocopy(option_node("trace-midi-input", trace_midi_input?"yes":"no"));
	}
	if (!user_only || trace_midi_output_is_user) {
		node->add_child_nocopy(option_node("trace-midi-output", trace_midi_output?"yes":"no"));
	}
	if (!user_only || plugins_stop_with_transport_is_user) {
		node->add_child_nocopy(option_node("plugins-stop-with-transport", plugins_stop_with_transport?"yes":"no"));
	}
	if (!user_only || use_sw_monitoring_is_user) {
		node->add_child_nocopy(option_node("use-sw-monitoring", use_sw_monitoring?"yes":"no"));
	}
	if (!user_only || stop_recording_on_xrun_is_user) {
		node->add_child_nocopy(option_node("stop-recording-on-xrun", stop_recording_on_xrun?"yes":"no"));
	}
	if (!user_only || verify_remove_last_capture_is_user) {
		node->add_child_nocopy(option_node("verify-remove-last-capture", verify_remove_last_capture?"yes":"no"));
	}
	if (!user_only || stop_at_session_end_is_user) {
		node->add_child_nocopy(option_node("stop-at-session-end", stop_at_session_end?"yes":"no"));
	}
	if (!user_only || seamless_looping_is_user) {
		node->add_child_nocopy(option_node("seamless-loop", seamless_looping?"yes":"no"));
	}
	if (!user_only || auto_xfade_is_user) {
		node->add_child_nocopy(option_node("auto-xfade", auto_xfade?"yes":"no"));
	}
	if (!user_only || no_new_session_dialog_is_user) {
		node->add_child_nocopy(option_node("no-new-session-dialog", no_new_session_dialog?"yes":"no"));
	}
	if (!user_only || timecode_source_is_synced_is_user) {
		node->add_child_nocopy(option_node("timecode-source-is-synced", timecode_source_is_synced?"yes":"no"));
	}
	if (!user_only || auditioner_output_left_is_user) {
		node->add_child_nocopy(option_node("auditioner-left-out", auditioner_output_left));
	}
	if (!user_only || auditioner_output_right_is_user) {
		node->add_child_nocopy(option_node("auditioner-right-out", auditioner_output_right));
	}
	if (!user_only || quieten_at_speed_is_user) {
		snprintf (buf, sizeof (buf), "%f", speed_quietning);
		node->add_child_nocopy(option_node("quieten-at-speed", buf));
	}

	/* use-vst is always per-user */
	node->add_child_nocopy (option_node ("use-vst", use_vst?"yes":"no"));

	root->add_child_nocopy (*node);

	if (key_node) {
		root->add_child_copy (*key_node);
	}

	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}

	return *root;
}

int
Configuration::set_state (const XMLNode& root)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;
	XMLProperty *prop;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "MIDI-port") {

			try {
				pair<string,MidiPortDescriptor*> newpair;
				newpair.second = new MidiPortDescriptor (*node);
				newpair.first = newpair.second->tag;
				midi_ports.insert (newpair);
			}

			catch (failed_constructor& err) {
				warning << _("ill-formed MIDI port specification in ardour rcfile (ignored)") << endmsg;
			}

		} else if (node->name() == "Config") {
			
			XMLNodeList option_list = node->children();
			XMLNodeConstIterator option_iter;
			XMLNode *option_node;

			string option_name;
			string option_value;

			for (option_iter = option_list.begin(); option_iter != option_list.end(); ++option_iter) {

				option_node = *option_iter;

				if (option_node->name() != "Option") {
					continue;
				}

				if ((prop = option_node->property ("name")) != 0) {
					option_name = prop->value();
				} else {
					throw failed_constructor ();
				}
				
				if ((prop = option_node->property ("value")) != 0) {
					option_value = prop->value();
				} else {
					throw failed_constructor ();
				}
				
				if (option_name == "minimum-disk-io-bytes") {
					set_minimum_disk_io (atoi (option_value.c_str()));
				} else if (option_name == "track-buffer-seconds") {
					set_track_buffer (atof (option_value.c_str()));
				} else if (option_name == "raid-path") {
					set_raid_path (option_value);
				} else if (option_name == "hiding-groups-deactivates-groups") {
					set_hiding_groups_deactivates_groups (option_value == "yes");
				} else if (option_name == "mute-affects-pre-fader") {
					set_mute_affects_pre_fader (option_value == "yes");
				} else if (option_name == "mute-affects-post-fader") {
					set_mute_affects_post_fader (option_value == "yes");
				} else if (option_name == "mute-affects-control-outs") {
					set_mute_affects_control_outs (option_value == "yes");
				} else if (option_name == "mute-affects-main-outs") {
					set_mute_affects_main_outs (option_value == "yes");
				} else if (option_name == "solo-latch") {
					set_solo_latch (option_value == "yes");
				} else if (option_name == "mtc-port") {
					set_mtc_port_name (option_value);
				} else if (option_name == "mmc-port") {
					set_mmc_port_name (option_value);
				} else if (option_name == "midi-port") {
					set_midi_port_name (option_value);
				} else if (option_name == "hardware-monitoring") {
					set_use_hardware_monitoring (option_value == "yes");
				} else if (option_name == "jack-time-master") {
					set_jack_time_master (option_value == "yes");
				} else if (option_name == "trace-midi-input") {
					set_trace_midi_input (option_value == "yes");
				} else if (option_name == "trace-midi-output") {
					set_trace_midi_output (option_value == "yes");
				} else if (option_name == "plugins-stop-with-transport") {
					set_plugins_stop_with_transport (option_value == "yes");
				} else if (option_name == "use-sw-monitoring") {
					set_use_sw_monitoring (option_value == "yes");
				} else if (option_name == "no-sw-monitoring") {     /* DEPRECATED */
					set_use_sw_monitoring (option_value != "yes");
				} else if (option_name == "stop-recording-on-xrun") {
					set_stop_recording_on_xrun (option_value == "yes");
				} else if (option_name == "verify-remove-last-capture") {
					set_verify_remove_last_capture (option_value == "yes");
				} else if (option_name == "stop-at-session-end") {
					set_stop_at_session_end (option_value == "yes");
				} else if (option_name == "seamless-loop") {
					set_seamless_looping (option_value == "yes");
				} else if (option_name == "auto-xfade") {
					set_auto_xfade (option_value == "yes");
				} else if (option_name == "no-new-session-dialog") {
					set_no_new_session_dialog (option_value == "yes");
				} else if (option_name == "timecode-source-is-synced") {
					set_timecode_source_is_synced (option_value == "yes");
				} else if (option_name == "auditioner-left-out") {
					set_auditioner_output_left (option_value);
				} else if (option_name == "auditioner-right-out") {
					set_auditioner_output_right (option_value);
				} else if (option_name == "use-vst") {
					set_use_vst (option_value == "yes");
				} else if (option_name == "quieten-at-speed") {
					float v;
					if (sscanf (option_value.c_str(), "%f", &v) == 1) {
						set_quieten_at_speed (v);
					}
				} else if (option_name == "midi-feedback-interval-ms") {
					set_midi_feedback_interval_ms (atoi (option_value.c_str()));
				}
			}
			
		} else if (node->name() == "Keys") {
			/* defer handling of this for UI objects */
			key_node = new XMLNode (*node);
		} else if (node->name() == "extra") {
			_extra_xml = new XMLNode (*node);
		}
	}

	DiskStream::set_disk_io_chunk_frames (minimum_disk_io_bytes / sizeof (Sample));

	return 0;
}

void
Configuration::set_defaults ()
{
	raid_path = "";
	orig_raid_path = raid_path;

	mtc_port_name = N_("default");
	mmc_port_name = N_("default");
	midi_port_name = N_("default");
#ifdef __APPLE__
	auditioner_output_left = N_("coreaudio:Built-in Audio:in1");
	auditioner_output_right = N_("coreaudio:Built-in Audio:in2");
#else
	auditioner_output_left = N_("alsa_pcm:playback_1");
	auditioner_output_right = N_("alsa_pcm:playback_2");
#endif
	minimum_disk_io_bytes = 1024 * 256;
	track_buffer_seconds = 5.0;
	hiding_groups_deactivates_groups = true;
	mute_affects_pre_fader = 1;
	mute_affects_post_fader = 1;
	mute_affects_control_outs = 1;
	mute_affects_main_outs = 1;
	solo_latch = 1;
	use_hardware_monitoring = true;
	be_jack_time_master = true;
	native_format_is_bwf = true;
	trace_midi_input = false;
	trace_midi_output = false;
	plugins_stop_with_transport = false;
	use_sw_monitoring = true;
	stop_recording_on_xrun = false;
	verify_remove_last_capture = true;
	stop_at_session_end = true;
	seamless_looping = true;
	auto_xfade = true;
	no_new_session_dialog = false;
	timecode_source_is_synced = true;
	use_vst = true; /* if we build with VST_SUPPORT, otherwise no effect */
	quieten_at_speed = true;

	midi_feedback_interval_ms = 100;
	
	// this is about 5 minutes at 48kHz, 4 bytes/sample
	disk_choice_space_threshold = 57600000;

	/* at this point, no variables from from the user */

	raid_path_is_user = false;
	minimum_disk_io_bytes_is_user = false;
	track_buffer_seconds_is_user = false;
	hiding_groups_deactivates_groups_is_user = false;
	auditioner_output_left_is_user = false;
	auditioner_output_right_is_user = false;
	mute_affects_pre_fader_is_user = false;
	mute_affects_post_fader_is_user = false;
	mute_affects_control_outs_is_user = false;
	mute_affects_main_outs_is_user = false;
	solo_latch_is_user = false;
	disk_choice_space_threshold_is_user = false;
	mtc_port_name_is_user = false;
	mmc_port_name_is_user = false;
	midi_port_name_is_user = false;
	use_hardware_monitoring_is_user = false;
	be_jack_time_master_is_user = false;
	native_format_is_bwf_is_user = false;
	trace_midi_input_is_user = false;
	trace_midi_output_is_user = false;
	plugins_stop_with_transport_is_user = false;
	use_sw_monitoring_is_user = false;
	stop_recording_on_xrun_is_user = false;
	verify_remove_last_capture_is_user = false;
	stop_at_session_end_is_user = false;
	seamless_looping_is_user = false;
	auto_xfade_is_user = false;
	no_new_session_dialog_is_user = false;
	timecode_source_is_synced_is_user = false;
	quieten_at_speed_is_user = false;
	midi_feedback_interval_ms_is_user = false;
}

Configuration::MidiPortDescriptor::MidiPortDescriptor (const XMLNode& node)
{
	const XMLProperty *prop;
	bool have_tag = false;
	bool have_device = false;
	bool have_type = false;
	bool have_mode = false;

	if ((prop = node.property ("tag")) != 0) {
		tag = prop->value();
		have_tag = true;
	}

	if ((prop = node.property ("device")) != 0) {
		device = prop->value();
		have_device = true;
	}

	if ((prop = node.property ("type")) != 0) {
		type = prop->value();
		have_type = true;
	}

	if ((prop = node.property ("mode")) != 0) {
		mode = prop->value();
		have_mode = true;
	}

	if (!have_tag || !have_device || !have_type || !have_mode) {
		throw failed_constructor();
	}
}

XMLNode&
Configuration::MidiPortDescriptor::get_state()
{
	XMLNode* root = new XMLNode("MIDI-port");

	root->add_property("tag", tag);
	root->add_property("device", device);
	root->add_property("type", type);
	root->add_property("mode", mode);

	return *root;
}

XMLNode&
Configuration::option_node(const string & name, const string & value)
{
	XMLNode* root = new XMLNode("Option");

	root->add_property("name", name);
	root->add_property("value", value);
	
	return *root;
}

string
Configuration::get_raid_path()
{
	return raid_path;
}

void
Configuration::set_raid_path(string path)
{
#ifdef HAVE_WORDEXP
	/* Handle tilde and environment variable expansion in session path */
	wordexp_t expansion;
	switch (wordexp (path.c_str(), &expansion, WRDE_NOCMD|WRDE_UNDEF)) {
	case 0:
		break;
	default:
		error << _("illegal or badly-formed string used for RAID path") << endmsg;
		return;
	}

	if (expansion.we_wordc > 1) {
		error << _("RAID search path is ambiguous") << endmsg;
		return;
	}

	raid_path = expansion.we_wordv[0];
	orig_raid_path = path;
	wordfree (&expansion);
#else
	raid_path = orig_raid_path = path;
#endif

	if (user_configuration) {
		raid_path_is_user = true;
	}
}

uint32_t
Configuration::get_minimum_disk_io()
{
	return minimum_disk_io_bytes;
}
	
void
Configuration::set_minimum_disk_io(uint32_t min)
{
	minimum_disk_io_bytes = min;
	if (user_configuration) {
		minimum_disk_io_bytes_is_user = true;
	}
}

float
Configuration::get_track_buffer()
{
	return track_buffer_seconds;
}

void 
Configuration::set_track_buffer(float buffer)
{
	track_buffer_seconds = buffer;
	if (user_configuration) {
		track_buffer_seconds_is_user = true;
	}
}

bool 
Configuration::does_hiding_groups_deactivates_groups()
{
	return hiding_groups_deactivates_groups;
}

void 
Configuration::set_hiding_groups_deactivates_groups(bool hiding)
{
	hiding_groups_deactivates_groups = hiding;
	if (user_configuration) {
		hiding_groups_deactivates_groups_is_user = true;
	}
}

string
Configuration::get_auditioner_output_left ()
{
	return auditioner_output_left;
}

void
Configuration::set_auditioner_output_left (string str)
{
	auditioner_output_left = str;
	if (user_configuration) {
		auditioner_output_left_is_user = true;
	}
}

string
Configuration::get_auditioner_output_right ()
{
	return auditioner_output_right;
}

void
Configuration::set_auditioner_output_right (string str)
{
	auditioner_output_right = str;
	if (user_configuration) {
		auditioner_output_right_is_user = true;
	}
}

bool
Configuration::get_mute_affects_pre_fader()
{
	return mute_affects_pre_fader;
}

void 
Configuration::set_mute_affects_pre_fader (bool affects)
{
	mute_affects_pre_fader = affects;
	if (user_configuration) {
		mute_affects_pre_fader_is_user = true;
	}
}

bool 
Configuration::get_mute_affects_post_fader()
{
	return mute_affects_post_fader;
}

void 
Configuration::set_mute_affects_post_fader (bool affects)
{
	mute_affects_post_fader = affects;
	if (user_configuration) {
		mute_affects_post_fader_is_user = true;
	}
}

bool 
Configuration::get_mute_affects_control_outs()
{
	return mute_affects_control_outs;
}

void 
Configuration::set_mute_affects_control_outs (bool affects)
{
	mute_affects_control_outs = affects;
	if (user_configuration) {
		mute_affects_control_outs_is_user = true;
	}
}

bool 
Configuration::get_mute_affects_main_outs()
{
	return mute_affects_main_outs;
}

void 
Configuration::set_mute_affects_main_outs (bool affects)
{
	mute_affects_main_outs = affects;
	if (user_configuration) {
		mute_affects_main_outs_is_user = true;
	}
}

bool 
Configuration::get_solo_latch()
{
	return solo_latch;
}

void 
Configuration::set_solo_latch (bool latch)
{
	solo_latch = latch;
	if (user_configuration) {
		solo_latch_is_user = true;
	}
}

XMLNode *
Configuration::get_keys () const
{
	return key_node;
}

void
Configuration::set_keys (XMLNode* keys)
{
	key_node = keys;
}

uint32_t
Configuration::get_disk_choice_space_threshold ()
{
	return disk_choice_space_threshold;
}

void
Configuration::set_disk_choice_space_threshold (uint32_t val)
{
	disk_choice_space_threshold = val;
	if (user_configuration) {
		disk_choice_space_threshold_is_user = true;
	}
}

string
Configuration::get_mmc_port_name ()
{
	return mmc_port_name;
}

void
Configuration::set_mmc_port_name (string name)
{
	mmc_port_name = name;
	if (user_configuration) {
		mmc_port_name_is_user = true;
	}
}

string
Configuration::get_mtc_port_name ()
{
	return mtc_port_name;
}

void
Configuration::set_mtc_port_name (string name)
{
	mtc_port_name = name;
	if (user_configuration) {
		mtc_port_name_is_user = true;
	}
}

string
Configuration::get_midi_port_name ()
{
	return midi_port_name;
}

void
Configuration::set_midi_port_name (string name)
{
	midi_port_name = name;
	if (user_configuration) {
		midi_port_name_is_user = true;
	}
}

uint32_t
Configuration::get_midi_feedback_interval_ms ()
{
	return midi_feedback_interval_ms;
}

void
Configuration::set_midi_feedback_interval_ms (uint32_t val)
{
	midi_feedback_interval_ms = val;
	if (user_configuration) {
		midi_feedback_interval_ms_is_user = true;
	}
}

bool
Configuration::get_use_hardware_monitoring()
{
	return use_hardware_monitoring;
}

void
Configuration::set_use_hardware_monitoring(bool yn)
{
	use_hardware_monitoring = yn;
	if (user_configuration) {
		use_hardware_monitoring_is_user = true;
	}
}

bool
Configuration::get_jack_time_master()
{
	return be_jack_time_master;
}

void
Configuration::set_jack_time_master(bool yn)
{
	be_jack_time_master = yn;
	if (user_configuration) {
		be_jack_time_master_is_user = true;
	}
}

bool
Configuration::get_native_format_is_bwf()
{
	return native_format_is_bwf;
}

void
Configuration::set_native_format_is_bwf(bool yn)
{
	native_format_is_bwf = yn;
	if (user_configuration) {
		native_format_is_bwf_is_user = true;
	}
}

bool
Configuration::get_trace_midi_input ()
{
	return trace_midi_input;
}

void
Configuration::set_trace_midi_input (bool yn)
{
	trace_midi_input = yn;
	if (user_configuration) {
		trace_midi_input_is_user = true;
	}
}

bool
Configuration::get_trace_midi_output ()
{
	return trace_midi_output;
}

void
Configuration::set_trace_midi_output (bool yn)
{
	trace_midi_output = yn;
	if (user_configuration) {
		trace_midi_output_is_user = true;
	}
}

bool
Configuration::get_plugins_stop_with_transport ()
{
	return plugins_stop_with_transport;
}

void
Configuration::set_plugins_stop_with_transport (bool yn)
{
	plugins_stop_with_transport = yn;
	if (user_configuration) {
		plugins_stop_with_transport_is_user = true;
	}
}

bool
Configuration::get_use_sw_monitoring ()
{
	return use_sw_monitoring;
}

void
Configuration::set_use_sw_monitoring (bool yn)
{
	use_sw_monitoring = yn;
	if (user_configuration) {
		use_sw_monitoring_is_user = true;
	}
}

bool
Configuration::get_stop_recording_on_xrun ()
{
	return stop_recording_on_xrun;
}

void
Configuration::set_stop_recording_on_xrun (bool yn)
{
	stop_recording_on_xrun = yn;
	if (user_configuration) {
		stop_recording_on_xrun_is_user = true;
	}
}

bool
Configuration::get_verify_remove_last_capture ()
{
	return verify_remove_last_capture;
}

void
Configuration::set_verify_remove_last_capture (bool yn)
{
	verify_remove_last_capture = yn;
	if (user_configuration) {
		verify_remove_last_capture_is_user = true;
	}
}

bool
Configuration::get_stop_at_session_end ()
{
	return stop_at_session_end;
}

void
Configuration::set_stop_at_session_end (bool yn)
{
	stop_at_session_end = yn;
	if (user_configuration) {
		stop_at_session_end_is_user = true;
	}
}

bool
Configuration::get_seamless_looping ()
{
	return seamless_looping;
}

void
Configuration::set_seamless_looping (bool yn)
{
	seamless_looping = yn;
	if (user_configuration) {
		seamless_looping_is_user = true;
	}
}

bool
Configuration::get_auto_xfade ()
{
	return auto_xfade;
}

void
Configuration::set_auto_xfade (bool yn)
{
	auto_xfade = yn;
	if (user_configuration) {
		auto_xfade_is_user = true;
	}
}

string
Configuration::get_user_ardour_path ()
{
	string path;
	char* envvar;
	
	if ((envvar = getenv ("HOME")) == 0 || strlen (envvar) == 0) {
		return "/";
	}
		
	path = envvar;
	path += "/.ardour/";
	
	return path;
}

string
Configuration::get_system_ardour_path ()
{
	string path;
	char* envvar;

	if ((envvar = getenv ("ARDOUR_DATA_PATH")) != 0) {
		path += envvar;
		if (path[path.length()-1] != ':') {
			path += ':';
		}
	}

	path += DATA_DIR;
	path += "/ardour/";
	
	return path;
}

bool 
Configuration::get_no_new_session_dialog()
{
	return no_new_session_dialog;
}

void 
Configuration::set_no_new_session_dialog(bool yn)
{
	no_new_session_dialog = yn;
	if (user_configuration) {
		no_new_session_dialog_is_user = true;
	}
}

bool
Configuration::get_timecode_source_is_synced()
{
	return timecode_source_is_synced;
}

void
Configuration::set_timecode_source_is_synced (bool yn)
{
	timecode_source_is_synced = yn;
	if (user_configuration) {
		timecode_source_is_synced_is_user = true;
	}
}

bool
Configuration::get_use_vst ()
{
	return use_vst;
}

void
Configuration::set_use_vst (bool yn)
{
	use_vst = yn;
}

gain_t
Configuration::get_quieten_at_speed()
{
	return speed_quietning;
}

void
Configuration::set_quieten_at_speed (float gain_coefficient)
{
	speed_quietning = gain_coefficient;
	if (user_configuration) {
		quieten_at_speed_is_user = true;
	}
}
