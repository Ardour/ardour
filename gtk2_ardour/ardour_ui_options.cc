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

void
ARDOUR_UI::setup_config_options ()
{
	std::vector<Glib::ustring> groups;
	groups.push_back("options");
	groups.push_back("Editor");
	
	struct { 
	    char* name;
	    bool (Configuration::*method)(void) const;
	    char act_type;  // (t)oggle or (r)adio
	} options[] = {
		{ "ToggleTimeMaster", &Configuration::get_jack_time_master, 't' },
		{ "StopPluginsWithTransport", &Configuration::get_plugins_stop_with_transport, 't' },
		{ "LatchedRecordEnable", &Configuration::get_latched_record_enable, 't' },
		{ "VerifyRemoveLastCapture", &Configuration::get_verify_remove_last_capture, 't' },
		{ "StopRecordingOnXrun", &Configuration::get_stop_recording_on_xrun, 't' },
		{ "StopTransportAtEndOfSession", &Configuration::get_stop_at_session_end, 't' },
		{ "UseHardwareMonitoring", &Configuration::get_use_hardware_monitoring, 'r' },
		{ "UseSoftwareMonitoring", &Configuration::get_use_sw_monitoring, 'r' },
		{ "UseExternalMonitoring", &Configuration::get_use_external_monitoring, 'r' },
		{ "MeterFalloffOff", &Configuration::get_meter_falloff_off, 'r' },
		{ "MeterFalloffSlowest", &Configuration::get_meter_falloff_slowest, 'r' },
		{ "MeterFalloffSlow", &Configuration::get_meter_falloff_slow, 'r' },
		{ "MeterFalloffMedium", &Configuration::get_meter_falloff_medium, 'r' },
		{ "MeterFalloffFast", &Configuration::get_meter_falloff_fast, 'r' },
		{ "MeterFalloffFaster", &Configuration::get_meter_falloff_faster, 'r' },
		{ "MeterFalloffFastest", &Configuration::get_meter_falloff_fastest, 'r' },
		{ "MeterHoldOff", &Configuration::get_meter_hold_off, 'r' },
		{ "MeterHoldShort", &Configuration::get_meter_hold_short, 'r' },
		{ "MeterHoldMedium", &Configuration::get_meter_hold_medium, 'r' },
		{ "MeterHoldLong", &Configuration::get_meter_hold_long, 'r' },
		{ 0, 0, 0 }
	};
	
	for (uint32_t n = 0; options[n].name; ++n) {
		for (std::vector<Glib::ustring>::iterator i = groups.begin(); i != groups.end(); i++) {
			Glib::RefPtr<Action> act = ActionManager::get_action (i->c_str(), options[n].name);
			if (act) {
				Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
				cerr << "action = " << (options[n].name) << " val = " << (Config->*(options[n].method))() << endl;//DEBUG
				if (options[n].act_type == 't' || (options[n].act_type == 'r' && (Config->*(options[n].method))()))
					tact->set_active ((Config->*(options[n].method))());
				continue;
			}
		}
	}
}

void
ARDOUR_UI::toggle_time_master ()
{
	toggle_config_state ("Transport", "ToggleTimeMaster", &Configuration::set_jack_time_master);
	if (session) {
		session->engine().reset_timebase ();
	}
}

void
ARDOUR_UI::toggle_config_state (const char* group, const char* action, void (Configuration::*set)(bool))
{
	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		(Config->*set) (tact->get_active());
	}
}

void
ARDOUR_UI::toggle_session_state (const char* group, const char* action, void (Session::*set)(bool), bool (Session::*get)(void) const)
{
	if (session) {
		Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
		if (act) {
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
			bool x = (session->*get)();

			if (x != tact->get_active()) {
				(session->*set) (!x);
			}
		}
	}
}

void
ARDOUR_UI::toggle_session_state (const char* group, const char* action, sigc::slot<void> theSlot)
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
ARDOUR_UI::toggle_send_mtc ()
{
	toggle_session_state ("options", "SendMTC", &Session::set_send_mtc, &Session::get_send_mtc);
}

void
ARDOUR_UI::toggle_send_mmc ()
{
	toggle_session_state ("options", "SendMMC", &Session::set_send_mmc, &Session::get_send_mmc);
}

void
ARDOUR_UI::toggle_use_mmc ()
{
	toggle_session_state ("options", "UseMMC", &Session::set_mmc_control, &Session::get_mmc_control);
}

