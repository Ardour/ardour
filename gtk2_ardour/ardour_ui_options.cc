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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "pbd/convert.h"
#include "pbd/stacktrace.h"

#include <gtkmm2ext/utils.h>

#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#ifdef HAVE_LIBLO
#include "ardour/osc.h"
#endif

#include "audio_clock.h"
#include "ardour_ui.h"
#include "actions.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "main_clock.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;

void
ARDOUR_UI::toggle_keep_tearoffs ()
{
	ActionManager::toggle_config_state ("Common", "KeepTearoffs", &RCConfiguration::set_keep_tearoffs, &RCConfiguration::get_keep_tearoffs);

	ARDOUR_UI::update_tearoff_visibility();
}

void
ARDOUR_UI::toggle_external_sync()
{
	if (_session) {
		if (_session->config.get_video_pullup() != 0.0f) {
			if (Config->get_sync_source() == JACK) {
				MessageDialog msg (
					_("It is not possible to use JACK as the the sync source\n\
when the pull up/down setting is non-zero."));
				msg.run ();
				return;
			}
		}

		ActionManager::toggle_config_state_foo ("Transport", "ToggleExternalSync", sigc::mem_fun (_session->config, &SessionConfiguration::set_external_sync), sigc::mem_fun (_session->config, &SessionConfiguration::get_external_sync));
	}
}

void
ARDOUR_UI::toggle_time_master ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleTimeMaster", sigc::mem_fun (_session->config, &SessionConfiguration::set_jack_time_master), sigc::mem_fun (_session->config, &SessionConfiguration::get_jack_time_master));
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
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoInput", sigc::mem_fun (_session->config, &SessionConfiguration::set_auto_input), sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_input));
}

void
ARDOUR_UI::toggle_auto_play ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoPlay", sigc::mem_fun (_session->config, &SessionConfiguration::set_auto_play), sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_play));
}

void
ARDOUR_UI::toggle_auto_return ()
{
	ActionManager::toggle_config_state_foo ("Transport", "ToggleAutoReturn", sigc::mem_fun (_session->config, &SessionConfiguration::set_auto_return), sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_return));
}

void
ARDOUR_UI::toggle_click ()
{
	ActionManager::toggle_config_state ("Transport", "ToggleClick", &RCConfiguration::set_clicking, &RCConfiguration::get_clicking);
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
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("TogglePunchIn"));
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	if (tact->get_active() != _session->config.get_punch_in()) {
		_session->config.set_punch_in (tact->get_active ());
	}

	if (tact->get_active()) {
		/* if punch-in is turned on, make sure the loop/punch ruler is visible, and stop it being hidden,
		   to avoid confusing the user */
		show_loop_punch_ruler_and_disallow_hide ();
	}

	reenable_hide_loop_punch_ruler_if_appropriate ();
}

void
ARDOUR_UI::toggle_punch_out ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("TogglePunchOut"));
	if (!act) {
		return;
	}

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	if (tact->get_active() != _session->config.get_punch_out()) {
		_session->config.set_punch_out (tact->get_active ());
	}

	if (tact->get_active()) {
		/* if punch-out is turned on, make sure the loop/punch ruler is visible, and stop it being hidden,
		   to avoid confusing the user */
		show_loop_punch_ruler_and_disallow_hide ();
	}

	reenable_hide_loop_punch_ruler_if_appropriate ();
}

void
ARDOUR_UI::show_loop_punch_ruler_and_disallow_hide ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Rulers"), "toggle-loop-punch-ruler");
	if (!act) {
		return;
	}

	act->set_sensitive (false);

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	if (!tact->get_active()) {
		tact->set_active ();
	}
}

/* This is a bit of a silly name for a method */
void
ARDOUR_UI::reenable_hide_loop_punch_ruler_if_appropriate ()
{
	if (!_session->config.get_punch_in() && !_session->config.get_punch_out()) {
		/* if punch in/out are now both off, reallow hiding of the loop/punch ruler */
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Rulers"), "toggle-loop-punch-ruler");
		if (act) {
			act->set_sensitive (true);
		}
	}
}

void
ARDOUR_UI::toggle_video_sync()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", "ToggleVideoSync");
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		_session->config.set_use_video_sync (tact->get_active());
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
ARDOUR_UI::setup_session_options ()
{
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::parameter_changed, this, _1), gui_context());
	boost::function<void (std::string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
	_session->config.map_parameters (pc);
}

