/*
    Copyright (C) 20002-2004 Paul Davis 

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

/* This file contains any ARDOUR_UI methods that require knowledge of
   the editor, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the PublicEditor class. This
   is to cut down on the nasty compile times for both these classes.
*/

#include <pbd/pathscanner.h>

#include <gtkmm2ext/utils.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "editor.h"
#include "actions.h"

#include <ardour/session.h>
#include <ardour/control_protocol_manager.h>

#include <control_protocol/control_protocol.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;
using namespace sigc;

int
ARDOUR_UI::create_editor ()

{
	try {
		editor = new Editor (*engine);
	}

	catch (failed_constructor& err) {
		return -1;
	}

	editor->Realized.connect (mem_fun (*this, &ARDOUR_UI::editor_realized));

	return 0;
}

void
ARDOUR_UI::install_actions ()
{
	Glib::RefPtr<ActionGroup> main_actions = ActionGroup::create (X_("Main"));
	Glib::RefPtr<Action> act;

	/* menus + submenus that need action items */

	ActionManager::register_action (main_actions, X_("Session"), _("Session"));
	ActionManager::register_action (main_actions, X_("Export"), _("Export"));
	ActionManager::register_action (main_actions, X_("Cleanup"), _("Cleanup"));
	ActionManager::register_action (main_actions, X_("Sync"), _("Sync"));
	ActionManager::register_action (main_actions, X_("Options"), _("Options"));
	ActionManager::register_action (main_actions, X_("TransportOptions"), _("Options"));
	ActionManager::register_action (main_actions, X_("Help"), _("Help"));
 	ActionManager::register_action (main_actions, X_("KeyMouse Actions"), _("KeyMouse Actions"));
	ActionManager::register_action (main_actions, X_("AudioFileFormat"), _("Audio File Format"));
	ActionManager::register_action (main_actions, X_("AudioFileFormatHeader"), _("Header"));
	ActionManager::register_action (main_actions, X_("AudioFileFormatData"), _("Data"));
	ActionManager::register_action (main_actions, X_("ControlSurfaces"), _("Control Surfaces"));
	ActionManager::register_action (main_actions, X_("Metering"), _("Metering"));
	ActionManager::register_action (main_actions, X_("MeteringFallOffRate"), _("Fall off rate"));
	ActionManager::register_action (main_actions, X_("MeteringHoldTime"), _("Hold Time"));

	/* the real actions */

	act = ActionManager::register_action (main_actions, X_("New"), _("New"),  bind (mem_fun(*this, &ARDOUR_UI::new_session), false, string ()));

	ActionManager::register_action (main_actions, X_("Open"), _("Open"),  mem_fun(*this, &ARDOUR_UI::open_session));
	ActionManager::register_action (main_actions, X_("Recent"), _("Recent"),  mem_fun(*this, &ARDOUR_UI::open_recent_session));
	act = ActionManager::register_action (main_actions, X_("Close"), _("Close"),  mem_fun(*this, &ARDOUR_UI::close_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("AddTrackBus"), _("Add Track/Bus"),  mem_fun(*this, &ARDOUR_UI::add_route));
	ActionManager::session_sensitive_actions.push_back (act);

	
	/* <CMT Additions> */

	PathScanner scanner;
	vector<string*>* results = scanner (getenv ("PATH"), "AniComp", false, false);

	if (results) {
		if (!results->empty()) {
			act = ActionManager::register_action (main_actions, X_("aniConnect"), _("Connect"),  (mem_fun (*editor, &PublicEditor::connect_to_image_compositor)));
			ActionManager::session_sensitive_actions.push_back (act);
		}
		delete results;
	}

	/* </CMT Additions> */

	act = ActionManager::register_action (main_actions, X_("Snapshot"), _("Snapshot"),  mem_fun(*this, &ARDOUR_UI::snapshot_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SaveTemplate"), _("Save Template..."),  mem_fun(*this, &ARDOUR_UI::save_template));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportSession"), _("Export session to audiofile..."),  mem_fun (*editor, &PublicEditor::export_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportSelection"), _("Export selection to audiofile..."),  mem_fun (*editor, &PublicEditor::export_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::time_selection_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportRangeMarkers"), _("Export range markers to audiofile..."),  mem_fun (*editor, &PublicEditor::export_range_markers));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::range_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Export"), _("Export"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("CleanupUnused"), _("Cleanup unused sources"),  mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (main_actions, X_("FlushWastebasket"), _("Flush wastebasket"),  mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::flush_trash));
	ActionManager::session_sensitive_actions.push_back (act);
	
	/* JACK actions for controlling ... JACK */

	Glib::RefPtr<ActionGroup> jack_actions = ActionGroup::create (X_("JACK"));
	ActionManager::register_action (jack_actions, X_("JACK"), _("JACK"));
	ActionManager::register_action (jack_actions, X_("Latency"), _("Latency"));
	
	act = ActionManager::register_action (jack_actions, X_("JACKReconnect"), _("Reconnect"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::reconnect_to_jack));
	ActionManager::jack_opposite_sensitive_actions.push_back (act);

	act = ActionManager::register_action (jack_actions, X_("JACKDisconnect"), _("Disconnect"), mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::disconnect_from_jack));
	ActionManager::jack_sensitive_actions.push_back (act);
	
	RadioAction::Group jack_latency_group;
	
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency32"), X_("32"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 32));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency64"), X_("64"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 64));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency128"), X_("128"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 128));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency256"), X_("256"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 256));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency512"), X_("512"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 512));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency1024"), X_("1024"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 1024));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency2048"), X_("2048"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 2048));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency4096"), X_("4096"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 4096));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency8192"), X_("8192"), bind (mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (nframes_t) 8192));
	ActionManager::jack_sensitive_actions.push_back (act);
	
	/* these actions are intended to be shared across all windows */
	
	common_actions = ActionGroup::create (X_("Common"));
	ActionManager::register_action (main_actions, X_("Windows"), _("Windows"));
	ActionManager::register_action (common_actions, X_("Start-Prefix"), _("start prefix"), mem_fun(*this, &ARDOUR_UI::start_keyboard_prefix));
	ActionManager::register_action (common_actions, X_("Quit"), _("Quit"), (mem_fun(*this, &ARDOUR_UI::finish)));

        /* windows visibility actions */

	ActionManager::register_toggle_action (common_actions, X_("ToggleMaximalEditor"), _("Maximise Editor Space"), mem_fun (*this, &ARDOUR_UI::toggle_editing_space));

	ActionManager::register_action (common_actions, X_("goto-editor"), _("Show Editor"),  mem_fun(*this, &ARDOUR_UI::goto_editor_window));
	ActionManager::register_action (common_actions, X_("goto-mixer"), _("Show Mixer"),  mem_fun(*this, &ARDOUR_UI::goto_mixer_window));
	ActionManager::register_toggle_action (common_actions, X_("ToggleSoundFileBrowser"), _("Sound File Browser"), mem_fun(*this, &ARDOUR_UI::toggle_sound_file_browser));
	ActionManager::register_toggle_action (common_actions, X_("ToggleOptionsEditor"), _("Options Editor"), mem_fun(*this, &ARDOUR_UI::toggle_options_window));
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleInspector"), _("Track/Bus Inspector"), mem_fun(*this, &ARDOUR_UI::toggle_route_params_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleConnections"), _("Connections"), mem_fun(*this, &ARDOUR_UI::toggle_connection_editor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleLocations"), _("Locations"), mem_fun(*this, &ARDOUR_UI::toggle_location_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleBigClock"), _("Big Clock"), mem_fun(*this, &ARDOUR_UI::toggle_big_clock_window));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_action (common_actions, X_("About"), _("About"),  mem_fun(*this, &ARDOUR_UI::show_splash));
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleColorManager"), _("Colors"), mem_fun(*this, &ARDOUR_UI::toggle_color_manager));
	
	act = ActionManager::register_action (common_actions, X_("AddAudioTrack"), _("Add Audio Track"), bind (mem_fun(*this, &ARDOUR_UI::session_add_audio_track), 1, 1, ARDOUR::Normal, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("AddAudioBus"), _("Add Audio Bus"), bind (mem_fun(*this, &ARDOUR_UI::session_add_audio_bus), 1, 1, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("Save"), _("Save"),  bind (mem_fun(*this, &ARDOUR_UI::save_state), string("")));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("RemoveLastCapture"), _("Remove Last Capture"), mem_fun(*this, &ARDOUR_UI::remove_last_capture));
	ActionManager::session_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> transport_actions = ActionGroup::create (X_("Transport"));

	/* do-nothing action for the "transport" menu bar item */

	ActionManager::register_action (transport_actions, X_("Transport"), _("Transport"));

	/* these two are not used by key bindings, instead use ToggleRoll for that. these two do show up in
	   menus and via button proxies.
	*/
	
	act = ActionManager::register_action (transport_actions, X_("Stop"), _("Stop"), mem_fun(*this, &ARDOUR_UI::transport_stop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Roll"), _("Roll"), mem_fun(*this, &ARDOUR_UI::transport_roll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	ActionManager::register_action (transport_actions, X_("ToggleRoll"), _("Start/Stop"), bind (mem_fun (*editor, &PublicEditor::toggle_playback), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	ActionManager::register_action (transport_actions, X_("ToggleRollForgetCapture"), _("Stop + Forget Capture"), bind (mem_fun(*editor, &PublicEditor::toggle_playback), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Loop"), _("Play Loop Range"), mem_fun(*this, &ARDOUR_UI::toggle_session_auto_loop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("PlaySelection"), _("Play Selection"), mem_fun(*this, &ARDOUR_UI::transport_play_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("Record"), _("Enable Record"), mem_fun(*this, &ARDOUR_UI::transport_record));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Rewind"), _("Rewind"), bind (mem_fun(*this, &ARDOUR_UI::transport_rewind), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("RewindSlow"), _("Rewind (Slow)"), bind (mem_fun(*this, &ARDOUR_UI::transport_rewind), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("RewindFast"), _("Rewind (Fast)"), bind (mem_fun(*this, &ARDOUR_UI::transport_rewind), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Forward"), _("Forward"), bind (mem_fun(*this, &ARDOUR_UI::transport_forward), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ForwardSlow"), _("Forward (Slow)"), bind (mem_fun(*this, &ARDOUR_UI::transport_forward), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ForwardFast"), _("Forward (Fast)"), bind (mem_fun(*this, &ARDOUR_UI::transport_forward), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoZero"), _("Goto Zero"), mem_fun(*this, &ARDOUR_UI::transport_goto_zero));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoStart"), _("Goto Start"), mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoEnd"), _("Goto End"), mem_fun(*this, &ARDOUR_UI::transport_goto_end));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchIn"), _("Punch In"), mem_fun(*this, &ARDOUR_UI::toggle_punch_in));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchOut"), _("Punch Out"), mem_fun(*this, &ARDOUR_UI::toggle_punch_out));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleClick"), _("Click"), mem_fun(*this, &ARDOUR_UI::toggle_click));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoInput"), _("Auto Input"), mem_fun(*this, &ARDOUR_UI::toggle_auto_input));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoPlay"), _("Auto Play"), mem_fun(*this, &ARDOUR_UI::toggle_auto_play));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoReturn"), _("Auto Return"), mem_fun(*this, &ARDOUR_UI::toggle_auto_return));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	ActionManager::register_toggle_action (transport_actions, X_("ToggleVideoSync"), _("Sync startup to video"), mem_fun(*this, &ARDOUR_UI::toggle_video_sync));
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleTimeMaster"), _("Time master"), mem_fun(*this, &ARDOUR_UI::toggle_time_master));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack1"), _("Toggle Record Enable Track1"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  0U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack2"), _("Toggle Record Enable Track2"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  1U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack3"), _("Toggle Record Enable Track3"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  2U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack4"), _("Toggle Record Enable Track4"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  3U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack5"), _("Toggle Record Enable Track5"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  4U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack6"), _("Toggle Record Enable Track6"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  5U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack7"), _("Toggle Record Enable Track7"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  6U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack8"), _("Toggle Record Enable Track8"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  7U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack9"), _("Toggle Record Enable Track9"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  8U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack10"), _("Toggle Record Enable Track10"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 9U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack11"), _("Toggle Record Enable Track11"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 10U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack12"), _("Toggle Record Enable Track12"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 11U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack13"), _("Toggle Record Enable Track13"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 12U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack14"), _("Toggle Record Enable Track14"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 13U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack15"), _("Toggle Record Enable Track15"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 14U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack16"), _("Toggle Record Enable Track16"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 15U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack17"), _("Toggle Record Enable Track17"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 16U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack18"), _("Toggle Record Enable Track18"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 17U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack19"), _("Toggle Record Enable Track19"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 18U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack20"), _("Toggle Record Enable Track20"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 19U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack21"), _("Toggle Record Enable Track21"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 20U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack22"), _("Toggle Record Enable Track22"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 21U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack23"), _("Toggle Record Enable Track23"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 22U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack24"), _("Toggle Record Enable Track24"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 23U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack25"), _("Toggle Record Enable Track25"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 24U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack26"), _("Toggle Record Enable Track26"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 25U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack27"), _("Toggle Record Enable Track27"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 26U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack28"), _("Toggle Record Enable Track28"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 27U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack29"), _("Toggle Record Enable Track29"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 28U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack30"), _("Toggle Record Enable Track30"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 29U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack31"), _("Toggle Record Enable Track31"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 30U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("ToggleRecordEnableTrack32"), _("Toggle Record Enable Track32"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 31U));
	ActionManager::session_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> shuttle_actions = ActionGroup::create ("ShuttleActions");
	
	shuttle_actions->add (Action::create (X_("SetShuttleUnitsPercentage"), _("Percentage")), hide_return (bind (mem_fun (*Config, &Configuration::set_shuttle_units), Percentage)));
	shuttle_actions->add (Action::create (X_("SetShuttleUnitsSemitones"), _("Semitones")), hide_return (bind (mem_fun (*Config, &Configuration::set_shuttle_units), Semitones)));

	Glib::RefPtr<ActionGroup> option_actions = ActionGroup::create ("options");

	act = ActionManager::register_toggle_action (option_actions, X_("SendMTC"), _("Send MTC"), mem_fun (*this, &ARDOUR_UI::toggle_send_mtc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMMC"), _("Send MMC"), mem_fun (*this, &ARDOUR_UI::toggle_send_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("UseMMC"), _("Use MMC"), mem_fun (*this, &ARDOUR_UI::toggle_use_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMIDIfeedback"), _("Send MIDI feedback"), mem_fun (*this, &ARDOUR_UI::toggle_send_midi_feedback));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("UseMIDIcontrol"), _("Use MIDI control"), mem_fun (*this, &ARDOUR_UI::toggle_use_midi_control));
	ActionManager::session_sensitive_actions.push_back (act);

	ActionManager::register_toggle_action (option_actions, X_("StopPluginsWithTransport"), _("Stop plugins with transport"), mem_fun (*this, &ARDOUR_UI::toggle_StopPluginsWithTransport));
	ActionManager::register_toggle_action (option_actions, X_("VerifyRemoveLastCapture"), _("Verify remove last capture"), mem_fun (*this, &ARDOUR_UI::toggle_VerifyRemoveLastCapture));
	ActionManager::register_toggle_action (option_actions, X_("StopRecordingOnXrun"), _("Stop recording on xrun"), mem_fun (*this, &ARDOUR_UI::toggle_StopRecordingOnXrun));
	ActionManager::register_toggle_action (option_actions, X_("StopTransportAtEndOfSession"), _("Stop transport at session end"), mem_fun (*this, &ARDOUR_UI::toggle_StopTransportAtEndOfSession));
	ActionManager::register_toggle_action (option_actions, X_("GainReduceFastTransport"), _("-12dB gain reduce ffwd/rewind"), mem_fun (*this, &ARDOUR_UI::toggle_GainReduceFastTransport));
	ActionManager::register_toggle_action (option_actions, X_("LatchedRecordEnable"), _("Rec-enable stays engaged at stop"), mem_fun (*this, &ARDOUR_UI::toggle_LatchedRecordEnable));

	act = ActionManager::register_toggle_action (option_actions, X_("DoNotRunPluginsWhileRecording"), _("Do not run plugins while recording"), mem_fun (*this, &ARDOUR_UI::toggle_DoNotRunPluginsWhileRecording));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (option_actions, X_("LatchedSolo"), _("Latched solo"), mem_fun (*this, &ARDOUR_UI::toggle_LatchedSolo));
	ActionManager::session_sensitive_actions.push_back (act);

	/* !!! REMEMBER THAT RADIO ACTIONS HAVE TO BE HANDLED WITH MORE FINESSE THAN SIMPLE TOGGLES !!! */

	RadioAction::Group meter_falloff_group;
	RadioAction::Group meter_hold_group;

	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffOff"), _("Off"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffOff));
	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffSlowest"), _("Slowest"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffSlowest));
	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffSlow"), _("Slow"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffSlow));
	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffMedium"), _("Medium"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffMedium));
	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffFast"), _("Fast"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffFast));
	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffFaster"), _("Faster"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffFaster));
	ActionManager::register_radio_action (option_actions, meter_falloff_group, X_("MeterFalloffFastest"), _("Fastest"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_falloff), MeterFalloffFastest));

	ActionManager::register_radio_action (option_actions, meter_hold_group,  X_("MeterHoldOff"), _("Off"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_hold), MeterHoldOff));
	ActionManager::register_radio_action (option_actions, meter_hold_group,  X_("MeterHoldShort"), _("Short"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_hold), MeterHoldShort));
	ActionManager::register_radio_action (option_actions, meter_hold_group,  X_("MeterHoldMedium"), _("Medium"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_hold), MeterHoldMedium));
	ActionManager::register_radio_action (option_actions, meter_hold_group,  X_("MeterHoldLong"), _("Long"), bind (mem_fun (*this, &ARDOUR_UI::set_meter_hold), MeterHoldLong));

	RadioAction::Group file_header_group;

	act = ActionManager::register_radio_action (option_actions, file_header_group, X_("FileHeaderFormatBWF"), X_("Broadcast WAVE"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_header_format), ARDOUR::BWF));
	act = ActionManager::register_radio_action (option_actions, file_header_group, X_("FileHeaderFormatWAVE"), X_("WAVE"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_header_format), ARDOUR::WAVE));
	act = ActionManager::register_radio_action (option_actions, file_header_group, X_("FileHeaderFormatWAVE64"), X_("WAVE-64"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_header_format), ARDOUR::WAVE64));
	// act = ActionManager::register_radio_action (option_actions, file_header_group, X_("FileHeaderFormatiXML"), X_("iXML"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_header_format), ARDOUR::iXML));
	// act = ActionManager::register_radio_action (option_actions, file_header_group, X_("FileHeaderFormatRF64"), X_("RF64"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_header_format), ARDOUR::RF64));
	act = ActionManager::register_radio_action (option_actions, file_header_group, X_("FileHeaderFormatCAF"), X_("CAF"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_header_format), ARDOUR::CAF));

	RadioAction::Group file_data_group;

	act = ActionManager::register_radio_action (option_actions, file_data_group, X_("FileDataFormatFloat"), X_("32-bit floating point"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_data_format), ARDOUR::FormatFloat));
	act = ActionManager::register_radio_action (option_actions, file_data_group, X_("FileDataFormat24bit"), X_("24-bit signed integer"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_data_format), ARDOUR::FormatInt24));

	RadioAction::Group monitoring_group;

	act = ActionManager::register_radio_action (option_actions, monitoring_group, X_("UseHardwareMonitoring"), _("Hardware monitoring"), bind (mem_fun (*this, &ARDOUR_UI::set_monitor_model), HardwareMonitoring));
	act = ActionManager::register_radio_action (option_actions, monitoring_group, X_("UseSoftwareMonitoring"), _("Software monitoring"), bind (mem_fun (*this, &ARDOUR_UI::set_monitor_model), SoftwareMonitoring));
	act = ActionManager::register_radio_action (option_actions, monitoring_group, X_("UseExternalMonitoring"), _("External monitoring"), bind (mem_fun (*this, &ARDOUR_UI::set_monitor_model), ExternalMonitoring));

	RadioAction::Group solo_group;

	act = ActionManager::register_radio_action (option_actions, solo_group, X_("SoloInPlace"), _("Solo in-place"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_solo_model), InverseMute)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (option_actions, solo_group, X_("SoloViaBus"), _("Solo via bus"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_solo_model), SoloBus)));
	ActionManager::session_sensitive_actions.push_back (act);

	RadioAction::Group input_auto_connect_group;

	act = ActionManager::register_radio_action (option_actions, input_auto_connect_group, X_("InputAutoConnectPhysical"), _("Auto-connect inputs to physical inputs"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_input_auto_connect), AutoConnectPhysical)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (option_actions, input_auto_connect_group, X_("InputAutoConnectManual"), _("Manually connect inputs"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_input_auto_connect), (AutoConnectOption) 0)));
	ActionManager::session_sensitive_actions.push_back (act);

	RadioAction::Group output_auto_connect_group;

	act = ActionManager::register_radio_action (option_actions, output_auto_connect_group, X_("OutputAutoConnectPhysical"), _("Auto-connect outputs to physical outs"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_output_auto_connect), AutoConnectPhysical)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (option_actions, output_auto_connect_group, X_("OutputAutoConnectMaster"), _("Auto-connect outputs to master bus"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_output_auto_connect), AutoConnectMaster)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (option_actions, output_auto_connect_group, X_("OutputAutoConnectManual"), _("Manually connect outputs"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_output_auto_connect), (AutoConnectOption) 0)));
	ActionManager::session_sensitive_actions.push_back (act);

	ActionManager::add_action_group (shuttle_actions);
	ActionManager::add_action_group (option_actions);
	ActionManager::add_action_group (jack_actions);
	ActionManager::add_action_group (transport_actions);
	ActionManager::add_action_group (main_actions);
	ActionManager::add_action_group (common_actions);
}