void
ARDOUR_UI::toggle_use_midi_control ()
{
	toggle_session_state ("options", "UseMIDIcontrol", &Session::set_midi_control, &Session::get_midi_control);
}

void
ARDOUR_UI::toggle_send_midi_feedback ()
{
	toggle_session_state ("options", "SendMIDIfeedback", &Session::set_midi_feedback, &Session::get_midi_feedback);
}

void
ARDOUR_UI::toggle_AutoConnectNewTrackInputsToHardware()
{
	toggle_session_state ("options", "AutoConnectNewTrackInputsToHardware", &Session::set_input_auto_connect, &Session::get_input_auto_connect);
}
void
ARDOUR_UI::toggle_AutoConnectNewTrackOutputsToHardware()
{
	toggle_session_state ("options", "AutoConnectNewTrackOutputsToHardware", bind (mem_fun (session, &Session::set_output_auto_connect), Session::AutoConnectPhysical));
}
void
ARDOUR_UI::toggle_AutoConnectNewTrackOutputsToMaster()
{
	toggle_session_state ("options", "AutoConnectNewTrackOutputsToHardware", bind (mem_fun (session, &Session::set_output_auto_connect), Session::AutoConnectMaster));
}
void
ARDOUR_UI::toggle_ManuallyConnectNewTrackOutputs()
{
	toggle_session_state ("options", "AutoConnectNewTrackOutputsToHardware", bind (mem_fun (session, &Session::set_output_auto_connect), Session::AutoConnectOption (0)));
}

void
ARDOUR_UI::toggle_auto_input ()
{
	toggle_session_state ("Transport", "ToggleAutoInput", &Session::set_auto_input, &Session::get_auto_input);
}

void
ARDOUR_UI::toggle_auto_play ()
{
	toggle_session_state ("Transport", "ToggleAutoPlay", &Session::set_auto_play, &Session::get_auto_play);
}

void
ARDOUR_UI::toggle_auto_return ()
{
	toggle_session_state ("Transport", "ToggleAutoReturn", &Session::set_auto_return, &Session::get_auto_return);
}

void
ARDOUR_UI::toggle_click ()
{
	toggle_session_state ("Transport", "ToggleClick", &Session::set_clicking, &Session::get_clicking);
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (session) {
		if (session->get_auto_loop()) {
			if (session->transport_rolling()) {
				transport_roll();
			} else {
				session->request_auto_loop (false);
			}
		} else {
			session->request_auto_loop (true);
		}
	}
}

void
ARDOUR_UI::toggle_punch_in ()
{
	toggle_session_state ("Transport", "TogglePunchIn", &Session::set_punch_in, &Session::get_punch_in);
}

void
ARDOUR_UI::toggle_punch_out ()
{
	toggle_session_state ("Transport", "TogglePunchOut", &Session::set_punch_out, &Session::get_punch_out);
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
		cerr << "get_active() cond = " << tact->get_active() << endl;//DEBUG
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
	toggle_config_state ("options", "StopPluginsWithTransport", &Configuration::set_plugins_stop_with_transport);
}

void
ARDOUR_UI::toggle_LatchedRecordEnable()
{
	toggle_config_state ("options", "LatchedRecordEnable", &Configuration::set_latched_record_enable);
}

void
ARDOUR_UI::toggle_DoNotRunPluginsWhileRecording()
{
	toggle_session_state ("options", "DoNotRunPluginsWhileRecording", &Session::set_do_not_record_plugins, &Session::get_do_not_record_plugins);
}

void
ARDOUR_UI::toggle_VerifyRemoveLastCapture()
{
	toggle_config_state ("options", "VerifyRemoveLastCapture", &Configuration::set_verify_remove_last_capture);
}

void
ARDOUR_UI::toggle_StopRecordingOnXrun()
{
	toggle_config_state ("options", "StopRecordingOnXrun", &Configuration::set_stop_recording_on_xrun);
}

void
ARDOUR_UI::toggle_StopTransportAtEndOfSession()
{
	toggle_config_state ("options", "StopTransportAtEndOfSession", &Configuration::set_stop_at_session_end);
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
	toggle_session_state ("options", "LatchedSolo", &Session::set_solo_latched, &Session::solo_latched);
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
			session->set_solo_model (Session::SoloBus);
		} else {
			session->set_solo_model (Session::InverseMute);
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

	if (have_mtc) {
		const gchar *psync_strings[] = {
			N_("Internal"),
			N_("MTC"),
			N_("JACK"),
			0
		};
		
		positional_sync_strings = PBD::internationalize (psync_strings);
		
	} else {
		const gchar *psync_strings[] = {
			N_("Internal"),
			N_("JACK"),
			0
		};
		positional_sync_strings = PBD::internationalize (psync_strings);
	}
	
	set_popdown_strings (sync_option_combo, positional_sync_strings);
}

