/*
    Copyright (C) 2005 Paul Davis 

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

#include <pbd/convert.h>

#include <gtkmm2ext/utils.h>

#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>

#include "ardour_ui.h"
#include "actions.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

void
ARDOUR_UI::toggle_config_state (const char* group, const char* action, bool (Configuration::*set)(bool), bool (Configuration::*get)(void) const)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {
			bool x = (Config->*get)();

			cerr << "\ttoggle config, action = " << tact->get_active() << " config = " << x << endl;
			
			if (x != tact->get_active()) {
				(Config->*set) (!x);
			}
		}
	}
}

void
ARDOUR_UI::toggle_config_state (const char* group, const char* action, sigc::slot<void> theSlot)
{
	if (session) {
		Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
		if (act) {
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
			if (tact->get_active()) {
				theSlot ();
			}
		}
	}
}

void
ARDOUR_UI::toggle_time_master ()
{
	toggle_config_state ("Transport", "ToggleTimeMaster", &Configuration::set_jack_time_master, &Configuration::get_jack_time_master);
	if (session) {
		session->engine().reset_timebase ();
	}
}

void
ARDOUR_UI::toggle_send_mtc ()
{
	toggle_config_state ("options", "SendMTC", &Configuration::set_send_mtc, &Configuration::get_send_mtc);
}

void
ARDOUR_UI::toggle_send_mmc ()
{
	toggle_config_state ("options", "SendMMC", &Configuration::set_send_mmc, &Configuration::get_send_mmc);
}

void
ARDOUR_UI::toggle_use_mmc ()
{
	toggle_config_state ("options", "UseMMC", &Configuration::set_mmc_control, &Configuration::get_mmc_control);
}

void
ARDOUR_UI::toggle_use_midi_control ()
{
	toggle_config_state ("options", "UseMIDIcontrol", &Configuration::set_midi_control, &Configuration::get_midi_control);
}

void
ARDOUR_UI::toggle_send_midi_feedback ()
{
	toggle_config_state ("options", "SendMIDIfeedback", &Configuration::set_midi_feedback, &Configuration::get_midi_feedback);
}

void
ARDOUR_UI::toggle_AutoConnectNewTrackInputsToHardware()
{
	toggle_config_state ("options", "AutoConnectNewTrackInputsToHardware", hide_return (bind (mem_fun (*Config, &Configuration::set_input_auto_connect), AutoConnectPhysical)));
}
void
ARDOUR_UI::toggle_AutoConnectNewTrackOutputsToHardware()
{
	toggle_config_state ("options", "AutoConnectNewTrackOutputsToHardware", hide_return (bind (mem_fun (*Config, &Configuration::set_output_auto_connect), AutoConnectPhysical)));
}
void
ARDOUR_UI::toggle_AutoConnectNewTrackOutputsToMaster()
{
	toggle_config_state ("options", "AutoConnectNewTrackOutputsToHardware", hide_return (bind (mem_fun (*Config, &Configuration::set_output_auto_connect), AutoConnectMaster)));
}
void
ARDOUR_UI::toggle_ManuallyConnectNewTrackOutputs()
{
	toggle_config_state ("options", "AutoConnectNewTrackOutputsToHardware", hide_return (bind (mem_fun (*Config, &Configuration::set_output_auto_connect), AutoConnectOption (0))));
}

void
ARDOUR_UI::toggle_auto_input ()
{
	toggle_config_state ("Transport", "ToggleAutoInput", &Configuration::set_auto_input, &Configuration::get_auto_input);
}

void
ARDOUR_UI::toggle_auto_play ()
{
	toggle_config_state ("Transport", "ToggleAutoPlay", &Configuration::set_auto_play, &Configuration::get_auto_play);
}

void
ARDOUR_UI::toggle_auto_return ()
{
	toggle_config_state ("Transport", "ToggleAutoReturn", &Configuration::set_auto_return, &Configuration::get_auto_return);
}

void
ARDOUR_UI::toggle_click ()
{
	toggle_config_state ("Transport", "ToggleClick", &Configuration::set_clicking, &Configuration::get_clicking);
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (session) {
		if (Config->get_auto_loop()) {
			if (session->transport_rolling()) {
				transport_roll();
			} else {
				session->request_play_loop (false);
			}
		} else {
			session->request_play_loop (true);
		}
	}
}

void
ARDOUR_UI::toggle_punch_in ()
{
	toggle_config_state ("Transport", "TogglePunchIn", &Configuration::set_punch_in, &Configuration::get_punch_in);
}

void
ARDOUR_UI::toggle_punch_out ()
{
	toggle_config_state ("Transport", "TogglePunchOut", &Configuration::set_punch_out, &Configuration::get_punch_out);
}

void
ARDOUR_UI::toggle_video_sync()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", "ToggleVideoSync");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		Config->set_use_video_sync (tact->get_active());
	}
}

void
ARDOUR_UI::toggle_editing_space()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Common", "ToggleMaximalEditor");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			maximise_editing_space ();
		} else {
			restore_editing_space ();
		}
	}
}

void
ARDOUR_UI::toggle_UseHardwareMonitoring()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("options", "UseHardwareMonitoring");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			Config->set_use_hardware_monitoring (true);
			Config->set_use_sw_monitoring (false);
			Config->set_use_external_monitoring (false);
			if (session) {
				session->reset_input_monitor_state();
			}
		}
	}
}

void
ARDOUR_UI::toggle_UseSoftwareMonitoring()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("options", "UseSoftwareMonitoring");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			Config->set_use_hardware_monitoring (false);
			Config->set_use_sw_monitoring (true);
			Config->set_use_external_monitoring (false);
			if (session) {
				session->reset_input_monitor_state();
			}
		}
	}
}

void
ARDOUR_UI::toggle_UseExternalMonitoring()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("options", "UseExternalMonitoring");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			Config->set_use_hardware_monitoring (false);
			Config->set_use_sw_monitoring (false);
			Config->set_use_external_monitoring (true);
			if (session) {
				session->reset_input_monitor_state();
			}
		}
	}
}

void
ARDOUR_UI::toggle_StopPluginsWithTransport()
{
	toggle_config_state ("options", "StopPluginsWithTransport", &Configuration::set_plugins_stop_with_transport, &Configuration::get_plugins_stop_with_transport);
}

void
ARDOUR_UI::toggle_LatchedRecordEnable()
{
	toggle_config_state ("options", "LatchedRecordEnable", &Configuration::set_latched_record_enable, &Configuration::get_latched_record_enable);
}

void
ARDOUR_UI::toggle_DoNotRunPluginsWhileRecording()
{
	toggle_config_state ("options", "DoNotRunPluginsWhileRecording", &Configuration::set_do_not_record_plugins, &Configuration::get_do_not_record_plugins);
}

void
ARDOUR_UI::toggle_VerifyRemoveLastCapture()
{
	toggle_config_state ("options", "VerifyRemoveLastCapture", &Configuration::set_verify_remove_last_capture, &Configuration::get_verify_remove_last_capture);
}

void
ARDOUR_UI::toggle_StopRecordingOnXrun()
{
	toggle_config_state ("options", "StopRecordingOnXrun", &Configuration::set_stop_recording_on_xrun, &Configuration::get_stop_recording_on_xrun);
}

void
ARDOUR_UI::toggle_StopTransportAtEndOfSession()
{
	toggle_config_state ("options", "StopTransportAtEndOfSession", &Configuration::set_stop_at_session_end, &Configuration::get_stop_at_session_end);
}

void
ARDOUR_UI::toggle_GainReduceFastTransport()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("options", "GainReduceFastTransport");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			Config->set_quieten_at_speed (0.251189); // -12dB reduction for ffwd or rewind
		} else {
			Config->set_quieten_at_speed (1.0); /* no change */
		}
	}
}

