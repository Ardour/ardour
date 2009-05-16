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

*/

#include "pbd/convert.h"
#include "pbd/stacktrace.h"

#include <gtkmm2ext/utils.h>

#include "ardour/configuration.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"

#ifdef HAVE_LIBLO
#include "ardour/osc.h"
#endif

#include "ardour_ui.h"
#include "actions.h"
#include "gui_thread.h"
#include "public_editor.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

void
ARDOUR_UI::toggle_time_master ()
{
	ActionManager::toggle_config_state ("Transport", "ToggleTimeMaster", &RCConfiguration::set_jack_time_master, &RCConfiguration::get_jack_time_master);
}

void
ARDOUR_UI::toggle_send_mtc ()
{
	ActionManager::toggle_config_state ("options", "SendMTC", &RCConfiguration::set_send_mtc, &RCConfiguration::get_send_mtc);
}

void
ARDOUR_UI::toggle_send_mmc ()
{
	ActionManager::toggle_config_state ("options", "SendMMC", &RCConfiguration::set_send_mmc, &RCConfiguration::get_send_mmc);
}

void
ARDOUR_UI::toggle_send_midi_clock ()
{
	ActionManager::toggle_config_state ("options", "SendMidiClock", &RCConfiguration::set_send_midi_clock, &RCConfiguration::get_send_midi_clock);
}

void
ARDOUR_UI::toggle_use_mmc ()
{
	ActionManager::toggle_config_state ("options", "UseMMC", &RCConfiguration::set_mmc_control, &RCConfiguration::get_mmc_control);
}

void
ARDOUR_UI::toggle_use_osc ()
{
	ActionManager::toggle_config_state ("options", "UseOSC", &RCConfiguration::set_use_osc, &RCConfiguration::get_use_osc);
}

void
ARDOUR_UI::toggle_send_midi_feedback ()
{
	ActionManager::toggle_config_state ("options", "SendMIDIfeedback", &RCConfiguration::set_midi_feedback, &RCConfiguration::get_midi_feedback);
}

void
ARDOUR_UI::toggle_denormal_protection ()
{
	ActionManager::toggle_config_state ("options", "DenormalProtection", &RCConfiguration::set_denormal_protection, &RCConfiguration::get_denormal_protection);
}

void
ARDOUR_UI::toggle_only_copy_imported_files ()
{
	ActionManager::toggle_config_state ("options", "OnlyCopyImportedFiles", &RCConfiguration::set_only_copy_imported_files, &RCConfiguration::get_only_copy_imported_files);
}


void
ARDOUR_UI::set_native_file_header_format (HeaderFormat hf)
{
	const char *action = 0;

	switch (hf) {
	case BWF:
		action = X_("FileHeaderFormatBWF");
		break;
	case WAVE:
		action = X_("FileHeaderFormatWAVE");
		break;
	case WAVE64:
		action = X_("FileHeaderFormatWAVE64");
		break;
	case iXML:
		action = X_("FileHeaderFormatiXML");
		break;
	case RF64:
		action = X_("FileHeaderFormatRF64");
		break;
	case CAF:
		action = X_("FileHeaderFormatCAF");
		break;
	case AIFF:
		action = X_("FileHeaderFormatAIFF");
		break;
	default:
		fatal << string_compose (_("programming error: %1"), "illegal file header format in ::set_native_file_header_format") << endmsg;
		/*NOTREACHED*/
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		if (ract && ract->get_active() && session->config.get_native_file_header_format() != hf) {
			session->config.set_native_file_header_format (hf);
		}
	}
}

void
ARDOUR_UI::set_native_file_data_format (SampleFormat sf)
{
	const char* action = 0;

	switch (sf) {
	case FormatFloat:
		action = X_("FileDataFormatFloat");
		break;
	case FormatInt24:
		action = X_("FileDataFormat24bit");
		break;
	case FormatInt16:
		action = X_("FileDataFormat16bit");
		break;
	default:
		fatal << string_compose (_("programming error: %1"), "illegal file data format in ::set_native_file_data_format") << endmsg;
		/*NOTREACHED*/
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		if (ract && ract->get_active() && session->config.get_native_file_data_format() != sf) {
			session->config.set_native_file_data_format (sf);
		}
	}
}

