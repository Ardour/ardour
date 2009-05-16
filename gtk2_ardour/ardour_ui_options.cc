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
	ActionManager::toggle_config_state_foo ("Transport", "ToggleTimeMaster", mem_fun (session->config, &SessionConfiguration::set_jack_time_master), mem_fun (session->config, &SessionConfiguration::get_jack_time_master));
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
ARDOUR_UI::toggle_send_midi_feedback ()
{
	ActionManager::toggle_config_state ("options", "SendMIDIfeedback", &RCConfiguration::set_midi_feedback, &RCConfiguration::get_midi_feedback);
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
		session->config.set_use_video_sync (tact->get_active());
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

	} else if (p == "mmc-control") {
		ActionManager::map_some_state ("options", "UseMMC", &RCConfiguration::get_mmc_control);
	} else if (p == "midi-feedback") {
		ActionManager::map_some_state ("options", "SendMIDIfeedback", &RCConfiguration::get_midi_feedback);
	} else if (p == "auto-play") {
		ActionManager::map_some_state ("Transport", "ToggleAutoPlay", mem_fun (session->config, &SessionConfiguration::get_auto_play));
	} else if (p == "auto-return") {
		ActionManager::map_some_state ("Transport", "ToggleAutoReturn", mem_fun (session->config, &SessionConfiguration::get_auto_return));
	} else if (p == "auto-input") {
		ActionManager::map_some_state ("Transport", "ToggleAutoInput", mem_fun (session->config, &SessionConfiguration::get_auto_input));
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
		ActionManager::map_some_state ("Transport",  "ToggleTimeMaster", mem_fun (session->config, &SessionConfiguration::get_jack_time_master));
	} else if (p == "use-video-sync") {
		ActionManager::map_some_state ("Transport",  "ToggleVideoSync", mem_fun (session->config, &SessionConfiguration::get_use_video_sync));
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
	} else if (p == "video-pullup" || p == "smpte-format") {
		if (session) {
			primary_clock.set (session->audible_frame(), true);
			secondary_clock.set (session->audible_frame(), true);
		} else {
			primary_clock.set (0, true);
			secondary_clock.set (0, true);
		}
	} else if (p == "show-track-meters") {
		editor->toggle_meter_updating();
	}
}
