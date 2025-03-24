/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <ytkmm/sizegroup.h>

#include "ardour/dB.h"
#include "ardour/profile.h"
#include "widgets/tooltips.h"
#include "gtkmm2ext/gui_thread.h"

#include "actions.h"
#include "ardour_ui.h"
#include "opts.h"
#include "timers.h"
#include "transport_control_ui.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

TransportControlUI::TransportControlUI ()
{
	Config->ParameterChanged.connect (config_connection, MISSING_INVALIDATOR, std::bind (&TransportControlUI::parameter_changed, this, _1), gui_context());
}

void
TransportControlUI::map_actions ()
{
	/* setup actions */
	RefPtr<Action> act;

	act = ActionManager::get_action (X_("Transport"), X_("ToggleClick"));
	_click_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Stop"));
	_stop_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Roll"));
	_roll_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Record"));
	_rec_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("GotoStart"));
	_goto_start_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("GotoEnd"));
	_goto_end_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Loop"));
	_auto_loop_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("PlaySelection"));
	_play_selection_button.set_related_action (act);

	act = ActionManager::get_action (X_("MIDI"), X_("panic"));
	_midi_panic_button.set_related_action (act);

	/* tooltips depend on actions */
	set_tooltip (_roll_button, _("Play from playhead"));
	set_tooltip (_stop_button, _("Stop playback"));
	set_tooltip (_rec_button, _("Toggle record"));
	set_tooltip (_play_selection_button, _("Play range/selection"));
	set_tooltip (_goto_start_button, _("Go to start of session"));
	set_tooltip (_goto_end_button, _("Go to end of session"));
	set_tooltip (_auto_loop_button, _("Play loop range"));
	set_tooltip (_midi_panic_button, _("MIDI Panic\nSend note off and reset controller messages on all MIDI channels"));

	/* set _click_button tooltip */
	parameter_changed ("click-gain");
}

void
TransportControlUI::setup (TransportControlProvider* ui)
{
	_click_button.signal_button_press_event().connect (sigc::mem_fun (*ui, &TransportControlProvider::click_button_clicked), false);
	_click_button.signal_scroll_event().connect (sigc::mem_fun (*this, &TransportControlUI::click_button_scroll), false);

	/* setup icons */

	_click_button.set_icon (ArdourIcon::TransportMetronom);
	_goto_start_button.set_icon (ArdourIcon::TransportStart);
	_goto_end_button.set_icon (ArdourIcon::TransportEnd);
	_roll_button.set_icon (ArdourIcon::TransportPlay);
	_stop_button.set_icon (ArdourIcon::TransportStop);
	_play_selection_button.set_icon (ArdourIcon::TransportRange);
	_auto_loop_button.set_icon (ArdourIcon::TransportLoop);
	_rec_button.set_icon (ArdourIcon::RecButton);
	_midi_panic_button.set_icon (ArdourIcon::TransportPanic);

	/* transport control size-group */

	Glib::RefPtr<SizeGroup> transport_button_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	if (!ARDOUR::Profile->get_mixbus()) {
		transport_button_size_group->add_widget (_goto_start_button);
		transport_button_size_group->add_widget (_goto_end_button);
		transport_button_size_group->add_widget (_auto_loop_button);
		transport_button_size_group->add_widget (_rec_button);
		transport_button_size_group->add_widget (_play_selection_button);
		transport_button_size_group->add_widget (_roll_button);
		transport_button_size_group->add_widget (_stop_button);
		transport_button_size_group->add_widget (_midi_panic_button);
		transport_button_size_group->add_widget (_click_button);
	}

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

	_click_button.set_size_request (PX_SCALE(20), PX_SCALE(20));
	set_spacing (PX_SCALE(2));

#undef PX_SCALE

	if (!ARDOUR::Profile->get_mixbus()) {
		pack_start (_midi_panic_button, true, true, 0);
		pack_start (_click_button, true, true, 0);
		pack_start (_goto_start_button, true, true);
		pack_start (_goto_end_button, true, true);
		pack_start (_auto_loop_button, true, true);
		pack_start (_play_selection_button, true, true);
		pack_start (_roll_button, true, true);
		pack_start (_stop_button, true, true);
		pack_start (_rec_button, true, true, 3);
	}

	_roll_button.set_name ("transport button");
	_stop_button.set_name ("transport button");
	_goto_start_button.set_name ("transport button");
	_goto_end_button.set_name ("transport button");
	_auto_loop_button.set_name ("transport button");
	_play_selection_button.set_name ("transport button");
	_rec_button.set_name ("transport recenable button");
	_midi_panic_button.set_name ("transport button"); // XXX ???
	_click_button.set_name ("transport button");

	_roll_button.set_controllable (ui->roll_controllable);
	_stop_button.set_controllable (ui->stop_controllable);
	_goto_start_button.set_controllable (ui->goto_start_controllable);
	_goto_end_button.set_controllable (ui->goto_end_controllable);
	_auto_loop_button.set_controllable (ui->auto_loop_controllable);
	_play_selection_button.set_controllable (ui->play_selection_controllable);
	_rec_button.set_controllable (ui->rec_controllable);

	_stop_button.set_active (true);

	show_all ();

	Timers::blink_connect (sigc::mem_fun (*this, &TransportControlUI::blink_rec_enable));
}

