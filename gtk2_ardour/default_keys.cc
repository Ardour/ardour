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

#include <sigc++/bind.h>
#include <pbd/error.h>

#include "ardour_ui.h"
#include "keyboard_target.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;

void
ARDOUR_UI::install_keybindings ()
{
	register_action (shared_actions, "start-prefix", _("start-prefix"), mem_fun(*this, &ARDOUR_UI::start_keyboard_prefix));

	register_action (shared_actions, "toggle-editor-window", _("toggle-editor-window"), mem_fun(*this, &ARDOUR_UI::goto_editor_window));
	register_action (shared_actions, "toggle-mixer-window", _("toggle-mixer-window"), mem_fun(*this, &ARDOUR_UI::goto_mixer_window));
	register_action (shared_actions, "toggle-locations-window", _("toggle-locations-window"), mem_fun(*this, &ARDOUR_UI::toggle_location_window));
	register_action (shared_actions, "toggle-big-clock-window", _("toggle-big-clock-window"), mem_fun(*this, &ARDOUR_UI::toggle_big_clock_window));
	register_action (shared_actions, "toggle-options-window", _("toggle-options-window"), mem_fun(*this, &ARDOUR_UI::toggle_options_window));
	register_action (shared_actions, "toggle-auto-loop", _("toggle-auto-loop"), mem_fun(*this, &ARDOUR_UI::toggle_session_auto_loop));
	register_action (shared_actions, "toggle-punch-in", _("toggle-punch-in"), mem_fun(*this, &ARDOUR_UI::toggle_session_punch_in));

	register_action (shared_actions, "new-session", _("new-session"), bind (mem_fun(*this, &ARDOUR_UI::new_session), false, string()));
	register_action (shared_actions, "add-audio-track", _("add-audio-track"), bind (mem_fun(*this, &ARDOUR_UI::session_add_audio_track), 1, 1));
	register_action (shared_actions, "add-audio-bus", _("add-audio-bus"), bind (mem_fun(*this, &ARDOUR_UI::session_add_audio_bus), 1, 1));

	register_action (shared_actions, "save-state", _("save-state"), bind (mem_fun(*this, &ARDOUR_UI::save_state), string ("")));
	register_action (shared_actions, "quit", _("quit"), (mem_fun(*this, &ARDOUR_UI::finish)));
	register_action (shared_actions, "remove-last-capture", _("remove-last-capture"), mem_fun(*this, &ARDOUR_UI::remove_last_capture));

	register_action (shared_actions, "transport-stop", _("transport-stop"), mem_fun(*this, &ARDOUR_UI::transport_stop));
	register_action (shared_actions, "transport-stop-and-forget-capture", _("transport-stop-and-forget-capture"), mem_fun(*this, &ARDOUR_UI::transport_stop_and_forget_capture));
	register_action (shared_actions, "transport-roll", _("transport-roll"), mem_fun(*this, &ARDOUR_UI::transport_roll));
	register_action (shared_actions, "transport-loop", _("transport-loop"), mem_fun(*this, &ARDOUR_UI::transport_loop));
	register_action (shared_actions, "transport-record", _("transport-record"), mem_fun(*this, &ARDOUR_UI::transport_record));
	register_action (shared_actions, "transport-rewind", _("transport-rewind"), bind (mem_fun(*this, &ARDOUR_UI::transport_rewind), 0));
	register_action (shared_actions, "transport-rewind-slow", _("transport-rewind-slow"), bind (mem_fun(*this, &ARDOUR_UI::transport_rewind), -1));
	register_action (shared_actions, "transport-rewind-fast", _("transport-rewind-fast"), bind (mem_fun(*this, &ARDOUR_UI::transport_rewind), 1));
	register_action (shared_actions, "transport-forward", _("transport-forward"), bind (mem_fun(*this, &ARDOUR_UI::transport_forward), 0));
	register_action (shared_actions, "transport-forward-slow", _("transport-forward-slow"), bind (mem_fun(*this, &ARDOUR_UI::transport_forward), -1));
	register_action (shared_actions, "transport-forward-fast", _("transport-forward-fast"), bind (mem_fun(*this, &ARDOUR_UI::transport_forward), 1));

	register_action (shared_actions, "transport-goto-start", _("transport-goto-start"), mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	register_action (shared_actions, "transport-goto-end", _("transport-goto-end"), mem_fun(*this, &ARDOUR_UI::transport_goto_end));

	register_action (shared_actions, "send-all-midi-feedback", _("send-all-midi-feedback"), mem_fun(*this, &ARDOUR_UI::send_all_midi_feedback));
	
	register_action (shared_actions, "toggle-record-enable-track1", _("toggle-record-enable-track1"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  0U));
	register_action (shared_actions, "toggle-record-enable-track2", _("toggle-record-enable-track2"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  1U));
	register_action (shared_actions, "toggle-record-enable-track3", _("toggle-record-enable-track3"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  2U));
	register_action (shared_actions, "toggle-record-enable-track4", _("toggle-record-enable-track4"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  3U));
	register_action (shared_actions, "toggle-record-enable-track5", _("toggle-record-enable-track5"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  4U));
	register_action (shared_actions, "toggle-record-enable-track6", _("toggle-record-enable-track6"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  5U));
	register_action (shared_actions, "toggle-record-enable-track7", _("toggle-record-enable-track7"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  6U));
	register_action (shared_actions, "toggle-record-enable-track8", _("toggle-record-enable-track8"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  7U));
	register_action (shared_actions, "toggle-record-enable-track9", _("toggle-record-enable-track9"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable),  8U));
	register_action (shared_actions, "toggle-record-enable-track10", _("toggle-record-enable-track10"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 9U));
	register_action (shared_actions, "toggle-record-enable-track11", _("toggle-record-enable-track11"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 10U));
	register_action (shared_actions, "toggle-record-enable-track12", _("toggle-record-enable-track12"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 11U));
	register_action (shared_actions, "toggle-record-enable-track13", _("toggle-record-enable-track13"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 12U));
	register_action (shared_actions, "toggle-record-enable-track14", _("toggle-record-enable-track14"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 13U));
	register_action (shared_actions, "toggle-record-enable-track15", _("toggle-record-enable-track15"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 14U));
	register_action (shared_actions, "toggle-record-enable-track16", _("toggle-record-enable-track16"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 15U));
	register_action (shared_actions, "toggle-record-enable-track17", _("toggle-record-enable-track17"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 16U));
	register_action (shared_actions, "toggle-record-enable-track18", _("toggle-record-enable-track18"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 17U));
	register_action (shared_actions, "toggle-record-enable-track19", _("toggle-record-enable-track19"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 18U));
	register_action (shared_actions, "toggle-record-enable-track20", _("toggle-record-enable-track20"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 19U));
	register_action (shared_actions, "toggle-record-enable-track21", _("toggle-record-enable-track21"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 20U));
	register_action (shared_actions, "toggle-record-enable-track22", _("toggle-record-enable-track22"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 21U));
	register_action (shared_actions, "toggle-record-enable-track23", _("toggle-record-enable-track23"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 22U));
	register_action (shared_actions, "toggle-record-enable-track24", _("toggle-record-enable-track24"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 23U));
	register_action (shared_actions, "toggle-record-enable-track25", _("toggle-record-enable-track25"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 24U));
	register_action (shared_actions, "toggle-record-enable-track26", _("toggle-record-enable-track26"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 25U));
	register_action (shared_actions, "toggle-record-enable-track27", _("toggle-record-enable-track27"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 26U));
	register_action (shared_actions, "toggle-record-enable-track28", _("toggle-record-enable-track28"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 27U));
	register_action (shared_actions, "toggle-record-enable-track29", _("toggle-record-enable-track29"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 28U));
	register_action (shared_actions, "toggle-record-enable-track30", _("toggle-record-enable-track30"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 29U));
	register_action (shared_actions, "toggle-record-enable-track31", _("toggle-record-enable-track31"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 30U));
	register_action (shared_actions, "toggle-record-enable-track32", _("toggle-record-enable-track32"), bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 31U));
	
#if 0
	ADD ME TO ARDOUR RC SOMEDAY
	add_binding ("Shift-F1",, bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 0+12U));
	add_binding ("Control-F1",, bind (mem_fun(*this, &ARDOUR_UI::toggle_record_enable), 0+24U));
	add_binding ("Alt-F1",, bind (mem_fun(*this, &ARDOUR_UI::toggle_monitor_enable), 0U));
	add_binding ("Alt-Shift-F1",, bind (mem_fun(*this, &ARDOUR_UI::toggle_monitor_enable), 0+12U));
	add_binding ("Alt-Control-F1",, bind (mem_fun(*this, &ARDOUR_UI::toggle_monitor_enable), 0+24U));
#endif
}
