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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

/* This file contains any ARDOUR_UI methods that require knowledge of
   the editor, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the PublicEditor class. This
   is to cut down on the nasty compile times for both these classes.
*/

#include <cmath>

#include <glibmm/miscutils.h>
#include <gtkmm/accelmap.h>
#include <gtk/gtk.h>

#include "gtkmm2ext/cairo_packer.h"
#include "gtkmm2ext/tearoff.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "pbd/file_utils.h"
#include "pbd/fpu.h"
#include "pbd/convert.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "keyboard.h"
#include "monitor_section.h"
#include "engine_dialog.h"
#include "editor.h"
#include "editing.h"
#include "actions.h"
#include "meterbridge.h"
#include "luawindow.h"
#include "mixer_ui.h"
#include "startup.h"
#include "window_manager.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "main_clock.h"
#include "rc_option_editor.h"

#include <gtkmm2ext/application.h>

#include "ardour/session.h"
#include "ardour/profile.h"
#include "ardour/audioengine.h"

#include "control_protocol/control_protocol.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;

int
ARDOUR_UI::create_editor ()
{
	try {
		editor = new Editor ();
		editor->StateChange.connect (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_state_change));
	}

	catch (failed_constructor& err) {
		return -1;
	}

	// editor->signal_event().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), editor));

	return 0;
}

int
ARDOUR_UI::create_luawindow ()

{
	try {
		luawindow = LuaWindow::instance ();
	}
	catch (failed_constructor& err) {
		return -1;
	}

	return 0;
}

void
ARDOUR_UI::escape ()
{
	Escape (); /* EMIT SIGNAL */
}

void
ARDOUR_UI::close_current_dialog ()
{
	Keyboard::close_current_dialog ();
}