void
ARDOUR_UI::set_input_auto_connect (AutoConnectOption option)
{
	const char* action;

	switch (option) {
	case AutoConnectPhysical:
		action = X_("InputAutoConnectPhysical");
		break;
	default:
		action = X_("InputAutoConnectManual");
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);

		if (ract && ract->get_active() && Config->get_input_auto_connect() != option) {
			Config->set_input_auto_connect (option);
		}
	}
}

void
ARDOUR_UI::set_output_auto_connect (AutoConnectOption option)
{
	const char* action;

	switch (option) {
	case AutoConnectPhysical:
		action = X_("OutputAutoConnectPhysical");
		break;
	case AutoConnectMaster:
		action = X_("OutputAutoConnectMaster");
		break;
	default:
		action = X_("OutputAutoConnectManual");
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);

		if (ract && ract->get_active() && Config->get_output_auto_connect() != option) {
			Config->set_output_auto_connect (option);
		}
	}
}

void
ARDOUR_UI::set_solo_model (SoloModel model)
{
	const char* action = 0;

	switch (model) {
	case SoloBus:
		action = X_("SoloViaBus");
		break;

	case InverseMute:
		action = X_("SoloInPlace");
		break;
	default:
		fatal << string_compose (_("programming error: unknown solo model in ARDOUR_UI::set_solo_model: %1"), model) << endmsg;
		/*NOTREACHED*/
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);

		if (ract && ract->get_active() && Config->get_solo_model() != model) {
			Config->set_solo_model (model);
		}
	}

}

void
ARDOUR_UI::set_remote_model (RemoteModel model)
{
	const char* action = 0;

	switch (model) {
	case UserOrdered:
		action = X_("RemoteUserDefined");
		break;
	case MixerOrdered:
		action = X_("RemoteMixerDefined");
		break;
	case EditorOrdered:
		action = X_("RemoteEditorDefined");
		break;

	default:
		fatal << string_compose (_("programming error: unknown remote model in ARDOUR_UI::set_remote_model: %1"), model) << endmsg;
		/*NOTREACHED*/
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);

		if (ract && ract->get_active() && Config->get_remote_model() != model) {
			Config->set_remote_model (model);
		}
	}

}

void
ARDOUR_UI::set_monitor_model (MonitorModel model)
{
	const char* action = 0;

	switch (model) {
	case HardwareMonitoring:
		action = X_("UseHardwareMonitoring");
		break;

	case SoftwareMonitoring:
		action = X_("UseSoftwareMonitoring");
		break;
	case ExternalMonitoring:
		action = X_("UseExternalMonitoring");
		break;

	default:
		fatal << string_compose (_("programming error: unknown monitor model in ARDOUR_UI::set_monitor_model: %1"), model) << endmsg;
		/*NOTREACHED*/
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);

		if (ract && ract->get_active() && Config->get_monitoring_model() != model) {
			Config->set_monitoring_model (model);
		}
	}

}

void
ARDOUR_UI::set_denormal_model (DenormalModel model)
{
	const char* action = 0;

	switch (model) {
	case DenormalNone:
		action = X_("DenormalNone");
		break;

	case DenormalFTZ:
		action = X_("DenormalFTZ");
		break;

	case DenormalDAZ:
		action = X_("DenormalDAZ");
		break;

	case DenormalFTZDAZ:
		action = X_("DenormalFTZDAZ");
		break;

	default:
		fatal << string_compose (_("programming error: unknown denormal model in ARDOUR_UI::set_denormal_model: %1"), model) << endmsg;
		/*NOTREACHED*/
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);

		if (ract && ract->get_active() && Config->get_denormal_model() != model) {
			Config->set_denormal_model (model);
		}
	}

}

void
ARDOUR_UI::toggle_auto_input ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoInput", mem_fun (session->config, &SessionConfiguration::set_auto_input), mem_fun (session->config, &SessionConfiguration::get_auto_input));
}

void
ARDOUR_UI::toggle_auto_play ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoPlay", mem_fun (session->config, &SessionConfiguration::set_auto_play), mem_fun (session->config, &SessionConfiguration::get_auto_play));
}