void
TransportControlUI::set_session (ARDOUR::Session *s)
{
	SessionHandlePtr::set_session (s);
	set_loop_sensitivity ();
	map_transport_state ();

	if (!_session) {
		_rec_button.set_sensitive (false);
		return;
	}

	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&TransportControlUI::parameter_changed, this, _1), gui_context());
	_session->StepEditStatusChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&TransportControlUI::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&TransportControlUI::map_transport_state, this), gui_context());
	_session->auto_loop_location_changed.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&TransportControlUI::set_loop_sensitivity, this), gui_context ());
	_session->PunchLoopConstraintChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&TransportControlUI::set_loop_sensitivity, this), gui_context ());

	_rec_button.set_sensitive (true);
}

void
TransportControlUI::parameter_changed (std::string p)
{
	if (p == "external-sync") {
		set_loop_sensitivity ();
	} else if (p == "click-record-only") {
		// TODO set a flag, blink or gray-out metronome button while rolling, only
		if (Config->get_click_record_only()) {
			_click_button.set_name ("generic button"); // XXX
		} else {
			_click_button.set_name ("transport button");
		}
	} else if (p == "click-gain") {
		float gain_db = accurate_coefficient_to_dB (Config->get_click_gain());
		char tmp[32];
		snprintf(tmp, 31, "%+.1f", gain_db);
		set_tooltip (_click_button, string_compose (_("Enable/Disable metronome\n\nRight-click to access preferences\nMouse-wheel to modify level\nSignal Level: %1 dBFS"), tmp));
	}
}

void
TransportControlUI::map_transport_state ()
{
	if (!_session) {
		_auto_loop_button.unset_active_state ();
		_play_selection_button.unset_active_state ();
		_roll_button.unset_active_state ();
		_stop_button.set_active_state (Gtkmm2ext::ExplicitActive);
		return;
	}

	float sp = _session->transport_speed();

	if (sp != 0.0f) {

		/* we're rolling */

		if (_session->get_play_range()) {

			_play_selection_button.set_active_state (Gtkmm2ext::ExplicitActive);
			_roll_button.unset_active_state ();
			_auto_loop_button.unset_active_state ();

		} else if (_session->get_play_loop ()) {

			_auto_loop_button.set_active (true);
			_play_selection_button.set_active (false);

			if (Config->get_loop_is_mode()) {
				_roll_button.set_active (true);
			} else {
				_roll_button.set_active (false);
			}

		} else {

			_roll_button.set_active (true);
			_play_selection_button.set_active (false);
			_auto_loop_button.set_active (false);

		}

		if (UIConfiguration::instance().get_follow_edits() && !_session->config.get_external_sync()) {
			/* light up both roll and play-selection if they are joined */
			_roll_button.set_active (true);
			_play_selection_button.set_active (true);
		}

		_stop_button.set_active (false);

	} else {

		_stop_button.set_active (true);
		_roll_button.set_active (false);
		_play_selection_button.set_active (false);
		if (Config->get_loop_is_mode ()) {
			_auto_loop_button.set_active (_session->get_play_loop());
		} else {
			_auto_loop_button.set_active (false);
		}
	}
}

void
TransportControlUI::step_edit_status_change (bool yn)
{
	// XXX should really store pre-step edit status of things
	// we make insensitive

	if (yn) {
		_rec_button.set_active_state (Gtkmm2ext::ImplicitActive);
		_rec_button.set_sensitive (false);
	} else {
		_rec_button.unset_active_state ();;
		_rec_button.set_sensitive (true);
	}
}

void
TransportControlUI::set_loop_sensitivity ()
{
	if (!_session || _session->config.get_external_sync()) {
		_auto_loop_button.set_sensitive (false);
	} else {
		_auto_loop_button.set_sensitive (_session && _session->loop_is_possible() && _session->locations()->auto_loop_location());
	}
}

void
TransportControlUI::blink_rec_enable (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->step_editing()) {
		return;
	}

	RecordState const r = _session->record_status ();
	bool const h = _session->have_rec_enabled_track ();

	if (UIConfiguration::instance().get_no_strobe()) {
		onoff = true;
	}

	if (r == Enabled || (r == Recording && !h)) {
		if (onoff) {
			_rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			_rec_button.set_active_state (Gtkmm2ext::Off);
		}
	} else if (r == Recording && h) {
		_rec_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		_rec_button.unset_active_state ();
	}
}

bool
TransportControlUI::click_button_scroll (GdkEventScroll* ev)
{
	gain_t gain = Config->get_click_gain();
	float gain_db = accurate_coefficient_to_dB (gain);

	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			gain_db += 1;
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			gain_db -= 1;
			break;
	}
	gain_db = std::max (-60.f, gain_db);
	gain = dB_to_coefficient (gain_db);
	gain = std::min (gain, Config->get_max_gain());
	Config->set_click_gain (gain);
	return true;
}