void
ARDOUR_UI::install_actions ()
{
	Glib::RefPtr<ActionGroup> main_actions = global_actions.create_action_group (X_("Main"));
	Glib::RefPtr<ActionGroup> main_menu_actions = global_actions.create_action_group (X_("Main_menu"));
	Glib::RefPtr<Action> act;

	global_actions.register_action (main_actions, X_("Escape"), _("Escape (deselect all)"), sigc::mem_fun (*this, &ARDOUR_UI::escape));
	/* This is hard-wired into the Keyboard code as "Primary-w". Maybe it
	   doesn't need to be. This action makes it possible to do this from a
	   control surface.
	*/
	global_actions.register_action (main_actions, X_("close-current-dialog"), _("Close Current Dialog"), sigc::mem_fun (*this, &ARDOUR_UI::close_current_dialog));

	/* menus + submenus that need action items */

	global_actions.register_action (main_menu_actions, X_("Session"), _("Session"));
	act = global_actions.register_action (main_menu_actions, X_("Cleanup"), _("Clean-up"));
	ActionManager::write_sensitive_actions.push_back (act);
	global_actions.register_action (main_menu_actions, X_("Sync"), _("Sync"));
	global_actions.register_action (main_menu_actions, X_("TransportOptions"), _("Options"));
	global_actions.register_action (main_menu_actions, X_("WindowMenu"), _("Window"));
	global_actions.register_action (main_menu_actions, X_("MixerMenu"), _("Mixer"));
	global_actions.register_action (main_menu_actions, X_("EditorMenu"), _("Editor"));
	global_actions.register_action (main_menu_actions, X_("PrefsMenu"), _("Preferences"));
	global_actions.register_action (main_menu_actions, X_("DetachMenu"), _("Detach"));
	global_actions.register_action (main_menu_actions, X_("Help"), _("Help"));
	global_actions.register_action (main_menu_actions, X_("KeyMouseActions"), _("Misc. Shortcuts"));
	global_actions.register_action (main_menu_actions, X_("AudioFileFormat"), _("Audio File Format"));
	global_actions.register_action (main_menu_actions, X_("AudioFileFormatHeader"), _("File Type"));
	global_actions.register_action (main_menu_actions, X_("AudioFileFormatData"), _("Sample Format"));
	global_actions.register_action (main_menu_actions, X_("ControlSurfaces"), _("Control Surfaces"));
	global_actions.register_action (main_menu_actions, X_("Plugins"), _("Plugins"));
	global_actions.register_action (main_menu_actions, X_("Metering"), _("Metering"));
	global_actions.register_action (main_menu_actions, X_("MeteringFallOffRate"), _("Fall Off Rate"));
	global_actions.register_action (main_menu_actions, X_("MeteringHoldTime"), _("Hold Time"));
	global_actions.register_action (main_menu_actions, X_("Denormals"), _("Denormal Handling"));

	/* the real actions */

	act = global_actions.register_action (main_actions, X_("New"), _("New..."),  hide_return (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::get_session_parameters), false, true, "")));

	global_actions.register_action (main_actions, X_("Open"), _("Open..."),  sigc::mem_fun(*this, &ARDOUR_UI::open_session));
	global_actions.register_action (main_actions, X_("Recent"), _("Recent..."),  sigc::mem_fun(*this, &ARDOUR_UI::open_recent_session));
	act = global_actions.register_action (main_actions, X_("Close"), _("Close"),  sigc::mem_fun(*this, &ARDOUR_UI::close_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("AddTrackBus"), _("Add Track, Bus or VCA..."), sigc::mem_fun(*this, &ARDOUR_UI::add_route));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::rec_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("duplicate-routes"), _("Duplicate Tracks/Busses..."),
	                                      sigc::mem_fun(*this, &ARDOUR_UI::start_duplicate_routes));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::track_selection_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("cancel-solo"), _("Cancel Solo"), sigc::mem_fun(*this, &ARDOUR_UI::cancel_solo));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("Scripting"), S_("Session|Scripting"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("OpenVideo"), _("Open Video..."),
					      sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::add_video), (Gtk::Window*) 0));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (main_actions, X_("CloseVideo"), _("Remove Video"),
					      sigc::mem_fun (*this, &ARDOUR_UI::remove_video));
	act->set_sensitive (false);
	act = global_actions.register_action (main_actions, X_("ExportVideo"), _("Export to Video File..."),
	                                      hide_return (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::export_video), false)));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("SnapshotStay"), _("Snapshot (& keep working on current version) ..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::snapshot_session), false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("SnapshotSwitch"), _("Snapshot (& switch to new version) ..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::snapshot_session), true));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("QuickSnapshotStay"), _("Quick Snapshot (& keep working on current version) ..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::quick_snapshot_session), false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("QuickSnapshotSwitch"), _("Quick Snapshot (& switch to new version) ..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::quick_snapshot_session), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("SaveAs"), _("Save As..."), sigc::mem_fun(*this, &ARDOUR_UI::save_session_as));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("Archive"), _("Archive..."), sigc::mem_fun(*this, &ARDOUR_UI::archive_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("Rename"), _("Rename..."), sigc::mem_fun(*this, &ARDOUR_UI::rename_session));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("SaveTemplate"), _("Save Template..."),  sigc::mem_fun(*this, &ARDOUR_UI::save_template));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("Metadata"), _("Metadata"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("EditMetadata"), _("Edit Metadata..."),  sigc::mem_fun(*this, &ARDOUR_UI::edit_metadata));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("ImportMetadata"), _("Import Metadata..."),  sigc::mem_fun(*this, &ARDOUR_UI::import_metadata));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("ExportAudio"), _("Export to Audio File(s)..."),  sigc::mem_fun (*editor, &PublicEditor::export_audio));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("StemExport"), _("Stem export..."),  sigc::mem_fun (*editor, &PublicEditor::stem_export));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("Export"), _("Export"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("CleanupUnused"), _("Clean-up Unused Sources..."),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("CleanupPeakFiles"), _("Reset Peak Files"),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup_peakfiles));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (main_actions, X_("FlushWastebasket"), _("Flush Wastebasket"),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::flush_trash));

	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::session_sensitive_actions.push_back (act);

	/* these actions are intended to be shared across all windows */

	common_actions = global_actions.create_action_group (X_("Common"));
	global_actions.register_action (common_actions, X_("Quit"), _("Quit"), (hide_return (sigc::mem_fun(*this, &ARDOUR_UI::finish))));
	global_actions.register_action (common_actions, X_("Hide"), _("Hide"), sigc::mem_fun (*this, &ARDOUR_UI::hide_application));

	global_actions.register_action (common_actions, X_("show-preferences"), _("Show"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::show_tabbable), rc_option_editor));
	global_actions.register_action (common_actions, X_("menu-show-preferences"), _("Preferences"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::show_tabbable), rc_option_editor));

	global_actions.register_action (common_actions, X_("hide-editor"), _("Hide"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::hide_tabbable), editor));
	global_actions.register_action (common_actions, X_("hide-mixer"), _("Hide"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::hide_tabbable), mixer));
	global_actions.register_action (common_actions, X_("hide-preferences"), _("Hide"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::hide_tabbable), rc_option_editor));

	global_actions.register_action (common_actions, X_("attach-editor"), _("Attach"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::attach_tabbable), editor));
	global_actions.register_action (common_actions, X_("attach-mixer"), _("Attach"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::attach_tabbable), mixer));
	global_actions.register_action (common_actions, X_("attach-preferences"), _("Attach"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::attach_tabbable), rc_option_editor));

	global_actions.register_action (common_actions, X_("detach-editor"), _("Detach"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::detach_tabbable), editor));
	global_actions.register_action (common_actions, X_("detach-mixer"), _("Detach"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::detach_tabbable), mixer));
	global_actions.register_action (common_actions, X_("detach-preferences"), _("Detach"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::detach_tabbable), rc_option_editor));

	Glib::RefPtr<Gtk::ActionGroup> window_actions = ARDOUR_UI::instance()->global_actions.create_action_group (X_("Window"));
	global_actions.register_action (window_actions, X_("show-mixer"), _("Show Mixer"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::show_tabbable), mixer));

	/* these actions are all currently implemented by the Editor, but need
	 * to be accessible from anywhere as actions.
	 */

	act = global_actions.register_action (common_actions, "alternate-jump-forward-to-mark", _("Jump to Next Mark"), sigc::mem_fun(editor, &PublicEditor::jump_forward_to_mark));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "alternate-jump-backward-to-mark", _("Jump to Previous Mark"), sigc::mem_fun(editor, &PublicEditor::jump_backward_to_mark));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions, "set-session-start-from-playhead", _("Set Session Start from Playhead"), sigc::mem_fun(editor, &PublicEditor::set_session_start_from_playhead));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "set-session-end-from-playhead", _("Set Session End from Playhead"), sigc::mem_fun(editor, &PublicEditor::set_session_end_from_playhead));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions, "toggle-location-at-playhead", _("Toggle Mark at Playhead"), sigc::mem_fun(editor, &PublicEditor::toggle_location_at_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "add-location-from-playhead", _("Add Mark from Playhead"), sigc::mem_fun(editor, &PublicEditor::add_location_from_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "alternate-add-location-from-playhead", _("Add Mark from Playhead"), sigc::mem_fun(editor, &PublicEditor::add_location_from_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions, "remove-location-from-playhead", _("Remove Mark at Playhead"), sigc::mem_fun(editor, &PublicEditor::remove_location_at_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "alternate-remove-location-from-playhead", _("Remove Mark at Playhead"), sigc::mem_fun(editor, &PublicEditor::remove_location_at_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions, "nudge-next-forward", _("Nudge Next Later"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::nudge_forward), true, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "nudge-next-backward", _("Nudge Next Earlier"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::nudge_backward), true, false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions, "nudge-playhead-forward", _("Nudge Playhead Forward"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::nudge_forward), false, true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "nudge-playhead-backward", _("Nudge Playhead Backward"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::nudge_backward), false, true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "playhead-forward-to-grid", _("Playhead to Next Grid"), sigc::mem_fun(editor, &PublicEditor::playhead_forward_to_grid));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "playhead-backward-to-grid", _("Playhead to Previous Grid"), sigc::mem_fun(editor, &PublicEditor::playhead_backward_to_grid));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions, "start-range-from-playhead", _("Start Range from Playhead"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::keyboard_selection_begin), Editing::EDIT_IGNORE_MOUSE ));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "finish-range-from-playhead", _("Finish Range from Playhead"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::keyboard_selection_finish), false, Editing::EDIT_IGNORE_MOUSE ));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "start-range", _("Start Range"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::keyboard_selection_begin), Editing::EDIT_IGNORE_NONE));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "finish-range", _("Finish Range"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::keyboard_selection_finish), false, Editing::EDIT_IGNORE_NONE));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "start-punch-range", _("Start Punch Range"), sigc::mem_fun(editor, &PublicEditor::set_punch_start_from_edit_point));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "finish-punch-range", _("Finish Punch Range"), sigc::mem_fun(editor, &PublicEditor::set_punch_end_from_edit_point));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "start-loop-range", _("Start Loop Range"), sigc::mem_fun(editor, &PublicEditor::set_loop_start_from_edit_point));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "finish-loop-range", _("Finish Loop Range"), sigc::mem_fun(editor, &PublicEditor::set_loop_end_from_edit_point));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "alt-start-range", _("Start Range"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::keyboard_selection_begin), Editing::EDIT_IGNORE_NONE));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (common_actions, "alt-finish-range", _("Finish Range"), sigc::bind (sigc::mem_fun(editor, &PublicEditor::keyboard_selection_finish), false, Editing::EDIT_IGNORE_NONE));
	ActionManager::session_sensitive_actions.push_back (act);

	/* These "change" actions are not intended to be used inside menus, but
	   are for the tab/window control buttons, which have somewhat odd
	   semantics.
	*/
	global_actions.register_action (common_actions, X_("change-editor-visibility"), _("Change"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::button_change_tabbable_visibility), editor));
	global_actions.register_action (common_actions, X_("change-mixer-visibility"), _("Change"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::button_change_tabbable_visibility), mixer));
	global_actions.register_action (common_actions, X_("change-preferences-visibility"), _("Change"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::button_change_tabbable_visibility), rc_option_editor));

	/* These "change" actions are not intended to be used inside menus, but
	   are for the tab/window control key bindings, which have somewhat odd
	   semantics.
	*/
	global_actions.register_action (common_actions, X_("key-change-editor-visibility"), _("Change"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::key_change_tabbable_visibility), editor));
	global_actions.register_action (common_actions, X_("key-change-mixer-visibility"), _("Change"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::key_change_tabbable_visibility), mixer));
	global_actions.register_action (common_actions, X_("key-change-preferences-visibility"), _("Change"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::key_change_tabbable_visibility), rc_option_editor));

	global_actions.register_action (common_actions, X_("previous-tab"), _("Previous Tab"), sigc::mem_fun (*this, &ARDOUR_UI::step_up_through_tabs));
	global_actions.register_action (common_actions, X_("next-tab"), _("Next Tab"), sigc::mem_fun (*this, &ARDOUR_UI::step_down_through_tabs));

	/* windows visibility actions */

	global_actions.register_toggle_action (common_actions, X_("ToggleMaximalEditor"), _("Maximise Editor Space"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_editing_space));
	global_actions.register_toggle_action (common_actions, X_("ToggleMaximalMixer"), _("Maximise Mixer Space"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_mixer_space));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_toggle_action (common_actions, X_("ToggleMixerList"), _("Toggle Mixer List"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_mixer_list));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_toggle_action (common_actions, X_("ToggleMonitorSection"), _("Toggle Monitor Section Visibility"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_monitor_section_visibility));
	act->set_sensitive (false);

	if (Profile->get_mixbus()) {
		global_actions.register_action (common_actions, X_("show-ui-prefs"), _("Show more UI preferences"), sigc::mem_fun (*this, &ARDOUR_UI::show_ui_prefs));
	}

	global_actions.register_action (common_actions, X_("toggle-luawindow"), S_("Window|Scripting"),  sigc::mem_fun(*this, &ARDOUR_UI::toggle_luawindow));
	global_actions.register_action (common_actions, X_("toggle-meterbridge"), S_("Window|Meterbridge"),  sigc::mem_fun(*this, &ARDOUR_UI::toggle_meterbridge));

	act = global_actions.register_action (common_actions, X_("NewMIDITracer"), _("MIDI Tracer"), sigc::mem_fun(*this, &ARDOUR_UI::new_midi_tracer_window));
	ActionManager::session_sensitive_actions.push_back (act);

	global_actions.register_action (common_actions, X_("chat"), _("Chat"),  sigc::mem_fun(*this, &ARDOUR_UI::launch_chat));
	/** TRANSLATORS: This is `Manual' in the sense of an instruction book that tells a user how to use Ardour */
	global_actions.register_action (common_actions, X_("manual"), S_("Help|Manual"),  mem_fun(*this, &ARDOUR_UI::launch_manual));
	global_actions.register_action (common_actions, X_("reference"), S_("Manual|Reference"),  mem_fun(*this, &ARDOUR_UI::launch_reference));
	global_actions.register_action (common_actions, X_("tracker"), _("Report a Bug"), mem_fun(*this, &ARDOUR_UI::launch_tracker));
	global_actions.register_action (common_actions, X_("cheat-sheet"), _("Cheat Sheet"), mem_fun(*this, &ARDOUR_UI::launch_cheat_sheet));
	global_actions.register_action (common_actions, X_("website"), _("Ardour Website"), mem_fun(*this, &ARDOUR_UI::launch_website));
	global_actions.register_action (common_actions, X_("website-dev"), _("Ardour Development"), mem_fun(*this, &ARDOUR_UI::launch_website_dev));
	global_actions.register_action (common_actions, X_("forums"), _("User Forums"), mem_fun(*this, &ARDOUR_UI::launch_forums));
	global_actions.register_action (common_actions, X_("howto-report"), _("How to Report a Bug"), mem_fun(*this, &ARDOUR_UI::launch_howto_report));

	act = global_actions.register_action (common_actions, X_("Save"), _("Save"),  sigc::hide_return (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::save_state), string(""), false)));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions,
			"jump-backward-to-mark", _("Jump to Previous Mark"), sigc::mem_fun(*editor, &PublicEditor::jump_backward_to_mark));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions,
			"jump-forward-to-mark", _("Jump to Next Mark"), sigc::mem_fun(*editor, &PublicEditor::jump_forward_to_mark));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (common_actions,
			X_("addExistingAudioFiles"), _("Import"), sigc::mem_fun (*editor, &PublicEditor::external_audio_dialog));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> transport_actions = global_actions.create_action_group (X_("Transport"));

	/* do-nothing action for the "transport" menu bar item */

	global_actions.register_action (transport_actions, X_("Transport"), _("Transport"));

	/* these two are not used by key bindings, instead use ToggleRoll for that. these two do show up in
	   menus and via button proxies.
	*/

	act = global_actions.register_action (transport_actions, X_("Stop"), _("Stop"), sigc::mem_fun(*this, &ARDOUR_UI::transport_stop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("Roll"), _("Roll"), sigc::mem_fun(*this, &ARDOUR_UI::transport_roll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("ToggleRoll"), _("Start/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("alternate-ToggleRoll"), _("Start/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("ToggleRollMaybe"), _("Start/Continue/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("ToggleRollForgetCapture"), _("Stop and Forget Capture"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::toggle_roll), true, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	/* these two behave as follows:

	   - if transport speed != 1.0 or != -1.0, change speed to 1.0 or -1.0 (respectively)
	   - otherwise do nothing
	*/

	act = global_actions.register_action (transport_actions, X_("TransitionToRoll"), _("Transition to Roll"), sigc::bind (sigc::mem_fun (*editor, &PublicEditor::transition_to_rolling), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("TransitionToReverse"), _("Transition to Reverse"), sigc::bind (sigc::mem_fun (*editor, &PublicEditor::transition_to_rolling), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("Loop"), _("Play Loop Range"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_session_auto_loop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("PlaySelection"), _("Play Selection"), sigc::mem_fun(*this, &ARDOUR_UI::transport_play_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("PlayPreroll"), _("Play w/Preroll"), sigc::mem_fun(*this, &ARDOUR_UI::transport_play_preroll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("RecordPreroll"), _("Record w/Preroll"), sigc::mem_fun(*this, &ARDOUR_UI::transport_rec_preroll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("RecordCountIn"), _("Record w/Count-In"), sigc::mem_fun(*this, &ARDOUR_UI::transport_rec_count_in));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("Record"), _("Enable Record"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("record-roll"), _("Start Recording"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("alternate-record-roll"), _("Start Recording"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("Rewind"), _("Rewind"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("RewindSlow"), _("Rewind (Slow)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("RewindFast"), _("Rewind (Fast)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("Forward"), _("Forward"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("ForwardSlow"), _("Forward (Slow)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("ForwardFast"), _("Forward (Fast)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("GotoZero"), _("Go to Zero"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_zero));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("GotoStart"), _("Go to Start"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("alternate-GotoStart"), _("Go to Start"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("GotoEnd"), _("Go to End"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_end));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("GotoWallClock"), _("Go to Wall Clock"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_wallclock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	//these actions handle the numpad events, ProTools style
	act = global_actions.register_action (transport_actions, X_("numpad-decimal"), _("Numpad Decimal"), mem_fun(*this, &ARDOUR_UI::transport_numpad_decimal));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("alternate-numpad-decimal"), _("Numpad Decimal"), mem_fun(*this, &ARDOUR_UI::transport_numpad_decimal));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-0"), _("Numpad 0"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-1"), _("Numpad 1"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-2"), _("Numpad 2"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 2));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-3"), _("Numpad 3"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 3));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-4"), _("Numpad 4"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 4));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-5"), _("Numpad 5"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 5));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-6"), _("Numpad 6"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 6));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-7"), _("Numpad 7"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 7));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-8"), _("Numpad 8"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 8));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("numpad-9"), _("Numpad 9"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 9));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("focus-on-clock"), _("Focus On Clock"), sigc::mem_fun(*this, &ARDOUR_UI::focus_on_clock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("primary-clock-timecode"), _("Timecode"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Timecode, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("primary-clock-bbt"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::BBT, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("primary-clock-minsec"), _("Minutes & Seconds"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::MinSec, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("primary-clock-samples"), _("Samples"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Frames, false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_action (transport_actions, X_("secondary-clock-timecode"), _("Timecode"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Timecode, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("secondary-clock-bbt"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::BBT, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("secondary-clock-minsec"), _("Minutes & Seconds"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::MinSec, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_action (transport_actions, X_("secondary-clock-samples"), _("Samples"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Frames, false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_toggle_action (transport_actions, X_("SessionMonitorIn"), _("All Input"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_session_monitoring_in));
	act->set_short_label (_("All In"));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("SessionMonitorDisk"), _("All Disk"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_session_monitoring_disk));
	act->set_short_label (_("All Disk"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = global_actions.register_toggle_action (transport_actions, X_("TogglePunchIn"), _("Punch In"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch_in));
	act->set_short_label (_("In"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("TogglePunchOut"), _("Punch Out"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch_out));
	act->set_short_label (_("Out"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("TogglePunch"), _("Punch In/Out"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch));
	act->set_short_label (_("In/Out"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleClick"), _("Click"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_click));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleAutoInput"), _("Auto Input"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_input));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleAutoPlay"), _("Auto Play"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_play));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleAutoReturn"), _("Auto Return"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_return));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleFollowEdits"), _("Follow Range"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_follow_edits));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);


	act = global_actions.register_toggle_action (transport_actions, X_("ToggleVideoSync"), _("Sync Startup to Video"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_video_sync));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleTimeMaster"), _("Time Master"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_time_master));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (transport_actions, X_("ToggleExternalSync"), _("Use External Positional Sync Source"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_external_sync));
	ActionManager::session_sensitive_actions.push_back (act);

	for (int i = 1; i <= 32; ++i) {
		string const a = string_compose (X_("ToggleRecordEnableTrack%1"), i);
		string const n = string_compose (_("Toggle Record Enable Track %1"), i);
		act = global_actions.register_action (common_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_record_enable), i - 1));
		ActionManager::session_sensitive_actions.push_back (act);
	}

	Glib::RefPtr<ActionGroup> shuttle_actions = global_actions.create_action_group ("ShuttleActions");

	shuttle_actions->add (Action::create (X_("SetShuttleUnitsPercentage"), _("Percentage")), hide_return (sigc::bind (sigc::mem_fun (*Config, &RCConfiguration::set_shuttle_units), Percentage)));
	shuttle_actions->add (Action::create (X_("SetShuttleUnitsSemitones"), _("Semitones")), hide_return (sigc::bind (sigc::mem_fun (*Config, &RCConfiguration::set_shuttle_units), Semitones)));

	Glib::RefPtr<ActionGroup> option_actions = global_actions.create_action_group ("Options");

	act = global_actions.register_toggle_action (option_actions, X_("SendMTC"), _("Send MTC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_mtc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (option_actions, X_("SendMMC"), _("Send MMC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (option_actions, X_("UseMMC"), _("Use MMC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_use_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = global_actions.register_toggle_action (option_actions, X_("SendMidiClock"), _("Send MIDI Clock"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_midi_clock));
	ActionManager::session_sensitive_actions.push_back (act);

	/* MIDI */

	Glib::RefPtr<ActionGroup> midi_actions = global_actions.create_action_group (X_("MIDI"));
	global_actions.register_action (midi_actions, X_("panic"), _("Panic (Send MIDI all-notes-off)"), sigc::mem_fun(*this, &ARDOUR_UI::midi_panic));
}

void
ARDOUR_UI::build_menu_bar ()
{
	menu_bar = dynamic_cast<MenuBar*> (ActionManager::get_widget (X_("/Main")));
	menu_bar->set_name ("MainMenuBar");

	EventBox* ev = manage (new EventBox);
	ev->show ();
	CairoHPacker* hbox = manage (new CairoHPacker);
	hbox->set_name (X_("StatusBarBox"));
	hbox->show ();
	hbox->set_border_width (3);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*hbox, true, false);
	vbox->show();

	ev->add (*vbox);

	wall_clock_label.set_name ("WallClock");
	wall_clock_label.set_use_markup ();
	disk_space_label.set_name ("WallClock");
	disk_space_label.set_use_markup ();
	timecode_format_label.set_name ("WallClock");
	timecode_format_label.set_use_markup ();
	cpu_load_label.set_name ("CPULoad");
	cpu_load_label.set_use_markup ();
	xrun_label.set_name ("XrunLabel");
	xrun_label.set_use_markup ();
	peak_thread_work_label.set_name ("PeakThreadWork");
	peak_thread_work_label.set_use_markup ();
	buffer_load_label.set_name ("BufferLoad");
	buffer_load_label.set_use_markup ();
	sample_rate_label.set_name ("SampleRate");
	sample_rate_label.set_use_markup ();
	format_label.set_name ("Format");
	format_label.set_use_markup ();

#ifndef TOP_MENUBAR
	menu_hbox.pack_start (*menu_bar, false, false);
#else
	use_menubar_as_top_menubar ();
#endif

	hbox->pack_end (error_alert_button, false, false, 2);

	hbox->pack_end (wall_clock_label, false, false, 2);
	hbox->pack_end (disk_space_label, false, false, 4);
	hbox->pack_end (xrun_label, false, false, 4);
	hbox->pack_end (peak_thread_work_label, false, false, 4);
	hbox->pack_end (cpu_load_label, false, false, 4);
	hbox->pack_end (buffer_load_label, false, false, 4);
	hbox->pack_end (sample_rate_label, false, false, 4);
	hbox->pack_end (timecode_format_label, false, false, 4);
	hbox->pack_end (format_label, false, false, 4);

	menu_hbox.pack_end (*ev, false, false, 2);

	menu_bar_base.set_name ("MainMenuBar");
	menu_bar_base.add (menu_hbox);

#ifndef __APPLE__
	// OSX provides its own wallclock, thank you very much
	_status_bar_visibility.add (&wall_clock_label,      X_("WallClock"), _("Wall Clock"), true);
#endif
	_status_bar_visibility.add (&disk_space_label,      X_("Disk"),      _("Disk Space"), !Profile->get_small_screen());
	_status_bar_visibility.add (&cpu_load_label,        X_("DSP"),       _("DSP"), true);
	_status_bar_visibility.add (&xrun_label,            X_("XRun"),      _("X-run"), false);
	_status_bar_visibility.add (&peak_thread_work_label,X_("Peakfile"),  _("Active Peak-file Work"), false);
	_status_bar_visibility.add (&buffer_load_label,     X_("Buffers"),   _("Buffers"), true);
	_status_bar_visibility.add (&sample_rate_label,     X_("Audio"),     _("Audio"), true);
	_status_bar_visibility.add (&timecode_format_label, X_("TCFormat"),  _("Timecode Format"), true);
	_status_bar_visibility.add (&format_label,          X_("Format"),    _("File Format"), true);

	ev->signal_button_press_event().connect (sigc::mem_fun (_status_bar_visibility, &VisibilityGroup::button_press_event));
	ev->signal_button_release_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::xrun_button_release));
}

void
ARDOUR_UI::use_menubar_as_top_menubar ()
{
	Gtk::Widget* widget;
	Application* app = Application::instance ();

        /* the addresses ("/ui/Main...") used below are based on the menu definitions in the menus file
         */

	/* Quit will be taken care of separately */

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/Quit"))) {
		widget->hide ();
	}

	/* Put items for About and Preferences into App menu (the
	 * ardour.menus.in file does not list them for OS X)
	 */

	GtkApplicationMenuGroup* group = app->add_app_menu_group ();

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/toggle-about"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
        }

	if ((widget = ActionManager::get_widget ("/ui/Main/Edit/menu-show-preferences"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
        }

	app->set_menu_bar (*menu_bar);
}

void
ARDOUR_UI::save_ardour_state ()
{
	if (!keyboard || !mixer || !editor || !meterbridge) {
		return;
	}

	/* XXX this is all a bit dubious. add_extra_xml() uses
	   a different lifetime model from add_instant_xml().
	*/

	LocaleGuard lg; // one guard to rule them all
	XMLNode* node = new XMLNode (keyboard->get_state());
	Config->add_extra_xml (*node);

	XMLNode* window_node = new XMLNode (X_("UI"));
	window_node->set_property (_status_bar_visibility.get_state_name().c_str(), _status_bar_visibility.get_state_value ());

	/* main window */

	gint mx, my, mw, mh;
	_main_window.get_position (mx, my);
	_main_window.get_size (mw, mh);

	XMLNode main_window_node (X_("Main"));
	main_window_node.set_property (X_("x"), mx);
	main_window_node.set_property (X_("y"), my);
	main_window_node.set_property (X_("w"), mw);
	main_window_node.set_property (X_("h"), mh);

	string current_tab;
	int current_page_number = _tabs.get_current_page ();
	if (current_page_number == _tabs.page_num (editor->contents())) {
		current_tab = "editor";
	} else if (current_page_number == _tabs.page_num (mixer->contents())) {
		current_tab = "mixer";
	} else if (current_page_number == _tabs.page_num (rc_option_editor->contents())) {
		current_tab = "preferences";
	}

	main_window_node.set_property (X_("current-tab"), current_tab);

	/* Windows */

	WM::Manager::instance().add_state (*window_node);

	XMLNode* tearoff_node = new XMLNode (X_("Tearoffs"));

	if (mixer->monitor_section()) {
		XMLNode* t = new XMLNode (X_("monitor-section"));
		mixer->monitor_section()->tearoff().add_state (*t);
		tearoff_node->add_child_nocopy (*t);
	}

	window_node->add_child_nocopy (*tearoff_node);

	XMLNode& enode (editor->get_state());
	XMLNode& mnode (mixer->get_state());
	XMLNode& bnode (meterbridge->get_state());
	XMLNode& pnode (rc_option_editor->get_state());

	Config->add_extra_xml (*window_node);
	Config->add_extra_xml (audio_midi_setup->get_state());

	Config->save_state();

	UIConfiguration::instance().save_state ();

	if (_session) {
		_session->add_instant_xml (main_window_node);
		_session->add_instant_xml (enode);
		_session->add_instant_xml (mnode);
		_session->add_instant_xml (pnode);
		_session->add_instant_xml (bnode);
		if (location_ui) {
			_session->add_instant_xml (location_ui->ui().get_state ());
		}
	} else {
		Config->add_instant_xml (main_window_node);
		Config->add_instant_xml (enode);
		Config->add_instant_xml (mnode);
		Config->add_instant_xml (pnode);
		Config->add_instant_xml (bnode);
		if (location_ui) {
			Config->add_instant_xml (location_ui->ui().get_state ());
		}
	}

	delete &enode;
	delete &mnode;
	delete &bnode;
	delete &pnode;

	Keyboard::save_keybindings ();
}

void
ARDOUR_UI::resize_text_widgets ()
{
	set_size_request_to_display_given_text (cpu_load_label, "DSP: 100.0%", 2, 2);
	set_size_request_to_display_given_text (buffer_load_label, "Buffers: p:100% c:100%", 2, 2);
	set_size_request_to_display_given_text (xrun_label, "X: 9999", 2, 2);
}

void
ARDOUR_UI::focus_on_clock ()
{
	if (primary_clock) {
		primary_clock->focus ();
	}
}

bool
ARDOUR_UI::xrun_button_release (GdkEventButton* ev)
{
	if (ev->button != 1 || !Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
		return false;
	}

	if (_session) {
		_session->reset_xrun_count ();
		update_xrun_count ();
	}
	return true;
}

Gtk::Notebook&
ARDOUR_UI::tabs()
{
	return _tabs;
}

bool
ARDOUR_UI::tabbable_visibility_button_press (GdkEventButton* ev, string const& tabbable_name)
{
	if (ev->button != 3) {
		return false;
	}

	/* context menu is defined in *.menus.in
	 */

	string menu_name = string ("/ui/") + tabbable_name + X_("TabbableButtonMenu");
	Gtk::Menu* menu = dynamic_cast<Gtk::Menu*> (ActionManager::get_widget (menu_name.c_str()));
	if (menu) {
		menu->popup (3, ev->time);
	}
	return true;
}
