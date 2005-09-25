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

#include <gtkmm.h>
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
	KeyboardTarget::add_action ("start-prefix", slot (*this, &ARDOUR_UI::start_keyboard_prefix));

	KeyboardTarget::add_action ("toggle-editor-window", slot (*this, &ARDOUR_UI::goto_editor_window));
	KeyboardTarget::add_action ("toggle-mixer-window", slot (*this, &ARDOUR_UI::goto_mixer_window));
	KeyboardTarget::add_action ("toggle-locations-window", slot (*this, &ARDOUR_UI::toggle_location_window));
	KeyboardTarget::add_action ("toggle-big-clock-window", slot (*this, &ARDOUR_UI::toggle_big_clock_window));
	KeyboardTarget::add_action ("toggle-options-window", slot (*this, &ARDOUR_UI::toggle_options_window));
	KeyboardTarget::add_action ("toggle-auto-loop", slot (*this, &ARDOUR_UI::toggle_session_auto_loop));
	KeyboardTarget::add_action ("toggle-punch-in", slot (*this, &ARDOUR_UI::toggle_session_punch_in));

	KeyboardTarget::add_action ("new-session", bind (slot (*this, &ARDOUR_UI::new_session), false, string()));
	KeyboardTarget::add_action ("add-audio-track", bind (slot (*this, &ARDOUR_UI::session_add_audio_track), 1, 1));
	KeyboardTarget::add_action ("add-audio-bus", bind (slot (*this, &ARDOUR_UI::session_add_audio_bus), 1, 1));

	KeyboardTarget::add_action ("save-state", bind (slot (*this, &ARDOUR_UI::save_state), string ("")));
	KeyboardTarget::add_action ("quit", (slot (*this, &ARDOUR_UI::finish)));

	KeyboardTarget::add_action ("remove-last-capture", slot (*this, &ARDOUR_UI::remove_last_capture));

	KeyboardTarget::add_action ("transport-stop", slot (*this, &ARDOUR_UI::transport_stop));
	KeyboardTarget::add_action ("transport-stop-and-forget-capture", slot (*this, &ARDOUR_UI::transport_stop_and_forget_capture));
	KeyboardTarget::add_action ("transport-roll", slot (*this, &ARDOUR_UI::transport_roll));
	KeyboardTarget::add_action ("transport-loop", slot (*this, &ARDOUR_UI::transport_loop));
	KeyboardTarget::add_action ("transport-record", slot (*this, &ARDOUR_UI::transport_record));
	KeyboardTarget::add_action ("transport-rewind", bind (slot (*this, &ARDOUR_UI::transport_rewind), 0));
	KeyboardTarget::add_action ("transport-rewind-slow", bind (slot (*this, &ARDOUR_UI::transport_rewind), -1));
	KeyboardTarget::add_action ("transport-rewind-fast", bind (slot (*this, &ARDOUR_UI::transport_rewind), 1));
	KeyboardTarget::add_action ("transport-forward", bind (slot (*this, &ARDOUR_UI::transport_forward), 0));
	KeyboardTarget::add_action ("transport-forward-slow", bind (slot (*this, &ARDOUR_UI::transport_forward), -1));
	KeyboardTarget::add_action ("transport-forward-fast", bind (slot (*this, &ARDOUR_UI::transport_forward), 1));

	KeyboardTarget::add_action ("transport-goto-start", slot (*this, &ARDOUR_UI::transport_goto_start));
	KeyboardTarget::add_action ("transport-goto-end", slot (*this, &ARDOUR_UI::transport_goto_end));

	KeyboardTarget::add_action ("send-all-midi-feedback", slot (*this, &ARDOUR_UI::send_all_midi_feedback));
	
	KeyboardTarget::add_action ("toggle-record-enable-track1", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  0U));
	KeyboardTarget::add_action ("toggle-record-enable-track2", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  1U));
	KeyboardTarget::add_action ("toggle-record-enable-track3", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  2U));
	KeyboardTarget::add_action ("toggle-record-enable-track4", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  3U));
	KeyboardTarget::add_action ("toggle-record-enable-track5", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  4U));
	KeyboardTarget::add_action ("toggle-record-enable-track6", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  5U));
	KeyboardTarget::add_action ("toggle-record-enable-track7", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  6U));
	KeyboardTarget::add_action ("toggle-record-enable-track8", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  7U));
	KeyboardTarget::add_action ("toggle-record-enable-track9", bind (slot (*this, &ARDOUR_UI::toggle_record_enable),  8U));
	KeyboardTarget::add_action ("toggle-record-enable-track10", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 9U));
	KeyboardTarget::add_action ("toggle-record-enable-track11", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 10U));
	KeyboardTarget::add_action ("toggle-record-enable-track12", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 11U));
	KeyboardTarget::add_action ("toggle-record-enable-track13", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 12U));
	KeyboardTarget::add_action ("toggle-record-enable-track14", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 13U));
	KeyboardTarget::add_action ("toggle-record-enable-track15", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 14U));
	KeyboardTarget::add_action ("toggle-record-enable-track16", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 15U));
	KeyboardTarget::add_action ("toggle-record-enable-track17", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 16U));
	KeyboardTarget::add_action ("toggle-record-enable-track18", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 17U));
	KeyboardTarget::add_action ("toggle-record-enable-track19", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 18U));
	KeyboardTarget::add_action ("toggle-record-enable-track20", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 19U));
	KeyboardTarget::add_action ("toggle-record-enable-track21", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 20U));
	KeyboardTarget::add_action ("toggle-record-enable-track22", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 21U));
	KeyboardTarget::add_action ("toggle-record-enable-track23", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 22U));
	KeyboardTarget::add_action ("toggle-record-enable-track24", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 23U));
	KeyboardTarget::add_action ("toggle-record-enable-track25", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 24U));
	KeyboardTarget::add_action ("toggle-record-enable-track26", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 25U));
	KeyboardTarget::add_action ("toggle-record-enable-track27", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 26U));
	KeyboardTarget::add_action ("toggle-record-enable-track28", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 27U));
	KeyboardTarget::add_action ("toggle-record-enable-track29", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 28U));
	KeyboardTarget::add_action ("toggle-record-enable-track30", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 29U));
	KeyboardTarget::add_action ("toggle-record-enable-track31", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 30U));
	KeyboardTarget::add_action ("toggle-record-enable-track32", bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 31U));
	
#if 0
	ADD ME TO ARDOUR RC SOMEDAY
	add_binding ("Shift-F1",, bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 0+12U));
	add_binding ("Control-F1",, bind (slot (*this, &ARDOUR_UI::toggle_record_enable), 0+24U));
	add_binding ("Alt-F1",, bind (slot (*this, &ARDOUR_UI::toggle_monitor_enable), 0U));
	add_binding ("Alt-Shift-F1",, bind (slot (*this, &ARDOUR_UI::toggle_monitor_enable), 0+12U));
	add_binding ("Alt-Control-F1",, bind (slot (*this, &ARDOUR_UI::toggle_monitor_enable), 0+24U));
#endif
}