void
ARDOUR_UI::toggle_control_protocol (ControlProtocolInfo* cpi)
{
	if (!session) {
		/* this happens when we build the menu bar when control protocol support
		   has been used in the past for some given protocol - the item needs
		   to be made active, but there is no session yet.
		*/
		return;
	}

	if (cpi->protocol == 0) {
		ControlProtocolManager::instance().instantiate (*cpi);
	} else {
		ControlProtocolManager::instance().teardown (*cpi);
	}
}

void
ARDOUR_UI::toggle_control_protocol_feedback (ControlProtocolInfo* cpi, const char* group, string action)
{
	if (!session) {
		/* this happens when we build the menu bar when control protocol support
		   has been used in the past for some given protocol - the item needs
		   to be made active, but there is no session yet.
		*/
		return;
	}

	if (cpi->protocol) {
		Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (group, action.c_str());

		if (act) {
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);

			if (tact) {
				bool x = tact->get_active();

				if (x != cpi->protocol->get_feedback()) {
					cpi->protocol->set_feedback (x);
				}
			}
		}
	}
}

void
ARDOUR_UI::build_control_surface_menu ()
{
	list<ControlProtocolInfo*>::iterator i;
	bool with_feedback;

	/* !!! this has to match the top level entry from ardour.menus */

	string ui = "<menubar name='Main' action='MainMenu'>\n<menu name='Options' action='Options'>\n<menu action='ControlSurfaces'><separator/>\n";

	for (i = ControlProtocolManager::instance().control_protocol_info.begin(); i != ControlProtocolManager::instance().control_protocol_info.end(); ++i) {

		if (!(*i)->mandatory) {

			string action_name = "Toggle";
			action_name += legalize_for_path ((*i)->name);
			action_name += "Surface";
			
			string action_label = (*i)->name;
			
			Glib::RefPtr<Action> act = ActionManager::register_toggle_action (editor->editor_actions, action_name.c_str(), action_label.c_str(),
											  (bind (mem_fun (*this, &ARDOUR_UI::toggle_control_protocol), *i)));
			
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);

			with_feedback = false;
			
			if ((*i)->protocol || (*i)->requested) {
				tact->set_active ();
			}
			
			ui += "<menuitem action='";
			ui += action_name;
			ui += "'/>\n";

			if ((*i)->supports_feedback) {

				string submenu_name = action_name;
				
				submenu_name += "SubMenu";

				ActionManager::register_action (editor->editor_actions, submenu_name.c_str(), _("Controls"));

				action_name += "Feedback";

				Glib::RefPtr<Action> act = ActionManager::register_toggle_action (editor->editor_actions, action_name.c_str(), _("Feedback"),
												  (bind (mem_fun (*this, &ARDOUR_UI::toggle_control_protocol_feedback), 
													 *i, 
													 "Editor",
													 action_name)));
				
				ui += "<menu action='";
				ui += submenu_name;
				ui += "'>\n<menuitem action='";
				ui += action_name;
				ui += "'/>\n</menu>\n";
				
				if ((*i)->protocol) {
					Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
					tact->set_active ((*i)->protocol->get_feedback ());
				}
			}
		}
	}

	ui += "</menu>\n</menu>\n</menubar>\n";

	ActionManager::ui_manager->add_ui_from_string (ui);
}

