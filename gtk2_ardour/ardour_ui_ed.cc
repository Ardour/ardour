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

*/

/* This file contains any ARDOUR_UI methods that require knowledge of
   the editor, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the PublicEditor class. This
   is to cut down on the nasty compile times for both these classes.
*/

#include <pbd/pathscanner.h>
#include <pbd/fpu.h>

#include <glibmm/miscutils.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>
#include <gtk/gtk.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "engine_dialog.h"
#include "editor.h"
#include "actions.h"
#include "mixer_ui.h"
#include "utils.h"

#include <gtkmm2ext/gtkapplication.h>
#include <gtkmm2ext/application.h>

#include <ardour/session.h>
#include <ardour/profile.h>
#include <ardour/audioengine.h>
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
		editor = new Editor ();
	}

	catch (failed_constructor& err) {
		return -1;
	}

	editor->Realized.connect (mem_fun (*this, &ARDOUR_UI::editor_realized));
	editor->signal_window_state_event().connect (sigc::bind (mem_fun (*this, &ARDOUR_UI::main_window_state_event_handler), true));

	return 0;
}

void
ARDOUR_UI::install_actions ()
{
	Glib::RefPtr<ActionGroup> main_actions = ActionGroup::create (X_("Main"));
	Glib::RefPtr<ActionGroup> main_menu_actions = ActionGroup::create (X_("Main_menu"));
	Glib::RefPtr<Action> act;

	/* menus + submenus that need action items */

	ActionManager::register_action (main_menu_actions, X_("Session"), _("Session"));
	act = ActionManager::register_action (main_menu_actions, X_("Cleanup"), _("Cleanup"));
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::register_action (main_menu_actions, X_("Sync"), _("Sync"));
	ActionManager::register_action (main_menu_actions, X_("Options"), _("Options"));
	ActionManager::register_action (main_menu_actions, X_("WindowMenu"), _("Window"));
	ActionManager::register_action (main_menu_actions, X_("TransportOptions"), _("Options"));
	ActionManager::register_action (main_menu_actions, X_("Help"), _("Help"));
 	ActionManager::register_action (main_menu_actions, X_("KeyMouseActions"), _("Editing Modes"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormat"), _("Audio File Format"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormatHeader"), _("File Type"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormatData"), _("Sample Format"));
	ActionManager::register_action (main_menu_actions, X_("ControlSurfaces"), _("Control Surfaces"));
	ActionManager::register_action (main_menu_actions, X_("Metering"), _("Metering"));
	ActionManager::register_action (main_menu_actions, X_("MeteringFallOffRate"), _("Fall off rate"));
	ActionManager::register_action (main_menu_actions, X_("MeteringHoldTime"), _("Hold Time"));
	ActionManager::register_action (main_menu_actions, X_("Denormals"), _("Denormal Handling"));

	/* the real actions */

	act = ActionManager::register_action (main_actions, X_("New"), _("New"),  hide_return (bind (mem_fun(*this, &ARDOUR_UI::get_session_parameters), true, true)));

	ActionManager::register_action (main_actions, X_("Open"), _("Open"),  mem_fun(*this, &ARDOUR_UI::open_session));
	ActionManager::register_action (main_actions, X_("Recent"), _("Recent"),  mem_fun(*this, &ARDOUR_UI::open_recent_session));
	act = ActionManager::register_action (main_actions, X_("Close"), _("Close"),  mem_fun(*this, &ARDOUR_UI::close_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("AddTrackBus"), _("Add Track/Bus"),  
					      bind (mem_fun(*this, &ARDOUR_UI::add_route), (Gtk::Window*) 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	
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

	act = ActionManager::register_action (main_actions, X_("Snapshot"), _("Snapshot"),  bind (mem_fun(*this, &ARDOUR_UI::snapshot_session), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Save As"), _("Save As"),  bind (mem_fun(*this, &ARDOUR_UI::snapshot_session), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SaveTemplate"), _("Save Template..."),  mem_fun(*this, &ARDOUR_UI::save_template));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportSession"), _("Export session to audiofile..."),  mem_fun (*editor, &PublicEditor::export_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportSelection"), _("Export selected range to audiofile..."),  mem_fun (*editor, &PublicEditor::export_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::time_selection_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportRangeMarkers"), _("Export range markers to multiple audiofiles..."),  mem_fun (*editor, &PublicEditor::export_range_markers));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::range_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Export"), _("Export"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("CleanupUnused"), _("Cleanup unused sources"),  mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	act = ActionManager::register_action (main_actions, X_("FlushWastebasket"), _("Flush wastebasket"),  mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::flush_trash));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	
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
	ActionManager::register_action (common_actions, X_("Quit"), _("Quit"), (hide_return (mem_fun(*this, &ARDOUR_UI::finish))));

        /* windows visibility actions */

	ActionManager::register_toggle_action (common_actions, X_("ToggleMaximalEditor"), _("Maximise Editor Space"), mem_fun (*this, &ARDOUR_UI::toggle_editing_space));

	ActionManager::register_action (common_actions, X_("goto-editor"), _("Show Editor"),  mem_fun(*this, &ARDOUR_UI::goto_editor_window));
	ActionManager::register_action (common_actions, X_("goto-mixer"), _("Show Mixer"),  mem_fun(*this, &ARDOUR_UI::goto_mixer_window));
	ActionManager::register_action (common_actions, X_("toggle-editor-mixer-on-top"), _("Toggle Editor or Mixer on Top"),  mem_fun(*this, &ARDOUR_UI::toggle_editor_mixer_on_top));
	ActionManager::register_toggle_action (common_actions, X_("ToggleOptionsEditor"), _("Preferences"), mem_fun(*this, &ARDOUR_UI::toggle_options_window));
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleInspector"), _("Track/Bus Inspector"), mem_fun(*this, &ARDOUR_UI::toggle_route_params_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleConnections"), _("Connections"), mem_fun(*this, &ARDOUR_UI::toggle_connection_editor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleLocations"), _("Locations"), mem_fun(*this, &ARDOUR_UI::toggle_location_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleBigClock"), _("Big Clock"), mem_fun(*this, &ARDOUR_UI::toggle_big_clock_window));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_action (common_actions, X_("About"), _("About"),  mem_fun(*this, &ARDOUR_UI::show_about));

	ActionManager::register_action (common_actions, X_("Chat"), _("Chat"),  mem_fun(*this, &ARDOUR_UI::launch_chat));
	ActionManager::register_action (common_actions, X_("Manual"), _("Manual"),  mem_fun(*this, &ARDOUR_UI::launch_manual));
	ActionManager::register_action (common_actions, X_("Reference"), _("Reference"),  mem_fun(*this, &ARDOUR_UI::launch_reference));

	ActionManager::register_toggle_action (common_actions, X_("ToggleThemeManager"), _("Theme Manager"), mem_fun(*this, &ARDOUR_UI::toggle_theme_manager));
	ActionManager::register_toggle_action (common_actions, X_("ToggleKeyEditor"), _("Keybindings"), mem_fun(*this, &ARDOUR_UI::toggle_key_editor));
	
	act = ActionManager::register_action (common_actions, X_("AddAudioTrack"), _("Add Audio Track"), bind (mem_fun(*this, &ARDOUR_UI::session_add_audio_track), 1, 1, ARDOUR::Normal, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("AddAudioBus"), _("Add Audio Bus"), bind (mem_fun(*this, &ARDOUR_UI::session_add_audio_bus), 1, 1, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("Save"), _("Save"),  bind (mem_fun(*this, &ARDOUR_UI::save_state), string(""), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
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

	act = ActionManager::register_action (transport_actions, X_("ToggleRoll"), _("Start/Stop"), bind (mem_fun (*this, &ARDOUR_UI::toggle_roll), false, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ToggleRollMaybe"), _("Start/Continue/Stop"), bind (mem_fun (*this, &ARDOUR_UI::toggle_roll), false, true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ToggleRollForgetCapture"), _("Stop + Forget Capture"), bind (mem_fun(*this, &ARDOUR_UI::toggle_roll), true, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	/* these two behave as follows:

	   - if transport speed != 1.0 or != -1.0, change speed to 1.0 or -1.0 (respectively)
	   - otherwise do nothing
	*/
	
	act =  ActionManager::register_action (transport_actions, X_("TransitionToRoll"), _("Transition To Roll"), bind (mem_fun (*editor, &PublicEditor::transition_to_rolling), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("TransitionToReverse"), _("Transition To Reverse"), bind (mem_fun (*editor, &PublicEditor::transition_to_rolling), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);


	act = ActionManager::register_action (transport_actions, X_("Loop"), _("Play Loop Range"), mem_fun(*this, &ARDOUR_UI::toggle_session_auto_loop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("PlaySelection"), _("Play Selection"), mem_fun(*this, &ARDOUR_UI::transport_play_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("Record"), _("Enable Record"), bind (mem_fun(*this, &ARDOUR_UI::transport_record), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("record-roll"), _("Start Recording"), bind (mem_fun(*this, &ARDOUR_UI::transport_record), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
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
	act = ActionManager::register_action (transport_actions, X_("GotoWallClock"), _("Goto Wall Clock"), mem_fun(*this, &ARDOUR_UI::transport_goto_wallclock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("focus-on-clock"), _("Focus On Clock"), mem_fun(primary_clock, &AudioClock::focus));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("primary-clock-bbt"), _("Bars & Beats"), bind (mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::BBT));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-minsec"), _("Minutes & Seconds"), bind (mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::MinSec));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-samples"), _("Samples"), bind (mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Frames));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-off"), _("Off"), bind (mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Off));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("secondary-clock-bbt"), _("Bars & Beats"), bind (mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::BBT));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-minsec"), _("Minutes & Seconds"), bind (mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::MinSec));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-samples"), _("Samples"), bind (mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Frames));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-off"), _("Off"), bind (mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Off));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchIn"), _("Punch In"), mem_fun(*this, &ARDOUR_UI::toggle_punch_in));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchOut"), _("Punch Out"), mem_fun(*this, &ARDOUR_UI::toggle_punch_out));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunch"), _("Punch In/Out"), mem_fun(*this, &ARDOUR_UI::toggle_punch));
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

	act = ActionManager::register_toggle_action (option_actions, X_("toggle-seamless-loop"), _("Seamless Looping"), mem_fun (*this, &ARDOUR_UI::toggle_seamless_loop));

	act = ActionManager::register_toggle_action (option_actions, X_("UseOSC"), _("Use OSC"), mem_fun (*this, &ARDOUR_UI::toggle_use_osc));
#ifndef HAVE_LIBLO
	act->set_sensitive (false);
#endif
	act = ActionManager::register_toggle_action (option_actions, X_("ToggleTapeMachineMode"), _("Tape Machine mode"), mem_fun (*this, &ARDOUR_UI::toggle_TapeMachineMode));
	ActionManager::session_sensitive_actions.push_back (act);

	ActionManager::register_toggle_action (option_actions, X_("SyncEditorAndMixerTrackOrder"), _("Sync Editor and Mixer track order"), mem_fun (*this, &ARDOUR_UI::toggle_sync_order_keys));
	ActionManager::register_toggle_action (option_actions, X_("StopPluginsWithTransport"), _("Stop plugins with transport"), mem_fun (*this, &ARDOUR_UI::toggle_StopPluginsWithTransport));
	ActionManager::register_toggle_action (option_actions, X_("NewPluginsActive"), _("New plugins are active"), mem_fun (*this, &ARDOUR_UI::toggle_new_plugins_active));
	ActionManager::register_toggle_action (option_actions, X_("VerifyRemoveLastCapture"), _("Verify remove last capture"), mem_fun (*this, &ARDOUR_UI::toggle_VerifyRemoveLastCapture));
	ActionManager::register_toggle_action (option_actions, X_("PeriodicSafetyBackups"), _("Make periodic safety backups"), mem_fun (*this, &ARDOUR_UI::toggle_PeriodicSafetyBackups));
	ActionManager::register_toggle_action (option_actions, X_("StopRecordingOnXrun"), _("Stop recording on xrun"), mem_fun (*this, &ARDOUR_UI::toggle_StopRecordingOnXrun));
	ActionManager::register_toggle_action (option_actions, X_("CreateXrunMarker"), _("Create marker at xrun location"), mem_fun (*this, &ARDOUR_UI::toggle_CreateXrunMarker));
	ActionManager::register_toggle_action (option_actions, X_("StopTransportAtEndOfSession"), _("Stop transport at session end"), mem_fun (*this, &ARDOUR_UI::toggle_StopTransportAtEndOfSession));
	ActionManager::register_toggle_action (option_actions, X_("GainReduceFastTransport"), _("-12dB gain reduce ffwd/rewind"), mem_fun (*this, &ARDOUR_UI::toggle_GainReduceFastTransport));
	ActionManager::register_toggle_action (option_actions, X_("LatchedRecordEnable"), _("Rec-enable stays engaged at stop"), mem_fun (*this, &ARDOUR_UI::toggle_LatchedRecordEnable));
	ActionManager::register_toggle_action (option_actions, X_("RegionEquivalentsOverlap"), _("Region equivalents overlap"), mem_fun (*this, &ARDOUR_UI::toggle_RegionEquivalentsOverlap));
	ActionManager::register_toggle_action (option_actions, X_("PrimaryClockDeltaEditCursor"), _("Primary Clock delta to edit point"), mem_fun (*this, &ARDOUR_UI::toggle_PrimaryClockDeltaEditCursor));
	ActionManager::register_toggle_action (option_actions, X_("SecondaryClockDeltaEditCursor"), _("Secondary Clock delta to edit point"), mem_fun (*this, &ARDOUR_UI::toggle_SecondaryClockDeltaEditCursor));	
	ActionManager::register_toggle_action (option_actions, X_("ShowTrackMeters"), _("Enable Editor Meters"), mem_fun (*this, &ARDOUR_UI::toggle_ShowTrackMeters));
	ActionManager::register_toggle_action (option_actions, X_("OnlyCopyImportedFiles"), _("Always copy imported files"), mem_fun (*this, &ARDOUR_UI::toggle_only_copy_imported_files));
	ActionManager::register_toggle_action (option_actions, X_("RubberbandingSnapsToGrid"), _("Rubberbanding Snaps to Grid"), mem_fun (*this, &ARDOUR_UI::toggle_rubberbanding_snaps_to_grid));
	ActionManager::register_toggle_action (option_actions, X_("AutoAnalyseAudio"), _("Auto-analyse new audio"), mem_fun (*this, &ARDOUR_UI::toggle_auto_analyse_audio));

	ActionManager::register_toggle_action (option_actions, X_("DefaultNarrowMS"), _("Use narrow mixer strips"), mem_fun (*this, &ARDOUR_UI::toggle_use_narrow_ms));
	ActionManager::register_toggle_action (option_actions, X_("NameNewMarkers"), _("Name New Markers"), mem_fun (*this, &ARDOUR_UI::toggle_NameNewMarkers));

	RadioAction::Group denormal_group;

	ActionManager::register_toggle_action (option_actions, X_("DenormalProtection"), _("Use DC bias"), mem_fun (*this, &ARDOUR_UI::toggle_denormal_protection));

	ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalNone"), _("No processor handling"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalNone));

	// as of September 10th 2007, Valgrind cannot handle various FPU flag setting instructions
	// so avoid them

	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {

		/* we still need these actions to exist, but make them all insensitive */

		act = ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalFTZ"), _("Use FlushToZero"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalFTZ));
		act->set_sensitive (false);
		act = ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalDAZ"), _("Use DenormalsAreZero"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalDAZ));
		act->set_sensitive (false);
		act = ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalFTZDAZ"), _("Use FlushToZero & DenormalsAreZero"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalFTZDAZ));
		act->set_sensitive (false);

	} else {

		FPU fpu;

		act = ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalFTZ"), _("Use FlushToZero"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalFTZ));
		if (!fpu.has_flush_to_zero()) {
			act->set_sensitive (false);
		}

		act = ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalDAZ"), _("Use DenormalsAreZero"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalDAZ));
		if (!fpu.has_denormals_are_zero()) {
			act->set_sensitive (false);
		}

		act = ActionManager::register_radio_action (option_actions, denormal_group, X_("DenormalFTZDAZ"), _("Use FlushToZero & DenormalsAreZero"), bind (mem_fun (*this, &ARDOUR_UI::set_denormal_model), DenormalFTZDAZ));
		if (!fpu.has_flush_to_zero() || !fpu.has_denormals_are_zero()) {
			act->set_sensitive (false);
		}
	}

	act = ActionManager::register_toggle_action (option_actions, X_("DoNotRunPluginsWhileRecording"), _("Do Not Run Plugins while Recording"), mem_fun (*this, &ARDOUR_UI::toggle_DoNotRunPluginsWhileRecording));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("LatchedSolo"), _("Latched solo"), mem_fun (*this, &ARDOUR_UI::toggle_LatchedSolo));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("ShowSoloMutes"), _("Show solo muting"), mem_fun (*this, &ARDOUR_UI::toggle_ShowSoloMutes));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SoloMuteOverride"), _("Override muting"), mem_fun (*this, &ARDOUR_UI::toggle_SoloMuteOverride));
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
	act = ActionManager::register_radio_action (option_actions, file_data_group, X_("FileDataFormat24bit"), X_("24-bit integer"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_data_format), ARDOUR::FormatInt24));
	act = ActionManager::register_radio_action (option_actions, file_data_group, X_("FileDataFormat16bit"), X_("16-bit integer"), bind (mem_fun (*this, &ARDOUR_UI::set_native_file_data_format), ARDOUR::FormatInt16));

	RadioAction::Group monitoring_group;

	/* it would be nice not to even show this item, but that messes up the 
	   menus which are not dynamically modified at runtime. so we make it
	   sensitive only if it makes sense.
	*/

#ifndef __APPLE__
        /* JACK has no monitoring capabilities with the CoreAudio backend */
	act = ActionManager::register_radio_action (option_actions, monitoring_group, X_("UseHardwareMonitoring"), _("JACK does monitoring"), bind (mem_fun (*this, &ARDOUR_UI::set_monitor_model), HardwareMonitoring));
	if (!engine->can_request_hardware_monitoring()) {
		act->set_sensitive (false);
	}
#endif
	act = ActionManager::register_radio_action (option_actions, monitoring_group, X_("UseSoftwareMonitoring"), string_compose (_("%1 does monitoring"), PROGRAM_NAME).c_str(), bind (mem_fun (*this, &ARDOUR_UI::set_monitor_model), SoftwareMonitoring));
	act = ActionManager::register_radio_action (option_actions, monitoring_group, X_("UseExternalMonitoring"), _("Audio Hardware does monitoring"), bind (mem_fun (*this, &ARDOUR_UI::set_monitor_model), ExternalMonitoring));

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

	RadioAction::Group remote_group;

	act = ActionManager::register_radio_action (option_actions, remote_group, X_("RemoteUserDefined"), _("Remote ID assigned by User"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_remote_model), UserOrdered)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (option_actions, remote_group, X_("RemoteMixerDefined"), _("Remote ID follows order of Mixer"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_remote_model), MixerOrdered)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (option_actions, remote_group, X_("RemoteEditorDefined"), _("Remote ID follows order of Editor"), hide_return (bind (mem_fun (*this, &ARDOUR_UI::set_remote_model), EditorOrdered)));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (option_actions, X_("AutoRebinding"), _("Auto Rebind Controls"), mem_fun (*(this->mixer), &Mixer_UI::toggle_auto_rebinding));
	ActionManager::session_sensitive_actions.push_back (act);

	if (getenv ("ARDOUR_BUNDLED")) {
		act = ActionManager::register_toggle_action (main_actions, X_("EnableTranslation"), _("Enable Translations"), mem_fun (*this, &ARDOUR_UI::toggle_translations));
		if (act) {
			RefPtr<ToggleAction> ract = RefPtr<ToggleAction>::cast_dynamic (act);
			if (ract) {
				ract->set_active (!ARDOUR::translations_are_disabled());
			}
		}
	}

	ActionManager::add_action_group (shuttle_actions);
	ActionManager::add_action_group (option_actions);
	ActionManager::add_action_group (jack_actions);
	ActionManager::add_action_group (transport_actions);
	ActionManager::add_action_group (main_actions);
	ActionManager::add_action_group (main_menu_actions);
	ActionManager::add_action_group (common_actions);
}

void
ARDOUR_UI::control_protocol_status_change (ControlProtocolInfo* cpi)
{
	string action_name = "Toggle";
	action_name += legalize_for_path (cpi->name);
	action_name += "Surface";

	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), action_name.c_str());
	
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	
	if (!tact) {
		return;
	}

	if (tact->get_active() == (cpi->protocol != 0)) {
		/* action state matches current protocol state, ignore this
		   bogus toggle (e.g. caused by initial ::set_active() call
		*/
		return;
	}

	if (cpi->protocol == 0) {
		tact->set_active (false);
	} else {
		tact->set_active (true);
	}
}

void
ARDOUR_UI::toggle_control_protocol (ControlProtocolInfo* cpi)
{
	string action_name = "Toggle";
	action_name += legalize_for_path (cpi->name);
	action_name += "Surface";

	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (X_("Editor"), action_name.c_str());
	
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	
	if (!tact) {
		return;
	}

	if (tact->get_active() == (cpi->protocol != 0)) {
		/* action state matches current protocol state, ignore this
		   bogus toggle (e.g. caused by initial ::set_active() call
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
ARDOUR_UI::set_jack_buffer_size (nframes_t nframes)
{
	Glib::RefPtr<Action> action;
	const char* action_name = 0;

	switch (nframes) {
	case 32:
		action_name = X_("JACKLatency32");
		break;
	case 64:
		action_name = X_("JACKLatency64");
		break;
	case 128:
		action_name = X_("JACKLatency128");
		break;
	case 256:
		action_name = X_("JACKLatency256");
		break;
	case 512:
		action_name = X_("JACKLatency512");
		break;
	case 1024:
		action_name = X_("JACKLatency1024");
		break;
	case 2048:
		action_name = X_("JACKLatency2048");
		break;
	case 4096:
		action_name = X_("JACKLatency4096");
		break;
	case 8192:
		action_name = X_("JACKLatency8192");
		break;
	default:
		/* XXX can we do anything useful ? */
		break;
	}

	if (action_name) {

		action = ActionManager::get_action (X_("JACK"), action_name);

		if (action) {
			Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (action);

			if (ract && ract->get_active()) {
				engine->request_buffer_size (nframes);
				update_sample_rate (0);
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
	if (!Profile->get_sae()) {
		build_control_surface_menu ();
	}

	menu_bar = dynamic_cast<MenuBar*> (ActionManager::get_widget (X_("/Main")));
	menu_bar->set_name ("MainMenuBar");

	/*
	 * This is needed because this property does not get installed
	 * until the Menu GObject class is registered, which happens
	 * when the first menu instance is created.
	 */
	// XXX bug in gtkmm causes this to popup an error message
	// Gtk::Settings::get_default()->property_gtk_can_change_accels() = true;	
	// so use this instead ...
	gtk_settings_set_long_property (gtk_settings_get_default(), "gtk-can-change-accels", 1, "Ardour:designers");

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

#ifndef TOP_MENUBAR
	menu_hbox.pack_start (*menu_bar, true, true);
#else
	use_menubar_as_top_menubar ();
#endif
	if (!Profile->get_small_screen()) {
#ifndef GTKOSX		
		// OSX provides its own wallclock, thank you very much
		menu_hbox.pack_end (wall_clock_box, false, false, 2);
#endif
		menu_hbox.pack_end (disk_space_box, false, false, 4);
	}
	menu_hbox.pack_end (cpu_load_box, false, false, 4);
	menu_hbox.pack_end (buffer_load_box, false, false, 4);
	menu_hbox.pack_end (sample_rate_box, false, false, 4);

	menu_bar_base.set_name ("MainMenuBar");
	menu_bar_base.add (menu_hbox);
}

void
ARDOUR_UI::use_menubar_as_top_menubar ()
{
	Gtk::Widget* widget;
	Application* app = Application::instance ();

	/* Quit will be taken of separately */

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/Quit"))) {
		widget->hide ();
	}

	GtkApplicationMenuGroup* group = app->add_app_menu_group ();

	if ((widget = ActionManager::get_widget ("/ui/Main/Help/About"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
	}

	if ((widget = ActionManager::get_widget ("/ui/Main/WindowMenu/ToggleOptionsEditor"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
	}

	app->set_menu_bar (*menu_bar);
}

void
ARDOUR_UI::setup_clock ()
{
	ARDOUR_UI::Clock.connect (bind (mem_fun (big_clock, &AudioClock::set), false));
	
	big_clock_window = new Window (WINDOW_TOPLEVEL);
	
	big_clock_window->set_keep_above (true);
	big_clock_window->set_border_width (0);
	big_clock_window->add  (big_clock);
	big_clock_window->signal_key_press_event().connect (bind (sigc::ptr_fun (relay_key_press), big_clock_window));

	big_clock_window->set_title (_("Clock"));
	big_clock_window->set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
	big_clock_window->signal_realize().connect (bind (sigc::ptr_fun (set_decoration), big_clock_window,  (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH)));
	big_clock_window->signal_unmap().connect (bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleBigClock")));

	manage_window (*big_clock_window);
}

void
ARDOUR_UI::float_big_clock (Gtk::Window* parent)
{
	if (big_clock_window) {
		if (parent) {
			big_clock_window->set_transient_for (*parent);
		} else {
			gtk_window_set_transient_for (big_clock_window->gobj(), (GtkWindow*) 0);
		}
	}
}