void
ARDOUR_UI::setup_session_options ()
{
	mtc_port_changed ();

	session_control_changed (Session::SlaveType);
	session_control_changed (Session::SendMTC);
	session_control_changed (Session::SendMMC);
	session_control_changed (Session::MMCControl);
	session_control_changed (Session::MidiFeedback);
	session_control_changed (Session::MidiControl);
	session_control_changed (Session::RecordingPlugins);
	session_control_changed (Session::CrossFadesActive);
	session_control_changed (Session::SoloLatch);
	session_control_changed (Session::SoloingModel);
	session_control_changed (Session::LayeringModel);
	session_control_changed (Session::CrossfadingModel);
	session_control_changed (Session::PunchOut);
	session_control_changed (Session::PunchIn);
	session_control_changed (Session::AutoPlay);
	session_control_changed (Session::AutoReturn);
	session_control_changed (Session::AutoInput);
	session_control_changed (Session::Clicking);
	
	session->ControlChanged.connect (mem_fun (*this, &ARDOUR_UI::queue_session_control_changed));
}

void
ARDOUR_UI::map_some_session_state (const char* group, const char* action, bool (Session::*get)() const)
{
	if (!session) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		bool x = (session->*get)();
		if (tact->get_active() != x) {
			tact->set_active (x);
		}
	}
}

void
ARDOUR_UI::queue_session_control_changed (Session::ControlType t)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &ARDOUR_UI::session_control_changed), t));
}

void
ARDOUR_UI::session_control_changed (Session::ControlType t)
{
	switch (t) {
	case Session::SlaveType:
		switch (session->slave_source()) {
		case Session::None:
			sync_option_combo.set_active_text (_("Internal"));
			break;
		case Session::MTC:
			sync_option_combo.set_active_text (_("MTC"));
			break;
		case Session::JACK:
			sync_option_combo.set_active_text (_("JACK"));
			break;
		}
		
		break;

	case Session::SendMTC:
		map_some_session_state ("options", "SendMTC", &Session::get_send_mtc);
		break;

	case Session::SendMMC:
		map_some_session_state ("options", "SendMMC", &Session::get_send_mmc);
		break;

	case Session::MMCControl:       
		map_some_session_state ("options", "UseMMC", &Session::get_mmc_control);
		break;

	case Session::MidiFeedback:       
		map_some_session_state ("options", "SendMIDIfeedback", &Session::get_midi_feedback);
		break;

	case Session::MidiControl:       
		map_some_session_state ("options", "UseMIDIcontrol", &Session::get_midi_control);
		break;

	case Session::RecordingPlugins:
		map_some_session_state ("options", "DoNotRunPluginsWhileRecording", &Session::get_do_not_record_plugins);
		break;

	case Session::CrossFadesActive:
		map_some_session_state ("options", "CrossfadesActive", &Session::get_crossfades_active);
		break;

	case Session::SoloLatch:
		break;

	case Session::SoloingModel:
		switch (session->solo_model()) {
		case Session::InverseMute:
			break;
		case Session::SoloBus:
			break;
		}
		break;

	case Session::LayeringModel:
		break;

	case Session::CrossfadingModel:
		break;

		
	case Session::AutoPlay:
		map_some_session_state ("Transport", "ToggleAutoPlay", &Session::get_auto_play);
		break;

	case Session::AutoLoop:
		break;

	case Session::AutoReturn:
		map_some_session_state ("Transport", "ToggleAutoReturn", &Session::get_auto_return);
		break;

	case Session::AutoInput:
		map_some_session_state ("Transport", "ToggleAutoInput", &Session::get_auto_input);
		break;

	case Session::PunchOut:
		map_some_session_state ("Transport", "TogglePunchOut", &Session::get_punch_out);
		break;

	case Session::PunchIn:
		map_some_session_state ("Transport", "TogglePunchIn", &Session::get_punch_in);
		break;

	case Session::Clicking:
		map_some_session_state ("Transport", "ToggleClick", &Session::get_clicking);
		break;

	default:
		// somebody else handles this 
		break;

	}
}