void
ARDOUR_UI::build_menu_bar ()
{
	build_control_surface_menu ();

	menu_bar = dynamic_cast<MenuBar*> (ActionManager::get_widget (X_("/Main")));
	menu_bar->set_name ("MainMenuBar");

	/*
	 * This is needed because this property does not get installed
	 * until the Menu GObject class is registered, which happens
	 * when the first menu instance is created.
	 */
	Gtk::Settings::get_default()->property_gtk_can_change_accels() = true;	
	
	wall_clock_box.add (wall_clock_label);
	wall_clock_box.set_name ("WallClock");
	wall_clock_label.set_name ("WallClock");

	disk_space_box.add (disk_space_label);
	disk_space_box.set_name ("WallClock");
	disk_space_label.set_name ("WallClock");

	cpu_load_box.add (cpu_load_label);
	cpu_load_box.set_name ("CPULoad");
	cpu_load_label.set_name ("CPULoad");

	buffer_load_box.add (buffer_load_label);
	buffer_load_box.set_name ("BufferLoad");
	buffer_load_label.set_name ("BufferLoad");

	sample_rate_box.add (sample_rate_label);
	sample_rate_box.set_name ("SampleRate");
	sample_rate_label.set_name ("SampleRate");

	menu_hbox.pack_start (*menu_bar, true, true);
	menu_hbox.pack_end (wall_clock_box, false, false, 10);
	menu_hbox.pack_end (disk_space_box, false, false, 10);
	menu_hbox.pack_end (cpu_load_box, false, false, 10);
	menu_hbox.pack_end (buffer_load_box, false, false, 10);
	menu_hbox.pack_end (sample_rate_box, false, false, 10);

	menu_bar_base.set_name ("MainMenuBar");
	menu_bar_base.add (menu_hbox);
}

void
ARDOUR_UI::setup_clock ()
{
	ARDOUR_UI::Clock.connect (bind (mem_fun (big_clock, &AudioClock::set), false));
	
	big_clock_window = new Window (WINDOW_TOPLEVEL);
	
	big_clock_window->set_border_width (0);
	big_clock_window->add  (big_clock);
	big_clock_window->set_title (_("ardour: clock"));
	big_clock_window->set_type_hint (Gdk::WINDOW_TYPE_HINT_MENU);
	big_clock_window->signal_realize().connect (bind (sigc::ptr_fun (set_decoration), big_clock_window,  (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));
	big_clock_window->signal_unmap().connect (bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleBigClock")));

	if (editor) {
		editor->ensure_float (*big_clock_window);
	}

	manage_window (*big_clock_window);
}