void
ARDOUR_UI::toggle_LatchedSolo()
{
	toggle_config_state ("options", "LatchedSolo", &Configuration::set_solo_latched, &Configuration::get_solo_latched);
}

void
ARDOUR_UI::toggle_SoloViaBus()
{
	if (!session) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", "SoloViaBus");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact->get_active()) {
			Config->set_solo_model (SoloBus);
		} else {
			Config->set_solo_model (InverseMute);
		}
	}
}

void
ARDOUR_UI::toggle_AutomaticallyCreateCrossfades()
{
}

void
ARDOUR_UI::toggle_UnmuteNewFullCrossfades()
{
}

void
ARDOUR_UI::mtc_port_changed ()
{
	bool have_mtc;

	if (session) {
		if (session->mtc_port()) {
			have_mtc = true;
		} else {
			have_mtc = false;
		}
	} else {
		have_mtc = false;
	}

	positional_sync_strings.clear ();
	positional_sync_strings.push_back (slave_source_to_string (None));
	if (have_mtc) {
		positional_sync_strings.push_back (slave_source_to_string (MTC));
	}
	positional_sync_strings.push_back (slave_source_to_string (JACK));
	
	set_popdown_strings (sync_option_combo, positional_sync_strings);
}

void
ARDOUR_UI::setup_session_options ()
{
	mtc_port_changed ();

	Config->ParameterChanged.connect (mem_fun (*this, &ARDOUR_UI::parameter_changed));
}