void
ARDOUR_UI::parameter_changed (std::string p)
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::parameter_changed, p)

	if (p == "external-sync") {

		ActionManager::map_some_state ("Transport", "ToggleExternalSync", sigc::mem_fun (_session->config, &SessionConfiguration::get_external_sync));

		if (!_session->config.get_external_sync()) {
			sync_button.set_text (_("Internal"));
			ActionManager::get_action ("Transport", "ToggleAutoPlay")->set_sensitive (true);
			ActionManager::get_action ("Transport", "ToggleAutoReturn")->set_sensitive (true);
		} else {
			sync_button.set_text (sync_source_to_string (Config->get_sync_source(), true));
			/* XXX need to make auto-play is off as well as insensitive */
			ActionManager::get_action ("Transport", "ToggleAutoPlay")->set_sensitive (false);
			ActionManager::get_action ("Transport", "ToggleAutoReturn")->set_sensitive (false);
		}

	} else if (p == "always-play-range") {

		ActionManager::map_some_state ("Transport", "AlwaysPlayRange", &RCConfiguration::get_always_play_range);

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

	} else if (p == "keep-tearoffs") {
		ActionManager::map_some_state ("Common", "KeepTearoffs", &RCConfiguration::get_keep_tearoffs);
	} else if (p == "mmc-control") {
		ActionManager::map_some_state ("options", "UseMMC", &RCConfiguration::get_mmc_control);
	} else if (p == "midi-feedback") {
		ActionManager::map_some_state ("options", "SendMIDIfeedback", &RCConfiguration::get_midi_feedback);
	} else if (p == "auto-play") {
		ActionManager::map_some_state ("Transport", "ToggleAutoPlay", sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_play));
	} else if (p == "auto-return") {
		ActionManager::map_some_state ("Transport", "ToggleAutoReturn", sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_return));
	} else if (p == "auto-input") {
		ActionManager::map_some_state ("Transport", "ToggleAutoInput", sigc::mem_fun (_session->config, &SessionConfiguration::get_auto_input));
	} else if (p == "punch-out") {
		ActionManager::map_some_state ("Transport", "TogglePunchOut", sigc::mem_fun (_session->config, &SessionConfiguration::get_punch_out));
		if (!_session->config.get_punch_out()) {
			unset_dual_punch ();
		}
	} else if (p == "punch-in") {
		ActionManager::map_some_state ("Transport", "TogglePunchIn", sigc::mem_fun (_session->config, &SessionConfiguration::get_punch_in));
		if (!_session->config.get_punch_in()) {
			unset_dual_punch ();
		}
	} else if (p == "clicking") {
		ActionManager::map_some_state ("Transport", "ToggleClick", &RCConfiguration::get_clicking);
	} else if (p == "use-video-sync") {
		ActionManager::map_some_state ("Transport",  "ToggleVideoSync", sigc::mem_fun (_session->config, &SessionConfiguration::get_use_video_sync));
	} else if (p == "video-pullup" || p == "timecode-format") {

		synchronize_sync_source_and_video_pullup ();
		reset_main_clocks ();

	} else if (p == "sync-source") {

		synchronize_sync_source_and_video_pullup ();

	} else if (p == "show-track-meters") {
		editor->toggle_meter_updating();
	} else if (p == "primary-clock-delta-edit-cursor") {
		if (Config->get_primary_clock_delta_edit_cursor()) {
			primary_clock->set_is_duration (true);
			primary_clock->set_editable (false);
			primary_clock->set_widget_name ("transport delta");
		} else {
			primary_clock->set_is_duration (false);
			primary_clock->set_editable (true);
			primary_clock->set_widget_name ("transport");
		}
	} else if (p == "secondary-clock-delta-edit-cursor") {
		if (Config->get_secondary_clock_delta_edit_cursor()) {
			secondary_clock->set_is_duration (true);
			secondary_clock->set_editable (false);
			secondary_clock->set_widget_name ("secondary delta");
		} else {
			secondary_clock->set_is_duration (false);
			secondary_clock->set_editable (true);
			secondary_clock->set_widget_name ("secondary");
		}
	}
}

void
ARDOUR_UI::session_parameter_changed (std::string p)
{
	if (p == "native-file-data-format" || p == "native-file-header-format") {
		update_format ();
	}
}

void
ARDOUR_UI::reset_main_clocks ()
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::reset_main_clocks)

	if (_session) {
		primary_clock->set (_session->audible_frame(), true);
		secondary_clock->set (_session->audible_frame(), true);
	} else {
		primary_clock->set (0, true);
		secondary_clock->set (0, true);
	}
}

void
ARDOUR_UI::synchronize_sync_source_and_video_pullup ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));

	if (!act) {
		return;
	}

	if (!_session) {
		goto just_label;
	}

	if (_session->config.get_video_pullup() == 0.0f) {
		/* with no video pull up/down, any sync source is OK */
		act->set_sensitive (true);
	} else {
		/* can't sync to JACK if video pullup != 0.0 */
		if (Config->get_sync_source() == JACK) {
			act->set_sensitive (false);
		} else {
			act->set_sensitive (true);
		}
	}

	/* XXX should really be able to set the video pull up
	   action to insensitive/sensitive, but there is no action.
	   FIXME
	*/

  just_label:
	if (act->get_sensitive ()) {
		set_tip (sync_button, _("Enable/Disable external positional sync"));
	} else {
		set_tip (sync_button, _("Sync to JACK is not possible: video pull up/down is set"));
	}

}