void
ARDOUR_UI::toggle_auto_return ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoReturn", mem_fun (session->config, &SessionConfiguration::set_auto_return), mem_fun (session->config, &SessionConfiguration::get_auto_return));
}

void
ARDOUR_UI::toggle_click ()
{
	ActionManager::toggle_config_state ("Transport", "ToggleClick", &RCConfiguration::set_clicking, &RCConfiguration::get_clicking);
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (session) {
		if (session->get_play_loop()) {
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
ARDOUR_UI::unset_dual_punch ()
{
	Glib::RefPtr<Action> action = ActionManager::get_action ("Transport", "TogglePunch");

	if (action) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(action);
		if (tact) {
			ignore_dual_punch = true;
			tact->set_active (false);
			ignore_dual_punch = false;
		}
	}
}

void
ARDOUR_UI::toggle_punch ()
{
	if (ignore_dual_punch) {
		return;
	}

	Glib::RefPtr<Action> action = ActionManager::get_action ("Transport", "TogglePunch");

	if (action) {

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(action);

		if (!tact) {
			return;
		}

		/* drive the other two actions from this one */

		Glib::RefPtr<Action> in_action = ActionManager::get_action ("Transport", "TogglePunchIn");
		Glib::RefPtr<Action> out_action = ActionManager::get_action ("Transport", "TogglePunchOut");

		if (in_action && out_action) {
			Glib::RefPtr<ToggleAction> tiact = Glib::RefPtr<ToggleAction>::cast_dynamic(in_action);
			Glib::RefPtr<ToggleAction> toact = Glib::RefPtr<ToggleAction>::cast_dynamic(out_action);
			tiact->set_active (tact->get_active());
			toact->set_active (tact->get_active());
		}
	}
}

void
ARDOUR_UI::toggle_punch_in ()
{
	ActionManager::toggle_config_state_foo ("Transport", "TogglePunchIn", mem_fun (session->config, &SessionConfiguration::set_punch_in), mem_fun (session->config, &SessionConfiguration::get_punch_in));
}

void
ARDOUR_UI::toggle_punch_out ()
{
	ActionManager::toggle_config_state_foo ("Transport", "TogglePunchOut", mem_fun (session->config, &SessionConfiguration::set_punch_out), mem_fun (session->config, &SessionConfiguration::get_punch_out));
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
ARDOUR_UI::toggle_new_plugins_active ()
{
	ActionManager::toggle_config_state ("options", "NewPluginsActive", &RCConfiguration::set_new_plugins_active, &RCConfiguration::get_new_plugins_active);
}

void
ARDOUR_UI::toggle_StopPluginsWithTransport()
{
	ActionManager::toggle_config_state ("options", "StopPluginsWithTransport", &RCConfiguration::set_plugins_stop_with_transport, &RCConfiguration::get_plugins_stop_with_transport);
}

void
ARDOUR_UI::toggle_LatchedRecordEnable()
{
	ActionManager::toggle_config_state ("options", "LatchedRecordEnable", &RCConfiguration::set_latched_record_enable, &RCConfiguration::get_latched_record_enable);
}

void
ARDOUR_UI::toggle_RegionEquivalentsOverlap()
{
	ActionManager::toggle_config_state ("options", "RegionEquivalentsOverlap", &RCConfiguration::set_use_overlap_equivalency, &RCConfiguration::get_use_overlap_equivalency);
}

void
ARDOUR_UI::toggle_DoNotRunPluginsWhileRecording()
{
	ActionManager::toggle_config_state ("options", "DoNotRunPluginsWhileRecording", &RCConfiguration::set_do_not_record_plugins, &RCConfiguration::get_do_not_record_plugins);
}

void
ARDOUR_UI::toggle_VerifyRemoveLastCapture()
{
	ActionManager::toggle_config_state ("options", "VerifyRemoveLastCapture", &RCConfiguration::set_verify_remove_last_capture, &RCConfiguration::get_verify_remove_last_capture);
}

void
ARDOUR_UI::toggle_PeriodicSafetyBackups()
{
	ActionManager::toggle_config_state ("options", "PeriodicSafetyBackups", &RCConfiguration::set_periodic_safety_backups, &RCConfiguration::get_periodic_safety_backups);
}

void
ARDOUR_UI::toggle_StopRecordingOnXrun()
{
	ActionManager::toggle_config_state ("options", "StopRecordingOnXrun", &RCConfiguration::set_stop_recording_on_xrun, &RCConfiguration::get_stop_recording_on_xrun);
}

void
ARDOUR_UI::toggle_CreateXrunMarker()
{
	ActionManager::toggle_config_state ("options", "CreateXrunMarker", &RCConfiguration::set_create_xrun_marker, &RCConfiguration::get_create_xrun_marker);
}

void
ARDOUR_UI::toggle_sync_order_keys ()
{
	ActionManager::toggle_config_state ("options", "SyncEditorAndMixerTrackOrder", &RCConfiguration::set_sync_all_route_ordering, &RCConfiguration::get_sync_all_route_ordering);
}

void
ARDOUR_UI::toggle_StopTransportAtEndOfSession()
{
	ActionManager::toggle_config_state ("options", "StopTransportAtEndOfSession", &RCConfiguration::set_stop_at_session_end, &RCConfiguration::get_stop_at_session_end);
}

void
ARDOUR_UI::toggle_GainReduceFastTransport()
{
	ActionManager::toggle_config_state ("options", "GainReduceFastTransport", &RCConfiguration::set_quieten_at_speed, &RCConfiguration::get_quieten_at_speed);
}

void
ARDOUR_UI::toggle_LatchedSolo()
{
	ActionManager::toggle_config_state ("options", "LatchedSolo", &RCConfiguration::set_solo_latched, &RCConfiguration::get_solo_latched);
}

void
ARDOUR_UI::toggle_ShowSoloMutes()
{
	ActionManager::toggle_config_state ("options", "ShowSoloMutes", &RCConfiguration::set_show_solo_mutes, &RCConfiguration::get_show_solo_mutes);
}

void
ARDOUR_UI::toggle_SoloMuteOverride()
{
	ActionManager::toggle_config_state ("options", "SoloMuteOverride", &RCConfiguration::set_solo_mute_override, &RCConfiguration::get_solo_mute_override);
}

void
ARDOUR_UI::toggle_PrimaryClockDeltaEditCursor()
{
	ActionManager::toggle_config_state ("options", "PrimaryClockDeltaEditCursor", &RCConfiguration::set_primary_clock_delta_edit_cursor, &RCConfiguration::get_primary_clock_delta_edit_cursor);
}

void
ARDOUR_UI::toggle_SecondaryClockDeltaEditCursor()
{
	ActionManager::toggle_config_state ("options", "SecondaryClockDeltaEditCursor", &RCConfiguration::set_secondary_clock_delta_edit_cursor, &RCConfiguration::get_secondary_clock_delta_edit_cursor);
}

void
ARDOUR_UI::toggle_ShowTrackMeters()
{
	ActionManager::toggle_config_state ("options", "ShowTrackMeters", &RCConfiguration::set_show_track_meters, &RCConfiguration::get_show_track_meters);
}

void
ARDOUR_UI::toggle_TapeMachineMode ()
{
	ActionManager::toggle_config_state ("options", "ToggleTapeMachineMode", &RCConfiguration::set_tape_machine_mode, &RCConfiguration::get_tape_machine_mode);
}

void
ARDOUR_UI::toggle_use_narrow_ms()
{
	ActionManager::toggle_config_state ("options", "DefaultNarrowMS", &RCConfiguration::set_default_narrow_ms, &RCConfiguration::get_default_narrow_ms);
}

void
ARDOUR_UI::toggle_NameNewMarkers()
{
	ActionManager::toggle_config_state ("options", "NameNewMarkers", &RCConfiguration::set_name_new_markers, &RCConfiguration::get_name_new_markers);
}

void
ARDOUR_UI::toggle_rubberbanding_snaps_to_grid ()
{
	ActionManager::toggle_config_state ("options", "RubberbandingSnapsToGrid", &RCConfiguration::set_rubberbanding_snaps_to_grid, &RCConfiguration::get_rubberbanding_snaps_to_grid);
}

void
ARDOUR_UI::toggle_auto_analyse_audio ()
{
	ActionManager::toggle_config_state ("options", "AutoAnalyseAudio", &RCConfiguration::set_auto_analyse_audio, &RCConfiguration::get_auto_analyse_audio);
}

void
ARDOUR_UI::mtc_port_changed ()
{
	bool have_mtc;
	bool have_midi_clock;

	if (session) {
		if (session->mtc_port()) {
			have_mtc = true;
		} else {
			have_mtc = false;
		}
		if (session->midi_clock_port()) {
			have_midi_clock = true;
		} else {
			have_midi_clock = false;
		}
	} else {
		have_mtc = false;
		have_midi_clock = false;
	}

	positional_sync_strings.clear ();
	positional_sync_strings.push_back (slave_source_to_string (None));
	if (have_mtc) {
		positional_sync_strings.push_back (slave_source_to_string (MTC));
	}
	if (have_midi_clock) {
		positional_sync_strings.push_back (slave_source_to_string (MIDIClock));
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
ARDOUR_UI::map_solo_model ()
{
	const char* on;

	if (Config->get_solo_model() == InverseMute) {
		on = X_("SoloInPlace");
	} else {
		on = X_("SoloViaBus");
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", on);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_monitor_model ()
{
	const char* on = 0;

	switch (Config->get_monitoring_model()) {
	case HardwareMonitoring:
		on = X_("UseHardwareMonitoring");
		break;
	case SoftwareMonitoring:
		on = X_("UseSoftwareMonitoring");
		break;
	case ExternalMonitoring:
		on = X_("UseExternalMonitoring");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", on);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_denormal_protection ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("options", X_("DenormalProtection"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (Config->get_denormal_protection());
		}
	}
}

void
ARDOUR_UI::map_denormal_model ()
{
	const char* on = 0;

	switch (Config->get_denormal_model()) {
	case DenormalNone:
		on = X_("DenormalNone");
		break;
	case DenormalFTZ:
		on = X_("DenormalFTZ");
		break;
	case DenormalDAZ:
		on = X_("DenormalDAZ");
		break;
	case DenormalFTZDAZ:
		on = X_("DenormalFTZDAZ");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", on);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_remote_model ()
{
	const char* on = 0;

	switch (Config->get_remote_model()) {
	case UserOrdered:
		on = X_("RemoteUserDefined");
		break;
	case MixerOrdered:
		on = X_("RemoteMixerDefined");
		break;
	case EditorOrdered:
		on = X_("RemoteEditorDefined");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", on);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_file_header_format ()
{
	const char* action = 0;

	switch (session->config.get_native_file_header_format()) {
	case BWF:
		action = X_("FileHeaderFormatBWF");
		break;

	case WAVE:
		action = X_("FileHeaderFormatWAVE");
		break;

	case WAVE64:
		action = X_("FileHeaderFormatWAVE64");
		break;

	case iXML:
		action = X_("FileHeaderFormatiXML");
		break;

	case RF64:
		action = X_("FileHeaderFormatRF64");
		break;

	case CAF:
		action = X_("FileHeaderFormatCAF");
		break;

	default:
		fatal << string_compose (_("programming error: unknown file header format passed to ARDOUR_UI::map_file_data_format: %1"),
					 session->config.get_native_file_header_format()) << endmsg;
		/*NOTREACHED*/
	}


	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_file_data_format ()
{
	const char* action = 0;

	switch (session->config.get_native_file_data_format()) {
	case FormatFloat:
		action = X_("FileDataFormatFloat");
		break;

	case FormatInt24:
		action = X_("FileDataFormat24bit");
		break;

	case FormatInt16:
		action = X_("FileDataFormat16bit");
		break;

	default:
		fatal << string_compose (_("programming error: unknown file data format passed to ARDOUR_UI::map_file_data_format: %1"),
					 session->config.get_native_file_data_format()) << endmsg;
		/*NOTREACHED*/
	}


	Glib::RefPtr<Action> act = ActionManager::get_action ("options", action);

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_input_auto_connect ()
{
	const char* on;

	if (Config->get_input_auto_connect() == (AutoConnectOption) 0) {
		on = "InputAutoConnectManual";
	} else {
		on = "InputAutoConnectPhysical";
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", on);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_output_auto_connect ()
{
	const char* on;

	if (Config->get_output_auto_connect() == (AutoConnectOption) 0) {
		on = "OutputAutoConnectManual";
	} else if (Config->get_output_auto_connect() == AutoConnectPhysical) {
		on = "OutputAutoConnectPhysical";
	} else {
		on = "OutputAutoConnectMaster";
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("options", on);
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_only_copy_imported_files ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("options", X_("OnlyCopyImportedFiles"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

		if (tact && !tact->get_active()) {
			tact->set_active (Config->get_only_copy_imported_files());
		}
	}

}


void
ARDOUR_UI::map_meter_falloff ()
{
	const char* action = X_("MeterFalloffMedium");

	float val = Config->get_meter_falloff ();
	MeterFalloff code = meter_falloff_from_float(val);

	switch (code) {
	case MeterFalloffOff:
		action = X_("MeterFalloffOff");
		break;
	case MeterFalloffSlowest:
		action = X_("MeterFalloffSlowest");
		break;
	case MeterFalloffSlow:
		action = X_("MeterFalloffSlow");
		break;
	case MeterFalloffMedium:
		action = X_("MeterFalloffMedium");
		break;
	case MeterFalloffFast:
		action = X_("MeterFalloffFast");
		break;
	case MeterFalloffFaster:
		action = X_("MeterFalloffFaster");
		break;
	case MeterFalloffFastest:
		action = X_("MeterFalloffFastest");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		if (ract && !ract->get_active()) {
			ract->set_active (true);
		}
	}
}

void
ARDOUR_UI::map_meter_hold ()
{
	const char* action = X_("MeterHoldMedium");

	/* XXX hack alert. Fix this. Please */

	float val = Config->get_meter_hold ();
	MeterHold code = (MeterHold) (int) (floor (val));

	switch (code) {
	case MeterHoldOff:
		action = X_("MeterHoldOff");
		break;
	case MeterHoldShort:
		action = X_("MeterHoldShort");
		break;
	case MeterHoldMedium:
		action = X_("MeterHoldMedium");
		break;
	case MeterHoldLong:
		action = X_("MeterHoldLong");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		if (ract && !ract->get_active()) {
			ract->set_active (true);
		}
	}
}

void
ARDOUR_UI::set_meter_hold (MeterHold val)
{
	const char* action = 0;
	float fval;

	fval = meter_hold_to_float (val);

	switch (val) {
	case MeterHoldOff:
		action = X_("MeterHoldOff");
		break;
	case MeterHoldShort:
		action = X_("MeterHoldShort");
		break;
	case MeterHoldMedium:
		action = X_("MeterHoldMedium");
		break;
	case MeterHoldLong:
		action = X_("MeterHoldLong");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		if (ract && ract->get_active() && Config->get_meter_hold() != fval) {
			Config->set_meter_hold (fval);
		}
	}
}

void
ARDOUR_UI::set_meter_falloff (MeterFalloff val)
{
	const char* action = 0;
	float fval;

	fval = meter_falloff_to_float (val);

	switch (val) {
	case MeterFalloffOff:
		action = X_("MeterFalloffOff");
		break;
	case MeterFalloffSlowest:
		action = X_("MeterFalloffSlowest");
		break;
	case MeterFalloffSlow:
		action = X_("MeterFalloffSlow");
		break;
	case MeterFalloffMedium:
		action = X_("MeterFalloffMedium");
		break;
	case MeterFalloffFast:
		action = X_("MeterFalloffFast");
		break;
	case MeterFalloffFaster:
		action = X_("MeterFalloffFaster");
		break;
	case MeterFalloffFastest:
		action = X_("MeterFalloffFastest");
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), action);

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		if (ract && ract->get_active() && Config->get_meter_falloff () != fval) {
			Config->set_meter_falloff (fval);
		}
	}
}

void
ARDOUR_UI::parameter_changed (std::string p)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &ARDOUR_UI::parameter_changed), p));

	if (p == "slave-source") {

		sync_option_combo.set_active_text (slave_source_to_string (Config->get_slave_source()));

		switch (Config->get_slave_source()) {
		case None:
			ActionManager::get_action ("Transport", "ToggleAutoPlay")->set_sensitive (true);
			ActionManager::get_action ("Transport", "ToggleAutoReturn")->set_sensitive (true);
			break;

		default:
			/* XXX need to make auto-play is off as well as insensitive */
			ActionManager::get_action ("Transport", "ToggleAutoPlay")->set_sensitive (false);
			ActionManager::get_action ("Transport", "ToggleAutoReturn")->set_sensitive (false);
			break;
		}

	} else if (p == "send-mtc") {

		ActionManager::map_some_state ("options", "SendMTC", &RCConfiguration::get_send_mtc);

	} else if (p == "send-mmc") {

		ActionManager::map_some_state ("options", "SendMMC", &RCConfiguration::get_send_mmc);

	} else if (p == "use-osc") {

#ifdef HAVE_LIBLO
		if (Config->get_use_osc()) {
			osc->start ();
		} else {
			osc->stop ();
		}
#endif

		ActionManager::map_some_state ("options", "UseOSC", &RCConfiguration::get_use_osc);

	} else if (p == "mmc-control") {
		ActionManager::map_some_state ("options", "UseMMC", &RCConfiguration::get_mmc_control);
	} else if (p == "midi-feedback") {
		ActionManager::map_some_state ("options", "SendMIDIfeedback", &RCConfiguration::get_midi_feedback);
	} else if (p == "do-not-record-plugins") {
		ActionManager::map_some_state ("options", "DoNotRunPluginsWhileRecording", &RCConfiguration::get_do_not_record_plugins);
	} else if (p == "latched-record-enable") {
		ActionManager::map_some_state ("options", "LatchedRecordEnable", &RCConfiguration::get_latched_record_enable);
	} else if (p == "solo-latched") {
		ActionManager::map_some_state ("options", "LatchedSolo", &RCConfiguration::get_solo_latched);
	} else if (p == "show-solo-mutes") {
		ActionManager::map_some_state ("options", "ShowSoloMutes", &RCConfiguration::get_show_solo_mutes);
	} else if (p == "solo-mute-override") {
		ActionManager::map_some_state ("options", "SoloMuteOverride", &RCConfiguration::get_solo_mute_override);
	} else if (p == "solo-model") {
		map_solo_model ();
	} else if (p == "auto-play") {
		ActionManager::map_some_state ("Transport", "ToggleAutoPlay", mem_fun (session->config, &SessionConfiguration::get_auto_play));
	} else if (p == "auto-return") {
		ActionManager::map_some_state ("Transport", "ToggleAutoReturn", mem_fun (session->config, &SessionConfiguration::get_auto_return));
	} else if (p == "auto-input") {
		ActionManager::map_some_state ("Transport", "ToggleAutoInput", mem_fun (session->config, &SessionConfiguration::get_auto_input));
	} else if (p == "tape-machine-mode") {
		ActionManager::map_some_state ("options", "ToggleTapeMachineMode", &RCConfiguration::get_tape_machine_mode);
	} else if (p == "punch-out") {
		ActionManager::map_some_state ("Transport", "TogglePunchOut", mem_fun (session->config, &SessionConfiguration::get_punch_out));
		if (!session->config.get_punch_out()) {
			unset_dual_punch ();
		}
	} else if (p == "punch-in") {
		ActionManager::map_some_state ("Transport", "TogglePunchIn", mem_fun (session->config, &SessionConfiguration::get_punch_in));
		if (!session->config.get_punch_in()) {
			unset_dual_punch ();
		}
	} else if (p == "clicking") {
		ActionManager::map_some_state ("Transport", "ToggleClick", &RCConfiguration::get_clicking);
	} else if (p == "jack-time-master") {
		ActionManager::map_some_state ("Transport",  "ToggleTimeMaster", &RCConfiguration::get_jack_time_master);
	} else if (p == "plugins-stop-with-transport") {
		ActionManager::map_some_state ("options",  "StopPluginsWithTransport", &RCConfiguration::get_plugins_stop_with_transport);
	} else if (p == "new-plugins-active") {
		ActionManager::map_some_state ("options",  "NewPluginsActive", &RCConfiguration::get_new_plugins_active);
	} else if (p == "latched-record-enable") {
		ActionManager::map_some_state ("options", "LatchedRecordEnable", &RCConfiguration::get_latched_record_enable);
	} else if (p == "verify-remove-last-capture") {
		ActionManager::map_some_state ("options",  "VerifyRemoveLastCapture", &RCConfiguration::get_verify_remove_last_capture);
	} else if (p == "periodic-safety-backups") {
		ActionManager::map_some_state ("options",  "PeriodicSafetyBackups", &RCConfiguration::get_periodic_safety_backups);
	} else if (p == "stop-recording-on-xrun") {
		ActionManager::map_some_state ("options",  "StopRecordingOnXrun", &RCConfiguration::get_stop_recording_on_xrun);
	} else if (p == "create-xrun-marker") {
		ActionManager::map_some_state ("options",  "CreateXrunMarker", &RCConfiguration::get_create_xrun_marker);
	} else if (p == "sync-all-route-ordering") {
		ActionManager::map_some_state ("options",  "SyncEditorAndMixerTrackOrder", &RCConfiguration::get_sync_all_route_ordering);
	} else if (p == "stop-at-session-end") {
		ActionManager::map_some_state ("options",  "StopTransportAtEndOfSession", &RCConfiguration::get_stop_at_session_end);
	} else if (p == "monitoring-model") {
		map_monitor_model ();
	} else if (p == "denormal-model") {
		map_denormal_model ();
	} else if (p == "denormal-protection") {
		map_denormal_protection ();
	} else if (p == "remote-model") {
		map_remote_model ();
	} else if (p == "use-video-sync") {
		ActionManager::map_some_state ("Transport",  "ToggleVideoSync", &RCConfiguration::get_use_video_sync);
	} else if (p == "quieten-at-speed") {
		ActionManager::map_some_state ("options",  "GainReduceFastTransport", &RCConfiguration::get_quieten_at_speed);
	} else if (p == "shuttle-behaviour") {

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

	} else if (p == "shuttle-units") {

		switch (Config->get_shuttle_units()) {
		case Percentage:
			shuttle_units_button.set_label("% ");
			break;
		case Semitones:
			shuttle_units_button.set_label(_("ST"));
			break;
		}
	} else if (p == "input-auto-connect") {
		map_input_auto_connect ();
	} else if (p == "output-auto-connect") {
		map_output_auto_connect ();
	} else if (p == "native-file-header-format") {
		map_file_header_format ();
	} else if (p == "native-file-data-format") {
		map_file_data_format ();
	} else if (p == "meter-hold") {
		map_meter_hold ();
	} else if (p == "meter-falloff") {
		map_meter_falloff ();
	} else if (p == "video-pullup" || p == "smpte-format") {
		if (session) {
			primary_clock.set (session->audible_frame(), true);
			secondary_clock.set (session->audible_frame(), true);
		} else {
			primary_clock.set (0, true);
			secondary_clock.set (0, true);
		}
	} else if (p == "use-overlap-equivalency") {
		ActionManager::map_some_state ("options", "RegionEquivalentsOverlap", &RCConfiguration::get_use_overlap_equivalency);
	} else if (p == "primary-clock-delta-edit-cursor") {
		ActionManager::map_some_state ("options",  "PrimaryClockDeltaEditCursor", &RCConfiguration::get_primary_clock_delta_edit_cursor);
	} else if (p == "secondary-clock-delta-edit-cursor") {
		ActionManager::map_some_state ("options",  "SecondaryClockDeltaEditCursor", &RCConfiguration::get_secondary_clock_delta_edit_cursor);
	} else if (p == "only-copy-imported-files") {
		map_only_copy_imported_files ();
	} else if (p == "show-track-meters") {
		ActionManager::map_some_state ("options",  "ShowTrackMeters", &RCConfiguration::get_show_track_meters);
		editor->toggle_meter_updating();
	} else if (p == "default-narrow_ms") {
		ActionManager::map_some_state ("options",  "DefaultNarrowMS", &RCConfiguration::get_default_narrow_ms);
	} else if (p =="rubberbanding-snaps-to-grid") {
		ActionManager::map_some_state ("options", "RubberbandingSnapsToGrid", &RCConfiguration::get_rubberbanding_snaps_to_grid);
	}
}