void
ARDOUR_UI::map_some_state (const char* group, const char* action, bool (Configuration::*get)() const)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact) {
			
			bool x = (Config->*get)();

			cerr << "\tmap state, action = " << tact->get_active() << " config = " << x << endl;
			
			if (tact->get_active() != x) {
				tact->set_active (x);
			}
		} else {
			cerr << "not a toggle\n";
		}
	} else {
		cerr << group << ':' << action << " not an action\n";
	}
}

void
ARDOUR_UI::parameter_changed (const char* parameter_name)
{
	cerr << "Parameter changed : " << parameter_name << endl;

#define PARAM_IS(x) (!strcmp (parameter_name, (x)))

	if (PARAM_IS ("slave-source")) {

		sync_option_combo.set_active_text (slave_source_to_string (Config->get_slave_source()));

	} else if (PARAM_IS ("send-mtc")) {

		map_some_state ("options", "SendMTC", &Configuration::get_send_mtc);

	} else if (PARAM_IS ("send-mmc")) {

		map_some_state ("options", "SendMMC", &Configuration::get_send_mmc);

	} else if (PARAM_IS ("mmc-control")) {
		map_some_state ("options", "UseMMC", &Configuration::get_mmc_control);
	} else if (PARAM_IS ("midi-feedback")) {
		map_some_state ("options", "SendMIDIfeedback", &Configuration::get_midi_feedback);
	} else if (PARAM_IS ("midi-control")) {
		map_some_state ("options", "UseMIDIcontrol", &Configuration::get_midi_control);
	} else if (PARAM_IS ("do-not-record-plugins")) {
		map_some_state ("options", "DoNotRunPluginsWhileRecording", &Configuration::get_do_not_record_plugins);
	} else if (PARAM_IS ("automatic-crossfades")) {
		map_some_state ("Editor", "toggle-auto-xfades", &Configuration::get_automatic_crossfades);
	} else if (PARAM_IS ("crossfades-active")) {
		map_some_state ("Editor", "toggle-xfades-active", &Configuration::get_crossfades_active);
	} else if (PARAM_IS ("crossfades-visible")) {
		map_some_state ("Editor", "toggle-xfades-visible", &Configuration::get_crossfades_visible);
	} else if (PARAM_IS ("latched-record-enable")) {
		map_some_state ("options", "LatchedRecordEnable", &Configuration::get_latched_record_enable);
	} else if (PARAM_IS ("solo-latched")) {
		map_some_state ("options", "LatchedSolo", &Configuration::get_solo_latched);
	} else if (PARAM_IS ("solo-model")) {
	} else if (PARAM_IS ("layer-model")) {
	} else if (PARAM_IS ("crossfade-model")) {
	} else if (PARAM_IS ("auto-play")) {
		map_some_state ("Transport", "ToggleAutoPlay", &Configuration::get_auto_play);
	} else if (PARAM_IS ("auto-loop")) {
		map_some_state ("Transport", "Loop", &Configuration::get_auto_loop);
	} else if (PARAM_IS ("auto-return")) {
		map_some_state ("Transport", "ToggleAutoReturn", &Configuration::get_auto_return);
	} else if (PARAM_IS ("auto-input")) {
		map_some_state ("Transport", "ToggleAutoInput", &Configuration::get_auto_input);
	} else if (PARAM_IS ("punch-out")) {
		map_some_state ("Transport", "TogglePunchOut", &Configuration::get_punch_out);
	} else if (PARAM_IS ("punch-in")) {
		map_some_state ("Transport", "TogglePunchIn", &Configuration::get_punch_in);
	} else if (PARAM_IS ("clicking")) {
		map_some_state ("Transport", "ToggleClick", &Configuration::get_clicking);
	} else if (PARAM_IS ("jack-time-master")) {
		map_some_state ("Transport",  "ToggleTimeMaster", &Configuration::get_jack_time_master);
	} else if (PARAM_IS ("plugins-stop-with-transport")) {
		map_some_state ("options",  "StopPluginsWithTransport", &Configuration::get_plugins_stop_with_transport);
	} else if (PARAM_IS ("latched-record-enable")) {
		map_some_state ("options", "LatchedRecordEnable", &Configuration::get_latched_record_enable);
	} else if (PARAM_IS ("verify-remove-last-capture")) {
		map_some_state ("options",  "VerifyRemoveLastCapture", &Configuration::get_verify_remove_last_capture);
	} else if (PARAM_IS ("stop-recording-on-xrun")) {
		map_some_state ("options",  "StopRecordingOnXrun", &Configuration::get_stop_recording_on_xrun);
	} else if (PARAM_IS ("stop-at-session-end")) {
		map_some_state ("options",  "StopTransportAtEndOfSession", &Configuration::get_stop_at_session_end);
	} else if (PARAM_IS ("use-hardware-monitoring")) {
		map_some_state ("options",  "UseHardwareMonitoring", &Configuration::get_use_hardware_monitoring);
	} else if (PARAM_IS ("use-sw-monitoring")) {
		map_some_state ("options",  "UseSoftwareMonitoring", &Configuration::get_use_sw_monitoring);
	} else if (PARAM_IS ("use-external-monitoring")) {
		map_some_state ("options",  "UseExternalMonitoring", &Configuration::get_use_external_monitoring);
	} else if (PARAM_IS ("use-video-sync")) {
		map_some_state ("Transport",  "ToggleVideoSync", &Configuration::get_use_video_sync);
	} else if (PARAM_IS ("quieten-at-speed")) {
		map_some_state ("options",  "GainReduceFastTransport", &Configuration::get_quieten_at_speed);
	} else if (PARAM_IS ("shuttle-behaviour")) {

		switch (Config->get_shuttle_behaviour ()) {
		case Sprung:
			shuttle_style_button.set_active_text (_("sprung"));
			shuttle_fract = 0.0;
			shuttle_box.queue_draw ();
			if (session) {
				if (session->transport_rolling()) {
					shuttle_fract = SHUTTLE_FRACT_SPEED1;
					session->request_transport_speed (1.0);
				}
			}
			break;
		case Wheel:
			shuttle_style_button.set_active_text (_("wheel"));
			break;
		}

	} else if (PARAM_IS ("shuttle-units")) {
		
		switch (Config->get_shuttle_units()) {
		case Percentage:
			shuttle_units_button.set_label("% ");
			break;
		case Semitones:
			shuttle_units_button.set_label(_("ST"));
			break;
		}
	}
	
#undef PARAM_IS
}
